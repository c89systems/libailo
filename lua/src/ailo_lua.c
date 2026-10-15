/* Private Lua 5.5 bridge. Only installed, public C headers are used. */
#include <ailo/client.h>
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
  lua_createtable(L, 0, 3);
  push_u64(L, value->prompt_tokens);
  lua_setfield(L, -2, "prompt_tokens");
  push_u64(L, value->completion_tokens);
  lua_setfield(L, -2, "completion_tokens");
  push_u64(L, value->cached_tokens);
  lua_setfield(L, -2, "cached_tokens");
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
static void release_work(work *value) {
  free(value->items);
  free(value->content);
  free(value->strings);
  free(value->biases);
  ailo_string_free(value->json);
  value->items = NULL;
  value->content = NULL;
  value->strings = NULL;
  value->biases = NULL;
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
static int event_thunk(lua_State *L) {
  const ailo_event *value = lua_touserdata(L, 2);
  lua_getiuservalue(L, 1, 1);
  static const char *names[] = {"run_begin",  "choice_begin", "text_delta",
                                "choice_end", "run_end",      "token"};
  lua_createtable(L, 0, 8);
  set_string(L, "kind", names[value->kind]);
  push_u64(L, value->sequence);
  lua_setfield(L, -2, "sequence");
  set_integer(L, "candidate", value->candidate);
  set_string(L, "model_id", value->model_id);
  lua_pushlstring(L, value->data ? value->data : "", value->size);
  lua_setfield(L, -2, "data");
  set_string(L, "finish_reason", finish(value->finish_reason));
  usage(L, &value->usage);
  lua_setfield(L, -2, "usage");
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
static int generate(lua_State *L) {
  client *value = get_client(L);
  int error_result = ready(L, value);
  if (error_result)
    return error_result;
  luaL_checktype(L, 2, LUA_TTABLE);
  luaL_checktype(L, 3, LUA_TFUNCTION);
  work *storage = new_work(L);
  for (int i = 1; i <= 3; ++i) {
    lua_pushvalue(L, i == 1 ? 3 : i == 2 ? 1 : 2);
    lua_setiuservalue(L, storage->index, i);
  }
  ailo_request request = ailo_request_default();
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
  lua_getfield(L, 2, "items");
  int items = lua_gettop(L);
  size_t count = lua_rawlen(L, items), parts = 0;
  storage->items = allocate(L, count, sizeof(ailo_item));
  for (size_t i = 0; i < count; ++i) {
    lua_geti(L, items, (lua_Integer)i + 1);
    lua_getfield(L, -1, "content");
    size_t n = lua_rawlen(L, -1);
    if (n > SIZE_MAX - parts)
      return failure(L, AILO_OUT_OF_RANGE, "too many content parts");
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
  request.items = storage->items;
  request.item_count = count;
  lua_getfield(L, 2, "stop");
  int stops = lua_gettop(L);
  count = lua_rawlen(L, stops);
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
  ailo_error error = {0};
  begin(L, value, 1);
  ailo_status status = ailo_client_generate(value->native, &request, event,
                                            storage, &output, &error);
  end(value);
  release_work(storage);
  if (status != AILO_OK) {
    if (value->progress_failed) {
      status = AILO_SINK_FAILED;
      strcpy(error.message, "model progress callback failed");
    }
    failure(L, status, error.message);
    info(L, &output);
    lua_setfield(L, -2, "info");
    if (storage->failed)
      cause(L, storage->index, 4);
    else if (value->progress_failed)
      cause(L, 1, 3);
    return 2;
  }
  info(L, &output);
  return 1;
}
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
  luaL_newmetatable(L, CLIENT);
  static const luaL_Reg methods[] = {{"generate", generate},
                                     {"model_card", card},
                                     {"capabilities", capabilities},
                                     {"devices", devices},
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
  static const luaL_Reg module[] = {
      {"new", new_client}, {"version", version}, {NULL, NULL}};
  luaL_newlib(L, module);
  return 1;
}
