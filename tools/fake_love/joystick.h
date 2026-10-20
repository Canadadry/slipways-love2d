#ifndef FAKE_LOVE_JOYSTICK_H
#define FAKE_LOVE_JOYSTICK_H

#include <lua.h>

/* Sets love.joystick, holding only getJoysticks, on the global `love` table,
 * and creates the one joystick, with nothing held. */
void joystick_install(lua_State *lua);

/* What isGamepadDown answers from now on: bit n-1 for button n. */
void joystick_set_held(lua_State *lua, unsigned held);

/* Pushes the joystick that getJoysticks returns. */
void joystick_push(lua_State *lua);

#endif
