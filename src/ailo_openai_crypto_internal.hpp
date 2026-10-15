#ifndef AILO_OPENAI_CRYPTO_INTERNAL_HPP
#define AILO_OPENAI_CRYPTO_INTERNAL_HPP
#include <functional>
#include <string>
namespace ailo {
namespace client {
struct state;
class wire_json;
enum class api_item_purpose { compaction, reasoning };
struct api_plaintext {
  std::string bytes;
  api_plaintext() = default;
  api_plaintext(const api_plaintext &) = delete;
  ~api_plaintext();
};
std::string api_seal_item(state &, api_item_purpose, const std::string &id,
                          const std::function<void(wire_json &)> &);
void api_open_item(state &, api_item_purpose, const char *id,
                   const char *encrypted, api_plaintext &);
} // namespace client
} // namespace ailo
#endif
