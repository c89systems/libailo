# C89 scalar and error boundary

This specifies the dependency-independent portion of the facade. Callback
closure implementation remains a pending dependency decision; no callback
registration is implied by this scalar contract.

The installed public base header is `ailo/base.h`. Project integer typedefs use
C89 signed/unsigned char, short and int for 8-, 16- and 32-bit values. Supported
targets must have 8-bit bytes, 16-bit short and 32-bit int; incompatible targets
fail compilation instead of producing subtly different values. `ailo_bool` is
an int, with zero false and nonzero true. No public header includes stdint.h,
stdbool.h, C++ library headers or a native upstream header.

`ailo_u64` and `ailo_i64` are separate value structs, each with unsigned 32-bit
`hi` and `lo` words. Unsigned values represent `hi * 2^32 + lo`. Signed values
encode the same 64 bits in two's-complement form; the high word's most
significant bit determines the sign. Word order in memory is deliberately
unrelated to native byte order, alignment or calling convention. The private
C++ bridge converts values explicitly. It must not reinterpret these structs
as native integers or cast a native integer array to an array of these structs.
Native array bridging is separate later work.

The public scalar helpers are:

- `ailo_u64_from_ulong` and `ailo_i64_from_long`: exact construction from native
  C89 long values on supported 32- or 64-bit-long targets.
- `ailo_u64_to_ulong` and `ailo_i64_to_long`: checked conversion to native C89
  long values. Success writes the exact value. A null output returns
  `AILO_INVALID_ARGUMENT`; overflow returns `AILO_OUT_OF_RANGE`. A nonnull
  output is zeroed on failure. An optional `ailo_error` receives the diagnostic
  and is cleared on success.

The low-level values preserve unsigned `0..2^64-1` and signed
`-2^63..2^63-1`, including signed minima without overflowing an intermediate.
The helpers neither allocate nor access environment, files or the network.

`ailo_status` describes bridge/library failures independently of upstream
return codes. The latter are preserved separately by subsequent facade
functions. `ailo_error` is a caller-owned record with status and a bounded,
NUL-terminated message. It owns no heap memory and can be initialized to zero.
`ailo_error_clear(NULL)` is harmless. `ailo_status_name` returns a borrowed
constant string, including an explicit name for unknown status values.
`ailo_string_free` is the cleanup function for future project-allocated strings
and tolerates NULL; it does not make borrowed native strings caller-owned.

All current base-library exports are enumerated in `cmake/ailo.exports`. The
shared library uses that exact allowlist, hiding its private error-setting
helper. Static archive implementation symbols are not a dynamic ABI boundary.
The initial shared ABI is 0; there is no prior released libailo ABI to advance.

Acceptance covers standalone C89 and C++ inclusion, 32-/64-bit long conversion
boundaries, full-width native bridge round trips, invalid/null outputs, success
error clearing, unknown statuses, null cleanup, static and shared C consumers,
exact shared exports and a negative link to the private error helper. Passing
these checks proves the scalar boundary only. Complete upstream record,
callback and function coverage remains required by the main specification.
