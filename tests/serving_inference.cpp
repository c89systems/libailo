// All sampling, tool parsing and summaries link the production generator.
#include "ailo_compaction_internal.hpp"
#include "ailo_openai_internal.hpp"
#include "serving_fixture_internal.hpp"
extern "C" llama_sampler *__real_llama_sampler_init_temp(float);
extern "C" llama_sampler *__wrap_llama_sampler_init_temp(float) {
  return __real_llama_sampler_init_temp(0);
}
namespace serving_test {
namespace {
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
  static const char *hashes[] = {
      "6eb923e7d26e9cea28811e1a8e852009b21242fb157b26149d3b188f3a8c8653"};
  static const std::string chat = developer_template(config.source.local_path);
  config.source.sha256 = hashes;
  config.source.sha256_count = 1;
  config.chat_template = chat.c_str();
  config.lazy = 0;
  config.threads = 2;
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
