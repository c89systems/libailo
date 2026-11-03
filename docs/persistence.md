# Default persistent storage

Without an application-supplied `openai.repository`, libailo uses liblockdc
0.27.0's embedded Pouch engine (core ABI 9), with liblql 0.4.1. No lockd server
or host library is required.

| Resource | Default | When the XDG variable is empty, relative or unset |
| --- | --- | --- |
| Pouch root | `$XDG_STATE_HOME/ailo` | `$HOME/.local/state/ailo` |
| Encryption key | `$XDG_CONFIG_HOME/ailo/pouch.key` | `$HOME/.config/ailo/pouch.key` |

`HOME` must be absolute when its fallback is needed. Construction and stateless
requests perform no storage I/O. The first storage operation creates private
mode-0700 application directories and lets Pouch generate a mode-0600 key file.
Existing stores require their original key; a missing key fails without creating
a replacement. Back up the state root and key together, and retain the key for
as long as retained records need to be read. Existing application directories
must be owned by the current user and have mode 0700.

C callers can override `ailo_openai_config.state_directory` and
`encryption_key_file`; Lua uses the same names in the receiver constructor's
`openai` table. Both paths are copied and must be absolute. An omitted namespace
uses `default`; applications serving different tenants must supply trusted,
separate namespaces. Wire metadata never selects or authorizes a namespace.
Custom repository callbacks override Pouch completely.

```lua
local client = assert(ailo.new({
  source = {local_path = "/models/model.gguf"},
  openai = {
    namespace_id = "trusted-tenant",
    state_directory = "/var/lib/my-service/ailo",
    encryption_key_file = "/etc/my-service/ailo/pouch.key",
  },
}))
```

## Publication and streaming

Payload sections are written incrementally into immutable 32 KiB chunks,
compressed with streaming zlib and encrypted by Pouch. Chunk documents use
base64 framing; complete responses, items and pages are never accumulated or
spooled by this provider. Small record descriptors contain chunk references and
query metadata. Pouch's KV projection
indexes keys; libailo maintains resident metadata indexes for its API queries.
The unrelated Pouch document/full-text indexer is disabled.

A transaction writes its descriptors, prepares aliases carrying both old and
new references, then durably publishes one commit marker. All response,
conversation and item changes resolve through that single publication point.
Reopening resolves interrupted aliases to their previous committed descriptors.
Records are not reconstructed into a complete JSON value when read; stable
readers pin immutable versions and forward chunks directly to their consumers.
Slow consumers do not hold the repository commit mutex.

Clients in one process share one exclusive Pouch root owner. Different
conversations may remain reserved concurrently; conflicting mutations of a
conversation, record or item ID return busy. Physical publication is serialized
briefly rather than held through model generation. A second process opening
the same root is rejected by Pouch's exclusive ownership policy. Use separate
roots or an application-owned repository when another process topology is needed.

Chat completions are stored only on explicit `store=true` by default, and expire
30 days from creation. `openai.chat_retention_seconds=0` selects 30 days;
nonzero values select a finite TTL. Reads and metadata updates do not extend it.
The optional trusted `chat_store_by_default` setting enables default storage
with the same expiry policy. Omitted Chat storage creates neither Pouch files
nor an encryption key. TTL changes apply to newly created records; existing
records retain their original deadlines.

Durable sync is enabled. Startup and a maintenance worker remove expired
response records and unreferenced storage objects; the worker runs every minute.
Open snapshots and admitted transactions protect their referenced chunks.
Pouch's background compaction reclaims obsolete log records. Conversations and
their item archives have no automatic TTL, and deleting a conversation retains
its item rows. ZDR or `store=false` avoids response persistence; explicitly
attached conversations retain their separate documented lifetime.

Admission checks the configured record limit and available unreserved disk
capacity before inference. External disk consumption can still cause subsequent
I/O failure. API errors abort unpublished state. Inference KV caches and
authenticated compaction/reasoning keys keep their existing separate policies;
Pouch encryption does not configure the compactor's replay keys.

An explicit zlib setting requests synchronous conversion of an existing
uncompressed root. Omitting compression inherits the stored mode. libailo
accepts the SDK's conversion on open and continues to request encrypted zlib
storage. Conversion preserves payloads, keys and logical record identities.

liblockdc 0.27 replaces one canonical segment or snapshot at a time, using
bounded chunk scratch. It does not duplicate the retained root. Allow temporary
space for the largest converted file plus recovery metadata and rebuilt indexes;
snapshots and oversized records can exceed the normal segment target.
Decompression additionally needs the final root's unavoidable growth. This
replaces the whole-root temporary-space requirement of 0.26.

Opening a root can take time and fail for insufficient space or an I/O error.
Conversion holds exclusive ownership and resumes after interruption before
admitting clients, even if the next open omits compression. Correct the reported
problem and reopen with this SDK; do not remove its intent or recovery files.
This does not change libailo's logical record layout or adopt collections.
See the [released conversion contract](https://github.com/c89systems/liblockdc/blob/v0.27.0/docs/pouch-compression.md).

## Collection adoption

The accepted collection policy is 90 days of conversation inactivity, renewed
by a newly committed append. Reads do not renew it. This policy is not active
in the current chunk-backed provider. The released SDK now provides public
push-fed append writers and atomic parent-retention renewal, verified through
both static and shared consumers. Its push writer stages encoded entries in
up to 64 KiB of memory and spills larger encoded entries to an anonymous file;
commit copies the staged entry. This is staged/spooled append, and does not
satisfy libailo's end-to-end streaming requirement. Collection reads deliver
bounded chunks directly. Resolve that write-path contract before adopting it
for conversation payloads; the upgrade alone does not change their provider.
The existing commit-marker visibility mechanism can be retained; collection
batches and XA participation are not required. Collection adoption will use a
released liblockdc SDK, with attachments and opaque inference state kept separate.
See [the implementation backlog](../TODO.md) for adoption and verification gates,
and the [upstream writer contract](https://github.com/c89systems/liblockdc/blob/v0.27.0/docs/collection-writer.md)
for staging and retention semantics.
