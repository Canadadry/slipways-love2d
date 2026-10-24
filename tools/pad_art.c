#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "fake_love/png_file.h"

#define SHEET_WIDTH 240
#define SHEET_HEIGHT 72
#define FRAME_WIDTH 240
#define FRAME_HEIGHT 84
#define SPRITE_COUNT 18
#define BUTTON_COUNT 8

#define FONT_WIDTH 48
#define FONT_HEIGHT 30
#define GLYPH_WIDTH 3
#define GLYPH_HEIGHT 5

#define ALPHA_CLEAR 0x00
#define ALPHA_FAINT 0x66
#define ALPHA_HALF 0x99
#define ALPHA_OPAQUE 0xFF

static const unsigned char PALETTE[16][3] = {
    {0x00, 0x00, 0x00}, {0x1D, 0x2B, 0x53}, {0x7E, 0x25, 0x53}, {0x00, 0x87, 0x51},
    {0xAB, 0x52, 0x36}, {0x5F, 0x57, 0x4F}, {0xC2, 0xC3, 0xC7}, {0xFF, 0xF1, 0xE8},
    {0xFF, 0x00, 0x4D}, {0xFF, 0xA3, 0x00}, {0xFF, 0xEC, 0x27}, {0x00, 0xE4, 0x36},
    {0x29, 0xAD, 0xFF}, {0x83, 0x76, 0x9C}, {0xFF, 0x77, 0xA8}, {0xFF, 0xCC, 0xAA},
};

typedef struct {
    int x, y, size;
} rectangle_t;

typedef enum { SHAPE_DISC, SHAPE_ROUNDED_SQUARE, SHAPE_PILL } shape_t;

typedef struct {
    shape_t shape;
    int body_colour;
    const char *label;
    int scale;
    int label_x, label_y;
} button_t;

/* Order a, b, x, y, leftshoulder, rightshoulder, start, back: love.pad_sheet's BUTTONS. */
static const button_t BUTTONS[BUTTON_COUNT] = {
    {SHAPE_DISC, 11, "A", 2, 9, 7},
    {SHAPE_DISC, 8, "B", 2, 9, 7},
    {SHAPE_DISC, 12, "X", 2, 9, 7},
    {SHAPE_DISC, 10, "Y", 2, 9, 7},
    {SHAPE_ROUNDED_SQUARE, 13, "L", 2, 9, 7},
    {SHAPE_ROUNDED_SQUARE, 13, "R", 2, 9, 7},
    {SHAPE_PILL, 5, "START", 1, 2, 9},
    {SHAPE_PILL, 5, "BACK", 1, 4, 9},
};

typedef struct {
    unsigned char *pixels;
    int width;
} image_t;

/* Sprite n of the flat order: stick base, stick knob, then each button released then pressed. */
static rectangle_t sprite_rectangle(int n) {
    if (n == 0)
        return (rectangle_t){0, 0, 48};
    if (n == 1)
        return (rectangle_t){0, 48, 24};
    int button = (n - 2) / 2;
    bool pressed = (n - 2) % 2 == 1;
    return (rectangle_t){48 + 24 * button, pressed ? 24 : 0, 24};
}

static void put(image_t *image, int x, int y, int colour, int alpha) {
    unsigned char *pixel = image->pixels + 4 * (y * image->width + x);
    pixel[0] = PALETTE[colour][0];
    pixel[1] = PALETTE[colour][1];
    pixel[2] = PALETTE[colour][2];
    pixel[3] = (unsigned char)alpha;
}

/* On doubled coordinates, so a texel is tested at its centre without fractions. */
static bool in_rounded_rectangle(int x, int y, int left, int top, int right, int bottom, int radius) {
    int cx = 2 * x + 1, cy = 2 * y + 1;
    if (cx < 2 * left || cx > 2 * right || cy < 2 * top || cy > 2 * bottom)
        return false;
    int inner_left = 2 * (left + radius), inner_right = 2 * (right - radius);
    int inner_top = 2 * (top + radius), inner_bottom = 2 * (bottom - radius);
    int qx = cx < inner_left ? inner_left - cx : cx > inner_right ? cx - inner_right : 0;
    int qy = cy < inner_top ? inner_top - cy : cy > inner_bottom ? cy - inner_bottom : 0;
    return qx * qx + qy * qy <= 4 * radius * radius;
}

static bool in_shape(shape_t shape, int size, int x, int y, int inset) {
    switch (shape) {
    case SHAPE_DISC:
        return in_rounded_rectangle(x, y, inset, inset, size - inset, size - inset, size / 2 - inset);
    case SHAPE_ROUNDED_SQUARE:
        return in_rounded_rectangle(x, y, inset, inset, size - inset, size - inset, 6 - inset);
    case SHAPE_PILL:
        return in_rounded_rectangle(x, y, inset, 6 + inset, size - inset, 18 - inset, 6 - inset);
    }
    return false;
}

static void draw_body(image_t *image, rectangle_t sprite, shape_t shape, int body_colour, int body_alpha,
                      int outline_colour) {
    for (int y = 0; y < sprite.size; y++)
        for (int x = 0; x < sprite.size; x++) {
            if (!in_shape(shape, sprite.size, x, y, 0))
                continue;
            if (in_shape(shape, sprite.size, x, y, 1))
                put(image, sprite.x + x, sprite.y + y, body_colour, body_alpha);
            else
                put(image, sprite.x + x, sprite.y + y, outline_colour, ALPHA_OPAQUE);
        }
}

static bool glyph_lit(const unsigned char *font, int code, int x, int y) {
    int column = (code - 32) % 16, row = (code - 32) / 16;
    int font_x = column * GLYPH_WIDTH + x, font_y = row * GLYPH_HEIGHT + y;
    return font[4 * (font_y * FONT_WIDTH + font_x)] != 0;
}

static void draw_label(image_t *image, const unsigned char *font, rectangle_t sprite, const button_t *button,
                       int colour) {
    for (int i = 0; button->label[i] != '\0'; i++)
        for (int y = 0; y < GLYPH_HEIGHT * button->scale; y++)
            for (int x = 0; x < GLYPH_WIDTH * button->scale; x++)
                if (glyph_lit(font, button->label[i], x / button->scale, y / button->scale))
                    put(image, sprite.x + button->label_x + i * (GLYPH_WIDTH + 1) * button->scale + x,
                        sprite.y + button->label_y + y, colour, ALPHA_OPAQUE);
}

static void draw_sheet(image_t *sheet, const unsigned char *font) {
    draw_body(sheet, sprite_rectangle(0), SHAPE_DISC, 1, ALPHA_FAINT, 6);
    draw_body(sheet, sprite_rectangle(1), SHAPE_DISC, 6, ALPHA_OPAQUE, 7);
    for (int k = 0; k < BUTTON_COUNT; k++) {
        const button_t *button = &BUTTONS[k];
        rectangle_t released = sprite_rectangle(2 + 2 * k), pressed = sprite_rectangle(3 + 2 * k);
        draw_body(sheet, released, button->shape, button->body_colour, ALPHA_HALF, button->body_colour);
        draw_label(sheet, font, released, button, 7);
        draw_body(sheet, pressed, button->shape, button->body_colour, ALPHA_OPAQUE, 7);
        draw_label(sheet, font, pressed, button, 0);
    }
}

static void draw_flat(image_t *flat) {
    for (int n = 0; n < SPRITE_COUNT; n++) {
        rectangle_t sprite = sprite_rectangle(n);
        unsigned char colour[4] = {(unsigned char)(0x20 + 0x0B * n), (unsigned char)(0xE0 - 0x0B * n),
                                   n % 2 == 0 ? 0x40 : 0xC0, ALPHA_OPAQUE};
        for (int y = 0; y < sprite.size; y++)
            for (int x = 0; x < sprite.size; x++)
                memcpy(flat->pixels + 4 * ((sprite.y + y) * flat->width + sprite.x + x), colour, 4);
    }
}

/* design.md's screen rule, the same multiply as tools/fake_love/screen.c. */
static int multiply(int p, int q) {
    return (p * q + ((p * q) >> 8) + 128) >> 8;
}

/* The base at (4, 4); the 17 others, flat order without the base, in slots of 7 per row. */
static void draw_frame(image_t *frame, const image_t *sheet) {
    for (int i = 0; i < FRAME_WIDTH * FRAME_HEIGHT; i++)
        memcpy(frame->pixels + 4 * i, (const unsigned char[]){0x00, 0x00, 0x00, 0xFF}, 4);
    for (int n = 0; n < SPRITE_COUNT; n++) {
        rectangle_t sprite = sprite_rectangle(n);
        int k = n - 1;
        int left = n == 0 ? 4 : 56 + 26 * (k % 7), top = n == 0 ? 4 : 4 + 26 * (k / 7);
        for (int y = 0; y < sprite.size; y++)
            for (int x = 0; x < sprite.size; x++) {
                const unsigned char *texel = sheet->pixels + 4 * ((sprite.y + y) * sheet->width + sprite.x + x);
                unsigned char *pixel = frame->pixels + 4 * ((top + y) * frame->width + left + x);
                for (int channel = 0; channel < 3; channel++)
                    pixel[channel] = (unsigned char)multiply(texel[channel], texel[3]);
            }
    }
}

static bool write_or_report(const char *path, const image_t *image, int height) {
    if (png_file_write(path, image->pixels, image->width, height))
        return true;
    fprintf(stderr, "pad_art: %s: cannot write\n", path);
    return false;
}

int main(int argc, char **argv) {
    if (argc != 5) {
        fprintf(stderr, "usage: pad_art <font.png> <pad.png> <pad_flat.png> <expected.png>\n");
        return 1;
    }
    unsigned char *font;
    int font_width, font_height;
    const char *reason = png_file_load(argv[1], &font, &font_width, &font_height);
    if (reason != NULL) {
        fprintf(stderr, "pad_art: %s: %s\n", argv[1], reason);
        return 1;
    }
    if (font_width != FONT_WIDTH || font_height != FONT_HEIGHT) {
        fprintf(stderr, "pad_art: %s: %dx%d, expected %dx%d\n", argv[1], font_width, font_height, FONT_WIDTH,
                FONT_HEIGHT);
        free(font);
        return 1;
    }
    image_t sheet = {calloc(SHEET_WIDTH * SHEET_HEIGHT, 4), SHEET_WIDTH};
    image_t flat = {calloc(SHEET_WIDTH * SHEET_HEIGHT, 4), SHEET_WIDTH};
    image_t frame = {calloc(FRAME_WIDTH * FRAME_HEIGHT, 4), FRAME_WIDTH};
    if (sheet.pixels == NULL || flat.pixels == NULL || frame.pixels == NULL) {
        fprintf(stderr, "pad_art: out of memory\n");
        return 1;
    }
    draw_sheet(&sheet, font);
    draw_flat(&flat);
    draw_frame(&frame, &sheet);
    bool ok = write_or_report(argv[2], &sheet, SHEET_HEIGHT) && write_or_report(argv[3], &flat, SHEET_HEIGHT) &&
              write_or_report(argv[4], &frame, FRAME_HEIGHT);
    free(font);
    free(sheet.pixels);
    free(flat.pixels);
    free(frame.pixels);
    return ok ? 0 : 1;
}
