#include <stdio.h>
#include <string.h>

#include <lauxlib.h>
#include <lua.h>
#include <luajit.h>
#include <lualib.h>

#define EXPECTED_PREFIX "LuaJIT 2.1"

int main(void) {
    lua_State *state = luaL_newstate();
    if (state == NULL) {
        fprintf(stderr, "luajit_version: luaL_newstate failed\n");
        return 1;
    }
    luaL_openlibs(state);
    lua_getglobal(state, "jit");
    lua_getfield(state, -1, "version");
    const char *runtime = lua_tostring(state, -1);
    printf("header  %s\nruntime %s\n", LUAJIT_VERSION, runtime ? runtime : "(none)");
    int ok = runtime != NULL && strcmp(runtime, LUAJIT_VERSION) == 0 &&
             strncmp(runtime, EXPECTED_PREFIX, strlen(EXPECTED_PREFIX)) == 0;
    if (!ok)
        fprintf(stderr, "luajit_version: header and runtime must match and start with \"%s\"\n",
                EXPECTED_PREFIX);
    lua_close(state);
    return ok ? 0 : 1;
}
