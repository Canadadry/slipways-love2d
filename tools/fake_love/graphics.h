#ifndef FAKE_LOVE_GRAPHICS_H
#define FAKE_LOVE_GRAPHICS_H

#include <lua.h>

#include "canvas.h"
#include "transform.h"

/* Sets love.graphics, holding only clear, draw, newCanvas, newImage,
 * newQuad, points, pop, push, rectangle, setCanvas, setColor and translate,
 * on the global `love` table. The store and the transform are not owned and
 * must outlive the Lua state. */
void graphics_install(lua_State *lua, canvas_store_t *store, transform_t *transform);

#endif
