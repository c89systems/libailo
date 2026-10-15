#include "ailo/continuation.h"
#include "ailo_compaction_internal.hpp"
#include <cstdio>
#include <cstring>
#include <memory>
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
using key_owner =
    std::unique_ptr<ailo_compactor, decltype(&ailo_compactor_destroy)>;
ailo_error error{};
const char *chat_template =
    "{% for m in messages %}{{ m.role }}:{{ m.content }}|{% endfor %}";
const char *summary =
    R"({"facts":[{"source":1,"kind":"message","role":"user","id":"old-goal","call_id":"","name":"","category":"goal","text":"Finish ticket T-1."}]})";
client_owner client(const ailo_client_config &config) {
  ailo_client *out = nullptr;
  CHECK(ailo_client_new(&config, &out, &error) == AILO_OK);
  return {out, ailo_client_destroy};
}
history_owner history(ailo_client *client) {
  ailo_continuation *out = nullptr;
  CHECK(ailo_continuation_new(client, nullptr, &out, &error) == AILO_OK);
  return {out, ailo_continuation_destroy};
}
key_owner keys(const ailo_compactor_config &config) {
  ailo_compactor *out = nullptr;
  CHECK(ailo_compactor_new(&config, &out, &error) == AILO_OK);
  return {out, ailo_compactor_destroy};
}
ailo_continuation_info info(ailo_continuation *value) {
  ailo_continuation_info out{};
  CHECK(ailo_continuation_get_info(value, &out, &error) == AILO_OK);
  return out;
}
struct output {
  unsigned events = 0, deltas = 0;
  int result = 1;
};
int events(void *user, const ailo_event *event) {
  auto &out = *static_cast<output *>(user);
  ++out.events;
  if (event->kind == AILO_EVENT_TEXT_DELTA) {
    if (!out.deltas)
      CHECK(event->usage.completion_tokens.lo == 1);
    ++out.deltas;
    return out.result;
  }
  return 1;
}
std::string replace(std::string text, const std::string &before,
                    const std::string &after) {
  auto at = text.find(before);
  CHECK(at != std::string::npos);
  return text.replace(at, before.size(), after);
}
int main(int argc, char **argv) {
  try {
    CHECK(argc == 3);
    std::array<unsigned char, 32> key{};
    key.fill('k');
    auto kc = ailo_compactor_default_config();
    kc.namespace_id = "replay-test";
    kc.current_key = {"current", key.data(), key.size()};
    auto owner = keys(kc);
    compaction_state producer(kc);
    ailo_client_config config{};
    config.source.local_path = argv[1];
    config.backend = AILO_BACKEND_CPU;
    config.context_tokens = 1024;
    config.threads = 1;
    config.chat_template = chat_template;
    config.compactor = owner.get();
    auto first = client(config);
    auto root = history(first.get());
    const std::string identity = info(root.get()).identity;
    ailo_content instruction{AILO_CONTENT_TEXT, "Keep original instructions.",
                             27},
        recent{AILO_CONTENT_TEXT, "continue", 8};
    ailo_item items[4]{};
    items[0].role = AILO_ROLE_SYSTEM;
    items[0].id = "instruction";
    items[0].pinned = 1;
    items[0].content = &instruction;
    items[0].content_count = 1;
    items[1].kind = AILO_ITEM_COMPACTION;
    items[2].id = "recent";
    items[2].content = &recent;
    items[2].content_count = 1;
    compaction_payload payload;
    payload.summary = summary;
    payload.cut = 1;
    payload.source_items = 5;
    payload.removed_items = 3;
    payload.retained = {fingerprint(items[0]), fingerprint(items[2])};
    auto sealed = producer.seal(payload, identity);
    items[1].id = sealed.id.c_str();
    items[1].encrypted_content = sealed.encrypted_content.c_str();
    auto request = ailo_request_default();
    request.items = items;
    request.item_count = 3;
    request.max_tokens = 3;
    request.temperature = 0;
    ailo_logit_bias bias{100, 100};
    request.logit_bias = &bias;
    request.logit_bias_count = 1;
    auto view =
        prepare_replay(request, retain_compactor(owner.get()), identity);
    CHECK(view.messages.size() == 3 && view.messages[0].role == "system" &&
          view.messages[0].content == "Keep original instructions." &&
          view.messages[1].role == "assistant" &&
          view.messages[1].content.find("Finish ticket T-1.") !=
              std::string::npos &&
          view.messages[1].content == view.compaction_text);
    auto facts = parse_summary(payload.summary, payload.source_items);
    CHECK(facts.size() == 1 && facts[0].source == 1 &&
          facts[0].role == "user" && facts[0].id == "old-goal" &&
          facts[0].category == "goal");
    ailo_run_info run{};
    output out;
    CHECK(first->generate(first.get(), &request, events, &out, &run, &error) ==
              AILO_OK &&
          out.deltas == 3);
    // Routine Lua tests use the very same C ABI replay state, generated with
    // real model/template identity, not a parallel crypto implementation.
    FILE *fixture = std::fopen(argv[2], "w");
    CHECK(fixture);
    CHECK(std::fprintf(
              fixture,
              "return "
              "{namespace_id='replay-test',key_id='current',key=string.rep('k',"
              "32),id='%s',encrypted_content='%s',template=[=[%s]=]}\n",
              sealed.id.c_str(), sealed.encrypted_content.c_str(),
              chat_template) > 0);
    CHECK(std::fclose(fixture) == 0);
    CHECK(root->append(root.get(), items, 3, &error) == AILO_OK);
    ailo_continuation *copy = nullptr;
    CHECK(root->clone(root.get(), &copy, &error) == AILO_OK);
    history_owner fork(copy, ailo_continuation_destroy);
    // Canonical ownership copies the token; visit never exposes plaintext.
    std::string visited;
    CHECK(root->visit(
              root.get(),
              [](void *user, const ailo_item *item) {
                if (item->kind == AILO_ITEM_COMPACTION) {
                  CHECK(item->encrypted_content && item->id && !item->content &&
                        !item->content_count);
                  *static_cast<std::string *>(user) = item->encrypted_content;
                }
                return 1;
              },
              &visited, &error) == AILO_OK &&
          visited == sealed.encrypted_content);
    request.continuation = root.get();
    request.items = nullptr;
    request.item_count = 0;
    for (int stop : {0, -1}) {
      out = {};
      out.result = stop;
      CHECK(
          first->generate(first.get(), &request, events, &out, &run, &error) ==
          (stop ? AILO_SINK_FAILED : AILO_CANCELLED));
      CHECK(out.deltas == 1 && run.usage.completion_tokens.lo == 1 &&
            info(root.get()).item_count == 3);
    }
    // No adapter history-owner key can grant a receiving client decryption.
    auto foreign_config = config;
    foreign_config.compactor = nullptr;
    auto foreign = client(foreign_config);
    out = {};
    CHECK(foreign->generate(foreign.get(), &request, events, &out, &run,
                            &error) == AILO_INVALID_ARGUMENT &&
          !out.events);
    CHECK(info(root.get()).revision.lo == 1);
    // Explicit compatible keys survive shell/client destruction and forks.
    auto compatible = client(config);
    owner.reset();
    first.reset();
    out = {};
    CHECK(compatible->generate(compatible.get(), &request, events, &out, &run,
                               &error) == AILO_OK &&
          out.deltas == 3);
    items[3].content = &recent;
    items[3].content_count = 1;
    CHECK(root->append(root.get(), items + 3, 1, &error) == AILO_OK);
    CHECK(info(root.get()).item_count == 4 && info(fork.get()).item_count == 3);
    request.continuation = nullptr;
    request.items = items;
    request.item_count = 3;
    auto reject = [&](ailo_status status) {
      out = {};
      CHECK(compatible->generate(compatible.get(), &request, events, &out, &run,
                                 &error) == status);
      CHECK(!out.events && !out.deltas && info(fork.get()).item_count == 3);
    };
    items[0].pinned = 0;
    reject(AILO_INVALID_ARGUMENT);
    items[0].pinned = 1;
    items[0].role = AILO_ROLE_DEVELOPER;
    reject(AILO_INVALID_ARGUMENT);
    items[0].role = AILO_ROLE_SYSTEM;
    instruction.data = "Changed original instruction";
    reject(AILO_INVALID_ARGUMENT);
    instruction.data = "Keep original instructions.";
    items[2].id = "substitute";
    reject(AILO_INVALID_ARGUMENT);
    items[2].id = "recent";
    items[1].role = AILO_ROLE_SYSTEM;
    reject(AILO_INVALID_ARGUMENT);
    items[1].role = AILO_ROLE_USER;
    items[1].content = &recent;
    reject(AILO_INVALID_ARGUMENT);
    items[1].content = nullptr;
    items[1].encrypted_content = "openai-foreign-token";
    reject(AILO_INVALID_ARGUMENT);
    items[1].encrypted_content = sealed.encrypted_content.c_str();
    items[1].id = "renamed";
    reject(AILO_INVALID_ARGUMENT);
    items[1].id = sealed.id.c_str();
    request.item_count = 2;
    reject(AILO_INVALID_ARGUMENT);
    request.item_count = 3;
    std::swap(items[0], items[1]);
    reject(AILO_INVALID_ARGUMENT);
    std::swap(items[0], items[1]);
    items[3] = items[1];
    request.item_count = 4;
    reject(AILO_INVALID_ARGUMENT);
    request.item_count = 3;
    items[2].encrypted_content = "hidden";
    reject(AILO_INVALID_ARGUMENT);
    items[2].encrypted_content = nullptr;
    // A valid signature alone does not authorize malformed/trust-escalating
    // structured data. This also exercises parser failure before any events.
    for (const auto &invalid : std::vector<std::string>{
             "{}", "null", "{", R"({"facts":[]})", R"({"facts":["goal"]})",
             replace(summary, "\"user\"", "\"system\""),
             replace(summary, "\"user\"", "\"developer\""),
             replace(summary, "\"source\":1", "\"source\":5"),
             replace(summary, "\"source\":1", "\"source\":1.0"),
             replace(summary, "\"source\":1", "\"source\":true"),
             replace(summary, "\"source\":1", "\"source\":1,\"source\":1"),
             replace(summary, "\"text\":", "\"unknown\":"),
             replace(summary, "Finish ticket T-1.", ""),
             replace(summary, "Finish ticket T-1.", "\\u0000"),
             replace(summary, "Finish ticket T-1.", std::string(4097, 'a')),
             replace(summary, "\"message\"", "\"function_result\""),
             replace(summary, "\"goal\"", "\"instructions\"")}) {
      payload.summary = invalid;
      auto bad = producer.seal(payload, identity);
      items[1].id = bad.id.c_str();
      items[1].encrypted_content = bad.encrypted_content.c_str();
      reject(AILO_INVALID_ARGUMENT);
      auto empty = history(compatible.get());
      CHECK(empty->append(empty.get(), items, 3, &error) ==
                AILO_INVALID_ARGUMENT &&
            info(empty.get()).revision.lo == 0);
    }
    items[1].id = sealed.id.c_str();
    items[1].encrypted_content = sealed.encrypted_content.c_str();
    payload.summary = summary;
    // Actual rendering, not merely template presence, must retain the summary.
    owner = keys(kc);
    auto discard_config = config;
    discard_config.compactor = owner.get();
    discard_config.chat_template =
        "{% for m in messages %}{% if m.role == 'user' %}{{ m.content }}{% "
        "endif %}{% endfor %}";
    auto discard = client(discard_config);
    auto discard_history = history(discard.get());
    const auto lost =
        producer.seal(payload, info(discard_history.get()).identity);
    items[1].id = lost.id.c_str();
    items[1].encrypted_content = lost.encrypted_content.c_str();
    out = {};
    CHECK(discard->generate(discard.get(), &request, events, &out, &run,
                            &error) == AILO_UNSUPPORTED &&
          !out.events);
    std::puts("Authenticated opaque replay, lower-trust provenance, canonical "
              "ownership, tenant isolation and immediate streaming passed");
  } catch (const std::exception &e) {
    std::fprintf(stderr, "compaction replay: %s (%s)\n", e.what(),
                 error.message);
    return 1;
  }
}
