#ifndef AILO_INTEGER_BRIDGE_HPP
#define AILO_INTEGER_BRIDGE_HPP

#include "ailo/base.h"
#include <cstdint>
#include <limits>

namespace ailo {
namespace detail {

inline std::uint64_t to_native(ailo_u64 value) {
  return (std::uint64_t(value.hi) << 32) | value.lo;
}

inline std::int64_t to_native(ailo_i64 value) {
  const std::uint64_t bits = (std::uint64_t(value.hi) << 32) | value.lo;
  if ((value.hi & 0x80000000U) == 0) {
    return std::int64_t(bits);
  }
  return -std::int64_t(~bits) - 1;
}

inline ailo_u64 from_unsigned(std::uint64_t value) {
  const ailo_u64 result = {ailo_u32(value >> 32), ailo_u32(value)};
  return result;
}

inline ailo_i64 from_signed(std::int64_t value) {
  const std::uint64_t bits = std::uint64_t(value);
  const ailo_i64 result = {ailo_u32(bits >> 32), ailo_u32(bits)};
  return result;
}

} // namespace detail
} // namespace ailo
#endif
