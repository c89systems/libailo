#include "ailo_cache_internal.hpp"
#include "ailo_client_internal.hpp"
#include "ailo_compaction_internal.hpp"
#include "ailo_continuation_internal.hpp"
#include "ailo_logging_internal.hpp"
#include "ailo_openai_internal.hpp"
#include <algorithm>
#include <climits>
#include <cstdlib>
#include <cstring>
#include <openssl/crypto.h>

#define AILO_PUBLIC extern "C" __attribute__((visibility("default")))

namespace ailo {
namespace client {
namespace {
std::string copy_string(const char *input) {
  if (!input)
    return {};
  const size_t n = strnlen(input, 65537);
  if (n > 65536)
    throw failure(AILO_OUT_OF_RANGE, "Configuration string exceeds 64 KiB");
  return std::string(input, n);
}

state &get_state(ailo_client *client) {
  if (!client || !client->state)
    throw failure(AILO_INVALID_ARGUMENT, "A live client is required");
  return *static_cast<state *>(client->state);
}

bool abort_inference(void *data) {
  return static_cast<state *>(data)->is_cancelled();
}

bool load_progress(float, void *data) {
  return !static_cast<state *>(data)->is_cancelled();
}

int acquisition_progress(void *data, unsigned shard, unsigned shards,
                         ailo_u64 received, ailo_u64 total) {
  auto &self = *static_cast<state *>(data);
  if (self.is_cancelled())
    return 0;
  try {
    return !self.source.value.progress ||
           self.source.value.progress(self.source.value.user_data, shard,
                                      shards, received, total);
  } catch (...) {
    self.progress_failed = true;
    return 0;
  }
}
} // namespace

source_copy::source_copy(const ailo_model_source &input)
    : value(input), local(copy_string(input.local_path)),
      repo(copy_string(input.repo)), revision(copy_string(input.revision)),
      filename(copy_string(input.filename)),
      cache(copy_string(input.cache_root)),
      token(copy_string(input.bearer_token)) {
  if (input.sha256_count > 65535 || (input.sha256_count && !input.sha256))
    throw failure(AILO_INVALID_ARGUMENT, "Invalid source checksum array");
  value.local_path = input.local_path ? local.c_str() : nullptr;
  value.repo = input.repo ? repo.c_str() : nullptr;
  value.revision = input.revision ? revision.c_str() : nullptr;
  value.filename = input.filename ? filename.c_str() : nullptr;
  value.cache_root = input.cache_root ? cache.c_str() : nullptr;
  value.bearer_token = input.bearer_token ? token.c_str() : nullptr;
  digests.reserve(input.sha256_count);
  digest_views.reserve(input.sha256_count);
  for (size_t i = 0; i < input.sha256_count; ++i)
    digests.push_back(copy_string(input.sha256[i]));
  for (size_t i = 0; i < digests.size(); ++i)
    digest_views.push_back(input.sha256[i] ? digests[i].c_str() : nullptr);
  value.sha256 = input.sha256 ? digest_views.data() : nullptr;
}

source_copy::~source_copy() {
  if (!token.empty())
    OPENSSL_cleanse(&token[0], token.size());
}

state::state(const ailo_client_config &input, pslog_logger *logger)
    : source(input.source), id(copy_string(input.model_id)),
      template_override(copy_string(input.chat_template)), config(input) {
  if (config.backend < AILO_BACKEND_AUTO ||
      config.backend > AILO_BACKEND_VULKAN || config.threads < 0 ||
      config.gpu_layers < -1 || config.context_tokens > INT_MAX ||
      config.batch_tokens > INT_MAX)
    throw failure(AILO_INVALID_ARGUMENT,
                  "Invalid client inference configuration");
  if (id.empty())
    id = "local-model";
  require_utf8(id.data(), id.size());
  if (id.size() > 256)
    throw failure(AILO_OUT_OF_RANGE, "Served model ID exceeds 256 bytes");
  config.source = source.value;
  config.model_id = id.c_str();
  config.chat_template =
      input.chat_template ? template_override.c_str() : nullptr;
  char *validated = nullptr;
  ailo_error error{};
  const ailo_status status =
      ailo_model_cache_path(&source.value, &validated, &error);
  std::unique_ptr<char, decltype(&std::free)> owned(validated, std::free);
  if (status != AILO_OK)
    throw failure(status, error.message);
  if (logger)
    logging = std::make_unique<logging_state>(logger, id);
  if (input.cache && input.disable_prompt_cache)
    throw failure(AILO_INVALID_ARGUMENT,
                  "Choose an explicit cache or disable caching");
  if (input.cache)
    cache = retain_cache(input.cache);
  else if (!input.disable_prompt_cache) {
    auto options = ailo_cache_default_config();
    options.namespace_id = "private";
    cache = std::make_shared<cache_state>(options);
  }
  config.cache = nullptr; // the retained owner, not the caller's shell, is used
  compactor =
      input.compactor
          ? retain_compactor(input.compactor)
          : std::make_shared<compaction_state>(ailo_compactor_default_config());
  config.compactor = nullptr;
  openai = std::make_unique<openai_state>(input.openai);
  config.openai = nullptr;
}

state::~state() {
  destroying.store(true);
  log_scope logs(logging.get(), log_phase::destroy);
  logs.trace("Destroying inference receiver");
  context.reset();
  model.reset();
  if (backend_owned)
    detail::backend_release();
}

void state::ensure_loaded() {
  if (context)
    return;
  log_scope logs(logging.get(), log_phase::load);
  logs.trace("Loading inference model");
  if (is_cancelled())
    throw failure(AILO_CANCELLED, "Model loading cancelled");
  if (!backend_owned) {
    detail::backend_acquire();
    backend_owned = true;
  }
  if (path.empty()) {
    char *acquired = nullptr;
    ailo_error error{};
    ailo_model_source options = source.value;
    options.progress = acquisition_progress;
    options.user_data = this;
    const ailo_status status = ailo_model_acquire(&options, &acquired, &error);
    std::unique_ptr<char, decltype(&std::free)> owned(acquired, std::free);
    if (progress_failed)
      throw failure(AILO_INTERNAL_ERROR,
                    "Acquisition progress callback raised an exception");
    if (status != AILO_OK)
      throw failure(status, error.message);
    path = acquired;
  }
  selected.clear();
  if (config.backend != AILO_BACKEND_CPU) {
    ggml_backend_reg_t registry = ggml_backend_reg_by_name("Vulkan");
    if (registry)
      for (size_t i = 0; i < ggml_backend_reg_dev_count(registry); ++i)
        selected.push_back(ggml_backend_reg_dev_get(registry, i));
    if (selected.empty() && config.backend == AILO_BACKEND_VULKAN)
      throw failure(AILO_UNSUPPORTED,
                    "No usable built Vulkan device is available");
  }
  actual = selected.empty() ? AILO_BACKEND_CPU : AILO_BACKEND_VULKAN;
  selected.push_back(nullptr);
  for (;;) {
    llama_model_params mp = llama_model_default_params();
    mp.devices = selected.data();
    mp.n_gpu_layers = actual == AILO_BACKEND_CPU
                          ? 0
                          : (config.gpu_layers ? config.gpu_layers : -1);
    mp.split_mode = LLAMA_SPLIT_MODE_LAYER;
    mp.progress_callback = load_progress;
    mp.progress_callback_user_data = this;
    model = std::shared_ptr<llama_model>(
        llama_model_load_from_file(path.c_str(), mp), llama_model_free);
    if (model && !is_cancelled()) {
      llama_context_params cp = llama_context_default_params();
      cp.n_ctx = config.context_tokens
                     ? config.context_tokens
                     : static_cast<uint32_t>(std::min(
                           4096, llama_model_n_ctx_train(model.get())));
      cp.n_batch =
          config.batch_tokens ? config.batch_tokens : std::min(cp.n_ctx, 512u);
      if (cp.n_batch > cp.n_ctx)
        throw failure(AILO_INVALID_ARGUMENT,
                      "Batch tokens exceed context tokens");
      cp.n_ubatch = std::min(cp.n_batch, 128u);
      cp.n_threads = cp.n_threads_batch = config.threads ? config.threads : 2;
      cp.abort_callback = abort_inference;
      cp.abort_callback_data = this;
      context_parameters = cp;
      context = new_context();
    }
    if (is_cancelled()) {
      context.reset();
      model.reset();
      throw failure(AILO_CANCELLED, "Model loading cancelled");
    }
    if (context) {
      logs.trace("Inference model ready");
      return;
    }
    model.reset();
    if (config.backend != AILO_BACKEND_AUTO || actual == AILO_BACKEND_CPU)
      throw failure(AILO_MODEL_FAILED, "Native model/context setup failed");
    actual = AILO_BACKEND_CPU;
    selected.assign(1, nullptr);
  }
}

std::unique_ptr<llama_context, decltype(&llama_free)> state::new_context() {
  return {llama_init_from_model(model.get(), context_parameters), llama_free};
}

operation::operation(ailo_client *client)
    : self(get_state(client)), lock(self.mutex, std::try_to_lock) {
  if (!lock.owns_lock() || self.destroying.load())
    throw failure(AILO_BUSY, "The client is already executing an operation");
  self.cancelled.store(false);
  self.progress_failed = false;
}

std::string metadata(const llama_model *model, const char *key) {
  const int32_t length = llama_model_meta_val_str(model, key, nullptr, 0);
  if (length < 0)
    return {};
  if (length > 1024 * 1024)
    throw failure(AILO_OUT_OF_RANGE, "Model metadata value exceeds 1 MiB");
  std::vector<char> value(static_cast<size_t>(length) + 1);
  if (llama_model_meta_val_str(model, key, value.data(), value.size()) !=
      length)
    throw failure(AILO_MODEL_FAILED, "Model metadata changed during read");
  return std::string(value.data(), static_cast<size_t>(length));
}

void owned_json(const std::string &value, char **output) {
  auto *data = static_cast<char *>(std::malloc(value.size() + 1));
  if (!data)
    throw std::bad_alloc();
  std::memcpy(data, value.c_str(), value.size() + 1);
  *output = data;
}
} // namespace client
} // namespace ailo

AILO_PUBLIC ailo_client_config ailo_client_default_config(void) { return {}; }

AILO_PUBLIC ailo_request ailo_request_default(void) {
  ailo_request result{};
  result.max_tokens = 128;
  result.candidates = 1;
  result.temperature = 0.8;
  result.top_p = 0.95;
  result.seed = LLAMA_DEFAULT_SEED;
  result.parallel_tool_calls = 1;
  return result;
}

AILO_PUBLIC ailo_status ailo_client_new(const ailo_client_config *config,
                                        ailo_client **output,
                                        ailo_error *error) {
  return ailo_client_new_with_logger(config, nullptr, output, error);
}

AILO_PUBLIC ailo_status ailo_client_new_with_logger(
    const ailo_client_config *config, pslog_logger *logger,
    ailo_client **output, ailo_error *error) {
  if (output)
    *output = nullptr;
  if (!config || !output)
    return ailo_error_set(error, AILO_INVALID_ARGUMENT,
                          "Client configuration and output are required");
  return ailo::client::boundary(error, [&]() {
    std::unique_ptr<ailo_client> shell(new ailo_client{});
    std::unique_ptr<ailo::client::state> state(
        new ailo::client::state(*config, logger));
    if (!config->lazy)
      state->ensure_loaded();
    shell->state = state.release();
    shell->generate = ailo_client_generate;
    shell->compact = ailo_client_compact;
    shell->model_card = ailo_client_model_card;
    shell->capabilities = ailo_client_capabilities;
    shell->devices = ailo_client_devices;
    shell->cache_stats = ailo_client_cache_stats;
    shell->cache_clear = ailo_client_cache_clear;
    shell->snapshot_export = ailo_client_snapshot_export;
    shell->snapshot_import = ailo_client_snapshot_import;
    shell->cancel = ailo_client_cancel;
    shell->destroy = ailo_client_destroy;
    shell->chat_completions = ailo_client_chat_completions;
    shell->chat_retrieve = ailo_client_chat_retrieve;
    shell->chat_delete = ailo_client_chat_delete;
    shell->responses = ailo_client_responses;
    shell->response_retrieve = ailo_client_response_retrieve;
    shell->response_delete = ailo_client_response_delete;
    shell->response_compact = ailo_client_response_compact;
    *output = shell.release();
  });
}

AILO_PUBLIC ailo_status ailo_client_generate(ailo_client *client,
                                             const ailo_request *request,
                                             ailo_event_sink sink, void *user,
                                             ailo_run_info *output,
                                             ailo_error *error) {
  if (output)
    *output = {};
  if (!request || !sink || !output)
    return ailo_error_set(error, AILO_INVALID_ARGUMENT,
                          "Request, event sink and run info are required");
  return ailo::client::boundary(error, [&]() {
    ailo::client::operation call(client);
    ailo::client::log_scope logs(call.self.logging.get(),
                                 ailo::client::log_phase::generate);
    std::unique_ptr<ailo::client::continuation_transaction> history;
    if (request->continuation) {
      history = std::make_unique<ailo::client::continuation_transaction>(
          call.self, *request);
    }
    logs.trace("Starting generation");
    ailo::client::generate(call.self, history ? history->input : *request, sink,
                           user, *output, history.get());
    if (history)
      history->commit();
    logs.trace("Generation complete");
  });
}

namespace {
template <typename Function>
ailo_status json_operation(ailo_client *client, char **output,
                           ailo_error *error, ailo::client::log_phase phase,
                           Function work) {
  if (output)
    *output = nullptr;
  if (!output)
    return ailo_error_set(error, AILO_INVALID_ARGUMENT,
                          "JSON output is required");
  return ailo::client::boundary(error, [&]() {
    ailo::client::operation call(client);
    ailo::client::log_scope logs(call.self.logging.get(), phase);
    ailo::client::owned_json(work(call.self), output);
  });
}
} // namespace

AILO_PUBLIC ailo_status ailo_client_cache_stats(ailo_client *client,
                                                ailo_cache_stats *output,
                                                ailo_error *error) {
  if (output)
    *output = {};
  if (!output)
    return ailo_error_set(error, AILO_INVALID_ARGUMENT,
                          "Cache statistics output is required");
  return ailo::client::boundary(error, [&]() {
    ailo::client::operation call(client);
    if (call.self.cache)
      call.self.cache->stats(*output);
  });
}
AILO_PUBLIC ailo_status ailo_client_cache_clear(ailo_client *client,
                                                ailo_error *error) {
  return ailo::client::boundary(error, [&]() {
    ailo::client::operation call(client);
    if (call.self.cache)
      call.self.cache->clear();
  });
}
AILO_PUBLIC ailo_status ailo_client_model_card(ailo_client *client,
                                               char **output,
                                               ailo_error *error) {
  return json_operation(client, output, error,
                        ailo::client::log_phase::metadata,
                        [](ailo::client::state &self) {
                          self.ensure_loaded();
                          return ailo::client::model_card(self);
                        });
}

AILO_PUBLIC ailo_status ailo_client_snapshot_export(ailo_client *client,
                                                    const char *slot,
                                                    ailo_error *error) {
  return ailo::client::boundary(error, [&]() {
    ailo::client::operation call(client);
    if (!call.self.cache)
      throw ailo::client::failure(AILO_UNSUPPORTED,
                                  "Prompt caching is disabled");
    call.self.cache->snapshot_export(call.self, slot);
  });
}
AILO_PUBLIC ailo_status ailo_client_snapshot_import(ailo_client *client,
                                                    const char *slot,
                                                    const char *key,
                                                    ailo_error *error) {
  return ailo::client::boundary(error, [&]() {
    ailo::client::operation call(client);
    if (!call.self.cache || !call.self.cache->disk)
      throw ailo::client::failure(AILO_UNSUPPORTED,
                                  "Encrypted snapshots are not configured");
    ailo::client::log_scope logs(call.self.logging.get(),
                                 ailo::client::log_phase::generate);
    call.self.ensure_loaded();
    ailo::client::identify_cache(call.self);
    call.self.cache->snapshot_import(call.self, slot, key);
  });
}

AILO_PUBLIC ailo_status ailo_client_capabilities(ailo_client *client,
                                                 char **output,
                                                 ailo_error *error) {
  return json_operation(client, output, error,
                        ailo::client::log_phase::metadata,
                        ailo::client::capabilities);
}

AILO_PUBLIC ailo_status ailo_client_devices(ailo_client *client, char **output,
                                            ailo_error *error) {
  return json_operation(client, output, error, ailo::client::log_phase::devices,
                        [](ailo::client::state &) {
                          ailo::detail::backend_acquire();
                          struct release {
                            ~release() { ailo::detail::backend_release(); }
                          } guard;
                          return ailo::client::devices();
                        });
}

AILO_PUBLIC void ailo_client_cancel(ailo_client *client) {
  if (client && client->state)
    static_cast<ailo::client::state *>(client->state)->cancelled.store(true);
}

AILO_PUBLIC void ailo_client_destroy(ailo_client *client) {
  if (client) {
    delete static_cast<ailo::client::state *>(client->state);
    delete client;
  }
}
