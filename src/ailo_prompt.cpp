#include "ailo_client_internal.hpp"
#include "ailo_conversation_internal.hpp"
#include "ailo_reasoning_internal.hpp"
#include "ailo_tools_internal.hpp"
#include <limits>

namespace ailo {
namespace client {
reasoning_plan render_prompt(state &self, const conversation &input,
                             tool_plan &tools, const ailo_request &request) {
  self.ensure_loaded();
  const char *tmpl = self.config.chat_template
                         ? self.config.chat_template
                         : llama_model_chat_template(self.model.get(), nullptr);
  if (!tmpl)
    throw failure(
        AILO_UNSUPPORTED,
        "The model has no chat template; configure an explicit template");
  qualify_tools(tools, tmpl, llama_model_get_vocab(self.model.get()));
  auto result = qualify_reasoning(tmpl, llama_model_get_vocab(self.model.get()),
                                  input, tools.definitions_json, request);
  if (!input.compaction_text.empty() &&
      result.prompt.find(input.compaction_text) == std::string::npos)
    throw failure(
        AILO_UNSUPPORTED,
        "The chat template does not preserve authenticated compaction context");
  return result;
}
size_t prompt_token_count(state &self, const std::string &prompt) {
  if (prompt.size() > static_cast<size_t>(INT32_MAX))
    throw failure(AILO_OUT_OF_RANGE,
                  "Rendered prompt exceeds tokenizer limits");
  const int32_t count = llama_tokenize(
      llama_model_get_vocab(self.model.get()), prompt.data(),
      static_cast<int32_t>(prompt.size()), nullptr, 0, true, true);
  if (count >= 0 || count == std::numeric_limits<int32_t>::min())
    throw failure(AILO_MODEL_FAILED, "Cannot size prompt tokenization");
  return static_cast<size_t>(-count);
}
} // namespace client
} // namespace ailo
