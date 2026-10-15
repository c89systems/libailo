#ifndef AILO_REASONING_INTERNAL_HPP
#define AILO_REASONING_INTERNAL_HPP
#include "ailo_client_internal.hpp"
#include "ailo_conversation_internal.hpp"
#include "ailo_text_stream_internal.hpp"
#include <functional>

namespace ailo {
namespace client {
struct reasoning_format {
  std::string open, close;
  bool prompt_open = false;
};
struct reasoning_plan {
  bool active = false;
  reasoning_format format;
  std::string prompt;
};
struct tool_plan;
reasoning_plan render_prompt(state &, const conversation &, tool_plan &,
                             const ailo_request &);
size_t prompt_token_count(state &, const std::string &);
reasoning_plan qualify_reasoning(const char *, const llama_vocab *,
                                 const conversation &, const std::string &tools,
                                 const ailo_request &);
/** Compose with an existing eager grammar whose entry point is root. The
 * resulting entry point is reasoning-root. No lazy grammar/output retention. */
std::string reasoning_grammar(const reasoning_format &,
                              const std::string &answer);
enum class reasoning_output_kind { reasoning, end, answer };
/** Exactly one initial reasoning block. Carries at most a delimiter prefix
 * and three UTF-8 bytes; never retains a complete generated item. */
class reasoning_stream {
  reasoning_format format;
  std::function<void(reasoning_output_kind, const char *, size_t)> sink;
  text_stream reasoning_text, answer_text;
  std::string pending;
  size_t opening = 0;
  uint64_t next = 0, pending_start = 0, emitted = 0;
  enum class stage { open, reasoning, answer } current;
  void feed(const char *, size_t, uint64_t);

public:
  reasoning_stream(
      reasoning_format,
      std::function<void(reasoning_output_kind, const char *, size_t)>);
  reasoning_stream(const reasoning_stream &) = delete;
  reasoning_stream &operator=(const reasoning_stream &) = delete;
  void push(const char *, size_t, uint64_t origin = no_offset);
  uint64_t output_offset() const { return emitted; }
  uint64_t pending_offset() const;
  /** Length truncation may leave a partial reasoning item. Other termination
   * requires a complete close delimiter. Finalization never leaks syntax. */
  void finish(bool truncated);
  bool in_reasoning() const { return current != stage::answer; }
};
} // namespace client
} // namespace ailo
#endif
