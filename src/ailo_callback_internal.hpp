#ifndef AILO_CALLBACK_INTERNAL_HPP
#define AILO_CALLBACK_INTERNAL_HPP

#include "ailo_native_internal.hpp"
#include <atomic>
#include <cstring>
#include <ffi.h>
#include <stdexcept>
#include <type_traits>
#include <vector>

namespace ailo {
namespace detail {
struct ffi_layout {
  struct aggregate_type {
    ffi_type type{};
    std::vector<ffi_type *> members;
  };
  std::vector<std::unique_ptr<aggregate_type>> aggregates;
  ffi_type *aggregate(std::vector<ffi_type *> members) {
    auto value = std::make_unique<aggregate_type>();
    value->members = std::move(members);
    value->members.push_back(nullptr);
    value->type.type = FFI_TYPE_STRUCT;
    value->type.elements = value->members.data();
    auto *result = &value->type;
    aggregates.push_back(std::move(value));
    return result;
  }
};

template <typename T> ffi_type *ffi_scalar() {
  if constexpr (std::is_void_v<T>)
    return &ffi_type_void;
  else if constexpr (std::is_pointer_v<T>)
    return &ffi_type_pointer;
  else if constexpr (std::is_same_v<T, float>)
    return &ffi_type_float;
  else if constexpr (std::is_same_v<T, double>)
    return &ffi_type_double;
  else if constexpr (std::is_enum_v<T>)
    return ffi_scalar<std::underlying_type_t<T>>();
  else if constexpr (std::is_integral_v<T>) {
    if constexpr (sizeof(T) == 1)
      return std::is_signed_v<T> ? &ffi_type_sint8 : &ffi_type_uint8;
    else if constexpr (sizeof(T) == 2)
      return std::is_signed_v<T> ? &ffi_type_sint16 : &ffi_type_uint16;
    else if constexpr (sizeof(T) == 4)
      return std::is_signed_v<T> ? &ffi_type_sint32 : &ffi_type_uint32;
    else if constexpr (sizeof(T) == 8)
      return std::is_signed_v<T> ? &ffi_type_sint64 : &ffi_type_uint64;
  }
  static_assert(std::is_void_v<T> || std::is_pointer_v<T> ||
                    std::is_arithmetic_v<T> || std::is_enum_v<T>,
                "Missing native ffi type description");
}

template <typename T> void ffi_store(void *result, const T &value) {
  if constexpr ((std::is_integral_v<T> || std::is_enum_v<T>) &&
                sizeof(T) < sizeof(ffi_arg)) {
    *static_cast<ffi_arg *>(result) = static_cast<ffi_arg>(value);
  } else {
    std::memcpy(result, &value, sizeof(value));
  }
}

struct callback_core {
  ffi_layout layout;
  ffi_cif cif{};
  std::vector<ffi_type *> arguments;
  ffi_closure *closure = nullptr;
  void *entry = nullptr;
  std::atomic<ailo_status> error{AILO_OK};
  callback_core() = default;
  callback_core(const callback_core &) = delete;
  ~callback_core() {
    if (closure)
      ffi_closure_free(closure);
  }
  void prepare(ffi_type *result, std::vector<ffi_type *> types,
               void (*dispatch)(ffi_cif *, void *, void **, void *),
               void *user) {
    arguments = std::move(types);
    if (ffi_prep_cif(&cif, FFI_DEFAULT_ABI, arguments.size(), result,
                     arguments.data()) != FFI_OK)
      throw std::runtime_error("Cannot prepare native callback ABI");
    closure = static_cast<ffi_closure *>(
        ffi_closure_alloc(sizeof(ffi_closure), &entry));
    if (!closure)
      throw std::bad_alloc();
    if (ffi_prep_closure_loc(closure, &cif, dispatch, user, entry) != FFI_OK)
      throw std::runtime_error("Cannot prepare native callback closure");
  }
  void failed(ailo_status status = AILO_INTERNAL_ERROR) noexcept {
    auto expected = AILO_OK;
    error.compare_exchange_strong(expected, status);
  }
};
} // namespace detail
} // namespace ailo
#endif
