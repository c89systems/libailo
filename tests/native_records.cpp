/* Generated from the pinned upstream public API inventory.
 * Regenerate with scripts/generate_native.py; do not edit manually.
 * See docs/facade-bindings.md for ownership and bridge semantics. */

#include "ailo/native.h"
#include "ailo_integer_bridge.hpp"
#include "ggml/include/ggml-alloc.h"
#include "ggml/include/ggml-backend.h"
#include "ggml/include/ggml-blas.h"
#include "ggml/include/ggml-cann.h"
#include "ggml/include/ggml-cpu.h"
#include "ggml/include/ggml-cuda.h"
#include "ggml/include/ggml-metal.h"
#include "ggml/include/ggml-openvino.h"
#include "ggml/include/ggml-opt.h"
#include "ggml/include/ggml-rpc.h"
#include "ggml/include/ggml-sycl.h"
#include "ggml/include/ggml-virtgpu.h"
#include "ggml/include/ggml-vulkan.h"
#include "ggml/include/ggml-webgpu.h"
#include "ggml/include/ggml-zendnn.h"
#include "ggml/include/ggml.h"
#include "ggml/include/gguf.h"
#include "include/llama.h"
#include "tools/mtmd/mtmd-helper.h"
#include "tools/mtmd/mtmd.h"
#include <cstdio>
#include <cstring>
#include <limits>
#define CHECK(x)                                                               \
  do {                                                                         \
    if (!(x)) {                                                                \
      std::fprintf(stderr, "%s:%d: %s\n", __FILE__, __LINE__, #x);             \
      return 1;                                                                \
    }                                                                          \
  } while (0)
int main() {
  ailo_error error = {};
  alignas(64) unsigned char address[64] = {};
  size_t tested = 0;
  {
    struct ailo_ggml_backend_dev_caps *self = nullptr;
    CHECK(ailo_ggml_backend_dev_caps_array_new(2, &self, &error) == AILO_OK &&
          self);
    auto native = reinterpret_cast<struct ggml_backend_dev_caps *>(self);
    (void)native;
    {
      ailo_bool value = 17;
      ailo_bool result = {};
      CHECK(ailo_ggml_backend_dev_caps_async_set(self, value, &error) ==
            AILO_OK);
      CHECK(ailo_ggml_backend_dev_caps_async_get(self, &result, &error) ==
            AILO_OK);
      CHECK(result == 1 && native->async);
      ++tested;
    }
    {
      ailo_bool value = 17;
      ailo_bool result = {};
      CHECK(ailo_ggml_backend_dev_caps_host_buffer_set(self, value, &error) ==
            AILO_OK);
      CHECK(ailo_ggml_backend_dev_caps_host_buffer_get(self, &result, &error) ==
            AILO_OK);
      CHECK(result == 1 && native->host_buffer);
      ++tested;
    }
    {
      ailo_bool value = 17;
      ailo_bool result = {};
      CHECK(ailo_ggml_backend_dev_caps_buffer_from_host_ptr_set(
                self, value, &error) == AILO_OK);
      CHECK(ailo_ggml_backend_dev_caps_buffer_from_host_ptr_get(
                self, &result, &error) == AILO_OK);
      CHECK(result == 1 && native->buffer_from_host_ptr);
      ++tested;
    }
    {
      ailo_bool value = 17;
      ailo_bool result = {};
      CHECK(ailo_ggml_backend_dev_caps_events_set(self, value, &error) ==
            AILO_OK);
      CHECK(ailo_ggml_backend_dev_caps_events_get(self, &result, &error) ==
            AILO_OK);
      CHECK(result == 1 && native->events);
      ++tested;
    }
    {
      ailo_bool value = 17;
      ailo_bool result = {};
      CHECK(ailo_ggml_backend_dev_caps_mmap_support_set(self, value, &error) ==
            AILO_OK);
      CHECK(ailo_ggml_backend_dev_caps_mmap_support_get(self, &result,
                                                        &error) == AILO_OK);
      CHECK(result == 1 && native->mmap_support);
      ++tested;
    }
    struct ailo_ggml_backend_dev_caps *element = nullptr;
    CHECK(ailo_ggml_backend_dev_caps_array_at(self, 2, 1, &element, &error) ==
          AILO_OK);
    CHECK(reinterpret_cast<struct ggml_backend_dev_caps *>(element) ==
          native + 1);
    CHECK(ailo_ggml_backend_dev_caps_array_at(self, 2, 2, &element, &error) ==
              AILO_OUT_OF_RANGE &&
          !element);
    CHECK(ailo_ggml_backend_dev_caps_array_new(
              std::numeric_limits<size_t>::max(), &element, &error) ==
              AILO_OUT_OF_RANGE &&
          !element);
    CHECK(ailo_ggml_backend_dev_caps_array_free(self, &error) == AILO_OK);
    CHECK(ailo_ggml_backend_dev_caps_array_free(nullptr, &error) == AILO_OK);
  }
  {
    struct ailo_ggml_backend_dev_props *self = nullptr;
    CHECK(ailo_ggml_backend_dev_props_array_new(2, &self, &error) == AILO_OK &&
          self);
    auto native = reinterpret_cast<struct ggml_backend_dev_props *>(self);
    (void)native;
    {
      const char *value = reinterpret_cast<const char *>(address);
      const char *result = {};
      CHECK(ailo_ggml_backend_dev_props_name_set(self, value, &error) ==
            AILO_OK);
      CHECK(ailo_ggml_backend_dev_props_name_get(self, &result, &error) ==
            AILO_OK);
      CHECK(result == value);
      CHECK(native->name == static_cast<const char *>(value));
      ++tested;
    }
    {
      const char *value = reinterpret_cast<const char *>(address);
      const char *result = {};
      CHECK(ailo_ggml_backend_dev_props_description_set(self, value, &error) ==
            AILO_OK);
      CHECK(ailo_ggml_backend_dev_props_description_get(self, &result,
                                                        &error) == AILO_OK);
      CHECK(result == value);
      CHECK(native->description == static_cast<const char *>(value));
      ++tested;
    }
    {
      size_t value = 97;
      size_t result = {};
      CHECK(ailo_ggml_backend_dev_props_memory_free_set(self, value, &error) ==
            AILO_OK);
      CHECK(ailo_ggml_backend_dev_props_memory_free_get(self, &result,
                                                        &error) == AILO_OK);
      CHECK(result == value);
      CHECK(native->memory_free == static_cast<size_t>(value));
      ++tested;
    }
    {
      size_t value = 97;
      size_t result = {};
      CHECK(ailo_ggml_backend_dev_props_memory_total_set(self, value, &error) ==
            AILO_OK);
      CHECK(ailo_ggml_backend_dev_props_memory_total_get(self, &result,
                                                         &error) == AILO_OK);
      CHECK(result == value);
      CHECK(native->memory_total == static_cast<size_t>(value));
      ++tested;
    }
    {
      enum ailo_ggml_backend_dev_type value = AILO_GGML_BACKEND_DEVICE_TYPE_CPU;
      enum ailo_ggml_backend_dev_type result = {};
      CHECK(ailo_ggml_backend_dev_props_type_set(self, value, &error) ==
            AILO_OK);
      CHECK(ailo_ggml_backend_dev_props_type_get(self, &result, &error) ==
            AILO_OK);
      CHECK(result == value);
      CHECK(native->type == static_cast<enum ggml_backend_dev_type>(value));
      ++tested;
    }
    {
      const char *value = reinterpret_cast<const char *>(address);
      const char *result = {};
      CHECK(ailo_ggml_backend_dev_props_device_id_set(self, value, &error) ==
            AILO_OK);
      CHECK(ailo_ggml_backend_dev_props_device_id_get(self, &result, &error) ==
            AILO_OK);
      CHECK(result == value);
      CHECK(native->device_id == static_cast<const char *>(value));
      ++tested;
    }
    {
      struct ailo_ggml_backend_dev_caps *value = nullptr, *result = nullptr;
      CHECK(ailo_ggml_backend_dev_caps_array_new(1, &value, &error) == AILO_OK);
      CHECK(ailo_ggml_backend_dev_caps_events_set(value, 1, &error) == AILO_OK);
      CHECK(ailo_ggml_backend_dev_props_caps_set(self, value, &error) ==
            AILO_OK);
      CHECK(ailo_ggml_backend_dev_props_caps_get(self, &result, &error) ==
                AILO_OK &&
            result != value);
      CHECK(native->caps.events == true);
      CHECK(reinterpret_cast<struct ggml_backend_dev_caps *>(result)->events ==
            true);
      CHECK(ailo_ggml_backend_dev_caps_array_free(result, &error) == AILO_OK);
      CHECK(ailo_ggml_backend_dev_caps_array_free(value, &error) == AILO_OK);
      ++tested;
    }
    struct ailo_ggml_backend_dev_props *element = nullptr;
    CHECK(ailo_ggml_backend_dev_props_array_at(self, 2, 1, &element, &error) ==
          AILO_OK);
    CHECK(reinterpret_cast<struct ggml_backend_dev_props *>(element) ==
          native + 1);
    CHECK(ailo_ggml_backend_dev_props_array_at(self, 2, 2, &element, &error) ==
              AILO_OUT_OF_RANGE &&
          !element);
    CHECK(ailo_ggml_backend_dev_props_array_new(
              std::numeric_limits<size_t>::max(), &element, &error) ==
              AILO_OUT_OF_RANGE &&
          !element);
    CHECK(ailo_ggml_backend_dev_props_array_free(self, &error) == AILO_OK);
    CHECK(ailo_ggml_backend_dev_props_array_free(nullptr, &error) == AILO_OK);
  }
  {
    struct ailo_ggml_backend_feature *self = nullptr;
    CHECK(ailo_ggml_backend_feature_array_new(2, &self, &error) == AILO_OK &&
          self);
    auto native = reinterpret_cast<struct ggml_backend_feature *>(self);
    (void)native;
    {
      const char *value = reinterpret_cast<const char *>(address);
      const char *result = {};
      CHECK(ailo_ggml_backend_feature_name_set(self, value, &error) == AILO_OK);
      CHECK(ailo_ggml_backend_feature_name_get(self, &result, &error) ==
            AILO_OK);
      CHECK(result == value);
      CHECK(native->name == static_cast<const char *>(value));
      ++tested;
    }
    {
      const char *value = reinterpret_cast<const char *>(address);
      const char *result = {};
      CHECK(ailo_ggml_backend_feature_value_set(self, value, &error) ==
            AILO_OK);
      CHECK(ailo_ggml_backend_feature_value_get(self, &result, &error) ==
            AILO_OK);
      CHECK(result == value);
      CHECK(native->value == static_cast<const char *>(value));
      ++tested;
    }
    struct ailo_ggml_backend_feature *element = nullptr;
    CHECK(ailo_ggml_backend_feature_array_at(self, 2, 1, &element, &error) ==
          AILO_OK);
    CHECK(reinterpret_cast<struct ggml_backend_feature *>(element) ==
          native + 1);
    CHECK(ailo_ggml_backend_feature_array_at(self, 2, 2, &element, &error) ==
              AILO_OUT_OF_RANGE &&
          !element);
    CHECK(ailo_ggml_backend_feature_array_new(
              std::numeric_limits<size_t>::max(), &element, &error) ==
              AILO_OUT_OF_RANGE &&
          !element);
    CHECK(ailo_ggml_backend_feature_array_free(self, &error) == AILO_OK);
    CHECK(ailo_ggml_backend_feature_array_free(nullptr, &error) == AILO_OK);
  }
  {
    struct ailo_ggml_backend_graph_copy *self = nullptr;
    CHECK(ailo_ggml_backend_graph_copy_array_new(2, &self, &error) == AILO_OK &&
          self);
    auto native = reinterpret_cast<struct ggml_backend_graph_copy *>(self);
    (void)native;
    {
      struct ailo_ggml_backend_buffer *value =
          reinterpret_cast<struct ailo_ggml_backend_buffer *>(address);
      struct ailo_ggml_backend_buffer *result = {};
      CHECK(ailo_ggml_backend_graph_copy_buffer_set(self, value, &error) ==
            AILO_OK);
      CHECK(ailo_ggml_backend_graph_copy_buffer_get(self, &result, &error) ==
            AILO_OK);
      CHECK(result == value);
      CHECK(native->buffer ==
            reinterpret_cast<struct ggml_backend_buffer *>(value));
      ++tested;
    }
    {
      struct ailo_ggml_context *value =
          reinterpret_cast<struct ailo_ggml_context *>(address);
      struct ailo_ggml_context *result = {};
      CHECK(ailo_ggml_backend_graph_copy_ctx_allocated_set(self, value,
                                                           &error) == AILO_OK);
      CHECK(ailo_ggml_backend_graph_copy_ctx_allocated_get(self, &result,
                                                           &error) == AILO_OK);
      CHECK(result == value);
      CHECK(native->ctx_allocated ==
            reinterpret_cast<struct ggml_context *>(value));
      ++tested;
    }
    {
      struct ailo_ggml_context *value =
          reinterpret_cast<struct ailo_ggml_context *>(address);
      struct ailo_ggml_context *result = {};
      CHECK(ailo_ggml_backend_graph_copy_ctx_unallocated_set(
                self, value, &error) == AILO_OK);
      CHECK(ailo_ggml_backend_graph_copy_ctx_unallocated_get(
                self, &result, &error) == AILO_OK);
      CHECK(result == value);
      CHECK(native->ctx_unallocated ==
            reinterpret_cast<struct ggml_context *>(value));
      ++tested;
    }
    {
      struct ailo_ggml_cgraph *value =
          reinterpret_cast<struct ailo_ggml_cgraph *>(address);
      struct ailo_ggml_cgraph *result = {};
      CHECK(ailo_ggml_backend_graph_copy_graph_set(self, value, &error) ==
            AILO_OK);
      CHECK(ailo_ggml_backend_graph_copy_graph_get(self, &result, &error) ==
            AILO_OK);
      CHECK(result == value);
      CHECK(native->graph == reinterpret_cast<struct ggml_cgraph *>(value));
      ++tested;
    }
    struct ailo_ggml_backend_graph_copy *element = nullptr;
    CHECK(ailo_ggml_backend_graph_copy_array_at(self, 2, 1, &element, &error) ==
          AILO_OK);
    CHECK(reinterpret_cast<struct ggml_backend_graph_copy *>(element) ==
          native + 1);
    CHECK(ailo_ggml_backend_graph_copy_array_at(self, 2, 2, &element, &error) ==
              AILO_OUT_OF_RANGE &&
          !element);
    CHECK(ailo_ggml_backend_graph_copy_array_new(
              std::numeric_limits<size_t>::max(), &element, &error) ==
              AILO_OUT_OF_RANGE &&
          !element);
    CHECK(ailo_ggml_backend_graph_copy_array_free(self, &error) == AILO_OK);
    CHECK(ailo_ggml_backend_graph_copy_array_free(nullptr, &error) == AILO_OK);
  }
  {
    struct ailo_ggml_backend_meta_split_state *self = nullptr;
    CHECK(ailo_ggml_backend_meta_split_state_array_new(2, &self, &error) ==
              AILO_OK &&
          self);
    auto native =
        reinterpret_cast<struct ggml_backend_meta_split_state *>(self);
    (void)native;
    {
      enum ailo_ggml_backend_meta_split_axis value =
          AILO_GGML_BACKEND_SPLIT_AXIS_0;
      enum ailo_ggml_backend_meta_split_axis result = {};
      CHECK(ailo_ggml_backend_meta_split_state_axis_set(self, value, &error) ==
            AILO_OK);
      CHECK(ailo_ggml_backend_meta_split_state_axis_get(self, &result,
                                                        &error) == AILO_OK);
      CHECK(result == value);
      CHECK(native->axis ==
            static_cast<enum ggml_backend_meta_split_axis>(value));
      ++tested;
    }
    {
      ailo_i64 value = ailo_i64{0x80000000U, 17};
      ailo_i64 result = {};
      CHECK(ailo_ggml_backend_meta_split_state_ne_set(self, 255, value,
                                                      &error) == AILO_OK);
      CHECK(ailo_ggml_backend_meta_split_state_ne_get(self, 255, &result,
                                                      &error) == AILO_OK);
      CHECK(result.hi == value.hi && result.lo == value.lo);
      CHECK(native->ne[255] == ailo::detail::to_native(value));
      CHECK(ailo_ggml_backend_meta_split_state_ne_get(
                self, 256, &result, &error) == AILO_OUT_OF_RANGE);
      ++tested;
    }
    {
      ailo_u32 value = 97;
      ailo_u32 result = {};
      CHECK(ailo_ggml_backend_meta_split_state_nr_set(self, 15, value,
                                                      &error) == AILO_OK);
      CHECK(ailo_ggml_backend_meta_split_state_nr_get(self, 15, &result,
                                                      &error) == AILO_OK);
      CHECK(result == value);
      CHECK(native->nr[15] == static_cast<uint32_t>(value));
      CHECK(ailo_ggml_backend_meta_split_state_nr_get(
                self, 16, &result, &error) == AILO_OUT_OF_RANGE);
      ++tested;
    }
    {
      ailo_u32 value = 97;
      ailo_u32 result = {};
      CHECK(ailo_ggml_backend_meta_split_state_n_segments_set(
                self, value, &error) == AILO_OK);
      CHECK(ailo_ggml_backend_meta_split_state_n_segments_get(
                self, &result, &error) == AILO_OK);
      CHECK(result == value);
      CHECK(native->n_segments == static_cast<uint32_t>(value));
      ++tested;
    }
    struct ailo_ggml_backend_meta_split_state *element = nullptr;
    CHECK(ailo_ggml_backend_meta_split_state_array_at(self, 2, 1, &element,
                                                      &error) == AILO_OK);
    CHECK(reinterpret_cast<struct ggml_backend_meta_split_state *>(element) ==
          native + 1);
    CHECK(ailo_ggml_backend_meta_split_state_array_at(
              self, 2, 2, &element, &error) == AILO_OUT_OF_RANGE &&
          !element);
    CHECK(ailo_ggml_backend_meta_split_state_array_new(
              std::numeric_limits<size_t>::max(), &element, &error) ==
              AILO_OUT_OF_RANGE &&
          !element);
    CHECK(ailo_ggml_backend_meta_split_state_array_free(self, &error) ==
          AILO_OK);
    CHECK(ailo_ggml_backend_meta_split_state_array_free(nullptr, &error) ==
          AILO_OK);
  }
  {
    struct ailo_ggml_cplan *self = nullptr;
    CHECK(ailo_ggml_cplan_array_new(2, &self, &error) == AILO_OK && self);
    auto native = reinterpret_cast<struct ggml_cplan *>(self);
    (void)native;
    {
      size_t value = 97;
      size_t result = {};
      CHECK(ailo_ggml_cplan_work_size_set(self, value, &error) == AILO_OK);
      CHECK(ailo_ggml_cplan_work_size_get(self, &result, &error) == AILO_OK);
      CHECK(result == value);
      CHECK(native->work_size == static_cast<size_t>(value));
      ++tested;
    }
    {
      ailo_u8 *value = reinterpret_cast<ailo_u8 *>(address);
      ailo_u8 *result = {};
      CHECK(ailo_ggml_cplan_work_data_set(self, value, &error) == AILO_OK);
      CHECK(ailo_ggml_cplan_work_data_get(self, &result, &error) == AILO_OK);
      CHECK(result == value);
      CHECK(native->work_data == static_cast<uint8_t *>(value));
      ++tested;
    }
    {
      int value = 97;
      int result = {};
      CHECK(ailo_ggml_cplan_n_threads_set(self, value, &error) == AILO_OK);
      CHECK(ailo_ggml_cplan_n_threads_get(self, &result, &error) == AILO_OK);
      CHECK(result == value);
      CHECK(native->n_threads == static_cast<int>(value));
      ++tested;
    }
    {
      struct ailo_ggml_threadpool *value =
          reinterpret_cast<struct ailo_ggml_threadpool *>(address);
      struct ailo_ggml_threadpool *result = {};
      CHECK(ailo_ggml_cplan_threadpool_set(self, value, &error) == AILO_OK);
      CHECK(ailo_ggml_cplan_threadpool_get(self, &result, &error) == AILO_OK);
      CHECK(result == value);
      CHECK(native->threadpool ==
            reinterpret_cast<struct ggml_threadpool *>(value));
      ++tested;
    }
    {
      void *value = reinterpret_cast<void *>(address);
      void *result = {};
      CHECK(ailo_ggml_cplan_abort_callback_data_set(self, value, &error) ==
            AILO_OK);
      CHECK(ailo_ggml_cplan_abort_callback_data_get(self, &result, &error) ==
            AILO_OK);
      CHECK(result == value);
      CHECK(native->abort_callback_data == static_cast<void *>(value));
      ++tested;
    }
    {
      ailo_bool value = 17;
      ailo_bool result = {};
      CHECK(ailo_ggml_cplan_use_ref_set(self, value, &error) == AILO_OK);
      CHECK(ailo_ggml_cplan_use_ref_get(self, &result, &error) == AILO_OK);
      CHECK(result == 1 && native->use_ref);
      ++tested;
    }
    struct ailo_ggml_cplan *element = nullptr;
    CHECK(ailo_ggml_cplan_array_at(self, 2, 1, &element, &error) == AILO_OK);
    CHECK(reinterpret_cast<struct ggml_cplan *>(element) == native + 1);
    CHECK(ailo_ggml_cplan_array_at(self, 2, 2, &element, &error) ==
              AILO_OUT_OF_RANGE &&
          !element);
    CHECK(ailo_ggml_cplan_array_new(std::numeric_limits<size_t>::max(),
                                    &element, &error) == AILO_OUT_OF_RANGE &&
          !element);
    CHECK(ailo_ggml_cplan_array_free(self, &error) == AILO_OK);
    CHECK(ailo_ggml_cplan_array_free(nullptr, &error) == AILO_OK);
  }
  {
    struct ailo_ggml_init_params *self = nullptr;
    CHECK(ailo_ggml_init_params_array_new(2, &self, &error) == AILO_OK && self);
    auto native = reinterpret_cast<struct ggml_init_params *>(self);
    (void)native;
    {
      size_t value = 97;
      size_t result = {};
      CHECK(ailo_ggml_init_params_mem_size_set(self, value, &error) == AILO_OK);
      CHECK(ailo_ggml_init_params_mem_size_get(self, &result, &error) ==
            AILO_OK);
      CHECK(result == value);
      CHECK(native->mem_size == static_cast<size_t>(value));
      ++tested;
    }
    {
      void *value = reinterpret_cast<void *>(address);
      void *result = {};
      CHECK(ailo_ggml_init_params_mem_buffer_set(self, value, &error) ==
            AILO_OK);
      CHECK(ailo_ggml_init_params_mem_buffer_get(self, &result, &error) ==
            AILO_OK);
      CHECK(result == value);
      CHECK(native->mem_buffer == static_cast<void *>(value));
      ++tested;
    }
    {
      ailo_bool value = 17;
      ailo_bool result = {};
      CHECK(ailo_ggml_init_params_no_alloc_set(self, value, &error) == AILO_OK);
      CHECK(ailo_ggml_init_params_no_alloc_get(self, &result, &error) ==
            AILO_OK);
      CHECK(result == 1 && native->no_alloc);
      ++tested;
    }
    struct ailo_ggml_init_params *element = nullptr;
    CHECK(ailo_ggml_init_params_array_at(self, 2, 1, &element, &error) ==
          AILO_OK);
    CHECK(reinterpret_cast<struct ggml_init_params *>(element) == native + 1);
    CHECK(ailo_ggml_init_params_array_at(self, 2, 2, &element, &error) ==
              AILO_OUT_OF_RANGE &&
          !element);
    CHECK(ailo_ggml_init_params_array_new(std::numeric_limits<size_t>::max(),
                                          &element,
                                          &error) == AILO_OUT_OF_RANGE &&
          !element);
    CHECK(ailo_ggml_init_params_array_free(self, &error) == AILO_OK);
    CHECK(ailo_ggml_init_params_array_free(nullptr, &error) == AILO_OK);
  }
  {
    struct ailo_ggml_opt_optimizer_params *self = nullptr;
    CHECK(ailo_ggml_opt_optimizer_params_array_new(2, &self, &error) ==
              AILO_OK &&
          self);
    auto native = reinterpret_cast<struct ggml_opt_optimizer_params *>(self);
    (void)native;
    {
      float value = 0.25f;
      float result = {};
      CHECK(ailo_ggml_opt_optimizer_params_adamw_alpha_set(self, value,
                                                           &error) == AILO_OK);
      CHECK(ailo_ggml_opt_optimizer_params_adamw_alpha_get(self, &result,
                                                           &error) == AILO_OK);
      CHECK(result == value);
      CHECK(native->adamw.alpha == static_cast<float>(value));
      ++tested;
    }
    {
      float value = 0.25f;
      float result = {};
      CHECK(ailo_ggml_opt_optimizer_params_adamw_beta1_set(self, value,
                                                           &error) == AILO_OK);
      CHECK(ailo_ggml_opt_optimizer_params_adamw_beta1_get(self, &result,
                                                           &error) == AILO_OK);
      CHECK(result == value);
      CHECK(native->adamw.beta1 == static_cast<float>(value));
      ++tested;
    }
    {
      float value = 0.25f;
      float result = {};
      CHECK(ailo_ggml_opt_optimizer_params_adamw_beta2_set(self, value,
                                                           &error) == AILO_OK);
      CHECK(ailo_ggml_opt_optimizer_params_adamw_beta2_get(self, &result,
                                                           &error) == AILO_OK);
      CHECK(result == value);
      CHECK(native->adamw.beta2 == static_cast<float>(value));
      ++tested;
    }
    {
      float value = 0.25f;
      float result = {};
      CHECK(ailo_ggml_opt_optimizer_params_adamw_eps_set(self, value, &error) ==
            AILO_OK);
      CHECK(ailo_ggml_opt_optimizer_params_adamw_eps_get(self, &result,
                                                         &error) == AILO_OK);
      CHECK(result == value);
      CHECK(native->adamw.eps == static_cast<float>(value));
      ++tested;
    }
    {
      float value = 0.25f;
      float result = {};
      CHECK(ailo_ggml_opt_optimizer_params_adamw_wd_set(self, value, &error) ==
            AILO_OK);
      CHECK(ailo_ggml_opt_optimizer_params_adamw_wd_get(self, &result,
                                                        &error) == AILO_OK);
      CHECK(result == value);
      CHECK(native->adamw.wd == static_cast<float>(value));
      ++tested;
    }
    {
      float value = 0.25f;
      float result = {};
      CHECK(ailo_ggml_opt_optimizer_params_sgd_alpha_set(self, value, &error) ==
            AILO_OK);
      CHECK(ailo_ggml_opt_optimizer_params_sgd_alpha_get(self, &result,
                                                         &error) == AILO_OK);
      CHECK(result == value);
      CHECK(native->sgd.alpha == static_cast<float>(value));
      ++tested;
    }
    {
      float value = 0.25f;
      float result = {};
      CHECK(ailo_ggml_opt_optimizer_params_sgd_wd_set(self, value, &error) ==
            AILO_OK);
      CHECK(ailo_ggml_opt_optimizer_params_sgd_wd_get(self, &result, &error) ==
            AILO_OK);
      CHECK(result == value);
      CHECK(native->sgd.wd == static_cast<float>(value));
      ++tested;
    }
    struct ailo_ggml_opt_optimizer_params *element = nullptr;
    CHECK(ailo_ggml_opt_optimizer_params_array_at(self, 2, 1, &element,
                                                  &error) == AILO_OK);
    CHECK(reinterpret_cast<struct ggml_opt_optimizer_params *>(element) ==
          native + 1);
    CHECK(ailo_ggml_opt_optimizer_params_array_at(
              self, 2, 2, &element, &error) == AILO_OUT_OF_RANGE &&
          !element);
    CHECK(ailo_ggml_opt_optimizer_params_array_new(
              std::numeric_limits<size_t>::max(), &element, &error) ==
              AILO_OUT_OF_RANGE &&
          !element);
    CHECK(ailo_ggml_opt_optimizer_params_array_free(self, &error) == AILO_OK);
    CHECK(ailo_ggml_opt_optimizer_params_array_free(nullptr, &error) ==
          AILO_OK);
  }
  {
    struct ailo_ggml_opt_params *self = nullptr;
    CHECK(ailo_ggml_opt_params_array_new(2, &self, &error) == AILO_OK && self);
    auto native = reinterpret_cast<struct ggml_opt_params *>(self);
    (void)native;
    {
      struct ailo_ggml_backend_sched *value =
          reinterpret_cast<struct ailo_ggml_backend_sched *>(address);
      struct ailo_ggml_backend_sched *result = {};
      CHECK(ailo_ggml_opt_params_backend_sched_set(self, value, &error) ==
            AILO_OK);
      CHECK(ailo_ggml_opt_params_backend_sched_get(self, &result, &error) ==
            AILO_OK);
      CHECK(result == value);
      CHECK(native->backend_sched ==
            reinterpret_cast<struct ggml_backend_sched *>(value));
      ++tested;
    }
    {
      struct ailo_ggml_context *value =
          reinterpret_cast<struct ailo_ggml_context *>(address);
      struct ailo_ggml_context *result = {};
      CHECK(ailo_ggml_opt_params_ctx_compute_set(self, value, &error) ==
            AILO_OK);
      CHECK(ailo_ggml_opt_params_ctx_compute_get(self, &result, &error) ==
            AILO_OK);
      CHECK(result == value);
      CHECK(native->ctx_compute ==
            reinterpret_cast<struct ggml_context *>(value));
      ++tested;
    }
    {
      struct ailo_ggml_tensor *value =
          reinterpret_cast<struct ailo_ggml_tensor *>(address);
      struct ailo_ggml_tensor *result = {};
      CHECK(ailo_ggml_opt_params_inputs_set(self, value, &error) == AILO_OK);
      CHECK(ailo_ggml_opt_params_inputs_get(self, &result, &error) == AILO_OK);
      CHECK(result == value);
      CHECK(native->inputs == reinterpret_cast<struct ggml_tensor *>(value));
      ++tested;
    }
    {
      struct ailo_ggml_tensor *value =
          reinterpret_cast<struct ailo_ggml_tensor *>(address);
      struct ailo_ggml_tensor *result = {};
      CHECK(ailo_ggml_opt_params_outputs_set(self, value, &error) == AILO_OK);
      CHECK(ailo_ggml_opt_params_outputs_get(self, &result, &error) == AILO_OK);
      CHECK(result == value);
      CHECK(native->outputs == reinterpret_cast<struct ggml_tensor *>(value));
      ++tested;
    }
    {
      enum ailo_ggml_opt_loss_type value = AILO_GGML_OPT_LOSS_TYPE_MEAN;
      enum ailo_ggml_opt_loss_type result = {};
      CHECK(ailo_ggml_opt_params_loss_type_set(self, value, &error) == AILO_OK);
      CHECK(ailo_ggml_opt_params_loss_type_get(self, &result, &error) ==
            AILO_OK);
      CHECK(result == value);
      CHECK(native->loss_type == static_cast<enum ggml_opt_loss_type>(value));
      ++tested;
    }
    {
      enum ailo_ggml_opt_build_type value = AILO_GGML_OPT_BUILD_TYPE_FORWARD;
      enum ailo_ggml_opt_build_type result = {};
      CHECK(ailo_ggml_opt_params_build_type_set(self, value, &error) ==
            AILO_OK);
      CHECK(ailo_ggml_opt_params_build_type_get(self, &result, &error) ==
            AILO_OK);
      CHECK(result == value);
      CHECK(native->build_type == static_cast<enum ggml_opt_build_type>(value));
      ++tested;
    }
    {
      ailo_i32 value = 97;
      ailo_i32 result = {};
      CHECK(ailo_ggml_opt_params_opt_period_set(self, value, &error) ==
            AILO_OK);
      CHECK(ailo_ggml_opt_params_opt_period_get(self, &result, &error) ==
            AILO_OK);
      CHECK(result == value);
      CHECK(native->opt_period == static_cast<int32_t>(value));
      ++tested;
    }
    {
      void *value = reinterpret_cast<void *>(address);
      void *result = {};
      CHECK(ailo_ggml_opt_params_get_opt_pars_ud_set(self, value, &error) ==
            AILO_OK);
      CHECK(ailo_ggml_opt_params_get_opt_pars_ud_get(self, &result, &error) ==
            AILO_OK);
      CHECK(result == value);
      CHECK(native->get_opt_pars_ud == static_cast<void *>(value));
      ++tested;
    }
    {
      enum ailo_ggml_opt_optimizer_type value =
          AILO_GGML_OPT_OPTIMIZER_TYPE_ADAMW;
      enum ailo_ggml_opt_optimizer_type result = {};
      CHECK(ailo_ggml_opt_params_optimizer_set(self, value, &error) == AILO_OK);
      CHECK(ailo_ggml_opt_params_optimizer_get(self, &result, &error) ==
            AILO_OK);
      CHECK(result == value);
      CHECK(native->optimizer ==
            static_cast<enum ggml_opt_optimizer_type>(value));
      ++tested;
    }
    struct ailo_ggml_opt_params *element = nullptr;
    CHECK(ailo_ggml_opt_params_array_at(self, 2, 1, &element, &error) ==
          AILO_OK);
    CHECK(reinterpret_cast<struct ggml_opt_params *>(element) == native + 1);
    CHECK(ailo_ggml_opt_params_array_at(self, 2, 2, &element, &error) ==
              AILO_OUT_OF_RANGE &&
          !element);
    CHECK(ailo_ggml_opt_params_array_new(std::numeric_limits<size_t>::max(),
                                         &element,
                                         &error) == AILO_OUT_OF_RANGE &&
          !element);
    CHECK(ailo_ggml_opt_params_array_free(self, &error) == AILO_OK);
    CHECK(ailo_ggml_opt_params_array_free(nullptr, &error) == AILO_OK);
  }
  {
    struct ailo_ggml_tallocr *self = nullptr;
    CHECK(ailo_ggml_tallocr_array_new(2, &self, &error) == AILO_OK && self);
    auto native = reinterpret_cast<struct ggml_tallocr *>(self);
    (void)native;
    {
      struct ailo_ggml_backend_buffer *value =
          reinterpret_cast<struct ailo_ggml_backend_buffer *>(address);
      struct ailo_ggml_backend_buffer *result = {};
      CHECK(ailo_ggml_tallocr_buffer_set(self, value, &error) == AILO_OK);
      CHECK(ailo_ggml_tallocr_buffer_get(self, &result, &error) == AILO_OK);
      CHECK(result == value);
      CHECK(native->buffer ==
            reinterpret_cast<struct ggml_backend_buffer *>(value));
      ++tested;
    }
    {
      void *value = reinterpret_cast<void *>(address);
      void *result = {};
      CHECK(ailo_ggml_tallocr_base_set(self, value, &error) == AILO_OK);
      CHECK(ailo_ggml_tallocr_base_get(self, &result, &error) == AILO_OK);
      CHECK(result == value);
      CHECK(native->base == static_cast<void *>(value));
      ++tested;
    }
    {
      size_t value = 97;
      size_t result = {};
      CHECK(ailo_ggml_tallocr_alignment_set(self, value, &error) == AILO_OK);
      CHECK(ailo_ggml_tallocr_alignment_get(self, &result, &error) == AILO_OK);
      CHECK(result == value);
      CHECK(native->alignment == static_cast<size_t>(value));
      ++tested;
    }
    {
      size_t value = 97;
      size_t result = {};
      CHECK(ailo_ggml_tallocr_offset_set(self, value, &error) == AILO_OK);
      CHECK(ailo_ggml_tallocr_offset_get(self, &result, &error) == AILO_OK);
      CHECK(result == value);
      CHECK(native->offset == static_cast<size_t>(value));
      ++tested;
    }
    struct ailo_ggml_tallocr *element = nullptr;
    CHECK(ailo_ggml_tallocr_array_at(self, 2, 1, &element, &error) == AILO_OK);
    CHECK(reinterpret_cast<struct ggml_tallocr *>(element) == native + 1);
    CHECK(ailo_ggml_tallocr_array_at(self, 2, 2, &element, &error) ==
              AILO_OUT_OF_RANGE &&
          !element);
    CHECK(ailo_ggml_tallocr_array_new(std::numeric_limits<size_t>::max(),
                                      &element, &error) == AILO_OUT_OF_RANGE &&
          !element);
    CHECK(ailo_ggml_tallocr_array_free(self, &error) == AILO_OK);
    CHECK(ailo_ggml_tallocr_array_free(nullptr, &error) == AILO_OK);
  }
  {
    struct ailo_ggml_tensor *self = nullptr;
    CHECK(ailo_ggml_tensor_array_new(2, &self, &error) == AILO_OK && self);
    auto native = reinterpret_cast<struct ggml_tensor *>(self);
    (void)native;
    {
      enum ailo_ggml_type value = AILO_GGML_TYPE_F32;
      enum ailo_ggml_type result = {};
      CHECK(ailo_ggml_tensor_type_set(self, value, &error) == AILO_OK);
      CHECK(ailo_ggml_tensor_type_get(self, &result, &error) == AILO_OK);
      CHECK(result == value);
      CHECK(native->type == static_cast<enum ggml_type>(value));
      ++tested;
    }
    {
      struct ailo_ggml_backend_buffer *value =
          reinterpret_cast<struct ailo_ggml_backend_buffer *>(address);
      struct ailo_ggml_backend_buffer *result = {};
      CHECK(ailo_ggml_tensor_buffer_set(self, value, &error) == AILO_OK);
      CHECK(ailo_ggml_tensor_buffer_get(self, &result, &error) == AILO_OK);
      CHECK(result == value);
      CHECK(native->buffer ==
            reinterpret_cast<struct ggml_backend_buffer *>(value));
      ++tested;
    }
    {
      ailo_i64 value = ailo_i64{0x80000000U, 17};
      ailo_i64 result = {};
      CHECK(ailo_ggml_tensor_ne_set(self, 3, value, &error) == AILO_OK);
      CHECK(ailo_ggml_tensor_ne_get(self, 3, &result, &error) == AILO_OK);
      CHECK(result.hi == value.hi && result.lo == value.lo);
      CHECK(native->ne[3] == ailo::detail::to_native(value));
      CHECK(ailo_ggml_tensor_ne_get(self, 4, &result, &error) ==
            AILO_OUT_OF_RANGE);
      ++tested;
    }
    {
      size_t value = 97;
      size_t result = {};
      CHECK(ailo_ggml_tensor_nb_set(self, 3, value, &error) == AILO_OK);
      CHECK(ailo_ggml_tensor_nb_get(self, 3, &result, &error) == AILO_OK);
      CHECK(result == value);
      CHECK(native->nb[3] == static_cast<size_t>(value));
      CHECK(ailo_ggml_tensor_nb_get(self, 4, &result, &error) ==
            AILO_OUT_OF_RANGE);
      ++tested;
    }
    {
      enum ailo_ggml_op value = AILO_GGML_OP_NONE;
      enum ailo_ggml_op result = {};
      CHECK(ailo_ggml_tensor_op_set(self, value, &error) == AILO_OK);
      CHECK(ailo_ggml_tensor_op_get(self, &result, &error) == AILO_OK);
      CHECK(result == value);
      CHECK(native->op == static_cast<enum ggml_op>(value));
      ++tested;
    }
    {
      ailo_i32 value = 97;
      ailo_i32 result = {};
      CHECK(ailo_ggml_tensor_op_params_set(self, 15, value, &error) == AILO_OK);
      CHECK(ailo_ggml_tensor_op_params_get(self, 15, &result, &error) ==
            AILO_OK);
      CHECK(result == value);
      CHECK(native->op_params[15] == static_cast<int32_t>(value));
      CHECK(ailo_ggml_tensor_op_params_get(self, 16, &result, &error) ==
            AILO_OUT_OF_RANGE);
      ++tested;
    }
    {
      ailo_i32 value = 97;
      ailo_i32 result = {};
      CHECK(ailo_ggml_tensor_flags_set(self, value, &error) == AILO_OK);
      CHECK(ailo_ggml_tensor_flags_get(self, &result, &error) == AILO_OK);
      CHECK(result == value);
      CHECK(native->flags == static_cast<int32_t>(value));
      ++tested;
    }
    {
      struct ailo_ggml_tensor *value =
          reinterpret_cast<struct ailo_ggml_tensor *>(address);
      struct ailo_ggml_tensor *result = {};
      CHECK(ailo_ggml_tensor_src_set(self, 9, value, &error) == AILO_OK);
      CHECK(ailo_ggml_tensor_src_get(self, 9, &result, &error) == AILO_OK);
      CHECK(result == value);
      CHECK(native->src[9] == reinterpret_cast<struct ggml_tensor *>(value));
      CHECK(ailo_ggml_tensor_src_get(self, 10, &result, &error) ==
            AILO_OUT_OF_RANGE);
      ++tested;
    }
    {
      struct ailo_ggml_tensor *value =
          reinterpret_cast<struct ailo_ggml_tensor *>(address);
      struct ailo_ggml_tensor *result = {};
      CHECK(ailo_ggml_tensor_view_src_set(self, value, &error) == AILO_OK);
      CHECK(ailo_ggml_tensor_view_src_get(self, &result, &error) == AILO_OK);
      CHECK(result == value);
      CHECK(native->view_src == reinterpret_cast<struct ggml_tensor *>(value));
      ++tested;
    }
    {
      size_t value = 97;
      size_t result = {};
      CHECK(ailo_ggml_tensor_view_offs_set(self, value, &error) == AILO_OK);
      CHECK(ailo_ggml_tensor_view_offs_get(self, &result, &error) == AILO_OK);
      CHECK(result == value);
      CHECK(native->view_offs == static_cast<size_t>(value));
      ++tested;
    }
    {
      void *value = reinterpret_cast<void *>(address);
      void *result = {};
      CHECK(ailo_ggml_tensor_data_set(self, value, &error) == AILO_OK);
      CHECK(ailo_ggml_tensor_data_get(self, &result, &error) == AILO_OK);
      CHECK(result == value);
      CHECK(native->data == static_cast<void *>(value));
      ++tested;
    }
    {
      char value = 97;
      char result = {};
      CHECK(ailo_ggml_tensor_name_set(self, 63, value, &error) == AILO_OK);
      CHECK(ailo_ggml_tensor_name_get(self, 63, &result, &error) == AILO_OK);
      CHECK(result == value);
      CHECK(native->name[63] == static_cast<char>(value));
      CHECK(ailo_ggml_tensor_name_get(self, 64, &result, &error) ==
            AILO_OUT_OF_RANGE);
      ++tested;
    }
    {
      void *value = reinterpret_cast<void *>(address);
      void *result = {};
      CHECK(ailo_ggml_tensor_extra_set(self, value, &error) == AILO_OK);
      CHECK(ailo_ggml_tensor_extra_get(self, &result, &error) == AILO_OK);
      CHECK(result == value);
      CHECK(native->extra == static_cast<void *>(value));
      ++tested;
    }
    {
      char value = 97;
      char result = {};
      CHECK(ailo_ggml_tensor_padding_set(self, 7, value, &error) == AILO_OK);
      CHECK(ailo_ggml_tensor_padding_get(self, 7, &result, &error) == AILO_OK);
      CHECK(result == value);
      CHECK(native->padding[7] == static_cast<char>(value));
      CHECK(ailo_ggml_tensor_padding_get(self, 8, &result, &error) ==
            AILO_OUT_OF_RANGE);
      ++tested;
    }
    struct ailo_ggml_tensor *element = nullptr;
    CHECK(ailo_ggml_tensor_array_at(self, 2, 1, &element, &error) == AILO_OK);
    CHECK(reinterpret_cast<struct ggml_tensor *>(element) == native + 1);
    CHECK(ailo_ggml_tensor_array_at(self, 2, 2, &element, &error) ==
              AILO_OUT_OF_RANGE &&
          !element);
    CHECK(ailo_ggml_tensor_array_new(std::numeric_limits<size_t>::max(),
                                     &element, &error) == AILO_OUT_OF_RANGE &&
          !element);
    CHECK(ailo_ggml_tensor_array_free(self, &error) == AILO_OK);
    CHECK(ailo_ggml_tensor_array_free(nullptr, &error) == AILO_OK);
  }
  {
    struct ailo_ggml_threadpool_params *self = nullptr;
    CHECK(ailo_ggml_threadpool_params_array_new(2, &self, &error) == AILO_OK &&
          self);
    auto native = reinterpret_cast<struct ggml_threadpool_params *>(self);
    (void)native;
    {
      ailo_bool value = 17;
      ailo_bool result = {};
      CHECK(ailo_ggml_threadpool_params_cpumask_set(self, 511, value, &error) ==
            AILO_OK);
      CHECK(ailo_ggml_threadpool_params_cpumask_get(self, 511, &result,
                                                    &error) == AILO_OK);
      CHECK(result == 1 && native->cpumask[511]);
      CHECK(ailo_ggml_threadpool_params_cpumask_get(
                self, 512, &result, &error) == AILO_OUT_OF_RANGE);
      ++tested;
    }
    {
      int value = 97;
      int result = {};
      CHECK(ailo_ggml_threadpool_params_n_threads_set(self, value, &error) ==
            AILO_OK);
      CHECK(ailo_ggml_threadpool_params_n_threads_get(self, &result, &error) ==
            AILO_OK);
      CHECK(result == value);
      CHECK(native->n_threads == static_cast<int>(value));
      ++tested;
    }
    {
      enum ailo_ggml_sched_priority value = AILO_GGML_SCHED_PRIO_LOW;
      enum ailo_ggml_sched_priority result = {};
      CHECK(ailo_ggml_threadpool_params_prio_set(self, value, &error) ==
            AILO_OK);
      CHECK(ailo_ggml_threadpool_params_prio_get(self, &result, &error) ==
            AILO_OK);
      CHECK(result == value);
      CHECK(native->prio == static_cast<enum ggml_sched_priority>(value));
      ++tested;
    }
    {
      ailo_u32 value = 97;
      ailo_u32 result = {};
      CHECK(ailo_ggml_threadpool_params_poll_set(self, value, &error) ==
            AILO_OK);
      CHECK(ailo_ggml_threadpool_params_poll_get(self, &result, &error) ==
            AILO_OK);
      CHECK(result == value);
      CHECK(native->poll == static_cast<uint32_t>(value));
      ++tested;
    }
    {
      ailo_bool value = 17;
      ailo_bool result = {};
      CHECK(ailo_ggml_threadpool_params_strict_cpu_set(self, value, &error) ==
            AILO_OK);
      CHECK(ailo_ggml_threadpool_params_strict_cpu_get(self, &result, &error) ==
            AILO_OK);
      CHECK(result == 1 && native->strict_cpu);
      ++tested;
    }
    {
      ailo_bool value = 17;
      ailo_bool result = {};
      CHECK(ailo_ggml_threadpool_params_paused_set(self, value, &error) ==
            AILO_OK);
      CHECK(ailo_ggml_threadpool_params_paused_get(self, &result, &error) ==
            AILO_OK);
      CHECK(result == 1 && native->paused);
      ++tested;
    }
    struct ailo_ggml_threadpool_params *element = nullptr;
    CHECK(ailo_ggml_threadpool_params_array_at(self, 2, 1, &element, &error) ==
          AILO_OK);
    CHECK(reinterpret_cast<struct ggml_threadpool_params *>(element) ==
          native + 1);
    CHECK(ailo_ggml_threadpool_params_array_at(self, 2, 2, &element, &error) ==
              AILO_OUT_OF_RANGE &&
          !element);
    CHECK(ailo_ggml_threadpool_params_array_new(
              std::numeric_limits<size_t>::max(), &element, &error) ==
              AILO_OUT_OF_RANGE &&
          !element);
    CHECK(ailo_ggml_threadpool_params_array_free(self, &error) == AILO_OK);
    CHECK(ailo_ggml_threadpool_params_array_free(nullptr, &error) == AILO_OK);
  }
  {
    struct ailo_ggml_type_traits *self = nullptr;
    CHECK(ailo_ggml_type_traits_array_new(2, &self, &error) == AILO_OK && self);
    auto native = reinterpret_cast<struct ggml_type_traits *>(self);
    (void)native;
    {
      const char *value = reinterpret_cast<const char *>(address);
      const char *result = {};
      CHECK(ailo_ggml_type_traits_type_name_set(self, value, &error) ==
            AILO_OK);
      CHECK(ailo_ggml_type_traits_type_name_get(self, &result, &error) ==
            AILO_OK);
      CHECK(result == value);
      CHECK(native->type_name == static_cast<const char *>(value));
      ++tested;
    }
    {
      ailo_i64 value = ailo_i64{0x80000000U, 17};
      ailo_i64 result = {};
      CHECK(ailo_ggml_type_traits_blck_size_set(self, value, &error) ==
            AILO_OK);
      CHECK(ailo_ggml_type_traits_blck_size_get(self, &result, &error) ==
            AILO_OK);
      CHECK(result.hi == value.hi && result.lo == value.lo);
      CHECK(native->blck_size == ailo::detail::to_native(value));
      ++tested;
    }
    {
      ailo_i64 value = ailo_i64{0x80000000U, 17};
      ailo_i64 result = {};
      CHECK(ailo_ggml_type_traits_blck_size_interleave_set(self, value,
                                                           &error) == AILO_OK);
      CHECK(ailo_ggml_type_traits_blck_size_interleave_get(self, &result,
                                                           &error) == AILO_OK);
      CHECK(result.hi == value.hi && result.lo == value.lo);
      CHECK(native->blck_size_interleave == ailo::detail::to_native(value));
      ++tested;
    }
    {
      size_t value = 97;
      size_t result = {};
      CHECK(ailo_ggml_type_traits_type_size_set(self, value, &error) ==
            AILO_OK);
      CHECK(ailo_ggml_type_traits_type_size_get(self, &result, &error) ==
            AILO_OK);
      CHECK(result == value);
      CHECK(native->type_size == static_cast<size_t>(value));
      ++tested;
    }
    {
      ailo_bool value = 17;
      ailo_bool result = {};
      CHECK(ailo_ggml_type_traits_is_quantized_set(self, value, &error) ==
            AILO_OK);
      CHECK(ailo_ggml_type_traits_is_quantized_get(self, &result, &error) ==
            AILO_OK);
      CHECK(result == 1 && native->is_quantized);
      ++tested;
    }
    struct ailo_ggml_type_traits *element = nullptr;
    CHECK(ailo_ggml_type_traits_array_at(self, 2, 1, &element, &error) ==
          AILO_OK);
    CHECK(reinterpret_cast<struct ggml_type_traits *>(element) == native + 1);
    CHECK(ailo_ggml_type_traits_array_at(self, 2, 2, &element, &error) ==
              AILO_OUT_OF_RANGE &&
          !element);
    CHECK(ailo_ggml_type_traits_array_new(std::numeric_limits<size_t>::max(),
                                          &element,
                                          &error) == AILO_OUT_OF_RANGE &&
          !element);
    CHECK(ailo_ggml_type_traits_array_free(self, &error) == AILO_OK);
    CHECK(ailo_ggml_type_traits_array_free(nullptr, &error) == AILO_OK);
  }
  {
    struct ailo_ggml_type_traits_cpu *self = nullptr;
    CHECK(ailo_ggml_type_traits_cpu_array_new(2, &self, &error) == AILO_OK &&
          self);
    auto native = reinterpret_cast<struct ggml_type_traits_cpu *>(self);
    (void)native;
    {
      enum ailo_ggml_type value = AILO_GGML_TYPE_F32;
      enum ailo_ggml_type result = {};
      CHECK(ailo_ggml_type_traits_cpu_vec_dot_type_set(self, value, &error) ==
            AILO_OK);
      CHECK(ailo_ggml_type_traits_cpu_vec_dot_type_get(self, &result, &error) ==
            AILO_OK);
      CHECK(result == value);
      CHECK(native->vec_dot_type == static_cast<enum ggml_type>(value));
      ++tested;
    }
    {
      ailo_i64 value = ailo_i64{0x80000000U, 17};
      ailo_i64 result = {};
      CHECK(ailo_ggml_type_traits_cpu_nrows_set(self, value, &error) ==
            AILO_OK);
      CHECK(ailo_ggml_type_traits_cpu_nrows_get(self, &result, &error) ==
            AILO_OK);
      CHECK(result.hi == value.hi && result.lo == value.lo);
      CHECK(native->nrows == ailo::detail::to_native(value));
      ++tested;
    }
    struct ailo_ggml_type_traits_cpu *element = nullptr;
    CHECK(ailo_ggml_type_traits_cpu_array_at(self, 2, 1, &element, &error) ==
          AILO_OK);
    CHECK(reinterpret_cast<struct ggml_type_traits_cpu *>(element) ==
          native + 1);
    CHECK(ailo_ggml_type_traits_cpu_array_at(self, 2, 2, &element, &error) ==
              AILO_OUT_OF_RANGE &&
          !element);
    CHECK(ailo_ggml_type_traits_cpu_array_new(
              std::numeric_limits<size_t>::max(), &element, &error) ==
              AILO_OUT_OF_RANGE &&
          !element);
    CHECK(ailo_ggml_type_traits_cpu_array_free(self, &error) == AILO_OK);
    CHECK(ailo_ggml_type_traits_cpu_array_free(nullptr, &error) == AILO_OK);
  }
  {
    struct ailo_gguf_init_params *self = nullptr;
    CHECK(ailo_gguf_init_params_array_new(2, &self, &error) == AILO_OK && self);
    auto native = reinterpret_cast<struct gguf_init_params *>(self);
    (void)native;
    {
      ailo_bool value = 17;
      ailo_bool result = {};
      CHECK(ailo_gguf_init_params_no_alloc_set(self, value, &error) == AILO_OK);
      CHECK(ailo_gguf_init_params_no_alloc_get(self, &result, &error) ==
            AILO_OK);
      CHECK(result == 1 && native->no_alloc);
      ++tested;
    }
    {
      struct ailo_storage_struct_ggml_context *value =
          reinterpret_cast<struct ailo_storage_struct_ggml_context *>(address);
      struct ailo_storage_struct_ggml_context *result = {};
      CHECK(ailo_gguf_init_params_ctx_set(self, value, &error) == AILO_OK);
      CHECK(ailo_gguf_init_params_ctx_get(self, &result, &error) == AILO_OK);
      CHECK(result == value);
      CHECK(native->ctx == reinterpret_cast<struct ggml_context **>(value));
      ++tested;
    }
    struct ailo_gguf_init_params *element = nullptr;
    CHECK(ailo_gguf_init_params_array_at(self, 2, 1, &element, &error) ==
          AILO_OK);
    CHECK(reinterpret_cast<struct gguf_init_params *>(element) == native + 1);
    CHECK(ailo_gguf_init_params_array_at(self, 2, 2, &element, &error) ==
              AILO_OUT_OF_RANGE &&
          !element);
    CHECK(ailo_gguf_init_params_array_new(std::numeric_limits<size_t>::max(),
                                          &element,
                                          &error) == AILO_OUT_OF_RANGE &&
          !element);
    CHECK(ailo_gguf_init_params_array_free(self, &error) == AILO_OK);
    CHECK(ailo_gguf_init_params_array_free(nullptr, &error) == AILO_OK);
  }
  {
    struct ailo_llama_batch *self = nullptr;
    CHECK(ailo_llama_batch_array_new(2, &self, &error) == AILO_OK && self);
    auto native = reinterpret_cast<struct llama_batch *>(self);
    (void)native;
    {
      ailo_i32 value = 97;
      ailo_i32 result = {};
      CHECK(ailo_llama_batch_n_tokens_set(self, value, &error) == AILO_OK);
      CHECK(ailo_llama_batch_n_tokens_get(self, &result, &error) == AILO_OK);
      CHECK(result == value);
      CHECK(native->n_tokens == static_cast<int32_t>(value));
      ++tested;
    }
    {
      ailo_i32 *value = reinterpret_cast<ailo_i32 *>(address);
      ailo_i32 *result = {};
      CHECK(ailo_llama_batch_token_set(self, value, &error) == AILO_OK);
      CHECK(ailo_llama_batch_token_get(self, &result, &error) == AILO_OK);
      CHECK(result == value);
      CHECK(native->token == static_cast<int32_t *>(value));
      ++tested;
    }
    {
      float *value = reinterpret_cast<float *>(address);
      float *result = {};
      CHECK(ailo_llama_batch_embd_set(self, value, &error) == AILO_OK);
      CHECK(ailo_llama_batch_embd_get(self, &result, &error) == AILO_OK);
      CHECK(result == value);
      CHECK(native->embd == static_cast<float *>(value));
      ++tested;
    }
    {
      ailo_i32 *value = reinterpret_cast<ailo_i32 *>(address);
      ailo_i32 *result = {};
      CHECK(ailo_llama_batch_pos_set(self, value, &error) == AILO_OK);
      CHECK(ailo_llama_batch_pos_get(self, &result, &error) == AILO_OK);
      CHECK(result == value);
      CHECK(native->pos == static_cast<int32_t *>(value));
      ++tested;
    }
    {
      ailo_i32 *value = reinterpret_cast<ailo_i32 *>(address);
      ailo_i32 *result = {};
      CHECK(ailo_llama_batch_n_seq_id_set(self, value, &error) == AILO_OK);
      CHECK(ailo_llama_batch_n_seq_id_get(self, &result, &error) == AILO_OK);
      CHECK(result == value);
      CHECK(native->n_seq_id == static_cast<int32_t *>(value));
      ++tested;
    }
    {
      ailo_i32 **value = reinterpret_cast<ailo_i32 **>(address);
      ailo_i32 **result = {};
      CHECK(ailo_llama_batch_seq_id_set(self, value, &error) == AILO_OK);
      CHECK(ailo_llama_batch_seq_id_get(self, &result, &error) == AILO_OK);
      CHECK(result == value);
      CHECK(native->seq_id == static_cast<int32_t **>(value));
      ++tested;
    }
    {
      ailo_i8 *value = reinterpret_cast<ailo_i8 *>(address);
      ailo_i8 *result = {};
      CHECK(ailo_llama_batch_logits_set(self, value, &error) == AILO_OK);
      CHECK(ailo_llama_batch_logits_get(self, &result, &error) == AILO_OK);
      CHECK(result == value);
      CHECK(native->logits == static_cast<int8_t *>(value));
      ++tested;
    }
    struct ailo_llama_batch *element = nullptr;
    CHECK(ailo_llama_batch_array_at(self, 2, 1, &element, &error) == AILO_OK);
    CHECK(reinterpret_cast<struct llama_batch *>(element) == native + 1);
    CHECK(ailo_llama_batch_array_at(self, 2, 2, &element, &error) ==
              AILO_OUT_OF_RANGE &&
          !element);
    CHECK(ailo_llama_batch_array_new(std::numeric_limits<size_t>::max(),
                                     &element, &error) == AILO_OUT_OF_RANGE &&
          !element);
    CHECK(ailo_llama_batch_array_free(self, &error) == AILO_OK);
    CHECK(ailo_llama_batch_array_free(nullptr, &error) == AILO_OK);
  }
  {
    struct ailo_llama_chat_message *self = nullptr;
    CHECK(ailo_llama_chat_message_array_new(2, &self, &error) == AILO_OK &&
          self);
    auto native = reinterpret_cast<struct llama_chat_message *>(self);
    (void)native;
    {
      const char *value = reinterpret_cast<const char *>(address);
      const char *result = {};
      CHECK(ailo_llama_chat_message_role_set(self, value, &error) == AILO_OK);
      CHECK(ailo_llama_chat_message_role_get(self, &result, &error) == AILO_OK);
      CHECK(result == value);
      CHECK(native->role == static_cast<const char *>(value));
      ++tested;
    }
    {
      const char *value = reinterpret_cast<const char *>(address);
      const char *result = {};
      CHECK(ailo_llama_chat_message_content_set(self, value, &error) ==
            AILO_OK);
      CHECK(ailo_llama_chat_message_content_get(self, &result, &error) ==
            AILO_OK);
      CHECK(result == value);
      CHECK(native->content == static_cast<const char *>(value));
      ++tested;
    }
    struct ailo_llama_chat_message *element = nullptr;
    CHECK(ailo_llama_chat_message_array_at(self, 2, 1, &element, &error) ==
          AILO_OK);
    CHECK(reinterpret_cast<struct llama_chat_message *>(element) == native + 1);
    CHECK(ailo_llama_chat_message_array_at(self, 2, 2, &element, &error) ==
              AILO_OUT_OF_RANGE &&
          !element);
    CHECK(ailo_llama_chat_message_array_new(std::numeric_limits<size_t>::max(),
                                            &element,
                                            &error) == AILO_OUT_OF_RANGE &&
          !element);
    CHECK(ailo_llama_chat_message_array_free(self, &error) == AILO_OK);
    CHECK(ailo_llama_chat_message_array_free(nullptr, &error) == AILO_OK);
  }
  {
    struct ailo_llama_context_params *self = nullptr;
    CHECK(ailo_llama_context_params_array_new(2, &self, &error) == AILO_OK &&
          self);
    auto native = reinterpret_cast<struct llama_context_params *>(self);
    (void)native;
    {
      ailo_u32 value = 97;
      ailo_u32 result = {};
      CHECK(ailo_llama_context_params_n_ctx_set(self, value, &error) ==
            AILO_OK);
      CHECK(ailo_llama_context_params_n_ctx_get(self, &result, &error) ==
            AILO_OK);
      CHECK(result == value);
      CHECK(native->n_ctx == static_cast<uint32_t>(value));
      ++tested;
    }
    {
      ailo_u32 value = 97;
      ailo_u32 result = {};
      CHECK(ailo_llama_context_params_n_batch_set(self, value, &error) ==
            AILO_OK);
      CHECK(ailo_llama_context_params_n_batch_get(self, &result, &error) ==
            AILO_OK);
      CHECK(result == value);
      CHECK(native->n_batch == static_cast<uint32_t>(value));
      ++tested;
    }
    {
      ailo_u32 value = 97;
      ailo_u32 result = {};
      CHECK(ailo_llama_context_params_n_ubatch_set(self, value, &error) ==
            AILO_OK);
      CHECK(ailo_llama_context_params_n_ubatch_get(self, &result, &error) ==
            AILO_OK);
      CHECK(result == value);
      CHECK(native->n_ubatch == static_cast<uint32_t>(value));
      ++tested;
    }
    {
      ailo_u32 value = 97;
      ailo_u32 result = {};
      CHECK(ailo_llama_context_params_n_seq_max_set(self, value, &error) ==
            AILO_OK);
      CHECK(ailo_llama_context_params_n_seq_max_get(self, &result, &error) ==
            AILO_OK);
      CHECK(result == value);
      CHECK(native->n_seq_max == static_cast<uint32_t>(value));
      ++tested;
    }
    {
      ailo_u32 value = 97;
      ailo_u32 result = {};
      CHECK(ailo_llama_context_params_n_rs_seq_set(self, value, &error) ==
            AILO_OK);
      CHECK(ailo_llama_context_params_n_rs_seq_get(self, &result, &error) ==
            AILO_OK);
      CHECK(result == value);
      CHECK(native->n_rs_seq == static_cast<uint32_t>(value));
      ++tested;
    }
    {
      ailo_u32 value = 97;
      ailo_u32 result = {};
      CHECK(ailo_llama_context_params_n_outputs_max_set(self, value, &error) ==
            AILO_OK);
      CHECK(ailo_llama_context_params_n_outputs_max_get(self, &result,
                                                        &error) == AILO_OK);
      CHECK(result == value);
      CHECK(native->n_outputs_max == static_cast<uint32_t>(value));
      ++tested;
    }
    {
      ailo_u32 value = 97;
      ailo_u32 result = {};
      CHECK(ailo_llama_context_params_n_outputs_max_per_seq_set(
                self, value, &error) == AILO_OK);
      CHECK(ailo_llama_context_params_n_outputs_max_per_seq_get(
                self, &result, &error) == AILO_OK);
      CHECK(result == value);
      CHECK(native->n_outputs_max_per_seq == static_cast<uint32_t>(value));
      ++tested;
    }
    {
      ailo_i32 value = 97;
      ailo_i32 result = {};
      CHECK(ailo_llama_context_params_n_threads_set(self, value, &error) ==
            AILO_OK);
      CHECK(ailo_llama_context_params_n_threads_get(self, &result, &error) ==
            AILO_OK);
      CHECK(result == value);
      CHECK(native->n_threads == static_cast<int32_t>(value));
      ++tested;
    }
    {
      ailo_i32 value = 97;
      ailo_i32 result = {};
      CHECK(ailo_llama_context_params_n_threads_batch_set(self, value,
                                                          &error) == AILO_OK);
      CHECK(ailo_llama_context_params_n_threads_batch_get(self, &result,
                                                          &error) == AILO_OK);
      CHECK(result == value);
      CHECK(native->n_threads_batch == static_cast<int32_t>(value));
      ++tested;
    }
    {
      enum ailo_llama_context_type value = AILO_LLAMA_CONTEXT_TYPE_DEFAULT;
      enum ailo_llama_context_type result = {};
      CHECK(ailo_llama_context_params_ctx_type_set(self, value, &error) ==
            AILO_OK);
      CHECK(ailo_llama_context_params_ctx_type_get(self, &result, &error) ==
            AILO_OK);
      CHECK(result == value);
      CHECK(native->ctx_type == static_cast<enum llama_context_type>(value));
      ++tested;
    }
    {
      enum ailo_llama_rope_scaling_type value =
          AILO_LLAMA_ROPE_SCALING_TYPE_UNSPECIFIED;
      enum ailo_llama_rope_scaling_type result = {};
      CHECK(ailo_llama_context_params_rope_scaling_type_set(self, value,
                                                            &error) == AILO_OK);
      CHECK(ailo_llama_context_params_rope_scaling_type_get(self, &result,
                                                            &error) == AILO_OK);
      CHECK(result == value);
      CHECK(native->rope_scaling_type ==
            static_cast<enum llama_rope_scaling_type>(value));
      ++tested;
    }
    {
      enum ailo_llama_pooling_type value = AILO_LLAMA_POOLING_TYPE_UNSPECIFIED;
      enum ailo_llama_pooling_type result = {};
      CHECK(ailo_llama_context_params_pooling_type_set(self, value, &error) ==
            AILO_OK);
      CHECK(ailo_llama_context_params_pooling_type_get(self, &result, &error) ==
            AILO_OK);
      CHECK(result == value);
      CHECK(native->pooling_type ==
            static_cast<enum llama_pooling_type>(value));
      ++tested;
    }
    {
      enum ailo_llama_attention_type value =
          AILO_LLAMA_ATTENTION_TYPE_UNSPECIFIED;
      enum ailo_llama_attention_type result = {};
      CHECK(ailo_llama_context_params_attention_type_set(self, value, &error) ==
            AILO_OK);
      CHECK(ailo_llama_context_params_attention_type_get(self, &result,
                                                         &error) == AILO_OK);
      CHECK(result == value);
      CHECK(native->attention_type ==
            static_cast<enum llama_attention_type>(value));
      ++tested;
    }
    {
      enum ailo_llama_flash_attn_type value = AILO_LLAMA_FLASH_ATTN_TYPE_AUTO;
      enum ailo_llama_flash_attn_type result = {};
      CHECK(ailo_llama_context_params_flash_attn_type_set(self, value,
                                                          &error) == AILO_OK);
      CHECK(ailo_llama_context_params_flash_attn_type_get(self, &result,
                                                          &error) == AILO_OK);
      CHECK(result == value);
      CHECK(native->flash_attn_type ==
            static_cast<enum llama_flash_attn_type>(value));
      ++tested;
    }
    {
      float value = 0.25f;
      float result = {};
      CHECK(ailo_llama_context_params_rope_freq_base_set(self, value, &error) ==
            AILO_OK);
      CHECK(ailo_llama_context_params_rope_freq_base_get(self, &result,
                                                         &error) == AILO_OK);
      CHECK(result == value);
      CHECK(native->rope_freq_base == static_cast<float>(value));
      ++tested;
    }
    {
      float value = 0.25f;
      float result = {};
      CHECK(ailo_llama_context_params_rope_freq_scale_set(self, value,
                                                          &error) == AILO_OK);
      CHECK(ailo_llama_context_params_rope_freq_scale_get(self, &result,
                                                          &error) == AILO_OK);
      CHECK(result == value);
      CHECK(native->rope_freq_scale == static_cast<float>(value));
      ++tested;
    }
    {
      float value = 0.25f;
      float result = {};
      CHECK(ailo_llama_context_params_yarn_ext_factor_set(self, value,
                                                          &error) == AILO_OK);
      CHECK(ailo_llama_context_params_yarn_ext_factor_get(self, &result,
                                                          &error) == AILO_OK);
      CHECK(result == value);
      CHECK(native->yarn_ext_factor == static_cast<float>(value));
      ++tested;
    }
    {
      float value = 0.25f;
      float result = {};
      CHECK(ailo_llama_context_params_yarn_attn_factor_set(self, value,
                                                           &error) == AILO_OK);
      CHECK(ailo_llama_context_params_yarn_attn_factor_get(self, &result,
                                                           &error) == AILO_OK);
      CHECK(result == value);
      CHECK(native->yarn_attn_factor == static_cast<float>(value));
      ++tested;
    }
    {
      float value = 0.25f;
      float result = {};
      CHECK(ailo_llama_context_params_yarn_beta_fast_set(self, value, &error) ==
            AILO_OK);
      CHECK(ailo_llama_context_params_yarn_beta_fast_get(self, &result,
                                                         &error) == AILO_OK);
      CHECK(result == value);
      CHECK(native->yarn_beta_fast == static_cast<float>(value));
      ++tested;
    }
    {
      float value = 0.25f;
      float result = {};
      CHECK(ailo_llama_context_params_yarn_beta_slow_set(self, value, &error) ==
            AILO_OK);
      CHECK(ailo_llama_context_params_yarn_beta_slow_get(self, &result,
                                                         &error) == AILO_OK);
      CHECK(result == value);
      CHECK(native->yarn_beta_slow == static_cast<float>(value));
      ++tested;
    }
    {
      ailo_u32 value = 97;
      ailo_u32 result = {};
      CHECK(ailo_llama_context_params_yarn_orig_ctx_set(self, value, &error) ==
            AILO_OK);
      CHECK(ailo_llama_context_params_yarn_orig_ctx_get(self, &result,
                                                        &error) == AILO_OK);
      CHECK(result == value);
      CHECK(native->yarn_orig_ctx == static_cast<uint32_t>(value));
      ++tested;
    }
    {
      float value = 0.25f;
      float result = {};
      CHECK(ailo_llama_context_params_defrag_thold_set(self, value, &error) ==
            AILO_OK);
      CHECK(ailo_llama_context_params_defrag_thold_get(self, &result, &error) ==
            AILO_OK);
      CHECK(result == value);
      CHECK(native->defrag_thold == static_cast<float>(value));
      ++tested;
    }
    {
      void *value = reinterpret_cast<void *>(address);
      void *result = {};
      CHECK(ailo_llama_context_params_cb_eval_user_data_set(self, value,
                                                            &error) == AILO_OK);
      CHECK(ailo_llama_context_params_cb_eval_user_data_get(self, &result,
                                                            &error) == AILO_OK);
      CHECK(result == value);
      CHECK(native->cb_eval_user_data == static_cast<void *>(value));
      ++tested;
    }
    {
      enum ailo_ggml_type value = AILO_GGML_TYPE_F32;
      enum ailo_ggml_type result = {};
      CHECK(ailo_llama_context_params_type_k_set(self, value, &error) ==
            AILO_OK);
      CHECK(ailo_llama_context_params_type_k_get(self, &result, &error) ==
            AILO_OK);
      CHECK(result == value);
      CHECK(native->type_k == static_cast<enum ggml_type>(value));
      ++tested;
    }
    {
      enum ailo_ggml_type value = AILO_GGML_TYPE_F32;
      enum ailo_ggml_type result = {};
      CHECK(ailo_llama_context_params_type_v_set(self, value, &error) ==
            AILO_OK);
      CHECK(ailo_llama_context_params_type_v_get(self, &result, &error) ==
            AILO_OK);
      CHECK(result == value);
      CHECK(native->type_v == static_cast<enum ggml_type>(value));
      ++tested;
    }
    {
      void *value = reinterpret_cast<void *>(address);
      void *result = {};
      CHECK(ailo_llama_context_params_abort_callback_data_set(
                self, value, &error) == AILO_OK);
      CHECK(ailo_llama_context_params_abort_callback_data_get(
                self, &result, &error) == AILO_OK);
      CHECK(result == value);
      CHECK(native->abort_callback_data == static_cast<void *>(value));
      ++tested;
    }
    {
      ailo_bool value = 17;
      ailo_bool result = {};
      CHECK(ailo_llama_context_params_embeddings_set(self, value, &error) ==
            AILO_OK);
      CHECK(ailo_llama_context_params_embeddings_get(self, &result, &error) ==
            AILO_OK);
      CHECK(result == 1 && native->embeddings);
      ++tested;
    }
    {
      ailo_bool value = 17;
      ailo_bool result = {};
      CHECK(ailo_llama_context_params_offload_kqv_set(self, value, &error) ==
            AILO_OK);
      CHECK(ailo_llama_context_params_offload_kqv_get(self, &result, &error) ==
            AILO_OK);
      CHECK(result == 1 && native->offload_kqv);
      ++tested;
    }
    {
      ailo_bool value = 17;
      ailo_bool result = {};
      CHECK(ailo_llama_context_params_no_perf_set(self, value, &error) ==
            AILO_OK);
      CHECK(ailo_llama_context_params_no_perf_get(self, &result, &error) ==
            AILO_OK);
      CHECK(result == 1 && native->no_perf);
      ++tested;
    }
    {
      ailo_bool value = 17;
      ailo_bool result = {};
      CHECK(ailo_llama_context_params_op_offload_set(self, value, &error) ==
            AILO_OK);
      CHECK(ailo_llama_context_params_op_offload_get(self, &result, &error) ==
            AILO_OK);
      CHECK(result == 1 && native->op_offload);
      ++tested;
    }
    {
      ailo_bool value = 17;
      ailo_bool result = {};
      CHECK(ailo_llama_context_params_swa_full_set(self, value, &error) ==
            AILO_OK);
      CHECK(ailo_llama_context_params_swa_full_get(self, &result, &error) ==
            AILO_OK);
      CHECK(result == 1 && native->swa_full);
      ++tested;
    }
    {
      ailo_bool value = 17;
      ailo_bool result = {};
      CHECK(ailo_llama_context_params_kv_unified_set(self, value, &error) ==
            AILO_OK);
      CHECK(ailo_llama_context_params_kv_unified_get(self, &result, &error) ==
            AILO_OK);
      CHECK(result == 1 && native->kv_unified);
      ++tested;
    }
    {
      struct ailo_llama_sampler_seq_config *value =
          reinterpret_cast<struct ailo_llama_sampler_seq_config *>(address);
      struct ailo_llama_sampler_seq_config *result = {};
      CHECK(ailo_llama_context_params_samplers_set(self, value, &error) ==
            AILO_OK);
      CHECK(ailo_llama_context_params_samplers_get(self, &result, &error) ==
            AILO_OK);
      CHECK(result == value);
      CHECK(native->samplers ==
            reinterpret_cast<struct llama_sampler_seq_config *>(value));
      ++tested;
    }
    {
      size_t value = 97;
      size_t result = {};
      CHECK(ailo_llama_context_params_n_samplers_set(self, value, &error) ==
            AILO_OK);
      CHECK(ailo_llama_context_params_n_samplers_get(self, &result, &error) ==
            AILO_OK);
      CHECK(result == value);
      CHECK(native->n_samplers == static_cast<size_t>(value));
      ++tested;
    }
    {
      struct ailo_llama_context *value =
          reinterpret_cast<struct ailo_llama_context *>(address);
      struct ailo_llama_context *result = {};
      CHECK(ailo_llama_context_params_ctx_other_set(self, value, &error) ==
            AILO_OK);
      CHECK(ailo_llama_context_params_ctx_other_get(self, &result, &error) ==
            AILO_OK);
      CHECK(result == value);
      CHECK(native->ctx_other ==
            reinterpret_cast<struct llama_context *>(value));
      ++tested;
    }
    struct ailo_llama_context_params *element = nullptr;
    CHECK(ailo_llama_context_params_array_at(self, 2, 1, &element, &error) ==
          AILO_OK);
    CHECK(reinterpret_cast<struct llama_context_params *>(element) ==
          native + 1);
    CHECK(ailo_llama_context_params_array_at(self, 2, 2, &element, &error) ==
              AILO_OUT_OF_RANGE &&
          !element);
    CHECK(ailo_llama_context_params_array_new(
              std::numeric_limits<size_t>::max(), &element, &error) ==
              AILO_OUT_OF_RANGE &&
          !element);
    CHECK(ailo_llama_context_params_array_free(self, &error) == AILO_OK);
    CHECK(ailo_llama_context_params_array_free(nullptr, &error) == AILO_OK);
  }
  {
    struct ailo_llama_embd *self = nullptr;
    CHECK(ailo_llama_embd_array_new(2, &self, &error) == AILO_OK && self);
    auto native = reinterpret_cast<struct llama_embd *>(self);
    (void)native;
    {
      const float *value = reinterpret_cast<const float *>(address);
      const float *result = {};
      CHECK(ailo_llama_embd_data_set(self, value, &error) == AILO_OK);
      CHECK(ailo_llama_embd_data_get(self, &result, &error) == AILO_OK);
      CHECK(result == value);
      CHECK(native->data == static_cast<const float *>(value));
      ++tested;
    }
    {
      size_t value = 97;
      size_t result = {};
      CHECK(ailo_llama_embd_n_rows_set(self, value, &error) == AILO_OK);
      CHECK(ailo_llama_embd_n_rows_get(self, &result, &error) == AILO_OK);
      CHECK(result == value);
      CHECK(native->n_rows == static_cast<size_t>(value));
      ++tested;
    }
    {
      size_t value = 97;
      size_t result = {};
      CHECK(ailo_llama_embd_n_embd_set(self, value, &error) == AILO_OK);
      CHECK(ailo_llama_embd_n_embd_get(self, &result, &error) == AILO_OK);
      CHECK(result == value);
      CHECK(native->n_embd == static_cast<size_t>(value));
      ++tested;
    }
    struct ailo_llama_embd *element = nullptr;
    CHECK(ailo_llama_embd_array_at(self, 2, 1, &element, &error) == AILO_OK);
    CHECK(reinterpret_cast<struct llama_embd *>(element) == native + 1);
    CHECK(ailo_llama_embd_array_at(self, 2, 2, &element, &error) ==
              AILO_OUT_OF_RANGE &&
          !element);
    CHECK(ailo_llama_embd_array_new(std::numeric_limits<size_t>::max(),
                                    &element, &error) == AILO_OUT_OF_RANGE &&
          !element);
    CHECK(ailo_llama_embd_array_free(self, &error) == AILO_OK);
    CHECK(ailo_llama_embd_array_free(nullptr, &error) == AILO_OK);
  }
  {
    struct ailo_llama_logit_bias *self = nullptr;
    CHECK(ailo_llama_logit_bias_array_new(2, &self, &error) == AILO_OK && self);
    auto native = reinterpret_cast<struct llama_logit_bias *>(self);
    (void)native;
    {
      ailo_i32 value = 97;
      ailo_i32 result = {};
      CHECK(ailo_llama_logit_bias_token_set(self, value, &error) == AILO_OK);
      CHECK(ailo_llama_logit_bias_token_get(self, &result, &error) == AILO_OK);
      CHECK(result == value);
      CHECK(native->token == static_cast<int32_t>(value));
      ++tested;
    }
    {
      float value = 0.25f;
      float result = {};
      CHECK(ailo_llama_logit_bias_bias_set(self, value, &error) == AILO_OK);
      CHECK(ailo_llama_logit_bias_bias_get(self, &result, &error) == AILO_OK);
      CHECK(result == value);
      CHECK(native->bias == static_cast<float>(value));
      ++tested;
    }
    struct ailo_llama_logit_bias *element = nullptr;
    CHECK(ailo_llama_logit_bias_array_at(self, 2, 1, &element, &error) ==
          AILO_OK);
    CHECK(reinterpret_cast<struct llama_logit_bias *>(element) == native + 1);
    CHECK(ailo_llama_logit_bias_array_at(self, 2, 2, &element, &error) ==
              AILO_OUT_OF_RANGE &&
          !element);
    CHECK(ailo_llama_logit_bias_array_new(std::numeric_limits<size_t>::max(),
                                          &element,
                                          &error) == AILO_OUT_OF_RANGE &&
          !element);
    CHECK(ailo_llama_logit_bias_array_free(self, &error) == AILO_OK);
    CHECK(ailo_llama_logit_bias_array_free(nullptr, &error) == AILO_OK);
  }
  {
    struct ailo_llama_model_imatrix_data *self = nullptr;
    CHECK(ailo_llama_model_imatrix_data_array_new(2, &self, &error) ==
              AILO_OK &&
          self);
    auto native = reinterpret_cast<struct llama_model_imatrix_data *>(self);
    (void)native;
    {
      const char *value = reinterpret_cast<const char *>(address);
      const char *result = {};
      CHECK(ailo_llama_model_imatrix_data_name_set(self, value, &error) ==
            AILO_OK);
      CHECK(ailo_llama_model_imatrix_data_name_get(self, &result, &error) ==
            AILO_OK);
      CHECK(result == value);
      CHECK(native->name == static_cast<const char *>(value));
      ++tested;
    }
    {
      const float *value = reinterpret_cast<const float *>(address);
      const float *result = {};
      CHECK(ailo_llama_model_imatrix_data_data_set(self, value, &error) ==
            AILO_OK);
      CHECK(ailo_llama_model_imatrix_data_data_get(self, &result, &error) ==
            AILO_OK);
      CHECK(result == value);
      CHECK(native->data == static_cast<const float *>(value));
      ++tested;
    }
    {
      size_t value = 97;
      size_t result = {};
      CHECK(ailo_llama_model_imatrix_data_size_set(self, value, &error) ==
            AILO_OK);
      CHECK(ailo_llama_model_imatrix_data_size_get(self, &result, &error) ==
            AILO_OK);
      CHECK(result == value);
      CHECK(native->size == static_cast<size_t>(value));
      ++tested;
    }
    struct ailo_llama_model_imatrix_data *element = nullptr;
    CHECK(ailo_llama_model_imatrix_data_array_at(self, 2, 1, &element,
                                                 &error) == AILO_OK);
    CHECK(reinterpret_cast<struct llama_model_imatrix_data *>(element) ==
          native + 1);
    CHECK(ailo_llama_model_imatrix_data_array_at(self, 2, 2, &element,
                                                 &error) == AILO_OUT_OF_RANGE &&
          !element);
    CHECK(ailo_llama_model_imatrix_data_array_new(
              std::numeric_limits<size_t>::max(), &element, &error) ==
              AILO_OUT_OF_RANGE &&
          !element);
    CHECK(ailo_llama_model_imatrix_data_array_free(self, &error) == AILO_OK);
    CHECK(ailo_llama_model_imatrix_data_array_free(nullptr, &error) == AILO_OK);
  }
  {
    struct ailo_llama_model_kv_override *self = nullptr;
    CHECK(ailo_llama_model_kv_override_array_new(2, &self, &error) == AILO_OK &&
          self);
    auto native = reinterpret_cast<struct llama_model_kv_override *>(self);
    (void)native;
    {
      enum ailo_llama_model_kv_override_type value =
          AILO_LLAMA_KV_OVERRIDE_TYPE_INT;
      enum ailo_llama_model_kv_override_type result = {};
      CHECK(ailo_llama_model_kv_override_tag_set(self, value, &error) ==
            AILO_OK);
      CHECK(ailo_llama_model_kv_override_tag_get(self, &result, &error) ==
            AILO_OK);
      CHECK(result == value);
      CHECK(native->tag ==
            static_cast<enum llama_model_kv_override_type>(value));
      ++tested;
    }
    {
      char value = 97;
      char result = {};
      CHECK(ailo_llama_model_kv_override_key_set(self, 127, value, &error) ==
            AILO_OK);
      CHECK(ailo_llama_model_kv_override_key_get(self, 127, &result, &error) ==
            AILO_OK);
      CHECK(result == value);
      CHECK(native->key[127] == static_cast<char>(value));
      CHECK(ailo_llama_model_kv_override_key_get(self, 128, &result, &error) ==
            AILO_OUT_OF_RANGE);
      ++tested;
    }
    {
      ailo_i64 value = ailo_i64{0x80000000U, 17};
      ailo_i64 result = {};
      CHECK(ailo_llama_model_kv_override_val_i64_set(self, value, &error) ==
            AILO_OK);
      CHECK(ailo_llama_model_kv_override_val_i64_get(self, &result, &error) ==
            AILO_OK);
      CHECK(result.hi == value.hi && result.lo == value.lo);
      CHECK(native->val_i64 == ailo::detail::to_native(value));
      ++tested;
    }
    {
      double value = 0.5;
      double result = {};
      CHECK(ailo_llama_model_kv_override_val_f64_set(self, value, &error) ==
            AILO_OK);
      CHECK(ailo_llama_model_kv_override_val_f64_get(self, &result, &error) ==
            AILO_OK);
      CHECK(result == value);
      CHECK(native->val_f64 == static_cast<double>(value));
      ++tested;
    }
    {
      ailo_bool value = 17;
      ailo_bool result = {};
      CHECK(ailo_llama_model_kv_override_val_bool_set(self, value, &error) ==
            AILO_OK);
      CHECK(ailo_llama_model_kv_override_val_bool_get(self, &result, &error) ==
            AILO_OK);
      CHECK(result == 1 && native->val_bool);
      ++tested;
    }
    {
      char value = 97;
      char result = {};
      CHECK(ailo_llama_model_kv_override_val_str_set(self, 127, value,
                                                     &error) == AILO_OK);
      CHECK(ailo_llama_model_kv_override_val_str_get(self, 127, &result,
                                                     &error) == AILO_OK);
      CHECK(result == value);
      CHECK(native->val_str[127] == static_cast<char>(value));
      CHECK(ailo_llama_model_kv_override_val_str_get(
                self, 128, &result, &error) == AILO_OUT_OF_RANGE);
      ++tested;
    }
    struct ailo_llama_model_kv_override *element = nullptr;
    CHECK(ailo_llama_model_kv_override_array_at(self, 2, 1, &element, &error) ==
          AILO_OK);
    CHECK(reinterpret_cast<struct llama_model_kv_override *>(element) ==
          native + 1);
    CHECK(ailo_llama_model_kv_override_array_at(self, 2, 2, &element, &error) ==
              AILO_OUT_OF_RANGE &&
          !element);
    CHECK(ailo_llama_model_kv_override_array_new(
              std::numeric_limits<size_t>::max(), &element, &error) ==
              AILO_OUT_OF_RANGE &&
          !element);
    CHECK(ailo_llama_model_kv_override_array_free(self, &error) == AILO_OK);
    CHECK(ailo_llama_model_kv_override_array_free(nullptr, &error) == AILO_OK);
  }
  {
    struct ailo_llama_model_params *self = nullptr;
    CHECK(ailo_llama_model_params_array_new(2, &self, &error) == AILO_OK &&
          self);
    auto native = reinterpret_cast<struct llama_model_params *>(self);
    (void)native;
    {
      struct ailo_storage_struct_ggml_backend_device *value =
          reinterpret_cast<struct ailo_storage_struct_ggml_backend_device *>(
              address);
      struct ailo_storage_struct_ggml_backend_device *result = {};
      CHECK(ailo_llama_model_params_devices_set(self, value, &error) ==
            AILO_OK);
      CHECK(ailo_llama_model_params_devices_get(self, &result, &error) ==
            AILO_OK);
      CHECK(result == value);
      CHECK(native->devices ==
            reinterpret_cast<struct ggml_backend_device **>(value));
      ++tested;
    }
    {
      const struct ailo_llama_model_tensor_buft_override *value =
          reinterpret_cast<
              const struct ailo_llama_model_tensor_buft_override *>(address);
      const struct ailo_llama_model_tensor_buft_override *result = {};
      CHECK(ailo_llama_model_params_tensor_buft_overrides_set(
                self, value, &error) == AILO_OK);
      CHECK(ailo_llama_model_params_tensor_buft_overrides_get(
                self, &result, &error) == AILO_OK);
      CHECK(result == value);
      CHECK(native->tensor_buft_overrides ==
            reinterpret_cast<const struct llama_model_tensor_buft_override *>(
                value));
      ++tested;
    }
    {
      ailo_i32 value = 97;
      ailo_i32 result = {};
      CHECK(ailo_llama_model_params_n_gpu_layers_set(self, value, &error) ==
            AILO_OK);
      CHECK(ailo_llama_model_params_n_gpu_layers_get(self, &result, &error) ==
            AILO_OK);
      CHECK(result == value);
      CHECK(native->n_gpu_layers == static_cast<int32_t>(value));
      ++tested;
    }
    {
      enum ailo_llama_split_mode value = AILO_LLAMA_SPLIT_MODE_NONE;
      enum ailo_llama_split_mode result = {};
      CHECK(ailo_llama_model_params_split_mode_set(self, value, &error) ==
            AILO_OK);
      CHECK(ailo_llama_model_params_split_mode_get(self, &result, &error) ==
            AILO_OK);
      CHECK(result == value);
      CHECK(native->split_mode == static_cast<enum llama_split_mode>(value));
      ++tested;
    }
    {
      enum ailo_llama_load_mode value = AILO_LLAMA_LOAD_MODE_AUTO;
      enum ailo_llama_load_mode result = {};
      CHECK(ailo_llama_model_params_load_mode_set(self, value, &error) ==
            AILO_OK);
      CHECK(ailo_llama_model_params_load_mode_get(self, &result, &error) ==
            AILO_OK);
      CHECK(result == value);
      CHECK(native->load_mode == static_cast<enum llama_load_mode>(value));
      ++tested;
    }
    {
      enum ailo_llama_lazy_mode value = AILO_LLAMA_LAZY_MODE_OFF;
      enum ailo_llama_lazy_mode result = {};
      CHECK(ailo_llama_model_params_lazy_mode_set(self, value, &error) ==
            AILO_OK);
      CHECK(ailo_llama_model_params_lazy_mode_get(self, &result, &error) ==
            AILO_OK);
      CHECK(result == value);
      CHECK(native->lazy_mode == static_cast<enum llama_lazy_mode>(value));
      ++tested;
    }
    {
      ailo_i32 value = 97;
      ailo_i32 result = {};
      CHECK(ailo_llama_model_params_main_gpu_set(self, value, &error) ==
            AILO_OK);
      CHECK(ailo_llama_model_params_main_gpu_get(self, &result, &error) ==
            AILO_OK);
      CHECK(result == value);
      CHECK(native->main_gpu == static_cast<int32_t>(value));
      ++tested;
    }
    {
      const float *value = reinterpret_cast<const float *>(address);
      const float *result = {};
      CHECK(ailo_llama_model_params_tensor_split_set(self, value, &error) ==
            AILO_OK);
      CHECK(ailo_llama_model_params_tensor_split_get(self, &result, &error) ==
            AILO_OK);
      CHECK(result == value);
      CHECK(native->tensor_split == static_cast<const float *>(value));
      ++tested;
    }
    {
      void *value = reinterpret_cast<void *>(address);
      void *result = {};
      CHECK(ailo_llama_model_params_progress_callback_user_data_set(
                self, value, &error) == AILO_OK);
      CHECK(ailo_llama_model_params_progress_callback_user_data_get(
                self, &result, &error) == AILO_OK);
      CHECK(result == value);
      CHECK(native->progress_callback_user_data == static_cast<void *>(value));
      ++tested;
    }
    {
      const struct ailo_llama_model_kv_override *value =
          reinterpret_cast<const struct ailo_llama_model_kv_override *>(
              address);
      const struct ailo_llama_model_kv_override *result = {};
      CHECK(ailo_llama_model_params_kv_overrides_set(self, value, &error) ==
            AILO_OK);
      CHECK(ailo_llama_model_params_kv_overrides_get(self, &result, &error) ==
            AILO_OK);
      CHECK(result == value);
      CHECK(native->kv_overrides ==
            reinterpret_cast<const struct llama_model_kv_override *>(value));
      ++tested;
    }
    {
      ailo_bool value = 17;
      ailo_bool result = {};
      CHECK(ailo_llama_model_params_vocab_only_set(self, value, &error) ==
            AILO_OK);
      CHECK(ailo_llama_model_params_vocab_only_get(self, &result, &error) ==
            AILO_OK);
      CHECK(result == 1 && native->vocab_only);
      ++tested;
    }
    {
      ailo_bool value = 17;
      ailo_bool result = {};
      CHECK(ailo_llama_model_params_check_tensors_set(self, value, &error) ==
            AILO_OK);
      CHECK(ailo_llama_model_params_check_tensors_get(self, &result, &error) ==
            AILO_OK);
      CHECK(result == 1 && native->check_tensors);
      ++tested;
    }
    {
      ailo_bool value = 17;
      ailo_bool result = {};
      CHECK(ailo_llama_model_params_use_extra_bufts_set(self, value, &error) ==
            AILO_OK);
      CHECK(ailo_llama_model_params_use_extra_bufts_get(self, &result,
                                                        &error) == AILO_OK);
      CHECK(result == 1 && native->use_extra_bufts);
      ++tested;
    }
    {
      ailo_bool value = 17;
      ailo_bool result = {};
      CHECK(ailo_llama_model_params_no_host_set(self, value, &error) ==
            AILO_OK);
      CHECK(ailo_llama_model_params_no_host_get(self, &result, &error) ==
            AILO_OK);
      CHECK(result == 1 && native->no_host);
      ++tested;
    }
    {
      ailo_bool value = 17;
      ailo_bool result = {};
      CHECK(ailo_llama_model_params_no_alloc_set(self, value, &error) ==
            AILO_OK);
      CHECK(ailo_llama_model_params_no_alloc_get(self, &result, &error) ==
            AILO_OK);
      CHECK(result == 1 && native->no_alloc);
      ++tested;
    }
    {
      ailo_bool value = 17;
      ailo_bool result = {};
      CHECK(ailo_llama_model_params_load_mtp_set(self, value, &error) ==
            AILO_OK);
      CHECK(ailo_llama_model_params_load_mtp_get(self, &result, &error) ==
            AILO_OK);
      CHECK(result == 1 && native->load_mtp);
      ++tested;
    }
    struct ailo_llama_model_params *element = nullptr;
    CHECK(ailo_llama_model_params_array_at(self, 2, 1, &element, &error) ==
          AILO_OK);
    CHECK(reinterpret_cast<struct llama_model_params *>(element) == native + 1);
    CHECK(ailo_llama_model_params_array_at(self, 2, 2, &element, &error) ==
              AILO_OUT_OF_RANGE &&
          !element);
    CHECK(ailo_llama_model_params_array_new(std::numeric_limits<size_t>::max(),
                                            &element,
                                            &error) == AILO_OUT_OF_RANGE &&
          !element);
    CHECK(ailo_llama_model_params_array_free(self, &error) == AILO_OK);
    CHECK(ailo_llama_model_params_array_free(nullptr, &error) == AILO_OK);
  }
  {
    struct ailo_llama_model_quantize_params *self = nullptr;
    CHECK(ailo_llama_model_quantize_params_array_new(2, &self, &error) ==
              AILO_OK &&
          self);
    auto native = reinterpret_cast<struct llama_model_quantize_params *>(self);
    (void)native;
    {
      ailo_i32 value = 97;
      ailo_i32 result = {};
      CHECK(ailo_llama_model_quantize_params_nthread_set(self, value, &error) ==
            AILO_OK);
      CHECK(ailo_llama_model_quantize_params_nthread_get(self, &result,
                                                         &error) == AILO_OK);
      CHECK(result == value);
      CHECK(native->nthread == static_cast<int32_t>(value));
      ++tested;
    }
    {
      enum ailo_llama_ftype value = AILO_LLAMA_FTYPE_ALL_F32;
      enum ailo_llama_ftype result = {};
      CHECK(ailo_llama_model_quantize_params_ftype_set(self, value, &error) ==
            AILO_OK);
      CHECK(ailo_llama_model_quantize_params_ftype_get(self, &result, &error) ==
            AILO_OK);
      CHECK(result == value);
      CHECK(native->ftype == static_cast<enum llama_ftype>(value));
      ++tested;
    }
    {
      enum ailo_ggml_type value = AILO_GGML_TYPE_F32;
      enum ailo_ggml_type result = {};
      CHECK(ailo_llama_model_quantize_params_output_tensor_type_set(
                self, value, &error) == AILO_OK);
      CHECK(ailo_llama_model_quantize_params_output_tensor_type_get(
                self, &result, &error) == AILO_OK);
      CHECK(result == value);
      CHECK(native->output_tensor_type == static_cast<enum ggml_type>(value));
      ++tested;
    }
    {
      enum ailo_ggml_type value = AILO_GGML_TYPE_F32;
      enum ailo_ggml_type result = {};
      CHECK(ailo_llama_model_quantize_params_token_embedding_type_set(
                self, value, &error) == AILO_OK);
      CHECK(ailo_llama_model_quantize_params_token_embedding_type_get(
                self, &result, &error) == AILO_OK);
      CHECK(result == value);
      CHECK(native->token_embedding_type == static_cast<enum ggml_type>(value));
      ++tested;
    }
    {
      ailo_bool value = 17;
      ailo_bool result = {};
      CHECK(ailo_llama_model_quantize_params_allow_requantize_set(
                self, value, &error) == AILO_OK);
      CHECK(ailo_llama_model_quantize_params_allow_requantize_get(
                self, &result, &error) == AILO_OK);
      CHECK(result == 1 && native->allow_requantize);
      ++tested;
    }
    {
      ailo_bool value = 17;
      ailo_bool result = {};
      CHECK(ailo_llama_model_quantize_params_quantize_output_tensor_set(
                self, value, &error) == AILO_OK);
      CHECK(ailo_llama_model_quantize_params_quantize_output_tensor_get(
                self, &result, &error) == AILO_OK);
      CHECK(result == 1 && native->quantize_output_tensor);
      ++tested;
    }
    {
      ailo_bool value = 17;
      ailo_bool result = {};
      CHECK(ailo_llama_model_quantize_params_only_copy_set(self, value,
                                                           &error) == AILO_OK);
      CHECK(ailo_llama_model_quantize_params_only_copy_get(self, &result,
                                                           &error) == AILO_OK);
      CHECK(result == 1 && native->only_copy);
      ++tested;
    }
    {
      ailo_bool value = 17;
      ailo_bool result = {};
      CHECK(ailo_llama_model_quantize_params_pure_set(self, value, &error) ==
            AILO_OK);
      CHECK(ailo_llama_model_quantize_params_pure_get(self, &result, &error) ==
            AILO_OK);
      CHECK(result == 1 && native->pure);
      ++tested;
    }
    {
      ailo_bool value = 17;
      ailo_bool result = {};
      CHECK(ailo_llama_model_quantize_params_keep_split_set(self, value,
                                                            &error) == AILO_OK);
      CHECK(ailo_llama_model_quantize_params_keep_split_get(self, &result,
                                                            &error) == AILO_OK);
      CHECK(result == 1 && native->keep_split);
      ++tested;
    }
    {
      ailo_bool value = 17;
      ailo_bool result = {};
      CHECK(ailo_llama_model_quantize_params_dry_run_set(self, value, &error) ==
            AILO_OK);
      CHECK(ailo_llama_model_quantize_params_dry_run_get(self, &result,
                                                         &error) == AILO_OK);
      CHECK(result == 1 && native->dry_run);
      ++tested;
    }
    {
      const struct ailo_llama_model_imatrix_data *value =
          reinterpret_cast<const struct ailo_llama_model_imatrix_data *>(
              address);
      const struct ailo_llama_model_imatrix_data *result = {};
      CHECK(ailo_llama_model_quantize_params_imatrix_set(self, value, &error) ==
            AILO_OK);
      CHECK(ailo_llama_model_quantize_params_imatrix_get(self, &result,
                                                         &error) == AILO_OK);
      CHECK(result == value);
      CHECK(native->imatrix ==
            reinterpret_cast<const struct llama_model_imatrix_data *>(value));
      ++tested;
    }
    {
      const struct ailo_llama_model_kv_override *value =
          reinterpret_cast<const struct ailo_llama_model_kv_override *>(
              address);
      const struct ailo_llama_model_kv_override *result = {};
      CHECK(ailo_llama_model_quantize_params_kv_overrides_set(
                self, value, &error) == AILO_OK);
      CHECK(ailo_llama_model_quantize_params_kv_overrides_get(
                self, &result, &error) == AILO_OK);
      CHECK(result == value);
      CHECK(native->kv_overrides ==
            reinterpret_cast<const struct llama_model_kv_override *>(value));
      ++tested;
    }
    {
      const struct ailo_llama_model_tensor_override *value =
          reinterpret_cast<const struct ailo_llama_model_tensor_override *>(
              address);
      const struct ailo_llama_model_tensor_override *result = {};
      CHECK(ailo_llama_model_quantize_params_tt_overrides_set(
                self, value, &error) == AILO_OK);
      CHECK(ailo_llama_model_quantize_params_tt_overrides_get(
                self, &result, &error) == AILO_OK);
      CHECK(result == value);
      CHECK(
          native->tt_overrides ==
          reinterpret_cast<const struct llama_model_tensor_override *>(value));
      ++tested;
    }
    {
      const ailo_i32 *value = reinterpret_cast<const ailo_i32 *>(address);
      const ailo_i32 *result = {};
      CHECK(ailo_llama_model_quantize_params_prune_layers_set(
                self, value, &error) == AILO_OK);
      CHECK(ailo_llama_model_quantize_params_prune_layers_get(
                self, &result, &error) == AILO_OK);
      CHECK(result == value);
      CHECK(native->prune_layers == static_cast<const int32_t *>(value));
      ++tested;
    }
    {
      size_t value = 97;
      size_t result = {};
      CHECK(ailo_llama_model_quantize_params_max_buf_size_set(
                self, value, &error) == AILO_OK);
      CHECK(ailo_llama_model_quantize_params_max_buf_size_get(
                self, &result, &error) == AILO_OK);
      CHECK(result == value);
      CHECK(native->max_buf_size == static_cast<size_t>(value));
      ++tested;
    }
    struct ailo_llama_model_quantize_params *element = nullptr;
    CHECK(ailo_llama_model_quantize_params_array_at(self, 2, 1, &element,
                                                    &error) == AILO_OK);
    CHECK(reinterpret_cast<struct llama_model_quantize_params *>(element) ==
          native + 1);
    CHECK(ailo_llama_model_quantize_params_array_at(
              self, 2, 2, &element, &error) == AILO_OUT_OF_RANGE &&
          !element);
    CHECK(ailo_llama_model_quantize_params_array_new(
              std::numeric_limits<size_t>::max(), &element, &error) ==
              AILO_OUT_OF_RANGE &&
          !element);
    CHECK(ailo_llama_model_quantize_params_array_free(self, &error) == AILO_OK);
    CHECK(ailo_llama_model_quantize_params_array_free(nullptr, &error) ==
          AILO_OK);
  }
  {
    struct ailo_llama_model_tensor_buft_override *self = nullptr;
    CHECK(ailo_llama_model_tensor_buft_override_array_new(2, &self, &error) ==
              AILO_OK &&
          self);
    auto native =
        reinterpret_cast<struct llama_model_tensor_buft_override *>(self);
    (void)native;
    {
      const char *value = reinterpret_cast<const char *>(address);
      const char *result = {};
      CHECK(ailo_llama_model_tensor_buft_override_pattern_set(
                self, value, &error) == AILO_OK);
      CHECK(ailo_llama_model_tensor_buft_override_pattern_get(
                self, &result, &error) == AILO_OK);
      CHECK(result == value);
      CHECK(native->pattern == static_cast<const char *>(value));
      ++tested;
    }
    {
      struct ailo_ggml_backend_buffer_type *value =
          reinterpret_cast<struct ailo_ggml_backend_buffer_type *>(address);
      struct ailo_ggml_backend_buffer_type *result = {};
      CHECK(ailo_llama_model_tensor_buft_override_buft_set(self, value,
                                                           &error) == AILO_OK);
      CHECK(ailo_llama_model_tensor_buft_override_buft_get(self, &result,
                                                           &error) == AILO_OK);
      CHECK(result == value);
      CHECK(native->buft ==
            reinterpret_cast<struct ggml_backend_buffer_type *>(value));
      ++tested;
    }
    struct ailo_llama_model_tensor_buft_override *element = nullptr;
    CHECK(ailo_llama_model_tensor_buft_override_array_at(self, 2, 1, &element,
                                                         &error) == AILO_OK);
    CHECK(reinterpret_cast<struct llama_model_tensor_buft_override *>(
              element) == native + 1);
    CHECK(ailo_llama_model_tensor_buft_override_array_at(
              self, 2, 2, &element, &error) == AILO_OUT_OF_RANGE &&
          !element);
    CHECK(ailo_llama_model_tensor_buft_override_array_new(
              std::numeric_limits<size_t>::max(), &element, &error) ==
              AILO_OUT_OF_RANGE &&
          !element);
    CHECK(ailo_llama_model_tensor_buft_override_array_free(self, &error) ==
          AILO_OK);
    CHECK(ailo_llama_model_tensor_buft_override_array_free(nullptr, &error) ==
          AILO_OK);
  }
  {
    struct ailo_llama_model_tensor_override *self = nullptr;
    CHECK(ailo_llama_model_tensor_override_array_new(2, &self, &error) ==
              AILO_OK &&
          self);
    auto native = reinterpret_cast<struct llama_model_tensor_override *>(self);
    (void)native;
    {
      const char *value = reinterpret_cast<const char *>(address);
      const char *result = {};
      CHECK(ailo_llama_model_tensor_override_pattern_set(self, value, &error) ==
            AILO_OK);
      CHECK(ailo_llama_model_tensor_override_pattern_get(self, &result,
                                                         &error) == AILO_OK);
      CHECK(result == value);
      CHECK(native->pattern == static_cast<const char *>(value));
      ++tested;
    }
    {
      enum ailo_ggml_type value = AILO_GGML_TYPE_F32;
      enum ailo_ggml_type result = {};
      CHECK(ailo_llama_model_tensor_override_type_set(self, value, &error) ==
            AILO_OK);
      CHECK(ailo_llama_model_tensor_override_type_get(self, &result, &error) ==
            AILO_OK);
      CHECK(result == value);
      CHECK(native->type == static_cast<enum ggml_type>(value));
      ++tested;
    }
    struct ailo_llama_model_tensor_override *element = nullptr;
    CHECK(ailo_llama_model_tensor_override_array_at(self, 2, 1, &element,
                                                    &error) == AILO_OK);
    CHECK(reinterpret_cast<struct llama_model_tensor_override *>(element) ==
          native + 1);
    CHECK(ailo_llama_model_tensor_override_array_at(
              self, 2, 2, &element, &error) == AILO_OUT_OF_RANGE &&
          !element);
    CHECK(ailo_llama_model_tensor_override_array_new(
              std::numeric_limits<size_t>::max(), &element, &error) ==
              AILO_OUT_OF_RANGE &&
          !element);
    CHECK(ailo_llama_model_tensor_override_array_free(self, &error) == AILO_OK);
    CHECK(ailo_llama_model_tensor_override_array_free(nullptr, &error) ==
          AILO_OK);
  }
  {
    struct ailo_llama_opt_params *self = nullptr;
    CHECK(ailo_llama_opt_params_array_new(2, &self, &error) == AILO_OK && self);
    auto native = reinterpret_cast<struct llama_opt_params *>(self);
    (void)native;
    {
      ailo_u32 value = 97;
      ailo_u32 result = {};
      CHECK(ailo_llama_opt_params_n_ctx_train_set(self, value, &error) ==
            AILO_OK);
      CHECK(ailo_llama_opt_params_n_ctx_train_get(self, &result, &error) ==
            AILO_OK);
      CHECK(result == value);
      CHECK(native->n_ctx_train == static_cast<uint32_t>(value));
      ++tested;
    }
    {
      void *value = reinterpret_cast<void *>(address);
      void *result = {};
      CHECK(ailo_llama_opt_params_param_filter_ud_set(self, value, &error) ==
            AILO_OK);
      CHECK(ailo_llama_opt_params_param_filter_ud_get(self, &result, &error) ==
            AILO_OK);
      CHECK(result == value);
      CHECK(native->param_filter_ud == static_cast<void *>(value));
      ++tested;
    }
    {
      void *value = reinterpret_cast<void *>(address);
      void *result = {};
      CHECK(ailo_llama_opt_params_get_opt_pars_ud_set(self, value, &error) ==
            AILO_OK);
      CHECK(ailo_llama_opt_params_get_opt_pars_ud_get(self, &result, &error) ==
            AILO_OK);
      CHECK(result == value);
      CHECK(native->get_opt_pars_ud == static_cast<void *>(value));
      ++tested;
    }
    {
      enum ailo_ggml_opt_optimizer_type value =
          AILO_GGML_OPT_OPTIMIZER_TYPE_ADAMW;
      enum ailo_ggml_opt_optimizer_type result = {};
      CHECK(ailo_llama_opt_params_optimizer_type_set(self, value, &error) ==
            AILO_OK);
      CHECK(ailo_llama_opt_params_optimizer_type_get(self, &result, &error) ==
            AILO_OK);
      CHECK(result == value);
      CHECK(native->optimizer_type ==
            static_cast<enum ggml_opt_optimizer_type>(value));
      ++tested;
    }
    struct ailo_llama_opt_params *element = nullptr;
    CHECK(ailo_llama_opt_params_array_at(self, 2, 1, &element, &error) ==
          AILO_OK);
    CHECK(reinterpret_cast<struct llama_opt_params *>(element) == native + 1);
    CHECK(ailo_llama_opt_params_array_at(self, 2, 2, &element, &error) ==
              AILO_OUT_OF_RANGE &&
          !element);
    CHECK(ailo_llama_opt_params_array_new(std::numeric_limits<size_t>::max(),
                                          &element,
                                          &error) == AILO_OUT_OF_RANGE &&
          !element);
    CHECK(ailo_llama_opt_params_array_free(self, &error) == AILO_OK);
    CHECK(ailo_llama_opt_params_array_free(nullptr, &error) == AILO_OK);
  }
  {
    struct ailo_llama_perf_context_data *self = nullptr;
    CHECK(ailo_llama_perf_context_data_array_new(2, &self, &error) == AILO_OK &&
          self);
    auto native = reinterpret_cast<struct llama_perf_context_data *>(self);
    (void)native;
    {
      double value = 0.5;
      double result = {};
      CHECK(ailo_llama_perf_context_data_t_start_ms_set(self, value, &error) ==
            AILO_OK);
      CHECK(ailo_llama_perf_context_data_t_start_ms_get(self, &result,
                                                        &error) == AILO_OK);
      CHECK(result == value);
      CHECK(native->t_start_ms == static_cast<double>(value));
      ++tested;
    }
    {
      double value = 0.5;
      double result = {};
      CHECK(ailo_llama_perf_context_data_t_load_ms_set(self, value, &error) ==
            AILO_OK);
      CHECK(ailo_llama_perf_context_data_t_load_ms_get(self, &result, &error) ==
            AILO_OK);
      CHECK(result == value);
      CHECK(native->t_load_ms == static_cast<double>(value));
      ++tested;
    }
    {
      double value = 0.5;
      double result = {};
      CHECK(ailo_llama_perf_context_data_t_p_eval_ms_set(self, value, &error) ==
            AILO_OK);
      CHECK(ailo_llama_perf_context_data_t_p_eval_ms_get(self, &result,
                                                         &error) == AILO_OK);
      CHECK(result == value);
      CHECK(native->t_p_eval_ms == static_cast<double>(value));
      ++tested;
    }
    {
      double value = 0.5;
      double result = {};
      CHECK(ailo_llama_perf_context_data_t_eval_ms_set(self, value, &error) ==
            AILO_OK);
      CHECK(ailo_llama_perf_context_data_t_eval_ms_get(self, &result, &error) ==
            AILO_OK);
      CHECK(result == value);
      CHECK(native->t_eval_ms == static_cast<double>(value));
      ++tested;
    }
    {
      ailo_i32 value = 97;
      ailo_i32 result = {};
      CHECK(ailo_llama_perf_context_data_n_p_eval_set(self, value, &error) ==
            AILO_OK);
      CHECK(ailo_llama_perf_context_data_n_p_eval_get(self, &result, &error) ==
            AILO_OK);
      CHECK(result == value);
      CHECK(native->n_p_eval == static_cast<int32_t>(value));
      ++tested;
    }
    {
      ailo_i32 value = 97;
      ailo_i32 result = {};
      CHECK(ailo_llama_perf_context_data_n_eval_set(self, value, &error) ==
            AILO_OK);
      CHECK(ailo_llama_perf_context_data_n_eval_get(self, &result, &error) ==
            AILO_OK);
      CHECK(result == value);
      CHECK(native->n_eval == static_cast<int32_t>(value));
      ++tested;
    }
    {
      ailo_i32 value = 97;
      ailo_i32 result = {};
      CHECK(ailo_llama_perf_context_data_n_reused_set(self, value, &error) ==
            AILO_OK);
      CHECK(ailo_llama_perf_context_data_n_reused_get(self, &result, &error) ==
            AILO_OK);
      CHECK(result == value);
      CHECK(native->n_reused == static_cast<int32_t>(value));
      ++tested;
    }
    struct ailo_llama_perf_context_data *element = nullptr;
    CHECK(ailo_llama_perf_context_data_array_at(self, 2, 1, &element, &error) ==
          AILO_OK);
    CHECK(reinterpret_cast<struct llama_perf_context_data *>(element) ==
          native + 1);
    CHECK(ailo_llama_perf_context_data_array_at(self, 2, 2, &element, &error) ==
              AILO_OUT_OF_RANGE &&
          !element);
    CHECK(ailo_llama_perf_context_data_array_new(
              std::numeric_limits<size_t>::max(), &element, &error) ==
              AILO_OUT_OF_RANGE &&
          !element);
    CHECK(ailo_llama_perf_context_data_array_free(self, &error) == AILO_OK);
    CHECK(ailo_llama_perf_context_data_array_free(nullptr, &error) == AILO_OK);
  }
  {
    struct ailo_llama_perf_sampler_data *self = nullptr;
    CHECK(ailo_llama_perf_sampler_data_array_new(2, &self, &error) == AILO_OK &&
          self);
    auto native = reinterpret_cast<struct llama_perf_sampler_data *>(self);
    (void)native;
    {
      double value = 0.5;
      double result = {};
      CHECK(ailo_llama_perf_sampler_data_t_sample_ms_set(self, value, &error) ==
            AILO_OK);
      CHECK(ailo_llama_perf_sampler_data_t_sample_ms_get(self, &result,
                                                         &error) == AILO_OK);
      CHECK(result == value);
      CHECK(native->t_sample_ms == static_cast<double>(value));
      ++tested;
    }
    {
      ailo_i32 value = 97;
      ailo_i32 result = {};
      CHECK(ailo_llama_perf_sampler_data_n_sample_set(self, value, &error) ==
            AILO_OK);
      CHECK(ailo_llama_perf_sampler_data_n_sample_get(self, &result, &error) ==
            AILO_OK);
      CHECK(result == value);
      CHECK(native->n_sample == static_cast<int32_t>(value));
      ++tested;
    }
    struct ailo_llama_perf_sampler_data *element = nullptr;
    CHECK(ailo_llama_perf_sampler_data_array_at(self, 2, 1, &element, &error) ==
          AILO_OK);
    CHECK(reinterpret_cast<struct llama_perf_sampler_data *>(element) ==
          native + 1);
    CHECK(ailo_llama_perf_sampler_data_array_at(self, 2, 2, &element, &error) ==
              AILO_OUT_OF_RANGE &&
          !element);
    CHECK(ailo_llama_perf_sampler_data_array_new(
              std::numeric_limits<size_t>::max(), &element, &error) ==
              AILO_OUT_OF_RANGE &&
          !element);
    CHECK(ailo_llama_perf_sampler_data_array_free(self, &error) == AILO_OK);
    CHECK(ailo_llama_perf_sampler_data_array_free(nullptr, &error) == AILO_OK);
  }
  {
    struct ailo_llama_sampler *self = nullptr;
    CHECK(ailo_llama_sampler_array_new(2, &self, &error) == AILO_OK && self);
    auto native = reinterpret_cast<struct llama_sampler *>(self);
    (void)native;
    {
      struct ailo_llama_sampler_i *value =
          reinterpret_cast<struct ailo_llama_sampler_i *>(address);
      struct ailo_llama_sampler_i *result = {};
      CHECK(ailo_llama_sampler_iface_set(self, value, &error) == AILO_OK);
      CHECK(ailo_llama_sampler_iface_get(self, &result, &error) == AILO_OK);
      CHECK(result == value);
      CHECK(native->iface == reinterpret_cast<struct llama_sampler_i *>(value));
      ++tested;
    }
    {
      void *value = reinterpret_cast<void *>(address);
      void *result = {};
      CHECK(ailo_llama_sampler_ctx_set(self, value, &error) == AILO_OK);
      CHECK(ailo_llama_sampler_ctx_get(self, &result, &error) == AILO_OK);
      CHECK(result == value);
      CHECK(native->ctx == static_cast<void *>(value));
      ++tested;
    }
    struct ailo_llama_sampler *element = nullptr;
    CHECK(ailo_llama_sampler_array_at(self, 2, 1, &element, &error) == AILO_OK);
    CHECK(reinterpret_cast<struct llama_sampler *>(element) == native + 1);
    CHECK(ailo_llama_sampler_array_at(self, 2, 2, &element, &error) ==
              AILO_OUT_OF_RANGE &&
          !element);
    CHECK(ailo_llama_sampler_array_new(std::numeric_limits<size_t>::max(),
                                       &element, &error) == AILO_OUT_OF_RANGE &&
          !element);
    CHECK(ailo_llama_sampler_array_free(self, &error) == AILO_OK);
    CHECK(ailo_llama_sampler_array_free(nullptr, &error) == AILO_OK);
  }
  {
    struct ailo_llama_sampler_chain_params *self = nullptr;
    CHECK(ailo_llama_sampler_chain_params_array_new(2, &self, &error) ==
              AILO_OK &&
          self);
    auto native = reinterpret_cast<struct llama_sampler_chain_params *>(self);
    (void)native;
    {
      ailo_bool value = 17;
      ailo_bool result = {};
      CHECK(ailo_llama_sampler_chain_params_no_perf_set(self, value, &error) ==
            AILO_OK);
      CHECK(ailo_llama_sampler_chain_params_no_perf_get(self, &result,
                                                        &error) == AILO_OK);
      CHECK(result == 1 && native->no_perf);
      ++tested;
    }
    struct ailo_llama_sampler_chain_params *element = nullptr;
    CHECK(ailo_llama_sampler_chain_params_array_at(self, 2, 1, &element,
                                                   &error) == AILO_OK);
    CHECK(reinterpret_cast<struct llama_sampler_chain_params *>(element) ==
          native + 1);
    CHECK(ailo_llama_sampler_chain_params_array_at(
              self, 2, 2, &element, &error) == AILO_OUT_OF_RANGE &&
          !element);
    CHECK(ailo_llama_sampler_chain_params_array_new(
              std::numeric_limits<size_t>::max(), &element, &error) ==
              AILO_OUT_OF_RANGE &&
          !element);
    CHECK(ailo_llama_sampler_chain_params_array_free(self, &error) == AILO_OK);
    CHECK(ailo_llama_sampler_chain_params_array_free(nullptr, &error) ==
          AILO_OK);
  }
  {
    struct ailo_llama_sampler_data *self = nullptr;
    CHECK(ailo_llama_sampler_data_array_new(2, &self, &error) == AILO_OK &&
          self);
    auto native = reinterpret_cast<struct llama_sampler_data *>(self);
    (void)native;
    {
      struct ailo_ggml_tensor *value =
          reinterpret_cast<struct ailo_ggml_tensor *>(address);
      struct ailo_ggml_tensor *result = {};
      CHECK(ailo_llama_sampler_data_logits_set(self, value, &error) == AILO_OK);
      CHECK(ailo_llama_sampler_data_logits_get(self, &result, &error) ==
            AILO_OK);
      CHECK(result == value);
      CHECK(native->logits == reinterpret_cast<struct ggml_tensor *>(value));
      ++tested;
    }
    {
      struct ailo_ggml_tensor *value =
          reinterpret_cast<struct ailo_ggml_tensor *>(address);
      struct ailo_ggml_tensor *result = {};
      CHECK(ailo_llama_sampler_data_probs_set(self, value, &error) == AILO_OK);
      CHECK(ailo_llama_sampler_data_probs_get(self, &result, &error) ==
            AILO_OK);
      CHECK(result == value);
      CHECK(native->probs == reinterpret_cast<struct ggml_tensor *>(value));
      ++tested;
    }
    {
      struct ailo_ggml_tensor *value =
          reinterpret_cast<struct ailo_ggml_tensor *>(address);
      struct ailo_ggml_tensor *result = {};
      CHECK(ailo_llama_sampler_data_sampled_set(self, value, &error) ==
            AILO_OK);
      CHECK(ailo_llama_sampler_data_sampled_get(self, &result, &error) ==
            AILO_OK);
      CHECK(result == value);
      CHECK(native->sampled == reinterpret_cast<struct ggml_tensor *>(value));
      ++tested;
    }
    {
      struct ailo_ggml_tensor *value =
          reinterpret_cast<struct ailo_ggml_tensor *>(address);
      struct ailo_ggml_tensor *result = {};
      CHECK(ailo_llama_sampler_data_candidates_set(self, value, &error) ==
            AILO_OK);
      CHECK(ailo_llama_sampler_data_candidates_get(self, &result, &error) ==
            AILO_OK);
      CHECK(result == value);
      CHECK(native->candidates ==
            reinterpret_cast<struct ggml_tensor *>(value));
      ++tested;
    }
    struct ailo_llama_sampler_data *element = nullptr;
    CHECK(ailo_llama_sampler_data_array_at(self, 2, 1, &element, &error) ==
          AILO_OK);
    CHECK(reinterpret_cast<struct llama_sampler_data *>(element) == native + 1);
    CHECK(ailo_llama_sampler_data_array_at(self, 2, 2, &element, &error) ==
              AILO_OUT_OF_RANGE &&
          !element);
    CHECK(ailo_llama_sampler_data_array_new(std::numeric_limits<size_t>::max(),
                                            &element,
                                            &error) == AILO_OUT_OF_RANGE &&
          !element);
    CHECK(ailo_llama_sampler_data_array_free(self, &error) == AILO_OK);
    CHECK(ailo_llama_sampler_data_array_free(nullptr, &error) == AILO_OK);
  }
  {
    struct ailo_llama_sampler_i *self = nullptr;
    CHECK(ailo_llama_sampler_i_array_new(2, &self, &error) == AILO_OK && self);
    auto native = reinterpret_cast<struct llama_sampler_i *>(self);
    (void)native;
    struct ailo_llama_sampler_i *element = nullptr;
    CHECK(ailo_llama_sampler_i_array_at(self, 2, 1, &element, &error) ==
          AILO_OK);
    CHECK(reinterpret_cast<struct llama_sampler_i *>(element) == native + 1);
    CHECK(ailo_llama_sampler_i_array_at(self, 2, 2, &element, &error) ==
              AILO_OUT_OF_RANGE &&
          !element);
    CHECK(ailo_llama_sampler_i_array_new(std::numeric_limits<size_t>::max(),
                                         &element,
                                         &error) == AILO_OUT_OF_RANGE &&
          !element);
    CHECK(ailo_llama_sampler_i_array_free(self, &error) == AILO_OK);
    CHECK(ailo_llama_sampler_i_array_free(nullptr, &error) == AILO_OK);
  }
  {
    struct ailo_llama_sampler_seq_config *self = nullptr;
    CHECK(ailo_llama_sampler_seq_config_array_new(2, &self, &error) ==
              AILO_OK &&
          self);
    auto native = reinterpret_cast<struct llama_sampler_seq_config *>(self);
    (void)native;
    {
      ailo_i32 value = 97;
      ailo_i32 result = {};
      CHECK(ailo_llama_sampler_seq_config_seq_id_set(self, value, &error) ==
            AILO_OK);
      CHECK(ailo_llama_sampler_seq_config_seq_id_get(self, &result, &error) ==
            AILO_OK);
      CHECK(result == value);
      CHECK(native->seq_id == static_cast<int32_t>(value));
      ++tested;
    }
    {
      struct ailo_llama_sampler *value =
          reinterpret_cast<struct ailo_llama_sampler *>(address);
      struct ailo_llama_sampler *result = {};
      CHECK(ailo_llama_sampler_seq_config_sampler_set(self, value, &error) ==
            AILO_OK);
      CHECK(ailo_llama_sampler_seq_config_sampler_get(self, &result, &error) ==
            AILO_OK);
      CHECK(result == value);
      CHECK(native->sampler == reinterpret_cast<struct llama_sampler *>(value));
      ++tested;
    }
    struct ailo_llama_sampler_seq_config *element = nullptr;
    CHECK(ailo_llama_sampler_seq_config_array_at(self, 2, 1, &element,
                                                 &error) == AILO_OK);
    CHECK(reinterpret_cast<struct llama_sampler_seq_config *>(element) ==
          native + 1);
    CHECK(ailo_llama_sampler_seq_config_array_at(self, 2, 2, &element,
                                                 &error) == AILO_OUT_OF_RANGE &&
          !element);
    CHECK(ailo_llama_sampler_seq_config_array_new(
              std::numeric_limits<size_t>::max(), &element, &error) ==
              AILO_OUT_OF_RANGE &&
          !element);
    CHECK(ailo_llama_sampler_seq_config_array_free(self, &error) == AILO_OK);
    CHECK(ailo_llama_sampler_seq_config_array_free(nullptr, &error) == AILO_OK);
  }
  {
    struct ailo_llama_token_data *self = nullptr;
    CHECK(ailo_llama_token_data_array_new(2, &self, &error) == AILO_OK && self);
    auto native = reinterpret_cast<struct llama_token_data *>(self);
    (void)native;
    {
      ailo_i32 value = 97;
      ailo_i32 result = {};
      CHECK(ailo_llama_token_data_id_set(self, value, &error) == AILO_OK);
      CHECK(ailo_llama_token_data_id_get(self, &result, &error) == AILO_OK);
      CHECK(result == value);
      CHECK(native->id == static_cast<int32_t>(value));
      ++tested;
    }
    {
      float value = 0.25f;
      float result = {};
      CHECK(ailo_llama_token_data_logit_set(self, value, &error) == AILO_OK);
      CHECK(ailo_llama_token_data_logit_get(self, &result, &error) == AILO_OK);
      CHECK(result == value);
      CHECK(native->logit == static_cast<float>(value));
      ++tested;
    }
    {
      float value = 0.25f;
      float result = {};
      CHECK(ailo_llama_token_data_p_set(self, value, &error) == AILO_OK);
      CHECK(ailo_llama_token_data_p_get(self, &result, &error) == AILO_OK);
      CHECK(result == value);
      CHECK(native->p == static_cast<float>(value));
      ++tested;
    }
    struct ailo_llama_token_data *element = nullptr;
    CHECK(ailo_llama_token_data_array_at(self, 2, 1, &element, &error) ==
          AILO_OK);
    CHECK(reinterpret_cast<struct llama_token_data *>(element) == native + 1);
    CHECK(ailo_llama_token_data_array_at(self, 2, 2, &element, &error) ==
              AILO_OUT_OF_RANGE &&
          !element);
    CHECK(ailo_llama_token_data_array_new(std::numeric_limits<size_t>::max(),
                                          &element,
                                          &error) == AILO_OUT_OF_RANGE &&
          !element);
    CHECK(ailo_llama_token_data_array_free(self, &error) == AILO_OK);
    CHECK(ailo_llama_token_data_array_free(nullptr, &error) == AILO_OK);
  }
  {
    struct ailo_llama_token_data_array *self = nullptr;
    CHECK(ailo_llama_token_data_array_array_new(2, &self, &error) == AILO_OK &&
          self);
    auto native = reinterpret_cast<struct llama_token_data_array *>(self);
    (void)native;
    {
      struct ailo_llama_token_data *value =
          reinterpret_cast<struct ailo_llama_token_data *>(address);
      struct ailo_llama_token_data *result = {};
      CHECK(ailo_llama_token_data_array_data_set(self, value, &error) ==
            AILO_OK);
      CHECK(ailo_llama_token_data_array_data_get(self, &result, &error) ==
            AILO_OK);
      CHECK(result == value);
      CHECK(native->data == reinterpret_cast<struct llama_token_data *>(value));
      ++tested;
    }
    {
      size_t value = 97;
      size_t result = {};
      CHECK(ailo_llama_token_data_array_size_set(self, value, &error) ==
            AILO_OK);
      CHECK(ailo_llama_token_data_array_size_get(self, &result, &error) ==
            AILO_OK);
      CHECK(result == value);
      CHECK(native->size == static_cast<size_t>(value));
      ++tested;
    }
    {
      ailo_i64 value = ailo_i64{0x80000000U, 17};
      ailo_i64 result = {};
      CHECK(ailo_llama_token_data_array_selected_set(self, value, &error) ==
            AILO_OK);
      CHECK(ailo_llama_token_data_array_selected_get(self, &result, &error) ==
            AILO_OK);
      CHECK(result.hi == value.hi && result.lo == value.lo);
      CHECK(native->selected == ailo::detail::to_native(value));
      ++tested;
    }
    {
      ailo_bool value = 17;
      ailo_bool result = {};
      CHECK(ailo_llama_token_data_array_sorted_set(self, value, &error) ==
            AILO_OK);
      CHECK(ailo_llama_token_data_array_sorted_get(self, &result, &error) ==
            AILO_OK);
      CHECK(result == 1 && native->sorted);
      ++tested;
    }
    struct ailo_llama_token_data_array *element = nullptr;
    CHECK(ailo_llama_token_data_array_array_at(self, 2, 1, &element, &error) ==
          AILO_OK);
    CHECK(reinterpret_cast<struct llama_token_data_array *>(element) ==
          native + 1);
    CHECK(ailo_llama_token_data_array_array_at(self, 2, 2, &element, &error) ==
              AILO_OUT_OF_RANGE &&
          !element);
    CHECK(ailo_llama_token_data_array_array_new(
              std::numeric_limits<size_t>::max(), &element, &error) ==
              AILO_OUT_OF_RANGE &&
          !element);
    CHECK(ailo_llama_token_data_array_array_free(self, &error) == AILO_OK);
    CHECK(ailo_llama_token_data_array_array_free(nullptr, &error) == AILO_OK);
  }
  {
    struct ailo_mtmd_caps *self = nullptr;
    CHECK(ailo_mtmd_caps_array_new(2, &self, &error) == AILO_OK && self);
    auto native = reinterpret_cast<struct mtmd_caps *>(self);
    (void)native;
    {
      ailo_bool value = 17;
      ailo_bool result = {};
      CHECK(ailo_mtmd_caps_inp_vision_set(self, value, &error) == AILO_OK);
      CHECK(ailo_mtmd_caps_inp_vision_get(self, &result, &error) == AILO_OK);
      CHECK(result == 1 && native->inp_vision);
      ++tested;
    }
    {
      ailo_bool value = 17;
      ailo_bool result = {};
      CHECK(ailo_mtmd_caps_inp_audio_set(self, value, &error) == AILO_OK);
      CHECK(ailo_mtmd_caps_inp_audio_get(self, &result, &error) == AILO_OK);
      CHECK(result == 1 && native->inp_audio);
      ++tested;
    }
    struct ailo_mtmd_caps *element = nullptr;
    CHECK(ailo_mtmd_caps_array_at(self, 2, 1, &element, &error) == AILO_OK);
    CHECK(reinterpret_cast<struct mtmd_caps *>(element) == native + 1);
    CHECK(ailo_mtmd_caps_array_at(self, 2, 2, &element, &error) ==
              AILO_OUT_OF_RANGE &&
          !element);
    CHECK(ailo_mtmd_caps_array_new(std::numeric_limits<size_t>::max(), &element,
                                   &error) == AILO_OUT_OF_RANGE &&
          !element);
    CHECK(ailo_mtmd_caps_array_free(self, &error) == AILO_OK);
    CHECK(ailo_mtmd_caps_array_free(nullptr, &error) == AILO_OK);
  }
  {
    struct ailo_mtmd_context_params *self = nullptr;
    CHECK(ailo_mtmd_context_params_array_new(2, &self, &error) == AILO_OK &&
          self);
    auto native = reinterpret_cast<struct mtmd_context_params *>(self);
    (void)native;
    {
      ailo_bool value = 17;
      ailo_bool result = {};
      CHECK(ailo_mtmd_context_params_use_gpu_set(self, value, &error) ==
            AILO_OK);
      CHECK(ailo_mtmd_context_params_use_gpu_get(self, &result, &error) ==
            AILO_OK);
      CHECK(result == 1 && native->use_gpu);
      ++tested;
    }
    {
      struct ailo_ggml_backend_device *value =
          reinterpret_cast<struct ailo_ggml_backend_device *>(address);
      struct ailo_ggml_backend_device *result = {};
      CHECK(ailo_mtmd_context_params_device_set(self, value, &error) ==
            AILO_OK);
      CHECK(ailo_mtmd_context_params_device_get(self, &result, &error) ==
            AILO_OK);
      CHECK(result == value);
      CHECK(native->device ==
            reinterpret_cast<struct ggml_backend_device *>(value));
      ++tested;
    }
    {
      ailo_bool value = 17;
      ailo_bool result = {};
      CHECK(ailo_mtmd_context_params_print_timings_set(self, value, &error) ==
            AILO_OK);
      CHECK(ailo_mtmd_context_params_print_timings_get(self, &result, &error) ==
            AILO_OK);
      CHECK(result == 1 && native->print_timings);
      ++tested;
    }
    {
      int value = 97;
      int result = {};
      CHECK(ailo_mtmd_context_params_n_threads_set(self, value, &error) ==
            AILO_OK);
      CHECK(ailo_mtmd_context_params_n_threads_get(self, &result, &error) ==
            AILO_OK);
      CHECK(result == value);
      CHECK(native->n_threads == static_cast<int>(value));
      ++tested;
    }
    {
      const char *value = reinterpret_cast<const char *>(address);
      const char *result = {};
      CHECK(ailo_mtmd_context_params_image_marker_set(self, value, &error) ==
            AILO_OK);
      CHECK(ailo_mtmd_context_params_image_marker_get(self, &result, &error) ==
            AILO_OK);
      CHECK(result == value);
      CHECK(native->image_marker == static_cast<const char *>(value));
      ++tested;
    }
    {
      const char *value = reinterpret_cast<const char *>(address);
      const char *result = {};
      CHECK(ailo_mtmd_context_params_media_marker_set(self, value, &error) ==
            AILO_OK);
      CHECK(ailo_mtmd_context_params_media_marker_get(self, &result, &error) ==
            AILO_OK);
      CHECK(result == value);
      CHECK(native->media_marker == static_cast<const char *>(value));
      ++tested;
    }
    {
      enum ailo_llama_flash_attn_type value = AILO_LLAMA_FLASH_ATTN_TYPE_AUTO;
      enum ailo_llama_flash_attn_type result = {};
      CHECK(ailo_mtmd_context_params_flash_attn_type_set(self, value, &error) ==
            AILO_OK);
      CHECK(ailo_mtmd_context_params_flash_attn_type_get(self, &result,
                                                         &error) == AILO_OK);
      CHECK(result == value);
      CHECK(native->flash_attn_type ==
            static_cast<enum llama_flash_attn_type>(value));
      ++tested;
    }
    {
      ailo_bool value = 17;
      ailo_bool result = {};
      CHECK(ailo_mtmd_context_params_warmup_set(self, value, &error) ==
            AILO_OK);
      CHECK(ailo_mtmd_context_params_warmup_get(self, &result, &error) ==
            AILO_OK);
      CHECK(result == 1 && native->warmup);
      ++tested;
    }
    {
      int value = 97;
      int result = {};
      CHECK(ailo_mtmd_context_params_image_min_tokens_set(self, value,
                                                          &error) == AILO_OK);
      CHECK(ailo_mtmd_context_params_image_min_tokens_get(self, &result,
                                                          &error) == AILO_OK);
      CHECK(result == value);
      CHECK(native->image_min_tokens == static_cast<int>(value));
      ++tested;
    }
    {
      int value = 97;
      int result = {};
      CHECK(ailo_mtmd_context_params_image_max_tokens_set(self, value,
                                                          &error) == AILO_OK);
      CHECK(ailo_mtmd_context_params_image_max_tokens_get(self, &result,
                                                          &error) == AILO_OK);
      CHECK(result == value);
      CHECK(native->image_max_tokens == static_cast<int>(value));
      ++tested;
    }
    {
      void *value = reinterpret_cast<void *>(address);
      void *result = {};
      CHECK(ailo_mtmd_context_params_cb_eval_user_data_set(self, value,
                                                           &error) == AILO_OK);
      CHECK(ailo_mtmd_context_params_cb_eval_user_data_get(self, &result,
                                                           &error) == AILO_OK);
      CHECK(result == value);
      CHECK(native->cb_eval_user_data == static_cast<void *>(value));
      ++tested;
    }
    {
      ailo_i32 value = 97;
      ailo_i32 result = {};
      CHECK(ailo_mtmd_context_params_batch_max_tokens_set(self, value,
                                                          &error) == AILO_OK);
      CHECK(ailo_mtmd_context_params_batch_max_tokens_get(self, &result,
                                                          &error) == AILO_OK);
      CHECK(result == value);
      CHECK(native->batch_max_tokens == static_cast<int32_t>(value));
      ++tested;
    }
    {
      void *value = reinterpret_cast<void *>(address);
      void *result = {};
      CHECK(ailo_mtmd_context_params_progress_callback_user_data_set(
                self, value, &error) == AILO_OK);
      CHECK(ailo_mtmd_context_params_progress_callback_user_data_get(
                self, &result, &error) == AILO_OK);
      CHECK(result == value);
      CHECK(native->progress_callback_user_data == static_cast<void *>(value));
      ++tested;
    }
    struct ailo_mtmd_context_params *element = nullptr;
    CHECK(ailo_mtmd_context_params_array_at(self, 2, 1, &element, &error) ==
          AILO_OK);
    CHECK(reinterpret_cast<struct mtmd_context_params *>(element) ==
          native + 1);
    CHECK(ailo_mtmd_context_params_array_at(self, 2, 2, &element, &error) ==
              AILO_OUT_OF_RANGE &&
          !element);
    CHECK(ailo_mtmd_context_params_array_new(std::numeric_limits<size_t>::max(),
                                             &element,
                                             &error) == AILO_OUT_OF_RANGE &&
          !element);
    CHECK(ailo_mtmd_context_params_array_free(self, &error) == AILO_OK);
    CHECK(ailo_mtmd_context_params_array_free(nullptr, &error) == AILO_OK);
  }
  {
    struct ailo_mtmd_decoder_pos *self = nullptr;
    CHECK(ailo_mtmd_decoder_pos_array_new(2, &self, &error) == AILO_OK && self);
    auto native = reinterpret_cast<struct mtmd_decoder_pos *>(self);
    (void)native;
    {
      ailo_u32 value = 97;
      ailo_u32 result = {};
      CHECK(ailo_mtmd_decoder_pos_t_set(self, value, &error) == AILO_OK);
      CHECK(ailo_mtmd_decoder_pos_t_get(self, &result, &error) == AILO_OK);
      CHECK(result == value);
      CHECK(native->t == static_cast<uint32_t>(value));
      ++tested;
    }
    {
      ailo_u32 value = 97;
      ailo_u32 result = {};
      CHECK(ailo_mtmd_decoder_pos_x_set(self, value, &error) == AILO_OK);
      CHECK(ailo_mtmd_decoder_pos_x_get(self, &result, &error) == AILO_OK);
      CHECK(result == value);
      CHECK(native->x == static_cast<uint32_t>(value));
      ++tested;
    }
    {
      ailo_u32 value = 97;
      ailo_u32 result = {};
      CHECK(ailo_mtmd_decoder_pos_y_set(self, value, &error) == AILO_OK);
      CHECK(ailo_mtmd_decoder_pos_y_get(self, &result, &error) == AILO_OK);
      CHECK(result == value);
      CHECK(native->y == static_cast<uint32_t>(value));
      ++tested;
    }
    {
      ailo_u32 value = 97;
      ailo_u32 result = {};
      CHECK(ailo_mtmd_decoder_pos_z_set(self, value, &error) == AILO_OK);
      CHECK(ailo_mtmd_decoder_pos_z_get(self, &result, &error) == AILO_OK);
      CHECK(result == value);
      CHECK(native->z == static_cast<uint32_t>(value));
      ++tested;
    }
    struct ailo_mtmd_decoder_pos *element = nullptr;
    CHECK(ailo_mtmd_decoder_pos_array_at(self, 2, 1, &element, &error) ==
          AILO_OK);
    CHECK(reinterpret_cast<struct mtmd_decoder_pos *>(element) == native + 1);
    CHECK(ailo_mtmd_decoder_pos_array_at(self, 2, 2, &element, &error) ==
              AILO_OUT_OF_RANGE &&
          !element);
    CHECK(ailo_mtmd_decoder_pos_array_new(std::numeric_limits<size_t>::max(),
                                          &element,
                                          &error) == AILO_OUT_OF_RANGE &&
          !element);
    CHECK(ailo_mtmd_decoder_pos_array_free(self, &error) == AILO_OK);
    CHECK(ailo_mtmd_decoder_pos_array_free(nullptr, &error) == AILO_OK);
  }
  {
    struct ailo_mtmd_gen_audio_info *self = nullptr;
    CHECK(ailo_mtmd_gen_audio_info_array_new(2, &self, &error) == AILO_OK &&
          self);
    auto native = reinterpret_cast<struct mtmd_gen_audio_info *>(self);
    (void)native;
    {
      enum ailo_mtmd_gen_audio_type value = AILO_MTMD_GEN_AUDIO_TYPE_NONE;
      enum ailo_mtmd_gen_audio_type result = {};
      CHECK(ailo_mtmd_gen_audio_info_type_set(self, value, &error) == AILO_OK);
      CHECK(ailo_mtmd_gen_audio_info_type_get(self, &result, &error) ==
            AILO_OK);
      CHECK(result == value);
      CHECK(native->type == static_cast<enum mtmd_gen_audio_type>(value));
      ++tested;
    }
    {
      ailo_i32 value = 97;
      ailo_i32 result = {};
      CHECK(ailo_mtmd_gen_audio_info_sample_rate_set(self, value, &error) ==
            AILO_OK);
      CHECK(ailo_mtmd_gen_audio_info_sample_rate_get(self, &result, &error) ==
            AILO_OK);
      CHECK(result == value);
      CHECK(native->sample_rate == static_cast<int32_t>(value));
      ++tested;
    }
    {
      const char *value = reinterpret_cast<const char *>(address);
      const char *result = {};
      CHECK(ailo_mtmd_gen_audio_info_model_variant_set(self, value, &error) ==
            AILO_OK);
      CHECK(ailo_mtmd_gen_audio_info_model_variant_get(self, &result, &error) ==
            AILO_OK);
      CHECK(result == value);
      CHECK(native->model_variant == static_cast<const char *>(value));
      ++tested;
    }
    struct ailo_mtmd_gen_audio_info *element = nullptr;
    CHECK(ailo_mtmd_gen_audio_info_array_at(self, 2, 1, &element, &error) ==
          AILO_OK);
    CHECK(reinterpret_cast<struct mtmd_gen_audio_info *>(element) ==
          native + 1);
    CHECK(ailo_mtmd_gen_audio_info_array_at(self, 2, 2, &element, &error) ==
              AILO_OUT_OF_RANGE &&
          !element);
    CHECK(ailo_mtmd_gen_audio_info_array_new(std::numeric_limits<size_t>::max(),
                                             &element,
                                             &error) == AILO_OUT_OF_RANGE &&
          !element);
    CHECK(ailo_mtmd_gen_audio_info_array_free(self, &error) == AILO_OK);
    CHECK(ailo_mtmd_gen_audio_info_array_free(nullptr, &error) == AILO_OK);
  }
  {
    struct ailo_mtmd_gen_inp *self = nullptr;
    CHECK(ailo_mtmd_gen_inp_array_new(2, &self, &error) == AILO_OK && self);
    auto native = reinterpret_cast<struct mtmd_gen_inp *>(self);
    (void)native;
    {
      enum ailo_mtmd_gen_process_type value =
          AILO_MTMD_GEN_PROCESS_TYPE_GEN_CODE;
      enum ailo_mtmd_gen_process_type result = {};
      CHECK(ailo_mtmd_gen_inp_type_set(self, value, &error) == AILO_OK);
      CHECK(ailo_mtmd_gen_inp_type_get(self, &result, &error) == AILO_OK);
      CHECK(result == value);
      CHECK(native->type == static_cast<enum mtmd_gen_process_type>(value));
      ++tested;
    }
    {
      ailo_i32 value = 97;
      ailo_i32 result = {};
      CHECK(ailo_mtmd_gen_inp_code0_set(self, value, &error) == AILO_OK);
      CHECK(ailo_mtmd_gen_inp_code0_get(self, &result, &error) == AILO_OK);
      CHECK(result == value);
      CHECK(native->code0 == static_cast<int32_t>(value));
      ++tested;
    }
    {
      float *value = reinterpret_cast<float *>(address);
      float *result = {};
      CHECK(ailo_mtmd_gen_inp_embd_set(self, value, &error) == AILO_OK);
      CHECK(ailo_mtmd_gen_inp_embd_get(self, &result, &error) == AILO_OK);
      CHECK(result == value);
      CHECK(native->embd == static_cast<float *>(value));
      ++tested;
    }
    {
      ailo_i32 value = 97;
      ailo_i32 result = {};
      CHECK(ailo_mtmd_gen_inp_top_k_set(self, value, &error) == AILO_OK);
      CHECK(ailo_mtmd_gen_inp_top_k_get(self, &result, &error) == AILO_OK);
      CHECK(result == value);
      CHECK(native->top_k == static_cast<int32_t>(value));
      ++tested;
    }
    {
      float value = 0.25f;
      float result = {};
      CHECK(ailo_mtmd_gen_inp_top_p_set(self, value, &error) == AILO_OK);
      CHECK(ailo_mtmd_gen_inp_top_p_get(self, &result, &error) == AILO_OK);
      CHECK(result == value);
      CHECK(native->top_p == static_cast<float>(value));
      ++tested;
    }
    {
      ailo_u32 value = 97;
      ailo_u32 result = {};
      CHECK(ailo_mtmd_gen_inp_seed_set(self, value, &error) == AILO_OK);
      CHECK(ailo_mtmd_gen_inp_seed_get(self, &result, &error) == AILO_OK);
      CHECK(result == value);
      CHECK(native->seed == static_cast<uint32_t>(value));
      ++tested;
    }
    {
      float value = 0.25f;
      float result = {};
      CHECK(ailo_mtmd_gen_inp_temp_set(self, value, &error) == AILO_OK);
      CHECK(ailo_mtmd_gen_inp_temp_get(self, &result, &error) == AILO_OK);
      CHECK(result == value);
      CHECK(native->temp == static_cast<float>(value));
      ++tested;
    }
    {
      ailo_i32 *value = reinterpret_cast<ailo_i32 *>(address);
      ailo_i32 *result = {};
      CHECK(ailo_mtmd_gen_inp_codes_set(self, value, &error) == AILO_OK);
      CHECK(ailo_mtmd_gen_inp_codes_get(self, &result, &error) == AILO_OK);
      CHECK(result == value);
      CHECK(native->codes == static_cast<int32_t *>(value));
      ++tested;
    }
    {
      size_t value = 97;
      size_t result = {};
      CHECK(ailo_mtmd_gen_inp_n_codes_set(self, value, &error) == AILO_OK);
      CHECK(ailo_mtmd_gen_inp_n_codes_get(self, &result, &error) == AILO_OK);
      CHECK(result == value);
      CHECK(native->n_codes == static_cast<size_t>(value));
      ++tested;
    }
    {
      const float *value = reinterpret_cast<const float *>(address);
      const float *result = {};
      CHECK(ailo_mtmd_gen_inp_feats_set(self, value, &error) == AILO_OK);
      CHECK(ailo_mtmd_gen_inp_feats_get(self, &result, &error) == AILO_OK);
      CHECK(result == value);
      CHECK(native->feats == static_cast<const float *>(value));
      ++tested;
    }
    {
      size_t value = 97;
      size_t result = {};
      CHECK(ailo_mtmd_gen_inp_n_feats_set(self, value, &error) == AILO_OK);
      CHECK(ailo_mtmd_gen_inp_n_feats_get(self, &result, &error) == AILO_OK);
      CHECK(result == value);
      CHECK(native->n_feats == static_cast<size_t>(value));
      ++tested;
    }
    {
      const char *value = reinterpret_cast<const char *>(address);
      const char *result = {};
      CHECK(ailo_mtmd_gen_inp_state_data_set(self, value, &error) == AILO_OK);
      CHECK(ailo_mtmd_gen_inp_state_data_get(self, &result, &error) == AILO_OK);
      CHECK(result == value);
      CHECK(native->state_data == static_cast<const char *>(value));
      ++tested;
    }
    {
      size_t value = 97;
      size_t result = {};
      CHECK(ailo_mtmd_gen_inp_state_size_set(self, value, &error) == AILO_OK);
      CHECK(ailo_mtmd_gen_inp_state_size_get(self, &result, &error) == AILO_OK);
      CHECK(result == value);
      CHECK(native->state_size == static_cast<size_t>(value));
      ++tested;
    }
    struct ailo_mtmd_gen_inp *element = nullptr;
    CHECK(ailo_mtmd_gen_inp_array_at(self, 2, 1, &element, &error) == AILO_OK);
    CHECK(reinterpret_cast<struct mtmd_gen_inp *>(element) == native + 1);
    CHECK(ailo_mtmd_gen_inp_array_at(self, 2, 2, &element, &error) ==
              AILO_OUT_OF_RANGE &&
          !element);
    CHECK(ailo_mtmd_gen_inp_array_new(std::numeric_limits<size_t>::max(),
                                      &element, &error) == AILO_OUT_OF_RANGE &&
          !element);
    CHECK(ailo_mtmd_gen_inp_array_free(self, &error) == AILO_OK);
    CHECK(ailo_mtmd_gen_inp_array_free(nullptr, &error) == AILO_OK);
  }
  {
    struct ailo_mtmd_gen_out *self = nullptr;
    CHECK(ailo_mtmd_gen_out_array_new(2, &self, &error) == AILO_OK && self);
    auto native = reinterpret_cast<struct mtmd_gen_out *>(self);
    (void)native;
    {
      const ailo_i32 *value = reinterpret_cast<const ailo_i32 *>(address);
      const ailo_i32 *result = {};
      CHECK(ailo_mtmd_gen_out_codes_set(self, value, &error) == AILO_OK);
      CHECK(ailo_mtmd_gen_out_codes_get(self, &result, &error) == AILO_OK);
      CHECK(result == value);
      CHECK(native->codes == static_cast<const int32_t *>(value));
      ++tested;
    }
    {
      size_t value = 97;
      size_t result = {};
      CHECK(ailo_mtmd_gen_out_n_codes_set(self, value, &error) == AILO_OK);
      CHECK(ailo_mtmd_gen_out_n_codes_get(self, &result, &error) == AILO_OK);
      CHECK(result == value);
      CHECK(native->n_codes == static_cast<size_t>(value));
      ++tested;
    }
    {
      const float *value = reinterpret_cast<const float *>(address);
      const float *result = {};
      CHECK(ailo_mtmd_gen_out_feats_set(self, value, &error) == AILO_OK);
      CHECK(ailo_mtmd_gen_out_feats_get(self, &result, &error) == AILO_OK);
      CHECK(result == value);
      CHECK(native->feats == static_cast<const float *>(value));
      ++tested;
    }
    {
      size_t value = 97;
      size_t result = {};
      CHECK(ailo_mtmd_gen_out_n_feats_set(self, value, &error) == AILO_OK);
      CHECK(ailo_mtmd_gen_out_n_feats_get(self, &result, &error) == AILO_OK);
      CHECK(result == value);
      CHECK(native->n_feats == static_cast<size_t>(value));
      ++tested;
    }
    {
      const float *value = reinterpret_cast<const float *>(address);
      const float *result = {};
      CHECK(ailo_mtmd_gen_out_embd_set(self, value, &error) == AILO_OK);
      CHECK(ailo_mtmd_gen_out_embd_get(self, &result, &error) == AILO_OK);
      CHECK(result == value);
      CHECK(native->embd == static_cast<const float *>(value));
      ++tested;
    }
    {
      ailo_bool value = 17;
      ailo_bool result = {};
      CHECK(ailo_mtmd_gen_out_is_eos_set(self, value, &error) == AILO_OK);
      CHECK(ailo_mtmd_gen_out_is_eos_get(self, &result, &error) == AILO_OK);
      CHECK(result == 1 && native->is_eos);
      ++tested;
    }
    {
      const float *value = reinterpret_cast<const float *>(address);
      const float *result = {};
      CHECK(ailo_mtmd_gen_out_audio_set(self, value, &error) == AILO_OK);
      CHECK(ailo_mtmd_gen_out_audio_get(self, &result, &error) == AILO_OK);
      CHECK(result == value);
      CHECK(native->audio == static_cast<const float *>(value));
      ++tested;
    }
    {
      size_t value = 97;
      size_t result = {};
      CHECK(ailo_mtmd_gen_out_n_samples_set(self, value, &error) == AILO_OK);
      CHECK(ailo_mtmd_gen_out_n_samples_get(self, &result, &error) == AILO_OK);
      CHECK(result == value);
      CHECK(native->n_samples == static_cast<size_t>(value));
      ++tested;
    }
    {
      const char *value = reinterpret_cast<const char *>(address);
      const char *result = {};
      CHECK(ailo_mtmd_gen_out_state_data_set(self, value, &error) == AILO_OK);
      CHECK(ailo_mtmd_gen_out_state_data_get(self, &result, &error) == AILO_OK);
      CHECK(result == value);
      CHECK(native->state_data == static_cast<const char *>(value));
      ++tested;
    }
    {
      size_t value = 97;
      size_t result = {};
      CHECK(ailo_mtmd_gen_out_state_size_set(self, value, &error) == AILO_OK);
      CHECK(ailo_mtmd_gen_out_state_size_get(self, &result, &error) == AILO_OK);
      CHECK(result == value);
      CHECK(native->state_size == static_cast<size_t>(value));
      ++tested;
    }
    struct ailo_mtmd_gen_out *element = nullptr;
    CHECK(ailo_mtmd_gen_out_array_at(self, 2, 1, &element, &error) == AILO_OK);
    CHECK(reinterpret_cast<struct mtmd_gen_out *>(element) == native + 1);
    CHECK(ailo_mtmd_gen_out_array_at(self, 2, 2, &element, &error) ==
              AILO_OUT_OF_RANGE &&
          !element);
    CHECK(ailo_mtmd_gen_out_array_new(std::numeric_limits<size_t>::max(),
                                      &element, &error) == AILO_OUT_OF_RANGE &&
          !element);
    CHECK(ailo_mtmd_gen_out_array_free(self, &error) == AILO_OK);
    CHECK(ailo_mtmd_gen_out_array_free(nullptr, &error) == AILO_OK);
  }
  {
    struct ailo_mtmd_helper_bitmap_wrapper *self = nullptr;
    CHECK(ailo_mtmd_helper_bitmap_wrapper_array_new(2, &self, &error) ==
              AILO_OK &&
          self);
    auto native = reinterpret_cast<struct mtmd_helper_bitmap_wrapper *>(self);
    (void)native;
    {
      struct ailo_mtmd_bitmap *value =
          reinterpret_cast<struct ailo_mtmd_bitmap *>(address);
      struct ailo_mtmd_bitmap *result = {};
      CHECK(ailo_mtmd_helper_bitmap_wrapper_bitmap_set(self, value, &error) ==
            AILO_OK);
      CHECK(ailo_mtmd_helper_bitmap_wrapper_bitmap_get(self, &result, &error) ==
            AILO_OK);
      CHECK(result == value);
      CHECK(native->bitmap == reinterpret_cast<struct mtmd_bitmap *>(value));
      ++tested;
    }
    {
      struct ailo_mtmd_helper_video *value =
          reinterpret_cast<struct ailo_mtmd_helper_video *>(address);
      struct ailo_mtmd_helper_video *result = {};
      CHECK(ailo_mtmd_helper_bitmap_wrapper_video_ctx_set(self, value,
                                                          &error) == AILO_OK);
      CHECK(ailo_mtmd_helper_bitmap_wrapper_video_ctx_get(self, &result,
                                                          &error) == AILO_OK);
      CHECK(result == value);
      CHECK(native->video_ctx ==
            reinterpret_cast<struct mtmd_helper_video *>(value));
      ++tested;
    }
    struct ailo_mtmd_helper_bitmap_wrapper *element = nullptr;
    CHECK(ailo_mtmd_helper_bitmap_wrapper_array_at(self, 2, 1, &element,
                                                   &error) == AILO_OK);
    CHECK(reinterpret_cast<struct mtmd_helper_bitmap_wrapper *>(element) ==
          native + 1);
    CHECK(ailo_mtmd_helper_bitmap_wrapper_array_at(
              self, 2, 2, &element, &error) == AILO_OUT_OF_RANGE &&
          !element);
    CHECK(ailo_mtmd_helper_bitmap_wrapper_array_new(
              std::numeric_limits<size_t>::max(), &element, &error) ==
              AILO_OUT_OF_RANGE &&
          !element);
    CHECK(ailo_mtmd_helper_bitmap_wrapper_array_free(self, &error) == AILO_OK);
    CHECK(ailo_mtmd_helper_bitmap_wrapper_array_free(nullptr, &error) ==
          AILO_OK);
  }
  {
    struct ailo_mtmd_helper_embd_batch *self = nullptr;
    CHECK(ailo_mtmd_helper_embd_batch_array_new(2, &self, &error) == AILO_OK &&
          self);
    auto native = reinterpret_cast<struct mtmd_helper_embd_batch *>(self);
    (void)native;
    {
      ailo_i32 value = 97;
      ailo_i32 result = {};
      CHECK(ailo_mtmd_helper_embd_batch_n_tokens_set(self, value, &error) ==
            AILO_OK);
      CHECK(ailo_mtmd_helper_embd_batch_n_tokens_get(self, &result, &error) ==
            AILO_OK);
      CHECK(result == value);
      CHECK(native->n_tokens == static_cast<int32_t>(value));
      ++tested;
    }
    {
      const float *value = reinterpret_cast<const float *>(address);
      const float *result = {};
      CHECK(ailo_mtmd_helper_embd_batch_embd_set(self, value, &error) ==
            AILO_OK);
      CHECK(ailo_mtmd_helper_embd_batch_embd_get(self, &result, &error) ==
            AILO_OK);
      CHECK(result == value);
      CHECK(native->embd == static_cast<const float *>(value));
      ++tested;
    }
    {
      ailo_i32 value = 97;
      ailo_i32 result = {};
      CHECK(ailo_mtmd_helper_embd_batch_n_embd_set(self, value, &error) ==
            AILO_OK);
      CHECK(ailo_mtmd_helper_embd_batch_n_embd_get(self, &result, &error) ==
            AILO_OK);
      CHECK(result == value);
      CHECK(native->n_embd == static_cast<int32_t>(value));
      ++tested;
    }
    {
      const ailo_i32 *value = reinterpret_cast<const ailo_i32 *>(address);
      const ailo_i32 *result = {};
      CHECK(ailo_mtmd_helper_embd_batch_pos_set(self, value, &error) ==
            AILO_OK);
      CHECK(ailo_mtmd_helper_embd_batch_pos_get(self, &result, &error) ==
            AILO_OK);
      CHECK(result == value);
      CHECK(native->pos == static_cast<const int32_t *>(value));
      ++tested;
    }
    {
      ailo_i32 value = 97;
      ailo_i32 result = {};
      CHECK(ailo_mtmd_helper_embd_batch_n_pos_set(self, value, &error) ==
            AILO_OK);
      CHECK(ailo_mtmd_helper_embd_batch_n_pos_get(self, &result, &error) ==
            AILO_OK);
      CHECK(result == value);
      CHECK(native->n_pos == static_cast<int32_t>(value));
      ++tested;
    }
    {
      ailo_i32 value = 97;
      ailo_i32 result = {};
      CHECK(ailo_mtmd_helper_embd_batch_seq_id_set(self, value, &error) ==
            AILO_OK);
      CHECK(ailo_mtmd_helper_embd_batch_seq_id_get(self, &result, &error) ==
            AILO_OK);
      CHECK(result == value);
      CHECK(native->seq_id == static_cast<int32_t>(value));
      ++tested;
    }
    struct ailo_mtmd_helper_embd_batch *element = nullptr;
    CHECK(ailo_mtmd_helper_embd_batch_array_at(self, 2, 1, &element, &error) ==
          AILO_OK);
    CHECK(reinterpret_cast<struct mtmd_helper_embd_batch *>(element) ==
          native + 1);
    CHECK(ailo_mtmd_helper_embd_batch_array_at(self, 2, 2, &element, &error) ==
              AILO_OUT_OF_RANGE &&
          !element);
    CHECK(ailo_mtmd_helper_embd_batch_array_new(
              std::numeric_limits<size_t>::max(), &element, &error) ==
              AILO_OUT_OF_RANGE &&
          !element);
    CHECK(ailo_mtmd_helper_embd_batch_array_free(self, &error) == AILO_OK);
    CHECK(ailo_mtmd_helper_embd_batch_array_free(nullptr, &error) == AILO_OK);
  }
  {
    struct ailo_mtmd_helper_gen_audio_inp *self = nullptr;
    CHECK(ailo_mtmd_helper_gen_audio_inp_array_new(2, &self, &error) ==
              AILO_OK &&
          self);
    auto native = reinterpret_cast<struct mtmd_helper_gen_audio_inp *>(self);
    (void)native;
    {
      ailo_i32 value = 97;
      ailo_i32 result = {};
      CHECK(ailo_mtmd_helper_gen_audio_inp_seq_id_set(self, value, &error) ==
            AILO_OK);
      CHECK(ailo_mtmd_helper_gen_audio_inp_seq_id_get(self, &result, &error) ==
            AILO_OK);
      CHECK(result == value);
      CHECK(native->seq_id == static_cast<int32_t>(value));
      ++tested;
    }
    {
      const char *value = reinterpret_cast<const char *>(address);
      const char *result = {};
      CHECK(ailo_mtmd_helper_gen_audio_inp_prompt_set(self, value, &error) ==
            AILO_OK);
      CHECK(ailo_mtmd_helper_gen_audio_inp_prompt_get(self, &result, &error) ==
            AILO_OK);
      CHECK(result == value);
      CHECK(native->prompt == static_cast<const char *>(value));
      ++tested;
    }
    {
      size_t value = 97;
      size_t result = {};
      CHECK(ailo_mtmd_helper_gen_audio_inp_prompt_len_set(self, value,
                                                          &error) == AILO_OK);
      CHECK(ailo_mtmd_helper_gen_audio_inp_prompt_len_get(self, &result,
                                                          &error) == AILO_OK);
      CHECK(result == value);
      CHECK(native->prompt_len == static_cast<size_t>(value));
      ++tested;
    }
    {
      struct ailo_mtmd_bitmap *value =
          reinterpret_cast<struct ailo_mtmd_bitmap *>(address);
      struct ailo_mtmd_bitmap *result = {};
      CHECK(ailo_mtmd_helper_gen_audio_inp_speaker_ref_set(self, value,
                                                           &error) == AILO_OK);
      CHECK(ailo_mtmd_helper_gen_audio_inp_speaker_ref_get(self, &result,
                                                           &error) == AILO_OK);
      CHECK(result == value);
      CHECK(native->speaker_ref ==
            reinterpret_cast<struct mtmd_bitmap *>(value));
      ++tested;
    }
    {
      const char *value = reinterpret_cast<const char *>(address);
      const char *result = {};
      CHECK(ailo_mtmd_helper_gen_audio_inp_lang_set(self, value, &error) ==
            AILO_OK);
      CHECK(ailo_mtmd_helper_gen_audio_inp_lang_get(self, &result, &error) ==
            AILO_OK);
      CHECK(result == value);
      CHECK(native->lang == static_cast<const char *>(value));
      ++tested;
    }
    {
      ailo_i32 value = 97;
      ailo_i32 result = {};
      CHECK(ailo_mtmd_helper_gen_audio_inp_top_k_set(self, value, &error) ==
            AILO_OK);
      CHECK(ailo_mtmd_helper_gen_audio_inp_top_k_get(self, &result, &error) ==
            AILO_OK);
      CHECK(result == value);
      CHECK(native->top_k == static_cast<int32_t>(value));
      ++tested;
    }
    {
      float value = 0.25f;
      float result = {};
      CHECK(ailo_mtmd_helper_gen_audio_inp_top_p_set(self, value, &error) ==
            AILO_OK);
      CHECK(ailo_mtmd_helper_gen_audio_inp_top_p_get(self, &result, &error) ==
            AILO_OK);
      CHECK(result == value);
      CHECK(native->top_p == static_cast<float>(value));
      ++tested;
    }
    {
      ailo_u32 value = 97;
      ailo_u32 result = {};
      CHECK(ailo_mtmd_helper_gen_audio_inp_seed_set(self, value, &error) ==
            AILO_OK);
      CHECK(ailo_mtmd_helper_gen_audio_inp_seed_get(self, &result, &error) ==
            AILO_OK);
      CHECK(result == value);
      CHECK(native->seed == static_cast<uint32_t>(value));
      ++tested;
    }
    {
      enum ailo_mtmd_helper_gen_audio_outtype value =
          AILO_MTMD_HELPER_GEN_AUDIO_OUTTYPE_PCM;
      enum ailo_mtmd_helper_gen_audio_outtype result = {};
      CHECK(ailo_mtmd_helper_gen_audio_inp_out_type_set(self, value, &error) ==
            AILO_OK);
      CHECK(ailo_mtmd_helper_gen_audio_inp_out_type_get(self, &result,
                                                        &error) == AILO_OK);
      CHECK(result == value);
      CHECK(native->out_type ==
            static_cast<enum mtmd_helper_gen_audio_outtype>(value));
      ++tested;
    }
    struct ailo_mtmd_helper_gen_audio_inp *element = nullptr;
    CHECK(ailo_mtmd_helper_gen_audio_inp_array_at(self, 2, 1, &element,
                                                  &error) == AILO_OK);
    CHECK(reinterpret_cast<struct mtmd_helper_gen_audio_inp *>(element) ==
          native + 1);
    CHECK(ailo_mtmd_helper_gen_audio_inp_array_at(
              self, 2, 2, &element, &error) == AILO_OUT_OF_RANGE &&
          !element);
    CHECK(ailo_mtmd_helper_gen_audio_inp_array_new(
              std::numeric_limits<size_t>::max(), &element, &error) ==
              AILO_OUT_OF_RANGE &&
          !element);
    CHECK(ailo_mtmd_helper_gen_audio_inp_array_free(self, &error) == AILO_OK);
    CHECK(ailo_mtmd_helper_gen_audio_inp_array_free(nullptr, &error) ==
          AILO_OK);
  }
  {
    struct ailo_mtmd_helper_init_opt *self = nullptr;
    CHECK(ailo_mtmd_helper_init_opt_array_new(2, &self, &error) == AILO_OK &&
          self);
    auto native = reinterpret_cast<struct mtmd_helper_init_opt *>(self);
    (void)native;
    {
      struct ailo_mtmd_helper_video_init_params *value = nullptr,
                                                *result = nullptr;
      CHECK(ailo_mtmd_helper_video_init_params_array_new(1, &value, &error) ==
            AILO_OK);
      CHECK(ailo_mtmd_helper_video_init_params_fps_target_set(
                value, 29.5f, &error) == AILO_OK);
      CHECK(ailo_mtmd_helper_init_opt_video_params_set(self, value, &error) ==
            AILO_OK);
      CHECK(ailo_mtmd_helper_init_opt_video_params_get(self, &result, &error) ==
                AILO_OK &&
            result != value);
      CHECK(native->video_params.fps_target == 29.5f);
      CHECK(reinterpret_cast<struct mtmd_helper_video_init_params *>(result)
                ->fps_target == 29.5f);
      CHECK(ailo_mtmd_helper_video_init_params_array_free(result, &error) ==
            AILO_OK);
      CHECK(ailo_mtmd_helper_video_init_params_array_free(value, &error) ==
            AILO_OK);
      ++tested;
    }
    struct ailo_mtmd_helper_init_opt *element = nullptr;
    CHECK(ailo_mtmd_helper_init_opt_array_at(self, 2, 1, &element, &error) ==
          AILO_OK);
    CHECK(reinterpret_cast<struct mtmd_helper_init_opt *>(element) ==
          native + 1);
    CHECK(ailo_mtmd_helper_init_opt_array_at(self, 2, 2, &element, &error) ==
              AILO_OUT_OF_RANGE &&
          !element);
    CHECK(ailo_mtmd_helper_init_opt_array_new(
              std::numeric_limits<size_t>::max(), &element, &error) ==
              AILO_OUT_OF_RANGE &&
          !element);
    CHECK(ailo_mtmd_helper_init_opt_array_free(self, &error) == AILO_OK);
    CHECK(ailo_mtmd_helper_init_opt_array_free(nullptr, &error) == AILO_OK);
  }
  {
    struct ailo_mtmd_helper_video_info *self = nullptr;
    CHECK(ailo_mtmd_helper_video_info_array_new(2, &self, &error) == AILO_OK &&
          self);
    auto native = reinterpret_cast<struct mtmd_helper_video_info *>(self);
    (void)native;
    {
      ailo_u32 value = 97;
      ailo_u32 result = {};
      CHECK(ailo_mtmd_helper_video_info_width_set(self, value, &error) ==
            AILO_OK);
      CHECK(ailo_mtmd_helper_video_info_width_get(self, &result, &error) ==
            AILO_OK);
      CHECK(result == value);
      CHECK(native->width == static_cast<uint32_t>(value));
      ++tested;
    }
    {
      ailo_u32 value = 97;
      ailo_u32 result = {};
      CHECK(ailo_mtmd_helper_video_info_height_set(self, value, &error) ==
            AILO_OK);
      CHECK(ailo_mtmd_helper_video_info_height_get(self, &result, &error) ==
            AILO_OK);
      CHECK(result == value);
      CHECK(native->height == static_cast<uint32_t>(value));
      ++tested;
    }
    {
      float value = 0.25f;
      float result = {};
      CHECK(ailo_mtmd_helper_video_info_fps_set(self, value, &error) ==
            AILO_OK);
      CHECK(ailo_mtmd_helper_video_info_fps_get(self, &result, &error) ==
            AILO_OK);
      CHECK(result == value);
      CHECK(native->fps == static_cast<float>(value));
      ++tested;
    }
    {
      ailo_i32 value = 97;
      ailo_i32 result = {};
      CHECK(ailo_mtmd_helper_video_info_n_frames_set(self, value, &error) ==
            AILO_OK);
      CHECK(ailo_mtmd_helper_video_info_n_frames_get(self, &result, &error) ==
            AILO_OK);
      CHECK(result == value);
      CHECK(native->n_frames == static_cast<int32_t>(value));
      ++tested;
    }
    struct ailo_mtmd_helper_video_info *element = nullptr;
    CHECK(ailo_mtmd_helper_video_info_array_at(self, 2, 1, &element, &error) ==
          AILO_OK);
    CHECK(reinterpret_cast<struct mtmd_helper_video_info *>(element) ==
          native + 1);
    CHECK(ailo_mtmd_helper_video_info_array_at(self, 2, 2, &element, &error) ==
              AILO_OUT_OF_RANGE &&
          !element);
    CHECK(ailo_mtmd_helper_video_info_array_new(
              std::numeric_limits<size_t>::max(), &element, &error) ==
              AILO_OUT_OF_RANGE &&
          !element);
    CHECK(ailo_mtmd_helper_video_info_array_free(self, &error) == AILO_OK);
    CHECK(ailo_mtmd_helper_video_info_array_free(nullptr, &error) == AILO_OK);
  }
  {
    struct ailo_mtmd_helper_video_init_params *self = nullptr;
    CHECK(ailo_mtmd_helper_video_init_params_array_new(2, &self, &error) ==
              AILO_OK &&
          self);
    auto native =
        reinterpret_cast<struct mtmd_helper_video_init_params *>(self);
    (void)native;
    {
      float value = 0.25f;
      float result = {};
      CHECK(ailo_mtmd_helper_video_init_params_fps_target_set(
                self, value, &error) == AILO_OK);
      CHECK(ailo_mtmd_helper_video_init_params_fps_target_get(
                self, &result, &error) == AILO_OK);
      CHECK(result == value);
      CHECK(native->fps_target == static_cast<float>(value));
      ++tested;
    }
    {
      const char *value = reinterpret_cast<const char *>(address);
      const char *result = {};
      CHECK(ailo_mtmd_helper_video_init_params_ffmpeg_bin_dir_set(
                self, value, &error) == AILO_OK);
      CHECK(ailo_mtmd_helper_video_init_params_ffmpeg_bin_dir_get(
                self, &result, &error) == AILO_OK);
      CHECK(result == value);
      CHECK(native->ffmpeg_bin_dir == static_cast<const char *>(value));
      ++tested;
    }
    {
      ailo_i64 value = ailo_i64{0x80000000U, 17};
      ailo_i64 result = {};
      CHECK(ailo_mtmd_helper_video_init_params_timestamp_interval_ms_set(
                self, value, &error) == AILO_OK);
      CHECK(ailo_mtmd_helper_video_init_params_timestamp_interval_ms_get(
                self, &result, &error) == AILO_OK);
      CHECK(result.hi == value.hi && result.lo == value.lo);
      CHECK(native->timestamp_interval_ms == ailo::detail::to_native(value));
      ++tested;
    }
    struct ailo_mtmd_helper_video_init_params *element = nullptr;
    CHECK(ailo_mtmd_helper_video_init_params_array_at(self, 2, 1, &element,
                                                      &error) == AILO_OK);
    CHECK(reinterpret_cast<struct mtmd_helper_video_init_params *>(element) ==
          native + 1);
    CHECK(ailo_mtmd_helper_video_init_params_array_at(
              self, 2, 2, &element, &error) == AILO_OUT_OF_RANGE &&
          !element);
    CHECK(ailo_mtmd_helper_video_init_params_array_new(
              std::numeric_limits<size_t>::max(), &element, &error) ==
              AILO_OUT_OF_RANGE &&
          !element);
    CHECK(ailo_mtmd_helper_video_init_params_array_free(self, &error) ==
          AILO_OK);
    CHECK(ailo_mtmd_helper_video_init_params_array_free(nullptr, &error) ==
          AILO_OK);
  }
  {
    struct ailo_mtmd_input_part *self = nullptr;
    CHECK(ailo_mtmd_input_part_array_new(2, &self, &error) == AILO_OK && self);
    auto native = reinterpret_cast<struct mtmd_input_part *>(self);
    (void)native;
    {
      const struct ailo_mtmd_input_text *value =
          reinterpret_cast<const struct ailo_mtmd_input_text *>(address);
      const struct ailo_mtmd_input_text *result = {};
      CHECK(ailo_mtmd_input_part_text_set(self, value, &error) == AILO_OK);
      CHECK(ailo_mtmd_input_part_text_get(self, &result, &error) == AILO_OK);
      CHECK(result == value);
      CHECK(native->text ==
            reinterpret_cast<const struct mtmd_input_text *>(value));
      ++tested;
    }
    {
      const struct ailo_mtmd_bitmap *value =
          reinterpret_cast<const struct ailo_mtmd_bitmap *>(address);
      const struct ailo_mtmd_bitmap *result = {};
      CHECK(ailo_mtmd_input_part_bitmap_set(self, value, &error) == AILO_OK);
      CHECK(ailo_mtmd_input_part_bitmap_get(self, &result, &error) == AILO_OK);
      CHECK(result == value);
      CHECK(native->bitmap ==
            reinterpret_cast<const struct mtmd_bitmap *>(value));
      ++tested;
    }
    struct ailo_mtmd_input_part *element = nullptr;
    CHECK(ailo_mtmd_input_part_array_at(self, 2, 1, &element, &error) ==
          AILO_OK);
    CHECK(reinterpret_cast<struct mtmd_input_part *>(element) == native + 1);
    CHECK(ailo_mtmd_input_part_array_at(self, 2, 2, &element, &error) ==
              AILO_OUT_OF_RANGE &&
          !element);
    CHECK(ailo_mtmd_input_part_array_new(std::numeric_limits<size_t>::max(),
                                         &element,
                                         &error) == AILO_OUT_OF_RANGE &&
          !element);
    CHECK(ailo_mtmd_input_part_array_free(self, &error) == AILO_OK);
    CHECK(ailo_mtmd_input_part_array_free(nullptr, &error) == AILO_OK);
  }
  {
    struct ailo_mtmd_input_text *self = nullptr;
    CHECK(ailo_mtmd_input_text_array_new(2, &self, &error) == AILO_OK && self);
    auto native = reinterpret_cast<struct mtmd_input_text *>(self);
    (void)native;
    {
      const char *value = reinterpret_cast<const char *>(address);
      const char *result = {};
      CHECK(ailo_mtmd_input_text_text_set(self, value, &error) == AILO_OK);
      CHECK(ailo_mtmd_input_text_text_get(self, &result, &error) == AILO_OK);
      CHECK(result == value);
      CHECK(native->text == static_cast<const char *>(value));
      ++tested;
    }
    {
      size_t value = 97;
      size_t result = {};
      CHECK(ailo_mtmd_input_text_text_len_set(self, value, &error) == AILO_OK);
      CHECK(ailo_mtmd_input_text_text_len_get(self, &result, &error) ==
            AILO_OK);
      CHECK(result == value);
      CHECK(native->text_len == static_cast<size_t>(value));
      ++tested;
    }
    {
      ailo_bool value = 17;
      ailo_bool result = {};
      CHECK(ailo_mtmd_input_text_add_special_set(self, value, &error) ==
            AILO_OK);
      CHECK(ailo_mtmd_input_text_add_special_get(self, &result, &error) ==
            AILO_OK);
      CHECK(result == 1 && native->add_special);
      ++tested;
    }
    {
      ailo_bool value = 17;
      ailo_bool result = {};
      CHECK(ailo_mtmd_input_text_parse_special_set(self, value, &error) ==
            AILO_OK);
      CHECK(ailo_mtmd_input_text_parse_special_get(self, &result, &error) ==
            AILO_OK);
      CHECK(result == 1 && native->parse_special);
      ++tested;
    }
    struct ailo_mtmd_input_text *element = nullptr;
    CHECK(ailo_mtmd_input_text_array_at(self, 2, 1, &element, &error) ==
          AILO_OK);
    CHECK(reinterpret_cast<struct mtmd_input_text *>(element) == native + 1);
    CHECK(ailo_mtmd_input_text_array_at(self, 2, 2, &element, &error) ==
              AILO_OUT_OF_RANGE &&
          !element);
    CHECK(ailo_mtmd_input_text_array_new(std::numeric_limits<size_t>::max(),
                                         &element,
                                         &error) == AILO_OUT_OF_RANGE &&
          !element);
    CHECK(ailo_mtmd_input_text_array_free(self, &error) == AILO_OK);
    CHECK(ailo_mtmd_input_text_array_free(nullptr, &error) == AILO_OK);
  }
  {
    struct ailo_mtmd_memory_usage *self = nullptr;
    CHECK(ailo_mtmd_memory_usage_array_new(2, &self, &error) == AILO_OK &&
          self);
    auto native = reinterpret_cast<struct mtmd_memory_usage *>(self);
    (void)native;
    {
      struct ailo_backend_memory_map *value = nullptr, *result = nullptr;
      CHECK(ailo_backend_memory_map_array_new(1, &value, &error) == AILO_OK);
      CHECK(ailo_backend_memory_map_set(value, nullptr, 1234, &error) ==
            AILO_OK);
      CHECK(ailo_mtmd_memory_usage_backend_mem_usage_set(self, value, &error) ==
            AILO_OK);
      CHECK(ailo_mtmd_memory_usage_backend_mem_usage_get(self, &result,
                                                         &error) == AILO_OK);
      size_t count = 0;
      CHECK(ailo_backend_memory_map_size(result, &count, &error) == AILO_OK &&
            count == 1);
      CHECK(native->backend_mem_usage.at(nullptr) == 1234);
      CHECK(ailo_backend_memory_map_array_free(result, &error) == AILO_OK);
      CHECK(ailo_backend_memory_map_array_free(value, &error) == AILO_OK);
      ++tested;
    }
    {
      int value = 97;
      int result = {};
      CHECK(ailo_mtmd_memory_usage_image_max_tokens_set(self, value, &error) ==
            AILO_OK);
      CHECK(ailo_mtmd_memory_usage_image_max_tokens_get(self, &result,
                                                        &error) == AILO_OK);
      CHECK(result == value);
      CHECK(native->image_max_tokens == static_cast<int>(value));
      ++tested;
    }
    {
      ailo_bool value = 17;
      ailo_bool result = {};
      CHECK(ailo_mtmd_memory_usage_use_non_causal_set(self, value, &error) ==
            AILO_OK);
      CHECK(ailo_mtmd_memory_usage_use_non_causal_get(self, &result, &error) ==
            AILO_OK);
      CHECK(result == 1 && native->use_non_causal);
      ++tested;
    }
    struct ailo_mtmd_memory_usage *element = nullptr;
    CHECK(ailo_mtmd_memory_usage_array_at(self, 2, 1, &element, &error) ==
          AILO_OK);
    CHECK(reinterpret_cast<struct mtmd_memory_usage *>(element) == native + 1);
    CHECK(ailo_mtmd_memory_usage_array_at(self, 2, 2, &element, &error) ==
              AILO_OUT_OF_RANGE &&
          !element);
    CHECK(ailo_mtmd_memory_usage_array_new(std::numeric_limits<size_t>::max(),
                                           &element,
                                           &error) == AILO_OUT_OF_RANGE &&
          !element);
    CHECK(ailo_mtmd_memory_usage_array_free(self, &error) == AILO_OK);
    CHECK(ailo_mtmd_memory_usage_array_free(nullptr, &error) == AILO_OK);
  }
  CHECK(tested == 281);
  std::printf("Native record fields verified: %zu\n", tested);
  return 0;
}
