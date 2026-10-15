#include "ailo/client.h"
#include "ailo_client_internal.hpp"
#include "ailo_conversation_internal.hpp"
#include <chrono>
#include <condition_variable>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <fcntl.h>
#include <filesystem>
#include <limits>
#include <stdexcept>
#include <thread>
#include <unistd.h>

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

void card_location(const char *path) {
  namespace fs = std::filesystem;
  const fs::path fixture = fs::absolute(path);
  struct cwd_guard {
    int fd = open(".", O_RDONLY | O_DIRECTORY);
    ~cwd_guard() {
      if (fd >= 0) {
        fchdir(fd);
        close(fd);
      }
    }
  } cwd;
  CHECK(cwd.fd >= 0);
  const auto expected =
      "\"bytes\":\"" + std::to_string(fs::file_size(fixture)) + "\"";
  auto config = ailo_client_default_config();
  config.backend = AILO_BACKEND_CPU;
  const auto name = fixture.filename().string();
  config.source.local_path = name.c_str();
  CHECK(chdir(fixture.parent_path().c_str()) == 0);
  ailo_client *raw = nullptr;
  ailo_error error{};
  CHECK(ailo_client_new(&config, &raw, &error) == AILO_OK);
  std::unique_ptr<ailo_client, client_deleter> local(raw);
  CHECK(fchdir(cwd.fd) == 0);
  char *text = nullptr;
  CHECK(local->model_card(local.get(), &text, &error) == AILO_OK);
  std::unique_ptr<char, decltype(&ailo_string_free)> card(text,
                                                          ailo_string_free);
  CHECK(std::strstr(card.get(), expected.c_str()));
  struct environment_guard {
    bool had = std::getenv("XDG_CACHE_HOME") != nullptr;
    std::string value = had ? std::getenv("XDG_CACHE_HOME") : "";
    ~environment_guard() {
      if (had)
        setenv("XDG_CACHE_HOME", value.c_str(), 1);
      else
        unsetenv("XDG_CACHE_HOME");
    }
  } environment;
  const auto cache = fixture.parent_path() / "card-cache";
  CHECK(setenv("XDG_CACHE_HOME", cache.c_str(), 1) == 0);
  config.source = {};
  config.source.repo = "owner/card-location";
  config.source.revision = "fixed";
  config.source.filename = "nested/tiny.gguf";
  text = nullptr;
  CHECK(ailo_model_cache_path(&config.source, &text, &error) == AILO_OK);
  std::unique_ptr<char, decltype(&ailo_string_free)> target(text,
                                                            ailo_string_free);
  fs::create_directories(fs::path(target.get()).parent_path());
  fs::copy_file(fixture, target.get(), fs::copy_options::overwrite_existing);
  CHECK(ailo_client_new(&config, &raw, &error) == AILO_OK);
  std::unique_ptr<ailo_client, client_deleter> cached(raw);
  CHECK(setenv("XDG_CACHE_HOME", "relative-invalid-cache", 1) == 0);
  text = nullptr;
  CHECK(cached->model_card(cached.get(), &text, &error) == AILO_OK);
  card.reset(text);
  CHECK(std::strstr(card.get(), expected.c_str()));
  CHECK(std::strstr(card.get(), "\"filename\":\"nested/tiny.gguf\""));
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
  ailo::client::conversation messages;
  ailo::client::chat_message message;
  message.role = "user";
  message.content = "hello";
  messages.messages.push_back(std::move(message));
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
    card_location(argv[1]);
    run(argv[1]);
    std::puts("Cross-thread cancellation, busy state, callback recovery and "
              "stored Jinja semantics: passed");
    return 0;
  } catch (const std::exception &error) {
    std::fprintf(stderr, "%s\n", error.what());
    return 1;
  }
}
