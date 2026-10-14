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

The backend init/free pair participates in the receiver's shared process-wide
ownership. Balance every successful `ailo_llama_backend_init` with one
`ailo_llama_backend_free`, and keep that ownership while low-level model/context
resources remain live. Releasing one owner preserves other native/receiver
owners. Unbalanced raw free calls do not provide a valid ownership boundary.

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
That implementation choice remains pending. Until those adapters are implemented
and tested, the facade is incomplete. The progress manifest must distinguish
implemented functions and record fields from these gaps; no declaration-only
placeholder counts as coverage.

The two native variadic operations are implemented separately in
`ailo/native_variadic.h`. Tensor-name formatting preserves the native bounded
buffer, return view and printf argument semantics. Fatal diagnostics preserve
the native file/line prefix, truncation, abort hook and process termination;
invalid file/format arguments or an oversized prefix fail before aborting.

`ailo/native_macros.h` provides C89 compositions for the public macros. Variadic
preprocessing uses a parenthesized argument tuple instead, for example
`AILO_GGML_ABORT((__FILE__, __LINE__, NULL, "failure %d", code))`.
`AILO_GGML_UNUSED_VARS((0, a, b, c))` requires an initial marker and references
its arguments without evaluating them. Tensor locals use `ailo_i64` for `ne`
and `size_t` for `nb`, reading opaque native records through typed helpers.
`AILO_GGML_TENSOR_SIZE` queries the native size at runtime; it is not a C89
constant expression. Annotation macros omit non-C89 compiler attributes.
Native header guards are classified separately from API capabilities.

At this pinned revision, restoring model state into a fresh context requires a
subsequent decode before logits can be queried. The native model tests compare
continuation logits against an uninterrupted context after both full-context
and sequence state restoration.

The remaining 83 inventory gaps comprise 30 callback typedefs, 25 record fields
and 28 functions. The dependency decision is required by concrete pinned API
shapes, including `ggml_to_float_t`/`ggml_from_float_t` (native `int64_t` length,
no userdata), `ggml_backend_comm_init_t` (no registration context), and fatal
abort callback chaining (no userdata). C89 word-pair integers require explicit
ABI conversion. Callable native views can use typed invocation helpers, but
installing independent C89 bindings into these native function-pointer slots
also needs a correctly typed, independently owned native call entry.

The private libffi 3.8.0 dependency is provided by checksum-pinned cpktmisc
0.2.0, independently installed alongside cpkt 0.3.0. Its native closure support
is verified by static/shared dependency tests and remains a separate library. The C89 surface would own
callback handles with explicit retain/release and bound-owner lifetimes; no
libffi/C++ types would be installed. Conversion/error tests must cover independent
registrations, callback chaining, returned native views, native booleans and
64-bit values, by-value optimizer results, sampler clones/destruction and
asynchronous/retained use. The callback bridge itself is still pending; dependency acquisition and native
closure verification are implemented.
Incompatible function-pointer casts, a fixed/global callback-slot substitute,
or handwritten executable trampolines are not an approved complete alternative.
