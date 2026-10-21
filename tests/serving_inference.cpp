// All sampling, tool parsing and summaries link the production generator.
#include "ailo_compaction_internal.hpp"
#include "ailo_openai_internal.hpp"
#include "serving_fixture_internal.hpp"
#include <cstdlib>
extern "C" llama_sampler *__real_llama_sampler_init_temp(float);
extern "C" llama_sampler *__wrap_llama_sampler_init_temp(float) {
  return __real_llama_sampler_init_temp(0);
}
namespace serving_test {
namespace {
std::string setting(const char *name, const char *fallback) {
  const char *value = std::getenv(name);
  return value ? value : fallback;
}
void replace(std::string &text, const std::string &from,
             const std::string &to) {
  size_t at = 0, count = 0;
  while ((at = text.find(from, at)) != std::string::npos) {
    text.replace(at, from.size(), to);
    at += to.size();
    ++count;
  }
  require(count != 0, "Pinned Qwen chat-template adaptation no longer matches");
}
std::string developer_template(const char *path) {
  auto params = llama_model_default_params();
  params.vocab_only = true;
  params.n_gpu_layers = 0;
  std::unique_ptr<llama_model, decltype(&llama_model_free)> model(
      llama_model_load_from_file(path, params), llama_model_free);
  require(bool(model), "Cannot read pinned model template");
  const char *original = llama_model_chat_template(model.get(), nullptr);
  require(original != nullptr, "Pinned model has no template");
  std::string result = original;
  // This model predates the developer role. Declare the test application's
  // explicit mapping to ChatML system instructions; never drop their content.
  replace(result, "messages[0]['role'] == 'system'",
          "(messages[0]['role'] == 'system' or messages[0]['role'] == "
          "'developer')");
  replace(result, "(message.role == \"system\" and not loop.first)",
          "((message.role == \"system\" or message.role == \"developer\") and "
          "not loop.first)");
  replace(result, "+ message.role",
          "+ ('system' if message.role == 'developer' else message.role)");
  return result;
}
} // namespace
void configure_model(ailo_client_config &config) {
  const auto kind = setting("AILO_TEST_KIND", "text");
  const auto backend = setting("AILO_TEST_BACKEND", "cpu");
  require(kind == "text" || kind == "vision" || kind == "audio",
          "Invalid fixture model kind");
  require(backend == "cpu" || backend == "vulkan", "Invalid fixture backend");
  static const char *hashes[] = {nullptr};
  hashes[0] =
      kind == "text"
          ? "6eb923e7d26e9cea28811e1a8e852009b21242fb157b26149d3b188f3a8c8653"
      : kind == "vision"
          ? "2a31195d3769c0b0fd0a4906201666108834848db768af11de1d2cef7cd35e65"
          : "bca259818b50ca7c4c05e9bdb35a5dc04fa039653a6d6f3f0f331f96f6aa1971";
  config.source.sha256 = hashes;
  config.source.sha256_count = 1;
  if (kind == "text") {
    static const std::string chat =
        developer_template(config.source.local_path);
    config.chat_template = chat.c_str();
  } else {
    static const char *projector_hash[] = {nullptr};
    projector_hash[0] =
        kind == "vision"
            ? "7e943f7c53f0382a6fc41b6ee0c2def63ba4fded9ab8ed039cc9e2ab905e0edd"
            : "41a342b5e4c514e968cb756de6cd1b7be39eff43c44c57a2ef5fc6522e36603"
              "d";
    static ailo_multimodal_config media{};
    media.projector.local_path = std::getenv("AILO_TEST_PROJECTOR");
    require(media.projector.local_path,
            "Media fixture needs its pinned projector");
    media.projector.sha256 = projector_hash;
    media.projector.sha256_count = 1;
    media.backend = backend == "cpu" ? AILO_BACKEND_CPU : AILO_BACKEND_VULKAN;
    config.multimodal = &media;
    config.context_tokens = 4096;
  }
  config.backend = backend == "cpu" ? AILO_BACKEND_CPU : AILO_BACKEND_VULKAN;
  static const ailo_gpu_config gpu{};
  if (config.backend == AILO_BACKEND_VULKAN)
    config.gpu = &gpu;
  config.lazy = 0;
  config.threads = 2;
}
void observe_model(ailo_client *client, json &receipt) {
  ailo_error error{};
  char *raw = nullptr;
  require(client->model_card(client, &raw, &error) == AILO_OK, error.message);
  std::unique_ptr<char, decltype(&ailo_string_free)> card(raw,
                                                          ailo_string_free);
  const auto data = json::parse(raw);
  const auto &placement = data.at("placement");
  receipt["placement"] = placement;
  receipt["multimodal"] = data.at("multimodal");
  const auto backend = setting("AILO_TEST_BACKEND", "cpu");
  require(placement.at("actual_backend") == backend,
          "Fixture backend silently changed");
  if (backend == "vulkan") {
    const auto &devices = placement.at("selected_devices");
    require(!devices.empty(), "No selected GPU");
    const auto selected = [&](const json &id) {
      for (const auto &device : devices)
        if (device.at("id") == id)
          return true;
      return false;
    };
    require(selected(placement.at("output_device_id")),
            "Output layer is not on Vulkan");
    for (const auto &layer : placement.at("layers"))
      require(selected(layer.at("device_id")),
              "Repeating layer is not on Vulkan");
    bool context = false;
    for (const auto &buffer : placement.at("native_buffers"))
      context =
          context ||
          (selected(buffer.at("device_id")) &&
           std::stoull(buffer.at("context_bytes").get<std::string>()) > 0);
    require(context, "Vulkan context allocation is missing");
    if (!data.at("multimodal").is_null())
      require(selected(data.at("multimodal").at("encoder_device_id")),
              "Media encoder is not on Vulkan");
  }
  if (!data.at("multimodal").is_null() && backend == "cpu")
    require(data.at("multimodal").at("encoder_device_id") == "cpu",
            "Media encoder is not on CPU");
}
void inspect_model_input(ailo_client *client, const std::string &body) {
  using namespace ailo::client;
  const auto wire = json::parse(body);
  if (!wire.contains("input") || !wire["input"].is_array())
    return;
  bool compacted = false;
  for (const auto &item : wire["input"])
    compacted = compacted || item.value("type", "") == "compaction";
  if (!compacted)
    return;
  // Observe the already-materialized request, never generated output. Use the
  // production authenticated replay to expose test-only semantic evidence.
  try {
    auto &self = *static_cast<state *>(client->state);
    openai_request request(openai_protocol::responses, body.data(), body.size(),
                           self.id);
    api_expand_compaction(self, request);
    const auto replay = prepare_replay(self, request.generation);
    std::ofstream(directory + "/replay-" + std::to_string(current_request) +
                  ".txt")
        << replay.compaction_text;
  } catch (const failure &) {
    // Invalid replay still goes through the handler's public error path.
  }
}
} // namespace serving_test
