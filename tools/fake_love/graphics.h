#ifndef FAKE_LOVE_GRAPHICS_H
#define FAKE_LOVE_GRAPHICS_H

#include <lua.h>

#include "canvas.h"

/* Sets love.graphics, holding only clear, draw, newCanvas, newImage,
 * newQuad, points, rectangle, setCanvas and setColor, on the global `love`
 * table. The store is not owned and must outlive the Lua state. */
void graphics_install(lua_State *lua, canvas_store_t *store);

#endif
