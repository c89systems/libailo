#include <lauxlib.h>
#include <lua.h>
#include <lualib.h>
#include <stdio.h>
#include <stdlib.h>
typedef struct allocation_state {
  int fail;
} allocation_state;
static void *allocator(void *data, void *ptr, size_t old_size, size_t size) {
  (void)old_size;
  if (!size) {
    free(ptr);
    return NULL;
  }
  if (((allocation_state *)data)->fail)
    return NULL;
  return realloc(ptr, size);
}
static int fail_allocations(lua_State *L) {
  void *data = NULL;
  lua_getallocf(L, &data);
  ((allocation_state *)data)->fail = lua_toboolean(L, 1);
  return 0;
}
int main(int argc, char **argv) {
  allocation_state state = {0};
  lua_State *L;
  int i, status;
  if (argc < 4)
    return 2;
  L = lua_newstate(allocator, &state, 0);
  if (!L)
    return 2;
  luaL_openlibs(L);
  lua_pushcfunction(L, fail_allocations);
  lua_setglobal(L, "_ailo_fail_allocations");
  lua_getglobal(L, "package");
  lua_pushstring(L, argv[2]);
  lua_setfield(L, -2, "path");
  lua_pushstring(L, argv[3]);
  lua_setfield(L, -2, "cpath");
  lua_pop(L, 1);
  lua_newtable(L);
  for (i = 4; i < argc; ++i) {
    lua_pushstring(L, argv[i]);
    lua_seti(L, -2, i - 3);
  }
  lua_setglobal(L, "arg");
  status = luaL_loadfile(L, argv[1]);
  if (!status)
    status = lua_pcall(L, 0, 0, 0);
  if (status)
    fprintf(stderr, "%s\n", lua_tostring(L, -1));
  lua_close(L);
  return status ? 1 : 0;
}
