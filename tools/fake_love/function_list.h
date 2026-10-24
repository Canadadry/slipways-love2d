#ifndef FAKE_LOVE_FUNCTION_LIST_H
#define FAKE_LOVE_FUNCTION_LIST_H

#include <stddef.h>

#include <lua.h>

/* The callbacks the game may set on `love`, and the modules `love` holds. */
extern const char *const function_list_callbacks[];
extern const size_t function_list_callback_count;
extern const char *const function_list_modules[];
extern const size_t function_list_module_count;

/* Every guard raises "<name> is not in the function list of
 * docs/love-port/design.md" at the game's line. Each sets a metatable, so
 * pairs, next and raw access are unaffected: the host and the installs reach
 * the guarded tables raw. */

/* Guards the global `love`: an absent callback or module name reads nil, any
 * other absent key fails; only a callback name can be created. */
void function_list_protect_love(lua_State *lua);

/* Guards the module table at `index`, named `prefix` ("love.graphics"):
 * reading an absent key or creating one fails. */
void function_list_protect_module(lua_State *lua, int index, const char *prefix);

/* Guards the method table at `index` of the objects of `kind` ("ImageData"):
 * reading an absent method fails, named "<kind>:<method>". */
void function_list_protect_methods(lua_State *lua, int index, const char *kind);

#endif
