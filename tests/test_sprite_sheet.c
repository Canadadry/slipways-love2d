#include "p8/sprite_sheet.h"
#include "p8/vram.h"

#include <assert.h>
#include <stdio.h>
#include <string.h>

static void test_load_leaves_the_rest_of_the_vram_initial(void) {
    static p8_vram_t loaded;
    static p8_vram_t fresh;
    p8_vram_init(&loaded);
    p8_vram_init(&fresh);

    p8_sprite_sheet_load(&loaded);

    assert(memcmp(loaded.draw_palette, fresh.draw_palette, sizeof(fresh.draw_palette)) == 0);
    assert(memcmp(loaded.map_cells, fresh.map_cells, sizeof(fresh.map_cells)) == 0);
    assert(memcmp(loaded.screen, fresh.screen, sizeof(fresh.screen)) == 0);
}

static int packed_pixel(int x, int y) {
    unsigned char byte = p8_sprite_sheet_packed()[y * 64 + x / 2];
    return (x % 2 == 0) ? (byte & 0x0F) : (byte >> 4);
}

static void test_packed_bytes_hold_the_left_pixel_in_the_low_nibble(void) {
    assert(packed_pixel(16, 0) == 3);
    assert(packed_pixel(2, 64) == 1);
    assert(packed_pixel(0, 127) == 3);
    assert(packed_pixel(22, 0) == 0);
    assert(packed_pixel(23, 0) == 3);
    assert(p8_sprite_sheet_packed()[11] == 0x30);
}

int main(void) {
    test_load_leaves_the_rest_of_the_vram_initial();
    test_packed_bytes_hold_the_left_pixel_in_the_low_nibble();

    printf("test_sprite_sheet: all tests passed\n");
    return 0;
}
