#include "ailo_reasoning_internal.hpp"
#include "ailo_text_stream_internal.hpp"
#include <algorithm>
#include <cstdio>
#include <cstring>
#include <json-schema-to-grammar.h>
#include <set>

namespace ailo {
namespace client {
namespace {
void validate(const reasoning_format &format) {
  if (format.open.empty() || format.close.empty() || format.open.size() > 128 ||
      format.close.size() > 128 || format.open == format.close)
    throw failure(AILO_UNSUPPORTED, "Reasoning delimiters must be distinct, "
                                    "nonempty and at most 128 bytes");
  for (const auto *text : {&format.open, &format.close})
    for (unsigned char ch : *text)
      if (!ch || ch > 127)
        throw failure(AILO_UNSUPPORTED,
                      "Reasoning delimiters must be non-NUL ASCII");
}
std::string rule(size_t n) { return "ailo-reasoning-" + std::to_string(n); }
bool white(const std::string &value) {
  return std::all_of(value.begin(), value.end(), [](char c) {
    return c == ' ' || c == '\n' || c == '\t' || c == '\r';
  });
}
bool ends(const std::string &value, const std::string &suffix) {
  return value.size() >= suffix.size() &&
         !value.compare(value.size() - suffix.size(), suffix.size(), suffix);
}
reasoning_format normalize(reasoning_format format) {
  validate(format);
  const auto first = format.close.find_first_not_of(" \t\r\n");
  if (first == std::string::npos)
    throw failure(AILO_UNSUPPORTED,
                  "Reasoning close delimiter cannot be only whitespace");
  // A newline before the tag is reasoning payload, not a precondition for
  // recognizing the tag. Once the tag appears, commit to its suffix/answer.
  format.close.erase(0, first);
  validate(format);
  return format;
}
} // namespace
reasoning_plan qualify_reasoning(const char *source, const llama_vocab *vocab,
                                 const conversation &input,
                                 const std::string &tools,
                                 const ailo_request &request) {
  if (request.reasoning < AILO_REASONING_AUTO ||
      request.reasoning > AILO_REASONING_ENABLED ||
      request.max_reasoning_tokens > static_cast<size_t>(INT32_MAX))
    throw failure(AILO_INVALID_ARGUMENT,
                  "Invalid reasoning mode or token budget");
  if (request.reasoning_effort) {
    const size_t n = strnlen(request.reasoning_effort, 17);
    const std::set<std::string> levels{"none", "minimal", "low", "medium",
                                       "high", "xhigh",   "max"};
    if (n > 16 || !levels.count(std::string(request.reasoning_effort, n)))
      throw failure(AILO_INVALID_ARGUMENT, "Unknown reasoning effort");
    if (request.reasoning == AILO_REASONING_DISABLED)
      throw failure(AILO_INVALID_ARGUMENT,
                    "Disabled reasoning cannot specify effort");
  }
  if (request.reasoning == AILO_REASONING_DISABLED &&
      request.max_reasoning_tokens)
    throw failure(AILO_INVALID_ARGUMENT,
                  "Disabled reasoning cannot specify a reasoning budget");
  reasoning_plan plan;
  template_options options{true, request.reasoning_effort};
  conversation probe;
  probe.rich = true;
  probe.messages.push_back({"user", "AILO_PROBE_USER", {}, {}, {}});
  const std::string r = "AILO_REASONING_PROBE_7b4c",
                    a = "AILO_ANSWER_PROBE_90dc";
  bool qualified = false;
  bool advertised = false;
  try {
    const auto prefix =
        apply_template(source, vocab, probe, tools, true, &options);
    for (const char *tag : {"<think>", "[THINK]"}) {
      const auto pos = prefix.rfind(tag);
      if (pos != std::string::npos &&
          white(prefix.substr(pos + std::strlen(tag))))
        advertised = true;
    }
    chat_message assistant{"assistant", a, {}, {}, {}};
    assistant.reasoning = r;
    probe.messages.push_back(std::move(assistant));
    const auto full =
        apply_template(source, vocab, probe, tools, false, &options);
    const auto rp = full.find(r), ap = full.find(a);
    advertised =
        advertised || (rp != std::string::npos && ap != std::string::npos);
    if (rp != std::string::npos && ap != std::string::npos &&
        ap > rp + r.size() &&
        full.find(r, rp + r.size()) == std::string::npos &&
        full.find(a, ap + a.size()) == std::string::npos) {
      const std::pair<std::string, std::string> tags[] = {
          {"<think>", "</think>"}, {"[THINK]", "[/THINK]"}};
      const auto before = full.substr(0, rp),
                 gap = full.substr(rp + r.size(), ap - rp - r.size());
      for (const auto &tags_pair : tags) {
        const auto pos = before.rfind(tags_pair.first);
        const auto close_pos = gap.find(tags_pair.second);
        if (pos == std::string::npos ||
            !white(before.substr(pos + tags_pair.first.size())) ||
            close_pos == std::string::npos ||
            !white(gap.substr(0, close_pos)) ||
            !white(gap.substr(close_pos + tags_pair.second.size())))
          continue;
        plan.format.open = before.substr(pos);
        plan.format.close = gap;
        if (before == prefix)
          plan.format.prompt_open = true;
        else if (before != prefix + plan.format.open)
          continue;
        validate(plan.format);
        qualified = true;
        break;
      }
    }
  } catch (const failure &e) {
    if (e.status != AILO_UNSUPPORTED)
      throw;
  }
  if (!qualified) {
    if (request.reasoning == AILO_REASONING_ENABLED ||
        request.reasoning_effort || request.max_reasoning_tokens || advertised)
      throw failure(AILO_UNSUPPORTED,
                    "Chat template has no qualified initial reasoning format");
    options.thinking = false;
    plan.prompt = apply_template(source, vocab, input, tools, true, &options);
    return plan;
  }
  if (request.reasoning == AILO_REASONING_DISABLED) {
    template_options disabled{};
    conversation user;
    user.messages.push_back({"user", "AILO_PROBE_USER", {}, {}, {}});
    const auto on = apply_template(source, vocab, user, tools, true, &options),
               off =
                   apply_template(source, vocab, user, tools, true, &disabled);
    if (on == off || ends(off, plan.format.open))
      throw failure(AILO_UNSUPPORTED,
                    "Chat template cannot disable its reasoning phase");
    plan.prompt = apply_template(source, vocab, input, tools, true, &disabled);
    return plan;
  }
  plan.active = true;
  plan.prompt = apply_template(source, vocab, input, tools, true, &options);
  if (plan.format.prompt_open && !ends(plan.prompt, plan.format.open))
    throw failure(
        AILO_UNSUPPORTED,
        "Reasoning prompt prefix does not match the qualified format");
  return plan;
}
std::string reasoning_grammar(const reasoning_format &value,
                              const std::string &answer) {
  const auto format = normalize(value);
  const auto end = format.close.find_last_not_of(" \t\r\n") + 1;
  const auto closing = format.close.substr(0, end),
             suffix = format.close.substr(end);
  std::string grammar =
      answer.empty() ? "root ::= [\\x00-\\U0010FFFF]*\n" : answer + "\n";
  if (grammar.find("\nailo-reasoning-") != std::string::npos ||
      grammar.find("\nreasoning-root ::=") != std::string::npos)
    throw failure(AILO_UNSUPPORTED,
                  "Answer grammar conflicts with reasoning rule names");
  grammar += "reasoning-root ::= " +
             (format.prompt_open ? std::string{}
                                 : gbnf_format_literal(format.open) + " ") +
             rule(0) + "\n";
  const std::set<char> alphabet(closing.begin(), closing.end());
  for (size_t k = 0; k < closing.size(); ++k) {
    std::string body = "[^";
    for (unsigned char ch : alphabet) {
      char escape[8];
      std::snprintf(escape, sizeof(escape), "\\x%02x", ch);
      body += escape;
    }
    body += "] " + rule(0);
    for (char ch : alphabet) {
      const auto prefix = closing.substr(0, k) + ch;
      size_t next = std::min(closing.size(), prefix.size());
      while (next &&
             prefix.compare(prefix.size() - next, next, closing, 0, next))
        --next;
      body += " | " + gbnf_format_literal(std::string(1, ch)) + " " +
              (next == closing.size() ? gbnf_format_literal(suffix) + " root"
                                      : rule(next));
    }
    grammar += rule(k) + " ::= " + body + "\n";
  }
  if (grammar.size() > 4 * 1024 * 1024)
    throw failure(AILO_OUT_OF_RANGE,
                  "Composed reasoning grammar exceeds 4 MiB");
  return grammar;
}
reasoning_stream::reasoning_stream(
    reasoning_format value,
    std::function<void(reasoning_output_kind, const char *, size_t)> output)
    : format(normalize(std::move(value))), sink(std::move(output)),
      reasoning_text({},
                     [this](const char *data, size_t size) {
                       sink(reasoning_output_kind::reasoning, data, size);
                     }),
      answer_text({},
                  [this](const char *data, size_t size) {
                    sink(reasoning_output_kind::answer, data, size);
                  }),
      current(format.prompt_open ? stage::reasoning : stage::open) {
  validate(format);
}
void reasoning_stream::feed(const char *data, size_t size) {
  if (current == stage::answer) {
    if (size)
      answer_text.push({data, size});
    return;
  }
  size_t offset = 0;
  if (current == stage::open) {
    while (offset < size && opening < format.open.size()) {
      if (data[offset++] != format.open[opening++])
        throw failure(AILO_MODEL_FAILED,
                      "Generated reasoning opening delimiter is invalid");
    }
    if (opening < format.open.size())
      return;
    current = stage::reasoning;
  }
  pending.append(data + offset, size - offset);
  const auto end = pending.find(format.close);
  size_t safe = end == std::string::npos ? pending.size() : end;
  if (end == std::string::npos)
    for (size_t n = std::min(format.close.size() - 1, pending.size()); n; --n)
      if (!pending.compare(pending.size() - n, n, format.close, 0, n)) {
        safe -= n;
        break;
      }
  if (safe)
    reasoning_text.push(pending.substr(0, safe));
  if (end == std::string::npos) {
    pending.erase(0, safe);
    return;
  }
  current = stage::answer;
  reasoning_text.push({}, true);
  sink(reasoning_output_kind::end, nullptr, 0);
  const size_t start = end + format.close.size();
  if (start < pending.size())
    answer_text.push(pending.substr(start));
  pending.clear();
}
void reasoning_stream::push(const char *data, size_t size) {
  if (!data && size)
    throw failure(AILO_INVALID_ARGUMENT, "Reasoning input bytes are missing");
  // Chunking bounds transient pending memory even for large native token
  // pieces.
  while (size) {
    const auto n = std::min(size, size_t(4096));
    feed(data, n);
    data += n;
    size -= n;
  }
}
void reasoning_stream::finish(bool truncated) {
  if (current != stage::answer && !truncated)
    throw failure(AILO_MODEL_FAILED,
                  "Generated reasoning ended before its closing delimiter");
  // A withheld delimiter prefix is protocol syntax, never reasoning or answer.
  pending.clear();
  reasoning_text.push({}, true);
  answer_text.push({}, true);
}
} // namespace client
} // namespace ailo
