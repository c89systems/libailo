/* Private Lua 5.5 bridge. Only installed, public C headers are used. */
#include <ailo/client.h>
#include <ailo/compaction.h>
#include <ailo/continuation.h>
#include <ailo/handler.h>
#include <ailo/openai.h>
#include <ailo/version.h>
#include <lauxlib.h>
#include <limits.h>
#include <lualib.h>
#include <pslog_lua.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#ifndef __STRICT_ANSI__
#error Project Lua binding must use strict C89
#endif
#ifdef __STDC_VERSION__
#error Project Lua binding must use -std=c89
#endif
#if LONG_MAX <= 2147483647L
#error The Lua facade requires the supported LP64 target ABI
#endif
/* Lua's supported C89 long/double declarations have the same 64-bit integer
 * and double calling ABI on our LP64 targets. luaL_checkversion verifies the
 * runtime numeric widths before creating any userdata. */

#define CLIENT "ailo.client"
#define WORK "ailo.work"
#define CACHE "ailo.cache"
#define CONTINUATION "ailo.continuation"
#define COMPACTOR "ailo.compactor"
#define HANDLER "ailo.handler"
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
  int repository_failed;
  int cleanup_failed;
  unsigned handlers;
} client;
typedef struct handler {
  ailo_handler *native;
  client *owner;
  int busy;
} handler;
typedef struct work {
  ailo_item *items;
  ailo_content *content;
  const char **strings;
  ailo_logit_bias *biases;
  ailo_function_tool *tools;
  const char **allowed;
  ailo_compaction_key *keys;
  double *weights;
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
  lua_Integer value;
  lua_getfield(L, index, key);
  value = luaL_checkinteger(L, -1);
  lua_pop(L, 1);
  return value;
}
static double number(lua_State *L, int index, const char *key) {
  double value;
  lua_getfield(L, index, key);
  value = luaL_checknumber(L, -1);
  lua_pop(L, 1);
  return value;
}
static const char *string(lua_State *L, int index, const char *key) {
  const char *value;
  lua_getfield(L, index, key);
  value = lua_isnil(L, -1) ? NULL : luaL_checkstring(L, -1);
  lua_pop(L, 1);
  return value;
}
static int boolean(lua_State *L, int index, const char *key) {
  int value;
  lua_getfield(L, index, key);
  value = lua_toboolean(L, -1);
  lua_pop(L, 1);
  return value;
}
static ailo_u64 unsigned_value(lua_State *L, int index) {
  ailo_u64 result = {0};
  index = lua_absindex(L, index);
  if (lua_istable(L, index)) {
    const char *keys[] = {"hi", "lo"};
    {
      int i = 0;
      for (; i < 2; ++i) {
        int exact;
        lua_Integer n;
        lua_getfield(L, index, keys[i]);
        exact = 0;
        n = lua_tointegerx(L, -1, &exact);
        if (lua_type(L, -1) != LUA_TNUMBER || !exact || n < 0 ||
            (uint64_t)n > UINT32_MAX)
          luaL_error(L,
                     "unsigned 64-bit words must be integers in [0, 2^32-1]");
        if (i == 0)
          result.hi = (ailo_u32)n;
        else
          result.lo = (ailo_u32)n;
        lua_pop(L, 1);
      }
    }
  } else {
    int exact = 0;
    lua_Integer n = lua_tointegerx(L, index, &exact);
    if (lua_type(L, index) != LUA_TNUMBER || !exact || n < 0)
      luaL_error(
          L, "unsigned 64-bit value must be a nonnegative integer or {hi,lo}");
    result.hi = (ailo_u32)((uint64_t)n >> 32);
    result.lo = (ailo_u32)n;
  }
  return result;
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
  cache *value;
  ailo_cache_config config;
  ailo_error error = {0};
  ailo_status status;
  luaL_checktype(L, 1, LUA_TTABLE);
  value = lua_newuserdatauv(L, sizeof(*value), 0);
  value->native = NULL;
  luaL_setmetatable(L, CACHE);
  config = ailo_cache_default_config();
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

  status = ailo_cache_new(&config, &value->native, &error);
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
  ailo_cache_stats output = {0};
  ailo_error error = {0};
  ailo_status status;
  if (!value->native)
    return failure(L, AILO_INVALID_ARGUMENT, "cache is closed");

  status = kind == 0   ? ailo_cache_get_stats(value->native, &output, &error)
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
  free(value->weights);
  ailo_string_free(value->json);
  value->items = NULL;
  value->content = NULL;
  value->strings = NULL;
  value->biases = NULL;
  value->tools = NULL;
  value->allowed = NULL;
  value->keys = NULL;
  value->weights = NULL;
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
  void *value;
  if (size && count > SIZE_MAX / size)
    luaL_error(L, "request exceeds native address space");
  value = calloc(count ? count : 1, size);
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
  compactor *out;
  int result;
  work *storage;
  ailo_compactor_config config;
  int keys;
  ailo_error error = {0};
  ailo_status status;
  luaL_checktype(L, 1, LUA_TTABLE);
  out = lua_newuserdatauv(L, sizeof(*out), 0);
  out->native = NULL;
  luaL_setmetatable(L, COMPACTOR);
  result = lua_gettop(L);
  storage = new_work(L);
  lua_pushvalue(L, 1);
  lua_setiuservalue(L, storage->index, 3);
  config = ailo_compactor_default_config();
  config.namespace_id = string(L, 1, "namespace_id");
  config.current_key = read_compaction_key(L, 1);
  config.ttl_seconds = (ailo_u32)integer(L, 1, "ttl_seconds");
  config.max_item_bytes = (size_t)integer(L, 1, "max_item_bytes");
  lua_getfield(L, 1, "old_keys");
  keys = lua_gettop(L);
  config.old_key_count = lua_rawlen(L, keys);
  if (config.old_key_count) {
    storage->keys =
        allocate(L, config.old_key_count, sizeof(ailo_compaction_key));
    {
      size_t i = 0;
      for (; i < config.old_key_count; ++i) {
        lua_geti(L, keys, (lua_Integer)i + 1);
        storage->keys[i] = read_compaction_key(L, lua_gettop(L));
        lua_pop(L, 1);
      }
    }
    config.old_keys = storage->keys;
  }

  status = ailo_compactor_new(&config, &out->native, &error);
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
  {
    int slot = 4;
    for (; slot <= 10; ++slot) {
      lua_pushnil(L);
      lua_setiuservalue(L, 1, slot);
    }
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
  value->repository_failed = value->cleanup_failed = 0;
  lua_pushnil(L);
  lua_setiuservalue(L, index, 3);
  lua_pushnil(L);
  lua_setiuservalue(L, index, 7);
  lua_pushnil(L);
  lua_setiuservalue(L, index, 8);
}
static void end(client *value) {
  value->active = NULL;
  value->active_index = 0;
  value->busy = 0;
}
static int progress_thunk(lua_State *L) {
  const ailo_u64 *counts;
  lua_getiuservalue(L, 1, 1);
  /* The native callback passes a stack-owned record valid through pcall. */
  counts = lua_touserdata(L, 2);
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
  ailo_u64 counts[3];
  int result;
  if (!L || !lua_checkstack(L, 5)) {
    value->progress_failed = 1;
    return 0;
  }

  counts[0].hi = shards;
  counts[0].lo = shard;
  counts[1] = received;
  counts[2] = total;
  lua_pushcfunction(L, progress_thunk);
  lua_pushvalue(L, value->active_index);
  lua_pushlightuserdata(L, counts);
  if (lua_pcall(L, 2, 1, 0) != LUA_OK) {
    value->progress_failed = 1;
    lua_setiuservalue(L, value->active_index, 3);
    return 0;
  }
  result = !(lua_isboolean(L, -1) && !lua_toboolean(L, -1));
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
/* All repository calls stay inside pcall. Slots 4..10 retain the provider
 * method snapshot, transaction, reader, primary/cleanup causes and active read
 * emitter/work. Native handles are markers for these separately pinned values.
 */
typedef enum repository_op {
  REPOSITORY_RESERVE,
  REPOSITORY_WRITE,
  REPOSITORY_COMMIT,
  REPOSITORY_ABORT,
  REPOSITORY_OPEN,
  REPOSITORY_READ,
  REPOSITORY_CLOSE,
  REPOSITORY_ERASE,
  REPOSITORY_AMEND,
  REPOSITORY_QUERY,
  REPOSITORY_SCAN,
  REPOSITORY_EDIT
} repository_op;
typedef struct read_emitter {
  ailo_output_sink sink;
  void *user;
  int active, rejected, result;
} read_emitter;
typedef struct repository_call {
  repository_op operation;
  const ailo_record_metadata *metadata;
  const ailo_conversation_change *change;
  const ailo_conversation_query *query;
  const ailo_record_query *record_query;
  const ailo_record_edit *edit;
  const char *namespace_id, *id;
  ailo_record_protocol protocol;
  ailo_record_section section;
  const void *bytes;
  size_t size;
  void **handle;
  void **reader;
  ailo_u64 *expires;
  ailo_output_sink sink;
  void *user;
  read_emitter *emitter;
} repository_call;
static void repository_cause(lua_State *L, client *owner, int index,
                             int cleanup) {
  int *failed = cleanup ? &owner->cleanup_failed : &owner->repository_failed;
  if (!*failed) {
    lua_pushvalue(L, index);
    lua_setiuservalue(L, owner->active_index, cleanup ? 8 : 7);
  }
  *failed = 1;
}
static ailo_status repository_error(lua_State *L, client *owner, int index,
                                    int cleanup) {
  index = lua_absindex(L, index);
  repository_cause(L, owner, index, cleanup);
  if (lua_istable(L, index)) {
    lua_getfield(L, index, "code");
    if (lua_type(L, -1) == LUA_TSTRING) {
      const char *code = lua_tostring(L, -1);
      {
        int status = AILO_INVALID_ARGUMENT;
        for (; status <= AILO_NOT_FOUND; ++status)
          if (strcmp(code, ailo_status_name(status)) == 0) {
            lua_pop(L, 1);
            return (ailo_status)status;
          }
      }
    }
    lua_pop(L, 1);
  }
  return AILO_IO_FAILED;
}
static int repository_emit(lua_State *L) {
  read_emitter *emit = lua_touserdata(L, lua_upvalueindex(1));
  size_t size;
  const char *bytes;
  if (!emit->active)
    return luaL_error(L, "repository read emitter is no longer active");
  luaL_checktype(L, 1, LUA_TSTRING);
  size = 0;
  bytes = lua_tolstring(L, 1, &size);
  if (!emit->rejected) {
    /* The provider calls this closure from a nested Lua frame. Re-anchor the
     * sink work and receiver in that frame while delivering bytes. */
    int top = lua_gettop(L);
    client *owner;
    int owner_index;
    work *storage;
    int saved_owner, saved_work;
    lua_getiuservalue(L, lua_upvalueindex(1), 1);
    owner = lua_touserdata(L, -1);
    if (L != owner->active)
      return luaL_error(
          L, "repository read emitter must run on invoking Lua thread");
    owner_index = lua_gettop(L);
    lua_getiuservalue(L, owner_index, 10);
    storage = lua_touserdata(L, -1);
    saved_owner = owner->active_index;
    saved_work = storage->index;
    owner->active_index = owner_index;
    storage->index = lua_gettop(L);
    emit->result = emit->sink(emit->user, bytes, size);
    owner->active_index = saved_owner;
    storage->index = saved_work;
    lua_settop(L, top);
    emit->rejected = emit->result <= 0;
  }
  lua_pushboolean(L, !emit->rejected);
  return 1;
}
static const char *repository_protocol(ailo_record_protocol protocol) {
  static const char *names[] = {"chat", "response", "conversation"};
  return names[protocol];
}
static void repository_metadata(lua_State *L, const ailo_record_metadata *m) {
  lua_createtable(L, 0, 6);
  set_string(L, "namespace_id", m->namespace_id);
  set_string(L, "id", m->id);
  set_string(L, "protocol", repository_protocol(m->protocol));
  push_u64(L, m->created_at);
  lua_setfield(L, -2, "created_at");
  push_u64(L, m->expires_at);
  lua_setfield(L, -2, "expires_at");
  set_integer(L, "max_bytes", (lua_Integer)m->max_bytes);
}
static int repository_thunk(lua_State *L) {
  static const char *names[] = {"reserve", "write", "commit", "abort",
                                "open",    "read",  "close",  "erase",
                                "amend",   "query", "scan",   "edit"};
  static const char *sections[] = {"output",
                                   "input",
                                   "metadata",
                                   "conversation_state",
                                   "conversation_items",
                                   "chat_messages",
                                   "response_input_items"};
  client *owner = lua_touserdata(L, 1);
  repository_call *call = lua_touserdata(L, 2);
  int arguments;
  ailo_status status;
  lua_getiuservalue(L, 1, 4);
  lua_getfield(L, -1, names[call->operation]);
  lua_getfield(L, -2, "receiver");
  arguments = 1;
  switch (call->operation) {
  case REPOSITORY_RESERVE: {
    repository_metadata(L, call->metadata);
    arguments = 2;
    break;
  }
  case REPOSITORY_AMEND: {
    const ailo_conversation_change *c = call->change;
    static const char *actions[] = {"create", "update", "delete", "append",
                                    "delete_item"};
    lua_createtable(L, 0, 9);
    set_string(L, "namespace_id", c->namespace_id);
    set_string(L, "id", c->id);
    set_string(L, "action", actions[c->action]);
    set_string(L, "item_id", c->item_id);
    push_u64(L, c->revision);
    lua_setfield(L, -2, "revision");
    set_integer(L, "max_bytes", (lua_Integer)c->max_bytes);
    if (c->response) {
      repository_metadata(L, c->response);
      lua_setfield(L, -2, "response");
    }
    lua_createtable(L, (int)c->input_count, 0);
    {
      size_t n = 0;
      for (; n < c->input_count; ++n) {
        lua_pushstring(L, c->input_ids[n]);
        lua_seti(L, -2, (lua_Integer)n + 1);
      }
    }
    lua_setfield(L, -2, "input_ids");
    arguments = 2;
    break;
  }
  case REPOSITORY_QUERY: {
    const ailo_conversation_query *q = call->query;
    lua_createtable(L, 0, 7);
    set_string(L, "namespace_id", q->namespace_id);
    set_string(L, "id", q->id);
    set_string(L, "item_id", q->item_id);
    set_string(L, "after", q->after);
    set_integer(L, "limit", (lua_Integer)q->limit);
    lua_pushboolean(L, q->descending);
    lua_setfield(L, -2, "descending");
    lua_pushboolean(L, q->canonical);
    lua_setfield(L, -2, "canonical");
    arguments = 2;
    break;
  }
  case REPOSITORY_SCAN: {
    const ailo_record_query *q = call->record_query;
    lua_createtable(L, 0, 11);
    set_string(L, "namespace_id", q->namespace_id);
    set_string(L, "protocol", repository_protocol(q->protocol));
    set_string(L, "section", sections[q->section]);
    set_string(L, "id", q->id);
    set_string(L, "after", q->after);
    set_string(L, "model", q->model);
    set_integer(L, "limit", (lua_Integer)q->limit);
    lua_pushboolean(L, q->descending);
    lua_setfield(L, -2, "descending");
    push_u64(L, q->now);
    lua_setfield(L, -2, "now");
    lua_createtable(L, 0, (int)q->metadata_count);
    {
      size_t n = 0;
      for (; n < q->metadata_count; ++n) {
        lua_pushlstring(L, q->metadata[n].key, q->metadata[n].key_size);
        lua_pushlstring(L, q->metadata[n].value, q->metadata[n].value_size);
        lua_settable(L, -3);
      }
    }
    lua_setfield(L, -2, "metadata");
    arguments = 2;
    break;
  }
  case REPOSITORY_EDIT: {
    const ailo_record_edit *e = call->edit;
    lua_createtable(L, 0, 5);
    set_string(L, "namespace_id", e->namespace_id);
    set_string(L, "protocol", repository_protocol(e->protocol));
    set_string(L, "id", e->id);
    set_integer(L, "max_bytes", (lua_Integer)e->max_bytes);
    push_u64(L, e->now);
    lua_setfield(L, -2, "now");
    arguments = 2;
    break;
  }
  case REPOSITORY_OPEN:
  case REPOSITORY_ERASE:
    lua_pushstring(L, call->namespace_id);
    lua_pushstring(L, repository_protocol(call->protocol));
    lua_pushstring(L, call->id);
    arguments = 4;
    break;
  case REPOSITORY_WRITE:
    lua_getiuservalue(L, 1, 5);
    lua_pushstring(L, sections[call->section]);
    lua_pushlstring(L, call->bytes, call->size);
    arguments = 4;
    break;
  case REPOSITORY_READ: {
    read_emitter *emit;
    lua_getiuservalue(L, 1, 6);
    lua_pushstring(L, sections[call->section]);
    emit = lua_newuserdatauv(L, sizeof(*emit), 1);
    emit->sink = call->sink;
    emit->user = call->user;
    emit->active = 1;
    emit->rejected = 0;
    emit->result = 1;
    call->emitter = emit;
    lua_pushvalue(L, -1);
    lua_setiuservalue(L, 1, 9);
    lua_pushvalue(L, 1);
    lua_setiuservalue(L, -2, 1);
    lua_pushcclosure(L, repository_emit, 1);
    arguments = 4;
    break;
  }
  default:
    lua_getiuservalue(L, 1, call->operation == REPOSITORY_CLOSE ? 6 : 5);
    arguments = 2;
    break;
  }
  {
    const int expiry = call->operation == REPOSITORY_OPEN ||
                       call->operation == REPOSITORY_SCAN;
    lua_call(L, arguments,
             call->operation == REPOSITORY_EDIT ? 4
             : expiry                           ? 3
                                                : 2);
    status = AILO_OK;
    if (call->operation == REPOSITORY_EDIT) {
      const int tx = lua_gettop(L) - 3, reader = tx + 1;
      if (lua_isnil(L, tx) && !lua_isnil(L, reader) && lua_isnil(L, tx + 2) &&
          lua_isnil(L, tx + 3)) {
        lua_pushinteger(L, repository_error(L, owner, reader, 0));
        return 1;
      }
      if (!lua_isnil(L, tx)) {
        lua_pushvalue(L, tx);
        lua_setiuservalue(L, 1, 5);
        *call->handle = owner;
      }
      if (!lua_isnil(L, reader)) {
        lua_pushvalue(L, reader);
        lua_setiuservalue(L, 1, 6);
        *call->reader = owner;
      }
      if (!lua_isnil(L, tx + 3))
        status = repository_error(L, owner, tx + 3, 0);
      else if (lua_isnil(L, tx) || lua_isnil(L, reader)) {
        if (lua_isnil(L, tx) && !lua_isnil(L, reader))
          status = repository_error(L, owner, reader, 0);
        else
          return luaL_error(L, "repository edit must return "
                               "transaction,reader,expiry or nil,error");
      } else
        *call->expires = unsigned_value(L, tx + 2);
    } else if (call->operation == REPOSITORY_RESERVE ||
               call->operation == REPOSITORY_OPEN ||
               call->operation == REPOSITORY_AMEND ||
               call->operation == REPOSITORY_QUERY ||
               call->operation == REPOSITORY_SCAN) {
      const int opened = call->operation == REPOSITORY_OPEN ||
                         call->operation == REPOSITORY_QUERY ||
                         call->operation == REPOSITORY_SCAN;
      int handle = lua_gettop(L) - (expiry ? 2 : 1);
      int problem;
      if (!lua_isnil(L, handle)) {
        lua_pushvalue(L, handle);
        lua_setiuservalue(L, 1, opened ? 6 : 5);
        *call->handle = owner;
      }
      problem = lua_gettop(L);
      if (expiry && lua_isnil(L, handle) && lua_isnil(L, problem))
        problem = handle + 1;
      if (!lua_isnil(L, problem))
        status = repository_error(L, owner, problem, 0);
      else if (lua_isnil(L, handle))
        return luaL_error(L, "repository must return a handle or nil,error");
      else if (expiry)
        *call->expires = unsigned_value(L, handle + 1);
    } else {
      const int cleanup = call->operation == REPOSITORY_ABORT ||
                          call->operation == REPOSITORY_CLOSE;
      if (!lua_isnil(L, -1))
        status = repository_error(L, owner, -1, cleanup);
      else if (!cleanup && !(lua_isboolean(L, -2) && lua_toboolean(L, -2)))
        return luaL_error(L,
                          "repository callback must return true or nil,error");
      else if (cleanup && lua_isboolean(L, -2) && !lua_toboolean(L, -2))
        return luaL_error(L, "repository cleanup callback must not fail");
      if (call->operation == REPOSITORY_COMMIT && status == AILO_OK) {
        lua_pushnil(L);
        lua_setiuservalue(L, 1, 5);
      }
    }
    lua_pushinteger(L, status);
    return 1;
  }
}
static ailo_status repository_invoke(client *owner, repository_call *call) {
  lua_State *L = owner->active;
  const int cleanup = call->operation == REPOSITORY_ABORT ||
                      call->operation == REPOSITORY_CLOSE;
  int top;
  ailo_status status;
  if (!L)
    return AILO_IO_FAILED;
  if (!cleanup && owner->cleanup_failed)
    return AILO_IO_FAILED;
  top = lua_gettop(L);
  status = AILO_IO_FAILED;
  if (lua_checkstack(L, 16)) {
    lua_pushcfunction(L, repository_thunk);
    lua_pushvalue(L, owner->active_index);
    lua_pushlightuserdata(L, call);
    if (lua_pcall(L, 2, 1, 0) == LUA_OK)
      status = (ailo_status)lua_tointeger(L, -1);
    else if (!call->emitter || !call->emitter->rejected)
      repository_cause(L, owner, -1, cleanup);
  } else if (cleanup)
    owner->cleanup_failed = 1;
  else
    owner->repository_failed = 1;
  if (call->emitter) {
    if (call->emitter->rejected)
      status = call->emitter->result == 0 ? AILO_CANCELLED : AILO_SINK_FAILED;
    call->emitter->active = 0;
    call->emitter->sink = NULL;
    call->emitter->user = NULL;
    lua_getiuservalue(L, owner->active_index, 9);
    lua_pushnil(L);
    lua_setiuservalue(L, -2, 1);
    lua_pop(L, 1);
    lua_pushnil(L);
    lua_setiuservalue(L, owner->active_index, 9);
  }
  if (cleanup) {
    lua_pushnil(L);
    lua_setiuservalue(L, owner->active_index,
                      call->operation == REPOSITORY_CLOSE ? 6 : 5);
  }
  lua_settop(L, top);
  return status;
}
static ailo_status repository_reserve(void *u, const ailo_record_metadata *m,
                                      void **tx, ailo_error *error) {
  repository_call call = {0};
  (void)error;

  call.operation = REPOSITORY_RESERVE;
  call.metadata = m;
  call.handle = tx;
  return repository_invoke(u, &call);
}
static ailo_status repository_write(void *u, void *tx,
                                    ailo_record_section section, const void *p,
                                    size_t n, ailo_error *error) {
  repository_call call = {0};
  (void)tx;
  (void)error;

  call.operation = REPOSITORY_WRITE;
  call.section = section;
  call.bytes = p;
  call.size = n;
  return repository_invoke(u, &call);
}
static ailo_status repository_amend(void *u,
                                    const ailo_conversation_change *change,
                                    void **tx, ailo_error *error) {
  repository_call call = {0};
  (void)error;
  *tx = NULL;

  call.operation = REPOSITORY_AMEND;
  call.change = change;
  call.handle = tx;
  return repository_invoke(u, &call);
}
static ailo_status repository_query(void *u,
                                    const ailo_conversation_query *query,
                                    void **handle, ailo_error *error) {
  repository_call call = {0};
  (void)error;
  *handle = NULL;

  call.operation = REPOSITORY_QUERY;
  call.query = query;
  call.handle = handle;
  return repository_invoke(u, &call);
}
static ailo_status repository_commit(void *u, void *tx, ailo_error *error) {
  repository_call call = {0};
  (void)tx;
  (void)error;

  call.operation = REPOSITORY_COMMIT;
  return repository_invoke(u, &call);
}
static ailo_status repository_scan(void *u, const ailo_record_query *query,
                                   void **reader, ailo_u64 *expiry,
                                   ailo_error *error) {
  repository_call call = {0};
  (void)error;

  call.operation = REPOSITORY_SCAN;
  call.record_query = query;
  call.handle = reader;
  call.expires = expiry;
  return repository_invoke(u, &call);
}
static ailo_status repository_edit(void *u, const ailo_record_edit *edit,
                                   void **tx, void **reader, ailo_u64 *expiry,
                                   ailo_error *error) {
  repository_call call = {0};
  (void)error;

  call.operation = REPOSITORY_EDIT;
  call.edit = edit;
  call.handle = tx;
  call.reader = reader;
  call.expires = expiry;
  return repository_invoke(u, &call);
}
static void repository_abort(void *u, void *tx) {
  repository_call call = {0};
  (void)tx;

  call.operation = REPOSITORY_ABORT;
  (void)repository_invoke(u, &call);
}
static ailo_status repository_open(void *u, const char *ns,
                                   ailo_record_protocol protocol,
                                   const char *id, void **handle,
                                   ailo_u64 *expires, ailo_error *error) {
  repository_call call = {0};
  (void)error;

  call.operation = REPOSITORY_OPEN;
  call.namespace_id = ns;
  call.protocol = protocol;
  call.id = id;
  call.handle = handle;
  call.expires = expires;
  return repository_invoke(u, &call);
}
static ailo_status repository_read(void *u, void *handle,
                                   ailo_record_section section,
                                   ailo_output_sink sink, void *user,
                                   ailo_error *error) {
  repository_call call = {0};
  (void)handle;
  (void)error;

  call.operation = REPOSITORY_READ;
  call.section = section;
  call.sink = sink;
  call.user = user;
  return repository_invoke(u, &call);
}
static void repository_close(void *u, void *handle) {
  repository_call call = {0};
  (void)handle;

  call.operation = REPOSITORY_CLOSE;
  (void)repository_invoke(u, &call);
}
static ailo_status repository_erase(void *u, const char *ns,
                                    ailo_record_protocol protocol,
                                    const char *id, ailo_error *error) {
  repository_call call = {0};
  (void)error;

  call.operation = REPOSITORY_ERASE;
  call.namespace_id = ns;
  call.protocol = protocol;
  call.id = id;
  return repository_invoke(u, &call);
}
/* Resolve only public provider symbols supplied by package.loadlib. */
#define RESOLVE(field, type, out)                                              \
  do {                                                                         \
    lua_CFunction raw;                                                         \
    lua_getfield(L, helpers, field);                                           \
    raw = lua_tocfunction(L, -1);                                              \
    lua_pop(L, 1);                                                             \
    (void)sizeof(char[sizeof(raw) == sizeof(type) ? 1 : -1]);                  \
    if (!raw)                                                                  \
      return failure(L, AILO_UNSUPPORTED,                                      \
                     "missing public pslog interop helper");                   \
    memcpy(&(out), &raw, sizeof(out));                                         \
  } while (0)
static void read_compaction_policy(lua_State *, int, ailo_compaction_policy *);
static int new_client(lua_State *L) {
  check_logger_fn check;
  ref_logger_fn retain;
  unref_logger_fn unref;
  int logger_index;
  client *value;
  int client_index;
  work *storage;
  ailo_client_config config;
  ailo_gpu_config gpu = {0};
  ailo_openai_config api;
  ailo_compaction_policy api_compaction;
  int s;
  int hashes;
  size_t count;
  ailo_error error = {0};
  ailo_status status;
  luaL_checktype(L, 1, LUA_TTABLE);
  check = NULL;
  retain = NULL;
  unref = NULL;
  lua_getfield(L, 1, "logger");
  logger_index = lua_gettop(L);
  if (!lua_isnil(L, logger_index)) {
    int helpers;
    lua_getfield(L, 1, "helpers");
    helpers = lua_gettop(L);
    luaL_checktype(L, helpers, LUA_TTABLE);
    RESOLVE("check_logger", check_logger_fn, check);
    RESOLVE("ref_logger", ref_logger_fn, retain);
    RESOLVE("unref_logger", unref_logger_fn, unref);
    {
      pslog_lua_logger_view view = {sizeof(view), PSLOG_LUA_INTEROP_ABI_VERSION,
                                    NULL, 0};
      if (check(L, logger_index, &view) != 0 ||
          !(view.flags & PSLOG_LUA_LOGGER_NATIVE_SINK))
        return failure(
            L, AILO_INVALID_ARGUMENT,
            "logger must be a live pslog logger with a qualified native sink");
    }
  }
  value = lua_newuserdatauv(L, sizeof(*value), 10);
  memset(value, 0, sizeof(*value));
  client_index = lua_gettop(L);
  luaL_setmetatable(L, CLIENT);
  value->logger.size = sizeof(value->logger);
  value->logger.abi_version = PSLOG_LUA_INTEROP_ABI_VERSION;
  value->logger.registry_ref = LUA_NOREF;
  /* Reference helpers may allocate; finalizer already owns a zeroed reference.
   */
  value->unref = unref;
  if (retain && retain(L, logger_index, NULL, 0, &value->logger) != 0)
    return failure(L, AILO_INVALID_ARGUMENT, "cannot retain pslog logger");
  storage = new_work(L);
  lua_pushvalue(L, 1);
  lua_setiuservalue(L, client_index, 2);
  config = ailo_client_default_config();
  config.model_id = string(L, 1, "model_id");
  config.chat_template = string(L, 1, "chat_template");
  config.model_metadata = string(L, 1, "model_metadata");
  config.backend = (ailo_backend_policy)integer(L, 1, "backend");
  config.lazy = boolean(L, 1, "lazy");
  config.gpu_layers = (int)integer(L, 1, "gpu_layers");
  config.threads = (int)integer(L, 1, "threads");
  config.context_tokens = (ailo_u32)integer(L, 1, "context_tokens");
  config.batch_tokens = (ailo_u32)integer(L, 1, "batch_tokens");

  lua_getfield(L, 1, "gpu");
  if (!lua_isnil(L, -1)) {
    int index = lua_gettop(L);
    gpu.split = (ailo_gpu_split)integer(L, index, "split");
    lua_getfield(L, index, "devices");
    luaL_checktype(L, -1, LUA_TTABLE);
    gpu.device_count = lua_rawlen(L, -1);
    if (gpu.device_count > 16)
      return failure(L, AILO_INVALID_ARGUMENT, "Too many GPU devices");
    storage->allowed = allocate(L, gpu.device_count, sizeof(char *));
    {
      size_t n = 0;
      for (; n < gpu.device_count; ++n) {
        lua_geti(L, -1, (lua_Integer)n + 1);
        storage->allowed[n] = luaL_checkstring(L, -1);
        lua_pop(L, 1);
      }
    }
    gpu.devices = storage->allowed;
    lua_pop(L, 1);
    lua_getfield(L, index, "weights");
    luaL_checktype(L, -1, LUA_TTABLE);
    gpu.weight_count = lua_rawlen(L, -1);
    if (gpu.weight_count > 16)
      return failure(L, AILO_INVALID_ARGUMENT, "Too many GPU weights");
    storage->weights = allocate(L, gpu.weight_count, sizeof(double));
    {
      size_t n = 0;
      for (; n < gpu.weight_count; ++n) {
        lua_geti(L, -1, (lua_Integer)n + 1);
        storage->weights[n] = luaL_checknumber(L, -1);
        lua_pop(L, 1);
      }
    }
    gpu.weights = storage->weights;
    lua_pop(L, 1);
    config.gpu = &gpu;
  }
  lua_pop(L, 1);
  config.disable_prompt_cache = boolean(L, 1, "disable_prompt_cache");
  api = ailo_openai_default_config();
  api_compaction = ailo_compaction_default_policy();
  {
    ailo_record_repository provider = {NULL,
                                       repository_reserve,
                                       repository_write,
                                       repository_commit,
                                       repository_abort,
                                       repository_open,
                                       repository_read,
                                       repository_close,
                                       repository_erase,
                                       NULL,
                                       NULL,
                                       NULL,
                                       NULL};
    provider.user_data = value;
    lua_getfield(L, 1, "openai");
    if (!lua_isnil(L, -1)) {
      int api_index = lua_gettop(L);
      api.namespace_id = string(L, api_index, "namespace_id");
      api.state_directory = string(L, api_index, "state_directory");
      api.encryption_key_file = string(L, api_index, "encryption_key_file");
      api.zero_data_retention = boolean(L, api_index, "zero_data_retention");
      api.chat_store_opt_in = boolean(L, api_index, "chat_store_opt_in");
      api.max_output_memory_bytes =
          (size_t)integer(L, api_index, "max_output_memory_bytes");
      api.max_record_bytes = (size_t)integer(L, api_index, "max_record_bytes");
      lua_getfield(L, api_index, "compaction");
      if (!lua_isnil(L, -1)) {
        read_compaction_policy(L, lua_gettop(L), &api_compaction);
        api.compaction = &api_compaction;
      }
      lua_pop(L, 1);
      lua_getfield(L, api_index, "response_retention_seconds");
      api.response_retention_seconds = unsigned_value(L, -1);
      lua_pop(L, 1);
      lua_getfield(L, api_index, "chat_retention_seconds");
      api.chat_retention_seconds = unsigned_value(L, -1);
      lua_pop(L, 1);
      lua_getfield(L, api_index, "repository");
      if (!lua_isnil(L, -1)) {
        lua_getfield(L, -1, "amend");
        if (!lua_isnil(L, -1)) {
          provider.amend = repository_amend;
          provider.query = repository_query;
        }
        lua_pop(L, 1);
        lua_getfield(L, -1, "scan");
        if (!lua_isnil(L, -1))
          provider.scan = repository_scan;
        lua_pop(L, 1);
        lua_getfield(L, -1, "edit");
        if (!lua_isnil(L, -1))
          provider.edit = repository_edit;
        lua_pop(L, 1);
        lua_setiuservalue(L, client_index, 4);
        api.repository = &provider;
      } else
        lua_pop(L, 1);
      config.openai = &api;
    }
    lua_pop(L, 1);
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
    s = lua_gettop(L);
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
    config.source.connect_timeout_ms =
        (long)integer(L, s, "connect_timeout_ms");
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
    hashes = lua_gettop(L);
    count = lua_rawlen(L, hashes);
    if (count) {
      storage->strings = allocate(L, count, sizeof(char *));
      {
        size_t i = 0;
        for (; i < count; ++i) {
          lua_geti(L, hashes, (lua_Integer)i + 1);
          storage->strings[i] =
              lua_isboolean(L, -1) ? NULL : luaL_checkstring(L, -1);
          lua_pop(L, 1);
        }
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

    begin(L, value, client_index);
    status = ailo_client_new_with_logger(&config, value->logger.borrowed,
                                         &value->native, &error);
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
    /* C copied the config. Progress lives in uservalue1, logger in provider
     * ref.
     */
    lua_pushnil(L);
    lua_setiuservalue(L, client_index, 2);
    lua_pushvalue(L, client_index);
    return 1;
  }
}
static void compaction_info(lua_State *, const ailo_compaction_info *);
static void push_item(lua_State *, const ailo_item *);
static int event_thunk(lua_State *L) {
  const ailo_event *value = lua_touserdata(L, 2);
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
                                "compaction",
                                "output_progress"};
  lua_getiuservalue(L, 1, 1);

  lua_createtable(L, 0, 14);
  set_string(L, "kind", names[value->kind]);
  push_u64(L, value->sequence);
  lua_setfield(L, -2, "sequence");
  set_integer(L, "candidate", value->candidate);
  set_string(L, "model_id", value->model_id);
  set_string(L, "run_id", value->run_id);
  set_string(L, "call_id", value->call_id);
  set_string(L, "name", value->name);
  if (value->raw_offset_valid) {
    push_u64(L, value->raw_offset);
    lua_setfield(L, -2, "raw_offset");
  }
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
    {
      size_t i = 0;
      for (; i < value->compacted_item_count; ++i) {
        push_item(L, &value->compacted_items[i]);
        lua_seti(L, -2, (lua_Integer)i + 1);
      }
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
    {
      size_t i = 0;
      for (; i < token->top_count; ++i) {
        const ailo_token_probability *p = &token->top[i];
        lua_createtable(L, 0, 3);
        set_integer(L, "token_id", p->token_id);
        lua_pushlstring(L, p->data, p->size);
        lua_setfield(L, -2, "data");
        lua_pushnumber(L, p->logprob);
        lua_setfield(L, -2, "logprob");
        lua_seti(L, -2, (lua_Integer)i + 1);
      }
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
  int result;
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
  result = !(lua_isboolean(L, -1) && !lua_toboolean(L, -1));
  lua_pop(L, 1);
  return result;
}
static void read_items(lua_State *L, work *storage, int request_index,
                       ailo_request *request) {
  int items;
  size_t count, parts;
  size_t offset;
  lua_getfield(L, request_index, "items");
  items = lua_gettop(L);
  count = lua_rawlen(L, items);
  parts = 0;
  storage->items = allocate(L, count, sizeof(ailo_item));
  {
    size_t i = 0;
    for (; i < count; ++i) {
      size_t n;
      lua_geti(L, items, (lua_Integer)i + 1);
      lua_getfield(L, -1, "content");
      n = lua_rawlen(L, -1);
      if (n > SIZE_MAX - parts)
        luaL_error(L, "too many content parts");
      parts += n;
      lua_pop(L, 2);
    }
  }
  storage->content = allocate(L, parts, sizeof(ailo_content));
  offset = 0;
  {
    size_t i = 0;
    for (; i < count; ++i) {
      int index;
      ailo_item *item;
      int content;
      size_t n;
      lua_geti(L, items, (lua_Integer)i + 1);
      index = lua_gettop(L);
      item = &storage->items[i];
      item->kind = (ailo_item_kind)integer(L, index, "kind");
      item->role = (ailo_role)integer(L, index, "role");
      item->id = string(L, index, "id");
      item->call_id = string(L, index, "call_id");
      item->name = string(L, index, "name");
      item->arguments = string(L, index, "arguments");
      item->encrypted_content = string(L, index, "encrypted_content");
      item->pinned = boolean(L, index, "pinned");
      lua_getfield(L, index, "content");
      content = lua_gettop(L);
      n = lua_rawlen(L, content);
      item->content = n ? storage->content + offset : NULL;
      item->content_count = n;
      {
        size_t j = 0;
        for (; j < n; ++j) {
          int part;
          ailo_content *out;
          lua_geti(L, content, (lua_Integer)j + 1);
          part = lua_gettop(L);
          out = &storage->content[offset++];
          out->kind = (ailo_content_kind)integer(L, part, "kind");
          lua_getfield(L, part, "data");
          out->data = luaL_checklstring(L, -1, &out->size);
          lua_pop(L, 2);
        }
      }
      lua_pop(L, 2);
    }
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
  ailo_continuation_config config;
  continuation *out;
  ailo_error error = {0};
  ailo_status status;
  if (failed)
    return failed;
  luaL_checktype(L, 2, LUA_TTABLE);
  config = ailo_continuation_default_config();
  config.max_items = (size_t)integer(L, 2, "max_items");
  config.max_bytes = (size_t)integer(L, 2, "max_bytes");
  out = new_continuation_shell(L);

  begin(L, value, 1);
  status = ailo_continuation_new(value->native, &config, &out->native, &error);
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
  work *storage;
  ailo_request request;
  ailo_error error = {0};
  ailo_status status;
  if (!value->native)
    return failure(L, AILO_INVALID_ARGUMENT, "continuation is closed");
  if (value->busy)
    return failure(L, AILO_BUSY, "continuation is active");
  storage = new_work(L);
  lua_pushvalue(L, 2);
  lua_setiuservalue(L, storage->index, 3);
  request = ailo_request_default();
  read_items(L, storage, 2, &request);

  status = ailo_continuation_append(value->native, request.items,
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
  {
    size_t i = 0;
    for (; i < item->content_count; ++i) {
      lua_createtable(L, 0, 2);
      set_string(L, "kind", "text");
      lua_pushlstring(L, item->content[i].data, item->content[i].size);
      lua_setfield(L, -2, "data");
      lua_seti(L, -2, (lua_Integer)i + 1);
    }
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
  int result;
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
  result = !(lua_isboolean(L, -1) && !lua_toboolean(L, -1));
  lua_pop(L, 1);
  return result;
}
static int continuation_visit(lua_State *L) {
  continuation *value = luaL_checkudata(L, 1, CONTINUATION);
  work *storage;
  ailo_error error = {0};
  ailo_status status;
  if (!value->native)
    return failure(L, AILO_INVALID_ARGUMENT, "continuation is closed");
  if (value->busy)
    return failure(L, AILO_BUSY, "continuation is active");
  luaL_checktype(L, 2, LUA_TFUNCTION);
  storage = new_work(L);
  lua_pushvalue(L, 2);
  lua_setiuservalue(L, storage->index, 1);
  lua_pushvalue(L, 1);
  lua_setiuservalue(L, storage->index, 2);

  value->busy = 1;
  status = ailo_continuation_visit(value->native, item_visit, storage, &error);
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
  work *storage;
  ailo_request request;
  ailo_auto_compaction_policy automatic;
  continuation *history;
  int tools;
  int allowed;
  int biases;
  int stops;
  size_t count;
  ailo_run_info output = {0};
  ailo_compaction_info compact_info = {0};
  ailo_compaction_policy policy;
  continuation *window;
  ailo_error error = {0};
  ailo_status status;
  if (error_result)
    return error_result;
  luaL_checktype(L, 2, LUA_TTABLE);
  luaL_checktype(L, 3, compacting ? LUA_TTABLE : LUA_TFUNCTION);
  storage = new_work(L);
  {
    int i = 1;
    for (; i <= 3; ++i) {
      lua_pushvalue(L, i == 1 ? 3 : i == 2 ? 1 : 2);
      lua_setiuservalue(L, storage->index, i);
    }
  }
  request = ailo_request_default();
  automatic = ailo_auto_compaction_default_policy();
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
  history = lua_isnil(L, -1) ? NULL : luaL_checkudata(L, -1, CONTINUATION);
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
  tools = lua_gettop(L);
  request.tool_count = lua_rawlen(L, tools);
  if (request.tool_count) {
    storage->tools =
        allocate(L, request.tool_count, sizeof(ailo_function_tool));
    {
      size_t i = 0;
      for (; i < request.tool_count; ++i) {
        int tool;
        lua_geti(L, tools, (lua_Integer)i + 1);
        tool = lua_gettop(L);
        storage->tools[i].name = string(L, tool, "name");
        storage->tools[i].description = string(L, tool, "description");
        storage->tools[i].parameters = string(L, tool, "parameters");
        storage->tools[i].strict = boolean(L, tool, "strict");
        lua_pop(L, 1);
      }
    }
    request.tools = storage->tools;
  }
  lua_getfield(L, 2, "allowed_tools");
  allowed = lua_gettop(L);
  request.allowed_tool_count = lua_rawlen(L, allowed);
  if (request.allowed_tool_count) {
    storage->allowed = allocate(L, request.allowed_tool_count, sizeof(char *));
    {
      size_t i = 0;
      for (; i < request.allowed_tool_count; ++i) {
        lua_geti(L, allowed, (lua_Integer)i + 1);
        storage->allowed[i] = luaL_checkstring(L, -1);
        lua_pop(L, 1);
      }
    }
    request.allowed_tools = storage->allowed;
  }
  lua_getfield(L, 2, "logit_bias");
  biases = lua_gettop(L);
  request.logit_bias_count = lua_rawlen(L, biases);
  if (request.logit_bias_count) {
    storage->biases =
        allocate(L, request.logit_bias_count, sizeof(ailo_logit_bias));
    {
      size_t i = 0;
      for (; i < request.logit_bias_count; ++i) {
        int bias;
        lua_geti(L, biases, (lua_Integer)i + 1);
        bias = lua_gettop(L);
        storage->biases[i].token_id = (ailo_i32)integer(L, bias, "token_id");
        storage->biases[i].bias = number(L, bias, "bias");
        lua_pop(L, 1);
      }
    }
    request.logit_bias = storage->biases;
  }
  read_items(L, storage, 2, &request);
  lua_getfield(L, 2, "stop");
  stops = lua_gettop(L);
  count = lua_rawlen(L, stops);
  if (count) {
    storage->strings = allocate(L, count, sizeof(char *));
    {
      size_t i = 0;
      for (; i < count; ++i) {
        lua_geti(L, stops, (lua_Integer)i + 1);
        storage->strings[i] = luaL_checkstring(L, -1);
        lua_pop(L, 1);
      }
    }
    request.stop = storage->strings;
    request.stop_count = count;
  }

  policy = ailo_compaction_default_policy();
  window = NULL;
  if (compacting) {
    read_compaction_policy(L, 3, &policy);
    window = new_continuation_shell(L);
  }

  if (history)
    history->busy = 1;
  begin(L, value, 1);
  status = compacting
               ? ailo_client_compact(value->native, &request, &policy,
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
typedef struct api_bytes {
  const void *data;
  size_t size;
} api_bytes;
static int bytes_thunk(lua_State *L) {
  const api_bytes *bytes = lua_touserdata(L, 2);
  lua_getiuservalue(L, 1, 1);
  lua_pushlstring(L, bytes->data, bytes->size);
  lua_call(L, 1, 1);
  return 1;
}
static int api_bytes_sink(void *user, const void *data, size_t size) {
  work *storage = user;
  lua_State *L = storage->L;
  api_bytes bytes;
  int accepted;
  bytes.data = data;
  bytes.size = size;
  if (!lua_checkstack(L, 5)) {
    storage->failed = 1;
    return -1;
  }
  lua_pushcfunction(L, bytes_thunk);
  lua_pushvalue(L, storage->index);
  lua_pushlightuserdata(L, &bytes);
  if (lua_pcall(L, 2, 1, 0) != LUA_OK) {
    storage->failed = 1;
    lua_setiuservalue(L, storage->index, 4);
    return -1;
  }
  accepted = !(lua_isboolean(L, -1) && !lua_toboolean(L, -1));
  lua_pop(L, 1);
  return accepted;
}
static void api_info(lua_State *L, const ailo_api_info *out) {
  info(L, &out->run);
  set_string(L, "id", out->id[0] ? out->id : NULL);
  set_integer(L, "http_status", out->http_status);
  set_string(L, "content_type", out->content_type);
  set_string(L, "allow", out->allow);
  lua_pushboolean(L, out->started);
  lua_setfield(L, -2, "started");
  lua_pushboolean(L, out->stored);
  lua_setfield(L, -2, "stored");
}
typedef struct http_callback {
  work *storage;
  int kind;
  const ailo_http_response *headers;
  const void *bytes;
  size_t size;
} http_callback;
static int http_thunk(lua_State *L) {
  static const char *names[] = {"begin", "write", "finish", "abort"};
  const http_callback *call = lua_touserdata(L, 2);
  const ailo_http_response *headers = call->headers;
  int arguments = 1;
  lua_getiuservalue(L, 1, 1);
  lua_getfield(L, -1, names[call->kind]);
  lua_pushvalue(L, -2);
  if (call->kind == 0) {
    lua_createtable(L, 0, 6);
    set_integer(L, "status", headers->status);
    set_string(L, "content_type", headers->content_type);
    set_string(L, "allow", headers->allow);
    set_string(L, "cache_control", headers->cache_control);
    set_string(L, "request_id", headers->request_id);
    set_string(L, "retry_after", headers->retry_after);
    ++arguments;
  } else if (call->kind == 1) {
    lua_pushlstring(L, call->bytes, call->size);
    ++arguments;
  }
  lua_call(L, arguments, 1);
  return 1;
}
static int http_invoke(http_callback *call) {
  work *storage = call->storage;
  lua_State *L = storage->L;
  int accepted;
  if (!lua_checkstack(L, 5)) {
    storage->failed = 1;
    return -1;
  }
  lua_pushcfunction(L, http_thunk);
  lua_pushvalue(L, storage->index);
  lua_pushlightuserdata(L, call);
  if (lua_pcall(L, 2, 1, 0) != LUA_OK) {
    if (!storage->failed)
      lua_setiuservalue(L, storage->index, 4);
    else
      lua_pop(L, 1);
    storage->failed = 1;
    return -1;
  }
  accepted = !(lua_isboolean(L, -1) && !lua_toboolean(L, -1));
  lua_pop(L, 1);
  return accepted;
}
static int http_begin(void *user, const ailo_http_response *headers) {
  http_callback call;
  memset(&call, 0, sizeof(call));
  call.storage = user;
  call.headers = headers;
  return http_invoke(&call);
}
static int http_write(void *user, const void *bytes, size_t size) {
  http_callback call;
  memset(&call, 0, sizeof(call));
  call.storage = user;
  call.kind = 1;
  call.bytes = bytes;
  call.size = size;
  return http_invoke(&call);
}
static int http_finish(void *user) {
  http_callback call;
  memset(&call, 0, sizeof(call));
  call.storage = user;
  call.kind = 2;
  return http_invoke(&call);
}
static void http_abort(void *user) {
  http_callback call;
  memset(&call, 0, sizeof(call));
  call.storage = user;
  call.kind = 3;
  http_invoke(&call);
}
static int handler_gc(lua_State *L) {
  handler *value = luaL_checkudata(L, 1, HANDLER);
  if (value->native) {
    ailo_handler_destroy(value->native);
    value->native = NULL;
    --value->owner->handlers;
  }
  return 0;
}
static int handler_close(lua_State *L) {
  handler *value = luaL_checkudata(L, 1, HANDLER);
  if (value->busy)
    return failure(L, AILO_BUSY, "handler is active");
  handler_gc(L);
  lua_pushnil(L);
  lua_setiuservalue(L, 1, 1);
  lua_pushboolean(L, 1);
  return 1;
}
static int new_handler(lua_State *L) {
  client *owner = get_client(L);
  handler *value;
  ailo_handler_config config = ailo_handler_default_config();
  ailo_error error = {0};
  ailo_status status;
  int checked = ready(L, owner);
  if (checked)
    return checked;
  luaL_checktype(L, 2, LUA_TTABLE);
  config.client = owner->native;
  config.max_body_bytes = (size_t)integer(L, 2, "max_body_bytes");
  config.max_target_bytes = (size_t)integer(L, 2, "max_target_bytes");
  value = lua_newuserdatauv(L, sizeof(*value), 1);
  memset(value, 0, sizeof(*value));
  luaL_setmetatable(L, HANDLER);
  value->owner = owner;
  lua_pushvalue(L, 1);
  lua_setiuservalue(L, -2, 1);
  status = ailo_handler_new(&config, &value->native, &error);
  if (status != AILO_OK)
    return failure(L, status, error.message);
  ++owner->handlers;
  return 1;
}
static void handler_info(lua_State *L, const ailo_handler_info *out) {
  api_info(L, &out->api);
  set_integer(L, "http_status", out->http_status);
  set_string(L, "request_id", out->request_id);
  push_u64(L, out->bytes_written);
  lua_setfield(L, -2, "bytes_written");
  lua_pushboolean(L, out->started);
  lua_setfield(L, -2, "started");
  lua_pushboolean(L, out->completed);
  lua_setfield(L, -2, "completed");
  lua_pushboolean(L, out->aborted);
  lua_setfield(L, -2, "aborted");
}
static int handler_handle(lua_State *L) {
  static const char *names[] = {"begin", "write", "finish", "abort"};
  handler *value = luaL_checkudata(L, 1, HANDLER);
  ailo_http_request request;
  ailo_http_transport transport;
  ailo_handler_info out;
  ailo_error error = {0};
  ailo_status status;
  work *storage;
  int i, owner_index;
  if (!value->native)
    return failure(L, AILO_INVALID_ARGUMENT, "handler is closed");
  if (value->busy || value->owner->busy)
    return failure(L, AILO_BUSY, "handler or receiver is active");
  luaL_checktype(L, 2, LUA_TTABLE);
  luaL_checktype(L, 3, LUA_TTABLE);
  for (i = 0; i < 4; ++i) {
    lua_getfield(L, 3, names[i]);
    luaL_checktype(L, -1, LUA_TFUNCTION);
    lua_pop(L, 1);
  }
  memset(&request, 0, sizeof(request));
  request.method = string(L, 2, "method");
  request.target = string(L, 2, "target");
  request.content_type = string(L, 2, "content_type");
  request.content_encoding = string(L, 2, "content_encoding");
  lua_getfield(L, 2, "body");
  request.body = luaL_optlstring(L, -1, "", &request.body_size);
  storage = new_work(L);
  lua_pushvalue(L, 3);
  lua_setiuservalue(L, storage->index, 1);
  lua_getiuservalue(L, 1, 1);
  owner_index = lua_gettop(L);
  begin(L, value->owner, owner_index);
  value->busy = 1;
  transport.user_data = storage;
  transport.begin = http_begin;
  transport.write = http_write;
  transport.finish = http_finish;
  transport.abort = http_abort;
  status =
      ailo_handler_handle(value->native, &request, &transport, &out, &error);
  value->busy = 0;
  end(value->owner);
  release_work(storage);
  if (status == AILO_OK && value->owner->cleanup_failed) {
    status = AILO_IO_FAILED;
    strcpy(error.message, "Repository cleanup callback failed");
  }
  if (storage->failed || value->owner->progress_failed) {
    status = AILO_SINK_FAILED;
    strcpy(error.message, "HTTP transport or model progress callback failed");
  }
  if (status != AILO_OK) {
    failure(L, status, error.message);
    handler_info(L, &out);
    lua_setfield(L, -2, "info");
    if (storage->failed)
      cause(L, storage->index, 4);
    else if (value->owner->progress_failed)
      cause(L, owner_index, 3);
    else if (value->owner->repository_failed)
      cause(L, owner_index, 7);
    if (value->owner->cleanup_failed) {
      lua_getiuservalue(L, owner_index, 8);
      lua_setfield(L, -2, "cleanup_cause");
      lua_pushnil(L);
      lua_setiuservalue(L, owner_index, 8);
    }
    lua_pushnil(L);
    lua_setiuservalue(L, owner_index, 7);
    return 2;
  }
  handler_info(L, &out);
  return 1;
}
static int api_operation(lua_State *L, int operation) {
  client *value = get_client(L);
  int checked = ready(L, value);
  size_t size;
  const char *json;
  int sink_index;
  const char *id, *item;
  const char *method, *target;
  work *storage;
  ailo_api_info out = {0};
  ailo_error error = {0};
  ailo_status status;
  if (checked)
    return checked;
  size = 0;
  json = luaL_checklstring(L, 2, &size);
  if (operation != 0 && operation != 3 && operation != 6 && operation != 7 &&
      operation != 15 && memchr(json, 0, size))
    return failure(L, AILO_INVALID_ARGUMENT, "completion ID contains NUL");
  sink_index = operation == 1 ? 4 : 3;
  id = json;
  item = NULL;
  method = NULL;
  target = NULL;
  if (operation == 19) {
    size_t target_size = 0;
    method = json;
    target = luaL_checklstring(L, 3, &target_size);
    if (memchr(target, 0, target_size))
      return failure(L, AILO_INVALID_ARGUMENT, "API target contains NUL");
    json = luaL_optlstring(L, 4, "", &size);
    sink_index = 5;
  } else if (operation == 9 || operation == 11 || operation == 14 ||
             operation >= 16) {
    json = luaL_checklstring(L, 3, &size);
    sink_index = 4;
  } else if (operation == 12 || operation == 13) {
    size_t item_size = 0;
    item = luaL_checklstring(L, 3, &item_size);
    if (memchr(item, 0, item_size))
      return failure(L, AILO_INVALID_ARGUMENT, "item ID contains NUL");
    if (operation == 12) {
      json = luaL_checklstring(L, 4, &size);
      sink_index = 5;
    } else
      sink_index = 4;
  }
  if (operation == 1)
    luaL_checktype(L, 3, LUA_TBOOLEAN);
  luaL_checktype(L, sink_index, LUA_TFUNCTION);
  storage = new_work(L);
  {
    int i = 1;
    for (; i <= 3; ++i) {
      lua_pushvalue(L, i == 1 ? sink_index : i == 2 ? 1 : 2);
      lua_setiuservalue(L, storage->index, i);
    }
  }

  lua_pushvalue(L, storage->index);
  lua_setiuservalue(L, 1, 10);
  begin(L, value, 1);
  status =
      operation == 0
          ? ailo_client_chat_completions(value->native, json, size,
                                         api_bytes_sink, storage, &out, &error)
      : operation == 1
          ? ailo_client_chat_retrieve(value->native, json, lua_toboolean(L, 3),
                                      api_bytes_sink, storage, &out, &error)
      : operation == 2
          ? ailo_client_chat_delete(value->native, json, api_bytes_sink,
                                    storage, &out, &error)
      : operation == 3
          ? ailo_client_responses(value->native, json, size, api_bytes_sink,
                                  storage, &out, &error)
      : operation == 4
          ? ailo_client_response_retrieve(value->native, json, api_bytes_sink,
                                          storage, &out, &error)
      : operation == 5
          ? ailo_client_response_delete(value->native, json, api_bytes_sink,
                                        storage, &out, &error)
      : operation == 6
          ? ailo_client_response_compact(value->native, json, size,
                                         api_bytes_sink, storage, &out, &error)
      : operation == 7 ? ailo_client_conversation_create(value->native, json,
                                                         size, api_bytes_sink,
                                                         storage, &out, &error)
      : operation == 8
          ? ailo_client_conversation_retrieve(value->native, id, api_bytes_sink,
                                              storage, &out, &error)
      : operation == 9
          ? ailo_client_conversation_update(value->native, id, json, size,
                                            api_bytes_sink, storage,
                                            &out, &error)
      : operation == 10
          ? ailo_client_conversation_delete(value->native, id, api_bytes_sink,
                                            storage, &out, &error)
      : operation == 11
          ? ailo_client_conversation_items_create(value->native, id, json, size,
                                                  api_bytes_sink, storage, &out,
                                                  &error)
      : operation == 12
          ? ailo_client_conversation_item_retrieve(value->native, id, item,
                                                   json, size, api_bytes_sink,
                                                   storage, &out, &error)
      : operation == 13
          ? ailo_client_conversation_item_delete(
                value->native, id, item, api_bytes_sink, storage, &out, &error)
      : operation == 14
          ? ailo_client_conversation_items_list(value->native, id, json, size,
                                                api_bytes_sink, storage, &out,
                                                &error)
      : operation == 15
          ? ailo_client_chat_list(value->native, json, size, api_bytes_sink,
                                  storage, &out, &error)
      : operation == 16
          ? ailo_client_chat_update(value->native, id, json, size,
                                    api_bytes_sink, storage, &out, &error)
      : operation == 17
          ? ailo_client_chat_messages_list(value->native, id, json, size,
                                           api_bytes_sink, storage, &out,
                                           &error)
      : operation == 18
          ? ailo_client_response_input_items_list(value->native, id, json, size,
                                                  api_bytes_sink, storage, &out,
                                                  &error)
          : ailo_client_handle(value->native, method, target, json, size,
                               api_bytes_sink, storage, &out, &error);
  end(value);
  lua_pushnil(L);
  lua_setiuservalue(L, 1, 10);
  release_work(storage);
  if (status == AILO_OK && value->cleanup_failed) {
    status = AILO_IO_FAILED;
    strcpy(error.message, "repository cleanup callback failed");
  }
  if (status != AILO_OK) {
    if (value->progress_failed) {
      status = AILO_SINK_FAILED;
      strcpy(error.message, "model progress callback failed");
    }
    failure(L, status, error.message);
    api_info(L, &out);
    lua_setfield(L, -2, "info");
    if (storage->failed)
      cause(L, storage->index, 4);
    else if (value->progress_failed)
      cause(L, 1, 3);
    else if (value->repository_failed)
      cause(L, 1, 7);
    if (value->cleanup_failed) {
      lua_getiuservalue(L, 1, 8);
      lua_setfield(L, -2, "cleanup_cause");
      lua_pushnil(L);
      lua_setiuservalue(L, 1, 8);
    }
    lua_pushnil(L);
    lua_setiuservalue(L, 1, 7);
    return 2;
  }
  api_info(L, &out);
  return 1;
}
static int chat_completions(lua_State *L) { return api_operation(L, 0); }
static int chat_retrieve(lua_State *L) { return api_operation(L, 1); }
static int chat_delete(lua_State *L) { return api_operation(L, 2); }
static int responses(lua_State *L) { return api_operation(L, 3); }
static int response_retrieve(lua_State *L) { return api_operation(L, 4); }
static int response_delete(lua_State *L) { return api_operation(L, 5); }
static int response_compact(lua_State *L) { return api_operation(L, 6); }
static int conversation_create(lua_State *L) { return api_operation(L, 7); }
static int conversation_retrieve(lua_State *L) { return api_operation(L, 8); }
static int conversation_update(lua_State *L) { return api_operation(L, 9); }
static int conversation_delete(lua_State *L) { return api_operation(L, 10); }
static int conversation_items_create(lua_State *L) {
  return api_operation(L, 11);
}
static int conversation_item_retrieve(lua_State *L) {
  return api_operation(L, 12);
}
static int conversation_item_delete(lua_State *L) {
  return api_operation(L, 13);
}
static int conversation_items_list(lua_State *L) {
  return api_operation(L, 14);
}
static int chat_list(lua_State *L) { return api_operation(L, 15); }
static int chat_update(lua_State *L) { return api_operation(L, 16); }
static int chat_messages_list(lua_State *L) { return api_operation(L, 17); }
static int response_input_items_list(lua_State *L) {
  return api_operation(L, 18);
}
static int handle(lua_State *L) { return api_operation(L, 19); }
static int metadata(lua_State *L, int kind) {
  client *value = get_client(L);
  int error_result = ready(L, value);
  work *storage;
  ailo_error error = {0};
  ailo_status status;
  if (error_result)
    return error_result;
  storage = new_work(L);

  begin(L, value, 1);
  status = kind == 0
               ? ailo_client_model_card(value->native, &storage->json, &error)
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
  ailo_cache_stats output = {0};
  ailo_error error = {0};
  ailo_status status;
  if (failed)
    return failed;

  begin(L, value, 1);
  status = kind == 0 ? ailo_client_cache_stats(value->native, &output, &error)
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
  const char *slot;
  const char *key;
  ailo_error error = {0};
  ailo_status status;
  if (failed)
    return failed;
  slot = luaL_checkstring(L, 2);
  key = lua_isnoneornil(L, 3) ? NULL : luaL_checkstring(L, 3);

  begin(L, value, 1);
  status = importing
               ? ailo_client_snapshot_import(value->native, slot, key, &error)
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
  if (value->busy || value->handlers)
    return failure(L, AILO_BUSY, "receiver is active");
  client_gc(L);
  {
    int i = 1;
    for (; i <= 3; ++i) {
      lua_pushnil(L);
      lua_setiuservalue(L, 1, i);
    }
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
  static const luaL_Reg cache_methods[] = {{"stats", cache_stats},
                                           {"clear", cache_clear},
                                           {"prune", cache_prune},
                                           {"close", close_cache},
                                           {NULL, NULL}};
  static const luaL_Reg continuation_methods[] = {
      {"append", continuation_append}, {"clone", continuation_clone},
      {"info", continuation_info},     {"visit", continuation_visit},
      {"close", continuation_close},   {NULL, NULL}};
  static const luaL_Reg methods[] = {
      {"generate", generate},
      {"chat_completions", chat_completions},
      {"chat_retrieve", chat_retrieve},
      {"chat_delete", chat_delete},
      {"responses", responses},
      {"response_retrieve", response_retrieve},
      {"response_delete", response_delete},
      {"response_compact", response_compact},
      {"conversation_create", conversation_create},
      {"conversation_retrieve", conversation_retrieve},
      {"conversation_update", conversation_update},
      {"conversation_delete", conversation_delete},
      {"conversation_items_create", conversation_items_create},
      {"conversation_item_retrieve", conversation_item_retrieve},
      {"conversation_item_delete", conversation_item_delete},
      {"conversation_items_list", conversation_items_list},
      {"chat_list", chat_list},
      {"chat_update", chat_update},
      {"chat_messages_list", chat_messages_list},
      {"response_input_items_list", response_input_items_list},
      {"handle", handle},
      {"new_handler", new_handler},
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
  static const luaL_Reg compactor_methods[] = {
      {"info", compactor_info}, {"close", compactor_close}, {NULL, NULL}};
  static const luaL_Reg module[] = {{"new", new_client},
                                    {"new_cache", new_cache},
                                    {"new_compactor", new_compactor},
                                    {"version", version},
                                    {NULL, NULL}};
  luaL_checkversion(L);
  if (sizeof(lua_Integer) != 8)
    return luaL_error(L, "ailo requires Lua 5.5 with 64-bit integers");
  luaL_newmetatable(L, WORK);
  lua_pushcfunction(L, work_gc);
  lua_setfield(L, -2, "__gc");
  lua_pop(L, 1);
  luaL_newmetatable(L, HANDLER);
  {
    static const luaL_Reg handler_methods[] = {
        {"handle", handler_handle}, {"close", handler_close}, {NULL, NULL}};
    luaL_setfuncs(L, handler_methods, 0);
  }
  lua_pushvalue(L, -1);
  lua_setfield(L, -2, "__index");
  lua_pushcfunction(L, handler_gc);
  lua_setfield(L, -2, "__gc");
  lua_pushcfunction(L, handler_close);
  lua_setfield(L, -2, "__close");
  lua_pushliteral(L, "ailo.handler");
  lua_setfield(L, -2, "__metatable");
  lua_pop(L, 1);
  luaL_newmetatable(L, CACHE);

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

  luaL_setfuncs(L, methods, 0);
  lua_pushvalue(L, -1);
  lua_setfield(L, -2, "__index");
  lua_pushcfunction(L, client_gc);
  lua_setfield(L, -2, "__gc");
  lua_pushliteral(L, "ailo.client");
  lua_setfield(L, -2, "__metatable");
  lua_pop(L, 1);
  luaL_newmetatable(L, COMPACTOR);

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

  luaL_newlib(L, module);
  return 1;
}
