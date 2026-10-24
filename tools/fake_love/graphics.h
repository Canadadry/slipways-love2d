#ifndef FAKE_LOVE_GRAPHICS_H
#define FAKE_LOVE_GRAPHICS_H

#include <lua.h>

#include "canvas.h"
#include "screen.h"
#include "transform.h"

typedef struct window_size {
    int width;
    int height;
} window_size_t;

/* Sets love.graphics, holding only clear, draw, getDimensions, newCanvas,
 * newImage, newQuad, points, pop, push, rectangle, setCanvas, setColor and
 * translate, on the global `love` table. getDimensions answers `*window`;
 * the present, draw(canvas, x, y, 0, s, s), draws onto `screen`. The store,
 * the transform, the window and the screen are not owned and must outlive
 * the Lua state. */
void graphics_install(lua_State *lua, canvas_store_t *store, transform_t *transform, const window_size_t *window,
                      screen_t *screen);

#endif
