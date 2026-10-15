#include "ailo_bounded_json_internal.hpp"
#include "ailo_openai_request_internal.hpp"
#include "ailo_sampling_internal.hpp"
#include "ailo_schema_internal.hpp"
#include <charconv>
#include <cmath>
#include <cstring>
#include <exception>
#include <set>

namespace ailo {
namespace client {
namespace {
using path = std::vector<std::string>;
enum class kind { object, array, string, number, boolean, null };
[[noreturn]] void invalid(const char *message) {
  throw failure(AILO_INVALID_ARGUMENT, message);
}
[[noreturn]] void unsupported() {
  throw failure(
      AILO_UNSUPPORTED,
      "Request field or value requires an unavailable API capability");
}
void expect(kind actual, kind wanted) {
  if (actual != wanted)
    invalid("An API request field has the wrong JSON type");
}
bool one_of(const std::string &value,
            std::initializer_list<const char *> options) {
  for (auto option : options)
    if (value == option)
      return true;
  return false;
}
size_t index(const std::string &value, size_t bound) {
  size_t result = 0;
  auto parsed =
      std::from_chars(value.data(), value.data() + value.size(), result);
  if (parsed.ec != std::errc{} || parsed.ptr != value.data() + value.size() ||
      result >= bound)
    invalid("An API request array exceeds its item limit");
  return result;
}
double number(const std::string &value, double low, double high) {
  double result = 0;
  auto parsed =
      std::from_chars(value.data(), value.data() + value.size(), result);
  if (parsed.ec != std::errc{} || parsed.ptr != value.data() + value.size() ||
      !std::isfinite(result) || result < low || result > high)
    invalid("An API request number is outside its supported range");
  return result;
}
size_t integer(const std::string &value, size_t low, size_t high) {
  size_t result = 0;
  const auto parsed =
      std::from_chars(value.data(), value.data() + value.size(), result);
  if (parsed.ec != std::errc{} || parsed.ptr != value.data() + value.size() ||
      result < low || result > high)
    invalid(
        "An API request count requires a nonnegative integer literal in range");
  return result;
}
void text_limit(const std::string &value, size_t limit, bool nonempty = false) {
  if (value.size() > limit || (nonempty && value.empty()) ||
      value.find('\0') != std::string::npos)
    invalid("An API request string is empty, too long or contains NUL");
}
void identifier(const std::string &value) {
  text_limit(value, 64, true);
  for (unsigned char c : value)
    if (!((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') ||
          (c >= '0' && c <= '9') || c == '_' || c == '-'))
      invalid("API function and format names require letters, digits, _ or -");
}
size_t characters(const std::string &value) {
  size_t count = 0;
  for (unsigned char c : value)
    count += (c & 0xc0) != 0x80;
  return count;
}
struct part {
  std::string type, text;
  bool has_text = false, output_metadata = false;
};
struct call {
  std::string type, id, name, arguments;
  bool has_arguments = false, function = false;
};
struct item {
  std::string type, role, id, call_id, name, arguments, encrypted, status;
  std::string content_field;
  std::vector<part> parts;
  std::vector<call> calls;
  bool has_content = false, content_null = false, has_arguments = false,
       has_encrypted = false, has_calls = false, has_summary = false;
  bool encrypted_null = false;
  bool content_string = false;
};
class decoder {
  openai_request &out;
  bool chat;
  bool compact, control, null_model = false;
  const char *selected_items;
  std::vector<item> input;
  std::vector<openai_function> allowed;
  std::set<std::string> root_keys, includes;
  size_t client_metadata_fields = 0;
  std::vector<std::set<std::string>> object_keys;
  std::string scalar, key, format_type, choice_type, choice_mode, context_type;
  bool format_object = false, choice_object = false, context_entry = false;
  bool named_choice_fields = false, allowed_choice_fields = false;
  bool input_present = false, stream_options = false, max_old = false,
       max_new = false, modalities = false, schema_wrapper = false,
       schema_fields = false, conversation_object = false;
  size_t key_count = 0;
  std::unique_ptr<bounded_json> capture;
  std::unique_ptr<bounded_json> item_capture;
  std::string *capture_target = nullptr;
  size_t capture_depth = 0;
  std::exception_ptr error;

  template <typename Work> lonejson_status safe(Work work) noexcept {
    try {
      work();
      return LONEJSON_STATUS_OK;
    } catch (...) {
      error = std::current_exception();
      return LONEJSON_STATUS_IO_ERROR;
    }
  }
  static path location(const lonejson_value_path *p) {
    path result;
    for (size_t i = 0; i < p->segment_count; ++i)
      result.emplace_back(p->segments[i].data, p->segments[i].len);
    return result;
  }
  template <kind K>
  static lonejson_status begin(void *u, const lonejson_value_path *p,
                               lonejson_error *) {
    auto &self = *static_cast<decoder *>(u);
    return self.safe([&] {
      if (K == kind::object)
        self.object_keys.emplace_back();
      if (K == kind::string || K == kind::number)
        self.scalar.clear();
      else
        self.value(location(p), K, {});
    });
  }
  template <kind K>
  static lonejson_status end(void *u, const lonejson_value_path *p,
                             lonejson_error *) {
    auto &self = *static_cast<decoder *>(u);
    return self.safe([&] {
      if (K == kind::string || K == kind::number)
        self.value(location(p), K, self.scalar);
      else if (self.capture) {
        if (K == kind::object)
          self.capture->end_object();
        else
          self.capture->end_array();
        if (p->segment_count == self.capture_depth) {
          *self.capture_target = self.capture->finish();
          self.capture.reset();
        }
      }
      if (self.item_capture && (K == kind::object || K == kind::array)) {
        if (K == kind::object)
          self.item_capture->end_object();
        else
          self.item_capture->end_array();
        if (p->segment_count == 2) {
          self.out.wire_items.push_back(self.item_capture->finish());
          self.out.wire_status.push_back(!self.input.back().status.empty());
          self.out.wire_simple.push_back(self.input.back().content_string &&
                                         self.input.back().type !=
                                             "function_call_output");
          self.item_capture.reset();
        }
      }
      if (K == kind::object)
        self.object_keys.pop_back();
    });
  }
  static lonejson_status chunk(void *u, const lonejson_value_path *,
                               const char *data, size_t size,
                               lonejson_error *) {
    auto &self = *static_cast<decoder *>(u);
    return self.safe([&] { self.scalar.append(data, size); });
  }
  static lonejson_status boolean(void *u, const lonejson_value_path *p, int v,
                                 lonejson_error *) {
    auto &self = *static_cast<decoder *>(u);
    return self.safe(
        [&] { self.value(location(p), kind::boolean, v ? "1" : "0"); });
  }
  static lonejson_status key_begin(void *u, const lonejson_value_path *,
                                   lonejson_error *) {
    static_cast<decoder *>(u)->key.clear();
    return LONEJSON_STATUS_OK;
  }
  static lonejson_status key_chunk(void *u, const lonejson_value_path *,
                                   const char *data, size_t size,
                                   lonejson_error *) {
    auto &self = *static_cast<decoder *>(u);
    return self.safe([&] { self.key.append(data, size); });
  }
  static lonejson_status key_end(void *u, const lonejson_value_path *p,
                                 lonejson_error *) {
    auto &self = *static_cast<decoder *>(u);
    return self.safe([&] {
      if (++self.key_count > 32768)
        invalid("API request exceeds 32768 object members");
      if (!self.object_keys.back().insert(self.key).second)
        invalid("API request contains a duplicate object member");
      if (!p->segment_count)
        self.root_keys.insert(self.key);
      if (self.capture)
        self.capture->key(self.key);
      if (self.item_capture)
        self.item_capture->key(self.key);
    });
  }
  void fragment(std::string &target, const path &p, kind k) {
    expect(k, kind::object);
    capture = std::make_unique<bounded_json>(65536);
    capture_target = &target;
    capture_depth = p.size();
    capture->object();
  }
  bool format_field(const path &p, kind k, const std::string &v) {
    const bool base = chat
                          ? p[0] == "response_format"
                          : p[0] == "text" && p.size() >= 2 && p[1] == "format";
    if (!base)
      return false;
    const size_t depth = chat ? 1 : 2;
    if (p.size() == depth) {
      if (k == kind::null && chat)
        return true;
      expect(k, kind::object);
      format_object = true;
      return true;
    }
    if (p.size() == depth + 1 && p.back() == "type") {
      expect(k, kind::string);
      format_type = v;
      return true;
    }
    if (chat && p.size() == 2 && p[1] == "json_schema") {
      expect(k, kind::object);
      schema_wrapper = true;
      return true;
    }
    if (p.size() != 3 || (chat && p[1] != "json_schema"))
      unsupported();
    schema_fields = true;
    const auto &field = p.back();
    if (field == "schema")
      fragment(out.schema, p, k);
    else if (field == "strict") {
      if (k != kind::null) {
        expect(k, kind::boolean);
        out.schema_strict = v == "1";
      }
    } else if (field == "name" || field == "description") {
      expect(k, kind::string);
      (field == "name" ? out.schema_name : out.schema_description) = v;
    } else
      unsupported();
    return true;
  }
  void choice(const path &p, kind k, const std::string &v) {
    if (p.size() == 1) {
      if (k == kind::object) {
        choice_object = true;
        return;
      }
      expect(k, kind::string);
      choice_mode = v;
      return;
    }
    if (p.size() == 2 && p[1] == "type") {
      expect(k, kind::string);
      choice_type = v;
      return;
    }
    if ((chat && p.size() == 2 &&
         (p[1] == "function" || p[1] == "allowed_tools"))) {
      expect(k, kind::object);
      (p[1] == "function" ? named_choice_fields : allowed_choice_fields) = true;
      return;
    }
    const bool named =
        chat ? p.size() == 3 && p[1] == "function" && p[2] == "name"
             : p.size() == 2 && p[1] == "name";
    if (named) {
      expect(k, kind::string);
      named_choice_fields = true;
      out.tool_name = v;
      return;
    }
    const size_t base = chat ? 2 : 1;
    allowed_choice_fields = true;
    if (chat && (p.size() < 3 || p[1] != "allowed_tools"))
      unsupported();
    if (p.size() == base + 1 && p.back() == "mode") {
      expect(k, kind::string);
      choice_mode = v;
    } else if (p.size() == base + 1 && p.back() == "tools")
      expect(k, kind::array);
    else if (p.size() > base + 1 && p[base] == "tools") {
      const size_t i = index(p[base + 1], 128);
      if (p.size() == base + 2) {
        expect(k, kind::object);
        if (i != allowed.size())
          invalid("Invalid allowed-tool array");
        allowed.emplace_back();
      } else if (chat && p.size() == base + 3 && p.back() == "function")
        expect(k, kind::object);
      else {
        const bool type = p.size() == base + 3 && p.back() == "type";
        const bool name = chat ? p.size() == base + 4 &&
                                     p[base + 2] == "function" &&
                                     p.back() == "name"
                               : p.size() == base + 3 && p.back() == "name";
        if (!type && !name)
          unsupported();
        expect(k, kind::string);
        (type ? allowed.at(i).type : allowed.at(i).name) = v;
      }
    } else
      unsupported();
  }
  void tool(const path &p, kind k, const std::string &v) {
    if (p.size() == 1) {
      expect(k, kind::array);
      return;
    }
    const size_t i = index(p[1], 128);
    if (p.size() == 2) {
      expect(k, kind::object);
      if (i != out.functions.size())
        invalid("Invalid tools array");
      out.functions.emplace_back();
      return;
    }
    auto &f = out.functions.at(i);
    if (p.size() == 3 && p[2] == "type") {
      expect(k, kind::string);
      f.type = v;
      return;
    }
    if (chat && p.size() == 3 && p[2] == "function") {
      expect(k, kind::object);
      return;
    }
    if ((chat && (p.size() != 4 || p[2] != "function")) ||
        (!chat && p.size() != 3))
      unsupported();
    const auto &field = p.back();
    if (field == "parameters") {
      if (chat || k != kind::null)
        fragment(f.parameters, p, k);
    } else if (field == "strict") {
      if (k != kind::null) {
        expect(k, kind::boolean);
        f.requested_strict = v == "1";
      }
    } else if (field == "name" || field == "description") {
      if (!chat && field == "description" && k == kind::null)
        return;
      expect(k, kind::string);
      (field == "name" ? f.name : f.description) = v;
    } else
      unsupported();
  }
  void message(const path &p, kind k, const std::string &v) {
    if (p.size() == 1) {
      input_present = true;
      if (!chat && k == kind::string) {
        item simple;
        simple.role = "user";
        simple.parts.push_back({"input_text", v, true});
        simple.has_content = true;
        input.push_back(std::move(simple));
      } else
        expect(k, kind::array);
      return;
    }
    const size_t i = index(p[1], 4096);
    if (p.size() == 2) {
      expect(k, kind::object);
      if (i != input.size())
        invalid("Invalid input item array");
      input.emplace_back();
      return;
    }
    auto &m = input.at(i);
    const auto &field = p[2];
    if (p.size() == 3) {
      if (field == "content" || (!chat && field == "output")) {
        if (m.has_content)
          invalid("Input item cannot contain both content and output");
        m.has_content = true;
        m.content_field = field;
        m.content_null = k == kind::null;
        m.content_string = k == kind::string;
        if (k == kind::string)
          m.parts.push_back({chat ? "text" : "input_text", v, true});
        else if (k != kind::null || !chat)
          expect(k, kind::array);
      } else if (!chat && field == "summary") {
        expect(k, kind::array);
        m.has_summary = true;
      } else if (!chat && field == "encrypted_content" && k == kind::null) {
        m.has_encrypted = m.encrypted_null = true;
      } else if (field == "tool_calls" && chat) {
        m.has_calls = true;
        if (k != kind::null)
          expect(k, kind::array);
      } else {
        expect(k, kind::string);
        if (field == "role")
          m.role = v;
        else if (field == "name") {
          identifier(v);
          m.name = v;
        } else if (chat && field == "tool_call_id")
          m.call_id = v;
        else if (!chat && field == "type") {
          text_limit(v, 64, true);
          m.type = v;
        } else if (!chat && field == "id") {
          text_limit(v, 256, true);
          m.id = v;
        } else if (!chat && field == "call_id") {
          text_limit(v, 256, true);
          m.call_id = v;
        } else if (!chat && field == "arguments") {
          m.arguments = v;
          m.has_arguments = true;
        } else if (!chat && field == "encrypted_content") {
          m.encrypted = v;
          m.has_encrypted = true;
        } else if (!chat && field == "status") {
          if (!one_of(v, {"completed", "incomplete"}))
            unsupported();
          m.status = v;
        } else
          unsupported();
      }
    } else if (field == "content" || (!chat && field == "output")) {
      const size_t j = index(p[3], 4096);
      if (p.size() == 4) {
        expect(k, kind::object);
        if (j != m.parts.size())
          invalid("Invalid message content array");
        m.parts.emplace_back();
      } else if (p.size() == 5 && (p[4] == "type" || p[4] == "text")) {
        expect(k, kind::string);
        auto &part = m.parts.at(j);
        if (p[4] == "type")
          part.type = v;
        else {
          part.text = v;
          part.has_text = true;
        }
      } else if (!chat && p.size() == 5 &&
                 one_of(p[4], {"annotations", "logprobs"})) {
        expect(k, kind::array);
        m.parts.at(j).output_metadata = true;
      } else if (!chat && p.size() >= 6 && p[4] == "logprobs") {
        (void)index(p[5], 1048576);
        if (p.size() == 6)
          expect(k, kind::object);
        else if (p.size() == 7 && p[6] == "top_logprobs")
          expect(k, kind::array);
        else if (p.size() == 8 && p[6] == "top_logprobs") {
          (void)index(p[7], 20);
          expect(k, kind::object);
        } else if ((p.size() == 7 ||
                    (p.size() == 9 && p[6] == "top_logprobs")) &&
                   one_of(p.back(), {"token", "logprob"})) {
          expect(k, p.back() == "token" ? kind::string : kind::number);
          if (p.back() == "logprob")
            (void)number(v, -1e38, 0);
        } else
          unsupported();
      } else
        unsupported();
    } else if (chat && field == "tool_calls") {
      const size_t j = index(p[3], 128);
      if (p.size() == 4) {
        expect(k, kind::object);
        if (j != m.calls.size())
          invalid("Invalid function-call array");
        m.calls.emplace_back();
      } else {
        auto &c = m.calls.at(j);
        if (p.size() == 5 && p[4] == "function") {
          expect(k, kind::object);
          c.function = true;
        } else {
          expect(k, kind::string);
          if (p.size() == 5 && p[4] == "type")
            c.type = v;
          else if (p.size() == 5 && p[4] == "id")
            c.id = v;
          else if (p.size() == 6 && p[4] == "function" && p[5] == "name")
            c.name = v;
          else if (p.size() == 6 && p[4] == "function" && p[5] == "arguments") {
            c.arguments = v;
            c.has_arguments = true;
          } else
            unsupported();
        }
      }
    } else
      unsupported();
  }
  void root(const path &p, kind k, const std::string &v) {
    const auto &field = p[0];
    if (p.size() != 1)
      unsupported();
    if (k == kind::null && one_of(field, {"store",
                                          "stream",
                                          "temperature",
                                          "top_p",
                                          "top_logprobs",
                                          "metadata",
                                          "user",
                                          "safety_identifier",
                                          "service_tier",
                                          "prompt_cache_key",
                                          "prompt_cache_retention",
                                          "max_tokens",
                                          "max_completion_tokens",
                                          "max_output_tokens",
                                          "max_tool_calls",
                                          "n",
                                          "seed",
                                          "frequency_penalty",
                                          "presence_penalty",
                                          "logprobs",
                                          "logit_bias",
                                          "instructions",
                                          "previous_response_id",
                                          "conversation",
                                          "background",
                                          "truncation",
                                          "reasoning_effort",
                                          "reasoning",
                                          "context_management",
                                          "include",
                                          "modalities"})) {
      // Validate protocol membership even for null; unknown fields cannot hide
      // behind a null value and acquire silently ignored semantics.
      if ((chat && one_of(field, {"max_output_tokens", "instructions",
                                  "previous_response_id", "conversation",
                                  "background", "truncation", "reasoning",
                                  "context_management", "include"})) ||
          (chat && field == "max_tool_calls") ||
          (!chat &&
           one_of(field, {"max_tokens", "max_completion_tokens", "n", "seed",
                          "frequency_penalty", "presence_penalty", "logprobs",
                          "logit_bias", "reasoning_effort", "modalities"})))
        unsupported();
      return;
    }
    auto &g = out.generation;
    if (field == "model") {
      expect(k, kind::string);
      out.model = v;
    } else if (one_of(field, {"store", "stream", "parallel_tool_calls"}) ||
               (!chat && field == "background") ||
               (chat && field == "logprobs")) {
      expect(k, kind::boolean);
      if (field == "store")
        out.store = v == "1";
      else if (field == "stream")
        out.streaming = v == "1";
      else if (field == "parallel_tool_calls")
        g.parallel_tool_calls = v == "1";
      else if (field == "logprobs")
        g.logprobs = v == "1";
      else if (v == "1")
        unsupported();
    } else if (one_of(field, {"temperature", "top_p", "top_logprobs"}) ||
               (chat && one_of(field, {"max_tokens", "max_completion_tokens",
                                       "n", "seed", "frequency_penalty",
                                       "presence_penalty"})) ||
               (!chat &&
                one_of(field, {"max_output_tokens", "max_tool_calls"}))) {
      expect(k, kind::number);
      if (field == "temperature")
        g.temperature = number(v, 0, 2);
      else if (field == "top_p") {
        g.top_p = number(v, 0, 1);
        if (!g.top_p)
          invalid("top_p must be greater than zero");
      } else if (field == "top_logprobs")
        g.top_logprobs = static_cast<unsigned>(integer(v, 0, 20));
      else if (field == "n")
        g.candidates = static_cast<unsigned>(integer(v, 1, 16));
      else if (field == "seed")
        g.seed = static_cast<ailo_u32>(integer(v, 0, UINT32_MAX));
      else if (field == "frequency_penalty")
        g.frequency_penalty = number(v, -2, 2);
      else if (field == "presence_penalty")
        g.presence_penalty = number(v, -2, 2);
      else if (field == "max_tool_calls")
        out.max_builtin_tool_calls = integer(v, 1, INT32_MAX);
      else {
        g.max_tokens = integer(v, 1, INT32_MAX);
        max_old |= field == "max_tokens";
        max_new |= field == "max_completion_tokens";
      }
    } else if (one_of(field, {"user", "safety_identifier", "service_tier",
                              "prompt_cache_key", "prompt_cache_retention"}) ||
               (chat && field == "reasoning_effort") ||
               (!chat && one_of(field, {"instructions", "previous_response_id",
                                        "conversation", "truncation"}))) {
      expect(k, kind::string);
      if (field == "user") {
        text_limit(v, 256);
        out.user = v;
      } else if (field == "safety_identifier") {
        text_limit(v, 512);
        if (characters(v) > 128)
          invalid("safety_identifier exceeds 128 characters");
        out.safety_identifier = v;
      } else if (field == "service_tier") {
        if (!one_of(v, {"auto", "default"}))
          unsupported();
      } else if (field == "prompt_cache_key") {
        text_limit(v, 256);
        out.cache_key = v;
      } else if (field == "prompt_cache_retention")
        unsupported();
      else if (field == "reasoning_effort")
        effort(v);
      else if (field == "instructions")
        out.instructions = v;
      else if (field == "previous_response_id") {
        text_limit(v, 256, true);
        out.previous_response_id = v;
      } else if (field == "conversation") {
        text_limit(v, 256, true);
        out.conversation_id = v;
      } else if (v != "disabled")
        unsupported();
    } else
      unsupported();
  }
  void effort(const std::string &v) {
    if (!one_of(v, {"none", "minimal", "low", "medium", "high", "xhigh"}))
      unsupported();
    if (v == "none")
      out.generation.reasoning = AILO_REASONING_DISABLED;
    else {
      out.effort = v;
      out.generation.reasoning = AILO_REASONING_ENABLED;
    }
  }
  void metadata_value(const path &p, kind k, const std::string &v) {
    if (p.size() == 1) {
      if (k != kind::null)
        expect(k, kind::object);
    } else if (p.size() == 2) {
      expect(k, kind::string);
      if (out.metadata.size() >= 16 || characters(p[1]) > 64 ||
          characters(v) > 512)
        invalid(
            "Metadata exceeds 16 entries or its key/value character limits");
      out.metadata.emplace(p[1], v);
    } else
      unsupported();
  }
  void value(const path &p, kind k, const std::string &v) {
    if (out.capture_wire && p.size() == 2 &&
        (p[0] == "input" || p[0] == "items") && k == kind::object)
      item_capture = std::make_unique<bounded_json>(8 * 1024 * 1024);
    if (item_capture) {
      switch (k) {
      case kind::object:
        item_capture->object();
        break;
      case kind::array:
        item_capture->array();
        break;
      case kind::string:
        item_capture->string(v);
        break;
      case kind::number:
        item_capture->number_text(v);
        break;
      case kind::boolean:
        item_capture->boolean(v == "1");
        break;
      case kind::null:
        item_capture->null();
        break;
      }
    }
    if (capture) {
      switch (k) {
      case kind::object:
        capture->object();
        break;
      case kind::array:
        capture->array();
        break;
      case kind::string:
        capture->string(v);
        break;
      case kind::number:
        capture->number_text(v);
        break;
      case kind::boolean:
        capture->boolean(v == "1");
        break;
      case kind::null:
        capture->null();
        break;
      }
      return;
    }
    if (p.empty()) {
      expect(k, kind::object);
      return;
    }
    const auto &field = p[0];
    if (selected_items) {
      if (field == selected_items)
        message(p, k, v);
      else if (p.size() == 1 && field == "ailo_model_identity") {
        expect(k, kind::string);
        text_limit(v, 256, true);
        out.record_identity = v;
      } else if (p.size() == 1 && field == "ailo_automatic") {
        expect(k, kind::boolean);
        out.record_automatic = v == "1";
      } else if (p.size() == 1 &&
                 (field == "ailo_revision" || field == "created_at")) {
        expect(k, kind::number);
        uint64_t n = 0;
        const auto parsed = std::from_chars(v.data(), v.data() + v.size(), n);
        if (parsed.ec != std::errc{} || parsed.ptr != v.data() + v.size())
          invalid("Invalid retained conversation timestamp/revision");
        (field == "ailo_revision" ? out.record_revision : out.record_created) =
            n;
      } else if (p.size() == 1 &&
                 (field == "ailo_deleted" || field == "ailo_rebuild")) {
        expect(k, kind::boolean);
        (field == "ailo_deleted" ? out.record_deleted : out.record_rebuild) =
            v == "1";
      } else if (p.size() == 1 && field == "id") {
        expect(k, kind::string);
        text_limit(v, 256, true);
        out.record_id = v;
      } else if (field == "metadata") {
        metadata_value(p, k, v);
      }
      return;
    }
    if (control) {
      const auto protocol = out.protocol;
      if (field == "metadata" &&
          (protocol == openai_protocol::conversation ||
           protocol == openai_protocol::conversation_update ||
           protocol == openai_protocol::chat_update ||
           protocol == openai_protocol::chat_list)) {
        metadata_value(p, k, v);
        return;
      }
      if (field == "items" &&
          (protocol == openai_protocol::conversation ||
           protocol == openai_protocol::conversation_items)) {
        if (!(protocol == openai_protocol::conversation && p.size() == 1 &&
              k == kind::null))
          message(p, k, v);
        return;
      }
      if (protocol == openai_protocol::chat_list && p.size() == 1 &&
          field == "model") {
        if (k != kind::null) {
          expect(k, kind::string);
          text_limit(v, 256, true);
          out.model = v;
        }
        return;
      }
      if (protocol != openai_protocol::conversation_query &&
          protocol != openai_protocol::chat_list &&
          protocol != openai_protocol::chat_messages &&
          protocol != openai_protocol::response_input_items)
        unsupported();
      if (p.size() == 1 && field == "after") {
        if (k != kind::null) {
          expect(k, kind::string);
          text_limit(v, 256, true);
          out.after = v;
        }
        return;
      }
      if (p.size() == 1 && field == "limit") {
        if (k != kind::null) {
          expect(k, kind::number);
          out.page_limit = integer(v, 1, 100);
        }
        return;
      }
      if (p.size() == 1 && field == "order") {
        if (k != kind::null) {
          expect(k, kind::string);
          if (!one_of(v, {"asc", "desc"}))
            invalid("Unknown item order");
          out.descending = v == "desc";
        }
        return;
      }
      if (field != "include" || protocol == openai_protocol::chat_list ||
          protocol == openai_protocol::chat_messages)
        unsupported();
    }
    if (compact && !one_of(field, {"model", "input", "instructions",
                                   "previous_response_id", "prompt_cache_key",
                                   "prompt_cache_retention", "service_tier"}))
      unsupported();
    if (compact && field == "model" && k == kind::null) {
      null_model = true;
      return;
    }
    if (field == (chat ? "messages" : "input")) {
      message(p, k, v);
      return;
    }
    if (field == "tools") {
      tool(p, k, v);
      return;
    }
    if (field == "tool_choice") {
      choice(p, k, v);
      return;
    }
    if (format_field(p, k, v))
      return;
    if (!chat && field == "text" && p.size() == 1) {
      expect(k, kind::object);
      return;
    }
    if (!chat && field == "client_metadata") {
      // Codex CLI telemetry extension. These untrusted strings confer no
      // authority and are not prompt input or retained application metadata.
      if (p.size() == 1) {
        expect(k, kind::object);
      } else if (p.size() == 2) {
        expect(k, kind::string);
        text_limit(p[1], 128, true);
        text_limit(v, 4096);
        if (++client_metadata_fields > 16)
          invalid("Client telemetry exceeds 16 string entries");
      } else
        invalid("Client telemetry must be a flat string map");
      return;
    }
    if (field == "metadata" && (k != kind::null || p.size() > 1)) {
      metadata_value(p, k, v);
      return;
    }
    if (field == "stream_options") {
      if (p.size() == 1) {
        if (k != kind::null) {
          expect(k, kind::object);
          stream_options = true;
        }
      } else if (p.size() == 2 && (p[1] == "include_obfuscation" ||
                                   (chat && p[1] == "include_usage"))) {
        expect(k, kind::boolean);
        (p[1] == "include_usage" ? out.include_usage
                                 : out.include_obfuscation) = v == "1";
      } else
        unsupported();
      return;
    }
    if (chat && field == "stop") {
      if (p.size() == 1 && (k == kind::null || k == kind::array))
        return;
      if (p.size() > 2)
        unsupported();
      expect(k, kind::string);
      text_limit(v, 4096, true);
      if (out.stops.size() == 4)
        invalid("At most four stop strings are supported");
      out.stops.push_back(v);
      return;
    }
    if (chat && field == "logit_bias" && (k != kind::null || p.size() > 1)) {
      if (p.size() == 1)
        expect(k, kind::object);
      else if (p.size() == 2) {
        expect(k, kind::number);
        if (out.biases.size() == 4096)
          invalid("At most 4096 logit biases are supported");
        const auto id =
            static_cast<ailo_i32>(index(p[1], size_t(INT32_MAX) + 1));
        out.biases.push_back({id, number(v, -100, 100)});
      } else
        unsupported();
      return;
    }
    if (!chat && field == "conversation" &&
        (k == kind::object || p.size() > 1)) {
      if (p.size() == 1) {
        expect(k, kind::object);
        conversation_object = true;
      } else if (p.size() == 2 && p[1] == "id") {
        expect(k, kind::string);
        text_limit(v, 256, true);
        out.conversation_id = v;
      } else
        unsupported();
      return;
    }
    if (!chat && field == "reasoning" && (k != kind::null || p.size() > 1)) {
      if (p.size() == 1)
        expect(k, kind::object);
      else if (p.size() == 2 && p[1] == "effort") {
        if (k != kind::null) {
          expect(k, kind::string);
          effort(v);
        }
      } else if (p.size() == 2 &&
                 one_of(p[1], {"summary", "generate_summary"}) &&
                 k == kind::null) {
      } else
        unsupported();
      return;
    }
    if (!chat && field == "context_management" &&
        (k != kind::null || p.size() > 1)) {
      if (p.size() == 1)
        expect(k, kind::array);
      else if (p.size() == 2) {
        if (p[1] != "0")
          invalid("Only one compaction policy is supported");
        expect(k, kind::object);
        context_entry = true;
      } else if (p.size() == 3 && p[2] == "type") {
        expect(k, kind::string);
        context_type = v;
      } else if (p.size() == 3 && p[2] == "compact_threshold") {
        if (k != kind::null) {
          expect(k, kind::number);
          out.compact_threshold = integer(v, 1, INT32_MAX);
        }
      } else
        unsupported();
      return;
    }
    if ((!chat && field == "include") || (chat && field == "modalities")) {
      if (p.size() == 1) {
        if (k != kind::null)
          expect(k, kind::array);
      } else if (p.size() == 2) {
        expect(k, kind::string);
        if (chat) {
          if (v != "text" || modalities)
            unsupported();
          modalities = true;
        } else {
          if (v != "message.output_text.logprobs" &&
              v != "reasoning.encrypted_content")
            unsupported();
          if (!includes.insert(v).second)
            invalid("Duplicate include value");
          if (v == "message.output_text.logprobs")
            out.generation.logprobs = 1;
          else
            out.include_reasoning = true;
        }
      } else
        unsupported();
      return;
    }
    root(p, k, v);
  }
  void append(const ailo_item &value) {
    if (out.input.items.size() == 4096)
      invalid("Expanded API input exceeds 4096 items");
    out.input.items.push_back(std::make_shared<owned_item>(
        value, out.input.bytes, 4 * 1024 * 1024, out.input.parts));
  }
  void finish_input() {
    if (!out.instructions.empty()) {
      ailo_content text{AILO_CONTENT_TEXT, out.instructions.data(),
                        out.instructions.size()};
      ailo_item value{};
      value.role = AILO_ROLE_DEVELOPER;
      value.content = &text;
      value.content_count = 1;
      append(value);
      out.instruction_items = 1;
    }
    for (auto &m : input) {
      text_limit(m.id, 256);
      text_limit(m.call_id, 256);
      text_limit(m.name, 64);
      text_limit(m.arguments, 4 * 1024 * 1024);
      text_limit(m.encrypted, 4 * 1024 * 1024);
      ailo_item value{};
      if (!m.id.empty())
        value.id = m.id.c_str();
      if (!m.status.empty() && !one_of(m.status, {"completed", "incomplete"}))
        unsupported();
      if (m.has_summary && m.type != "reasoning")
        invalid("Summary belongs to a reasoning item");
      if (m.type.empty() || m.type == "message") {
        if ((!chat && (m.content_field == "output" || !m.name.empty())) ||
            (m.content_null && m.role != "assistant") ||
            (m.has_calls && m.role != "assistant"))
          invalid("Message contains incompatible content or call fields");
        if (m.has_arguments || m.has_encrypted ||
            (!m.call_id.empty() && m.role != "tool"))
          invalid("Message contains fields for another item kind");
        if (m.role == "user")
          value.role = AILO_ROLE_USER;
        else if (m.role == "system")
          value.role = AILO_ROLE_SYSTEM;
        else if (m.role == "developer")
          value.role = AILO_ROLE_DEVELOPER;
        else if (m.role == "assistant")
          value.role = AILO_ROLE_ASSISTANT;
        else if (chat && m.role == "tool") {
          value.kind = AILO_ITEM_FUNCTION_RESULT;
          value.role = AILO_ROLE_TOOL;
          value.call_id = m.call_id.c_str();
        } else
          invalid("Message role is missing or unsupported");
        if (!m.name.empty())
          value.name = m.name.c_str();
        std::vector<ailo_content> parts;
        for (const auto &part : m.parts) {
          if (!part.has_text ||
              !(chat ? part.type == "text"
                     : one_of(part.type, {"input_text", "output_text"})))
            unsupported();
          if (part.output_metadata &&
              (part.type != "output_text" || m.role != "assistant"))
            invalid("Output metadata requires an assistant output_text part");
          if (!chat && part.type == "output_text" && m.role != "assistant")
            invalid("output_text requires an assistant message");
          parts.push_back(
              {AILO_CONTENT_TEXT, part.text.data(), part.text.size()});
        }
        if (!m.has_content && m.calls.empty())
          invalid("Message content is required");
        if (!parts.empty() || m.calls.empty()) {
          value.content = parts.data();
          value.content_count = parts.size();
          append(value);
        }
        if (!m.calls.empty() && m.role != "assistant")
          invalid("Only assistant messages contain tool calls");
        for (const auto &c : m.calls) {
          text_limit(c.id, 256, true);
          identifier(c.name);
          text_limit(c.arguments, 4 * 1024 * 1024, true);
          if (c.type != "function")
            unsupported();
          if (!c.function || !c.has_arguments || c.id.empty() || c.name.empty())
            invalid("Function call is incomplete");
          ailo_item call{};
          call.kind = AILO_ITEM_FUNCTION_CALL;
          call.call_id = c.id.c_str();
          call.name = c.name.c_str();
          call.arguments = c.arguments.c_str();
          append(call);
        }
      } else if (!chat && m.type == "function_call") {
        if (m.has_content || m.has_encrypted || !m.role.empty() ||
            !m.has_arguments || m.call_id.empty() || m.name.empty())
          invalid("Function call has missing or incompatible fields");
        value.kind = AILO_ITEM_FUNCTION_CALL;
        value.call_id = m.call_id.c_str();
        value.name = m.name.c_str();
        value.arguments = m.arguments.c_str();
        append(value);
      } else if (!chat && m.type == "function_call_output") {
        if (!m.has_content || m.content_field != "output" || m.has_arguments ||
            m.has_encrypted || !m.role.empty() || !m.name.empty() ||
            m.call_id.empty())
          invalid("Function output has missing or incompatible fields");
        std::vector<ailo_content> parts;
        for (const auto &part : m.parts) {
          if (!part.has_text || part.type != "input_text")
            unsupported();
          parts.push_back(
              {AILO_CONTENT_TEXT, part.text.data(), part.text.size()});
        }
        value.kind = AILO_ITEM_FUNCTION_RESULT;
        value.call_id = m.call_id.c_str();
        value.content = parts.data();
        value.content_count = parts.size();
        append(value);
      } else if (!chat && m.type == "reasoning") {
        const bool encrypted = m.has_encrypted && !m.encrypted_null;
        if ((!m.has_content && !encrypted) || m.has_arguments || m.has_calls ||
            !m.role.empty() || !m.name.empty() || !m.call_id.empty() ||
            !m.has_summary ||
            (encrypted && (m.encrypted.empty() || m.id.empty())))
          invalid("Reasoning item has missing or incompatible fields");
        std::vector<ailo_content> parts;
        for (const auto &part : m.parts) {
          if (part.type != "reasoning_text" || !part.has_text ||
              part.output_metadata)
            unsupported();
          parts.push_back(
              {AILO_CONTENT_TEXT, part.text.data(), part.text.size()});
        }
        value.kind = AILO_ITEM_REASONING;
        value.role = AILO_ROLE_ASSISTANT;
        value.content = parts.data();
        value.content_count = parts.size();
        value.encrypted_content = encrypted ? m.encrypted.c_str() : nullptr;
        append(value);
      } else if (!chat && m.type == "compaction") {
        if (!m.has_encrypted || m.encrypted.empty() || m.id.empty() ||
            m.has_content || m.has_arguments || !m.role.empty() ||
            !m.name.empty() || !m.call_id.empty())
          invalid("Compaction item has missing or incompatible fields");
        value.kind = AILO_ITEM_COMPACTION;
        value.encrypted_content = m.encrypted.c_str();
        append(value);
      } else
        unsupported();
    }
  }

public:
  explicit decoder(openai_request &request, const char *selected)
      : out(request), chat(request.protocol == openai_protocol::chat),
        compact(request.protocol == openai_protocol::compact),
        control(request.protocol >= openai_protocol::conversation),
        selected_items(selected) {
    if (request.protocol == openai_protocol::chat_list ||
        request.protocol == openai_protocol::chat_messages)
      request.descending = false;
  }
  void parse(const void *data, size_t size, const std::string &served_model) {
    if ((!data && size) || !size || size > 8 * 1024 * 1024)
      invalid("API request must contain between one byte and 8 MiB of JSON");
    auto config = lonejson_default_config();
    config.max_alloc_bytes = 4 * 1024 * 1024;
    config.max_depth = config.json_value_max_depth = 64;
    config.json_value_max_total_bytes = config.json_value_max_string_bytes =
        8 * 1024 * 1024;
    config.json_value_max_key_bytes = config.json_value_max_number_bytes = 1024;
    std::unique_ptr<lonejson, decltype(&lonejson_free)> runtime(
        lonejson_new(&config, nullptr), lonejson_free);
    if (!runtime)
      throw std::bad_alloc();
    auto visitor = lonejson_default_path_value_visitor();
    visitor.object_begin = begin<kind::object>;
    visitor.object_end = end<kind::object>;
    visitor.array_begin = begin<kind::array>;
    visitor.array_end = end<kind::array>;
    visitor.string_begin = begin<kind::string>;
    visitor.string_chunk = chunk;
    visitor.string_end = end<kind::string>;
    visitor.number_begin = begin<kind::number>;
    visitor.number_chunk = chunk;
    visitor.number_end = end<kind::number>;
    visitor.boolean_value = boolean;
    visitor.null_value = begin<kind::null>;
    visitor.object_key_begin = key_begin;
    visitor.object_key_chunk = key_chunk;
    visitor.object_key_end = key_end;
    auto status = lonejson_visit_path_value_buffer(runtime.get(), data, size,
                                                   &visitor, this, nullptr);
    if (error)
      std::rethrow_exception(error);
    if (status != LONEJSON_STATUS_OK)
      throw failure(
          status == LONEJSON_STATUS_ALLOCATION_FAILED ? AILO_NO_MEMORY
                                                      : AILO_INVALID_JSON,
          "API request contains malformed JSON or exceeds parser limits");
    if (selected_items) {
      if (!input_present && std::strcmp(selected_items, "__header__"))
        invalid("Retained record has no item array");
      out.model = served_model;
      finish_input();
      return;
    }
    if (control) {
      if ((out.protocol == openai_protocol::conversation_update ||
           out.protocol == openai_protocol::chat_update) &&
          !root_keys.count("metadata"))
        invalid("Update requires metadata");
      if (out.protocol == openai_protocol::conversation_items && !input_present)
        invalid("Conversation items requires an items array");
      if (input.size() > 20)
        invalid("At most 20 conversation items may be added at once");
      if (out.protocol != openai_protocol::chat_list)
        out.model = served_model;
      finish_input();
      return;
    }
    if (chat && !root_keys.count("model"))
      invalid("Chat Completions requires model");
    if (compact && !root_keys.count("model"))
      invalid("Compaction requires model");
    if (out.model.empty() && !chat && (!root_keys.count("model") || null_model))
      out.model = served_model;
    text_limit(out.model, 256, true);
    if (out.model != served_model)
      throw failure(AILO_UNKNOWN_MODEL, "Unknown served model");
    if (chat && (!input_present || input.empty()))
      invalid("Chat Completions requires messages");
    if (!chat && input.empty() && out.previous_response_id.empty() &&
        out.conversation_id.empty() && out.instructions.empty())
      invalid("Responses requires input or continuation state");
    if (!out.previous_response_id.empty() && !out.conversation_id.empty())
      invalid("conversation and previous_response_id are mutually exclusive");
    if (conversation_object && out.conversation_id.empty())
      invalid("Conversation object requires id");
    if (max_old && max_new)
      invalid("max_tokens and max_completion_tokens conflict");
    if (stream_options && !out.streaming)
      invalid("stream_options requires stream=true");
    if (context_entry) {
      if (context_type != "compaction")
        unsupported();
      out.automatic_compaction = true;
    }
    if (format_object) {
      if (format_type == "json_schema") {
        if (out.schema.empty() || out.schema_name.empty() ||
            (chat && !schema_wrapper))
          invalid("JSON schema format requires name and schema");
        identifier(out.schema_name);
        out.generation.output_format = AILO_OUTPUT_JSON_SCHEMA;
      } else if (format_type == "text" || format_type == "json_object") {
        if (schema_fields || schema_wrapper)
          invalid("Schema fields require json_schema format");
        out.generation.output_format =
            format_type == "text" ? AILO_OUTPUT_TEXT : AILO_OUTPUT_JSON_OBJECT;
      } else
        unsupported();
    }
    if (choice_object) {
      if (choice_type == "function") {
        identifier(out.tool_name);
        if (allowed_choice_fields || !allowed.empty() || !choice_mode.empty())
          invalid("Named choice contains allowed-tool fields");
        out.generation.tool_choice = AILO_TOOLS_NAMED;
      } else if (choice_type == "allowed_tools") {
        if (named_choice_fields || allowed.empty() || !out.tool_name.empty() ||
            !one_of(choice_mode, {"auto", "required"}))
          invalid("Allowed choice requires mode and tools");
        for (const auto &f : allowed) {
          if (f.type != "function")
            unsupported();
          identifier(f.name);
          out.allowed.push_back(f.name);
        }
        out.generation.tool_choice =
            choice_mode == "auto" ? AILO_TOOLS_AUTO : AILO_TOOLS_REQUIRED;
      } else
        unsupported();
    } else if (!choice_mode.empty()) {
      if (!one_of(choice_mode, {"none", "auto", "required"}))
        unsupported();
      out.generation.tool_choice = choice_mode == "none" ? AILO_TOOLS_NONE
                                   : choice_mode == "required"
                                       ? AILO_TOOLS_REQUIRED
                                       : AILO_TOOLS_AUTO;
    }
    for (auto &f : out.functions) {
      if (f.type != "function")
        unsupported();
      identifier(f.name);
      text_limit(f.description, 4 * 1024 * 1024);
      if (f.parameters.empty())
        f.parameters = "{\"type\":\"object\",\"properties\":{},"
                       "\"additionalProperties\":false}";
      f.parameters = openai_tool_parameters(
          f.parameters.c_str(), f.requested_strict, !chat, f.strict);
    }
    text_limit(out.instructions, 4 * 1024 * 1024);
    validate_sampling(out.generation);
    finish_input();
  }
};
} // namespace

openai_request::openai_request(openai_protocol format, const void *json,
                               size_t size, const std::string &served_model,
                               const char *selected_items, bool retain_wire)
    : protocol(format) {
  capture_wire = retain_wire;
  raw_json = json;
  raw_size = size;
  generation.temperature = 1;
  generation.top_p = 1;
  decoder(*this, selected_items).parse(json, size, served_model);
  items = input.views();
  generation.items = items.data();
  generation.item_count = items.size();
  for (const auto &f : functions)
    tools.push_back({f.name.c_str(), f.description.c_str(),
                     f.parameters.c_str(), f.strict});
  generation.tools = tools.data();
  generation.tool_count = tools.size();
  generation.logit_bias = biases.data();
  generation.logit_bias_count = biases.size();
  for (const auto &s : stops)
    stop_views.push_back(s.c_str());
  generation.stop = stop_views.data();
  generation.stop_count = stop_views.size();
  for (const auto &s : allowed)
    allowed_views.push_back(s.c_str());
  generation.allowed_tools = allowed_views.data();
  generation.allowed_tool_count = allowed_views.size();
  generation.json_schema = schema.empty() ? nullptr : schema.c_str();
  generation.tool_name = tool_name.empty() ? nullptr : tool_name.c_str();
  generation.prompt_cache_key = cache_key.empty() ? nullptr : cache_key.c_str();
  generation.reasoning_effort = effort.empty() ? nullptr : effort.c_str();
  validate_sampling(generation);
}
} // namespace client
} // namespace ailo
