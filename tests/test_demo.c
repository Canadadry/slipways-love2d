
#include "p8/vram.h"
#include "p8/sprite_sheet.h"
#include "p8/gfx.h"
#include "p8/input.h"
#include "devtools/dump.h"

#include <assert.h>
#include <stdio.h>

#define DEMO_EVENTS_PATH "tests/fixtures/demo_events.json"

static unsigned char screen_pixel(const p8_vram_t *mem, int x, int y) {
    return mem->screen[y][x];
}

int main(void) {
    p8_vram_t memory;
    p8_vram_init(&memory);

    p8_graphics_t graphics;
    p8_gfx_init(&graphics);

    p8_sprite_sheet_load(&memory);

    assert(p8_sget(&memory, 16, 0) == 3);

    p8_cls(&memory);
    assert(screen_pixel(&memory, 0, 0) == 0);
    assert(screen_pixel(&memory, P8_SCREEN_WIDTH - 1, P8_SCREEN_HEIGHT - 1) == 0);

    p8_line(&graphics, &memory, 0, 5, 4, 5, 8);
    for (int x = 0; x <= 4; x++) {
        assert(screen_pixel(&memory, x, 5) == 8);
    }
    assert(screen_pixel(&memory, 5, 5) == 0);

    p8_pset(&graphics, &memory, 10, 20, 7);
    assert(screen_pixel(&memory, 10, 20) == 7);
    assert(screen_pixel(&memory, 11, 20) == 0);

    p8_rect(&graphics, &memory, 30, 30, 34, 34, 9);
    for (int x = 30; x <= 34; x++) {
        assert(screen_pixel(&memory, x, 30) == 9);
        assert(screen_pixel(&memory, x, 34) == 9);
    }
    for (int y = 30; y <= 34; y++) {
        assert(screen_pixel(&memory, 30, y) == 9);
        assert(screen_pixel(&memory, 34, y) == 9);
    }
    assert(screen_pixel(&memory, 32, 32) == 0);

    p8_circ(&graphics, &memory, 60, 60, 3, 10);
    assert(screen_pixel(&memory, 63, 60) == 10);
    assert(screen_pixel(&memory, 57, 60) == 10);
    assert(screen_pixel(&memory, 60, 63) == 10);
    assert(screen_pixel(&memory, 60, 57) == 10);
    assert(screen_pixel(&memory, 60, 60) == 0);

    p8_spr(&graphics, &memory, 2, 0, 90, 1, 1);
    assert(screen_pixel(&memory, 0, 90) == 3);
    assert(screen_pixel(&memory, 7, 90) == 3);
    assert(screen_pixel(&memory, 2, 91) == 4);
    assert(screen_pixel(&memory, 1, 90) == 0);

    p8_mset(&memory, 1, 0, 81);
    p8_mset(&memory, 2, 0, 85);
    p8_mset(&memory, 3, 0, 1);
    p8_map(&graphics, &memory, 0, 0, 0, 100, 4, 1);
    /* Map cell (0,0) is 0, and `map()` takes sprite 0 to mean empty and
     * does not draw it, so the whole first 8x8 block stays cleared -- the
     * game's sprite 0 is palette data, not a tile. */
    for (int y = 100; y <= 107; y++) {
        for (int x = 0; x <= 7; x++) {
            assert(screen_pixel(&memory, x, y) == 0);
        }
    }
    assert(screen_pixel(&memory, 8, 100) == 0);
    assert(screen_pixel(&memory, 16, 100) == 1);
    assert(screen_pixel(&memory, 31, 107) == 6);

    p8_print(&graphics, &memory, "hi", 0, 115, 11);
    assert(screen_pixel(&memory, 0, 115) == 11);
    assert(screen_pixel(&memory, 1, 115) == 0);
    assert(screen_pixel(&memory, 2, 117) == 11);
    assert(screen_pixel(&memory, 5, 115) == 11);
    assert(screen_pixel(&memory, 4, 116) == 0);
    assert(screen_pixel(&memory, 5, 116) == 11);

    /* This block asserts the four-frame scenario's exact pad buttons, so it
     * reads a fixture of its own, never a playtest recording, which can be
     * replaced at any time. The fixture's shape and loadability are also
     * covered by tests/test_input.c. */
    p8_input_t input;
    p8_input_init(&input);
    assert(p8_input_load(&input, DEMO_EVENTS_PATH));
    assert(input.frame_count == 4);

    assert(input.frames[input.current_frame].pad_buttons == (P8_PAD_BUTTON_4 | P8_PAD_BUTTON_5));
    assert(p8_input_read_mouse(&input).x == 0);
    assert(p8_input_read_mouse(&input).buttons == 0);

    p8_input_step(&input);

    assert(input.frames[input.current_frame].pad_buttons == P8_PAD_BUTTON_1);

    p8_input_step(&input);

    assert(input.frames[input.current_frame].pad_buttons == 0);

    p8_input_free(&input);

    FILE *dump_out = fopen("tmp/demo_dump.txt", "w");
    assert(dump_out != NULL);
    int dump_ok = dump_screen(&memory, dump_out);
    fclose(dump_out);
    assert(dump_ok);

    printf("test_demo: all assertions passed; framebuffer dumped to tmp/demo_dump.txt\n");
    return 0;
}
