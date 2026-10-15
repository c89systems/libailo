#include "ailo_client_internal.hpp"
#include "ailo_gpu_internal.hpp"
#include <algorithm>
#include <cmath>
#include <cstring>
#include <set>
namespace ailo {
namespace client {
namespace {
std::string hex(const unsigned char *p, size_t n) {
  const char *digits = "0123456789abcdef";
  std::string out;
  for (size_t i = 0; i < n; ++i) {
    out += digits[p[i] >> 4];
    out += digits[p[i] & 15];
  }
  return out;
}
bool identity(const std::string &s) {
  if (s.size() != 39 || s.compare(0, 7, "vulkan:"))
    return false;
  bool nonzero = false;
  for (size_t n = 7; n < s.size(); ++n) {
    const auto c = s[n];
    if (!((c >= '0' && c <= '9') || (c >= 'a' && c <= 'f')))
      return false;
    nonzero |= c != '0';
  }
  return nonzero;
}
} // namespace
device_info describe_device(ggml_backend_dev_t device) {
  device_info out;
  out.device = device;
  ggml_backend_dev_get_props(device, &out.properties);
  const auto registry = ggml_backend_dev_backend_reg(device);
  if (std::strcmp(ggml_backend_reg_name(registry), "Vulkan"))
    return out;
  using query = bool (*)(ggml_backend_dev_t, unsigned char *, unsigned char *,
                         uint32_t *);
  auto inspect = reinterpret_cast<query>(
      ggml_backend_reg_get_proc_address(registry, "ailo_vk_device_identity"));
  unsigned char uuid[16]{}, driver[16]{};
  uint32_t values[4]{};
  if (!inspect || !inspect(device, uuid, driver, values)) {
    out.reason = "Vulkan identity query is unavailable";
    return out;
  }
  out.uuid = hex(uuid, 16);
  out.driver_uuid = hex(driver, 16);
  out.id = "vulkan:" + out.uuid;
  out.vendor = values[0];
  out.product = values[1];
  out.driver_id = values[2];
  out.budget_known = (values[3] & 1) != 0;
  out.integrated = out.properties.type == GGML_BACKEND_DEVICE_TYPE_IGPU;
  out.driver = out.driver_id == 3   ? "radv"
               : out.driver_id == 6 ? "anv"
                                    : "unsupported";
  if (!identity(out.id))
    out.reason = "Vulkan device has no usable UUID";
  else if (out.driver == "unsupported")
    out.reason = "Only Mesa RADV and ANV are supported";
  else {
    out.eligible = true;
    out.reason = "eligible";
  }
  return out;
}
std::vector<device_info> gpu_devices() {
  std::vector<device_info> out;
  const auto registry = ggml_backend_reg_by_name("Vulkan");
  if (registry)
    for (size_t n = 0; n < ggml_backend_reg_dev_count(registry); ++n)
      out.push_back(describe_device(ggml_backend_reg_dev_get(registry, n)));
  return out;
}
gpu_selection::gpu_selection(const ailo_gpu_config *input,
                             ailo_backend_policy backend) {
  if (!input)
    return;
  configured = true;
  if (backend == AILO_BACKEND_CPU)
    throw failure(AILO_INVALID_ARGUMENT,
                  "GPU placement cannot accompany backend=CPU");
  split = input->split;
  if (split < AILO_GPU_SPLIT_LAYER || split > AILO_GPU_SPLIT_ROW)
    throw failure(AILO_INVALID_ARGUMENT, "Unknown GPU split mode");
  if (split == AILO_GPU_SPLIT_ROW)
    throw failure(
        AILO_UNSUPPORTED,
        "The Vulkan receiver supports layer splitting, not row splitting");
  const size_t maximum = llama_max_devices();
  if (input->device_count > maximum ||
      (input->device_count && !input->devices) ||
      input->weight_count > maximum || (input->weight_count && !input->weights))
    throw failure(AILO_INVALID_ARGUMENT, "Invalid GPU device or weight array");
  std::set<std::string> unique;
  for (size_t n = 0; n < input->device_count; ++n) {
    if (!input->devices[n] || strnlen(input->devices[n], 40) != 39)
      throw failure(AILO_INVALID_ARGUMENT,
                    "GPU selection requires identities returned by devices()");
    std::string id(input->devices[n], 39);
    if (!identity(id) || !unique.insert(id).second)
      throw failure(AILO_INVALID_ARGUMENT,
                    "GPU identities must be valid and unique");
    ids.push_back(std::move(id));
  }
  if (split == AILO_GPU_SPLIT_NONE && ids.size() > 1)
    throw failure(AILO_INVALID_ARGUMENT,
                  "Single-device placement accepts at most one device");
  for (const auto &id : ids)
    id_views.push_back(id.c_str());
  if (!input->weight_count)
    return;
  if (ids.empty() || input->weight_count != ids.size() ||
      split != AILO_GPU_SPLIT_LAYER)
    throw failure(AILO_INVALID_ARGUMENT,
                  "GPU layer weights require a matching explicit device list");
  long double maximum_weight = 0;
  for (size_t n = 0; n < input->weight_count; ++n) {
    const auto w = input->weights[n];
    if (!std::isfinite(w) || w < 0)
      throw failure(AILO_INVALID_ARGUMENT,
                    "GPU layer weights must be finite and nonnegative");
    weights.push_back(w);
    maximum_weight = std::max(maximum_weight, static_cast<long double>(w));
  }
  if (!maximum_weight)
    throw failure(AILO_INVALID_ARGUMENT,
                  "At least one GPU layer weight must be positive");
  long double sum = 0;
  for (auto w : weights)
    sum += static_cast<long double>(w) / maximum_weight;
  tensor_split.assign(maximum, 0);
  for (size_t n = 0; n < weights.size(); ++n) {
    tensor_split[n] = static_cast<float>(
        (static_cast<long double>(weights[n]) / maximum_weight) / sum);
    if (weights[n] > 0 && !tensor_split[n])
      throw failure(
          AILO_OUT_OF_RANGE,
          "GPU layer weight is too small for the native split representation");
  }
}
std::vector<device_info>
gpu_selection::choose(const std::vector<device_info> &available) const {
  std::vector<device_info> out;
  if (ids.empty()) {
    for (const auto &dev : available)
      if (dev.eligible)
        out.push_back(dev);
    if (split == AILO_GPU_SPLIT_NONE && !out.empty())
      out.resize(1);
  } else
    for (const auto &id : ids) {
      const device_info *found = nullptr;
      for (const auto &dev : available)
        if (dev.id == id) {
          if (found)
            throw failure(AILO_UNSUPPORTED,
                          "GPU identity is ambiguous; rediscover devices");
          found = &dev;
        }
      if (!found)
        throw failure(AILO_NOT_FOUND, "Selected GPU is unavailable: " + id);
      if (!found->eligible)
        throw failure(AILO_UNSUPPORTED,
                      "Selected GPU is unusable: " + found->reason);
      out.push_back(*found);
    }
  if (out.size() > llama_max_devices())
    throw failure(AILO_OUT_OF_RANGE,
                  "Too many Vulkan devices for the native model");
  std::set<std::string> unique;
  for (const auto &dev : out)
    if (!unique.insert(dev.id).second)
      throw failure(
          AILO_UNSUPPORTED,
          "GPU identities are ambiguous; use a unique device configuration");
  return out;
}
void gpu_selection::verify(
    const std::vector<device_info> &selected,
    const std::vector<ggml_backend_dev_t> &layers, ggml_backend_dev_t output,
    const std::vector<ggml_backend_dev_t> &resident) const {
  if (!configured)
    return;
  if (selected.empty())
    throw failure(AILO_UNSUPPORTED,
                  "Explicit GPU placement has no eligible device");
  for (size_t n = 0; n < selected.size(); ++n) {
    if (!weights.empty() && weights[n] == 0)
      continue;
    if (std::find(layers.begin(), layers.end(), selected[n].device) ==
            layers.end() &&
        output != selected[n].device)
      throw failure(AILO_MODEL_FAILED,
                    "Selected GPU received no model layers: " + selected[n].id);
    if (std::find(resident.begin(), resident.end(), selected[n].device) ==
        resident.end())
      throw failure(AILO_MODEL_FAILED,
                    "Selected GPU has no device-resident model buffers: " +
                        selected[n].id);
  }
}
} // namespace client
} // namespace ailo
