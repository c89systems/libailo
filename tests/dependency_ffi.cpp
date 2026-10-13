#include <cstdint>
#include <cstdio>
#include <cstring>
#include <ffi.h>

static void closure(ffi_cif *, void *result, void **args, void *context) {
  const std::uint64_t value = *static_cast<std::uint64_t *>(args[0]);
  const std::int64_t adjustment = *static_cast<std::int64_t *>(args[1]);
  *static_cast<std::uint64_t *>(result) =
      value + static_cast<std::uint64_t>(adjustment) +
      *static_cast<std::uint64_t *>(context);
}

int main() {
  ffi_cif cif;
  ffi_type *types[] = {&ffi_type_uint64, &ffi_type_sint64};
  if (ffi_prep_cif(&cif, FFI_DEFAULT_ABI, 2, &ffi_type_uint64, types) != FFI_OK)
    return 1;
  std::uint64_t contexts[] = {7, 19};
  ffi_closure *closures[2] = {};
  void *entries[2] = {};
  int status = 0;
  for (unsigned i = 0; i < 2; ++i) {
    closures[i] = static_cast<ffi_closure *>(
        ffi_closure_alloc(sizeof(ffi_closure), &entries[i]));
    if (!closures[i] ||
        ffi_prep_closure_loc(closures[i], &cif, closure, &contexts[i],
                             entries[i]) != FFI_OK) {
      status = 2;
      break;
    }
  }
  if (!status) {
    typedef std::uint64_t (*callback)(std::uint64_t, std::int64_t);
    static_assert(sizeof(callback) == sizeof(void *),
                  "Unsupported code pointer ABI");
    for (unsigned repeat = 0; repeat < 3; ++repeat) {
      for (unsigned i = 0; i < 2; ++i) {
        callback call;
        std::memcpy(&call, &entries[i], sizeof(call));
        const std::uint64_t value = UINT64_C(0xfedcba9876543210);
        if (call(value, -INT64_C(0x123456789)) !=
            value - UINT64_C(0x123456789) + contexts[i])
          status = 3;
      }
    }
  }
  for (auto *entry : closures)
    if (entry)
      ffi_closure_free(entry);
  if (!status)
    std::puts("Independent libffi closures and full-width signed/unsigned ABI: "
              "passed");
  return status;
}
