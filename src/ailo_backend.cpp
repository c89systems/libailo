#include "ailo_backend_internal.hpp"
#include "llama.h"
#include <limits>
#include <mutex>
#include <stdexcept>

namespace ailo {
namespace detail {
namespace {
std::mutex backend_mutex;
size_t backend_users = 0;
} // namespace

void backend_acquire() {
  std::lock_guard<std::mutex> lock(backend_mutex);
  if (backend_users == std::numeric_limits<size_t>::max())
    throw std::overflow_error("Backend reference count exhausted");
  if (!backend_users)
    llama_backend_init();
  ++backend_users;
}

void backend_release() {
  std::lock_guard<std::mutex> lock(backend_mutex);
  if (backend_users && --backend_users == 0)
    llama_backend_free();
}
} // namespace detail
} // namespace ailo
