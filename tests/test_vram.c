#include "p8/vram.h"

#include <assert.h>
#include <stdio.h>

/* The draw palette comes up in PICO-8's documented power-on state: the
 * identity map, with colour 0 and only colour 0 transparent. */
static void test_init_zeroes_everything_but_the_default_draw_palette(void) {
    p8_vram_t mem;
    for (int c = 0; c < 16; c++) {
        mem.draw_palette[c] = 0xAB;
    }
    mem.sprite_sheet[0][0] = 0xAB;
    mem.sprite_sheet[P8_SPRITE_SHEET_HEIGHT - 1][P8_SPRITE_SHEET_WIDTH - 1] = 0xAB;
    mem.map_cells[0][0] = 0xAB;
    mem.map_cells[P8_MAP_HEIGHT - 1][P8_MAP_WIDTH - 1] = 0xAB;
    mem.screen[0][0] = 0xAB;
    mem.screen[P8_SCREEN_HEIGHT - 1][P8_SCREEN_WIDTH - 1] = 0xAB;
    p8_vram_init(&mem);
    for (int y = 0; y < P8_SPRITE_SHEET_HEIGHT; y++) {
        for (int x = 0; x < P8_SPRITE_SHEET_WIDTH; x++) {
            assert(mem.sprite_sheet[y][x] == 0);
        }
    }
    for (int y = 0; y < P8_MAP_HEIGHT; y++) {
        for (int x = 0; x < P8_MAP_WIDTH; x++) {
            assert(mem.map_cells[y][x] == 0);
        }
    }
    for (int y = 0; y < P8_SCREEN_HEIGHT; y++) {
        for (int x = 0; x < P8_SCREEN_WIDTH; x++) {
            assert(mem.screen[y][x] == 0);
        }
    }

    assert(mem.draw_palette[0] == (0 | P8_DRAW_PALETTE_TRANSPARENT_BIT));
    for (int c = 1; c < 16; c++) {
        assert(mem.draw_palette[c] == (unsigned char)c);
    }
}

int main(void) {
    test_init_zeroes_everything_but_the_default_draw_palette();

    printf("test_vram: all tests passed\n");
    return 0;
}
