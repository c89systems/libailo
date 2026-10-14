#include "ailo/client.h"
#include "ailo_client_internal.hpp"
#include <chrono>
#include <condition_variable>
#include <cstdio>
#include <cstring>
#include <limits>
#include <stdexcept>
#include <thread>

#define CHECK(value)                                                           \
  do {                                                                         \
    if (!(value))                                                              \
      throw std::runtime_error(#value);                                        \
  } while (0)

namespace {
struct client_deleter {
  void operator()(ailo_client *client) const { ailo_client_destroy(client); }
};
struct barrier {
  std::mutex mutex;
  std::condition_variable changed;
  bool entered = false, released = false;
};
int wait_for_cancel(void *data, const ailo_event *event) {
  if (event->kind != AILO_EVENT_TEXT_DELTA)
    return 1;
  auto &sync = *static_cast<barrier *>(data);
  std::unique_lock<std::mutex> lock(sync.mutex);
  sync.entered = true;
  sync.changed.notify_one();
  if (!sync.changed.wait_for(lock, std::chrono::seconds(10),
                             [&]() { return sync.released; }))
    return -1;
  return 1;
}
int throwing_sink(void *, const ailo_event *event) {
  if (event->kind == AILO_EVENT_TEXT_DELTA)
    throw std::runtime_error("private callback text");
  return 1;
}
int accept(void *data, const ailo_event *event) {
  if (event->kind == AILO_EVENT_TEXT_DELTA)
    ++*static_cast<unsigned *>(data);
  return 1;
}
int throwing_progress(void *, unsigned, unsigned, ailo_u64, ailo_u64) {
  throw std::runtime_error("private acquisition text");
}

void run(const char *path) {
  ailo_error error{};
  auto config = ailo_client_default_config();
  config.source.local_path = path;
  config.backend = AILO_BACKEND_CPU;
  config.context_tokens = 512;
  ailo_client *raw = nullptr;
  CHECK(ailo_client_new(&config, &raw, &error) == AILO_OK);
  std::unique_ptr<ailo_client, client_deleter> client(raw);
  ailo_content text{AILO_CONTENT_TEXT, "hello", 5};
  ailo_item item{};
  item.kind = AILO_ITEM_MESSAGE;
  item.content = &text;
  item.content_count = 1;
  auto request = ailo_request_default();
  request.items = &item;
  request.item_count = 1;
  request.temperature = 0;
  request.max_tokens = 8;

  barrier sync;
  ailo_run_info info{};
  ailo_status result = AILO_INTERNAL_ERROR;
  std::thread worker([&]() {
    result = client->generate(client.get(), &request, wait_for_cancel, &sync,
                              &info, &error);
  });
  bool entered;
  {
    std::unique_lock<std::mutex> lock(sync.mutex);
    entered = sync.changed.wait_for(lock, std::chrono::seconds(10),
                                    [&]() { return sync.entered; });
  }
  ailo_error concurrent_error{};
  char *json = reinterpret_cast<char *>(1);
  const auto busy = client->devices(client.get(), &json, &concurrent_error);
  client->cancel(client.get());
  {
    std::lock_guard<std::mutex> lock(sync.mutex);
    sync.released = true;
  }
  sync.changed.notify_one();
  worker.join();
  CHECK(entered && busy == AILO_BUSY && !json);
  CHECK(result == AILO_CANCELLED && info.usage.completion_tokens.lo == 1);
  CHECK(info.completed_candidates == 0);

  CHECK(client->generate(client.get(), &request, throwing_sink, nullptr, &info,
                         &error) == AILO_INTERNAL_ERROR);
  CHECK(!std::strstr(error.message, "private callback"));
  unsigned deltas = 0;
  CHECK(client->generate(client.get(), &request, accept, &deltas, &info,
                         &error) == AILO_OK);
  CHECK(deltas == 8 && info.completed_candidates == 1);
  request.temperature = std::numeric_limits<double>::quiet_NaN();
  deltas = 0;
  CHECK(client->generate(client.get(), &request, accept, &deltas, &info,
                         &error) == AILO_INVALID_ARGUMENT);
  CHECK(!deltas);
  request.temperature = 0;

  // Prove stored Jinja semantics, including a branch the legacy format
  // recognizer cannot express. This renders the request, not generated text.
  auto &state = *static_cast<ailo::client::state *>(client->state);
  const char *jinja =
      "{% for m in messages %}{{ m.role | upper }}:{{ m.content }};"
      "{% endfor %}{% if add_generation_prompt %}GO{% endif %}";
  std::vector<llama_chat_message> messages{{"user", "hello"}};
  CHECK(ailo::client::template_supported(jinja));
  CHECK(ailo::client::apply_template(jinja,
                                     llama_model_get_vocab(state.model.get()),
                                     messages) == "USER:hello;GO");
  CHECK(!ailo::client::template_supported("{{ broken"));
  config.chat_template = jinja;
  CHECK(ailo_client_new(&config, &raw, &error) == AILO_OK);
  std::unique_ptr<ailo_client, client_deleter> jinja_client(raw);
  CHECK(jinja_client->generate(jinja_client.get(), &request, accept, &deltas,
                               &info, &error) == AILO_OK);
  config.chat_template = "{{ raise_exception('private template text') }}";
  CHECK(ailo_client_new(&config, &raw, &error) == AILO_OK);
  std::unique_ptr<ailo_client, client_deleter> bad_template(raw);
  CHECK(bad_template->generate(bad_template.get(), &request, accept, &deltas,
                               &info, &error) == AILO_UNSUPPORTED);
  CHECK(!std::strstr(error.message, "private template"));

  config.source.progress = throwing_progress;
  config.chat_template = nullptr;
  CHECK(ailo_client_new(&config, &raw, &error) == AILO_INTERNAL_ERROR);
  CHECK(!raw && !std::strstr(error.message, "private acquisition"));

  config.source.progress = nullptr;
  const std::string eos_path = std::string(path) + ".eos";
  config.source.local_path = eos_path.c_str();
  CHECK(ailo_client_new(&config, &raw, &error) == AILO_OK);
  std::unique_ptr<ailo_client, client_deleter> eos_client(raw);
  deltas = 0;
  request.candidates = 2;
  CHECK(eos_client->generate(eos_client.get(), &request, accept, &deltas, &info,
                             &error) == AILO_OK);
  CHECK(!deltas && info.completed_candidates == 2);
  CHECK(info.finish_reason == AILO_FINISH_STOP);
  CHECK(info.usage.completion_tokens.lo == 2);
}
} // namespace

int main(int argc, char **argv) {
  if (argc != 2)
    return 2;
  try {
    run(argv[1]);
    std::puts("Cross-thread cancellation, busy state, callback recovery and "
              "stored Jinja semantics: passed");
    return 0;
  } catch (const std::exception &error) {
    std::fprintf(stderr, "%s\n", error.what());
    return 1;
  }
}
