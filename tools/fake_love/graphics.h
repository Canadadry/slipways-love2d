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

void graphics_install(lua_State *lua, canvas_store_t *store, transform_t *transform, const window_size_t *window,
                      screen_t *screen);

#endif
