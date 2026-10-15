/* Private Lua 5.5 bridge. Only installed, public C headers are used. */
#include <ailo/client.h>
#include <ailo/compaction.h>
#include <ailo/continuation.h>
#include <ailo/version.h>
#include <lauxlib.h>
#include <limits.h>
#include <lualib.h>
#include <pslog_lua.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#define CLIENT "ailo.client"
#define WORK "ailo.work"
#define CACHE "ailo.cache"
#define CONTINUATION "ailo.continuation"
#define COMPACTOR "ailo.compactor"
typedef struct compactor {
  ailo_compactor *native;
} compactor;
typedef struct continuation {
  ailo_continuation *native;
  int busy;
} continuation;
typedef struct cache {
  ailo_cache *native;
} cache;
typedef int (*check_logger_fn)(lua_State *, int, pslog_lua_logger_view *);
typedef int (*ref_logger_fn)(lua_State *, int, const pslog_field *, size_t,
                             pslog_lua_logger_ref *);
typedef void (*unref_logger_fn)(pslog_lua_logger_ref *);
typedef struct client {
  ailo_client *native;
  pslog_lua_logger_ref logger;
  unref_logger_fn unref;
  lua_State *active;
  int active_index;
  int busy;
  int progress_failed;
} client;
typedef struct work {
  ailo_item *items;
  ailo_content *content;
  const char **strings;
  ailo_logit_bias *biases;
  ailo_function_tool *tools;
  const char **allowed;
  ailo_compaction_key *keys;
  char *json;
  lua_State *L;
  int index;
  int failed;
} work;

static int failure(lua_State *L, ailo_status status, const char *message) {
  lua_pushnil(L);
  lua_createtable(L, 0, 4);
  lua_pushstring(L, ailo_status_name(status));
  lua_setfield(L, -2, "code");
  lua_pushinteger(L, status);
  lua_setfield(L, -2, "status");
  lua_pushstring(L, message);
  lua_setfield(L, -2, "message");
  return 2;
}
static void set_integer(lua_State *L, const char *key, lua_Integer value) {
  lua_pushinteger(L, value);
  lua_setfield(L, -2, key);
}
static void set_string(lua_State *L, const char *key, const char *value) {
  if (value) {
    lua_pushstring(L, value);
    lua_setfield(L, -2, key);
  }
}
static void push_u64(lua_State *L, ailo_u64 value) {
  if (value.hi <= 0x7fffffffu)
    lua_pushinteger(L, (lua_Integer)(((uint64_t)value.hi << 32) | value.lo));
  else {
    lua_createtable(L, 0, 2);
    set_integer(L, "hi", value.hi);
    set_integer(L, "lo", value.lo);
  }
}
static void usage(lua_State *L, const ailo_usage *value) {
  lua_createtable(L, 0, 4);
  push_u64(L, value->prompt_tokens);
  lua_setfield(L, -2, "prompt_tokens");
  push_u64(L, value->completion_tokens);
  lua_setfield(L, -2, "completion_tokens");
  push_u64(L, value->cached_tokens);
  lua_setfield(L, -2, "cached_tokens");
  push_u64(L, value->reasoning_tokens);
  lua_setfield(L, -2, "reasoning_tokens");
  push_u64(L, value->compaction_prompt_tokens);
  lua_setfield(L, -2, "compaction_prompt_tokens");
  push_u64(L, value->compaction_completion_tokens);
  lua_setfield(L, -2, "compaction_completion_tokens");
  push_u64(L, value->context_replay_tokens);
  lua_setfield(L, -2, "context_replay_tokens");
  set_integer(L, "compaction_passes", value->compaction_passes);
}
static const char *finish(ailo_finish_reason value) {
  switch (value) {
  case AILO_FINISH_STOP:
    return "stop";
  case AILO_FINISH_LENGTH:
    return "length";
  case AILO_FINISH_TOOL_CALLS:
    return "tool_calls";
  default:
    return "none";
  }
}
static void info(lua_State *L, const ailo_run_info *value) {
  lua_createtable(L, 0, 4);
  usage(L, &value->usage);
  lua_setfield(L, -2, "usage");
  set_integer(L, "completed_candidates", value->completed_candidates);
  set_string(L, "id", value->id[0] ? value->id : NULL);
  set_string(L, "finish_reason", finish(value->finish_reason));
  set_string(L, "actual_backend",
             value->actual_backend == AILO_BACKEND_VULKAN ? "vulkan"
             : value->actual_backend == AILO_BACKEND_CPU  ? "cpu"
                                                          : "auto");
}
static lua_Integer integer(lua_State *L, int index, const char *key) {
  lua_getfield(L, index, key);
  lua_Integer value = luaL_checkinteger(L, -1);
  lua_pop(L, 1);
  return value;
}
static double number(lua_State *L, int index, const char *key) {
  lua_getfield(L, index, key);
  double value = luaL_checknumber(L, -1);
  lua_pop(L, 1);
  return value;
}
static const char *string(lua_State *L, int index, const char *key) {
  lua_getfield(L, index, key);
  const char *value = lua_isnil(L, -1) ? NULL : luaL_checkstring(L, -1);
  lua_pop(L, 1);
  return value;
}
static int boolean(lua_State *L, int index, const char *key) {
  lua_getfield(L, index, key);
  int value = lua_toboolean(L, -1);
  lua_pop(L, 1);
  return value;
}
static int cache_gc(lua_State *L) {
  cache *value = luaL_checkudata(L, 1, CACHE);
  ailo_cache_destroy(value->native);
  value->native = NULL;
  return 0;
}
static int close_cache(lua_State *L) {
  cache_gc(L);
  lua_pushboolean(L, 1);
  return 1;
}
static int new_cache(lua_State *L) {
  luaL_checktype(L, 1, LUA_TTABLE);
  cache *value = lua_newuserdatauv(L, sizeof(*value), 0);
  value->native = NULL;
  luaL_setmetatable(L, CACHE);
  ailo_cache_config config = ailo_cache_default_config();
  config.namespace_id = string(L, 1, "namespace_id");
  config.max_memory_bytes = (size_t)integer(L, 1, "max_memory_bytes");
  config.max_entries = (size_t)integer(L, 1, "max_entries");
  config.idle_ttl_seconds = (ailo_u32)integer(L, 1, "idle_ttl_seconds");
  config.directory = string(L, 1, "directory");
  config.key_id = string(L, 1, "key_id");
  config.max_disk_bytes = (size_t)integer(L, 1, "max_disk_bytes");
  config.disk_ttl_seconds = (ailo_u32)integer(L, 1, "disk_ttl_seconds");
  config.max_disk_entries = (size_t)integer(L, 1, "max_disk_entries");
  config.max_snapshot_bytes = (size_t)integer(L, 1, "max_snapshot_bytes");
  lua_getfield(L, 1, "key");
  if (!lua_isnil(L, -1))
    config.key = luaL_checklstring(L, -1, &config.key_size);
  lua_pop(L, 1);
  ailo_error error = {0};
  ailo_status status = ailo_cache_new(&config, &value->native, &error);
  if (status != AILO_OK)
    return failure(L, status, error.message);
  return 1;
}
static int cache_statistics(lua_State *L, const ailo_cache_stats *value) {
  lua_createtable(L, 0, 20);
#define COUNT(name)                                                            \
  push_u64(L, value->name);                                                    \
  lua_setfield(L, -2, #name)
  COUNT(hits);
  COUNT(misses);
  COUNT(evictions);
  COUNT(oversized);
  COUNT(invalid);
  COUNT(busy_skips);
  COUNT(cached_tokens);
  COUNT(evaluated_tokens);
  COUNT(lookup_microseconds);
  COUNT(capture_microseconds);
  COUNT(prefill_microseconds);
  COUNT(snapshot_reads);
  COUNT(snapshot_writes);
  COUNT(snapshot_errors);
  COUNT(snapshot_skipped);
  COUNT(disk_evictions);
#undef COUNT
  set_integer(L, "entries", (lua_Integer)value->entries);
  set_integer(L, "memory_bytes", (lua_Integer)value->memory_bytes);
  set_integer(L, "disk_entries", (lua_Integer)value->disk_entries);
  set_integer(L, "disk_bytes", (lua_Integer)value->disk_bytes);
  return 1;
}
static int cache_operation(lua_State *L, int kind) {
  cache *value = luaL_checkudata(L, 1, CACHE);
  if (!value->native)
    return failure(L, AILO_INVALID_ARGUMENT, "cache is closed");
  ailo_cache_stats output = {0};
  ailo_error error = {0};
  ailo_status status =
      kind == 0   ? ailo_cache_get_stats(value->native, &output, &error)
      : kind == 1 ? ailo_cache_clear(value->native, &error)
                  : ailo_cache_prune(value->native, &error);
  if (status != AILO_OK)
    return failure(L, status, error.message);
  if (kind == 0)
    return cache_statistics(L, &output);
  lua_pushboolean(L, 1);
  return 1;
}
static int cache_stats(lua_State *L) { return cache_operation(L, 0); }
static int cache_clear(lua_State *L) { return cache_operation(L, 1); }
static int cache_prune(lua_State *L) { return cache_operation(L, 2); }
static void release_work(work *value) {
  free(value->items);
  free(value->content);
  free(value->strings);
  free(value->biases);
  free(value->tools);
  free(value->allowed);
  free(value->keys);
  ailo_string_free(value->json);
  value->items = NULL;
  value->content = NULL;
  value->strings = NULL;
  value->biases = NULL;
  value->tools = NULL;
  value->allowed = NULL;
  value->keys = NULL;
  value->json = NULL;
}
static int work_gc(lua_State *L) {
  release_work(luaL_checkudata(L, 1, WORK));
  return 0;
}
static work *new_work(lua_State *L) {
  work *value = lua_newuserdatauv(L, sizeof(*value), 4);
  memset(value, 0, sizeof(*value));
  luaL_setmetatable(L, WORK);
  value->L = L;
  value->index = lua_gettop(L);
  return value;
}
static void *allocate(lua_State *L, size_t count, size_t size) {
  if (size && count > SIZE_MAX / size)
    luaL_error(L, "request exceeds native address space");
  void *value = calloc(count ? count : 1, size);
  if (!value)
    luaL_error(L, "request allocation failed");
  return value;
}
static int compactor_gc(lua_State *L) {
  compactor *value = luaL_checkudata(L, 1, COMPACTOR);
  ailo_compactor_destroy(value->native);
  value->native = NULL;
  return 0;
}
static int compactor_close(lua_State *L) {
  compactor_gc(L);
  lua_pushboolean(L, 1);
  return 1;
}
static int compactor_info(lua_State *L) {
  compactor *value = luaL_checkudata(L, 1, COMPACTOR);
  ailo_compactor_info out = {0};
  ailo_error error = {0};
  ailo_status status = ailo_compactor_get_info(value->native, &out, &error);
  if (status != AILO_OK)
    return failure(L, status, error.message);
  lua_createtable(L, 0, 4);
  lua_pushboolean(L, out.ephemeral);
  lua_setfield(L, -2, "ephemeral");
  set_integer(L, "decryption_keys", (lua_Integer)out.decryption_keys);
  set_integer(L, "max_item_bytes", (lua_Integer)out.max_item_bytes);
  set_integer(L, "ttl_seconds", out.ttl_seconds);
  return 1;
}
static ailo_compaction_key read_compaction_key(lua_State *L, int index) {
  ailo_compaction_key out = {0};
  out.id = string(L, index, "key_id");
  lua_getfield(L, index, "key");
  if (!lua_isnil(L, -1))
    out.bytes = luaL_checklstring(L, -1, &out.size);
  lua_pop(L, 1);
  return out;
}
static int new_compactor(lua_State *L) {
  luaL_checktype(L, 1, LUA_TTABLE);
  compactor *out = lua_newuserdatauv(L, sizeof(*out), 0);
  out->native = NULL;
  luaL_setmetatable(L, COMPACTOR);
  int result = lua_gettop(L);
  work *storage = new_work(L);
  lua_pushvalue(L, 1);
  lua_setiuservalue(L, storage->index, 3);
  ailo_compactor_config config = ailo_compactor_default_config();
  config.namespace_id = string(L, 1, "namespace_id");
  config.current_key = read_compaction_key(L, 1);
  config.ttl_seconds = (ailo_u32)integer(L, 1, "ttl_seconds");
  config.max_item_bytes = (size_t)integer(L, 1, "max_item_bytes");
  lua_getfield(L, 1, "old_keys");
  int keys = lua_gettop(L);
  config.old_key_count = lua_rawlen(L, keys);
  if (config.old_key_count) {
    storage->keys =
        allocate(L, config.old_key_count, sizeof(ailo_compaction_key));
    for (size_t i = 0; i < config.old_key_count; ++i) {
      lua_geti(L, keys, (lua_Integer)i + 1);
      storage->keys[i] = read_compaction_key(L, lua_gettop(L));
      lua_pop(L, 1);
    }
    config.old_keys = storage->keys;
  }
  ailo_error error = {0};
  ailo_status status = ailo_compactor_new(&config, &out->native, &error);
  release_work(storage);
  if (status != AILO_OK)
    return failure(L, status, error.message);
  lua_pushvalue(L, result);
  return 1;
}
static int client_gc(lua_State *L) {
  client *value = luaL_checkudata(L, 1, CLIENT);
  if (value->native) {
    ailo_client_destroy(value->native);
    value->native = NULL;
  }
  if (value->unref) {
    value->unref(&value->logger);
    value->unref = NULL;
  }
  return 0;
}
static client *get_client(lua_State *L) {
  return luaL_checkudata(L, 1, CLIENT);
}
static int ready(lua_State *L, client *value) {
  if (value->busy)
    return failure(L, AILO_BUSY, "receiver is active");
  if (!value->native)
    return failure(L, AILO_INVALID_ARGUMENT, "receiver is closed");
  return 0;
}
static void begin(lua_State *L, client *value, int index) {
  value->busy = 1;
  value->active = L;
  value->active_index = index;
  value->progress_failed = 0;
  lua_pushnil(L);
  lua_setiuservalue(L, index, 3);
}
static void end(client *value) {
  value->active = NULL;
  value->active_index = 0;
  value->busy = 0;
}
static int progress_thunk(lua_State *L) {
  lua_getiuservalue(L, 1, 1);
  /* The native callback passes a stack-owned record valid through pcall. */
  const ailo_u64 *counts = lua_touserdata(L, 2);
  lua_createtable(L, 0, 4);
  set_integer(L, "shard", counts[0].lo);
  set_integer(L, "shards", counts[0].hi);
  push_u64(L, counts[1]);
  lua_setfield(L, -2, "received");
  push_u64(L, counts[2]);
  lua_setfield(L, -2, "total");
  lua_call(L, 1, 1);
  return 1;
}
static int progress(void *data, unsigned int shard, unsigned int shards,
                    ailo_u64 received, ailo_u64 total) {
  client *value = data;
  lua_State *L = value->active;
  if (!L || !lua_checkstack(L, 5)) {
    value->progress_failed = 1;
    return 0;
  }
  ailo_u64 counts[3] = {{shards, shard}, received, total};
  lua_pushcfunction(L, progress_thunk);
  lua_pushvalue(L, value->active_index);
  lua_pushlightuserdata(L, counts);
  if (lua_pcall(L, 2, 1, 0) != LUA_OK) {
    value->progress_failed = 1;
    lua_setiuservalue(L, value->active_index, 3);
    return 0;
  }
  int result = !(lua_isboolean(L, -1) && !lua_toboolean(L, -1));
  lua_pop(L, 1);
  return result;
}
static void cause(lua_State *L, int index, int slot) {
  lua_getiuservalue(L, index, slot);
  if (!lua_isnil(L, -1))
    lua_setfield(L, -2, "cause");
  else
    lua_pop(L, 1);
  lua_pushnil(L);
  lua_setiuservalue(L, index, slot);
}
/* Resolve only public provider symbols supplied by package.loadlib. */
#define RESOLVE(field, type, out)                                              \
  do {                                                                         \
    lua_getfield(L, helpers, field);                                           \
    lua_CFunction raw = lua_tocfunction(L, -1);                                \
    lua_pop(L, 1);                                                             \
    _Static_assert(sizeof(raw) == sizeof(type),                                \
                   "function pointer representation");                         \
    if (!raw)                                                                  \
      return failure(L, AILO_UNSUPPORTED,                                      \
                     "missing public pslog interop helper");                   \
    memcpy(&(out), &raw, sizeof(out));                                         \
  } while (0)
static int new_client(lua_State *L) {
  luaL_checktype(L, 1, LUA_TTABLE);
  check_logger_fn check = NULL;
  ref_logger_fn retain = NULL;
  unref_logger_fn unref = NULL;
  lua_getfield(L, 1, "logger");
  int logger_index = lua_gettop(L);
  if (!lua_isnil(L, logger_index)) {
    lua_getfield(L, 1, "helpers");
    int helpers = lua_gettop(L);
    luaL_checktype(L, helpers, LUA_TTABLE);
    RESOLVE("check_logger", check_logger_fn, check);
    RESOLVE("ref_logger", ref_logger_fn, retain);
    RESOLVE("unref_logger", unref_logger_fn, unref);
    pslog_lua_logger_view view = {sizeof(view), PSLOG_LUA_INTEROP_ABI_VERSION,
                                  NULL, 0};
    if (check(L, logger_index, &view) != 0 ||
        !(view.flags & PSLOG_LUA_LOGGER_NATIVE_SINK))
      return failure(
          L, AILO_INVALID_ARGUMENT,
          "logger must be a live pslog logger with a qualified native sink");
  }
  client *value = lua_newuserdatauv(L, sizeof(*value), 3);
  memset(value, 0, sizeof(*value));
  int client_index = lua_gettop(L);
  luaL_setmetatable(L, CLIENT);
  value->logger.size = sizeof(value->logger);
  value->logger.abi_version = PSLOG_LUA_INTEROP_ABI_VERSION;
  value->logger.registry_ref = LUA_NOREF;
  /* Reference helpers may allocate; finalizer already owns a zeroed reference.
   */
  value->unref = unref;
  if (retain && retain(L, logger_index, NULL, 0, &value->logger) != 0)
    return failure(L, AILO_INVALID_ARGUMENT, "cannot retain pslog logger");
  work *storage = new_work(L);
  lua_pushvalue(L, 1);
  lua_setiuservalue(L, client_index, 2);
  ailo_client_config config = ailo_client_default_config();
  config.model_id = string(L, 1, "model_id");
  config.chat_template = string(L, 1, "chat_template");
  config.backend = (ailo_backend_policy)integer(L, 1, "backend");
  config.lazy = boolean(L, 1, "lazy");
  config.gpu_layers = (int)integer(L, 1, "gpu_layers");
  config.threads = (int)integer(L, 1, "threads");
  config.context_tokens = (ailo_u32)integer(L, 1, "context_tokens");
  config.batch_tokens = (ailo_u32)integer(L, 1, "batch_tokens");
  config.disable_prompt_cache = boolean(L, 1, "disable_prompt_cache");
  lua_getfield(L, 1, "compactor");
  if (!lua_isnil(L, -1)) {
    compactor *owner = luaL_checkudata(L, -1, COMPACTOR);
    if (!owner->native)
      return failure(L, AILO_INVALID_ARGUMENT, "compactor is closed");
    config.compactor = owner->native;
  }
  lua_pop(L, 1);
  lua_getfield(L, 1, "cache");
  if (!lua_isnil(L, -1)) {
    cache *owner = luaL_checkudata(L, -1, CACHE);
    if (!owner->native)
      return failure(L, AILO_INVALID_ARGUMENT, "cache is closed");
    config.cache = owner->native;
  }
  lua_pop(L, 1);
  lua_getfield(L, 1, "source");
  int s = lua_gettop(L);
  luaL_checktype(L, s, LUA_TTABLE);
  config.source.local_path = string(L, s, "local_path");
  config.source.repo = string(L, s, "repo");
  config.source.revision = string(L, s, "revision");
  config.source.filename = string(L, s, "filename");
  config.source.cache_root = string(L, s, "cache_root");
  config.source.bearer_token = string(L, s, "bearer_token");
  config.source.allow_download = boolean(L, s, "allow_download");
  config.source.split_shards = boolean(L, s, "split_shards");
  config.source.max_attempts = (unsigned int)integer(L, s, "max_attempts");
  config.source.connect_timeout_ms = (long)integer(L, s, "connect_timeout_ms");
  config.source.transfer_timeout_ms =
      (long)integer(L, s, "transfer_timeout_ms");
  lua_getfield(L, s, "max_bytes");
  if (lua_istable(L, -1)) {
    config.source.max_bytes.hi = (ailo_u32)integer(L, lua_gettop(L), "hi");
    config.source.max_bytes.lo = (ailo_u32)integer(L, lua_gettop(L), "lo");
  } else {
    uint64_t n = (uint64_t)luaL_checkinteger(L, -1);
    config.source.max_bytes.hi = n >> 32;
    config.source.max_bytes.lo = (ailo_u32)n;
  }
  lua_pop(L, 1);
  lua_getfield(L, s, "sha256");
  int hashes = lua_gettop(L);
  size_t count = lua_rawlen(L, hashes);
  if (count) {
    storage->strings = allocate(L, count, sizeof(char *));
    for (size_t i = 0; i < count; ++i) {
      lua_geti(L, hashes, (lua_Integer)i + 1);
      storage->strings[i] =
          lua_isboolean(L, -1) ? NULL : luaL_checkstring(L, -1);
      lua_pop(L, 1);
    }
    config.source.sha256 = storage->strings;
    config.source.sha256_count = count;
  }
  lua_getfield(L, s, "progress");
  if (!lua_isnil(L, -1)) {
    luaL_checktype(L, -1, LUA_TFUNCTION);
    lua_setiuservalue(L, client_index, 1);
    config.source.progress = progress;
    config.source.user_data = value;
  } else
    lua_pop(L, 1);
  ailo_error error = {0};
  begin(L, value, client_index);
  ailo_status status = ailo_client_new_with_logger(
      &config, value->logger.borrowed, &value->native, &error);
  end(value);
  release_work(storage);
  if (status != AILO_OK) {
    if (value->progress_failed) {
      status = AILO_SINK_FAILED;
      strcpy(error.message, "model progress callback failed");
    }
    failure(L, status, error.message);
    cause(L, client_index, 3);
    if (value->unref) {
      value->unref(&value->logger);
      value->unref = NULL;
    }
    return 2;
  }
  /* C copied the config. Progress lives in uservalue1, logger in provider ref.
   */
  lua_pushnil(L);
  lua_setiuservalue(L, client_index, 2);
  lua_pushvalue(L, client_index);
  return 1;
}
static void compaction_info(lua_State *, const ailo_compaction_info *);
static void push_item(lua_State *, const ailo_item *);
static int event_thunk(lua_State *L) {
  const ailo_event *value = lua_touserdata(L, 2);
  lua_getiuservalue(L, 1, 1);
  static const char *names[] = {"run_begin",
                                "choice_begin",
                                "text_delta",
                                "choice_end",
                                "run_end",
                                "token",
                                "item_begin",
                                "content_begin",
                                "content_end",
                                "item_end",
                                "usage",
                                "function_name_delta",
                                "function_arguments_delta",
                                "reasoning_delta",
                                "compaction"};
  lua_createtable(L, 0, 14);
  set_string(L, "kind", names[value->kind]);
  push_u64(L, value->sequence);
  lua_setfield(L, -2, "sequence");
  set_integer(L, "candidate", value->candidate);
  set_string(L, "model_id", value->model_id);
  set_string(L, "run_id", value->run_id);
  set_string(L, "call_id", value->call_id);
  set_string(L, "name", value->name);
  if (value->item_id) {
    static const char *kinds[] = {"message", "function_call", "function_result",
                                  "reasoning", "compaction"};
    set_string(L, "item_id", value->item_id);
    set_string(L, "item_kind", kinds[value->item_kind]);
    set_integer(L, "item_index", value->item_index);
    set_integer(L, "content_index", value->content_index);
  }
  lua_pushlstring(L, value->data ? value->data : "", value->size);
  lua_setfield(L, -2, "data");
  set_string(L, "finish_reason", finish(value->finish_reason));
  usage(L, &value->usage);
  lua_setfield(L, -2, "usage");
  if (value->compaction) {
    compaction_info(L, value->compaction);
    lua_setfield(L, -2, "compaction");
    lua_createtable(L, (int)value->compacted_item_count, 0);
    for (size_t i = 0; i < value->compacted_item_count; ++i) {
      push_item(L, &value->compacted_items[i]);
      lua_seti(L, -2, (lua_Integer)i + 1);
    }
    lua_setfield(L, -2, "compacted_items");
  }
  if (value->token) {
    const ailo_token_info *token = value->token;
    lua_createtable(L, 0, 7);
    push_u64(L, token->index);
    lua_setfield(L, -2, "index");
    push_u64(L, token->byte_offset);
    lua_setfield(L, -2, "byte_offset");
    lua_pushboolean(L, token->end_of_generation);
    lua_setfield(L, -2, "end_of_generation");
    set_integer(L, "token_id", token->chosen.token_id);
    lua_pushlstring(L, token->chosen.data, token->chosen.size);
    lua_setfield(L, -2, "data");
    lua_pushnumber(L, token->chosen.logprob);
    lua_setfield(L, -2, "logprob");
    lua_createtable(L, (int)token->top_count, 0);
    for (size_t i = 0; i < token->top_count; ++i) {
      const ailo_token_probability *p = &token->top[i];
      lua_createtable(L, 0, 3);
      set_integer(L, "token_id", p->token_id);
      lua_pushlstring(L, p->data, p->size);
      lua_setfield(L, -2, "data");
      lua_pushnumber(L, p->logprob);
      lua_setfield(L, -2, "logprob");
      lua_seti(L, -2, (lua_Integer)i + 1);
    }
    lua_setfield(L, -2, "top_logprobs");
    lua_setfield(L, -2, "token");
  }
  lua_call(L, 1, 1);
  return 1;
}
static int event(void *data, const ailo_event *value) {
  work *storage = data;
  lua_State *L = storage->L;
  if (!lua_checkstack(L, 5)) {
    storage->failed = 1;
    return -1;
  }
  lua_pushcfunction(L, event_thunk);
  lua_pushvalue(L, storage->index);
  lua_pushlightuserdata(L, (void *)value);
  if (lua_pcall(L, 2, 1, 0) != LUA_OK) {
    storage->failed = 1;
    lua_setiuservalue(L, storage->index, 4);
    return -1;
  }
  int result = !(lua_isboolean(L, -1) && !lua_toboolean(L, -1));
  lua_pop(L, 1);
  return result;
}
static void read_items(lua_State *L, work *storage, int request_index,
                       ailo_request *request) {
  lua_getfield(L, request_index, "items");
  int items = lua_gettop(L);
  size_t count = lua_rawlen(L, items), parts = 0;
  storage->items = allocate(L, count, sizeof(ailo_item));
  for (size_t i = 0; i < count; ++i) {
    lua_geti(L, items, (lua_Integer)i + 1);
    lua_getfield(L, -1, "content");
    size_t n = lua_rawlen(L, -1);
    if (n > SIZE_MAX - parts)
      luaL_error(L, "too many content parts");
    parts += n;
    lua_pop(L, 2);
  }
  storage->content = allocate(L, parts, sizeof(ailo_content));
  size_t offset = 0;
  for (size_t i = 0; i < count; ++i) {
    lua_geti(L, items, (lua_Integer)i + 1);
    int index = lua_gettop(L);
    ailo_item *item = &storage->items[i];
    item->kind = (ailo_item_kind)integer(L, index, "kind");
    item->role = (ailo_role)integer(L, index, "role");
    item->id = string(L, index, "id");
    item->call_id = string(L, index, "call_id");
    item->name = string(L, index, "name");
    item->arguments = string(L, index, "arguments");
    item->encrypted_content = string(L, index, "encrypted_content");
    item->pinned = boolean(L, index, "pinned");
    lua_getfield(L, index, "content");
    int content = lua_gettop(L);
    size_t n = lua_rawlen(L, content);
    item->content = n ? storage->content + offset : NULL;
    item->content_count = n;
    for (size_t j = 0; j < n; ++j) {
      lua_geti(L, content, (lua_Integer)j + 1);
      int part = lua_gettop(L);
      ailo_content *out = &storage->content[offset++];
      out->kind = (ailo_content_kind)integer(L, part, "kind");
      lua_getfield(L, part, "data");
      out->data = luaL_checklstring(L, -1, &out->size);
      lua_pop(L, 2);
    }
    lua_pop(L, 2);
  }
  request->items = storage->items;
  request->item_count = count;
  lua_pop(L, 1);
}
static int continuation_gc(lua_State *L) {
  continuation *value = luaL_checkudata(L, 1, CONTINUATION);
  ailo_continuation_destroy(value->native);
  value->native = NULL;
  return 0;
}
static int continuation_close(lua_State *L) {
  continuation *value = luaL_checkudata(L, 1, CONTINUATION);
  if (value->busy)
    return failure(L, AILO_BUSY, "continuation is active");
  continuation_gc(L);
  lua_pushboolean(L, 1);
  return 1;
}
static continuation *new_continuation_shell(lua_State *L) {
  continuation *value = lua_newuserdatauv(L, sizeof(*value), 0);
  value->native = NULL;
  value->busy = 0;
  luaL_setmetatable(L, CONTINUATION);
  return value;
}
static int new_continuation(lua_State *L) {
  client *value = get_client(L);
  int failed = ready(L, value);
  if (failed)
    return failed;
  luaL_checktype(L, 2, LUA_TTABLE);
  ailo_continuation_config config = ailo_continuation_default_config();
  config.max_items = (size_t)integer(L, 2, "max_items");
  config.max_bytes = (size_t)integer(L, 2, "max_bytes");
  continuation *out = new_continuation_shell(L);
  ailo_error error = {0};
  begin(L, value, 1);
  ailo_status status =
      ailo_continuation_new(value->native, &config, &out->native, &error);
  end(value);
  if (status != AILO_OK) {
    if (value->progress_failed) {
      status = AILO_SINK_FAILED;
      strcpy(error.message, "model progress callback failed");
    }
    failure(L, status, error.message);
    if (value->progress_failed)
      cause(L, 1, 3);
    return 2;
  }
  return 1;
}
static int continuation_clone(lua_State *L) {
  continuation *value = luaL_checkudata(L, 1, CONTINUATION);
  continuation *out = new_continuation_shell(L);
  ailo_error error = {0};
  ailo_status status =
      ailo_continuation_clone(value->native, &out->native, &error);
  if (status != AILO_OK)
    return failure(L, status, error.message);
  return 1;
}
static int continuation_info(lua_State *L) {
  continuation *value = luaL_checkudata(L, 1, CONTINUATION);
  ailo_continuation_info out = {0};
  ailo_error error = {0};
  ailo_status status = ailo_continuation_get_info(value->native, &out, &error);
  if (status != AILO_OK)
    return failure(L, status, error.message);
  lua_createtable(L, 0, 5);
  set_integer(L, "item_count", (lua_Integer)out.item_count);
  set_integer(L, "storage_bytes", (lua_Integer)out.storage_bytes);
  set_integer(L, "pending_calls", (lua_Integer)out.pending_calls);
  push_u64(L, out.revision);
  lua_setfield(L, -2, "revision");
  set_string(L, "identity", out.identity);
  return 1;
}
static int continuation_append(lua_State *L) {
  continuation *value = luaL_checkudata(L, 1, CONTINUATION);
  if (!value->native)
    return failure(L, AILO_INVALID_ARGUMENT, "continuation is closed");
  if (value->busy)
    return failure(L, AILO_BUSY, "continuation is active");
  work *storage = new_work(L);
  lua_pushvalue(L, 2);
  lua_setiuservalue(L, storage->index, 3);
  ailo_request request = ailo_request_default();
  read_items(L, storage, 2, &request);
  ailo_error error = {0};
  ailo_status status = ailo_continuation_append(value->native, request.items,
                                                request.item_count, &error);
  release_work(storage);
  if (status != AILO_OK)
    return failure(L, status, error.message);
  lua_pushboolean(L, 1);
  return 1;
}
static void push_item(lua_State *L, const ailo_item *item) {
  static const char *kinds[] = {"message", "function_call", "function_result",
                                "reasoning", "compaction"};
  static const char *roles[] = {"user", "system", "developer", "assistant",
                                "tool"};
  lua_createtable(L, 0, 8);
  set_string(L, "kind", kinds[item->kind]);
  set_string(L, "role", roles[item->role]);
  set_string(L, "id", item->id);
  set_string(L, "call_id", item->call_id);
  set_string(L, "name", item->name);
  set_string(L, "arguments", item->arguments);
  set_string(L, "encrypted_content", item->encrypted_content);
  lua_pushboolean(L, item->pinned);
  lua_setfield(L, -2, "pinned");
  lua_createtable(L, (int)item->content_count, 0);
  for (size_t i = 0; i < item->content_count; ++i) {
    lua_createtable(L, 0, 2);
    set_string(L, "kind", "text");
    lua_pushlstring(L, item->content[i].data, item->content[i].size);
    lua_setfield(L, -2, "data");
    lua_seti(L, -2, (lua_Integer)i + 1);
  }
  lua_setfield(L, -2, "content");
}
static int item_thunk(lua_State *L) {
  const ailo_item *item = lua_touserdata(L, 2);
  lua_getiuservalue(L, 1, 1);
  push_item(L, item);
  lua_call(L, 1, 1);
  return 1;
}
static int item_visit(void *user, const ailo_item *item) {
  work *storage = user;
  lua_State *L = storage->L;
  if (!lua_checkstack(L, 5)) {
    storage->failed = 1;
    return -1;
  }
  lua_pushcfunction(L, item_thunk);
  lua_pushvalue(L, storage->index);
  lua_pushlightuserdata(L, (void *)item);
  if (lua_pcall(L, 2, 1, 0) != LUA_OK) {
    storage->failed = 1;
    lua_setiuservalue(L, storage->index, 4);
    return -1;
  }
  int result = !(lua_isboolean(L, -1) && !lua_toboolean(L, -1));
  lua_pop(L, 1);
  return result;
}
static int continuation_visit(lua_State *L) {
  continuation *value = luaL_checkudata(L, 1, CONTINUATION);
  if (!value->native)
    return failure(L, AILO_INVALID_ARGUMENT, "continuation is closed");
  if (value->busy)
    return failure(L, AILO_BUSY, "continuation is active");
  luaL_checktype(L, 2, LUA_TFUNCTION);
  work *storage = new_work(L);
  lua_pushvalue(L, 2);
  lua_setiuservalue(L, storage->index, 1);
  lua_pushvalue(L, 1);
  lua_setiuservalue(L, storage->index, 2);
  ailo_error error = {0};
  value->busy = 1;
  ailo_status status =
      ailo_continuation_visit(value->native, item_visit, storage, &error);
  value->busy = 0;
  if (status != AILO_OK) {
    failure(L, status, error.message);
    if (storage->failed)
      cause(L, storage->index, 4);
    return 2;
  }
  lua_pushboolean(L, 1);
  return 1;
}
static void compaction_info(lua_State *L, const ailo_compaction_info *out) {
  lua_createtable(L, 0, 9);
  lua_pushboolean(L, out->changed);
  lua_setfield(L, -2, "changed");
  set_integer(L, "before_tokens", (lua_Integer)out->before_tokens);
  set_integer(L, "after_tokens", (lua_Integer)out->after_tokens);
  set_integer(L, "retained_items", (lua_Integer)out->retained_items);
  set_integer(L, "removed_items", (lua_Integer)out->removed_items);
  set_integer(L, "summary_passes", out->summary_passes);
  set_integer(L, "replayed_tokens", (lua_Integer)out->replayed_tokens);
  set_integer(L, "reused_tokens", (lua_Integer)out->reused_tokens);
  push_u64(L, out->summary_prompt_tokens);
  lua_setfield(L, -2, "summary_prompt_tokens");
  push_u64(L, out->summary_completion_tokens);
  lua_setfield(L, -2, "summary_completion_tokens");
}
static void read_compaction_policy(lua_State *L, int index,
                                   ailo_compaction_policy *policy) {
  policy->target_tokens = (size_t)integer(L, index, "target_tokens");
  policy->recent_turns = (size_t)integer(L, index, "recent_turns");
  policy->output_reserve_tokens =
      (size_t)integer(L, index, "output_reserve_tokens");
  policy->summary_context_tokens =
      (size_t)integer(L, index, "summary_context_tokens");
  policy->summary_tokens = (size_t)integer(L, index, "summary_tokens");
  policy->max_summary_bytes = (size_t)integer(L, index, "max_summary_bytes");
}
static int generate_or_compact(lua_State *L, int compacting) {
  client *value = get_client(L);
  int error_result = ready(L, value);
  if (error_result)
    return error_result;
  luaL_checktype(L, 2, LUA_TTABLE);
  luaL_checktype(L, 3, compacting ? LUA_TTABLE : LUA_TFUNCTION);
  work *storage = new_work(L);
  for (int i = 1; i <= 3; ++i) {
    lua_pushvalue(L, i == 1 ? 3 : i == 2 ? 1 : 2);
    lua_setiuservalue(L, storage->index, i);
  }
  ailo_request request = ailo_request_default();
  ailo_auto_compaction_policy automatic = ailo_auto_compaction_default_policy();
  lua_getfield(L, 2, "auto_compaction");
  if (!lua_isnil(L, -1)) {
    int automatic_index = lua_gettop(L);
    read_compaction_policy(L, automatic_index, &automatic.compaction);
    automatic.trigger_tokens =
        (size_t)integer(L, automatic_index, "trigger_tokens");
    automatic.max_passes =
        (unsigned int)integer(L, automatic_index, "max_passes");
    request.auto_compaction = &automatic;
  }
  lua_pop(L, 1);
  request.max_tokens = (size_t)integer(L, 2, "max_tokens");
  request.candidates = (unsigned int)integer(L, 2, "candidates");
  request.seed = (ailo_u32)integer(L, 2, "seed");
  request.temperature = number(L, 2, "temperature");
  request.top_p = number(L, 2, "top_p");
  request.frequency_penalty = number(L, 2, "frequency_penalty");
  request.presence_penalty = number(L, 2, "presence_penalty");
  request.logprobs = boolean(L, 2, "logprobs");
  request.top_logprobs = (unsigned int)integer(L, 2, "top_logprobs");
  request.max_output_bytes = (size_t)integer(L, 2, "max_output_bytes");
  request.output_format = (ailo_output_format)integer(L, 2, "output_format");
  request.json_schema = string(L, 2, "json_schema");
  request.prompt_cache_key = string(L, 2, "prompt_cache_key");
  request.reasoning = (ailo_reasoning_mode)integer(L, 2, "reasoning");
  request.reasoning_effort = string(L, 2, "reasoning_effort");
  request.max_reasoning_tokens = (size_t)integer(L, 2, "max_reasoning_tokens");
  lua_getfield(L, 2, "continuation");
  continuation *history =
      lua_isnil(L, -1) ? NULL : luaL_checkudata(L, -1, CONTINUATION);
  if (history && !history->native)
    return failure(L, AILO_INVALID_ARGUMENT, "continuation is closed");
  if (history && history->busy)
    return failure(L, AILO_BUSY, "continuation is active");
  request.continuation = history ? history->native : NULL;
  lua_pop(L, 1);
  request.tool_choice = (ailo_tool_choice)integer(L, 2, "tool_choice");
  request.tool_name = string(L, 2, "tool_name");
  request.parallel_tool_calls = boolean(L, 2, "parallel_tool_calls");
  request.max_tool_calls = (size_t)integer(L, 2, "max_tool_calls");
  lua_getfield(L, 2, "tools");
  int tools = lua_gettop(L);
  request.tool_count = lua_rawlen(L, tools);
  if (request.tool_count) {
    storage->tools =
        allocate(L, request.tool_count, sizeof(ailo_function_tool));
    for (size_t i = 0; i < request.tool_count; ++i) {
      lua_geti(L, tools, (lua_Integer)i + 1);
      int tool = lua_gettop(L);
      storage->tools[i].name = string(L, tool, "name");
      storage->tools[i].description = string(L, tool, "description");
      storage->tools[i].parameters = string(L, tool, "parameters");
      storage->tools[i].strict = boolean(L, tool, "strict");
      lua_pop(L, 1);
    }
    request.tools = storage->tools;
  }
  lua_getfield(L, 2, "allowed_tools");
  int allowed = lua_gettop(L);
  request.allowed_tool_count = lua_rawlen(L, allowed);
  if (request.allowed_tool_count) {
    storage->allowed = allocate(L, request.allowed_tool_count, sizeof(char *));
    for (size_t i = 0; i < request.allowed_tool_count; ++i) {
      lua_geti(L, allowed, (lua_Integer)i + 1);
      storage->allowed[i] = luaL_checkstring(L, -1);
      lua_pop(L, 1);
    }
    request.allowed_tools = storage->allowed;
  }
  lua_getfield(L, 2, "logit_bias");
  int biases = lua_gettop(L);
  request.logit_bias_count = lua_rawlen(L, biases);
  if (request.logit_bias_count) {
    storage->biases =
        allocate(L, request.logit_bias_count, sizeof(ailo_logit_bias));
    for (size_t i = 0; i < request.logit_bias_count; ++i) {
      lua_geti(L, biases, (lua_Integer)i + 1);
      int bias = lua_gettop(L);
      storage->biases[i].token_id = (ailo_i32)integer(L, bias, "token_id");
      storage->biases[i].bias = number(L, bias, "bias");
      lua_pop(L, 1);
    }
    request.logit_bias = storage->biases;
  }
  read_items(L, storage, 2, &request);
  lua_getfield(L, 2, "stop");
  int stops = lua_gettop(L);
  size_t count = lua_rawlen(L, stops);
  if (count) {
    storage->strings = allocate(L, count, sizeof(char *));
    for (size_t i = 0; i < count; ++i) {
      lua_geti(L, stops, (lua_Integer)i + 1);
      storage->strings[i] = luaL_checkstring(L, -1);
      lua_pop(L, 1);
    }
    request.stop = storage->strings;
    request.stop_count = count;
  }
  ailo_run_info output = {0};
  ailo_compaction_info compact_info = {0};
  ailo_compaction_policy policy = ailo_compaction_default_policy();
  continuation *window = NULL;
  if (compacting) {
    read_compaction_policy(L, 3, &policy);
    window = new_continuation_shell(L);
  }
  ailo_error error = {0};
  if (history)
    history->busy = 1;
  begin(L, value, 1);
  ailo_status status =
      compacting ? ailo_client_compact(value->native, &request, &policy,
                                       &window->native, &compact_info, &error)
                 : ailo_client_generate(value->native, &request, event, storage,
                                        &output, &error);
  end(value);
  if (history)
    history->busy = 0;
  release_work(storage);
  if (status != AILO_OK) {
    if (value->progress_failed) {
      status = AILO_SINK_FAILED;
      strcpy(error.message, "model progress callback failed");
    }
    failure(L, status, error.message);
    if (compacting)
      compaction_info(L, &compact_info);
    else
      info(L, &output);
    lua_setfield(L, -2, "info");
    if (storage->failed)
      cause(L, storage->index, 4);
    else if (value->progress_failed)
      cause(L, 1, 3);
    return 2;
  }
  if (compacting) {
    compaction_info(L, &compact_info);
    return 2;
  }
  info(L, &output);
  return 1;
}
static int generate(lua_State *L) { return generate_or_compact(L, 0); }
static int compact(lua_State *L) { return generate_or_compact(L, 1); }
static int metadata(lua_State *L, int kind) {
  client *value = get_client(L);
  int error_result = ready(L, value);
  if (error_result)
    return error_result;
  work *storage = new_work(L);
  ailo_error error = {0};
  begin(L, value, 1);
  ailo_status status =
      kind == 0 ? ailo_client_model_card(value->native, &storage->json, &error)
      : kind == 1
          ? ailo_client_capabilities(value->native, &storage->json, &error)
          : ailo_client_devices(value->native, &storage->json, &error);
  end(value);
  if (status != AILO_OK) {
    release_work(storage);
    if (value->progress_failed) {
      status = AILO_SINK_FAILED;
      strcpy(error.message, "model progress callback failed");
    }
    failure(L, status, error.message);
    if (value->progress_failed)
      cause(L, 1, 3);
    return 2;
  }
  lua_pushstring(L, storage->json);
  release_work(storage);
  return 1;
}
static int card(lua_State *L) { return metadata(L, 0); }
static int capabilities(lua_State *L) { return metadata(L, 1); }
static int devices(lua_State *L) { return metadata(L, 2); }
static int client_cache_operation(lua_State *L, int kind) {
  client *value = get_client(L);
  int failed = ready(L, value);
  if (failed)
    return failed;
  ailo_cache_stats output = {0};
  ailo_error error = {0};
  begin(L, value, 1);
  ailo_status status =
      kind == 0 ? ailo_client_cache_stats(value->native, &output, &error)
                : ailo_client_cache_clear(value->native, &error);
  end(value);
  if (status != AILO_OK)
    return failure(L, status, error.message);
  if (kind == 0)
    return cache_statistics(L, &output);
  lua_pushboolean(L, 1);
  return 1;
}
static int client_cache_stats(lua_State *L) {
  return client_cache_operation(L, 0);
}
static int client_cache_clear(lua_State *L) {
  return client_cache_operation(L, 1);
}
static int client_snapshot(lua_State *L, int importing) {
  client *value = get_client(L);
  int failed = ready(L, value);
  if (failed)
    return failed;
  const char *slot = luaL_checkstring(L, 2);
  const char *key = lua_isnoneornil(L, 3) ? NULL : luaL_checkstring(L, 3);
  ailo_error error = {0};
  begin(L, value, 1);
  ailo_status status =
      importing ? ailo_client_snapshot_import(value->native, slot, key, &error)
                : ailo_client_snapshot_export(value->native, slot, &error);
  end(value);
  if (status != AILO_OK) {
    if (value->progress_failed) {
      status = AILO_SINK_FAILED;
      strcpy(error.message, "model progress callback failed");
    }
    failure(L, status, error.message);
    if (value->progress_failed)
      cause(L, 1, 3);
    return 2;
  }
  lua_pushboolean(L, 1);
  return 1;
}
static int snapshot_export(lua_State *L) { return client_snapshot(L, 0); }
static int snapshot_import(lua_State *L) { return client_snapshot(L, 1); }
static int cancel(lua_State *L) {
  client *value = get_client(L);
  if (value->native)
    ailo_client_cancel(value->native);
  lua_pushboolean(L, 1);
  return 1;
}
static int close_client(lua_State *L) {
  client *value = get_client(L);
  if (value->busy)
    return failure(L, AILO_BUSY, "receiver is active");
  client_gc(L);
  for (int i = 1; i <= 3; ++i) {
    lua_pushnil(L);
    lua_setiuservalue(L, 1, i);
  }
  lua_pushboolean(L, 1);
  return 1;
}
static int version(lua_State *L) {
  lua_pushstring(L, AILO_VERSION);
  return 1;
}
#if defined(_WIN32)
#define EXPORT __declspec(dllexport)
#else
#define EXPORT __attribute__((visibility("default")))
#endif
EXPORT int luaopen_ailo_core(lua_State *L) {
  luaL_checkversion(L);
  if (sizeof(lua_Integer) != 8)
    return luaL_error(L, "ailo requires Lua 5.5 with 64-bit integers");
  luaL_newmetatable(L, WORK);
  lua_pushcfunction(L, work_gc);
  lua_setfield(L, -2, "__gc");
  lua_pop(L, 1);
  luaL_newmetatable(L, CACHE);
  static const luaL_Reg cache_methods[] = {{"stats", cache_stats},
                                           {"clear", cache_clear},
                                           {"prune", cache_prune},
                                           {"close", close_cache},
                                           {NULL, NULL}};
  luaL_setfuncs(L, cache_methods, 0);
  lua_pushvalue(L, -1);
  lua_setfield(L, -2, "__index");
  lua_pushcfunction(L, cache_gc);
  lua_setfield(L, -2, "__gc");
  lua_pushcfunction(L, close_cache);
  lua_setfield(L, -2, "__close");
  lua_pushliteral(L, "ailo.cache");
  lua_setfield(L, -2, "__metatable");
  lua_pop(L, 1);
  luaL_newmetatable(L, CONTINUATION);
  static const luaL_Reg continuation_methods[] = {
      {"append", continuation_append}, {"clone", continuation_clone},
      {"info", continuation_info},     {"visit", continuation_visit},
      {"close", continuation_close},   {NULL, NULL}};
  luaL_setfuncs(L, continuation_methods, 0);
  lua_pushvalue(L, -1);
  lua_setfield(L, -2, "__index");
  lua_pushcfunction(L, continuation_gc);
  lua_setfield(L, -2, "__gc");
  lua_pushcfunction(L, continuation_close);
  lua_setfield(L, -2, "__close");
  lua_pushliteral(L, "ailo.continuation");
  lua_setfield(L, -2, "__metatable");
  lua_pop(L, 1);
  luaL_newmetatable(L, CLIENT);
  static const luaL_Reg methods[] = {{"generate", generate},
                                     {"new_continuation", new_continuation},
                                     {"compact", compact},
                                     {"model_card", card},
                                     {"capabilities", capabilities},
                                     {"devices", devices},
                                     {"cache_stats", client_cache_stats},
                                     {"cache_clear", client_cache_clear},
                                     {"snapshot_export", snapshot_export},
                                     {"snapshot_import", snapshot_import},
                                     {"cancel", cancel},
                                     {"close", close_client},
                                     {NULL, NULL}};
  luaL_setfuncs(L, methods, 0);
  lua_pushvalue(L, -1);
  lua_setfield(L, -2, "__index");
  lua_pushcfunction(L, client_gc);
  lua_setfield(L, -2, "__gc");
  lua_pushliteral(L, "ailo.client");
  lua_setfield(L, -2, "__metatable");
  lua_pop(L, 1);
  luaL_newmetatable(L, COMPACTOR);
  static const luaL_Reg compactor_methods[] = {
      {"info", compactor_info}, {"close", compactor_close}, {NULL, NULL}};
  luaL_setfuncs(L, compactor_methods, 0);
  lua_pushvalue(L, -1);
  lua_setfield(L, -2, "__index");
  lua_pushcfunction(L, compactor_gc);
  lua_setfield(L, -2, "__gc");
  lua_pushcfunction(L, compactor_close);
  lua_setfield(L, -2, "__close");
  lua_pushliteral(L, "ailo.compactor");
  lua_setfield(L, -2, "__metatable");
  lua_pop(L, 1);
  static const luaL_Reg module[] = {{"new", new_client},
                                    {"new_cache", new_cache},
                                    {"new_compactor", new_compactor},
                                    {"version", version},
                                    {NULL, NULL}};
  luaL_newlib(L, module);
  return 1;
}
