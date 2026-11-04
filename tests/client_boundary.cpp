#include "ailo_client_internal.hpp"
#include <cstdio>
#include <cstring>
#include <stdexcept>
#include <string>

#define CHECK(value)                                                           \
  do {                                                                         \
    if (!(value))                                                              \
      throw std::runtime_error(#value);                                        \
  } while (0)

int main() {
  try {
    using ailo::client::boundary;
    struct {
      unsigned before = 0x12345678;
      ailo_error error{};
      unsigned after = 0x87654321;
    } guarded;
    auto *error = &guarded.error;
    CHECK(boundary(error, [] {
            throw std::runtime_error("vk::Queue::submit: ErrorDeviceLost");
          }) == AILO_INTERNAL_ERROR);
    CHECK(error->status == AILO_INTERNAL_ERROR);
    CHECK(std::strcmp(error->message, "vk::Queue::submit: ErrorDeviceLost") ==
          0);
    const std::string long_message(sizeof(error->message) * 2, 'x');
    CHECK(boundary(error, [&] { throw std::runtime_error(long_message); }) ==
          AILO_INTERNAL_ERROR);
    CHECK(std::strlen(error->message) == sizeof(error->message) - 1);
    CHECK(std::string(error->message) ==
          long_message.substr(0, sizeof(error->message) - 1));
    CHECK(guarded.before == 0x12345678 && guarded.after == 0x87654321);
    CHECK(boundary(error, [] { throw std::runtime_error(""); }) ==
          AILO_INTERNAL_ERROR);
    CHECK(std::strcmp(error->message,
                      "Inference receiver raised an exception") == 0);
    CHECK(boundary(error, [] { throw 42; }) == AILO_INTERNAL_ERROR);
    CHECK(std::strcmp(error->message,
                      "Inference receiver raised an exception") == 0);
    CHECK(boundary(error, [] { throw std::bad_alloc(); }) == AILO_NO_MEMORY);
    CHECK(error->status == AILO_NO_MEMORY);
    CHECK(std::strcmp(error->message, "Inference receiver allocation failed") ==
          0);
    CHECK(boundary(error, [] {
            throw ailo::client::failure(AILO_CANCELLED, "cancelled by caller");
          }) == AILO_CANCELLED);
    CHECK(error->status == AILO_CANCELLED);
    CHECK(std::strcmp(error->message, "cancelled by caller") == 0);
    CHECK(boundary(error, [] {}) == AILO_OK);
    CHECK(error->status == AILO_OK && error->message[0] == '\0');
    CHECK(boundary(nullptr, [] {}) == AILO_OK);
    CHECK(boundary(nullptr, [] { throw std::runtime_error("GPU failed"); }) ==
          AILO_INTERNAL_ERROR);
    CHECK(boundary(nullptr, [] { throw std::bad_alloc(); }) == AILO_NO_MEMORY);
    CHECK(boundary(nullptr, [] {
            throw ailo::client::failure(AILO_CANCELLED, "cancelled");
          }) == AILO_CANCELLED);
    CHECK(boundary(nullptr, [] { throw 42; }) == AILO_INTERNAL_ERROR);
    std::puts("Client boundary preserves owned, bounded native diagnostics");
    return 0;
  } catch (const std::exception &e) {
    std::fprintf(stderr, "%s\n", e.what());
    return 1;
  }
}
