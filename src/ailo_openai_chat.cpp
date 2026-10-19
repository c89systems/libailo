#include "ailo_logging_internal.hpp"
#include "ailo_media_internal.hpp"
#include "ailo_openai_internal.hpp"
#include "src/unicode.h"
#include <algorithm>
#include <cmath>
#include <cstring>
#include <deque>
#include <openssl/rand.h>

#define AILO_PUBLIC extern "C" __attribute__((visibility("default")))
namespace ailo {
namespace client {
namespace {
struct alternative {
  std::string bytes;
  double logprob;
};
struct probability {
  uint64_t start;
  std::string bytes, visible;
  double logprob;
  std::vector<alternative> top;
  size_t charge;
};
struct call_output {
  std::string id, name, arguments;
};
struct choice_output {
  std::string text;
  std::vector<call_output> calls;
  std::vector<probability> probabilities;
  ailo_finish_reason finish = AILO_FINISH_NONE;
};
std::string label(const std::string &raw) {
  std::string text;
  for (auto cpt : unicode_cpts_from_utf8(raw))
    text += unicode_cpt_to_utf8(cpt);
  return text;
}
void token(wire_json &out, const std::string &bytes, double value) {
  if (std::isnan(value) || value > 0)
    throw failure(AILO_MODEL_FAILED, "Invalid sampled token probability");
  out.key("token");
  out.string(label(bytes));
  out.key("bytes");
  out.array();
  for (unsigned char byte : bytes)
    out.number(byte);
  out.end_array();
  out.key("logprob");
  out.real(std::max(-9999.0, value));
}
void probability_json(wire_json &out, const probability &p) {
  out.object();
  token(out, p.bytes, p.logprob);
  out.key("top_logprobs");
  out.array();
  for (const auto &a : p.top) {
    out.object();
    token(out, a.bytes, a.logprob);
    out.end_object();
  }
  out.end_array();
  out.end_object();
}
void usage_json(wire_json &out, const ailo_usage &usage) {
  const auto prompt = detail::to_native(usage.prompt_tokens),
             completion = detail::to_native(usage.completion_tokens);
  out.object();
  out.key("prompt_tokens");
  out.number(prompt);
  out.key("completion_tokens");
  out.number(completion);
  out.key("total_tokens");
  out.number(prompt + completion);
  out.key("prompt_tokens_details");
  out.object();
  out.key("cached_tokens");
  out.number(detail::to_native(usage.cached_tokens));
  out.end_object();
  out.key("completion_tokens_details");
  out.object();
  out.key("reasoning_tokens");
  out.number(detail::to_native(usage.reasoning_tokens));
  out.end_object();
  out.end_object();
}
const char *finish_name(ailo_finish_reason finish) {
  return finish == AILO_FINISH_LENGTH       ? "length"
         : finish == AILO_FINISH_TOOL_CALLS ? "tool_calls"
                                            : "stop";
}
class chat_writer {
  state &self;
  openai_request &request;
  ailo_api_info &info;
  uint64_t created;
  byte_sink sink;
  bool retain;
  size_t charged = 0;
  std::vector<choice_output> choices;
  std::deque<probability> pending;
  size_t candidate = 0, call_index = 0;
  std::unique_ptr<record_transaction> record;
  std::exception_ptr problem;

  void charge(size_t size) {
    if (size > self.openai->memory_limit - charged)
      throw failure(AILO_OUT_OF_RANGE,
                    "API output exceeds its charged memory limit");
    charged += size;
  }
  void append(std::string &text, const void *data, size_t size) {
    charge(size);
    text.append(static_cast<const char *>(data), size);
  }
  void header(wire_json &out, bool chunk) {
    out.object();
    out.key("id");
    out.string(info.id);
    out.key("object");
    out.string(chunk ? "chat.completion.chunk" : "chat.completion");
    out.key("created");
    out.number(created);
    out.key("model");
    out.string(request.model);
    out.key("service_tier");
    out.string("default");
  }
  template <typename Emit> void frame(Emit emit) {
    sink("data: ", 6);
    size_t bytes = 0;
    wire_json out([&](const void *p, size_t n) {
      sink(p, n);
      bytes += n;
    });
    header(out, true);
    emit(out);
    if (request.include_obfuscation) {
      const size_t count = (256 - ((bytes + 18) % 256)) % 256;
      unsigned char random[256];
      if (count && RAND_bytes(random, static_cast<int>(count)) != 1)
        throw failure(AILO_IO_FAILED, "Cannot generate stream padding");
      std::string padding(count, 'a');
      for (size_t i = 0; i < count; ++i)
        padding[i] =
            "abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789_-"
                [random[i] & 63];
      out.key("obfuscation");
      out.string(padding);
    }
    out.end_object();
    out.finish();
    sink("\n\n", 2);
  }
  template <typename Delta>
  void chunk(Delta delta, const probability *prob = nullptr,
             ailo_finish_reason finish = AILO_FINISH_NONE) {
    if (!request.streaming)
      return;
    frame([&](wire_json &out) {
      out.key("choices");
      out.array();
      out.object();
      out.key("index");
      out.number(candidate);
      out.key("delta");
      out.object();
      delta(out);
      out.end_object();
      out.key("finish_reason");
      if (finish == AILO_FINISH_NONE)
        out.null();
      else
        out.string(finish_name(finish));
      out.key("logprobs");
      if (prob) {
        out.object();
        out.key("content");
        out.array();
        probability_json(out, *prob);
        out.end_array();
        out.key("refusal");
        out.null();
        out.end_object();
      } else
        out.null();
      out.end_object();
      out.end_array();
      if (request.include_usage) {
        out.key("usage");
        out.null();
      }
    });
  }
  void sampled(const ailo_token_info &t) {
    if (t.end_of_generation || !t.chosen.size)
      return;
    size_t size = sizeof(probability) + t.chosen.size;
    const size_t count =
        std::min(t.top_count, size_t(request.generation.top_logprobs));
    bool in_top_twenty = false;
    for (size_t i = 0; i < t.top_count; ++i)
      in_top_twenty |= t.top[i].token_id == t.chosen.token_id;
    for (size_t i = 0; i < count; ++i) {
      if (t.top[i].size > self.openai->memory_limit ||
          size > self.openai->memory_limit - t.top[i].size ||
          sizeof(alternative) >
              self.openai->memory_limit - size - t.top[i].size)
        throw failure(AILO_OUT_OF_RANGE,
                      "API probability metadata exceeds its memory limit");
      size += sizeof(alternative) + t.top[i].size;
    }
    charge(size);
    probability p{
        detail::to_native(t.byte_offset),
        std::string(static_cast<const char *>(t.chosen.data), t.chosen.size),
        {},
        in_top_twenty ? t.chosen.logprob : -9999.0,
        {},
        size};
    for (size_t i = 0; i < count; ++i)
      p.top.push_back(
          {std::string(static_cast<const char *>(t.top[i].data), t.top[i].size),
           t.top[i].logprob});
    pending.push_back(std::move(p));
  }
  void visible(const ailo_event &e) {
    if (!request.generation.logprobs)
      return;
    if (!e.raw_offset_valid)
      throw failure(AILO_INTERNAL_ERROR, "Text delta has no byte provenance");
    const uint64_t begin = detail::to_native(e.raw_offset),
                   end = begin + e.size;
    for (auto &p : pending) {
      const auto first = std::max(begin, p.start),
                 last = std::min(end, p.start + p.bytes.size());
      if (first < last) {
        append(p.visible, p.bytes.data() + first - p.start, last - first);
        p.charge += last - first;
      }
    }
  }
  void settle(uint64_t watermark) {
    while (!pending.empty() &&
           pending.front().start + pending.front().bytes.size() <= watermark) {
      auto &p = pending.front();
      if (p.visible.empty())
        charged -= p.charge;
      else {
        charged -= p.bytes.size();
        p.charge -= p.bytes.size();
        p.bytes = std::move(p.visible);
        chunk([](wire_json &) {}, &p);
        if (retain)
          choices[candidate].probabilities.push_back(std::move(p));
        else
          charged -= p.charge;
      }
      pending.pop_front();
    }
  }
  void input_json(byte_sink target, bool page = false) {
    wire_json out(std::move(target), self.openai->record_limit);
    out.array();
    size_t index = 0;
    for (const auto &item : request.items) {
      out.object();
      if (page) {
        out.key("id");
        out.string(std::string(info.id) + "-" + std::to_string(index++));
        out.key("name");
        if (item.name && item.kind != AILO_ITEM_FUNCTION_CALL)
          out.string(item.name);
        else
          out.null();
        out.key("content_parts");
        bool media = false;
        for (size_t i = 0; i < item.content_count; ++i)
          media |= item.content[i].kind == AILO_CONTENT_IMAGE ||
                   item.content[i].kind == AILO_CONTENT_AUDIO;
        if (media) {
          out.array();
          for (size_t i = 0; i < item.content_count; ++i) {
            if (item.content[i].kind != AILO_CONTENT_TEXT)
              write_media_part(out, item.content[i], true, true);
            else {
              out.object();
              out.key("type");
              out.string("text");
              out.key("text");
              out.string(item.content[i].data, item.content[i].size);
              out.end_object();
            }
          }
          out.end_array();
        } else
          out.null();
      }
      out.key("role");
      if (item.kind == AILO_ITEM_FUNCTION_CALL) {
        out.string("assistant");
        out.key("tool_calls");
        out.array();
        out.object();
        out.key("id");
        out.string(item.call_id);
        out.key("type");
        out.string("function");
        out.key("function");
        out.object();
        out.key("name");
        out.string(item.name);
        out.key("arguments");
        out.string(item.arguments);
        out.end_object();
        out.end_object();
        out.end_array();
        if (page) {
          out.key("content");
          out.null();
        }
      } else {
        out.string(item.kind == AILO_ITEM_FUNCTION_RESULT ? "tool"
                   : item.role == AILO_ROLE_SYSTEM        ? "system"
                   : item.role == AILO_ROLE_DEVELOPER     ? "developer"
                   : item.role == AILO_ROLE_ASSISTANT     ? "assistant"
                                                          : "user");
        if (item.kind == AILO_ITEM_FUNCTION_RESULT) {
          out.key("tool_call_id");
          out.string(item.call_id);
        } else if (item.name && !page) {
          out.key("name");
          out.string(item.name);
        }
        out.key("content");
        if (page)
          out.string_begin();
        else
          out.array();
        for (size_t i = 0; i < item.content_count; ++i) {
          if (page) {
            if (item.content[i].kind == AILO_CONTENT_TEXT)
              out.string_chunk(item.content[i].data, item.content[i].size);
            continue;
          }
          if (item.content[i].kind == AILO_CONTENT_IMAGE ||
              item.content[i].kind == AILO_CONTENT_AUDIO) {
            write_media_part(out, item.content[i], true);
            continue;
          }
          out.object();
          out.key("type");
          out.string("text");
          out.key("text");
          out.string(item.content[i].data, item.content[i].size);
          out.end_object();
        }
        if (page)
          out.string_end();
        else
          out.end_array();
      }
      out.end_object();
    }
    out.end_array();
    out.finish();
  }
  void result_json(byte_sink target) {
    wire_json out(std::move(target), self.openai->record_limit);
    header(out, false);
    out.key("choices");
    out.array();
    for (size_t i = 0; i < choices.size(); ++i) {
      const auto &choice = choices[i];
      out.object();
      out.key("index");
      out.number(i);
      out.key("message");
      out.object();
      out.key("role");
      out.string("assistant");
      out.key("content");
      if (choice.text.empty() && !choice.calls.empty())
        out.null();
      else
        out.string(choice.text);
      out.key("refusal");
      out.null();
      if (!choice.calls.empty()) {
        out.key("tool_calls");
        out.array();
        for (const auto &c : choice.calls) {
          out.object();
          out.key("id");
          out.string(c.id);
          out.key("type");
          out.string("function");
          out.key("function");
          out.object();
          out.key("name");
          out.string(c.name);
          out.key("arguments");
          out.string(c.arguments);
          out.end_object();
          out.end_object();
        }
        out.end_array();
      }
      out.end_object();
      out.key("finish_reason");
      out.string(finish_name(choice.finish));
      out.key("logprobs");
      if (request.generation.logprobs) {
        out.object();
        out.key("content");
        out.array();
        for (const auto &p : choice.probabilities)
          probability_json(out, p);
        out.end_array();
        out.key("refusal");
        out.null();
        out.end_object();
      } else
        out.null();
      out.end_object();
    }
    out.end_array();
    out.key("usage");
    usage_json(out, info.run.usage);
    out.key("metadata");
    out.object();
    for (const auto &p : request.metadata) {
      out.key(p.first);
      out.string(p.second);
    }
    out.end_object();
    out.end_object();
    out.finish();
  }
  void completed() {
    if (record) {
      result_json(record->section(AILO_RECORD_OUTPUT));
      record->commit();
      info.stored = 1;
    }
    if (!request.streaming)
      result_json(sink);
    else {
      for (candidate = 0; candidate < choices.size(); ++candidate)
        chunk([](wire_json &) {}, nullptr, choices[candidate].finish);
      if (request.include_usage)
        frame([&](wire_json &out) {
          out.key("choices");
          out.array();
          out.end_array();
          out.key("usage");
          usage_json(out, info.run.usage);
        });
      sink("data: [DONE]\n\n", 14);
    }
  }
  void receive(const ailo_event &e) {
    candidate = e.candidate;
    if (e.kind == AILO_EVENT_RUN_END) {
      completed();
      return;
    }
    if (candidate >= choices.size())
      throw failure(AILO_INTERNAL_ERROR,
                    "API candidate exceeds request bounds");
    auto &choice = choices[candidate];
    if (e.kind == AILO_EVENT_CHOICE_BEGIN) {
      call_index = 0;
      chunk([](wire_json &out) {
        out.key("role");
        out.string("assistant");
        out.key("content");
        out.string("");
      });
    } else if (e.kind == AILO_EVENT_TOKEN)
      sampled(*e.token);
    else if (e.kind == AILO_EVENT_TEXT_DELTA) {
      visible(e);
      if (retain)
        append(choice.text, e.data, e.size);
      chunk([&](wire_json &out) {
        out.key("content");
        out.string(e.data, e.size);
      });
    } else if (e.kind == AILO_EVENT_ITEM_BEGIN &&
               e.item_kind == AILO_ITEM_FUNCTION_CALL) {
      call_index = choice.calls.size();
      charge(sizeof(call_output) + std::strlen(e.call_id));
      choice.calls.push_back({e.call_id, {}, {}});
      chunk([&](wire_json &out) {
        out.key("tool_calls");
        out.array();
        out.object();
        out.key("index");
        out.number(call_index);
        out.key("id");
        out.string(e.call_id);
        out.key("type");
        out.string("function");
        out.key("function");
        out.object();
        out.end_object();
        out.end_object();
        out.end_array();
      });
    } else if (e.kind == AILO_EVENT_FUNCTION_NAME_DELTA ||
               e.kind == AILO_EVENT_FUNCTION_ARGUMENTS_DELTA) {
      if (call_index >= choice.calls.size())
        throw failure(AILO_INTERNAL_ERROR, "Function delta has no active call");
      const bool name = e.kind == AILO_EVENT_FUNCTION_NAME_DELTA;
      if (retain)
        append(name ? choice.calls[call_index].name
                    : choice.calls[call_index].arguments,
               e.data, e.size);
      chunk([&](wire_json &out) {
        out.key("tool_calls");
        out.array();
        out.object();
        out.key("index");
        out.number(call_index);
        out.key("function");
        out.object();
        out.key(name ? "name" : "arguments");
        out.string(e.data, e.size);
        out.end_object();
        out.end_object();
        out.end_array();
      });
    } else if (e.kind == AILO_EVENT_OUTPUT_PROGRESS)
      settle(detail::to_native(e.raw_offset));
    else if (e.kind == AILO_EVENT_CHOICE_END) {
      if (!pending.empty())
        throw failure(AILO_INTERNAL_ERROR,
                      "Unsettled API token probabilities at choice completion");
      choice.finish = e.finish_reason;
    }
  }

public:
  chat_writer(state &state, openai_request &input, ailo_api_info &output,
              ailo_output_sink callback, void *user, uint64_t now,
              const openai_retention_plan &plan)
      : self(state), request(input), info(output), created(now),
        sink(api_sink(state, callback, user, output)),
        retain(plan.retain_output_in_memory) {
    charge(sizeof(choice_output) * request.generation.candidates);
    choices.resize(request.generation.candidates);
    if (plan.store_response) {
      ailo_record_metadata meta{self.openai->policy.namespace_id.c_str(),
                                info.id,
                                AILO_RECORD_CHAT,
                                detail::from_unsigned(created),
                                detail::from_unsigned(plan.response_expires_at),
                                self.openai->record_limit};
      record = std::make_unique<record_transaction>(*self.openai, meta);
      input_json(record->section(AILO_RECORD_INPUT));
      input_json(record->section(AILO_RECORD_CHAT_MESSAGES), true);
      wire_json attributes(record->section(AILO_RECORD_METADATA),
                           self.openai->record_limit);
      attributes.object();
      for (const auto &field : request.metadata) {
        attributes.key(field.first);
        attributes.string(field.second);
      }
      attributes.end_object();
      attributes.finish();
    }
  }
  static int event(void *user, const ailo_event *e) noexcept {
    auto &self = *static_cast<chat_writer *>(user);
    try {
      self.receive(*e);
      return 1;
    } catch (...) {
      self.problem = std::current_exception();
      return -1;
    }
  }
  void rethrow() {
    if (problem)
      std::rethrow_exception(problem);
  }
};
} // namespace
} // namespace client
} // namespace ailo
AILO_PUBLIC ailo_status ailo_client_chat_completions(
    ailo_client *client, const void *json, size_t size, ailo_output_sink sink,
    void *user, ailo_api_info *info, ailo_error *error) {
  using namespace ailo::client;
  if (info)
    *info = {};
  if (!sink || !info) {
    if (info)
      info->http_status = 400;
    return ailo_error_set(error, AILO_INVALID_ARGUMENT,
                          "API sink and output info are required");
  }
  auto status = boundary(error, [&] {
    operation call(client);
    openai_request request(openai_protocol::chat, json, size, call.self.id);
    resolve_media(call.self, request);
    const auto now = api_now();
    auto plan = resolve_retention(request, call.self.openai->policy, now);
    info->content_type =
        request.streaming ? "text/event-stream" : "application/json";
    api_identity(*info, "chatcmpl-");
    chat_writer output(call.self, request, *info, sink, user, now, plan);
    log_scope logs(call.self.logging.get(), log_phase::generate);
    auto native = request.generation;
    // Chosen-token rank is independent of the requested alternative count.
    if (native.logprobs)
      native.top_logprobs = 20;
    try {
      generate(call.self, native, chat_writer::event, &output, info->run);
    } catch (...) {
      output.rethrow();
      throw;
    }
    output.rethrow();
  });
  if (!info->started)
    info->http_status = api_http_status(status);
  return status;
}
