/* A row holding one absent item must draw nothing at all: the item is
 * NOTHING, neither a string nor a sprite, so row_measure_item() gives it no
 * draw -- it still takes its default width of 6, invisible rather than
 * absent. The trap this pins is kept at the guard itself,
 * layout_row_cell_row() in src/game/layout.c.
 *
 * WHY THIS TEST EXISTS beside the e2e tests/e2e/pad_colonise.json, whose
 * colony inspection panel replays the reported bug end to end: that one pins
 * only the message box half of the fix (message_box_open_box() testing
 * has_description). Once the message box stops building the bogus row, no
 * absent item reaches a row in any replay: reverting the NULL clause of
 * layout_row_cell_row() alone left the whole suite green. Without this test
 * the guard is correct today and free to rot the moment another caller hands
 * a row an absent item.
 *
 * row_measure_item() is static to src/game/layout.c, so this test never
 * names it: it builds a real row holding ONE positional item and renders it
 * with layout_row_draw_at_default(), the way the game reaches it too. */

#include <assert.h>
#include <stdio.h>
#include <string.h>

#include "p8/sprite_sheet.h"
#include "p8/fixed.h"
#include "p8/gfx.h"
#include "game/layout.h"
#include "p8/vram.h"
#include "engine/vec.h"
#include "heap_allocator.h"

/* Where the row is rendered. Any position clear of the screen edges does;
 * these put the whole 8x8 sprite the faulty build drew well inside the
 * framebuffer. */
#define DRAW_X 20
#define DRAW_Y 30

/* Sprite #0's bottom pixel row. Row 7 and not row 0: row 0 is all zeros, so
 * it would match blank background at every position on screen and make the
 * search vacuous. Row 7 is six distinct colours and occurs exactly once in
 * the whole 128x128 sheet. */
#define SPRITE0_ROW_PIXELS 8
#define SPRITE0_BOTTOM_ROW_Y 7

static p8_vram_t memory;
static p8_graphics_t graphics;

static int screen_pixel(int x, int y) {
    return memory.screen[y][x];
}

/* How many places sprite #0's bottom row appears in the framebuffer, with
 * the first one's top-left x/y written through the out-params. The wanted
 * run is read back out of the loaded sprite sheet, never hard-coded. */
static int sprite0_bottom_row_hits(int *out_x, int *out_y) {
    int wanted[SPRITE0_ROW_PIXELS];
    int i, x, y, hits = 0;

    for (i = 0; i < SPRITE0_ROW_PIXELS; i++) {
        wanted[i] = p8_sget(&memory, i, SPRITE0_BOTTOM_ROW_Y);
    }

    for (y = 0; y < P8_SCREEN_HEIGHT; y++) {
        for (x = 0; x + SPRITE0_ROW_PIXELS <= P8_SCREEN_WIDTH; x++) {
            int match = 1;
            for (i = 0; i < SPRITE0_ROW_PIXELS; i++) {
                if (screen_pixel(x + i, y) != wanted[i]) {
                    match = 0;
                    break;
                }
            }
            if (match) {
                if (hits == 0) {
                    *out_x = x;
                    *out_y = y;
                }
                hits++;
            }
        }
    }
    return hits;
}

/* How many framebuffer pixels `cls()` did NOT leave behind -- i.e. how much
 * this render actually drew. */
static int pixels_drawn(void) {
    int x, y, n = 0;
    for (y = 0; y < P8_SCREEN_HEIGHT; y++) {
        for (x = 0; x < P8_SCREEN_WIDTH; x++) {
            if (screen_pixel(x, y) != 0) {
                n++;
            }
        }
    }
    return n;
}

/* One real row holding one item shaped as in the data tables, its keys
 * taken by layout_row_keys_of_row(), drawn at (DRAW_X, DRAW_Y) on a cleared
 * screen. Returns its one placed item. */
static layout_row_item_t render_one_item(data_item_t item) {
    const data_row_t row = {.count = 1, .items = &item};
    layout_row_keys_t keys = layout_row_keys_of_row(&row);
    layout_row_t *line;

    line = layout_row_new(&heap_allocator, &keys);
    assert(line != NULL && line->placed_items_count == 1);

    p8_cls(&memory);
    layout_row_draw_at_default(&graphics, &memory, line, vec_make(p8_fixed_from_int(DRAW_X), p8_fixed_from_int(DRAW_Y)));
    return line->placed_items[0];
}

/* THE SUBJECT: a row holding one absent item must draw nothing at all. */
static void test_nil_item_draws_nothing(void) {
    int hit_x = -1, hit_y = -1;
    int hits, drawn;
    layout_row_item_t placed;

    placed = render_one_item((data_item_t){.is_nil = 1});
    assert(placed.kind == LAYOUT_ROW_ITEM_NONE);

    hits = sprite0_bottom_row_hits(&hit_x, &hit_y);
    drawn = pixels_drawn();
    printf("[nil]     layout_row_draw_at of a row with an absent cell drew %d pixel(s); sprite #0's bottom row appears %d time(s)\n", drawn, hits);
    if (hits > 0) {
        printf("[nil]     first occurrence at (%d,%d)\n", hit_x, hit_y);
    }
    /* assert() aborts, and stdout is fully buffered when piped (it is, under
     * `make test`): flush so a failure still shows its measurement. */
    fflush(stdout);
    assert(hits == 0);
    assert(drawn == 0);
}

/* THE POSITIVE CONTROL, without which "draws nothing, ever" would pass the
 * assertions above: the same path, given a genuine sprite number, really
 * does draw that sprite, exactly where the layout put it. The item is the
 * number 0, a FIXED cell like any number, so control and subject differ
 * only in the item being absent, and both aim at sprite 0. */
static void test_numeric_item_still_draws_its_sprite(void) {
    int hit_x = -1, hit_y = -1;
    int hits, drawn;
    layout_row_item_t placed;

    placed = render_one_item((data_item_t){.number = 0});
    assert(placed.kind == LAYOUT_ROW_ITEM_SPRITE && placed.sprite_id == p8_fixed_from_int(0));

    hits = sprite0_bottom_row_hits(&hit_x, &hit_y);
    drawn = pixels_drawn();
    printf("[control] layout_row_draw_at of a row with a 0 cell drew %d pixel(s); sprite #0's bottom row appears %d time(s) at (%d,%d)\n", drawn,
           hits, hit_x, hit_y);
    fflush(stdout);
    assert(hits == 1);
    assert(hit_x == DRAW_X);
    assert(hit_y == DRAW_Y + SPRITE0_BOTTOM_ROW_Y);
    assert(drawn > 0);
}

int main(void) {
    p8_vram_init(&memory);
    p8_gfx_init(&graphics);

    p8_sprite_sheet_load(&memory);

    layout_init();

    test_nil_item_draws_nothing();
    test_numeric_item_still_draws_its_sprite();

    printf("test_layout_nil_item_draws_nothing: all tests passed\n");
    return 0;
}
