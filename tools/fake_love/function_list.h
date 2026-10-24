#ifndef FAKE_LOVE_FUNCTION_LIST_H
#define FAKE_LOVE_FUNCTION_LIST_H

#include <stddef.h>

#include <lua.h>

extern const char *const function_list_callbacks[];
extern const size_t function_list_callback_count;
extern const char *const function_list_modules[];
extern const size_t function_list_module_count;

void function_list_protect_love(lua_State *lua);

void function_list_protect_module(lua_State *lua, int index, const char *prefix);

void function_list_protect_methods(lua_State *lua, int index, const char *kind);

#endif
