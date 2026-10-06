# Native facade binding contract

The facade uses generated, typed C89 declarations under `ailo/native.h` and a
private C++ implementation. Generation is compiler-inventory processing, not a
build driver: generated headers and source are tracked and ordinary builds do
not run Python. The generator reports every unsupported declaration as a gap;
the complete-facade gate must reject any remaining gap.

Each upstream function maps to `ailo_<upstream_name>`. The facade returns an
`ailo_status` describing bridge validation, conversion, allocation or exception
failure. An upstream nonvoid result has a separate final output parameter before
the optional `ailo_error`. Native return codes, including errors and nullable
results, are preserved there rather than being conflated with bridge status.
Outputs are reset before validation; success clears the optional diagnostic.
Native preconditions still apply to raw operations. A successful bridge status
does not convert a native error result into a successful native operation.

Native opaque handles and public native records have project-prefixed opaque C
tags. Clients never include native headers or dereference native layouts.
Record accessors expose every field, including array elements and anonymous
union/struct members, using explicit scalar conversions. A by-value record
argument becomes a pointer to its native-backed facade record; a by-value native
result becomes a separately allocated facade record. These result records must
be released with the corresponding record cleanup function.

Record allocation helpers create zero-initialized native record arrays, not
upstream resources. They do not substitute for upstream default-parameter
factories or model/context constructors. Record cleanup frees only records
created by these helpers and by-value result wrappers, not borrowed views or
resources returned by native constructors. Resource ownership remains governed
by the upstream create/free operation pair. Cleanup accepts NULL.

Record-array element access takes the caller-declared extent and index. It
checks that index against the declared extent; the caller must supply the actual
extent and keep the backing allocation/owner alive. The facade does not pretend
to discover a borrowed native allocation's length. Returned element views are
borrowed and never individually freed.

Exact native scalar arrays whose C89 and native element types are identical
(bytes, short/int, float/double and ordinary character/void pointers) can remain
ordinary typed arrays. Native 64-bit integers, booleans, enums and arrays of
opaque-object pointers instead use native-backed opaque storage with typed
allocate, element-read, element-write and release helpers. These helpers preserve
the native element type. They do not cast arrays of public word-pair integers,
int booleans or different struct-pointer types into native arrays. Extent and
lifetime rules are explicit as for record arrays.

Backend-only APIs keep their inventory entries and availability conditions. A
disabled backend returns `AILO_UNSUPPORTED` and safe output values from its
facade entry point; it does not fabricate a successful native result. Required
CPU/Vulkan implementations are built and exercised separately. Native nullptr
results from an enabled backend retain native semantics.

All C++ exceptions are caught at the C boundary. Each shared-library export is
an explicit C declaration in the project allowlist; native and C++ implementation
symbols remain hidden. The static archive retains separate native and SDK link
dependencies instead of merging their archives into `libailo.a`.

Callback registration and callable native callback views require additional
typed adapters, with independent registration state and explicit lifetimes.
That implementation choice remains pending. Until those adapters and the two
native variadic functions are implemented and tested, the facade is incomplete.
The progress manifest must distinguish implemented functions and record fields
from these gaps; no declaration-only placeholder counts as coverage.
