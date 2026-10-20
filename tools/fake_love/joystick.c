#include "joystick.h"

#include <assert.h>
#include <stdbool.h>

#include <lauxlib.h>

#include "pad.h"

#define METATABLE "Joystick"
#define REGISTRY_KEY "fake_love.joystick"

typedef struct {
    unsigned held;
} joystick_t;

static joystick_t *installed(lua_State *lua) {
    lua_getfield(lua, LUA_REGISTRYINDEX, REGISTRY_KEY);
    joystick_t *joystick = luaL_testudata(lua, -1, METATABLE);
    lua_pop(lua, 1);
    assert(joystick != NULL && "joystick_install runs first");
    return joystick;
}

static int is_gamepad_down(lua_State *lua) {
    joystick_t *joystick = luaL_testudata(lua, 1, METATABLE);
    if (joystick == NULL)
        return luaL_error(lua, "Joystick:isGamepadDown needs the joystick: call it with a colon");
    int top = lua_gettop(lua);
    if (top < 2)
        return luaL_error(lua, "Joystick:isGamepadDown needs at least one button name");
    bool down = false;
    for (int index = 2; index <= top; index++) {
        if (lua_type(lua, index) != LUA_TSTRING)
            return luaL_error(lua, "Joystick:isGamepadDown: argument %d is a %s, not a button name", index - 1,
                              luaL_typename(lua, index));
        const char *name = lua_tostring(lua, index);
        int button;
        if (!pad_find_name(name, &button))
            return luaL_error(lua, "Invalid gamepad button: %s", name);
        if (button != 0 && (joystick->held & (1u << (button - 1))))
            down = true;
    }
    lua_pushboolean(lua, down);
    return 1;
}

static int get_joysticks(lua_State *lua) {
    lua_createtable(lua, 1, 0);
    joystick_push(lua);
    lua_rawseti(lua, -2, 1);
    return 1;
}

void joystick_install(lua_State *lua) {
    joystick_t *joystick = lua_newuserdata(lua, sizeof *joystick);
    joystick->held = 0;
    luaL_newmetatable(lua, METATABLE);
    lua_createtable(lua, 0, 1);
    lua_pushcfunction(lua, is_gamepad_down);
    lua_setfield(lua, -2, "isGamepadDown");
    lua_setfield(lua, -2, "__index");
    lua_setmetatable(lua, -2);
    lua_setfield(lua, LUA_REGISTRYINDEX, REGISTRY_KEY);

    lua_getglobal(lua, "love");
    lua_createtable(lua, 0, 1);
    lua_pushcfunction(lua, get_joysticks);
    lua_setfield(lua, -2, "getJoysticks");
    lua_setfield(lua, -2, "joystick");
    lua_pop(lua, 1);
}

void joystick_set_held(lua_State *lua, unsigned held) {
    assert(held >> PAD_BUTTON_COUNT == 0 && "a held set has 12 bits, one per button");
    installed(lua)->held = held;
}

void joystick_push(lua_State *lua) {
    lua_getfield(lua, LUA_REGISTRYINDEX, REGISTRY_KEY);
}
