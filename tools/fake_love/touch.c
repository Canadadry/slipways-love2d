#include "touch.h"

#include <assert.h>
#include <stdbool.h>
#include <stdint.h>

#include <lauxlib.h>

#include "function_list.h"

#define REGISTRY_KEY "fake_love.touch"

typedef struct {
    host_touch_t touches[HOST_TOUCH_MAX];
    size_t touch_count;
    host_mouse_t mouse;
} touch_state_t;

static touch_state_t *upvalue_state(lua_State *lua) {
    return lua_touserdata(lua, lua_upvalueindex(1));
}

static int get_touches(lua_State *lua) {
    if (lua_gettop(lua) != 0)
        return luaL_error(lua, "love.touch.getTouches takes no argument");
    const touch_state_t *state = upvalue_state(lua);
    lua_createtable(lua, (int)state->touch_count, 0);
    for (size_t i = 0; i < state->touch_count; i++) {
        lua_pushlightuserdata(lua, (void *)(intptr_t)state->touches[i].id);
        lua_rawseti(lua, -2, (int)i + 1);
    }
    return 1;
}

static int get_touch_position(lua_State *lua) {
    if (lua_gettop(lua) != 1 || lua_type(lua, 1) != LUA_TLIGHTUSERDATA)
        return luaL_error(lua, "love.touch.getPosition takes one touch id, not a %s",
                          lua_gettop(lua) == 0 ? "no value" : luaL_typename(lua, 1));
    intptr_t id = (intptr_t)lua_touserdata(lua, 1);
    const touch_state_t *state = upvalue_state(lua);
    for (size_t i = 0; i < state->touch_count; i++)
        if (state->touches[i].id == id) {
            lua_pushnumber(lua, state->touches[i].x);
            lua_pushnumber(lua, state->touches[i].y);
            return 2;
        }
    return luaL_error(lua, "Invalid active touch ID");
}

static int get_mouse_position(lua_State *lua) {
    if (lua_gettop(lua) != 0)
        return luaL_error(lua, "love.mouse.getPosition takes no argument");
    const touch_state_t *state = upvalue_state(lua);
    lua_pushnumber(lua, state->mouse.x);
    lua_pushnumber(lua, state->mouse.y);
    return 2;
}

static int is_down(lua_State *lua) {
    if (lua_gettop(lua) != 1 || lua_type(lua, 1) != LUA_TNUMBER || lua_tonumber(lua, 1) != 1)
        return luaL_error(lua, "love.mouse.isDown: only isDown(1), the left button, is in the function list");
    lua_pushboolean(lua, upvalue_state(lua)->mouse.left);
    return 1;
}

static void set_function(lua_State *lua, int state_index, lua_CFunction function, const char *name) {
    lua_pushvalue(lua, state_index);
    lua_pushcclosure(lua, function, 1);
    lua_setfield(lua, -2, name);
}

void touch_install(lua_State *lua) {
    touch_state_t *state = lua_newuserdata(lua, sizeof *state);
    *state = (touch_state_t){0};
    int state_index = lua_gettop(lua);
    lua_pushvalue(lua, state_index);
    lua_setfield(lua, LUA_REGISTRYINDEX, REGISTRY_KEY);

    lua_getglobal(lua, "love");
    lua_pushliteral(lua, "touch");
    lua_createtable(lua, 0, 2);
    set_function(lua, state_index, get_touches, "getTouches");
    set_function(lua, state_index, get_touch_position, "getPosition");
    function_list_protect_module(lua, -1, "love.touch");
    lua_rawset(lua, -3);
    lua_pushliteral(lua, "mouse");
    lua_createtable(lua, 0, 2);
    set_function(lua, state_index, get_mouse_position, "getPosition");
    set_function(lua, state_index, is_down, "isDown");
    function_list_protect_module(lua, -1, "love.mouse");
    lua_rawset(lua, -3);
    lua_pop(lua, 2);
}

void touch_set_frame(lua_State *lua, const host_frame_input_t *input) {
    assert(input->touch_count <= HOST_TOUCH_MAX);
    assert((input->touch_count == 0 || input->touches != NULL) && "a count without touches");
    lua_getfield(lua, LUA_REGISTRYINDEX, REGISTRY_KEY);
    touch_state_t *state = lua_touserdata(lua, -1);
    lua_pop(lua, 1);
    assert(state != NULL && "touch_install runs first");
    for (size_t i = 0; i < input->touch_count; i++)
        state->touches[i] = input->touches[i];
    state->touch_count = input->touch_count;
    state->mouse = input->mouse;
}
