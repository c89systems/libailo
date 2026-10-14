#include "ailo/native.h"
#include "ggml-backend.h"
#include "ggml-cpu.h"
#include "ggml-opt.h"
#include "llama.h"
#include <cstdio>
#include <cstring>
#include <stdexcept>
#include <thread>

#define CHECK(test)                                                            \
  do {                                                                         \
    if (!(test))                                                               \
      throw std::runtime_error(#test);                                         \
  } while (0)

namespace {
struct counter {
  unsigned calls = 0;
  void *native_user = nullptr;
};
ailo_bool progress(float value, void *native_user, void *data) {
  auto &self = *static_cast<counter *>(data);
  CHECK(value == 0.25f && native_user == self.native_user);
  ++self.calls;
  return 17;
}
void log_message(enum ailo_ggml_log_level level, const char *text,
                 void *native_user, void *data) {
  auto &self = *static_cast<counter *>(data);
  CHECK(level == AILO_GGML_LOG_LEVEL_WARN &&
        !std::strcmp(text, "callback-test"));
  CHECK(native_user == self.native_user);
  ++self.calls;
}
ailo_bool throws(void *, void *) {
  throw std::runtime_error("private callback");
}
struct ailo_ggml_opt_optimizer_params *optimizer(void *native_user,
                                                 void *data) {
  CHECK(native_user == data);
  return static_cast<ailo_ggml_opt_optimizer_params *>(data);
}
struct ailo_ggml_backend_meta_split_state *
split(const struct ailo_ggml_tensor *, void *native_user, void *data) {
  CHECK(native_user == data);
  return static_cast<ailo_ggml_backend_meta_split_state *>(data);
}
const char *sampler_name(const struct ailo_llama_sampler *, void *data) {
  return static_cast<const char *>(data);
}
void nested_callback(struct ailo_ggml_backend *, ailo_ggml_abort_callback cb,
                     void *native_user, void *data) {
  ailo_error error{};
  ailo_bool value = 0;
  CHECK(ailo_ggml_abort_callback_call(cb, native_user, &value, &error) ==
        AILO_OK);
  CHECK(value == 1);
  ++*static_cast<unsigned *>(data);
}
ailo_bool accept_abort(void *native_user, void *data) {
  CHECK(native_user == data);
  return 1;
}

struct sampler_owner {
  ailo_llama_sampler_i *interface = nullptr;
  unsigned cloned = 0, freed = 0, applied = 0;
};
void sampler_apply(ailo_llama_sampler *, ailo_llama_token_data_array *,
                   void *data) {
  ++static_cast<sampler_owner *>(data)->applied;
}
ailo_llama_sampler *sampler_clone(const ailo_llama_sampler *, void *data) {
  auto &owner = *static_cast<sampler_owner *>(data);
  ailo_llama_sampler *copy = nullptr;
  ailo_error error{};
  CHECK(ailo_llama_sampler_init(owner.interface, new unsigned(7), &copy,
                                &error) == AILO_OK);
  ++owner.cloned;
  return copy;
}
void sampler_free(ailo_llama_sampler *sampler, void *data) {
  void *context = nullptr;
  ailo_error error{};
  CHECK(ailo_llama_sampler_ctx_get(sampler, &context, &error) == AILO_OK);
  delete static_cast<unsigned *>(context);
  ++static_cast<sampler_owner *>(data)->freed;
}

void run() {
  ailo_error error{};
  counter first, second;
  int marker = 3;
  first.native_user = &marker;
  second.native_user = &second;
  ailo_llama_progress_callback a = nullptr, b = nullptr;
  CHECK(ailo_llama_progress_callback_new(progress, &first, &a, &error) ==
        AILO_OK);
  CHECK(ailo_llama_progress_callback_new(progress, &second, &b, &error) ==
        AILO_OK);
  auto invoke = [&](ailo_llama_progress_callback cb, void *user) {
    for (unsigned i = 0; i < 32; ++i) {
      ailo_bool value = 0;
      ailo_error local{};
      CHECK(ailo_llama_progress_callback_call(cb, 0.25f, user, &value,
                                              &local) == AILO_OK &&
            value == 1);
    }
  };
  std::thread worker([&]() { invoke(a, first.native_user); });
  invoke(b, second.native_user);
  worker.join();
  CHECK(first.calls == 32 && second.calls == 32);
  CHECK(ailo_llama_progress_callback_error(a, &error) == AILO_OK);
  CHECK(ailo_llama_progress_callback_error(b, &error) == AILO_OK);
  CHECK(ailo_llama_progress_callback_free(a, &error) == AILO_OK);
  invoke(b, second.native_user);
  CHECK(second.calls == 64);
  CHECK(ailo_llama_progress_callback_free(b, &error) == AILO_OK);
  for (unsigned i = 0; i < 1000; ++i) {
    CHECK(ailo_llama_progress_callback_new(progress, &first, &a, &error) ==
          AILO_OK);
    CHECK(ailo_llama_progress_callback_free(a, &error) == AILO_OK);
  }

  ailo_ggml_log_callback logger = nullptr, view = nullptr;
  CHECK(ailo_ggml_log_callback_new(log_message, &first, &logger, &error) ==
        AILO_OK);
  CHECK(ailo_llama_log_set(logger, &marker, &error) == AILO_OK);
  void *native_user = nullptr;
  CHECK(ailo_llama_log_get(&view, &native_user, &error) == AILO_OK && view &&
        native_user == &marker);
  CHECK(ailo_ggml_log_callback_call(view, AILO_GGML_LOG_LEVEL_WARN,
                                    "callback-test", native_user,
                                    &error) == AILO_OK);
  CHECK(first.calls == 33);
  CHECK(ailo_llama_log_set(nullptr, nullptr, &error) == AILO_OK);
  CHECK(ailo_ggml_log_callback_free(view, &error) == AILO_OK);
  CHECK(ailo_ggml_log_callback_free(logger, &error) == AILO_OK);

  struct ailo_ggml_opt_optimizer_params *parameters = nullptr;
  struct ailo_ggml_opt_params *options = nullptr;
  ailo_ggml_opt_get_optimizer_params get = nullptr;
  CHECK(ailo_ggml_opt_optimizer_params_array_new(1, &parameters, &error) ==
        AILO_OK);
  CHECK(ailo_ggml_opt_optimizer_params_adamw_alpha_set(parameters, 0.125f,
                                                       &error) == AILO_OK);
  CHECK(ailo_ggml_opt_optimizer_params_sgd_wd_set(parameters, 0.75f, &error) ==
        AILO_OK);
  CHECK(ailo_ggml_opt_params_array_new(1, &options, &error) == AILO_OK);
  CHECK(ailo_ggml_opt_get_optimizer_params_new(optimizer, parameters, &get,
                                               &error) == AILO_OK);
  CHECK(ailo_ggml_opt_params_get_opt_pars_set(options, get, &error) == AILO_OK);
  auto value =
      reinterpret_cast<ggml_opt_params *>(options)->get_opt_pars(parameters);
  CHECK(value.adamw.alpha == 0.125f && value.sgd.wd == 0.75f);
  CHECK(ailo_ggml_opt_get_optimizer_params_free(get, &error) == AILO_OK);
  CHECK(ailo_ggml_opt_params_array_free(options, &error) == AILO_OK);
  CHECK(ailo_ggml_opt_optimizer_params_array_free(parameters, &error) ==
        AILO_OK);

  struct ailo_ggml_backend_meta_split_state *state = nullptr;
  ailo_ggml_backend_meta_get_split_state_t get_split = nullptr;
  CHECK(ailo_ggml_backend_meta_split_state_array_new(1, &state, &error) ==
        AILO_OK);
  const ailo_i64 full{0x12345678U, 0xabcdef01U};
  CHECK(ailo_ggml_backend_meta_split_state_ne_set(state, 255, full, &error) ==
        AILO_OK);
  CHECK(ailo_ggml_backend_meta_split_state_nr_set(state, 15, 91, &error) ==
        AILO_OK);
  CHECK(ailo_ggml_backend_meta_get_split_state_t_new(split, state, &get_split,
                                                     &error) == AILO_OK);
  struct ailo_ggml_backend_meta_split_state *result = nullptr;
  CHECK(ailo_ggml_backend_meta_get_split_state_t_call(
            get_split, nullptr, state, &result, &error) == AILO_OK);
  ailo_i64 actual{};
  CHECK(result && result != state);
  CHECK(ailo_ggml_backend_meta_split_state_ne_get(result, 255, &actual,
                                                  &error) == AILO_OK);
  CHECK(actual.hi == full.hi && actual.lo == full.lo);
  ailo_u32 count = 0;
  CHECK(ailo_ggml_backend_meta_split_state_nr_get(result, 15, &count, &error) ==
            AILO_OK &&
        count == 91);
  CHECK(ailo_ggml_backend_meta_split_state_array_free(result, &error) ==
        AILO_OK);
  CHECK(ailo_ggml_backend_meta_split_state_array_free(state, &error) ==
        AILO_OK);
  CHECK(ailo_ggml_backend_meta_get_split_state_t_free(get_split, &error) ==
        AILO_OK);

  struct ailo_llama_sampler_i *interface = nullptr;
  ailo_llama_sampler_i_name_callback name = nullptr, borrowed_name = nullptr;
  CHECK(ailo_llama_sampler_i_array_new(1, &interface, &error) == AILO_OK);
  CHECK(ailo_llama_sampler_i_name_callback_new(
            sampler_name, const_cast<char *>("custom-sampler"), &name,
            &error) == AILO_OK);
  CHECK(ailo_llama_sampler_i_name_set(interface, name, &error) == AILO_OK);
  CHECK(!std::strcmp(
      reinterpret_cast<llama_sampler_i *>(interface)->name(nullptr),
      "custom-sampler"));
  CHECK(ailo_llama_sampler_i_name_get(interface, &borrowed_name, &error) ==
        AILO_OK);
  const char *text = nullptr;
  CHECK(ailo_llama_sampler_i_name_callback_call(borrowed_name, nullptr, &text,
                                                &error) == AILO_OK &&
        !std::strcmp(text, "custom-sampler"));
  CHECK(ailo_llama_sampler_i_name_callback_free(borrowed_name, &error) ==
        AILO_OK);
  CHECK(ailo_llama_sampler_i_array_free(interface, &error) == AILO_OK);
  CHECK(ailo_llama_sampler_i_name_callback_free(name, &error) == AILO_OK);

  sampler_owner owner;
  ailo_llama_sampler_i_apply_callback apply = nullptr;
  ailo_llama_sampler_i_clone_callback clone = nullptr;
  ailo_llama_sampler_i_free_callback release = nullptr;
  CHECK(ailo_llama_sampler_i_array_new(1, &owner.interface, &error) == AILO_OK);
  CHECK(ailo_llama_sampler_i_apply_callback_new(sampler_apply, &owner, &apply,
                                                &error) == AILO_OK);
  CHECK(ailo_llama_sampler_i_clone_callback_new(sampler_clone, &owner, &clone,
                                                &error) == AILO_OK);
  CHECK(ailo_llama_sampler_i_free_callback_new(sampler_free, &owner, &release,
                                               &error) == AILO_OK);
  CHECK(ailo_llama_sampler_i_apply_set(owner.interface, apply, &error) ==
        AILO_OK);
  CHECK(ailo_llama_sampler_i_clone_set(owner.interface, clone, &error) ==
        AILO_OK);
  CHECK(ailo_llama_sampler_i_free_set(owner.interface, release, &error) ==
        AILO_OK);
  ailo_llama_sampler *original = nullptr, *copy = nullptr;
  CHECK(ailo_llama_sampler_init(owner.interface, new unsigned(5), &original,
                                &error) == AILO_OK);
  CHECK(ailo_llama_sampler_clone(original, &copy, &error) == AILO_OK && copy &&
        owner.cloned == 1);
  ailo_llama_token_data_array *candidates = nullptr;
  CHECK(ailo_llama_token_data_array_array_new(1, &candidates, &error) ==
        AILO_OK);
  CHECK(ailo_llama_sampler_free(original, &error) == AILO_OK &&
        owner.freed == 1);
  CHECK(ailo_llama_sampler_apply(copy, candidates, &error) == AILO_OK &&
        owner.applied == 1);
  CHECK(ailo_llama_sampler_free(copy, &error) == AILO_OK && owner.freed == 2);
  CHECK(ailo_llama_token_data_array_array_free(candidates, &error) == AILO_OK);
  CHECK(ailo_llama_sampler_i_array_free(owner.interface, &error) == AILO_OK);
  CHECK(ailo_llama_sampler_i_apply_callback_free(apply, &error) == AILO_OK);
  CHECK(ailo_llama_sampler_i_clone_callback_free(clone, &error) == AILO_OK);
  CHECK(ailo_llama_sampler_i_free_callback_free(release, &error) == AILO_OK);

  ailo_ggml_abort_callback abort = nullptr;
  ailo_ggml_backend_set_abort_callback_t setter = nullptr;
  unsigned calls = 0;
  CHECK(ailo_ggml_abort_callback_new(accept_abort, &marker, &abort, &error) ==
        AILO_OK);
  CHECK(ailo_ggml_backend_set_abort_callback_t_new(nested_callback, &calls,
                                                   &setter, &error) == AILO_OK);
  CHECK(ailo_ggml_backend_set_abort_callback_t_call(
            setter, nullptr, abort, &marker, &error) == AILO_OK &&
        calls == 1);
  CHECK(ailo_ggml_backend_set_abort_callback_t_free(setter, &error) == AILO_OK);
  CHECK(ailo_ggml_abort_callback_free(abort, &error) == AILO_OK);
  CHECK(ailo_ggml_abort_callback_new(throws, nullptr, &abort, &error) ==
        AILO_OK);
  ailo_bool cancelled = 0;
  CHECK(ailo_ggml_abort_callback_call(abort, nullptr, &cancelled, &error) ==
            AILO_OK &&
        cancelled);
  CHECK(ailo_ggml_abort_callback_error(abort, &error) == AILO_INTERNAL_ERROR);
  CHECK(!std::strstr(error.message, "private callback"));
  CHECK(ailo_ggml_abort_callback_clear_error(abort, &error) == AILO_OK);
  CHECK(ailo_ggml_abort_callback_error(abort, &error) == AILO_OK);
  CHECK(ailo_ggml_abort_callback_free(abort, &error) == AILO_OK);

  auto native_procedure = ggml_backend_cpu_set_n_threads;
  void *address = nullptr;
  static_assert(sizeof(address) == sizeof(native_procedure),
                "Native procedure ABI");
  std::memcpy(&address, &native_procedure, sizeof(address));
  ailo_ggml_backend_set_n_threads_t procedure = nullptr;
  CHECK(ailo_ggml_backend_set_n_threads_t_from_address(address, &procedure,
                                                       &error) == AILO_OK);
  auto backend = ggml_backend_cpu_init();
  CHECK(backend);
  CHECK(ailo_ggml_backend_set_n_threads_t_call(
            procedure, reinterpret_cast<ailo_ggml_backend *>(backend), 3,
            &error) == AILO_OK);
  ggml_backend_free(backend);
  CHECK(ailo_ggml_backend_set_n_threads_t_free(procedure, &error) == AILO_OK);
}
} // namespace

int main() {
  try {
    run();
    std::puts("Independent C89 callback closures, native registration, nested "
              "callbacks, aggregate ABI and exception containment: passed");
    return 0;
  } catch (const std::exception &error) {
    std::fprintf(stderr, "%s\n", error.what());
    return 1;
  }
}
