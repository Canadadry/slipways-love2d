#ifndef FAKE_LOVE_IMAGE_H
#define FAKE_LOVE_IMAGE_H

#include <lua.h>

#include "image_data.h"

/* Sets love.image, holding only newImageData, on the global `love` table. */
void image_install(lua_State *lua);

/* The ImageData at stack `index`, owned by its userdata and valid while that
 * stays reachable from Lua; NULL when the value is anything else. */
const image_data_t *image_find_data(lua_State *lua, int index);

#endif
