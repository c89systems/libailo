#include "ailo/continuation.h"
#include "ailo_compaction_internal.hpp"
#include <cstdio>
#include <cstring>
#include <memory>
#include <pslog.h>
#include <stdexcept>

using namespace ailo::client;
#define CHECK(c)                                                               \
  do {                                                                         \
    if (!(c))                                                                  \
      throw std::runtime_error(std::string(#c) + " at " +                      \
                               std::to_string(__LINE__));                      \
  } while (0)
using client_owner =
    std::unique_ptr<ailo_client, decltype(&ailo_client_destroy)>;
using history_owner =
    std::unique_ptr<ailo_continuation, decltype(&ailo_continuation_destroy)>;
ailo_error error{};
client_owner client(ailo_client_config config, pslog_logger *logger = nullptr) {
  ailo_client *out = nullptr;
  CHECK(ailo_client_new_with_logger(&config, logger, &out, &error) == AILO_OK);
  return {out, ailo_client_destroy};
}
history_owner history(ailo_client *client) {
  ailo_continuation *out = nullptr;
  CHECK(ailo_continuation_new(client, nullptr, &out, &error) == AILO_OK);
  return {out, ailo_continuation_destroy};
}
ailo_continuation_info info(ailo_continuation *history) {
  ailo_continuation_info out{};
  CHECK(history->info(history, &out, &error) == AILO_OK);
  return out;
}
struct log_output {
  ailo_client *client = nullptr;
  bool armed = false, cancelled = false;
  std::string tail;
  static int write(void *user, const char *data, size_t size, size_t *written) {
    auto &self = *static_cast<log_output *>(user);
    try {
      self.tail.append(data, size);
      if (self.armed &&
          self.tail.find("Generating bounded compaction summary") !=
              std::string::npos) {
        self.client->cancel(self.client);
        self.cancelled = true;
        self.armed = false;
      }
      if (self.tail.size() > 256)
        self.tail.erase(0, self.tail.size() - 256);
      *written = size;
      return 0;
    } catch (...) {
      *written = 0;
      return -1;
    }
  }
};
struct captured {
  unsigned count = 0, compactions = 0;
  std::string opaque, token_id, instruction, arguments;
};
int visit(void *user, const ailo_item *item) {
  auto &out = *static_cast<captured *>(user);
  ++out.count;
  if (item->kind == AILO_ITEM_COMPACTION) {
    ++out.compactions;
    out.opaque = item->encrypted_content;
    out.token_id = item->id;
  }
  if (item->role == AILO_ROLE_SYSTEM && item->content_count) {
    CHECK(item->id && std::string(item->id) == "system" && item->pinned);
    out.instruction.assign(static_cast<const char *>(item->content[0].data),
                           item->content[0].size);
  }
  if (item->arguments)
    out.arguments += item->arguments;
  return 1;
}
int delta(void *user, const ailo_event *event) {
  auto &count = *static_cast<unsigned *>(user);
  if (event->kind == AILO_EVENT_TEXT_DELTA) {
    if (!count)
      CHECK(event->usage.completion_tokens.lo == 1);
    ++count;
  }
  return 1;
}
int main(int argc, char **argv) {
  try {
    CHECK(argc == 2);
    std::string path = std::string(argv[1]) + "-compaction";
    ailo_client_config config{};
    config.source.local_path = path.c_str();
    config.backend = AILO_BACKEND_CPU;
    config.threads = 1;
    config.context_tokens = 4096;
    config.chat_template =
        "{% for m in messages %}{{m.role}}:{{m.content}}{% if m.tool_calls is "
        "defined %}{% for c in m.tool_calls "
        "%}{{c.id}}={{c.function.name}}({{c.function.arguments | tojson}});{% "
        "endfor %}{% endif %}|{% endfor %}";
    log_output logging;
    pslog_config lc;
    pslog_default_config(&lc);
    lc.min_level = PSLOG_LEVEL_TRACE;
    lc.color = PSLOG_COLOR_NEVER;
    lc.output = {log_output::write, nullptr, nullptr, &logging, 0};
    std::unique_ptr<pslog_logger, void (*)(pslog_logger *)> logger(
        pslog_new(&lc), [](pslog_logger *p) {
          if (p)
            p->destroy(p);
        });
    CHECK(logger);
    auto receiver = client(config, logger.get());
    logging.client = receiver.get();
    auto source = history(receiver.get());
    std::string old = "Finish ticket T-1. ";
    for (int i = 0; i < 70; ++i)
      old += "Unneeded repeated detail. ";
    std::string answer(500, 'a');
    const std::string instructions =
        "Original instructions: preserve these bytes.";
    std::string text[] = {instructions,    old,
                          answer,          "recent question",
                          "recent answer", "newest question",
                          "newest answer"};
    ailo_content parts[7]{};
    ailo_item items[7]{};
    for (size_t i = 0; i < 7; ++i) {
      parts[i] = {AILO_CONTENT_TEXT, text[i].data(), text[i].size(), nullptr};
      items[i].content = &parts[i];
      items[i].content_count = 1;
      items[i].role = i % 2 ? AILO_ROLE_USER : AILO_ROLE_ASSISTANT;
    }
    items[0].role = AILO_ROLE_SYSTEM;
    items[0].id = "system";
    items[0].pinned = 1;
    items[1].id = "old-goal";
    CHECK(source->append(source.get(), items, 7, &error) == AILO_OK);
    auto request = ailo_request_default();
    request.continuation = source.get();
    request.max_tokens = 3;
    request.temperature = 0;
    ailo_logit_bias bias{100, 100};
    request.logit_bias = &bias;
    request.logit_bias_count = 1;
    unsigned deltas = 0;
    ailo_run_info run{};
    CHECK(receiver->generate(receiver.get(), &request, delta, &deltas, &run,
                             &error) == AILO_OK &&
          deltas == 3);
    auto policy = ailo_compaction_default_policy();
    policy.target_tokens = 1000;
    policy.summary_context_tokens = 8192;
    ailo_continuation *raw = nullptr;
    ailo_compaction_info compacted{};
    CHECK(receiver->compact(receiver.get(), &request, &policy, &raw, &compacted,
                            &error) == AILO_OK);
    history_owner window(raw, ailo_continuation_destroy);
    CHECK(compacted.changed &&
          compacted.before_tokens == run.usage.prompt_tokens.lo &&
          compacted.after_tokens < 1000 && compacted.removed_items == 2 &&
          compacted.retained_items == 5 && compacted.summary_passes == 1 &&
          compacted.summary_prompt_tokens.lo &&
          compacted.summary_completion_tokens.lo == 2);
    CHECK(info(source.get()).item_count == 7 &&
          info(source.get()).revision.lo == 1);
    captured observed;
    CHECK(window->visit(window.get(), visit, &observed, &error) == AILO_OK &&
          observed.count == 6 && observed.compactions == 1 &&
          observed.instruction == instructions);
    const auto &self = *static_cast<state *>(receiver->state);
    const auto payload =
        self.compactor->open(observed.token_id.c_str(), observed.opaque.c_str(),
                             self.conversation_identity);
    const auto facts = parse_summary(payload->summary, 7);
    CHECK(facts.size() == 1 && facts[0].source == 1 &&
          facts[0].role == "user" && facts[0].id == "old-goal" &&
          facts[0].text == "Finish ticket T-1.");
    // Summary work must not evict/mutate the original prompt checkpoint.
    deltas = 0;
    CHECK(receiver->generate(receiver.get(), &request, delta, &deltas, &run,
                             &error) == AILO_OK &&
          run.usage.cached_tokens.lo > 0);
    auto replay = request;
    replay.continuation = window.get();
    deltas = 0;
    CHECK(receiver->generate(receiver.get(), &replay, delta, &deltas, &run,
                             &error) == AILO_OK &&
          run.usage.prompt_tokens.lo == compacted.after_tokens && deltas == 3);
    // Already-small windows are independent owned no-op forks.
    raw = nullptr;
    ailo_compaction_info noop{};
    CHECK(receiver->compact(receiver.get(), &replay, &policy, &raw, &noop,
                            &error) == AILO_OK &&
          !noop.changed && !noop.summary_passes);
    history_owner clone(raw, ailo_continuation_destroy);
    CHECK(info(clone.get()).item_count == 6);
    // A prior summary is eligible even without additional old raw groups.
    // This fixture repeats the same fact, so a tighter target must detect
    // actual lack of reduction rather than silently accepting another token.
    auto repeated_policy = policy;
    repeated_policy.target_tokens = compacted.after_tokens - 1;
    ailo_compaction_info repeated{};
    CHECK(receiver->compact(receiver.get(), &replay, &repeated_policy, &raw,
                            &repeated, &error) == AILO_MODEL_FAILED &&
          !raw && !repeated.changed);
    CHECK(info(window.get()).item_count == 6);
    auto reject = [&](ailo_status status, const ailo_request &input,
                      const ailo_compaction_policy &limits) {
      raw = reinterpret_cast<ailo_continuation *>(1);
      ailo_compaction_info failed{};
      failed.changed = 1;
      CHECK(receiver->compact(receiver.get(), &input, &limits, &raw, &failed,
                              &error) == status &&
            !raw && !failed.changed && !failed.before_tokens);
      CHECK(info(source.get()).item_count == 7 &&
            info(source.get()).revision.lo == 1);
    };
    auto bad = policy;
    bad.summary_tokens = 0;
    reject(AILO_INVALID_ARGUMENT, request, bad);
    bad = policy;
    bad.target_tokens = 4096;
    reject(AILO_INVALID_ARGUMENT, request, bad);
    bad = policy;
    bad.summary_context_tokens = 128;
    bad.summary_tokens = 16;
    reject(AILO_CONTEXT_LIMIT, request, bad);
    bad = policy;
    bad.summary_tokens = 1;
    reject(AILO_MODEL_FAILED, request, bad);
    bad = policy;
    bad.target_tokens = 5;
    reject(AILO_CONTEXT_LIMIT, request, bad);
    auto direct = request;
    direct.continuation = nullptr;
    direct.items = items;
    direct.item_count = 7;
    items[2].pinned = 1;
    reject(AILO_CONTEXT_LIMIT, direct, policy);
    bad = policy;
    bad.recent_turns = 0;
    bad.target_tokens = compacted.before_tokens - 20;
    reject(AILO_MODEL_FAILED, direct, bad); // model cites protected source 1
    items[2].pinned = 0;
    std::string long_id(256, 'x');
    items[1].id = long_id.c_str();
    bad = policy;
    bad.max_summary_bytes = 256;
    reject(AILO_OUT_OF_RANGE, direct, bad);
    items[1].id = "old-goal";
    logging.tail.clear();
    logging.armed = true;
    reject(AILO_CANCELLED, request, policy);
    CHECK(logging.cancelled);
    // A partial parallel-call group remains intact and unresolved.
    ailo_item calls[3]{};
    for (size_t i = 0; i < 2; ++i) {
      calls[i].kind = AILO_ITEM_FUNCTION_CALL;
      calls[i].name = "lookup";
      calls[i].arguments = "{ \"x\": 1 }";
    }
    calls[0].call_id = "one";
    calls[1].call_id = "two";
    calls[2].kind = AILO_ITEM_FUNCTION_RESULT;
    calls[2].call_id = "two";
    calls[2].content = &parts[3];
    calls[2].content_count = 1;
    CHECK(source->append(source.get(), calls, 3, &error) == AILO_OK);
    // The unfinished call turn is protected in addition to the two recent
    // COMPLETE turns. It must not consume their retention allowance.
    CHECK(receiver->compact(receiver.get(), &request, &policy, &raw, &compacted,
                            &error) == AILO_CONTEXT_LIMIT &&
          !raw);
    auto pending_policy = policy;
    pending_policy.recent_turns = 1;
    CHECK(receiver->compact(receiver.get(), &request, &pending_policy, &raw,
                            &compacted, &error) == AILO_OK);
    history_owner pending(raw, ailo_continuation_destroy);
    CHECK(info(pending.get()).pending_calls == 1 &&
          info(source.get()).pending_calls == 1);
    captured pending_items;
    CHECK(pending->visit(pending.get(), visit, &pending_items, &error) ==
              AILO_OK &&
          pending_items.arguments == "{ \"x\": 1 }{ \"x\": 1 }");
    // Replacement compaction supersedes the old token only in its new fork.
    auto next = replay;
    next.items = items + 3;
    next.item_count = 4;
    bad = policy;
    bad.recent_turns = 0;
    bad.target_tokens = 420;
    const auto status = receiver->compact(receiver.get(), &next, &bad, &raw,
                                          &compacted, &error);
    CHECK(status == AILO_OK && compacted.changed);
    if (status == AILO_OK) {
      history_owner replacement(raw, ailo_continuation_destroy);
      captured changed;
      CHECK(replacement->visit(replacement.get(), visit, &changed, &error) ==
                AILO_OK &&
            changed.compactions == 1 && changed.token_id != observed.token_id);
      const auto replaced =
          self.compactor->open(changed.token_id.c_str(), changed.opaque.c_str(),
                               self.conversation_identity);
      const auto inherited =
          parse_summary(replaced->summary, replaced->source_items);
      CHECK(inherited.size() == 1 && inherited[0].role == "user" &&
            inherited[0].kind == "message" && inherited[0].id == "old-goal" &&
            inherited[0].text == "Finish ticket T-1.");
    }
    deltas = 0;
    CHECK(receiver->generate(receiver.get(), &replay, delta, &deltas, &run,
                             &error) == AILO_OK);
    std::puts("Model-driven semantic compaction: protected history, exact "
              "tokens, authenticated forks, work/byte/context failures and "
              "cancellation passed");
  } catch (const std::exception &e) {
    std::fprintf(stderr, "compaction: %s (%s)\n", e.what(), error.message);
    return 1;
  }
}
