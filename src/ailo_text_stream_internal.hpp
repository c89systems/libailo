#ifndef AILO_TEXT_STREAM_INTERNAL_HPP
#define AILO_TEXT_STREAM_INTERNAL_HPP
#include <functional>
#include <string>
#include <vector>

namespace ailo {
namespace client {
class text_stream {
  std::vector<std::string> stops;
  std::function<void(const char *, size_t)> emit;
  std::string carry;
  bool stopped = false;

public:
  text_stream(std::vector<std::string> stop,
              std::function<void(const char *, size_t)> sink);
  bool push(const std::string &piece, bool final = false);
};
} // namespace client
} // namespace ailo
#endif
