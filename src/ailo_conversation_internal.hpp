#ifndef AILO_CONVERSATION_INTERNAL_HPP
#define AILO_CONVERSATION_INTERNAL_HPP
#include "ailo_client_internal.hpp"

namespace ailo {
namespace client {
struct chat_call {
  std::string id, name, arguments;
};
struct chat_message {
  std::string role, content, name, call_id;
  std::vector<chat_call> calls;
  std::string reasoning{};
  bool has_reasoning = false;
};
struct conversation {
  std::vector<chat_message> messages;
  bool rich = false;
  size_t max_parallel_calls = 0;
  size_t pending_calls = 0;
  // Nonempty only after authenticated replay; the actual renderer must preserve
  // this lower-trust message verbatim rather than silently dropping history.
  std::string compaction_text;
};
conversation prepare_conversation(const ailo_request &request,
                                  bool allow_pending = false);
struct template_options {
  bool thinking = false;
  const char *effort = nullptr;
};
std::string apply_template(const char *source, const llama_vocab *vocab,
                           const conversation &input,
                           const std::string &tools = "[]",
                           bool generation = true,
                           const template_options *options = nullptr);
} // namespace client
} // namespace ailo
#endif
