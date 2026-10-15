#include "ailo_conversation_internal.hpp"
#include <cstdio>
#include <cstring>
#include <set>

using namespace ailo::client;
#define CHECK(condition)                                                       \
  do {                                                                         \
    if (!(condition))                                                          \
      throw std::runtime_error(#condition);                                    \
  } while (0)
const char *rich_template =
    "{% for m in messages %}{{ m.role }}:"
    "{% if m.name is defined %}{{ m.name }}:{% endif %}{{ m.content }}"
    "{% if m.tool_calls is defined %}{% for c in m.tool_calls %}"
    "{{ c.id }}={{ c.function.name }}({{ c.function.arguments | tojson }});"
    "{% endfor %}{% endif %}"
    "{% if m.tool_call_id is defined %}#{{ m.tool_call_id }}{% endif %}|"
    "{% endfor %}";

struct observation {
  unsigned events = 0, items = 0, contents = 0, usage = 0, deltas = 0;
  bool item = false, content = false;
  std::string run, current;
  std::set<std::string> item_ids;
};
int observe(void *user, const ailo_event *event) {
  auto &seen = *static_cast<observation *>(user);
  CHECK(event->sequence.lo == seen.events++ && !event->sequence.hi);
  CHECK(event->run_id && std::strlen(event->run_id) == 36);
  if (seen.run.empty())
    seen.run = event->run_id;
  CHECK(seen.run == event->run_id);
  if (event->kind == AILO_EVENT_ITEM_BEGIN) {
    CHECK(!seen.item && event->item_id &&
          event->item_kind == AILO_ITEM_MESSAGE);
    CHECK(seen.item_ids.insert(event->item_id).second);
    seen.current = event->item_id;
    seen.item = true;
    ++seen.items;
  }
  CHECK(bool(event->item_id) == seen.item);
  if (event->item_id)
    CHECK(seen.current == event->item_id);
  if (event->kind == AILO_EVENT_CONTENT_BEGIN) {
    CHECK(!seen.content);
    seen.content = true;
    ++seen.contents;
  }
  if (event->kind == AILO_EVENT_TEXT_DELTA) {
    CHECK(seen.item && seen.content);
    ++seen.deltas;
  }
  if (event->kind == AILO_EVENT_CONTENT_END) {
    CHECK(seen.content);
    seen.content = false;
  }
  if (event->kind == AILO_EVENT_ITEM_END) {
    CHECK(!seen.content);
    seen.item = false;
  }
  if (event->kind == AILO_EVENT_USAGE) {
    CHECK(!seen.item && !seen.content);
    ++seen.usage;
  }
  return 1;
}

int main(int argc, char **argv) {
  try {
    CHECK(argc == 2);
    ailo_content question{AILO_CONTENT_TEXT, "question", 8};
    ailo_content checking{AILO_CONTENT_TEXT, "checking", 8};
    ailo_content first{AILO_CONTENT_TEXT, "first", 5};
    ailo_content second{AILO_CONTENT_TEXT, "second", 6};
    ailo_item items[7]{};
    items[0].name = "Ada";
    items[0].content = &question;
    items[0].content_count = 1;
    items[1].role = AILO_ROLE_ASSISTANT;
    items[1].content = &checking;
    items[1].content_count = 1;
    for (unsigned i : {2u, 3u}) {
      items[i].kind = AILO_ITEM_FUNCTION_CALL;
    }
    items[2].id = "item-one";
    items[2].call_id = "call-one";
    items[2].name = "lookup";
    items[2].arguments = "{ \"x\" : 1 }";
    items[3].id = "item-two";
    items[3].call_id = "call-two";
    items[3].name = "count";
    items[3].arguments = "{}";
    items[4].kind = items[5].kind = AILO_ITEM_FUNCTION_RESULT;
    items[4].call_id = "call-two";
    items[4].content = &second;
    items[4].content_count = 1;
    items[5].call_id = "call-one";
    items[5].name = "lookup";
    items[5].content = &first;
    items[5].content_count = 1;
    items[6].content = &question;
    items[6].content_count = 1;
    auto request = ailo_request_default();
    request.items = items;
    request.item_count = 7;
    request.max_tokens = 3;
    request.temperature = 0;
    request.candidates = 2;
    const auto input = prepare_conversation(request);
    CHECK(input.messages.size() == 5 && input.max_parallel_calls == 2);
    CHECK(input.messages[1].calls[0].id == "call-one");
    CHECK(input.messages[1].calls[0].arguments == items[2].arguments);
    ailo_client_config config{};
    config.source.local_path = argv[1];
    config.backend = AILO_BACKEND_CPU;
    config.context_tokens = 512;
    config.chat_template = rich_template;
    state self(config, nullptr);
    self.ensure_loaded();
    const auto rendered = apply_template(
        rich_template, llama_model_get_vocab(self.model.get()), input);
    CHECK(rendered ==
          "user:Ada:question|assistant:checkingcall-one=lookup({\"x\": "
          "1});call-two=count({});|tool:count:second#call-two|tool:lookup:"
          "first#call-one|user:question|");
    const char *implicit =
        "{% for m in messages %}{{ m.role }}:{% if m.name is defined %}{{ "
        "m.name }}:{% endif %}{{ m.content }}"
        "{% if m.tool_calls is defined %}{% for c in m.tool_calls %}{{ "
        "c.function.name }}:{{ c.function.arguments | tojson }}{% endfor %}{% "
        "endif %}|{% endfor %}";
    const auto prompt = apply_template(
        implicit, llama_model_get_vocab(self.model.get()), input);
    CHECK(prompt.find("tool:lookup:first") < prompt.find("tool:count:second"));
    CHECK(input.messages[2].call_id == "call-two");
    ailo_client *raw = nullptr;
    ailo_error error{};
    CHECK(ailo_client_new(&config, &raw, &error) == AILO_OK);
    std::unique_ptr<ailo_client, decltype(&ailo_client_destroy)> client(
        raw, ailo_client_destroy);
    ailo_run_info info{};
    observation seen;
    CHECK(client->generate(client.get(), &request, observe, &seen, &info,
                           &error) == AILO_OK);
    CHECK(seen.run == info.id && seen.items == 2 && seen.contents == 2 &&
          seen.usage == 1 && seen.deltas == 6);
    const auto previous = seen.run;
    for (int mode = 0; mode < 8; ++mode) {
      auto bad = request;
      ailo_item invalid[7];
      std::memcpy(invalid, items, sizeof(items));
      bad.items = invalid;
      switch (mode) {
      case 0:
        invalid[3].call_id = "call-one";
        break;
      case 1:
        invalid[4].call_id = "orphan";
        break;
      case 2:
        invalid[5].call_id = "call-two";
        break;
      case 3:
        invalid[5].name = "wrong";
        break;
      case 4:
        invalid[3].id = "item-one";
        break;
      case 5:
        invalid[2].arguments = "[]";
        break;
      case 6:
        bad.item_count = 4;
        break;
      case 7:
        invalid[2].name = "bad name";
        break;
      }
      seen = {};
      CHECK(client->generate(client.get(), &bad, observe, &seen, &info,
                             &error) == AILO_INVALID_ARGUMENT);
      CHECK(!seen.events && !info.id[0]);
    }
    seen = {};
    CHECK(client->generate(client.get(), &request, observe, &seen, &info,
                           &error) == AILO_OK);
    CHECK(previous != seen.run);
    try {
      (void)apply_template("chatml", llama_model_get_vocab(self.model.get()),
                           input);
      CHECK(false);
    } catch (const failure &e) {
      CHECK(e.status == AILO_UNSUPPORTED);
    }
    const char *ignores = "{% for m in messages %}{{ m.content }}{% endfor %}";
    try {
      (void)apply_template(ignores, llama_model_get_vocab(self.model.get()),
                           input);
      CHECK(false);
    } catch (const failure &e) {
      CHECK(e.status == AILO_UNSUPPORTED);
    }
    std::puts("Function histories, provenance validation and typed item "
              "identities passed");
  } catch (const std::exception &e) {
    std::fprintf(stderr, "conversation: %s\n", e.what());
    return 1;
  }
}
