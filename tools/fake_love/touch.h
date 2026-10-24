#ifndef FAKE_LOVE_TOUCH_H
#define FAKE_LOVE_TOUCH_H

#include <stddef.h>

#include <lua.h>

#include "host.h"

void touch_install(lua_State *lua);

void touch_set_frame(lua_State *lua, const host_frame_input_t *input);

#endif
