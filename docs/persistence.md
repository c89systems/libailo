# Default persistent storage

Without an application-supplied `openai.repository`, libailo uses liblockdc
0.21.0's embedded Pouch engine. No lockd server or host library is required.

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
encrypted by Pouch. Chunk documents use base64 framing; complete responses,
items and pages are never accumulated or spooled by this provider. Small record
descriptors contain chunk references and query metadata. Pouch's KV projection
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
