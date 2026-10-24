#include "screen.h"

#include <assert.h>
#include <math.h>
#include <stdlib.h>
#include <string.h>

#include "point.h"

struct screen {
    unsigned char *pixels;
    int width;
    int height;
    bool recorded;
    int texel_columns[SCREEN_MAX_SIDE];
    int texel_rows[SCREEN_MAX_SIDE];
};

#define NO_TEXEL (-1)

screen_t *screen_create(void) {
    return calloc(1, sizeof(screen_t));
}

void screen_free(screen_t *screen) {
    if (screen != NULL) {
        free(screen->pixels);
        free(screen);
    }
}

bool screen_begin_frame(screen_t *screen, int width, int height, bool recorded) {
    assert(screen != NULL);
    assert(width >= 1 && width <= SCREEN_MAX_SIDE && height >= 1 && height <= SCREEN_MAX_SIDE);
    screen->recorded = false;
    if (!recorded) {
        return true;
    }
    size_t count = (size_t)width * (size_t)height;
    if (screen->pixels == NULL || width != screen->width || height != screen->height) {
        unsigned char *pixels = realloc(screen->pixels, 4 * count);
        if (pixels == NULL) {
            return false;
        }
        screen->pixels = pixels;
        screen->width = width;
        screen->height = height;
    }
    static const unsigned char black[4] = {0x00, 0x00, 0x00, 0xFF};
    size_t size = 4 * count;
    memcpy(screen->pixels, black, 4);
    for (size_t done = 4; done < size; done *= 2) {
        memcpy(screen->pixels + done, screen->pixels, done < size - done ? done : size - done);
    }
    screen->recorded = true;
    return true;
}

/* texels[p] is the texel index screen pixel p of `pixel_count` takes along
 * one axis, or NO_TEXEL when its centre is outside start <= c < end. */
static void fill_texel_table(int *texels, int pixel_count, double start, int texel_count, double scale) {
    double end = start + texel_count * scale;
    for (int p = 0; p < pixel_count; p++) {
        double centre = p + 0.5;
        if (!(start <= centre && centre < end)) {
            texels[p] = NO_TEXEL;
            continue;
        }
        double texel = floor((centre - start) / scale);
        texels[p] = texel < 0 ? 0 : texel > texel_count - 1 ? texel_count - 1 : (int)texel;
    }
}

void screen_draw_canvas(screen_t *screen, const canvas_source_t *source, double x, double y, double scale) {
    assert(screen != NULL && source != NULL && source->pixels != NULL);
    assert(source->width >= 1 && source->height >= 1);
    assert(isfinite(x) && fabs(x) < POINT_COORDINATE_LIMIT && isfinite(y) && fabs(y) < POINT_COORDINATE_LIMIT);
    assert(isfinite(scale) && scale > 0 && scale <= SCREEN_MAX_SCALE);
    if (!screen->recorded) {
        return;
    }
    fill_texel_table(screen->texel_columns, screen->width, x, source->width, scale);
    fill_texel_table(screen->texel_rows, screen->height, y, source->height, scale);
    for (int j = 0; j < screen->height; j++) {
        int v = screen->texel_rows[j];
        if (v == NO_TEXEL) {
            continue;
        }
        const unsigned char *texel_row = source->pixels + 4 * (size_t)v * (size_t)source->width;
        unsigned char *pixel_row = screen->pixels + 4 * (size_t)j * (size_t)screen->width;
        for (int i = 0; i < screen->width; i++) {
            int u = screen->texel_columns[i];
            if (u == NO_TEXEL) {
                continue;
            }
            const unsigned char *texel = texel_row + 4 * u;
            assert((texel[3] == 0x00 || texel[3] == 0xFF) && "the present refuses a translucent canvas first");
            if (texel[3] == 0xFF) {
                memcpy(pixel_row + 4 * i, texel, 4);
            }
        }
    }
}

const unsigned char *screen_read_pixels(const screen_t *screen, int *width, int *height) {
    assert(screen != NULL);
    if (!screen->recorded) {
        return NULL;
    }
    *width = screen->width;
    *height = screen->height;
    return screen->pixels;
}
