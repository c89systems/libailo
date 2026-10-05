#include "ailo_integer_bridge.hpp"
#include <cstdio>
#include <cstring>
#include <limits>

#define CHECK(expr)                                                            \
  do {                                                                         \
    if (!(expr)) {                                                             \
      std::fprintf(stderr, "%s:%d: %s\n", __FILE__, __LINE__, #expr);          \
      return 1;                                                                \
    }                                                                          \
  } while (0)

int main() {
  using namespace ailo::detail;
  CHECK(std::strcmp(ailo_status_name(-1), "unknown_status") == 0);
  CHECK(std::strcmp(ailo_status_name(999), "unknown_status") == 0);
  const std::uint64_t unsigned_cases[] = {
      0,
      1,
      std::uint64_t(1) << 31,
      std::uint64_t(1) << 32,
      (std::uint64_t(1) << 63) - 1,
      std::uint64_t(1) << 63,
      std::numeric_limits<std::uint64_t>::max()};
  const std::int64_t signed_cases[] = {
      0, 1, -1, std::numeric_limits<std::int64_t>::min(),
      std::numeric_limits<std::int64_t>::max()};
  for (std::uint64_t v : unsigned_cases) {
    CHECK(to_native(from_unsigned(v)) == v);
  }
  for (std::int64_t v : signed_cases) {
    CHECK(to_native(from_signed(v)) == v);
  }
  const ailo_u64 max_unsigned = {0xffffffffU, 0xffffffffU};
  const ailo_i64 min_signed = {0x80000000U, 0};
  const ailo_i64 max_signed = {0x7fffffffU, 0xffffffffU};
  CHECK(to_native(max_unsigned) == std::numeric_limits<std::uint64_t>::max());
  CHECK(to_native(min_signed) == std::numeric_limits<std::int64_t>::min());
  CHECK(to_native(max_signed) == std::numeric_limits<std::int64_t>::max());

  std::uint64_t state = 0x4a63d22;
  for (unsigned int i = 0; i < 16384; ++i) {
    state ^= state << 13;
    state ^= state >> 7;
    state ^= state << 17;
    const ailo_u64 u = from_unsigned(state);
    const ailo_i64 s = {u.hi, u.lo};
    /* Independent oracle: the supported native ABI's signed representation. */
    std::int64_t signed_value;
    std::memcpy(&signed_value, &state, sizeof(signed_value));
    CHECK(to_native(u) == state);
    CHECK(to_native(s) == signed_value);
    CHECK(from_signed(signed_value).hi == s.hi);
    CHECK(from_signed(signed_value).lo == s.lo);
    unsigned long ul = 7;
    long sl = 7;
    const ailo_status us = ailo_u64_to_ulong(u, &ul, NULL);
    const ailo_status ss = ailo_i64_to_long(s, &sl, NULL);
    if (state > std::numeric_limits<unsigned long>::max()) {
      CHECK(us == AILO_OUT_OF_RANGE && ul == 0);
    } else {
      CHECK(us == AILO_OK && ul == state);
    }
    if (signed_value < std::numeric_limits<long>::min() ||
        signed_value > std::numeric_limits<long>::max()) {
      CHECK(ss == AILO_OUT_OF_RANGE && sl == 0);
    } else {
      CHECK(ss == AILO_OK && sl == signed_value);
    }
  }
  std::puts("Native full-width scalar bridge: passed");
  return 0;
}
