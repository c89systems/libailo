#include "ailo/client.h"
#include "ailo/openai.h"
#include "ailo_logging_internal.hpp"
#include "ggml-cpu.h"
#include "ggml/src/ggml-library-log.h"
#include "llama.h"
#include <atomic>
#include <chrono>
#include <condition_variable>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <fstream>
#include <iterator>
#include <limits>
#include <lonejson.h>
#include <mutex>
#include <sstream>
#include <stdexcept>
#include <thread>
#include <unistd.h>
#ifdef AILO_TEST_VULKAN
extern "C" {
#include "drm-library-log.h"
#include "loader/log.h"
#include "util/library-log-data.h"
#include "util/library-log.h"
}
#endif

extern "C" void ggml_log_internal(ggml_log_level, const char *, ...);
extern "C" void ggml_print_backtrace(void);

#define CHECK(value)                                                           \
  do {                                                                         \
    if (!(value))                                                              \
      throw std::runtime_error(#value);                                        \
  } while (0)

namespace {
struct standard_streams {
  FILE *out = std::tmpfile(), *err = std::tmpfile();
  int saved_out = -1, saved_err = -1;
  standard_streams() {
    CHECK(out && err);
    CHECK(std::fflush(nullptr) == 0);
    saved_out = dup(STDOUT_FILENO);
    saved_err = dup(STDERR_FILENO);
    CHECK(saved_out >= 0 && saved_err >= 0);
    CHECK(dup2(fileno(out), STDOUT_FILENO) >= 0);
    CHECK(dup2(fileno(err), STDERR_FILENO) >= 0);
  }
  void empty() {
    CHECK(std::fflush(stdout) == 0 && std::fflush(stderr) == 0);
    CHECK(std::ftell(out) == 0 && std::ftell(err) == 0);
  }
  ~standard_streams() {
    std::fflush(stdout);
    std::fflush(stderr);
    if (saved_out >= 0) {
      dup2(saved_out, STDOUT_FILENO);
      close(saved_out);
    }
    if (saved_err >= 0) {
      dup2(saved_err, STDERR_FILENO);
      close(saved_err);
    }
    if (out)
      std::fclose(out);
    if (err)
      std::fclose(err);
  }
};
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
  explicit capture(const std::string &name,
                   pslog_level minimum = PSLOG_LEVEL_TRACE)
      : path(name), file(std::fopen(path.c_str(), "w+")) {
    CHECK(file);
    pslog_config config;
    pslog_default_config(&config);
    config.mode = PSLOG_MODE_JSON;
    config.color = PSLOG_COLOR_NEVER;
    config.min_level = minimum;
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

#ifdef AILO_TEST_VULKAN
void gpu_probe(const char *model, pslog_logger *logger) {
  auto config = ailo_client_default_config();
  config.source.local_path = model;
  config.backend = AILO_BACKEND_VULKAN;
  ailo_client *raw = nullptr;
  ailo_error error{};
  const auto status =
      ailo_client_new_with_logger(&config, logger, &raw, &error);
  if (status == AILO_UNSUPPORTED) {
    CHECK(!raw);
    return; // Builds without an eligible card still exercise driver hooks.
  }
  CHECK(status == AILO_OK && raw);
  std::unique_ptr<ailo_client, client_deleter> client(raw);
  ailo_content content{AILO_CONTENT_TEXT, "hello", 5, nullptr};
  ailo_item item{};
  item.content = &content;
  item.content_count = 1;
  auto request = ailo_request_default();
  request.items = &item;
  request.item_count = 1;
  request.max_tokens = 1;
  request.temperature = 0;
  ailo_run_info info{};
  unsigned deltas = 0;
  CHECK(client->generate(client.get(), &request, event, &deltas, &info,
                         &error) == AILO_OK);
  CHECK(deltas == 1 && info.actual_backend == AILO_BACKEND_VULKAN);
}
#endif

void run(const char *model, const std::string &directory) {
  standard_streams standard;
  capture a(directory + "/A.log"), b(directory + "/B.log");
  // No logger: successful inference, repository startup and failed model
  // loading must return through their ordinary surfaces without standard-stream
  // output.
  auto quiet_config = ailo_client_default_config();
  quiet_config.source.local_path = model;
  quiet_config.backend = AILO_BACKEND_CPU;
  ailo_error quiet_error{};
  ailo_client *quiet_raw = nullptr;
  CHECK(ailo_client_new(&quiet_config, &quiet_raw, &quiet_error) == AILO_OK);
  {
    std::unique_ptr<ailo_client, client_deleter> quiet(quiet_raw);
    char *devices = nullptr;
    CHECK(quiet->devices(quiet.get(), &devices, &quiet_error) == AILO_OK &&
          devices);
    ailo_string_free(devices);
    ailo_content content{AILO_CONTENT_TEXT, "hello", 5, nullptr};
    ailo_item item{};
    item.content = &content;
    item.content_count = 1;
    auto input = ailo_request_default();
    input.items = &item;
    input.item_count = 1;
    input.max_tokens = 1;
    input.temperature = 0;
    unsigned deltas = 0;
    ailo_run_info info{};
    CHECK(quiet->generate(quiet.get(), &input, event, &deltas, &info,
                          &quiet_error) == AILO_OK &&
          deltas == 1);
    ailo_api_info api{};
    const auto sink = [](void *, const void *, size_t) { return 1; };
    CHECK(quiet->handle(quiet.get(), "GET", "/v1/chat/completions", nullptr, 0,
                        sink, nullptr, &api, &quiet_error) == AILO_OK);
    CHECK(quiet->handle(quiet.get(), "GET", "/v1/responses/missing-log-test",
                        nullptr, 0, sink, nullptr, &api,
                        &quiet_error) == AILO_NOT_FOUND);
  }
  const auto broken = directory + "/invalid.gguf";
  {
    std::ofstream file(broken);
    file << "invalid GGUF";
  }
  quiet_config.source.local_path = broken.c_str();
  quiet_raw = nullptr;
  CHECK(ailo_client_new(&quiet_config, &quiet_raw, &quiet_error) != AILO_OK &&
        !quiet_raw);
#ifdef AILO_TEST_VULKAN
  mesa_library_fprintf(0, stderr, "quiet-mesa-error\n");
  drm_library_fprintf(0, stderr, "quiet-drm-error\n");
  loader_log(nullptr, VULKAN_LOADER_FATAL_ERROR_BIT, 0, "quiet-loader-fatal");
#endif
  standard.empty();
#ifdef AILO_TEST_VULKAN
  gpu_probe(model, nullptr);
  gpu_probe(model, a.parent.get());
  standard.empty();
#endif

  // Diagnostic FILE output is routed to the scoped logger. Explicit data-file
  // output retains native stdio semantics.
  {
    ailo::client::logging_state owner(a.parent.get(), "native-diagnostics");
    ailo::client::log_scope scope(&owner, ailo::client::log_phase::load);
    ggml_log_fprintf(GGML_LOG_LEVEL_ERROR, stderr, "native-error-probe\n");
    ggml_log_printf(GGML_LOG_LEVEL_TRACE, "native-stdout-probe\n");
    ggml_log_stream{GGML_LOG_LEVEL_TRACE}.stream()
        << "vulkan-pipeline-trace" << 42 << std::endl;
#ifdef AILO_TEST_VULKAN
    mesa_library_fprintf(2, stderr, "mesa-trace-probe\n");
    mesa_library_fprintf(1, stderr, "mesa-warning-probe\n");
    mesa_library_fprintf(0, stderr, "mesa-error-probe\n");
    const char binary[] = {'n', 'u', 'l', '\0', 'x'};
    CHECK(mesa_library_fwrite(2, binary, 1, sizeof(binary), stderr) ==
          sizeof(binary));
    drm_library_printf(2, "drm-trace-probe\n");
    drm_library_fprintf(0, stderr, "drm-error-probe\n");
    loader_log(nullptr, VULKAN_LOADER_DEBUG_BIT, 0, "loader-trace-probe");
    loader_log(nullptr, VULKAN_LOADER_WARN_BIT, 0, "loader-warning-probe");
    loader_log(nullptr, VULKAN_LOADER_ERROR_BIT, 0, "loader-error-probe");
    loader_log(nullptr, VULKAN_LOADER_FATAL_ERROR_BIT, 0, "loader-fatal-probe");
#endif
    const float invalid = std::numeric_limits<float>::quiet_NaN();
    CHECK(!ggml_validate_row_data(GGML_TYPE_F32, &invalid, sizeof(invalid)));
    if (!std::getenv("GGML_NO_BACKTRACE")) {
      ggml_print_backtrace();
      CHECK(a.text().find("#0 ") != std::string::npos);
    }
    std::unique_ptr<FILE, file_deleter> file(std::tmpfile());
    CHECK(file && ggml_log_fprintf(GGML_LOG_LEVEL_INFO, file.get(), "%s",
                                   "caller-file") == 11);
    std::rewind(file.get());
    char bytes[12]{};
    CHECK(std::fread(bytes, 1, 11, file.get()) == 11 &&
          std::string(bytes) == "caller-file");
#ifdef AILO_TEST_VULKAN
    std::rewind(file.get());
    CHECK(mesa_library_fwrite(2, binary, 1, sizeof(binary), file.get()) ==
          sizeof(binary));
    std::rewind(file.get());
    char copied[sizeof(binary)]{};
    CHECK(std::fread(copied, 1, sizeof(copied), file.get()) == sizeof(copied));
    CHECK(std::memcmp(binary, copied, sizeof(binary)) == 0);
    CHECK(mesa_library_fwrite(2, binary, 0, sizeof(binary), stderr) == 0);
    CHECK(mesa_library_fwrite(2, binary, 2, SIZE_MAX, stderr) == 0);
#endif
  }
  {
    capture filtered(directory + "/info.log", PSLOG_LEVEL_INFO);
    ailo::client::logging_state owner(filtered.parent.get(), "filtered");
    ailo::client::log_scope scope(&owner, ailo::client::log_phase::devices);
    ggml_log_stream{GGML_LOG_LEVEL_TRACE}.stream()
        << "filtered-vulkan-detail" << std::endl;
    ggml_log_stream{GGML_LOG_LEVEL_ERROR}.stream()
        << "visible-vulkan-failure" << std::endl;
#ifdef AILO_TEST_VULKAN
    mesa_library_fprintf(2, stderr, "filtered-mesa-detail\n");
    drm_library_printf(2, "filtered-drm-detail\n");
    loader_log(nullptr, VULKAN_LOADER_DEBUG_BIT, 0, "filtered-loader-detail");
    mesa_library_fprintf(0, stderr, "visible-mesa-failure\n");
    drm_library_fprintf(0, stderr, "visible-drm-failure\n");
    loader_log(nullptr, VULKAN_LOADER_ERROR_BIT, 0, "visible-loader-failure");
#endif
    const auto output = filtered.text();
    CHECK(output.find("filtered-vulkan-detail") == std::string::npos);
    CHECK(output.find("visible-vulkan-failure") != std::string::npos);
#ifdef AILO_TEST_VULKAN
    for (const char *component : {"mesa", "drm", "loader"}) {
      CHECK(output.find(std::string("filtered-") + component + "-detail") ==
            std::string::npos);
      CHECK(output.find(std::string("visible-") + component + "-failure") !=
            std::string::npos);
    }
#endif
  }
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
  char *devices = nullptr;
  CHECK(client->devices(client.get(), &devices, &error) == AILO_OK && devices);
  ailo_string_free(devices);
  a.parent.reset();
  ailo_content content{AILO_CONTENT_TEXT, "hello", 5, nullptr};
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
  CHECK(text_a.find("native-error-probe") != std::string::npos);
  CHECK(text_a.find("native-stdout-probe") != std::string::npos);
  CHECK(text_a.find("vulkan-pipeline-trace") != std::string::npos);
  CHECK(text_a.find("found nan value") != std::string::npos);
  CHECK(text_a.find("caller-file") == std::string::npos);
#ifdef AILO_TEST_VULKAN
  CHECK(text_a.find("nul\\\\x00x") != std::string::npos);
  for (const char *probe :
       {"mesa-trace-probe", "mesa-warning-probe", "mesa-error-probe",
        "drm-trace-probe", "drm-error-probe", "loader-trace-probe",
        "loader-warning-probe", "loader-error-probe", "loader-fatal-probe"})
    CHECK(text_a.find(probe) != std::string::npos);
  for (const auto &entry : {std::make_pair("mesa-trace-probe", "trace"),
                            std::make_pair("mesa-warning-probe", "warn"),
                            std::make_pair("mesa-error-probe", "error"),
                            std::make_pair("drm-trace-probe", "trace"),
                            std::make_pair("drm-error-probe", "error"),
                            std::make_pair("loader-trace-probe", "trace"),
                            std::make_pair("loader-warning-probe", "warn"),
                            std::make_pair("loader-error-probe", "error"),
                            std::make_pair("loader-fatal-probe", "error")}) {
    std::istringstream lines(text_a);
    std::string line;
    bool found = false;
    while (std::getline(lines, line))
      if (line.find(entry.first) != std::string::npos) {
        CHECK(line.find(std::string("\"lvl\":\"") + entry.second + "\"") !=
              std::string::npos);
        found = true;
      }
    CHECK(found);
  }
#endif
  standard.empty();
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
