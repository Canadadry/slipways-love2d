#include "frame_capture.h"

#include <assert.h>
#include <string.h>

#define PALETTE_SIZE 16

static const unsigned char palette[PALETTE_SIZE][3] = {
    {0x00, 0x00, 0x00}, {0x1D, 0x2B, 0x53}, {0x7E, 0x25, 0x53}, {0x00, 0x87, 0x51},
    {0xAB, 0x52, 0x36}, {0x5F, 0x57, 0x4F}, {0xC2, 0xC3, 0xC7}, {0xFF, 0xF1, 0xE8},
    {0xFF, 0x00, 0x4D}, {0xFF, 0xA3, 0x00}, {0xFF, 0xEC, 0x27}, {0x00, 0xE4, 0x36},
    {0x29, 0xAD, 0xFF}, {0x83, 0x76, 0x9C}, {0xFF, 0x77, 0xA8}, {0xFF, 0xCC, 0xAA},
};

int frame_capture_palette_number(const unsigned char rgba[4]) {
    if (rgba[3] != 0xFF) {
        return FRAME_CAPTURE_FOREIGN;
    }
    for (int n = 0; n < PALETTE_SIZE; n++) {
        if (rgba[0] == palette[n][0] && rgba[1] == palette[n][1] && rgba[2] == palette[n][2]) {
            return n;
        }
    }
    return FRAME_CAPTURE_FOREIGN;
}

typedef struct conversion {
    bool cached;
    unsigned char rgba[4];
    int number;
} conversion_t;

static int convert(conversion_t *last, const unsigned char rgba[4]) {
    if (!last->cached || memcmp(last->rgba, rgba, 4) != 0) {
        memcpy(last->rgba, rgba, 4);
        last->number = frame_capture_palette_number(rgba);
        last->cached = true;
    }
    return last->number;
}

static int pack_pixel(conversion_t *last, const unsigned char *pixels, int x, int y, frame_capture_foreign_t *first) {
    const unsigned char *rgba = pixels + 4 * (y * FRAME_CAPTURE_WIDTH + x);
    int number = convert(last, rgba);
    if (number != FRAME_CAPTURE_FOREIGN) {
        return number;
    }
    if (!first->found) {
        *first = (frame_capture_foreign_t){.found = true, .x = x, .y = y};
        memcpy(first->rgba, rgba, 4);
    }
    return FRAME_CAPTURE_FOREIGN;
}

int frame_capture_pack(const unsigned char *pixels, unsigned char frame[FRAME_CAPTURE_FRAME_SIZE], frame_capture_foreign_t *first) {
    assert(frame != NULL && first != NULL);
    memset(frame, 0, FRAME_CAPTURE_FRAME_SIZE);
    if (pixels == NULL) {
        if (!first->found) {
            *first = (frame_capture_foreign_t){.found = true, .no_canvas = true};
        }
        return FRAME_CAPTURE_WIDTH * FRAME_CAPTURE_HEIGHT;
    }
    conversion_t last = {0};
    int foreign = 0;
    unsigned char *byte = frame;
    for (int y = 0; y < FRAME_CAPTURE_HEIGHT; y++) {
        for (int x = 0; x < FRAME_CAPTURE_WIDTH; x += 2) {
            int even = pack_pixel(&last, pixels, x, y, first);
            int odd = pack_pixel(&last, pixels, x + 1, y, first);
            foreign += (even == FRAME_CAPTURE_FOREIGN) + (odd == FRAME_CAPTURE_FOREIGN);
            *byte++ = (unsigned char)((even == FRAME_CAPTURE_FOREIGN ? 0 : even) | (odd == FRAME_CAPTURE_FOREIGN ? 0 : odd) << 4);
        }
    }
    return foreign;
}
