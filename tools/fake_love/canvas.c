#include "canvas.h"

#include <assert.h>
#include <math.h>
#include <stdlib.h>
#include <string.h>

#define PIXEL_COUNT (CANVAS_WIDTH * CANVAS_HEIGHT)

struct canvas_store {
    unsigned char *pixels[CANVAS_MAX_COUNT];
    int count;
    int target;
    unsigned char color[4];
};

canvas_store_t *canvas_create_store(void) {
    canvas_store_t *store = calloc(1, sizeof(*store));
    assert(store != NULL && "canvas store: out of memory");
    store->target = CANVAS_NONE;
    memset(store->color, 0xFF, sizeof store->color);
    return store;
}

void canvas_free_store(canvas_store_t *store) {
    for (int canvas = 0; canvas < store->count; canvas++) {
        free(store->pixels[canvas]);
    }
    free(store);
}

int canvas_create(canvas_store_t *store) {
    if (store->count == CANVAS_MAX_COUNT) {
        return CANVAS_NONE;
    }
    unsigned char *pixels = calloc(PIXEL_COUNT, 4);
    assert(pixels != NULL && "canvas store: out of memory");
    store->pixels[store->count] = pixels;
    return store->count++;
}

void canvas_select(canvas_store_t *store, int canvas) {
    assert((canvas == CANVAS_NONE || (canvas >= 0 && canvas < store->count)) && "canvas_select: no such canvas");
    store->target = canvas;
}

int canvas_get_target(const canvas_store_t *store) {
    return store->target;
}

void canvas_clear(canvas_store_t *store, const unsigned char rgba[4]) {
    if (store->target == CANVAS_NONE) {
        return;
    }
    unsigned char *pixels = store->pixels[store->target];
    for (int pixel = 0; pixel < PIXEL_COUNT; pixel++) {
        memcpy(pixels + 4 * pixel, rgba, 4);
    }
}

void canvas_plot(canvas_store_t *store, int x, int y) {
    assert(x >= 0 && x < CANVAS_WIDTH && y >= 0 && y < CANVAS_HEIGHT && "canvas_plot: pixel off the canvas");
    assert(store->color[3] == 0xFF && "canvas_plot: a translucent colour needs blending, which step 9 decides");
    if (store->target == CANVAS_NONE) {
        return;
    }
    memcpy(store->pixels[store->target] + 4 * (y * CANVAS_WIDTH + x), store->color, 4);
}

void canvas_fill_range(canvas_store_t *store, int left, int top, int right, int bottom) {
    assert(left >= 0 && left <= right && right < CANVAS_WIDTH && "canvas_fill_range: columns off the canvas or empty");
    assert(top >= 0 && top <= bottom && bottom < CANVAS_HEIGHT && "canvas_fill_range: rows off the canvas or empty");
    assert(store->color[3] == 0xFF && "canvas_fill_range: a translucent colour needs blending, which step 9 decides");
    if (store->target == CANVAS_NONE) {
        return;
    }
    for (int row = top; row <= bottom; row++) {
        unsigned char *pixel = store->pixels[store->target] + 4 * (row * CANVAS_WIDTH + left);
        for (int column = left; column <= right; column++, pixel += 4) {
            memcpy(pixel, store->color, 4);
        }
    }
}

const unsigned char *canvas_read_pixels(const canvas_store_t *store, int canvas) {
    if (canvas < 0 || canvas >= store->count) {
        return NULL;
    }
    return store->pixels[canvas];
}

unsigned char canvas_convert_component(double component) {
    assert(!isnan(component) && "canvas_convert_component: NaN has no byte");
    float clamped = fminf(fmaxf((float)component, 0.0f), 1.0f);
    return (unsigned char)rintf(clamped * 255.0f);
}
