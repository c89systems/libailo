#ifndef AILO_CLIENT_INTERNAL_HPP
#define AILO_CLIENT_INTERNAL_HPP

#include "ailo/client.h"
#include "ailo_backend_internal.hpp"
#include "ailo_error_internal.h"
#include "ailo_integer_bridge.hpp"
#include "gguf.h"
#include "llama.h"
#include <atomic>
#include <memory>
#include <mutex>
#include <stdexcept>
#include <string>
#include <vector>

namespace ailo {
namespace client {
struct failure : std::runtime_error {
  ailo_status status;
  failure(ailo_status code, const std::string &message)
      : std::runtime_error(message), status(code) {}
};

struct source_copy {
  ailo_model_source value;
  std::string local, repo, revision, filename, cache, token;
  std::vector<std::string> digests;
  std::vector<const char *> digest_views;
  explicit source_copy(const ailo_model_source &source);
  ~source_copy();
};

struct logging_state;
struct cache_state;

struct state {
  std::mutex mutex;
  std::atomic<bool> cancelled{false};
  std::atomic<bool> destroying{false};
  source_copy source;
  std::string id, template_override, path;
  ailo_client_config config;
  std::unique_ptr<llama_model, decltype(&llama_model_free)> model{
      nullptr, llama_model_free};
  std::unique_ptr<llama_context, decltype(&llama_free)> context{nullptr,
                                                                llama_free};
  ailo_backend_policy actual = AILO_BACKEND_CPU;
  std::vector<ggml_backend_dev_t> selected;
  bool backend_owned = false;
  bool progress_failed = false;
  std::unique_ptr<logging_state> logging;
  std::shared_ptr<cache_state> cache;
  std::string cache_identity;
  llama_context_params context_parameters{};
  explicit state(const ailo_client_config &input, pslog_logger *logger);
  ~state();
  void ensure_loaded();
  std::unique_ptr<llama_context, decltype(&llama_free)> new_context();
};

template <typename Work> ailo_status boundary(ailo_error *error, Work work) {
  try {
    work();
    ailo_error_clear(error);
    return AILO_OK;
  } catch (const failure &e) {
    return ailo_error_set(error, e.status, e.what());
  } catch (const std::bad_alloc &) {
    return ailo_error_set(error, AILO_NO_MEMORY,
                          "Inference receiver allocation failed");
  } catch (...) {
    return ailo_error_set(error, AILO_INTERNAL_ERROR,
                          "Inference receiver raised an exception");
  }
}

struct operation {
  state &self;
  std::unique_lock<std::mutex> lock;
  explicit operation(ailo_client *client);
};

std::string metadata(const llama_model *model, const char *key);
void owned_json(const std::string &value, char **output);
std::string model_card(state &self);
std::string capabilities(state &self);
std::string devices();
void generate(state &self, const ailo_request &request, ailo_event_sink sink,
              void *user, ailo_run_info &info);
void require_utf8(const char *data, size_t size);
bool template_supported(const char *source);
} // namespace client
} // namespace ailo
#endif
