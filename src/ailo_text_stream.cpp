#include "ailo_client_internal.hpp"
#include "ailo_text_stream_internal.hpp"
#include <algorithm>
#include <utility>

namespace ailo {
namespace client {
namespace {
size_t utf8_prefix(const char *data, size_t size, bool final) {
  size_t i = 0;
  while (i < size) {
    const auto first = static_cast<unsigned char>(data[i]);
    size_t count = 0;
    uint32_t code = 0;
    if (first < 0x80) {
      ++i;
      continue;
    } else if (first >= 0xc2 && first <= 0xdf) {
      count = 2;
      code = first & 0x1f;
    } else if (first >= 0xe0 && first <= 0xef) {
      count = 3;
      code = first & 0x0f;
    } else if (first >= 0xf0 && first <= 0xf4) {
      count = 4;
      code = first & 7;
    } else {
      throw failure(AILO_INVALID_ARGUMENT, "Text contains invalid UTF-8");
    }
    for (size_t j = 1; j < std::min(count, size - i); ++j) {
      const auto ch = static_cast<unsigned char>(data[i + j]);
      if ((ch & 0xc0) != 0x80)
        throw failure(AILO_INVALID_ARGUMENT,
                      "Text contains invalid UTF-8 continuation");
      code = (code << 6) | (ch & 0x3f);
    }
    if (size - i < count) {
      if (!final)
        return i;
      throw failure(AILO_INVALID_ARGUMENT,
                    "Text ends inside a UTF-8 character");
    }
    if ((count == 3 && code < 0x800) || (count == 4 && code < 0x10000) ||
        code > 0x10ffff || (code >= 0xd800 && code <= 0xdfff))
      throw failure(AILO_INVALID_ARGUMENT,
                    "Text contains invalid Unicode scalar");
    i += count;
  }
  return i;
}
} // namespace

void require_utf8(const char *data, size_t size) {
  (void)utf8_prefix(data, size, true);
}

text_stream::text_stream(std::vector<std::string> stop,
                         std::function<void(const char *, size_t)> sink)
    : stops(std::move(stop)), emit(std::move(sink)) {}

bool text_stream::push(const std::string &piece, bool final, uint64_t origin) {
  if (stopped)
    return true;
  if (!piece.empty()) {
    const auto base = origin == no_offset ? next : origin;
    if (!carry.empty() && base != next)
      throw failure(AILO_INTERNAL_ERROR,
                    "Noncontiguous bytes inside pending text");
    if (carry.empty())
      start = base;
    next = base + piece.size();
    carry += piece;
  }
  size_t match = std::string::npos;
  for (const auto &stop : stops)
    match = std::min(match, carry.find(stop));
  size_t safe = match == std::string::npos ? carry.size() : match;
  if (!final && match == std::string::npos)
    for (const auto &stop : stops)
      for (size_t n = std::min(stop.size() - 1, carry.size()); n; --n)
        if (carry.compare(carry.size() - n, n, stop, 0, n) == 0) {
          safe = std::min(safe, carry.size() - n);
          break;
        }
  try {
    safe = utf8_prefix(carry.data(), safe, final || match != std::string::npos);
  } catch (const failure &) {
    throw failure(AILO_MODEL_FAILED,
                  "Native output contains incomplete or invalid UTF-8");
  }
  if (safe) {
    emitted = start;
    emit(carry.data(), safe);
    start += safe;
  }
  carry.erase(0, safe);
  if (match != std::string::npos) {
    carry.clear();
    start = next;
    stopped = true;
  }
  return stopped;
}
} // namespace client
} // namespace ailo
