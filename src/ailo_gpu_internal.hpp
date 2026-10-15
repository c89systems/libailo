#ifndef AILO_GPU_INTERNAL_HPP
#define AILO_GPU_INTERNAL_HPP
#include "ailo/client.h"
#include "ggml-backend.h"
#include <cstdint>
#include <string>
#include <vector>
namespace ailo {
namespace client {
struct device_info {
  ggml_backend_dev_t device = nullptr;
  ggml_backend_dev_props properties{};
  std::string id, uuid, driver_uuid, driver, reason;
  uint32_t vendor = 0, product = 0, driver_id = 0;
  bool eligible = false, budget_known = false, integrated = false;
};
device_info describe_device(ggml_backend_dev_t);
std::vector<device_info> gpu_devices();
struct gpu_selection {
  std::vector<std::string> ids;
  std::vector<const char *> id_views;
  std::vector<double> weights;
  std::vector<float> tensor_split;
  ailo_gpu_split split = AILO_GPU_SPLIT_LAYER;
  bool configured = false;
  explicit gpu_selection(const ailo_gpu_config *, ailo_backend_policy);
  gpu_selection(const gpu_selection &) = delete;
  gpu_selection &operator=(const gpu_selection &) = delete;
  ailo_gpu_config view() const {
    return {id_views.data(), id_views.size(), weights.data(), weights.size(),
            split};
  }
  bool required() const { return configured; }
  std::vector<device_info> choose(const std::vector<device_info> &) const;
  void verify(const std::vector<device_info> &,
              const std::vector<ggml_backend_dev_t> &layers,
              ggml_backend_dev_t output,
              const std::vector<ggml_backend_dev_t> &resident) const;
};
} // namespace client
} // namespace ailo
#endif
