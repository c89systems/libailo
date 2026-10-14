#include "ailo_client_internal.hpp"
#include <cstring>
#include <jinja/parser.h>
#include <json.h>

namespace ailo {
namespace client {
namespace {
bool is_jinja(const char *source) {
  return std::strstr(source, "{{") || std::strstr(source, "{%");
}

jinja::program parse(const char *source) {
  const size_t size = strnlen(source, 65537);
  if (size > 65536)
    throw failure(AILO_OUT_OF_RANGE, "Chat template exceeds 64 KiB");
  require_utf8(source, size);
  jinja::lexer lexer;
  return jinja::parse_from_tokens(lexer.tokenize(std::string(source, size)));
}

std::string token_text(const llama_vocab *vocab, llama_token token) {
  return token == LLAMA_TOKEN_NULL ? "" : llama_vocab_get_text(vocab, token);
}
} // namespace

bool template_supported(const char *source) {
  if (!source)
    return false;
  try {
    if (is_jinja(source)) {
      (void)parse(source);
      return true;
    }
    return llama_chat_apply_template(source, nullptr, 0, true, nullptr, 0) >= 0;
  } catch (const std::bad_alloc &) {
    throw;
  } catch (...) {
    return false;
  }
}

std::string apply_template(const char *source, const llama_vocab *vocab,
                           const std::vector<llama_chat_message> &messages) {
  std::string result;
  if (is_jinja(source)) {
    try {
      const auto program = parse(source);
      jinja::context context(source);
      common_json inputs{
          {"messages", common_json::array()},
          {"bos_token", token_text(vocab, llama_vocab_bos(vocab))},
          {"eos_token", token_text(vocab, llama_vocab_eos(vocab))},
          {"add_generation_prompt", true},
          {"enable_thinking", false}};
      for (const auto &message : messages)
        inputs["messages"].push_back(
            {{"role", message.role}, {"content", message.content}});
      jinja::global_from_json(context, inputs, false);
      jinja::runtime runtime(context);
      result = jinja::runtime::gather_string_parts(runtime.execute(program))
                   ->as_string()
                   .str();
    } catch (const failure &) {
      throw;
    } catch (const std::bad_alloc &) {
      throw;
    } catch (...) {
      // Native exceptions can contain source text or conversation contents.
      throw failure(AILO_UNSUPPORTED,
                    "The chat template cannot render this conversation");
    }
  } else {
    const int32_t size = llama_chat_apply_template(
        source, messages.data(), messages.size(), true, nullptr, 0);
    if (size < 0)
      throw failure(AILO_UNSUPPORTED, "Unknown named chat template");
    if (size > 8 * 1024 * 1024)
      throw failure(AILO_OUT_OF_RANGE, "Rendered prompt exceeds 8 MiB");
    std::vector<char> buffer(static_cast<size_t>(size) + 1);
    if (llama_chat_apply_template(source, messages.data(), messages.size(),
                                  true, buffer.data(), buffer.size()) != size)
      throw failure(AILO_MODEL_FAILED, "Chat template rendering failed");
    result.assign(buffer.data(), static_cast<size_t>(size));
  }
  if (result.size() > 8 * 1024 * 1024)
    throw failure(AILO_OUT_OF_RANGE, "Rendered prompt exceeds 8 MiB");
  require_utf8(result.data(), result.size());
  return result;
}
} // namespace client
} // namespace ailo
