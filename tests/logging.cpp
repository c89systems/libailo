#include "ailo/client.h"
#include "ailo_logging_internal.hpp"
#include "ggml-cpu.h"
#include "llama.h"
#include <atomic>
#include <chrono>
#include <condition_variable>
#include <cstdio>
#include <fstream>
#include <iterator>
#include <lonejson.h>
#include <mutex>
#include <sstream>
#include <stdexcept>
#include <thread>

extern "C" void ggml_log_internal(ggml_log_level, const char *, ...);

#define CHECK(value)                                                           \
  do {                                                                         \
    if (!(value))                                                              \
      throw std::runtime_error(#value);                                        \
  } while (0)

namespace {
struct file_deleter {
  void operator()(FILE *value) const {
    if (value)
      std::fclose(value);
  }
};
struct client_deleter {
  void operator()(ailo_client *value) const { ailo_client_destroy(value); }
};
struct capture {
  std::string path;
  std::unique_ptr<FILE, file_deleter> file;
  std::unique_ptr<pslog_logger, ailo::client::logger_deleter> parent;
  explicit capture(const std::string &name)
      : path(name), file(std::fopen(path.c_str(), "w+")) {
    CHECK(file);
    pslog_config config;
    pslog_default_config(&config);
    config.mode = PSLOG_MODE_JSON;
    config.color = PSLOG_COLOR_NEVER;
    config.min_level = PSLOG_LEVEL_TRACE;
    config.output = pslog_output_from_fp(file.get(), 0);
    parent.reset(pslog_new(&config));
    CHECK(parent);
  }
  std::string text() {
    CHECK(!std::fflush(file.get()));
    std::ifstream input(path);
    return {std::istreambuf_iterator<char>(input),
            std::istreambuf_iterator<char>()};
  }
};

void fallback(ggml_log_level, const char *, void *data) {
  ++*static_cast<std::atomic<unsigned> *>(data);
}
struct synchronization {
  std::mutex mutex;
  std::condition_variable changed;
  unsigned arrived = 0;
  std::atomic<bool> failed{false};
};
struct worker_data {
  synchronization *sync;
  const char *label;
};
void worker_log(ggml_tensor *output, const ggml_tensor *, int index,
                int threads, void *data) {
  const auto &input = *static_cast<worker_data *>(data);
  if (index == 0) {
    std::unique_lock<std::mutex> lock(input.sync->mutex);
    ++input.sync->arrived;
    input.sync->changed.notify_all();
    if (!input.sync->changed.wait_for(lock, std::chrono::seconds(5), [&]() {
          return input.sync->arrived == 2;
        }))
      input.sync->failed.store(true);
  }
  ggml_log_internal(GGML_LOG_LEVEL_INFO, "%s worker %d/%d\n", input.label,
                    index, threads);
  auto *values = static_cast<float *>(output->data);
  for (int i = index; i < 32; i += threads)
    values[i] = 1;
}

void graph(ailo::client::logging_state &logger, worker_data &data) {
  ailo::client::log_scope scope(&logger, ailo::client::log_phase::generate);
  std::unique_ptr<ggml_context, decltype(&ggml_free)> context(
      ggml_init({1024 * 1024, nullptr, false}), ggml_free);
  CHECK(context);
  auto *input = ggml_new_tensor_1d(context.get(), GGML_TYPE_F32, 32);
  auto *output = ggml_map_custom1(context.get(), input, worker_log,
                                  GGML_N_TASKS_MAX, &data);
  auto *graph = ggml_new_graph(context.get());
  ggml_build_forward_expand(graph, output);
  CHECK(ggml_graph_compute_with_ctx(context.get(), graph, 2) ==
        GGML_STATUS_SUCCESS);
}

int event(void *data, const ailo_event *value) {
  if (value->kind == AILO_EVENT_TEXT_DELTA)
    ++*static_cast<unsigned *>(data);
  return 1;
}

void valid_records(const std::string &text) {
  std::unique_ptr<lonejson, decltype(&lonejson_free)> json(
      lonejson_new(nullptr, nullptr), lonejson_free);
  CHECK(json);
  std::istringstream input(text);
  std::string line;
  unsigned records = 0;
  while (std::getline(input, line)) {
    CHECK(lonejson_validate_cstr(json.get(), line.c_str(), nullptr) ==
          LONEJSON_STATUS_OK);
    ++records;
  }
  CHECK(records > 0);
}

void run(const char *model, const std::string &directory) {
  capture a(directory + "/A.log"), b(directory + "/B.log");
  std::atomic<unsigned> global{0};
  ggml_log_callback saved;
  void *saved_user = nullptr;
  llama_log_get(&saved, &saved_user);
  llama_log_set(fallback, &global);
  {
    ailo::client::logging_state logger_a(a.parent.get(), "graph-A");
    ailo::client::logging_state logger_b(b.parent.get(), "graph-B");
    {
      ailo::client::log_scope outer(&logger_a,
                                    ailo::client::log_phase::generate);
      const ggml_log_level levels[] = {
          GGML_LOG_LEVEL_TRACE, GGML_LOG_LEVEL_DEBUG, GGML_LOG_LEVEL_INFO,
          GGML_LOG_LEVEL_WARN, GGML_LOG_LEVEL_ERROR};
      for (auto level : levels)
        ggml_log_internal(level, "outer-A\n");
      {
        ailo::client::log_scope inner(&logger_b,
                                      ailo::client::log_phase::metadata);
        ggml_log_internal(GGML_LOG_LEVEL_INFO, "inner-B\n");
      }
      ggml_log_internal(GGML_LOG_LEVEL_CONT, "outer-A-continuation\n");
      {
        ailo::client::log_scope none(nullptr, ailo::client::log_phase::devices);
        ggml_log_internal(GGML_LOG_LEVEL_INFO, "global-only\n");
      }
      ggml_log_internal(GGML_LOG_LEVEL_INFO, "outer-A-restored\n");
    }
    CHECK(global == 1);
    synchronization sync;
    worker_data data_a{&sync, "graph-A"}, data_b{&sync, "graph-B"};
    std::thread first([&]() { graph(logger_a, data_a); });
    std::thread second([&]() { graph(logger_b, data_b); });
    first.join();
    second.join();
    CHECK(!sync.failed.load() && sync.arrived == 2);
  }

  auto config = ailo_client_default_config();
  config.source.local_path = model;
  config.backend = AILO_BACKEND_CPU;
  config.model_id = "receiver-A";
  ailo_error error{};
  ailo_client *raw = nullptr;
  CHECK(ailo_client_new_with_logger(&config, a.parent.get(), &raw, &error) ==
        AILO_OK);
  std::unique_ptr<ailo_client, client_deleter> client(raw);
  a.parent.reset();
  ailo_content content{AILO_CONTENT_TEXT, "hello", 5};
  ailo_item item{};
  item.content = &content;
  item.content_count = 1;
  auto request = ailo_request_default();
  request.items = &item;
  request.item_count = 1;
  request.max_tokens = 3;
  request.temperature = 0;
  ailo_run_info info{};
  unsigned deltas = 0;
  CHECK(client->generate(client.get(), &request, event, &deltas, &info,
                         &error) == AILO_OK &&
        deltas == 3);
  client.reset();
  ggml_log_callback actual;
  void *actual_user = nullptr;
  llama_log_get(&actual, &actual_user);
  CHECK(actual == fallback && actual_user == &global);
  llama_log_set(saved, saved_user);

  const auto text_a = a.text(), text_b = b.text();
  valid_records(text_a);
  valid_records(text_b);
  for (const char *level : {"trace", "debug", "info", "warn", "error"})
    CHECK(text_a.find(std::string("\"lvl\":\"") + level + "\"") !=
          std::string::npos);
  CHECK(text_a.find("inner-B") == std::string::npos &&
        text_a.find("graph-B") == std::string::npos);
  CHECK(text_b.find("outer-A") == std::string::npos &&
        text_b.find("receiver-A") == std::string::npos);
  CHECK(text_a.find("graph-A worker 1/2") != std::string::npos);
  CHECK(text_b.find("graph-B worker 1/2") != std::string::npos);
  CHECK(text_a.find("outer-A-restored") != std::string::npos);
  CHECK(text_a.find("\"continuation\":true") != std::string::npos);
  CHECK(text_a.find("Generation complete") != std::string::npos);
  CHECK(text_a.find("Destroying inference receiver") != std::string::npos);
}
} // namespace

int main(int argc, char **argv) {
  if (argc != 3)
    return 2;
  try {
    run(argv[1], argv[2]);
    std::puts("Per-receiver pslog levels, concurrent worker routing, global "
              "fallback and logger ownership: passed");
    return 0;
  } catch (const std::exception &error) {
    std::fprintf(stderr, "%s\n", error.what());
    return 1;
  }
}
