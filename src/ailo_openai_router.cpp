#include "ailo_bounded_json_internal.hpp"
#include "ailo_openai_internal.hpp"
#include <charconv>
#include <cstring>
#include <map>
#include <set>

namespace ailo {
namespace client {
namespace {
enum class endpoint {
  unknown,
  models,
  model,
  chat,
  chat_record,
  chat_messages,
  responses,
  response,
  compact,
  response_input,
  conversations,
  conversation,
  items,
  item
};
enum class query_kind { none, chat, messages, page, item };
struct route {
  endpoint destination = endpoint::unknown;
  std::vector<std::string> parts;
  std::string query = "{}";
  const char *allow = nullptr;
};
[[noreturn]] void invalid(const char *message) {
  throw failure(AILO_INVALID_ARGUMENT, message);
}
std::string bounded(const char *value, size_t maximum, const char *message) {
  if (!value)
    invalid(message);
  const auto n = strnlen(value, maximum + 1);
  if (!n || n > maximum)
    invalid(message);
  require_utf8(value, n);
  return {value, n};
}
int hex(unsigned char c) {
  if (c >= '0' && c <= '9')
    return c - '0';
  if (c >= 'a' && c <= 'f')
    return c - 'a' + 10;
  if (c >= 'A' && c <= 'F')
    return c - 'A' + 10;
  return -1;
}
std::string decode(const std::string &value, bool form) {
  std::string out;
  for (size_t n = 0; n < value.size(); ++n) {
    unsigned char c = value[n];
    if (c == '%') {
      if (value.size() - n < 3 || hex(value[n + 1]) < 0 ||
          hex(value[n + 2]) < 0)
        invalid("Malformed percent encoding in API request target");
      c = static_cast<unsigned char>(hex(value[n + 1]) * 16 +
                                     hex(value[n + 2]));
      n += 2;
    } else if (c == '+' && form)
      c = ' ';
    if (c < 32 || c == 127)
      invalid("API request target contains a control character");
    out.push_back(static_cast<char>(c));
  }
  require_utf8(out.data(), out.size());
  return out;
}
std::string query_json(const std::string &input, query_kind kind) {
  if (input.empty())
    return "{}";
  if (kind == query_kind::none)
    invalid("This API operation accepts no query parameters");
  std::map<std::string, std::string> scalars, metadata;
  std::vector<std::string> include;
  for (size_t first = 0; first < input.size();) {
    const auto end = input.find('&', first);
    const auto field = input.substr(first, end - first);
    const auto equal = field.find('=');
    if (equal == std::string::npos || !equal)
      invalid("API query parameters require key=value");
    const auto key = decode(field.substr(0, equal), true);
    const auto value = decode(field.substr(equal + 1), true);
    if (key == "include[]" &&
        (kind == query_kind::page || kind == query_kind::item)) {
      if (include.size() >= 16)
        invalid("API query has too many include values");
      include.push_back(value);
    } else if (kind == query_kind::chat && key.size() > 10 &&
               key.compare(0, 9, "metadata[") == 0 && key.back() == ']') {
      const auto name = key.substr(9, key.size() - 10);
      if (metadata.size() >= 16 || !metadata.emplace(name, value).second)
        invalid("API metadata query has duplicate or too many filters");
    } else {
      const bool page = kind != query_kind::item;
      if (!((page && (key == "after" || key == "limit" || key == "order")) ||
            (kind == query_kind::chat && key == "model")))
        invalid("Unknown or unavailable API query parameter");
      if (!scalars.emplace(key, value).second)
        invalid("API query contains a duplicate scalar parameter");
    }
    if (end == std::string::npos)
      break;
    first = end + 1;
    if (first == input.size())
      invalid("API query contains an empty parameter");
  }
  bounded_json out(128 * 1024);
  out.object();
  for (const auto &p : scalars) {
    out.key(p.first);
    if (p.first == "limit") {
      unsigned count = 0;
      const auto number = std::from_chars(
          p.second.data(), p.second.data() + p.second.size(), count);
      if (number.ec != std::errc{} ||
          number.ptr != p.second.data() + p.second.size() || count < 1 ||
          count > 100)
        invalid("API page limit requires an integer from 1 to 100");
      out.number(count);
    } else
      out.string(p.second);
  }
  if (!metadata.empty()) {
    out.key("metadata");
    out.object();
    for (const auto &p : metadata) {
      out.key(p.first);
      out.string(p.second);
    }
    out.end_object();
  }
  if (!include.empty()) {
    out.key("include");
    out.array();
    for (const auto &v : include)
      out.string(v);
    out.end_array();
  }
  out.end_object();
  return out.finish();
}
route resolve(const std::string &target, const std::string &method,
              ailo_api_info &info) {
  route r;
  if (target[0] != '/' || target.find('#') != std::string::npos ||
      target.find_first_of(" \r\n\t") != std::string::npos)
    invalid("API target must be an origin-form path without a fragment");
  const auto question = target.find('?');
  const auto path =
      target.substr(1, question == std::string::npos ? question : question - 1);
  for (size_t n = 0;;) {
    const auto end = path.find('/', n);
    auto part = decode(path.substr(n, end - n), false);
    if (part.empty())
      throw failure(AILO_NOT_FOUND, "Unknown API route");
    r.parts.push_back(std::move(part));
    if (end == std::string::npos)
      break;
    n = end + 1;
  }
  const auto &p = r.parts;
  query_kind q = query_kind::none;
  if (p.size() >= 2 && p[0] == "v1") {
    if (p[1] == "models") {
      if (p.size() == 2)
        r.destination = endpoint::models;
      if (p.size() == 3)
        r.destination = endpoint::model;
      r.allow = "GET";
    } else if (p[1] == "chat" && p.size() >= 3 && p[2] == "completions") {
      if (p.size() == 3) {
        r.destination = endpoint::chat;
        r.allow = "GET, POST";
        if (method == "GET")
          q = query_kind::chat;
      } else if (p.size() == 4) {
        r.destination = endpoint::chat_record;
        r.allow = "GET, POST, DELETE";
      } else if (p.size() == 5 && p[4] == "messages") {
        r.destination = endpoint::chat_messages;
        r.allow = "GET";
        q = query_kind::messages;
      }
    } else if (p[1] == "responses") {
      if (p.size() == 2) {
        r.destination = endpoint::responses;
        r.allow = "POST";
      } else if (p.size() == 3 && p[2] == "compact") {
        r.destination = endpoint::compact;
        r.allow = "POST";
      } else if (p.size() == 3) {
        r.destination = endpoint::response;
        r.allow = "GET, DELETE";
      } else if (p.size() == 4 && p[3] == "input_items") {
        r.destination = endpoint::response_input;
        r.allow = "GET";
        q = query_kind::page;
      }
    } else if (p[1] == "conversations") {
      if (p.size() == 2) {
        r.destination = endpoint::conversations;
        r.allow = "POST";
      } else if (p.size() == 3) {
        r.destination = endpoint::conversation;
        r.allow = "GET, POST, DELETE";
      } else if (p.size() == 4 && p[3] == "items") {
        r.destination = endpoint::items;
        r.allow = "GET, POST";
        if (method == "GET")
          q = query_kind::page;
      } else if (p.size() == 5 && p[3] == "items") {
        r.destination = endpoint::item;
        r.allow = "GET, DELETE";
        if (method == "GET")
          q = query_kind::item;
      }
    }
  }
  if (r.destination == endpoint::unknown)
    throw failure(AILO_NOT_FOUND, "Unknown API route");
  const bool allowed = (method == "GET" && std::strstr(r.allow, "GET")) ||
                       (method == "POST" && std::strstr(r.allow, "POST")) ||
                       (method == "DELETE" && std::strstr(r.allow, "DELETE"));
  if (!allowed) {
    info.http_status = 405;
    info.allow = r.allow;
    throw failure(AILO_UNSUPPORTED, "Method is not allowed for this API route");
  }
  r.query = query_json(
      question == std::string::npos ? "" : target.substr(question + 1), q);
  return r;
}
void model(wire_json &out, const state &self) {
  out.object();
  out.key("id");
  out.string(self.id);
  out.key("object");
  out.string("model");
  out.key("created");
  out.number(0); // Unknown; do not invent a training timestamp.
  out.key("owned_by");
  out.string("local"); // Serving deployment, not a license claim.
  out.end_object();
}
ailo_status dispatch(const route &r, bool get, bool post, ailo_client *c,
                     const void *body, size_t size, ailo_output_sink sink,
                     void *user, ailo_api_info *info, ailo_error *error) {
  const auto *q = r.query.data();
  const auto n = r.query.size();
  const auto &p = r.parts;
  switch (r.destination) {
  case endpoint::chat:
    return get ? ailo_client_chat_list(c, q, n, sink, user, info, error)
               : ailo_client_chat_completions(c, body, size, sink, user, info,
                                              error);
  case endpoint::chat_record:
    if (get)
      return ailo_client_chat_retrieve(c, p[3].c_str(), 0, sink, user, info,
                                       error);
    return post ? ailo_client_chat_update(c, p[3].c_str(), body, size, sink,
                                          user, info, error)
                : ailo_client_chat_delete(c, p[3].c_str(), sink, user, info,
                                          error);
  case endpoint::chat_messages:
    return ailo_client_chat_messages_list(c, p[3].c_str(), q, n, sink, user,
                                          info, error);
  case endpoint::responses:
    return ailo_client_responses(c, body, size, sink, user, info, error);
  case endpoint::response:
    return get ? ailo_client_response_retrieve(c, p[2].c_str(), sink, user,
                                               info, error)
               : ailo_client_response_delete(c, p[2].c_str(), sink, user, info,
                                             error);
  case endpoint::compact:
    return ailo_client_response_compact(c, body, size, sink, user, info, error);
  case endpoint::response_input:
    return ailo_client_response_input_items_list(c, p[2].c_str(), q, n, sink,
                                                 user, info, error);
  case endpoint::conversations:
    return ailo_client_conversation_create(c, body, size, sink, user, info,
                                           error);
  case endpoint::conversation:
    if (get)
      return ailo_client_conversation_retrieve(c, p[2].c_str(), sink, user,
                                               info, error);
    return post ? ailo_client_conversation_update(c, p[2].c_str(), body, size,
                                                  sink, user, info, error)
                : ailo_client_conversation_delete(c, p[2].c_str(), sink, user,
                                                  info, error);
  case endpoint::items:
    return get ? ailo_client_conversation_items_list(c, p[2].c_str(), q, n,
                                                     sink, user, info, error)
               : ailo_client_conversation_items_create(
                     c, p[2].c_str(), body, size, sink, user, info, error);
  case endpoint::item:
    return get ? ailo_client_conversation_item_retrieve(c, p[2].c_str(),
                                                        p[4].c_str(), q, n,
                                                        sink, user, info, error)
               : ailo_client_conversation_item_delete(
                     c, p[2].c_str(), p[4].c_str(), sink, user, info, error);
  default:
    break;
  }
  return ailo_error_set(error, AILO_INTERNAL_ERROR,
                        "Invalid API route dispatch");
}
} // namespace
} // namespace client
} // namespace ailo

extern "C" __attribute__((visibility("default"))) ailo_status
ailo_client_handle(ailo_client *client, const char *method, const char *target,
                   const void *body, size_t size, ailo_output_sink sink,
                   void *user, ailo_api_info *info, ailo_error *error) {
  using namespace ailo::client;
  if (info)
    *info = {};
  if (!info || !sink || (!body && size)) {
    if (info)
      info->http_status = 400;
    return ailo_error_set(error, AILO_INVALID_ARGUMENT,
                          "API info, sink and valid input bytes are required");
  }
  route r;
  std::string verb;
  const auto status = boundary(error, [&] {
    operation call(client);
    verb = bounded(method, 32, "An HTTP method is required (maximum 32 bytes)");
    for (unsigned char c : verb)
      if (!((c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z') ||
            (c >= '0' && c <= '9') || std::strchr("!#$%&'*+-.^_`|~", c)))
        invalid("Invalid HTTP method token");
    r = resolve(
        bounded(target, 16384, "An API target is required (maximum 16 KiB)"),
        verb, *info);
    if (verb != "POST" && size)
      invalid("GET and DELETE API operations accept no request body");
    if (size > 8 * 1024 * 1024)
      throw failure(AILO_OUT_OF_RANGE, "API request body exceeds 8 MiB");
    const bool discovery =
        r.destination == endpoint::models || r.destination == endpoint::model;
    if (!discovery) {
      // Preserve the admitted lock/cancellation through adapter entry. The
      // adapter consumes this handoff before invoking any user callback.
      operation_handoff next(call);
      ailo_error diagnostic{};
      const auto delegated =
          dispatch(r, verb == "GET", verb == "POST", client, size ? body : "{}",
                   size ? size : 2, sink, user, info, &diagnostic);
      if (delegated != AILO_OK)
        throw failure(delegated, diagnostic.message);
      return;
    }
    if (r.destination == endpoint::model && r.parts[2] != call.self.id)
      throw failure(AILO_UNKNOWN_MODEL, "Unknown served model ID");
    info->content_type = "application/json";
    wire_json out(api_sink(call.self, sink, user, *info));
    if (r.destination == endpoint::models) {
      out.object();
      out.key("object");
      out.string("list");
      out.key("data");
      out.array();
      model(out, call.self);
      out.end_array();
      out.end_object();
    } else
      model(out, call.self);
    out.finish();
  });
  if (info->http_status != 405)
    info->http_status = api_http_status(status);
  return status;
}
