/* Forty-eight hand-written hex bytes are exactly where a typo hides
 * without breaking anything visible: a slightly wrong blue fails no build
 * and only shows to the eye, on a window nobody here can open. Hence these
 * sixteen assertions, one per color, against the published PICO-8 default
 * palette. */

#include "p8/palette.h"

#include <assert.h>
#include <stdio.h>

static void test_the_shared_palette_is_the_published_pico8_one(void) {
    static const unsigned char expected[P8_PALETTE_COLORS][3] = {
        { 0x00, 0x00, 0x00 }, { 0x1D, 0x2B, 0x53 }, { 0x7E, 0x25, 0x53 }, { 0x00, 0x87, 0x51 },
        { 0xAB, 0x52, 0x36 }, { 0x5F, 0x57, 0x4F }, { 0xC2, 0xC3, 0xC7 }, { 0xFF, 0xF1, 0xE8 },
        { 0xFF, 0x00, 0x4D }, { 0xFF, 0xA3, 0x00 }, { 0xFF, 0xEC, 0x27 }, { 0x00, 0xE4, 0x36 },
        { 0x29, 0xAD, 0xFF }, { 0x83, 0x76, 0x9C }, { 0xFF, 0x77, 0xA8 }, { 0xFF, 0xCC, 0xAA },
    };

    for (int i = 0; i < P8_PALETTE_COLORS; i++) {
        assert(P8_PALETTE_RGB[i].red == expected[i][0]);
        assert(P8_PALETTE_RGB[i].green == expected[i][1]);
        assert(P8_PALETTE_RGB[i].blue == expected[i][2]);
    }
}

int main(void) {
    test_the_shared_palette_is_the_published_pico8_one();

    printf("test_palette: OK\n");
    return 0;
}
