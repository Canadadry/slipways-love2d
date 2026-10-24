#ifndef FAKE_LOVE_TOUCH_H
#define FAKE_LOVE_TOUCH_H

#include <stddef.h>

#include <lua.h>

#include "host.h"

/* Sets love.touch and love.mouse on the global `love` table, with no touch
 * and the mouse at (0, 0), up. */
void touch_install(lua_State *lua);

/* What the four functions answer from now on; the touches are copied. */
void touch_set_frame(lua_State *lua, const host_frame_input_t *input);

#endif
