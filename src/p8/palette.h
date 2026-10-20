#ifndef P8_PALETTE_H
#define P8_PALETTE_H

#define P8_PALETTE_COLORS 16

typedef struct {
    unsigned char red;
    unsigned char green;
    unsigned char blue;
} p8_rgb_t;

extern const p8_rgb_t P8_PALETTE_RGB[P8_PALETTE_COLORS];

#endif
