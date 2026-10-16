// Test-only loopback transport. Production adapters serialize every event.
// The generate definition below replaces only the static archive's inference
// translation unit in THIS executable; it introduces no product test hook.
#include "ailo/handler.h"
#include "ailo_auto_compaction_internal.hpp"
#include "ailo_client_internal.hpp"
#include "ailo_reasoning_internal.hpp"
#include "ailo_tools_internal.hpp"
#include "serving_repository.hpp"
#include <arpa/inet.h>
#include <charconv>
#include <csignal>
#include <cstdio>
#include <cstring>
#include <fstream>
#include <map>
#include <netinet/tcp.h>
#include <poll.h>
#include <sstream>
#include <string>
#include <sys/socket.h>
#include <thread>
#include <unistd.h>

namespace {
volatile std::sig_atomic_t stopping = 0;
unsigned generations = 0, tool_calls = 0, tool_results = 0, answers = 0;
unsigned summaries = 0, current_request = 0;
std::string directory;
using serving_test::json;
void stop(int) { stopping = 1; }
void require(bool condition, const char *message) {
  if (!condition)
    throw std::runtime_error(message);
}
struct socket_owner {
  int fd;
  ~socket_owner() {
    if (fd >= 0)
      close(fd);
  }
};
int send_all(int fd, const void *bytes, size_t size) {
  const char *p = static_cast<const char *>(bytes);
  while (size) {
    const auto n = send(fd, p, size, MSG_NOSIGNAL);
    if (n < 0 && errno == EINTR)
      continue;
    if (n <= 0)
      return 0;
    p += n;
    size -= static_cast<size_t>(n);
  }
  return 1;
}
int begin(void *u, const ailo_http_response *headers) {
  const int fd = *static_cast<int *>(u);
  std::ostringstream out;
  out << "HTTP/1.1 " << headers->status
      << " Result\r\nContent-Type: " << headers->content_type
      << "\r\nCache-Control: " << headers->cache_control
      << "\r\nX-Request-ID: " << headers->request_id
      << "\r\nTransfer-Encoding: chunked\r\nConnection: close\r\n";
  if (headers->allow)
    out << "Allow: " << headers->allow << "\r\n";
  if (headers->retry_after)
    out << "Retry-After: " << headers->retry_after << "\r\n";
  out << "\r\n";
  const auto bytes = out.str();
  return send_all(fd, bytes.data(), bytes.size());
}
int write(void *u, const void *bytes, size_t size) {
  const int fd = *static_cast<int *>(u);
  char length[32];
  const auto n = std::snprintf(length, sizeof(length), "%zx\r\n", size);
  return send_all(fd, length, static_cast<size_t>(n)) &&
         send_all(fd, bytes, size) && send_all(fd, "\r\n", 2);
}
int finish(void *u) { return send_all(*static_cast<int *>(u), "0\r\n\r\n", 5); }
void abort_response(void *u) { shutdown(*static_cast<int *>(u), SHUT_RDWR); }
std::string lower(std::string value) {
  for (auto &c : value)
    if (c >= 'A' && c <= 'Z')
      c += 'a' - 'A';
  return value;
}
std::string receive(int fd, size_t maximum) {
  char bytes[4096];
  const auto n = recv(fd, bytes, sizeof(bytes), 0);
  require(n > 0 && static_cast<size_t>(n) <= maximum,
          "Invalid fixture request size/read");
  return {bytes, static_cast<size_t>(n)};
}
void request(int fd, const char *model, unsigned number) {
  current_request = number;
  constexpr size_t limit = 8 * 1024 * 1024;
  std::string input;
  while (input.find("\r\n\r\n") == std::string::npos) {
    input += receive(fd, 32768 - input.size());
    require(input.size() < 32768, "Fixture request headers exceed 32 KiB");
  }
  const auto boundary = input.find("\r\n\r\n") + 4;
  std::istringstream lines(input.substr(0, boundary));
  std::string line, method, target, version;
  std::getline(lines, line);
  std::istringstream first(line);
  first >> method >> target >> version;
  require(version == "HTTP/1.1", "Fixture requires HTTP/1.1");
  std::map<std::string, std::string> headers;
  while (std::getline(lines, line) && line != "\r") {
    const auto colon = line.find(':');
    require(colon != std::string::npos, "Invalid fixture header");
    auto value = line.substr(colon + 1);
    if (!value.empty() && value.back() == '\r')
      value.pop_back();
    const auto start = value.find_first_not_of(" \t");
    value = start == std::string::npos ? "" : value.substr(start);
    require(headers.emplace(lower(line.substr(0, colon)), value).second,
            "Duplicate fixture header");
  }
  require(!headers.count("transfer-encoding"),
          "Fixture expects a fixed-length request body");
  size_t length = 0;
  if (headers.count("content-length")) {
    const auto &text = headers.at("content-length");
    const auto parsed =
        std::from_chars(text.data(), text.data() + text.size(), length);
    require(parsed.ec == std::errc() &&
                parsed.ptr == text.data() + text.size() && length <= limit,
            "Invalid fixture Content-Length");
  }
  while (input.size() - boundary < length)
    input += receive(fd, limit + 32768 - input.size());
  require(input.size() - boundary == length,
          "Fixture rejects pipelined/trailing request bytes");
  // Request inputs are explicitly materialized. Generated output is never
  // captured or spooled here; the transport forwards each borrowed byte range.
  std::ofstream saved(directory + "/request-" + std::to_string(number) +
                      ".json");
  saved.write(input.data() + boundary, static_cast<std::streamsize>(length));
  saved.close();
  const auto control = serving_test::load(directory + "/control.json");
  serving_test::repository repository(directory, control.value("fault", ""));
  auto config = ailo_client_default_config();
  config.source.local_path = model;
  config.model_id = "ailo-fixture";
  config.lazy = 1;
  config.backend = AILO_BACKEND_CPU;
  config.context_tokens = 32768;
  config.threads = 1;
  auto api = ailo_openai_default_config();
  const auto ns = control.value("namespace", "tenant-A");
  api.namespace_id = ns.c_str();
  api.repository = &repository.api;
  auto policy = ailo_compaction_default_policy();
  policy.target_tokens = 9000;
  policy.recent_turns = 0;
  policy.summary_tokens = 512;
  policy.summary_context_tokens = 32768;
  api.compaction = &policy;
  config.openai = &api;
  const std::string key(32,
                        'k'); // Public test key, never used outside fixtures.
  auto keys = ailo_compactor_default_config();
  keys.namespace_id = "ailo-serving-tests";
  keys.current_key = {"fixture", key.data(), key.size()};
  ailo_compactor *owner = nullptr;
  ailo_error error{};
  require(ailo_compactor_new(&keys, &owner, &error) == AILO_OK, error.message);
  std::unique_ptr<ailo_compactor, decltype(&ailo_compactor_destroy)> compactor(
      owner, ailo_compactor_destroy);
  config.compactor = owner;
  config.chat_template =
      "{% if tools %}{{tools | tojson}}{% endif %}{% for m in messages "
      "%}{{m.role}}:{{m.content}}{% if m.tool_calls is defined %}{% for c in "
      "m.tool_calls %}<call>{\"name\":{{c.function.name | "
      "tojson}},\"arguments\":{% if c.function.arguments is string "
      "%}{{c.function.arguments}}{% else %}{{c.function.arguments | "
      "tojson}}{% endif %}}</call>{% endfor %}{% endif %}|{% endfor %}{% if "
      "add_generation_prompt %}assistant:{% endif %}";
  ailo_client *raw = nullptr;
  require(ailo_client_new(&config, &raw, &error) == AILO_OK, error.message);
  std::unique_ptr<ailo_client, decltype(&ailo_client_destroy)> client(
      raw, ailo_client_destroy);
  auto handler_config = ailo_handler_default_config();
  handler_config.client = raw;
  ailo_handler *serving = nullptr;
  require(ailo_handler_new(&handler_config, &serving, &error) == AILO_OK,
          error.message);
  std::unique_ptr<ailo_handler, decltype(&ailo_handler_destroy)> handler(
      serving, ailo_handler_destroy);
  ailo_http_request incoming{};
  incoming.method = method.c_str();
  incoming.target = target.c_str();
  incoming.body = input.data() + boundary;
  incoming.body_size = length;
  incoming.content_type = headers.count("content-type")
                              ? headers.at("content-type").c_str()
                              : nullptr;
  incoming.content_encoding = headers.count("content-encoding")
                                  ? headers.at("content-encoding").c_str()
                                  : nullptr;
  ailo_http_transport outgoing{&fd, begin, write, finish, abort_response};
  ailo_handler_info info{};
  const auto status =
      serving->handle(serving, &incoming, &outgoing, &info, &error);
  auto receipt = repository.stats;
  receipt["target"] = target;
  receipt["method"] = method;
  receipt["status"] = info.http_status;
  receipt["result"] = ailo_status_name(status);
  receipt["stored"] = bool(info.api.stored);
  receipt["id"] = info.api.id;
  receipt["generations"] = generations;
  receipt["summaries"] = summaries;
  serving_test::save(directory + "/receipt-" + std::to_string(number) + ".json",
                     receipt);
  serving_test::save(directory + "/latest.json", receipt);
  std::fprintf(stderr,
               "request=%u method=%s target=%s status=%d result=%s "
               "completed=%d bytes=%u diagnostic=%s\n",
               number, method.c_str(), target.c_str(), info.http_status,
               ailo_status_name(status), info.completed, info.bytes_written.lo,
               error.message);
}
} // namespace

namespace ailo {
namespace client {
void generate(state &self, const ailo_request &request, ailo_event_sink sink,
              void *user, ailo_run_info &info,
              continuation_transaction *history, const compaction_task *task) {
  const unsigned generation = ++generations;
  require(generation <= 256, "Fixture exceeded its generation bound");
  self.ensure_loaded();
  std::unique_ptr<automatic_compaction> automatic;
  std::unique_ptr<compaction_change> changed;
  if (request.auto_compaction) {
    require(history != nullptr, "Fixture automatic history is missing");
    automatic = std::make_unique<automatic_compaction>(
        self, *history, *request.auto_compaction, task);
    auto tools = prepare_tools(request);
    const auto rendered =
        render_prompt(self, prepare_replay(self, request), tools, request);
    if (prompt_token_count(self, rendered.prompt) >=
        automatic->policy.trigger_tokens) {
      changed = std::make_unique<compaction_change>(automatic->propose(0));
      automatic->adopt(*changed);
    }
  }
  const auto conversation = prepare_replay(self, request);
  std::string effective, latest;
  for (const auto &m : conversation.messages) {
    effective += m.role + ":" + m.content + "\n";
    for (const auto &c : m.calls)
      effective += c.id + ":" + c.arguments + "\n";
    if (m.role == "user")
      latest = m.content;
  }
  serving_test::save(directory + "/generation-" + std::to_string(generation) +
                         ".json",
                     {{"request", current_request},
                      {"effective", effective},
                      {"latest", latest},
                      {"automatic", bool(changed)},
                      {"items", request.item_count}});
  const auto marker = [](const std::string &s, const std::string &prefix) {
    const auto at = s.rfind(prefix);
    if (at == std::string::npos)
      return std::string();
    const auto begin = at + prefix.size();
    return s.substr(
        begin,
        s.find_first_not_of(
            "abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789_-",
            begin) -
            begin);
  };
  std::string name, text;
  const bool summary =
      request.json_schema && std::strstr(request.json_schema, "\"facts\"");
  const auto memory = marker(effective, "AILO_REMEMBER:");
  const auto expected = marker(latest, "AILO_EXPECT:");
  const auto finding = marker(latest, "AILO_CHECK_TOOL:");
  const auto command = marker(latest, "AILO_TOOL:");
  bool received = false;
  const auto call_id = "call_ailo_" + command;
  if (!command.empty())
    for (size_t n = 0; n < request.item_count; ++n) {
      const auto &item = request.items[n];
      if (item.kind == AILO_ITEM_FUNCTION_RESULT && item.call_id &&
          call_id == item.call_id)
        for (size_t c = 0; c < item.content_count; ++c)
          received |=
              std::string(static_cast<const char *>(item.content[c].data),
                          item.content[c].size)
                  .find("AILO_TOOL_OK_" + command) != std::string::npos;
    }
  if (summary) {
    ++summaries;
    const auto &part = request.items[request.item_count - 1].content[0];
    const auto sources =
        json::parse(static_cast<const char *>(part.data),
                    static_cast<const char *>(part.data) + part.size);
    auto facts = json::array();
    for (const auto &source : sources) {
      const auto remembered = marker(source.dump(), "AILO_REMEMBER:");
      if (!remembered.empty())
        facts.push_back({{"source", source.at("source")},
                         {"category", "identifier"},
                         {"text", "AILO_REMEMBER:" + remembered}});
      if (source.at("kind") == "function_call") {
        require(std::any_of(sources.begin(), sources.end(),
                            [&](const json &row) {
                              return row.at("kind") == "function_result" &&
                                     row.at("call_id") == source.at("call_id");
                            }),
                "Compaction split a complete function call/result group");
      }
      if (source.at("kind") == "function_result") {
        const auto result = marker(source.dump(), "AILO_TOOL_OK_");
        if (!result.empty())
          facts.push_back({{"source", source.at("source")},
                           {"category", "finding"},
                           {"text", "AILO_TOOL_OK_" + result}});
      }
    }
    if (facts.empty())
      facts.push_back({{"source", sources.at(0).at("source")},
                       {"category", "other"},
                       {"text", "Earlier task completed."}});
    text = json{{"facts", facts}}.dump();
  } else if ((!expected.empty() && memory != expected) ||
             (!finding.empty() &&
              effective.find("AILO_TOOL_OK_" + finding) == std::string::npos)) {
    throw failure(AILO_MODEL_FAILED,
                  "Fixture resolved history lost the expected memory");
  } else if (!command.empty() && !received) {
    for (size_t n = 0; n < request.tool_count; ++n) {
      const auto &tool = request.tools[n];
      if (!std::strcmp(tool.name, "exec_command")) {
        name = tool.name;
        text = json{{"cmd", "printf 'AILO_TOOL_OK_" + command + "\\n'"},
                    {"max_output_tokens", 128},
                    {"yield_time_ms", 1000}}
                   .dump();
        break;
      }
      if (!std::strcmp(tool.name, "shell_command")) {
        name = tool.name;
        text = json{{"command", "printf 'AILO_TOOL_OK_" + command + "\\n'"},
                    {"timeout_ms", 1000}}
                   .dump();
        break;
      }
      if (!std::strcmp(tool.name, "shell")) {
        name = tool.name;
        text =
            json{{"command",
                  {"bash", "-c", "printf 'AILO_TOOL_OK_" + command + "\\n'"}},
                 {"timeout_ms", 1000}}
                .dump();
        break;
      }
      if (!std::strcmp(tool.name, "fixture_echo")) {
        name = tool.name;
        text = json{{"value", command}}.dump();
        break;
      }
    }
    require(!name.empty(),
            "Agent must advertise a supported function shell tool");
    ++tool_calls;
  } else {
    if (received)
      ++tool_results;
    ++answers;
    text = !command.empty()    ? "AILO_DONE:" + command
           : !expected.empty() ? "AILO_RECALLED:" + memory
                               : "AILO_ACK:" + memory;
  }
  const bool function = !name.empty();
  info = {};
  std::snprintf(info.id, sizeof(info.id), "00000000-0000-4000-8000-%012u",
                generation);
  info.actual_backend = AILO_BACKEND_CPU;
  if (changed) {
    info.usage.compaction_passes = changed->info.summary_passes;
    info.usage.compaction_prompt_tokens = changed->info.summary_prompt_tokens;
    info.usage.compaction_completion_tokens =
        changed->info.summary_completion_tokens;
  }
  ailo_event event{};
  event.model_id = self.id.c_str();
  event.run_id = info.id;
  event.item_kind = function ? AILO_ITEM_FUNCTION_CALL : AILO_ITEM_MESSAGE;
  const auto item_id = std::string(function ? "fc_" : "msg_") + info.id;
  event.item_id = item_id.c_str();
  event.call_id = function ? call_id.c_str() : nullptr;
  unsigned sequence = 0, tokens = 0;
  const auto emit = [&](ailo_event_kind kind, const void *data = nullptr,
                        size_t size = 0) {
    if (self.is_cancelled())
      throw failure(AILO_CANCELLED, "Fixture generation cancelled");
    event.kind = kind;
    event.data = data;
    event.size = size;
    event.usage = info.usage;
    event.sequence = detail::from_unsigned(sequence++);
    const int accepted = sink(user, &event);
    if (accepted <= 0)
      throw failure(accepted ? AILO_SINK_FAILED : AILO_CANCELLED,
                    "Fixture event consumer stopped");
  };
  emit(AILO_EVENT_RUN_BEGIN);
  if (changed) {
    event.compaction = &changed->info;
    event.compacted_items = changed->items.data();
    event.compacted_item_count = changed->items.size();
    emit(AILO_EVENT_COMPACTION);
    event.compaction = nullptr;
    event.compacted_items = nullptr;
    event.compacted_item_count = 0;
  }
  emit(AILO_EVENT_CHOICE_BEGIN);
  emit(AILO_EVENT_ITEM_BEGIN);
  if (!function)
    emit(AILO_EVENT_CONTENT_BEGIN);
  const auto stream = [&](const std::string &bytes, ailo_event_kind kind) {
    for (char c : bytes) {
      ailo_token_info token{};
      token.chosen.data = &c;
      token.chosen.size = 1;
      token.chosen.token_id = static_cast<unsigned char>(c);
      token.index = detail::from_unsigned(tokens);
      token.byte_offset = detail::from_unsigned(tokens);
      event.raw_offset = detail::from_unsigned(tokens);
      event.raw_offset_valid = 1;
      info.usage.completion_tokens = detail::from_unsigned(++tokens);
      event.token = &token;
      if (request.logprobs)
        emit(AILO_EVENT_TOKEN);
      event.token = nullptr;
      emit(kind, &c, 1);
      if (!summary && tokens == 1 &&
          std::ifstream(directory + "/hold-stream")) {
        std::ofstream(directory + "/first-delta") << "ready\n";
        unsigned waits = 0;
        while (!std::ifstream(directory + "/release-stream") && ++waits < 500 &&
               !stopping)
          std::this_thread::sleep_for(std::chrono::milliseconds(10));
        require(waits < 500 && !stopping,
                "HTTP consumer did not release first delta");
      }
      if (request.logprobs) {
        event.raw_offset = detail::from_unsigned(tokens);
        emit(AILO_EVENT_OUTPUT_PROGRESS);
      }
    }
  };
  if (function)
    stream(name, AILO_EVENT_FUNCTION_NAME_DELTA);
  stream(text, function ? AILO_EVENT_FUNCTION_ARGUMENTS_DELTA
                        : AILO_EVENT_TEXT_DELTA);
  event.finish_reason = function ? AILO_FINISH_TOOL_CALLS : AILO_FINISH_STOP;
  if (!function)
    emit(AILO_EVENT_CONTENT_END);
  emit(AILO_EVENT_ITEM_END);
  emit(AILO_EVENT_CHOICE_END);
  info.finish_reason = event.finish_reason;
  info.completed_candidates = 1;
  emit(AILO_EVENT_USAGE);
  emit(AILO_EVENT_RUN_END);
  std::fprintf(
      stderr, "fixture calls=%u results=%u answers=%u summaries=%u tokens=%u\n",
      tool_calls, tool_results, answers, summaries, tokens);
}
} // namespace client
} // namespace ailo

int main(int argc, char **argv) {
  try {
    require(argc == 3, "Usage: ailo_serving_fixture MODEL WORK_DIRECTORY");
    directory = argv[2];
    socket_owner listener{socket(AF_INET, SOCK_STREAM, 0)};
    require(listener.fd >= 0, "Fixture socket failed");
    sockaddr_in address{};
    address.sin_family = AF_INET;
    address.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
    require(bind(listener.fd, reinterpret_cast<sockaddr *>(&address),
                 sizeof(address)) == 0,
            "Fixture bind failed");
    socklen_t length = sizeof(address);
    require(getsockname(listener.fd, reinterpret_cast<sockaddr *>(&address),
                        &length) == 0 &&
                listen(listener.fd, 4) == 0,
            "Fixture listen failed");
    std::signal(SIGTERM, stop);
    std::signal(SIGINT, stop);
    {
      std::ofstream ready(directory + "/port");
      ready << ntohs(address.sin_port) << '\n';
    }
    unsigned count = 0;
    while (!stopping && count < 256) {
      pollfd pending{listener.fd, POLLIN, 0};
      if (poll(&pending, 1, 250) <= 0)
        continue;
      socket_owner peer{accept(listener.fd, nullptr, nullptr)};
      require(peer.fd >= 0, "Fixture accept failed");
      const int enabled = 1;
      const timeval timeout{10, 0};
      setsockopt(peer.fd, IPPROTO_TCP, TCP_NODELAY, &enabled, sizeof(enabled));
      setsockopt(peer.fd, SOL_SOCKET, SO_RCVTIMEO, &timeout, sizeof(timeout));
      setsockopt(peer.fd, SOL_SOCKET, SO_SNDTIMEO, &timeout, sizeof(timeout));
      request(peer.fd, argv[1], ++count);
    }
    return 0;
  } catch (const std::exception &e) {
    std::fprintf(stderr, "serving fixture: %s\n", e.what());
    return 1;
  }
}
