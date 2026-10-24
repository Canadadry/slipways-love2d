#include "expected_screen.h"

#include <assert.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "../tools/fake_love/png_file.h"

expected_screen_t expected_screen_black(int width, int height) {
    expected_screen_t screen = {calloc((size_t)width * (size_t)height, 4), width, height};
    assert(screen.pixels != NULL);
    for (int k = 0; k < width * height; k++)
        screen.pixels[4 * k + 3] = 0xFF;
    return screen;
}

void expected_screen_free(expected_screen_t *screen) {
    free(screen->pixels);
    screen->pixels = NULL;
}

static int texel_index(double centre, double start, double scale, int count) {
    int index = (int)floor((centre - start) / scale);
    return index < 0 ? 0 : index > count - 1 ? count - 1 : index;
}

static int multiply(int p, int q) {
    return (p * q + ((p * q) >> 8) + 128) >> 8;
}

static int tint_component(int color, int texel) {
    return (2 * color * texel + 255) / 510;
}

void expected_screen_draw(expected_screen_t *screen, expected_screen_texel_t texel_of, const int quad[4], double x,
                          double y, double scale_x, double scale_y, const unsigned char tint[4]) {
    double x1 = x + quad[2] * scale_x;
    double y1 = y + quad[3] * scale_y;
    for (int j = 0; j < screen->height; j++) {
        double cy = j + 0.5;
        if (!(y <= cy && cy < y1))
            continue;
        int row = quad[1] + texel_index(cy, y, scale_y, quad[3]);
        for (int i = 0; i < screen->width; i++) {
            double cx = i + 0.5;
            if (!(x <= cx && cx < x1))
                continue;
            unsigned char texel[4];
            texel_of(quad[0] + texel_index(cx, x, scale_x, quad[2]), row, texel);
            int alpha = tint_component(tint[3], texel[3]);
            unsigned char *pixel = screen->pixels + 4 * ((size_t)j * (size_t)screen->width + (size_t)i);
            for (int channel = 0; channel < 3; channel++) {
                int sum = multiply(tint_component(tint[channel], texel[channel]), alpha) +
                          multiply(pixel[channel], 255 - alpha);
                pixel[channel] = (unsigned char)(sum > 255 ? 255 : sum);
            }
        }
    }
}

void expected_screen_assert_pixel(const expected_screen_t *screen, int i, int j, unsigned r, unsigned g, unsigned b) {
    const unsigned char *pixel = screen->pixels + 4 * ((size_t)j * (size_t)screen->width + (size_t)i);
    if (pixel[0] != r || pixel[1] != g || pixel[2] != b || pixel[3] != 0xFF) {
        fprintf(stderr, "pixel (%d, %d) is %02X%02X%02X%02X, not %02X%02X%02XFF\n", i, j, pixel[0], pixel[1], pixel[2],
                pixel[3], r, g, b);
        assert(!"the landmark pixel must hold its colour");
    }
}

void expected_screen_assert_file(const char *path, const expected_screen_t *expected) {
    unsigned char *pixels = NULL;
    int width = 0, height = 0;
    const char *reason = png_file_load(path, &pixels, &width, &height);
    if (reason != NULL)
        fprintf(stderr, "%s: %s\n", path, reason);
    assert(reason == NULL && "the listed frame's screen PNG must load");
    assert(width == expected->width && height == expected->height);
    size_t size = (size_t)width * (size_t)height * 4;
    if (memcmp(pixels, expected->pixels, size) != 0) {
        size_t k = 0;
        while (pixels[k] == expected->pixels[k])
            k++;
        fprintf(stderr, "%s: pixel (%zu, %zu) is %02X%02X%02X%02X, not %02X%02X%02X%02X\n", path,
                k / 4 % (size_t)width, k / 4 / (size_t)width, pixels[k / 4 * 4], pixels[k / 4 * 4 + 1],
                pixels[k / 4 * 4 + 2], pixels[k / 4 * 4 + 3], expected->pixels[k / 4 * 4],
                expected->pixels[k / 4 * 4 + 1], expected->pixels[k / 4 * 4 + 2], expected->pixels[k / 4 * 4 + 3]);
        assert(!"the screen must match the expected frame");
    }
    free(pixels);
}
