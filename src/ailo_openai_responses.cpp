#include "ailo_cache_internal.hpp"
#include "ailo_compaction_internal.hpp"
#include "ailo_logging_internal.hpp"
#include "ailo_openai_internal.hpp"
#include "src/unicode.h"
#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstring>
#include <deque>
#include <openssl/rand.h>

#define AILO_PUBLIC extern "C" __attribute__((visibility("default")))
namespace ailo {
namespace client {
namespace {
struct response_alternative {
  std::string bytes;
  double logprob;
};
struct response_probability {
  std::string bytes;
  double logprob;
  std::vector<response_alternative> top;
};
struct projection {
  size_t item;
  std::string bytes;
};
struct pending_probability {
  uint64_t start;
  response_probability value;
  std::vector<projection> visible;
  size_t charge;
};
struct response_item {
  ailo_item_kind kind;
  std::string id, call_id, name, text;
  std::vector<response_probability> probabilities;
  bool done = false, incomplete = false, added = false;
  std::string encrypted;
  std::shared_ptr<transcript> window;
};
std::string token_label(const std::string &bytes) {
  std::string text;
  for (auto cpt : unicode_cpts_from_utf8(bytes))
    text += unicode_cpt_to_utf8(cpt);
  return text;
}
void token_json(wire_json &out, const std::string &bytes, double logprob) {
  if (std::isnan(logprob) || logprob > 0)
    throw failure(AILO_MODEL_FAILED, "Invalid response token probability");
  out.key("token");
  out.string(token_label(bytes));
  out.key("logprob");
  out.real(std::max(-9999.0, logprob));
}
void probability_json(wire_json &out, const response_probability &p) {
  out.object();
  token_json(out, p.bytes, p.logprob);
  out.key("top_logprobs");
  out.array();
  for (const auto &a : p.top) {
    out.object();
    token_json(out, a.bytes, a.logprob);
    out.end_object();
  }
  out.end_array();
  out.end_object();
}
void canonical_item(wire_json &out, const ailo_item &item) {
  out.object();
  if (item.id) {
    out.key("id");
    out.string(item.id);
  }
  out.key("type");
  if (item.kind == AILO_ITEM_COMPACTION) {
    out.string("compaction");
    out.key("encrypted_content");
    out.string(item.encrypted_content);
  } else if (item.kind == AILO_ITEM_FUNCTION_CALL) {
    out.string("function_call");
    out.key("call_id");
    out.string(item.call_id);
    out.key("name");
    out.string(item.name);
    out.key("arguments");
    out.string(item.arguments);
  } else {
    const bool result = item.kind == AILO_ITEM_FUNCTION_RESULT;
    const bool reasoning = item.kind == AILO_ITEM_REASONING;
    out.string(result      ? "function_call_output"
               : reasoning ? "reasoning"
                           : "message");
    if (result) {
      out.key("call_id");
      out.string(item.call_id);
    } else if (reasoning) {
      out.key("summary");
      out.array();
      out.end_array();
    } else {
      out.key("role");
      out.string(item.role == AILO_ROLE_SYSTEM      ? "system"
                 : item.role == AILO_ROLE_DEVELOPER ? "developer"
                 : item.role == AILO_ROLE_ASSISTANT ? "assistant"
                                                    : "user");
    }
    out.key(result ? "output" : "content");
    out.array();
    for (size_t i = 0; i < item.content_count; ++i) {
      out.object();
      out.key("type");
      out.string(reasoning ? "reasoning_text"
                 : !result && item.role == AILO_ROLE_ASSISTANT ? "output_text"
                                                               : "input_text");
      out.key("text");
      out.string(item.content[i].data, item.content[i].size);
      if (!result && !reasoning && item.role == AILO_ROLE_ASSISTANT) {
        out.key("annotations");
        out.array();
        out.end_array();
        out.key("logprobs");
        out.array();
        out.end_array();
      }
      out.end_object();
    }
    out.end_array();
  }
  out.end_object();
}
void prior_input(state &self, openai_request &request) {
  if (request.previous_response_id.empty())
    return;
  auto section = [&](ailo_record_section part) {
    std::string bytes;
    read_record(*self.openai, AILO_RECORD_RESPONSE,
                request.previous_response_id.c_str(), part,
                [&](const void *p, size_t n) {
                  if (n > 8 * 1024 * 1024 - bytes.size())
                    throw failure(AILO_OUT_OF_RANGE,
                                  "Retained response input exceeds 8 MiB");
                  bytes.append(static_cast<const char *>(p), n);
                });
    return bytes;
  };
  const auto input_bytes = section(AILO_RECORD_INPUT);
  openai_request prior(openai_protocol::responses, input_bytes.data(),
                       input_bytes.size(), self.id, "input");
  self.ensure_loaded();
  identify_cache(self, true);
  if (prior.record_identity != self.conversation_identity)
    throw failure(AILO_INVALID_ARGUMENT,
                  "Retained response model/template identity is incompatible");
  openai_request output(openai_protocol::responses, input_bytes.data(),
                        input_bytes.size(), self.id, "output");
  transcript merged;
  auto add = [&](const std::shared_ptr<const owned_item> &item) {
    if (merged.items.size() == 4096 || item->parts.size() > 4096 - merged.parts)
      throw failure(AILO_OUT_OF_RANGE,
                    "Response lineage exceeds its item/part budget");
    merged.items.push_back(std::make_shared<owned_item>(
        item->view(), merged.bytes, 4 * 1024 * 1024, merged.parts));
  };
  for (size_t i = 0; i < request.instruction_items; ++i)
    add(request.input.items[i]);
  for (const auto &i : prior.input.items)
    add(i);
  for (const auto &i : output.input.items)
    add(i);
  for (size_t i = request.instruction_items; i < request.input.items.size();
       ++i)
    add(request.input.items[i]);
  request.input = std::move(merged);
  request.items = request.input.views();
  request.generation.items = request.items.data();
  request.generation.item_count = request.items.size();
}
class response_writer {
  state &self;
  openai_request &request;
  ailo_api_info &info;
  byte_sink sink;
  uint64_t created, completed_at = 0, sequence = 0;
  size_t charged = 0, current = 0, next_done = 0;
  bool store, frame_open = false;
  bool begun = false;
  size_t active_control = SIZE_MAX;
  std::chrono::steady_clock::time_point last_progress;
  std::vector<response_item> items;
  std::deque<pending_probability> pending;
  std::unique_ptr<record_transaction> record;
  std::exception_ptr problem;

  void charge(size_t n) {
    if (n > self.openai->memory_limit - charged)
      throw failure(AILO_OUT_OF_RANGE,
                    "Response output exceeds its charged memory limit");
    charged += n;
  }
  void append(std::string &s, const void *p, size_t n) {
    charge(n);
    s.append(static_cast<const char *>(p), n);
  }
  void nullable(wire_json &out, const std::string &s) {
    if (s.empty())
      out.null();
    else
      out.string(s);
  }
  void probabilities(wire_json &out, const response_item &i) {
    out.array();
    for (const auto &p : i.probabilities)
      probability_json(out, p);
    out.end_array();
  }
  void part(wire_json &out, const response_item &i, bool full,
            bool diagnostics = true) {
    out.object();
    out.key("type");
    out.string(i.kind == AILO_ITEM_REASONING ? "reasoning_text"
                                             : "output_text");
    out.key("text");
    out.string(full ? i.text : "");
    if (i.kind != AILO_ITEM_REASONING) {
      out.key("annotations");
      out.array();
      out.end_array();
      out.key("logprobs");
      if (full && diagnostics)
        probabilities(out, i);
      else {
        out.array();
        out.end_array();
      }
    }
    out.end_object();
  }
  void item(wire_json &out, const response_item &i, bool full,
            bool diagnostics = true) {
    out.object();
    out.key("id");
    out.string(i.id);
    out.key("type");
    if (i.kind == AILO_ITEM_COMPACTION) {
      out.string("compaction");
      out.key("encrypted_content");
      out.string(full ? i.encrypted : "");
      out.end_object();
      return;
    }
    out.string(i.kind == AILO_ITEM_FUNCTION_CALL ? "function_call"
               : i.kind == AILO_ITEM_REASONING   ? "reasoning"
                                                 : "message");
    out.key("status");
    out.string(!full                     ? "in_progress"
               : !i.done || i.incomplete ? "incomplete"
                                         : "completed");
    if (i.kind == AILO_ITEM_FUNCTION_CALL) {
      out.key("call_id");
      out.string(i.call_id);
      out.key("name");
      out.string(i.name);
      out.key("arguments");
      out.string(full ? i.text : "");
    } else {
      if (i.kind == AILO_ITEM_REASONING) {
        out.key("summary");
        out.array();
        out.end_array();
      } else {
        out.key("role");
        out.string("assistant");
      }
      out.key("content");
      out.array();
      if (full)
        part(out, i, true, diagnostics);
      out.end_array();
    }
    out.end_object();
  }
  void usage(wire_json &out) {
    const auto &u = info.run.usage;
    out.object();
    out.key("input_tokens");
    const auto input = detail::to_native(u.prompt_tokens) +
                       detail::to_native(u.compaction_prompt_tokens);
    const auto output = detail::to_native(u.completion_tokens) +
                        detail::to_native(u.compaction_completion_tokens);
    out.number(input);
    out.key("output_tokens");
    out.number(output);
    out.key("total_tokens");
    out.number(input + output);
    out.key("input_tokens_details");
    out.object();
    out.key("cached_tokens");
    out.number(detail::to_native(u.cached_tokens));
    out.end_object();
    out.key("output_tokens_details");
    out.object();
    out.key("reasoning_tokens");
    out.number(detail::to_native(u.reasoning_tokens));
    out.end_object();
    out.end_object();
  }
  void attributes(wire_json &out) {
    out.object();
    for (const auto &p : request.metadata) {
      out.key(p.first);
      out.string(p.second);
    }
    out.end_object();
  }
  void response(wire_json &out, const char *status,
                ailo_status failed = AILO_OK) {
    out.object();
    out.key("id");
    out.string(info.id);
    out.key("object");
    out.string("response");
    out.key("created_at");
    out.number(created);
    out.key("completed_at");
    if (completed_at)
      out.number(completed_at);
    else
      out.null();
    out.key("status");
    out.string(status);
    out.key("model");
    out.string(request.model);
    out.key("error");
    if (failed != AILO_OK) {
      out.object();
      out.key("code");
      out.string("server_error");
      out.key("message");
      out.string(ailo_status_name(failed));
      out.end_object();
    } else
      out.null();
    out.key("incomplete_details");
    if (!std::strcmp(status, "incomplete")) {
      out.object();
      out.key("reason");
      out.string("max_output_tokens");
      out.end_object();
    } else
      out.null();
    out.key("instructions");
    nullable(out, request.instructions);
    out.key("max_output_tokens");
    if (request.generation.max_tokens)
      out.number(request.generation.max_tokens);
    else
      out.null();
    out.key("output");
    out.array();
    if (std::strcmp(status, "in_progress"))
      for (const auto &i : items)
        item(out, i, true);
    out.end_array();
    out.key("parallel_tool_calls");
    out.boolean(request.generation.parallel_tool_calls);
    out.key("previous_response_id");
    nullable(out, request.previous_response_id);
    if (request.automatic_compaction) {
      out.key("context_management");
      out.array();
      out.object();
      out.key("type");
      out.string("compaction");
      out.key("compact_threshold");
      out.number(request.compact_threshold);
      out.end_object();
      out.end_array();
    }
    out.key("store");
    out.boolean(store);
    out.key("temperature");
    out.real(request.generation.temperature);
    out.key("top_p");
    out.real(request.generation.top_p);
    out.key("top_logprobs");
    out.number(request.generation.top_logprobs);
    out.key("text");
    out.object();
    out.key("format");
    out.object();
    out.key("type");
    out.string(request.generation.output_format == AILO_OUTPUT_JSON_SCHEMA
                   ? "json_schema"
               : request.generation.output_format == AILO_OUTPUT_JSON_OBJECT
                   ? "json_object"
                   : "text");
    if (request.generation.output_format == AILO_OUTPUT_JSON_SCHEMA) {
      out.key("name");
      out.string(request.schema_name);
      out.key("schema");
      out.value(request.schema);
      out.key("strict");
      out.boolean(request.schema_strict);
      if (!request.schema_description.empty()) {
        out.key("description");
        out.string(request.schema_description);
      }
    }
    out.end_object();
    out.end_object();
    out.key("tools");
    out.array();
    for (const auto &f : request.functions) {
      out.object();
      out.key("type");
      out.string("function");
      out.key("name");
      out.string(f.name);
      out.key("description");
      out.string(f.description);
      out.key("parameters");
      out.value(f.parameters);
      out.key("strict");
      out.boolean(f.strict);
      out.end_object();
    }
    out.end_array();
    out.key("tool_choice");
    if (!request.allowed.empty()) {
      out.object();
      out.key("type");
      out.string("allowed_tools");
      out.key("mode");
      out.string(request.generation.tool_choice == AILO_TOOLS_REQUIRED
                     ? "required"
                     : "auto");
      out.key("tools");
      out.array();
      for (const auto &name : request.allowed) {
        out.object();
        out.key("type");
        out.string("function");
        out.key("name");
        out.string(name);
        out.end_object();
      }
      out.end_array();
      out.end_object();
    } else if (request.generation.tool_choice == AILO_TOOLS_NAMED) {
      out.object();
      out.key("type");
      out.string("function");
      out.key("name");
      out.string(request.tool_name);
      out.end_object();
    } else
      out.string(request.generation.tool_choice == AILO_TOOLS_NONE ? "none"
                 : request.generation.tool_choice == AILO_TOOLS_REQUIRED
                     ? "required"
                     : "auto");
    out.key("reasoning");
    out.object();
    out.key("effort");
    nullable(out, request.effort);
    out.key("summary");
    out.null();
    out.end_object();
    out.key("truncation");
    out.string("disabled");
    out.key("service_tier");
    out.string("default");
    out.key("user");
    nullable(out, request.user);
    out.key("metadata");
    attributes(out);
    out.key("usage");
    if (!std::strcmp(status, "in_progress"))
      out.null();
    else
      usage(out);
    out.end_object();
  }
  template <typename Emit> void frame(const char *type, Emit emit) {
    if (!request.streaming)
      return;
    frame_open = true;
    sink("event: ", 7);
    sink(type, std::strlen(type));
    sink("\ndata: ", 7);
    size_t bytes = 0;
    wire_json out(
        [&](const void *p, size_t n) {
          sink(p, n);
          bytes += n;
        },
        self.openai->record_limit);
    out.object();
    out.key("type");
    out.string(type);
    out.key("sequence_number");
    out.number(sequence++);
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
    frame_open = false;
  }
  void location(wire_json &out, size_t index, bool content = true) {
    out.key("item_id");
    out.string(items[index].id);
    out.key("output_index");
    out.number(index);
    if (content) {
      out.key("content_index");
      out.number(0);
    }
  }
  void added(size_t index) {
    auto &i = items[index];
    if (i.added)
      return;
    i.added = true;
    frame("response.output_item.added", [&](wire_json &out) {
      out.key("output_index");
      out.number(index);
      out.key("item");
      item(out, i, false);
    });
  }
  void flush_done() {
    // Stored streams publish the atomic record before successful done events.
    if (record || (request.automatic_compaction && !completed_at))
      return;
    while (next_done < items.size() && items[next_done].done) {
      bool waiting = false;
      for (const auto &p : pending)
        for (const auto &v : p.visible)
          waiting |= v.item == next_done;
      if (waiting)
        break;
      const size_t index = next_done++;
      const auto &i = items[index];
      if (i.kind == AILO_ITEM_FUNCTION_CALL) {
        frame("response.function_call_arguments.done", [&](wire_json &out) {
          location(out, index, false);
          out.key("arguments");
          out.string(i.text);
        });
      } else if (i.kind != AILO_ITEM_COMPACTION) {
        frame(i.kind == AILO_ITEM_REASONING ? "response.reasoning_text.done"
                                            : "response.output_text.done",
              [&](wire_json &out) {
                location(out, index);
                out.key("text");
                out.string(i.text);
                if (i.kind != AILO_ITEM_REASONING) {
                  out.key("logprobs");
                  probabilities(out, i);
                }
              });
        frame("response.content_part.done", [&](wire_json &out) {
          location(out, index);
          out.key("part");
          part(out, i, true);
        });
      }
      frame("response.output_item.done", [&](wire_json &out) {
        out.key("output_index");
        out.number(index);
        out.key("item");
        item(out, i, true);
      });
    }
  }
  void sampled(const ailo_token_info &t) {
    if (t.end_of_generation || !t.chosen.size)
      return;
    size_t size = sizeof(pending_probability) + t.chosen.size;
    size_t count =
        std::min(t.top_count, size_t(request.generation.top_logprobs));
    bool in_top = false;
    for (size_t i = 0; i < t.top_count; ++i)
      in_top |= t.top[i].token_id == t.chosen.token_id;
    charge(size);
    pending_probability p{
        detail::to_native(t.byte_offset),
        {std::string(static_cast<const char *>(t.chosen.data), t.chosen.size),
         in_top ? t.chosen.logprob : -9999.0,
         {}},
        {},
        size};
    for (size_t i = 0; i < count; ++i) {
      size_t n = sizeof(response_alternative) + t.top[i].size;
      charge(n);
      p.charge += n;
      p.value.top.push_back(
          {std::string(static_cast<const char *>(t.top[i].data), t.top[i].size),
           t.top[i].logprob});
    }
    pending.push_back(std::move(p));
  }
  void visible(const ailo_event &e) {
    if (!request.generation.logprobs)
      return;
    const auto begin = detail::to_native(e.raw_offset), end = begin + e.size;
    for (auto &p : pending) {
      const auto first = std::max(begin, p.start),
                 last = std::min(end, p.start + p.value.bytes.size());
      if (first >= last)
        continue;
      if (p.visible.empty() || p.visible.back().item != current) {
        charge(sizeof(projection));
        p.charge += sizeof(projection);
        p.visible.push_back({current, {}});
      }
      append(p.visible.back().bytes, p.value.bytes.data() + first - p.start,
             last - first);
      p.charge += last - first;
    }
  }
  void settle(uint64_t end) {
    while (!pending.empty() &&
           pending.front().start + pending.front().value.bytes.size() <= end) {
      auto &p = pending.front();
      // Each projection owns its copied alternatives and visible vocabulary
      // bytes.
      for (const auto &v : p.visible) {
        size_t n = sizeof(response_probability) + v.bytes.size();
        charge(n);
        for (const auto &a : p.value.top)
          charge(sizeof(response_alternative) + a.bytes.size());
        response_probability projected{v.bytes, p.value.logprob, p.value.top};
        items[v.item].probabilities.push_back(std::move(projected));
        frame("response.output_text.delta", [&](wire_json &out) {
          location(out, v.item);
          out.key("delta");
          out.string("");
          out.key("logprobs");
          out.array();
          probability_json(out, items[v.item].probabilities.back());
          out.end_array();
        });
      }
      charged -= p.charge;
      pending.pop_front();
    }
    flush_done();
  }
  void completed() {
    if (!pending.empty())
      throw failure(AILO_INTERNAL_ERROR,
                    "Unsettled response probabilities at completion");
    completed_at = api_now();
    seal_controls();
    const char *status = info.run.finish_reason == AILO_FINISH_LENGTH
                             ? "incomplete"
                             : "completed";
    if (record) {
      identify_cache(self, true);
      wire_json input(record->section(AILO_RECORD_INPUT),
                      self.openai->record_limit);
      input.object();
      input.key("ailo_model_identity");
      input.string(self.conversation_identity);
      input.key("input");
      input.array();
      const auto source = active_control == SIZE_MAX
                              ? request.items
                              : items[active_control].window->views();
      const bool compacted =
          std::any_of(source.begin(), source.end(), [](const ailo_item &i) {
            return i.kind == AILO_ITEM_COMPACTION;
          });
      if (compacted) {
        const auto lineage =
            api_compacted_window(self, source, request.instruction_items);
        for (const auto &i : lineage.items)
          canonical_item(input, i->view());
      } else
        for (size_t i = request.instruction_items; i < request.items.size();
             ++i)
          canonical_item(input, request.items[i]);
      input.end_array();
      input.key("output");
      input.array();
      for (const auto &i : items)
        if (i.kind != AILO_ITEM_COMPACTION)
          item(input, i, true, false);
      input.end_array();
      input.end_object();
      input.finish();
      wire_json metadata(record->section(AILO_RECORD_METADATA),
                         self.openai->record_limit);
      attributes(metadata);
      metadata.finish();
      wire_json stored(record->section(AILO_RECORD_OUTPUT),
                       self.openai->record_limit);
      response(stored, status);
      stored.finish();
      record->commit();
      info.stored = 1;
      record.reset();
    }
    flush_done();
    if (request.streaming)
      frame(!std::strcmp(status, "incomplete") ? "response.incomplete"
                                               : "response.completed",
            [&](wire_json &out) {
              out.key("response");
              response(out, status);
            });
    else {
      wire_json out(sink, self.openai->record_limit);
      response(out, status);
      out.finish();
    }
  }
  void receive(const ailo_event &e) {
    if (e.kind == AILO_EVENT_RUN_BEGIN) {
      start();
    } else if (e.kind == AILO_EVENT_ITEM_BEGIN) {
      charge(sizeof(response_item) + std::strlen(e.item_id) +
             (e.call_id ? std::strlen(e.call_id) : 0));
      current = items.size();
      response_item i{};
      i.kind = e.item_kind;
      i.id = e.item_id;
      i.call_id = e.call_id ? e.call_id : "";
      items.push_back(std::move(i));
      if (e.item_kind != AILO_ITEM_FUNCTION_CALL)
        added(current);
    } else if (e.kind == AILO_EVENT_CONTENT_BEGIN) {
      frame("response.content_part.added", [&](wire_json &out) {
        location(out, current);
        out.key("part");
        part(out, items.at(current), false);
      });
    } else if (e.kind == AILO_EVENT_TOKEN)
      sampled(*e.token);
    else if (e.kind == AILO_EVENT_TEXT_DELTA ||
             e.kind == AILO_EVENT_REASONING_DELTA ||
             e.kind == AILO_EVENT_FUNCTION_ARGUMENTS_DELTA) {
      auto &i = items.at(current);
      if (e.kind == AILO_EVENT_FUNCTION_ARGUMENTS_DELTA)
        added(current);
      append(i.text, e.data, e.size);
      if (e.kind == AILO_EVENT_TEXT_DELTA)
        visible(e);
      frame(e.kind == AILO_EVENT_TEXT_DELTA ? "response.output_text.delta"
            : e.kind == AILO_EVENT_REASONING_DELTA
                ? "response.reasoning_text.delta"
                : "response.function_call_arguments.delta",
            [&](wire_json &out) {
              location(out, current,
                       e.kind != AILO_EVENT_FUNCTION_ARGUMENTS_DELTA);
              out.key("delta");
              out.string(e.data, e.size);
              if (e.kind == AILO_EVENT_TEXT_DELTA) {
                out.key("logprobs");
                out.array();
                out.end_array();
              }
            });
    } else if (e.kind == AILO_EVENT_FUNCTION_NAME_DELTA)
      append(items.at(current).name, e.data, e.size);
    else if (e.kind == AILO_EVENT_ITEM_END) {
      auto &i = items.at(current);
      added(current);
      i.done = true;
      i.incomplete = e.finish_reason == AILO_FINISH_LENGTH;
      flush_done();
    } else if (e.kind == AILO_EVENT_COMPACTION) {
      if (active_control == SIZE_MAX || items[active_control].window)
        throw failure(AILO_INTERNAL_ERROR, "Unannounced Responses compaction");
      auto window = std::make_shared<transcript>();
      for (size_t i = 0; i < e.compacted_item_count; ++i)
        window->items.push_back(
            std::make_shared<owned_item>(e.compacted_items[i], window->bytes,
                                         4 * 1024 * 1024, window->parts));
      charge(window->bytes);
      items[active_control].window = std::move(window);
    } else if (e.kind == AILO_EVENT_OUTPUT_PROGRESS)
      settle(detail::to_native(e.raw_offset));
    else if (e.kind == AILO_EVENT_RUN_END)
      completed();
  }

public:
  void start() {
    if (begun)
      return;
    begun = true;
    for (auto type : {"response.created", "response.in_progress"})
      frame(type, [&](wire_json &out) {
        out.key("response");
        response(out, "in_progress");
      });
  }
  void compact_begin() {
    start();
    ailo_api_info identity{};
    api_identity(identity, "cmp_");
    charge(sizeof(response_item) + std::strlen(identity.id));
    response_item control{};
    control.kind = AILO_ITEM_COMPACTION;
    control.id = identity.id;
    active_control = items.size();
    items.push_back(std::move(control));
    last_progress = std::chrono::steady_clock::now();
    added(active_control);
  }
  void compact_progress() {
    const auto now = std::chrono::steady_clock::now();
    if (now - last_progress < std::chrono::seconds(30))
      return;
    last_progress = now;
    frame("response.compaction.compacting",
          [&](wire_json &out) { location(out, active_control, false); });
  }
  void seal_controls() {
    if (active_control == SIZE_MAX)
      return;
    std::vector<ailo_content> parts;
    std::vector<ailo_item> prefix;
    parts.reserve(items.size());
    prefix.reserve(items.size());
    for (auto &i : items) {
      if (i.kind == AILO_ITEM_COMPACTION) {
        if (!i.window)
          throw failure(AILO_INTERNAL_ERROR, "Unfinished Responses compaction");
        const auto sealed = api_automatic_window(
            self, i.window->views(), request.instruction_items, prefix, i.id);
        append(i.encrypted, sealed.encrypted_content.data(),
               sealed.encrypted_content.size());
        i.done = true;
        continue;
      }
      ailo_item value{};
      value.kind = i.kind;
      value.role = AILO_ROLE_ASSISTANT;
      value.id = i.id.c_str();
      if (i.kind == AILO_ITEM_FUNCTION_CALL) {
        value.call_id = i.call_id.c_str();
        value.name = i.name.c_str();
        value.arguments = i.text.c_str();
      } else {
        parts.push_back({AILO_CONTENT_TEXT, i.text.data(), i.text.size()});
        value.content = &parts.back();
        value.content_count = 1;
      }
      prefix.push_back(value);
    }
  }
  response_writer(state &s, openai_request &r, ailo_api_info &i,
                  ailo_output_sink callback, void *user, uint64_t now,
                  const openai_retention_plan &plan)
      : self(s), request(r), info(i), sink(api_sink(s, callback, user, i)),
        created(now), store(plan.store_response) {
    if (store) {
      ailo_record_metadata metadata{
          self.openai->policy.namespace_id.c_str(),
          info.id,
          AILO_RECORD_RESPONSE,
          detail::from_unsigned(now),
          detail::from_unsigned(plan.response_expires_at),
          self.openai->record_limit};
      record = std::make_unique<record_transaction>(*self.openai, metadata);
    }
  }
  static int event(void *user, const ailo_event *e) noexcept {
    auto &self = *static_cast<response_writer *>(user);
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
  void failed(ailo_status status) noexcept {
    if (!info.started || frame_open || status == AILO_CANCELLED ||
        status == AILO_SINK_FAILED)
      return;
    completed_at = 0;
    try {
      frame("response.failed", [&](wire_json &out) {
        out.key("response");
        response(out, "failed", status);
      });
    } catch (...) {
    }
  }
};
} // namespace
void api_canonical_item(wire_json &out, const ailo_item &item) {
  canonical_item(out, item);
}
void api_prior_input(state &self, openai_request &request) {
  prior_input(self, request);
}
} // namespace client
} // namespace ailo
AILO_PUBLIC ailo_status ailo_client_responses(ailo_client *client,
                                              const void *json, size_t size,
                                              ailo_output_sink sink, void *user,
                                              ailo_api_info *info,
                                              ailo_error *error) {
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
    openai_request request(openai_protocol::responses, json, size,
                           call.self.id);
    const auto now = api_now();
    const auto plan = resolve_retention(request, call.self.openai->policy, now);
    if (!request.conversation_id.empty())
      throw failure(AILO_UNSUPPORTED,
                    "Conversation repository is not yet connected");
    info->content_type =
        request.streaming ? "text/event-stream" : "application/json";
    api_identity(*info, "resp_");
    response_writer output(call.self, request, *info, sink, user, now, plan);
    log_scope logs(call.self.logging.get(), log_phase::generate);
    try {
      prior_input(call.self, request);
      bool compacted = false;
      for (const auto &i : request.items)
        compacted |= i.kind == AILO_ITEM_COMPACTION;
      if ((compacted || request.automatic_compaction) && plan.store_response &&
          (call.self.compactor->ephemeral ||
           (call.self.compactor->ttl_seconds &&
            call.self.compactor->ttl_seconds <
                call.self.openai->policy.response_retention_seconds)))
        throw failure(AILO_INVALID_ARGUMENT,
                      "Stored compacted Responses require stable keys and a "
                      "compaction TTL covering response retention");
      api_expand_compaction(call.self, request);
      auto native = request.generation;
      if (native.logprobs)
        native.top_logprobs = 20;
      auto automatic = ailo_auto_compaction_default_policy();
      compaction_task task{};
      std::unique_ptr<ailo_continuation, decltype(&ailo_continuation_destroy)>
          window(nullptr, ailo_continuation_destroy);
      std::unique_ptr<continuation_transaction> history;
      if (request.automatic_compaction) {
        automatic.compaction = call.self.openai->compaction;
        automatic.trigger_tokens = request.compact_threshold;
        if (!automatic.compaction.target_tokens && automatic.trigger_tokens)
          automatic.compaction.target_tokens = automatic.trigger_tokens * 3 / 4;
        call.self.ensure_loaded();
        identify_cache(call.self, true);
        if (!request.compact_threshold) {
          const auto resolved =
              resolve_compaction_policy(call.self, automatic.compaction);
          request.compact_threshold = llama_n_ctx(call.self.context.get()) -
                                      resolved.output_reserve_tokens;
        }
        window.reset(new_window(call.self, ailo_continuation_default_config(),
                                native.items, native.item_count));
        native.items = nullptr;
        native.item_count = 0;
        native.continuation = window.get();
        native.auto_compaction = &automatic;
        history = std::make_unique<continuation_transaction>(call.self, native);
        task.instructions = request.instructions.empty()
                                ? nullptr
                                : request.instructions.c_str();
        task.begin = [&] { output.compact_begin(); };
        task.progress = [&] { output.compact_progress(); };
      }
      try {
        generate(call.self, history ? history->input : native,
                 response_writer::event, &output, info->run, history.get(),
                 &task);
      } catch (...) {
        output.rethrow();
        throw;
      }
      output.rethrow();
    } catch (const failure &e) {
      output.failed(e.status);
      throw;
    } catch (const std::bad_alloc &) {
      output.failed(AILO_NO_MEMORY);
      throw;
    } catch (...) {
      output.failed(AILO_INTERNAL_ERROR);
      throw;
    }
  });
  if (!info->started)
    info->http_status = api_http_status(status);
  return status;
}
AILO_PUBLIC ailo_status ailo_client_response_retrieve(
    ailo_client *client, const char *id, ailo_output_sink sink, void *user,
    ailo_api_info *info, ailo_error *error) {
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
    info->content_type = "application/json";
    read_record(*call.self.openai, AILO_RECORD_RESPONSE, id, AILO_RECORD_OUTPUT,
                api_sink(call.self, sink, user, *info));
    info->stored = 1;
  });
  if (!info->started)
    info->http_status = api_http_status(status);
  return status;
}
AILO_PUBLIC ailo_status ailo_client_response_delete(
    ailo_client *client, const char *id, ailo_output_sink sink, void *user,
    ailo_api_info *info, ailo_error *error) {
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
    erase_record(*call.self.openai, AILO_RECORD_RESPONSE, id);
    info->content_type = "application/json";
    wire_json out(api_sink(call.self, sink, user, *info));
    out.object();
    out.key("id");
    out.string(id);
    out.key("object");
    out.string("response.deleted");
    out.key("deleted");
    out.boolean(true);
    out.end_object();
    out.finish();
  });
  if (!info->started)
    info->http_status = api_http_status(status);
  return status;
}
