#include "ailo/continuation.h"
#include <cstdio>
#include <cstring>
#include <memory>
#include <stdexcept>
#include <string>
#include <vector>

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
client_owner client(ailo_client_config config) {
  ailo_client *value = nullptr;
  CHECK(ailo_client_new(&config, &value, &error) == AILO_OK);
  return {value, ailo_client_destroy};
}
history_owner history(ailo_client *receiver,
                      const ailo_continuation_config *config = nullptr) {
  ailo_continuation *value = nullptr;
  CHECK(ailo_continuation_new(receiver, config, &value, &error) == AILO_OK);
  return {value, ailo_continuation_destroy};
}
ailo_continuation_info info(ailo_continuation *value) {
  ailo_continuation_info result{};
  CHECK(value->info(value, &result, &error) == AILO_OK);
  return result;
}
struct seen {
  ailo_continuation *owner = nullptr;
  unsigned items = 0;
  std::string text{}, arguments{};
  bool pinned = false;
};
int visit(void *user, const ailo_item *item) {
  auto &out = *static_cast<seen *>(user);
  ++out.items;
  out.pinned = out.pinned || item->pinned;
  if (item->arguments)
    out.arguments += item->arguments;
  for (size_t i = 0; i < item->content_count; ++i)
    out.text.append(static_cast<const char *>(item->content[i].data),
                    item->content[i].size);
  ailo_continuation_info output{};
  CHECK(out.owner->info(out.owner, &output, &error) == AILO_BUSY &&
        output.item_count == 0);
  CHECK(out.owner->append(out.owner, nullptr, 0, &error) == AILO_BUSY);
  ailo_continuation *clone = reinterpret_cast<ailo_continuation *>(1);
  CHECK(out.owner->clone(out.owner, &clone, &error) == AILO_BUSY && !clone);
  return 1;
}
int halt(void *user, const ailo_item *) { return *static_cast<int *>(user); }
struct generation {
  ailo_continuation *owner;
  ailo_client *other;
  unsigned events = 0, deltas = 0;
  int reject = 1;
};
int events(void *user, const ailo_event *event) {
  auto &out = *static_cast<generation *>(user);
  ++out.events;
  if (event->kind == AILO_EVENT_TEXT_DELTA)
    ++out.deltas;
  CHECK(out.owner->append(out.owner, nullptr, 0, &error) == AILO_BUSY);
  if (out.other && event->kind == AILO_EVENT_RUN_BEGIN) {
    auto request = ailo_request_default();
    request.continuation = out.owner;
    ailo_run_info run{};
    CHECK(out.other->generate(out.other, &request, events, user, &run,
                              &error) == AILO_BUSY);
    CHECK(run.id[0] == '\0');
  }
  return event->kind == AILO_EVENT_RUN_END ? out.reject : 1;
}
int main(int argc, char **argv) {
  try {
    CHECK(argc == 2);
    ailo_client_config config{};
    config.source.local_path = argv[1];
    config.backend = AILO_BACKEND_CPU;
    config.context_tokens = 512;
    config.threads = 1;
    config.disable_prompt_cache = 1;
    config.chat_template =
        "{% for m in messages %}{{ m.role }}:{{ m.content }}{% if m.tool_calls "
        "is defined %}{% for c in m.tool_calls %}{{ c.id }}={{ c.function.name "
        "}}({{ c.function.arguments | tojson }});{% endfor %}{% endif %}{% if "
        "m.tool_call_id is defined %}#{{ m.tool_call_id }}{% endif %}|{% "
        "endfor %}";
    auto first = client(config), other = client(config);
    auto root = history(first.get());
    CHECK(info(root.get()).item_count == 0 &&
          std::strlen(info(root.get()).identity) == 64);
    char input[] = "keep";
    ailo_content parts[] = {{AILO_CONTENT_TEXT, input, 4},
                            {AILO_CONTENT_TEXT, " me", 3}};
    ailo_item instruction{};
    instruction.id = "system-one";
    instruction.role = AILO_ROLE_SYSTEM;
    instruction.content = parts;
    instruction.content_count = 2;
    instruction.pinned = 1;
    CHECK(root->append(root.get(), &instruction, 1, &error) == AILO_OK);
    input[0] = 'X';
    seen observed{root.get()};
    CHECK(root->visit(root.get(), visit, &observed, &error) == AILO_OK);
    CHECK(observed.text == "keep me" && observed.pinned && observed.items == 1);
    for (int result : {0, -1})
      CHECK(root->visit(root.get(), halt, &result, &error) ==
            (result ? AILO_SINK_FAILED : AILO_CANCELLED));
    ailo_continuation *copy = nullptr;
    CHECK(root->clone(root.get(), &copy, &error) == AILO_OK);
    history_owner fork(copy, ailo_continuation_destroy);
    ailo_content question{AILO_CONTENT_TEXT, "hello", 5};
    ailo_item user{};
    user.id = "user-one";
    user.content = &question;
    user.content_count = 1;
    auto request = ailo_request_default();
    request.continuation = root.get();
    request.items = &user;
    request.item_count = 1;
    request.max_tokens = 3;
    request.temperature = 0;
    ailo_logit_bias bias{100, 100};
    request.logit_bias = &bias;
    request.logit_bias_count = 1;
    ailo_run_info run{};
    generation generated{root.get(), other.get()};
    for (int reject : {0, -1}) {
      generated.reject = reject;
      CHECK(first->generate(first.get(), &request, events, &generated, &run,
                            &error) ==
            (reject ? AILO_SINK_FAILED : AILO_CANCELLED));
      CHECK(info(root.get()).item_count == 1 &&
            info(root.get()).revision.lo == 1);
    }
    generated.reject = 1;
    CHECK(first->generate(first.get(), &request, events, &generated, &run,
                          &error) == AILO_OK);
    CHECK(generated.deltas && info(root.get()).item_count == 2 &&
          info(root.get()).revision.lo == 2);
    CHECK(info(fork.get()).item_count == 1);
    // Output is streamed but never silently appended. Empty-input generation
    // replays canonical input without advancing its revision.
    request.items = nullptr;
    request.item_count = 0;
    const auto previous = info(root.get());
    CHECK(first->generate(first.get(), &request, events, &generated, &run,
                          &error) == AILO_OK);
    CHECK(info(root.get()).item_count == previous.item_count &&
          info(root.get()).revision.lo == previous.revision.lo);
    CHECK(root->append(root.get(), &user, 1, &error) == AILO_INVALID_ARGUMENT);
    CHECK(info(root.get()).revision.lo == 2);
    // Fork independently; support a pending final parallel tool group and
    // partial results while rejecting interrupted groups and orphan results.
    ailo_item calls[2]{};
    for (auto &call : calls) {
      call.kind = AILO_ITEM_FUNCTION_CALL;
      call.name = "lookup";
      call.arguments = "{ \"x\" : 1 }";
    }
    calls[0].id = "fc-1";
    calls[0].call_id = "call-1";
    calls[1].id = "fc-2";
    calls[1].call_id = "call-2";
    CHECK(fork->append(fork.get(), calls, 2, &error) == AILO_OK);
    CHECK(info(fork.get()).pending_calls == 2 &&
          info(fork.get()).item_count == 3);
    CHECK(fork->append(fork.get(), &user, 1, &error) == AILO_INVALID_ARGUMENT);
    request.continuation = fork.get();
    generated.owner = fork.get();
    generated.events = 0;
    CHECK(first->generate(first.get(), &request, events, &generated, &run,
                          &error) == AILO_INVALID_ARGUMENT &&
          !generated.events);
    ailo_item result{};
    result.kind = AILO_ITEM_FUNCTION_RESULT;
    result.call_id = "orphan";
    result.content = &question;
    result.content_count = 1;
    CHECK(fork->append(fork.get(), &result, 1, &error) ==
          AILO_INVALID_ARGUMENT);
    result.call_id = "call-2";
    CHECK(fork->append(fork.get(), &result, 1, &error) == AILO_OK);
    CHECK(info(fork.get()).pending_calls == 1);
    CHECK(fork->append(fork.get(), &result, 1, &error) ==
          AILO_INVALID_ARGUMENT);
    result.call_id = "call-1";
    CHECK(fork->append(fork.get(), &result, 1, &error) == AILO_OK);
    CHECK(!info(fork.get()).pending_calls);
    CHECK(first->generate(first.get(), &request, events, &generated, &run,
                          &error) == AILO_OK);
    seen saved{fork.get()};
    CHECK(fork->visit(fork.get(), visit, &saved, &error) == AILO_OK);
    CHECK(saved.arguments == "{ \"x\" : 1 }{ \"x\" : 1 }");
    auto incompatible_config = config;
    incompatible_config.chat_template =
        "{% for m in messages %}{{ m.content }}{% endfor %}";
    auto incompatible = client(incompatible_config);
    generated.events = 0;
    CHECK(incompatible->generate(incompatible.get(), &request, events,
                                 &generated, &run,
                                 &error) == AILO_INVALID_ARGUMENT &&
          !generated.events);
    CHECK(incompatible->cache_clear(incompatible.get(), &error) == AILO_OK &&
          info(fork.get()).item_count == 5);
    auto resized_config = config;
    resized_config.context_tokens = 256;
    auto resized = client(resized_config);
    CHECK(resized->generate(resized.get(), &request, events, &generated, &run,
                            &error) == AILO_OK);
    // Explicit bounds reject atomically; no special storage or cache owner.
    auto limits = ailo_continuation_default_config();
    limits.max_items = 1;
    auto small = history(first.get(), &limits);
    CHECK(small->append(small.get(), &user, 1, &error) == AILO_OK);
    CHECK(small->append(small.get(), &user, 1, &error) == AILO_OUT_OF_RANGE &&
          info(small.get()).item_count == 1);
    limits.max_bytes = 1;
    auto tiny = history(first.get(), &limits);
    CHECK(tiny->append(tiny.get(), &user, 1, &error) == AILO_OUT_OF_RANGE &&
          info(tiny.get()).revision.lo == 0);
    limits.max_items = 0;
    copy = reinterpret_cast<ailo_continuation *>(1);
    CHECK(ailo_continuation_new(first.get(), &limits, &copy, &error) ==
              AILO_INVALID_ARGUMENT &&
          !copy);
    auto before = info(root.get());
    ailo_content unsupported{AILO_CONTENT_IMAGE, "x", 1};
    user.id = "media";
    user.content = &unsupported;
    CHECK(root->append(root.get(), &user, 1, &error) == AILO_UNSUPPORTED &&
          info(root.get()).revision.lo == before.revision.lo);
    user.content = &question;
    first.reset();
    other.reset();
    auto replacement = client(config);
    request.continuation = root.get();
    generated.owner = root.get();
    generated.other = nullptr;
    CHECK(replacement->generate(replacement.get(), &request, events, &generated,
                                &run, &error) == AILO_OK);
    root.reset();
    CHECK(info(fork.get()).item_count == 5);
    ailo_continuation_destroy(nullptr);
    std::puts("Owned continuation copies/forks, pending tools, transactional "
              "streaming, identity and bounds passed");
  } catch (const std::exception &e) {
    std::fprintf(stderr, "continuation: %s (%s)\n", e.what(), error.message);
    return 1;
  }
}
