#ifndef AILO_NATIVE_INTERNAL_HPP
#define AILO_NATIVE_INTERNAL_HPP

#include "ailo_error_internal.h"
#include "ailo_integer_bridge.hpp"
#include <exception>
#include <iterator>
#include <limits>
#include <memory>
#include <new>

namespace ailo {
namespace detail {
struct native_range {};
struct native_unsupported {};

template <typename F>
ailo_status native_boundary(ailo_error *error, F function) {
  try {
    function();
    ailo_error_clear(error);
    return AILO_OK;
  } catch (const native_range &) {
    return ailo_error_set(
        error, AILO_OUT_OF_RANGE,
        "Native element index or allocation size is out of range");
  } catch (const native_unsupported &) {
    return ailo_error_set(error, AILO_UNSUPPORTED,
                          "The selected native backend is not compiled");
  } catch (const std::bad_alloc &) {
    return ailo_error_set(error, AILO_NO_MEMORY,
                          "Native facade allocation failed");
  } catch (const std::exception &) {
    return ailo_error_set(error, AILO_INTERNAL_ERROR,
                          "Native operation raised a C++ exception");
  } catch (...) {
    return ailo_error_set(error, AILO_INTERNAL_ERROR,
                          "Native operation raised an unknown exception");
  }
}
} // namespace detail
} // namespace ailo
#endif
