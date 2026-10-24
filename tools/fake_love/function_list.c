#include "function_list.h"

#include <stdbool.h>
#include <string.h>

#include <lauxlib.h>

const char *const function_list_callbacks[] = {"conf",           "load",           "update", "draw",
                                               "gamepadpressed", "gamepadreleased", "resize"};
const size_t function_list_callback_count = sizeof function_list_callbacks / sizeof function_list_callbacks[0];
const char *const function_list_modules[] = {"graphics", "image", "joystick", "filesystem", "touch", "mouse"};
const size_t function_list_module_count = sizeof function_list_modules / sizeof function_list_modules[0];

static bool is_one_of(lua_State *lua, int key_index, const char *const *names, size_t count) {
    if (lua_type(lua, key_index) != LUA_TSTRING)
        return false;
    const char *key = lua_tostring(lua, key_index);
    for (size_t i = 0; i < count; i++)
        if (strcmp(key, names[i]) == 0)
            return true;
    return false;
}

/* Upvalue 1 is the prefix, upvalue 2 the separator before a string key. */
static int refuse(lua_State *lua, int key_index) {
    const char *prefix = lua_tostring(lua, lua_upvalueindex(1));
    if (lua_type(lua, key_index) == LUA_TSTRING)
        return luaL_error(lua, "%s%s%s is not in the function list of docs/love-port/design.md", prefix,
                          lua_tostring(lua, lua_upvalueindex(2)), lua_tostring(lua, key_index));
    lua_getglobal(lua, "tostring");
    lua_pushvalue(lua, key_index);
    lua_call(lua, 1, 1);
    return luaL_error(lua, "%s[%s] is not in the function list of docs/love-port/design.md", prefix,
                      lua_tostring(lua, -1));
}

static int refuse_access(lua_State *lua) {
    return refuse(lua, 2);
}

static int read_love(lua_State *lua) {
    if (is_one_of(lua, 2, function_list_callbacks, function_list_callback_count) ||
        is_one_of(lua, 2, function_list_modules, function_list_module_count)) {
        lua_pushnil(lua);
        return 1;
    }
    return refuse(lua, 2);
}

static int create_in_love(lua_State *lua) {
    if (!is_one_of(lua, 2, function_list_callbacks, function_list_callback_count))
        return refuse(lua, 2);
    lua_rawset(lua, 1);
    return 0;
}

static void set_guard(lua_State *lua, const char *field, lua_CFunction guard, const char *prefix,
                      const char *separator) {
    lua_pushstring(lua, prefix);
    lua_pushstring(lua, separator);
    lua_pushcclosure(lua, guard, 2);
    lua_setfield(lua, -2, field);
}

static int absolute_index(lua_State *lua, int index) {
    return index < 0 ? lua_gettop(lua) + index + 1 : index;
}

void function_list_protect_module(lua_State *lua, int index, const char *prefix) {
    index = absolute_index(lua, index);
    lua_createtable(lua, 0, 2);
    set_guard(lua, "__index", refuse_access, prefix, ".");
    set_guard(lua, "__newindex", refuse_access, prefix, ".");
    lua_setmetatable(lua, index);
}

void function_list_protect_love(lua_State *lua) {
    lua_getglobal(lua, "love");
    lua_createtable(lua, 0, 2);
    set_guard(lua, "__index", read_love, "love", ".");
    set_guard(lua, "__newindex", create_in_love, "love", ".");
    lua_setmetatable(lua, -2);
    lua_pop(lua, 1);
}

void function_list_protect_methods(lua_State *lua, int index, const char *kind) {
    index = absolute_index(lua, index);
    lua_createtable(lua, 0, 1);
    set_guard(lua, "__index", refuse_access, kind, ":");
    lua_setmetatable(lua, index);
}
