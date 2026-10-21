#ifndef FAKE_LOVE_FRAME_CAPTURE_H
#define FAKE_LOVE_FRAME_CAPTURE_H

#include <stdbool.h>

#define FRAME_CAPTURE_WIDTH 160
#define FRAME_CAPTURE_HEIGHT 120
#define FRAME_CAPTURE_FRAME_SIZE (FRAME_CAPTURE_WIDTH * FRAME_CAPTURE_HEIGHT / 2)
#define FRAME_CAPTURE_FOREIGN (-1)

typedef struct frame_capture_foreign {
    bool found;
    bool no_canvas;
    int x;
    int y;
    unsigned char rgba[4];
} frame_capture_foreign_t;

/* 0 ... 15 when rgba is opaque and its RGB is that palette colour byte for
 * byte; FRAME_CAPTURE_FOREIGN otherwise, alpha 00 included. */
int frame_capture_palette_number(const unsigned char rgba[4]);

/* Packs FRAME_CAPTURE_WIDTH x FRAME_CAPTURE_HEIGHT row-major RGBA8 pixels
 * as the C replay does: per row, per even x, number(x) | number(x + 1) << 4,
 * a foreign pixel written as 0. NULL pixels, no canvas: every pixel is
 * foreign. Returns the count of foreign pixels; fills `first` with the
 * first one only while first->found is false. */
int frame_capture_pack(const unsigned char *pixels, unsigned char frame[FRAME_CAPTURE_FRAME_SIZE], frame_capture_foreign_t *first);

#endif
