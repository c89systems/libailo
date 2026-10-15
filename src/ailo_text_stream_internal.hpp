#ifndef AILO_TEXT_STREAM_INTERNAL_HPP
#define AILO_TEXT_STREAM_INTERNAL_HPP
#include <cstdint>
#include <functional>
#include <limits>
#include <string>
#include <vector>

namespace ailo {
namespace client {
constexpr uint64_t no_offset = std::numeric_limits<uint64_t>::max();
class text_stream {
  std::vector<std::string> stops;
  std::function<void(const char *, size_t)> emit;
  std::string carry;
  bool stopped = false;
  uint64_t next = 0, start = 0, emitted = 0;

public:
  text_stream(std::vector<std::string> stop,
              std::function<void(const char *, size_t)> sink);
  bool push(const std::string &piece, bool final = false,
            uint64_t origin = no_offset);
  uint64_t output_offset() const { return emitted; }
  uint64_t pending_offset() const { return carry.empty() ? no_offset : start; }
};
} // namespace client
} // namespace ailo
#endif
