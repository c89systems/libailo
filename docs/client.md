# Inference receiver

`include/ailo/client.h` provides the first implemented shared inference core.
It calls the pinned llama library in the embedding process. Chat Completions
and Responses wire adapters are separate work under the broader
[backend contract](inference-backend.md).

Construct an `ailo_client` with `ailo_client_new`, then call its `generate`,
`model_card`, `capabilities`, `devices`, `cancel` and `destroy` methods.
Equivalent `ailo_client_*` functions are exported. Configuration strings and
checksum arrays are copied; callback user state remains borrowed until destroy.
Requests are borrowed until the synchronous call returns. Each callback borrows
its event and payload only until it returns.

The constructor uses the existing `ailo_model_source` acquisition facility,
including explicit download permission, checksums, sharded files, credentials,
progress, limits and configurable XDG caching. Lazy construction validates and
copies configuration without downloading or loading weights. Model cards and
generation force a lazy load; device and capability queries do not acquire files.

## Generation

Supply ordered message items with UTF-8 text parts. The model's stored Jinja
template is interpreted by selected private llama helpers; named native
templates such as `chatml` and explicit overrides are also supported. Model
files and templates are trusted executable inputs: the interpreter is not a
sandbox. The rendered prompt has an 8 MiB acceptance limit, applied after
template execution. Request text is limited to 4 MiB, 4096 items and 4096 parts
per message. Embedded NUL text currently returns unsupported because the
private message boundary uses C strings.

Generation emits run begin, choice begin, text deltas, choice end and run end.
Each delta goes to the sink before the next native token evaluation. There is
no retained complete answer, output spool or whole-answer JSON conversion.
Only an incomplete UTF-8 character and a possible stop-string prefix can delay
text. Four stop strings of up to 4096 bytes each are supported; matched stops
and subsequent bytes are excluded from output. The token-piece safety limit
is 1 MiB. A consumer can forward each delta directly to lonejson's incremental
string writer, as demonstrated in [examples/infer.c](../examples/infer.c).

The sink returns positive to accept, zero to cancel or negative to report sink
failure. Cancellation and failures return their status immediately, without a
synthetic successful terminal event. Run info retains usage observed so far.
Sampled EOS and stop-producing tokens count toward completion usage. Prompt
usage counts the input once; completion usage totals all candidates. Up to 16
candidates are generated sequentially, with independent sampler/context state.

Sampling supports greedy or temperature/top-p selection, seeds, and frequency
and presence penalties. Zero configuration selects auto backend, eager loading,
two CPU threads, up to 4096 context tokens and up to 512 batch tokens. Native
context padding can increase the effective context; the model card reports
requested and effective values separately. A request exceeding the effective
context fails before events or decoding. There is no silent input truncation.

CPU selection forces CPU model placement. Vulkan selection requires an eligible
built-in Vulkan device. Auto selects available Vulkan devices and retries on
CPU if GPU model/context setup fails. Native layer splitting uses all selected
Vulkan devices. Explicit per-device placement and full static RADV/ANV deployment
remain required work; this change does not qualify them.

One operation owns a client at a time. Concurrent calls and callback reentry
return busy. Cancellation is allowed from a callback or another thread during
an active operation. It is reset for the next operation. Join all calls before
destroying the receiver. Process backend ownership is shared with balanced
native-facade initialization; destroying one receiver preserves other owners.

## Model cards and current capabilities

Model cards, capabilities and devices return explicitly buffered, bounded owned
JSON strings. Free them with `ailo_string_free`. Cards report loaded GGUF/native
identity, source, license, architecture, parameter count, actual tensor types,
context limits, template fingerprint and file/checksum provenance. They do not
infer facts from filenames or expose local paths or credentials. Unknown facts
remain null. Publisher metadata enrichment is not yet implemented. Template
support means the named helper recognizes it or the interpreter can parse it;
rendering can still reject a conversation the template does not accept.

Capabilities currently advertise text, streaming and multiple candidates.
Tools, structured output, separated reasoning, media, prompt/KV reuse,
persistent snapshots and compaction are reported unavailable. Their request
item kinds return unsupported rather than losing semantics. These remain
implementation requirements in the full specification; no cache/compaction or
protocol handler is claimed by this initial core.

## Usage and verification

Build with `make build PRESET=cpu`, then run:

```sh
build/cpu/ailo_infer_example --local /absolute/model.gguf --backend cpu \
  --prompt 'Hello' --tokens 32
```

The example writes one JSON string incrementally through lonejson and flushes
each sink write. An error after output begins leaves an incomplete JSON value
and a nonzero exit status. `--card` prints the model card instead. Use `--help`
for Hugging Face source and cache options.

Deterministic static/shared C89 tests prove first-token delivery through
lonejson before token two, lifecycle ordering, multiple candidates, cross-token
stops, cancellation, sink errors, busy/reentry and independent backend owners.
Additional tests cover cross-thread cancellation, callback exception recovery,
stored Jinja semantics, EOS termination/usage, split UTF-8 and immediate delivery
of long output.
Installed C89 and C++ consumers execute generation through CMake/pkg-config.

A real CPU smoke run used `bartowski/SmolLM2-135M-Instruct-GGUF`, revision
`09816acd5d99df7be770d85ea30822623dab342c`, file
`SmolLM2-135M-Instruct-Q4_K_M.gguf`, SHA-256
`2e8040ceae7815abe0dcb3540b9995eaa1fa0d2ca9e797d0a635ae4433c68c2d`.
The receiver acquired and validated the 105,454,432-byte file under an owned
`build/` cache, generated through the stored Jinja template, and reopened the
cached model offline to obtain its card. This proves the integration, not model
answer quality or GPU qualification. Routine tests require no network download.

The same cached instruct model also generated successfully through a fully
static x86_64 musl executable. `readelf -l` showed no interpreter and `readelf -d`
reported no dynamic section. The build used the pinned musl Bootlin compiler
and cpkt/lonejson static archives, with no host target libraries:

```sh
cmake -S . -B build/cpu-musl-static -G Ninja \
  -DAILO_TARGET_ID=x86_64-linux-musl -DAILO_VULKAN=OFF \
  -DAILO_BUILD_SHARED=OFF -DAILO_BUILD_TESTS=OFF -DBUILD_TESTING=OFF \
  -DAILO_INSTALL=OFF -DCMAKE_BUILD_TYPE=Release \
  -DCMAKE_EXE_LINKER_FLAGS=-static
cmake --build build/cpu-musl-static --target ailo_infer_example --parallel 2
```

This is CPU executable evidence; it does not qualify static GPU inference or
replace the required release matrix. Dependency preparation must precede this
direct CMake invocation.

The private Jinja helper uses the MIT-licensed nlohmann JSON implementation
already vendored in the pinned llama source. It is restricted to request/template
handling; generated text passes directly through the event sink. Public JSON
serialization uses the separate pinned lonejson SDK. Template/native/compiler
runtime archives remain separate from `libailo.a` in the installed static SDK.
