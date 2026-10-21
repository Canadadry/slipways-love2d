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

void canvas_set_color(canvas_store_t *store, const unsigned char rgba[4]) {
    assert(rgba[3] == 0xFF && "canvas_set_color: setColor refuses translucent colours, blending is not modelled");
    memcpy(store->color, rgba, sizeof store->color);
}

void canvas_get_color(const canvas_store_t *store, unsigned char rgba[4]) {
    memcpy(rgba, store->color, sizeof store->color);
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

static long long clamp_long(long long value, long long low, long long high) {
    return value < low ? low : value > high ? high : value;
}

void canvas_draw_texels(canvas_store_t *store, const canvas_source_t *source, const canvas_area_t *area, int x, int y) {
    assert(area->column >= 0 && area->width > 0 && area->column + area->width <= source->width &&
           "canvas_draw_texels: columns outside the source or empty");
    assert(area->row >= 0 && area->height > 0 && area->row + area->height <= source->height &&
           "canvas_draw_texels: rows outside the source or empty");
    if (store->target == CANVAS_NONE) {
        return;
    }
    int first_i = (int)clamp_long(-(long long)x, 0, area->width);
    int end_i = (int)clamp_long((long long)CANVAS_WIDTH - x, first_i, area->width);
    int first_j = (int)clamp_long(-(long long)y, 0, area->height);
    int end_j = (int)clamp_long((long long)CANVAS_HEIGHT - y, first_j, area->height);
    if (first_i == end_i || first_j == end_j) {
        return;
    }
    for (int j = first_j; j < end_j; j++) {
        const unsigned char *texel = source->pixels + 4 * ((area->row + j) * source->width + area->column + first_i);
        unsigned char *pixel = store->pixels[store->target] + 4 * ((y + j) * CANVAS_WIDTH + x + first_i);
        for (int i = first_i; i < end_i; i++, texel += 4, pixel += 4) {
            assert((texel[3] == 0x00 || texel[3] == 0xFF) && "canvas_draw_texels: newImage refuses translucent texels, blending is not modelled");
            if (texel[3] == 0x00) {
                continue;
            }
            for (int channel = 0; channel < 3; channel++) {
                pixel[channel] = canvas_tint_component(store->color[channel], texel[channel]);
            }
            pixel[3] = 0xFF;
        }
    }
}

const unsigned char *canvas_read_pixels(const canvas_store_t *store, int canvas) {
    if (canvas < 0 || canvas >= store->count) {
        return NULL;
    }
    return store->pixels[canvas];
}

unsigned char canvas_convert_clear_component(double component) {
    assert(!isnan(component) && "canvas_convert_clear_component: NaN has no byte");
    float clamped = fminf(fmaxf((float)component, 0.0f), 1.0f);
    return (unsigned char)rintf(clamped * 255.0f);
}

unsigned char canvas_convert_draw_component(double component) {
    assert(!isnan(component) && "canvas_convert_draw_component: NaN has no byte");
    float clamped = fminf(fmaxf((float)component, 0.0f), 1.0f);
    float product = clamped * 255.0f;
    return (unsigned char)product;
}

unsigned char canvas_tint_component(unsigned char color, unsigned char texel) {
    return (unsigned char)((2 * color * texel + 255) / 510);
}
