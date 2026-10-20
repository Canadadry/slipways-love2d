#include "p8/sprite_sheet.h"
#include "p8/gfx.h"
#include "p8/vram.h"

#include <assert.h>
#include <stdio.h>
#include <string.h>

static void test_cls_clears_whole_screen_to_zero(void) {
    p8_vram_t mem;
    p8_vram_init(&mem);

    memset(mem.screen, 9, sizeof(mem.screen));

    p8_cls(&mem);

    for (int y = 0; y < P8_SCREEN_HEIGHT; y++) {
        for (int x = 0; x < P8_SCREEN_WIDTH; x++) {
            assert(mem.screen[y][x] == 0);
        }
    }
}

static void test_pset_plots_pixel_at_coordinate(void) {
    p8_vram_t mem;
    p8_graphics_t gfx;
    p8_vram_init(&mem);
    p8_gfx_init(&gfx);

    p8_pset(&gfx, &mem, 10, 20, 7);

    assert(mem.screen[20][10] == 7);
    assert(mem.screen[20][11] == 0);
}

static void test_pset_off_screen_is_clipped_silently(void) {
    p8_vram_t mem;
    p8_graphics_t gfx;
    p8_vram_init(&mem);
    p8_gfx_init(&gfx);

    p8_pset(&gfx, &mem, -1, 0, 7);
    p8_pset(&gfx, &mem, 0, -1, 7);
    p8_pset(&gfx, &mem, P8_SCREEN_WIDTH, 0, 7);
    p8_pset(&gfx, &mem, 0, P8_SCREEN_HEIGHT, 7);

    for (int y = 0; y < P8_SCREEN_HEIGHT; y++) {
        for (int x = 0; x < P8_SCREEN_WIDTH; x++) {
            assert(mem.screen[y][x] == 0);
        }
    }
}

static void test_camera_offset_shifts_subsequent_pset(void) {
    p8_vram_t mem;
    p8_graphics_t gfx;
    p8_vram_init(&mem);
    p8_gfx_init(&gfx);

    p8_camera(&gfx, 10, 5);
    p8_pset(&gfx, &mem, 15, 15, 7);

    /* camera(10,5) shifts the world left/up by (10,5): world (15,15)
     * lands at screen (5,10). */
    assert(mem.screen[10][5] == 7);
    assert(mem.screen[15][15] == 0);
}

static void test_camera_reset_restores_zero_offset(void) {
    p8_vram_t mem;
    p8_graphics_t gfx;
    p8_vram_init(&mem);
    p8_gfx_init(&gfx);

    p8_camera(&gfx, 10, 5);
    p8_camera_reset(&gfx);
    p8_pset(&gfx, &mem, 3, 4, 6);

    assert(mem.screen[4][3] == 6);
}

static void test_vram_init_default_transparency_is_color_0_only(void) {
    p8_vram_t mem;
    p8_vram_init(&mem);

    assert(p8_gfx_is_transparent(&mem, 0));
    for (int c = 1; c < 16; c++) {
        assert(!p8_gfx_is_transparent(&mem, c));
    }
}

static void test_palt_marks_color_transparent_and_opaque(void) {
    p8_vram_t mem;
    p8_vram_init(&mem);

    p8_palt(&mem, 3, 1);
    assert(p8_gfx_is_transparent(&mem, 3));

    p8_palt(&mem, 0, 0);
    assert(!p8_gfx_is_transparent(&mem, 0));

    p8_palt(&mem, 3, 0);
    assert(!p8_gfx_is_transparent(&mem, 3));
}

/* The consequence of that single store, which the game depends on:
 * hud_select_palette_block()'s write of the draw palette replaces whatever
 * `palt()` last said. The game undoes its p8_palt(mem, 0, 1) with
 * hud_select_palette_block_default(), never with p8_palt(mem, 0, 0). */
static void test_draw_palette_write_overrides_a_previous_palt(void) {
    p8_vram_t mem;
    p8_vram_init(&mem);

    p8_palt(&mem, 0, 1);
    assert(p8_gfx_is_transparent(&mem, 0));

    for (int c = 0; c < 16; c++) {
        mem.draw_palette[c] = (unsigned char)(c == 3 ? (c | P8_DRAW_PALETTE_TRANSPARENT_BIT) : c);
    }

    assert(!p8_gfx_is_transparent(&mem, 0));
    assert(p8_gfx_is_transparent(&mem, 3));
}

static void test_palt_out_of_range_col_is_noop_and_queries_as_opaque(void) {
    p8_vram_t mem;
    p8_vram_init(&mem);

    p8_palt(&mem, -1, 1);
    p8_palt(&mem, 16, 1);

    assert(!p8_gfx_is_transparent(&mem, -1));
    assert(!p8_gfx_is_transparent(&mem, 16));
}

static void test_palt_affects_a_subsequent_transparent_color_draw(void) {
    /* `pset` never consults `palt`: as in real PICO-8, transparency only
     * affects the sprite primitives. What is exercised here is the "does
     * this pixel draw" query `spr()` makes per source pixel. */
    p8_vram_t mem;
    p8_graphics_t gfx;
    p8_vram_init(&mem);
    p8_gfx_init(&gfx);

    p8_palt(&mem, 3, 1);

    int col = 3;
    if (!p8_gfx_is_transparent(&mem, col)) {
        p8_pset(&gfx, &mem, 5, 5, col);
    }
    assert(mem.screen[5][5] == 0);

    p8_palt(&mem, 3, 0);
    if (!p8_gfx_is_transparent(&mem, col)) {
        p8_pset(&gfx, &mem, 5, 5, col);
    }
    assert(mem.screen[5][5] == 3);
}

static void test_line_draws_diagonal_pixels(void) {
    p8_vram_t mem;
    p8_graphics_t gfx;
    p8_vram_init(&mem);
    p8_gfx_init(&gfx);

    p8_line(&gfx, &mem, 2, 2, 5, 5, 8);

    for (int i = 2; i <= 5; i++) {
        assert(mem.screen[i][i] == 8);
    }
    assert(mem.screen[2][3] == 0);
}

static void test_line_zero_length_draws_single_pixel(void) {
    p8_vram_t mem;
    p8_graphics_t gfx;
    p8_vram_init(&mem);
    p8_gfx_init(&gfx);

    p8_line(&gfx, &mem, 7, 9, 7, 9, 11);

    assert(mem.screen[9][7] == 11);
    assert(mem.screen[9][8] == 0);
}

static void test_rect_draws_unfilled_outline(void) {
    p8_vram_t mem;
    p8_graphics_t gfx;
    p8_vram_init(&mem);
    p8_gfx_init(&gfx);

    p8_rect(&gfx, &mem, 2, 2, 5, 4, 8);

    assert(mem.screen[2][2] == 8);
    assert(mem.screen[2][5] == 8);
    assert(mem.screen[4][2] == 8);
    assert(mem.screen[4][5] == 8);
    assert(mem.screen[2][3] == 8);
    assert(mem.screen[3][2] == 8);
    assert(mem.screen[3][3] == 0);
}

static void test_rect_single_pixel_draws_one_point(void) {
    p8_vram_t mem;
    p8_graphics_t gfx;
    p8_vram_init(&mem);
    p8_gfx_init(&gfx);

    p8_rect(&gfx, &mem, 6, 6, 6, 6, 12);

    assert(mem.screen[6][6] == 12);
    assert(mem.screen[6][7] == 0);
    assert(mem.screen[7][6] == 0);
}

static void test_rectfill_fills_every_pixel_in_bounds(void) {
    p8_vram_t mem;
    p8_graphics_t gfx;
    p8_vram_init(&mem);
    p8_gfx_init(&gfx);

    p8_rectfill(&gfx, &mem, 2, 2, 4, 3, 9);

    for (int y = 2; y <= 3; y++) {
        for (int x = 2; x <= 4; x++) {
            assert(mem.screen[y][x] == 9);
        }
    }
    assert(mem.screen[4][2] == 0);
    assert(mem.screen[2][5] == 0);
}

static void test_circ_draws_unfilled_outline(void) {
    p8_vram_t mem;
    p8_graphics_t gfx;
    p8_vram_init(&mem);
    p8_gfx_init(&gfx);

    p8_circ(&gfx, &mem, 10, 10, 3, 8);

    assert(mem.screen[10][13] == 8);
    assert(mem.screen[10][7] == 8);
    assert(mem.screen[13][10] == 8);
    assert(mem.screen[7][10] == 8);
    assert(mem.screen[10][10] == 0);
}

static void test_circ_zero_radius_draws_single_pixel(void) {
    p8_vram_t mem;
    p8_graphics_t gfx;
    p8_vram_init(&mem);
    p8_gfx_init(&gfx);

    p8_circ(&gfx, &mem, 5, 5, 0, 6);

    assert(mem.screen[5][5] == 6);
    assert(mem.screen[5][6] == 0);
    assert(mem.screen[4][5] == 0);
}

static void test_circfill_fills_the_disc(void) {
    p8_vram_t mem;
    p8_graphics_t gfx;
    p8_vram_init(&mem);
    p8_gfx_init(&gfx);

    p8_circfill(&gfx, &mem, 10, 10, 3, 8);

    assert(mem.screen[10][10] == 8);
    assert(mem.screen[10][13] == 8);
    assert(mem.screen[10][7] == 8);
    assert(mem.screen[10][14] == 0);
}

static void test_circfill_zero_radius_draws_single_pixel(void) {
    p8_vram_t mem;
    p8_graphics_t gfx;
    p8_vram_init(&mem);
    p8_gfx_init(&gfx);

    p8_circfill(&gfx, &mem, 5, 5, 0, 6);

    assert(mem.screen[5][5] == 6);
    assert(mem.screen[5][6] == 0);
    assert(mem.screen[4][5] == 0);
}

static void test_fillp_pattern_alters_rectfill_output(void) {
    p8_vram_t mem;
    p8_graphics_t gfx;
    p8_vram_init(&mem);
    p8_gfx_init(&gfx);

    /* Rows alternate solid/skip in groups of 4: row%4==0..2 draws (bits
     * clear), row%4==3 skips (bits set) -- 0b0000000000001111 marks only
     * the pattern's last row as "skip". */
    p8_fillp(&gfx, 0x000F);
    p8_rectfill(&gfx, &mem, 0, 0, 3, 3, 8);

    for (int y = 0; y < 3; y++) {
        for (int x = 0; x < 4; x++) {
            assert(mem.screen[y][x] == 8);
        }
    }
    for (int x = 0; x < 4; x++) {
        assert(mem.screen[3][x] == 0);
    }

    p8_fillp_reset(&gfx);
    p8_rectfill(&gfx, &mem, 0, 0, 3, 3, 8);
    for (int y = 0; y < 4; y++) {
        for (int x = 0; x < 4; x++) {
            assert(mem.screen[y][x] == 8);
        }
    }
}

static void test_sget_reads_known_pixel_from_loaded_sheet(void) {
    /* The three known-good values, in raw sheet pixel coordinates, not
     * sprite index plus offset: (16,0) is sprite 2's top-left, (2,64) is
     * sprite 128's top row two pixels in, (0,127) is sprite 240's
     * bottom-left. */
    p8_vram_t mem;
    p8_vram_init(&mem);
    p8_sprite_sheet_load(&mem);

    assert(p8_sget(&mem, 16, 0) == 3);
    assert(p8_sget(&mem, 2, 64) == 1);
    assert(p8_sget(&mem, 0, 127) == 3);
}

static void test_sget_out_of_range_returns_zero(void) {
    p8_vram_t mem;
    p8_vram_init(&mem);
    /* Write a nonzero value adjacent to the sheet's bounds to make sure a
     * naive/unclamped read wouldn't accidentally also read 0 here. */
    mem.sprite_sheet[0][0] = 9;

    assert(p8_sget(&mem, -1, 0) == 0);
    assert(p8_sget(&mem, 0, -1) == 0);
    assert(p8_sget(&mem, P8_SPRITE_SHEET_WIDTH, 0) == 0);
    assert(p8_sget(&mem, 0, P8_SPRITE_SHEET_HEIGHT) == 0);
}

static void test_spr_default_draws_1x1_sprite_at_world_position(void) {
    /* Sprite 5 lives at sheet pixel (40,0) (col 5*8, row 0*8, since 16 sprites/row). */
    p8_vram_t mem;
    p8_graphics_t gfx;
    p8_vram_init(&mem);
    p8_gfx_init(&gfx);

    mem.sprite_sheet[0][40] = 7;
    mem.sprite_sheet[1][41] = 8;

    p8_spr_default(&gfx, &mem, 5, 20, 30);

    assert(mem.screen[30][20] == 7);
    assert(mem.screen[31][21] == 8);
    /* Outside the 8x8 block is untouched. */
    assert(mem.screen[38][20] == 0);
}

static void test_spr_with_explicit_width_height_draws_multi_block_sprite(void) {
    /* Sprite 0 spans a 2x2-sprite (16x16 pixel) area starting at sheet
     * (0,0) when drawn 2 cells wide and 2 tall: it reads pixels from sprite
     * 0's own block plus sprite 1 (to its right), sprite 16 (below), and
     * sprite 17 (diagonal) -- real PICO-8's multi-cell sprite behavior. */
    p8_vram_t mem;
    p8_graphics_t gfx;
    p8_vram_init(&mem);
    p8_gfx_init(&gfx);

    mem.sprite_sheet[0][0] = 1;   /* sprite 0, corner */
    mem.sprite_sheet[0][8] = 2;   /* sprite 1's corner */
    mem.sprite_sheet[8][0] = 3;   /* sprite 16's corner */
    mem.sprite_sheet[15][15] = 4; /* sprite 17's far corner */

    p8_spr(&gfx, &mem, 0, 0, 0, 2, 2);

    assert(mem.screen[0][0] == 1);
    assert(mem.screen[0][8] == 2);
    assert(mem.screen[8][0] == 3);
    assert(mem.screen[15][15] == 4);
    /* Beyond the 16x16 area is untouched. */
    assert(mem.screen[16][0] == 0);
}

static void test_spr_skips_pixels_marked_transparent_by_palt(void) {
    p8_vram_t mem;
    p8_graphics_t gfx;
    p8_vram_init(&mem);
    p8_gfx_init(&gfx);

    mem.sprite_sheet[0][0] = 3; /* sprite 0, corner pixel */
    mem.screen[0][0] = 9; /* pre-existing content */

    p8_palt(&mem, 3, 1);
    p8_spr_default(&gfx, &mem, 0, 0, 0);
    assert(mem.screen[0][0] == 9);

    p8_palt(&mem, 3, 0);
    p8_spr_default(&gfx, &mem, 0, 0, 0);
    assert(mem.screen[0][0] == 3);
}

static void test_spr_draws_source_colors_through_the_draw_palette_remap(void) {
    static const unsigned char grey_block[16] = {0, 1, 1, 129, 1, 1, 5, 13, 1, 1, 5, 5, 1, 1, 5, 5};
    p8_vram_t mem;
    p8_graphics_t gfx;
    p8_vram_init(&mem);
    p8_gfx_init(&gfx);
    for (int c = 0; c < 16; c++) {
        mem.draw_palette[c] = grey_block[c];
    }

    mem.sprite_sheet[0][0] = 7;
    mem.sprite_sheet[0][1] = 10;
    mem.sprite_sheet[0][2] = 3;
    mem.screen[0][2] = 9;

    p8_spr_default(&gfx, &mem, 0, 0, 0);

    assert(mem.screen[0][0] == 13);
    assert(mem.screen[0][1] == 5);
    assert(mem.screen[0][2] == 9);
}

static void test_mset_writes_base_region_cell(void) {
    p8_vram_t mem;
    p8_vram_init(&mem);

    p8_mset(&mem, 9, 0, 213);
    p8_mset(&mem, 0, 31, 51);

    assert(mem.map_cells[0][9] == 213);
    assert(mem.map_cells[31][0] == 51);
}

static void test_mset_outside_32x32_changes_no_cell(void) {
    p8_vram_t mem;
    p8_graphics_t gfx;
    p8_vram_init(&mem);
    p8_gfx_init(&gfx);

    mem.sprite_sheet[0][40] = 7; /* sprite 5's corner */
    p8_mset(&mem, 31, 31, 5);
    p8_mset(&mem, 32, 0, 5);
    p8_mset(&mem, 0, 32, 5);
    p8_mset(&mem, 0, 63, 5);

    p8_map(&gfx, &mem, 31, 31, 0, 0, 2, 2);
    p8_map(&gfx, &mem, 32, 0, 16, 0, 1, 1);
    p8_map(&gfx, &mem, 0, 63, 24, 0, 1, 1);

    assert(mem.screen[0][0] == 7);
    assert(mem.screen[0][8] == 0);
    assert(mem.screen[8][0] == 0);
    assert(mem.screen[0][16] == 0);
    assert(mem.screen[0][24] == 0);
    for (int y = 0; y < P8_MAP_HEIGHT; y++) {
        for (int x = 0; x < P8_MAP_WIDTH; x++) {
            assert(mem.map_cells[y][x] == ((x == 31 && y == 31) ? 5 : 0));
        }
    }
}

static void test_mset_out_of_range_is_noop(void) {
    p8_vram_t mem;
    p8_vram_init(&mem);

    p8_mset(&mem, -1, 0, 5);
    p8_mset(&mem, 0, -1, 5);
    p8_mset(&mem, P8_MAP_WIDTH, 0, 5);
    p8_mset(&mem, 0, P8_MAP_HEIGHT, 5);

    for (int y = 0; y < P8_MAP_HEIGHT; y++) {
        for (int x = 0; x < P8_MAP_WIDTH; x++) {
            assert(mem.map_cells[y][x] == 0);
        }
    }
}

static void test_map_draws_cells_via_sprite_drawing_logic(void) {
    /* A 2x2-cell region, cell (1,0) pointing to sprite 5, the rest to
     * sprite 0. map() draws each cell as a 1x1 spr() call, cells 8 pixels
     * apart from the drawing origin -- confirmed by cell (1,0) landing
     * sprite 5's known pixel at screen column 8 (the second cell), not
     * column 0. */
    p8_vram_t mem;
    p8_graphics_t gfx;
    p8_vram_init(&mem);
    p8_gfx_init(&gfx);

    mem.sprite_sheet[0][40] = 7; /* sprite 5's corner */
    p8_mset(&mem, 1, 0, 5);

    p8_map(&gfx, &mem, 0, 0, 0, 0, 2, 2);

    assert(mem.screen[0][8] == 7);
    /* Cell (0,0) is sprite 0, whose corner pixel was never written (stays
     * palette index 0, which is transparent by default and so is
     * skipped -- screen stays at its cleared 0 value either way). */
    assert(mem.screen[0][0] == 0);
}

static void test_map_draws_the_same_screen_as_one_spr_per_cell(void) {
    static p8_vram_t culled;
    static p8_vram_t reference;
    static const int cameras[][2] = {{-153, -113}, {-7, -7}, {3, 5}, {97, 137}, {251, 249}, {-160, 120}};
    p8_graphics_t gfx;
    p8_vram_init(&culled);
    for (int y = 0; y < P8_SPRITE_SHEET_HEIGHT; y++) {
        for (int x = 0; x < P8_SPRITE_SHEET_WIDTH; x++) {
            culled.sprite_sheet[y][x] = (unsigned char)((x * 7 + y * 3) % 16);
        }
    }
    for (int r = 0; r < 32; r++) {
        for (int c = 0; c < 32; c++) {
            p8_mset(&culled, c, r, (r * 5 + c) % 64);
        }
    }
    for (int i = 0; i < 6; i++) {
        p8_gfx_init(&gfx);
        p8_camera(&gfx, cameras[i][0], cameras[i][1]);
        memset(culled.screen, 1, sizeof(culled.screen));
        reference = culled;

        p8_map(&gfx, &culled, 0, 0, 0, 0, 32, 32);
        for (int r = 0; r < 32; r++) {
            for (int c = 0; c < 32; c++) {
                int n = (r * 5 + c) % 64;
                if (n != 0) {
                    p8_spr(&gfx, &reference, n, c * P8_SPRITE_SIZE, r * P8_SPRITE_SIZE, 1, 1);
                }
            }
        }

        assert(memcmp(culled.screen, reference.screen, sizeof(culled.screen)) == 0);
    }
}

static void test_map_treats_sprite_0_cells_as_empty(void) {
    /* Real PICO-8's map() "takes sprite 0 to mean empty and does not draw
     * it" (PICO-8 manual, MAP()); the opt-out POKE(0x5F36,0x8) is never
     * poked by the game. So a cell holding 0 leaves its 8x8 block alone
     * even when sprite 0's own pixels are opaque -- which they are in the
     * game's sheet, whose sprite 0 is palette data, not artwork. Direct
     * spr(0,...) is unaffected by that rule and still draws. */
    p8_vram_t mem;
    p8_graphics_t gfx;
    p8_vram_init(&mem);
    p8_gfx_init(&gfx);

    /* Sprite 0's top-left pixel, opaque colour 7. */
    mem.sprite_sheet[0][0] = 7;
    p8_mset(&mem, 0, 0, 0);

    p8_map(&gfx, &mem, 0, 0, 0, 0, 1, 1);
    assert(mem.screen[0][0] == 0);

    p8_spr(&gfx, &mem, 0, 0, 0, 1, 1);
    assert(mem.screen[0][0] == 7);
}

static void test_map_and_mset_round_trip_against_real_sheet(void) {
    p8_vram_t mem;
    p8_graphics_t gfx;
    p8_vram_init(&mem);
    p8_gfx_init(&gfx);
    p8_sprite_sheet_load(&mem);

    /* Sprite 2's top-left pixel is known-good color 3 (see
     * test_sget_reads_known_pixel_from_loaded_sheet). */
    unsigned char expected = p8_sget(&mem, 16, 0);
    assert(expected == 3);

    p8_mset(&mem, 4, 20, 2);
    p8_map(&gfx, &mem, 4, 20, 0, 0, 1, 1);

    assert(mem.map_cells[20][4] == 2);
    assert(mem.screen[0][0] == expected);
}

static void test_print_draws_known_glyph_pixels(void) {
    /* Spot-check the hand-authored 'a' glyph: a 3-wide x 5-tall bitmap
     * (see P8_FONT_GLYPH_WIDTH/HEIGHT) left-aligned at (x,y), with
     * column x+3 (the 4th column of the 4px advance cell) always blank. */
    p8_vram_t mem;
    p8_graphics_t gfx;
    p8_vram_init(&mem);
    p8_gfx_init(&gfx);

    p8_print(&gfx, &mem, "a", 10, 20, 7);

    int expected[5][3] = {
        {0, 1, 0},
        {1, 0, 1},
        {1, 1, 1},
        {1, 0, 1},
        {1, 0, 1},
    };
    for (int row = 0; row < 5; row++) {
        for (int col = 0; col < 3; col++) {
            unsigned char pixel = mem.screen[20 + row][10 + col];
            assert(pixel == (expected[row][col] ? 7 : 0));
        }
        assert(mem.screen[20 + row][13] == 0);
    }
}

static void test_print_advances_cursor_by_cell_width_between_chars(void) {
    p8_vram_t mem;
    p8_graphics_t gfx;
    p8_vram_init(&mem);
    p8_gfx_init(&gfx);

    p8_print(&gfx, &mem, "ab", 0, 0, 6);

    /* Second glyph ('b') starts 4px to the right of the first, at x=4. */
    int expected_b[5][3] = {
        {1, 0, 0},
        {1, 0, 0},
        {1, 1, 0},
        {1, 0, 1},
        {1, 1, 0},
    };
    for (int row = 0; row < 5; row++) {
        for (int col = 0; col < 3; col++) {
            unsigned char pixel = mem.screen[row][4 + col];
            assert(pixel == (expected_b[row][col] ? 6 : 0));
        }
    }
}

static void test_print_unmapped_character_draws_placeholder_block(void) {
    p8_vram_t mem;
    p8_graphics_t gfx;
    p8_vram_init(&mem);
    p8_gfx_init(&gfx);

    p8_print(&gfx, &mem, "@", 0, 0, 8);

    for (int row = 0; row < 5; row++) {
        for (int col = 0; col < 3; col++) {
            assert(mem.screen[row][col] == 8);
        }
    }
}

static void test_print_space_is_blank(void) {
    p8_vram_t mem;
    p8_graphics_t gfx;
    p8_vram_init(&mem);
    p8_gfx_init(&gfx);

    p8_print(&gfx, &mem, " ", 0, 0, 5);

    for (int row = 0; row < 5; row++) {
        for (int col = 0; col < 4; col++) {
            assert(mem.screen[row][col] == 0);
        }
    }
}

static void test_print_respects_camera_offset(void) {
    p8_vram_t mem;
    p8_graphics_t gfx;
    p8_vram_init(&mem);
    p8_gfx_init(&gfx);
    p8_camera(&gfx, 10, 20);

    p8_print(&gfx, &mem, "a", 10, 20, 7);

    /* World (10,20) minus camera (10,20) lands at screen (0,0); the 'a'
     * glyph's top-left pixel (row0,col1) should be lit there. */
    assert(mem.screen[0][1] == 7);
    assert(mem.screen[0][0] == 0);
}

static void test_print_digit_and_punctuation_glyphs(void) {
    p8_vram_t mem;
    p8_graphics_t gfx;
    p8_vram_init(&mem);
    p8_gfx_init(&gfx);

    p8_print(&gfx, &mem, "1:", 0, 0, 9);

    int expected_1[5][3] = {
        {0, 1, 0},
        {1, 1, 0},
        {0, 1, 0},
        {0, 1, 0},
        {1, 1, 1},
    };
    for (int row = 0; row < 5; row++) {
        for (int col = 0; col < 3; col++) {
            unsigned char pixel = mem.screen[row][col];
            assert(pixel == (expected_1[row][col] ? 9 : 0));
        }
    }

    /* ':' (second cell, starting at x=4) row-by-row: ... / .#. / ... / .#. / ... */
    int expected_colon[5][3] = {
        {0, 0, 0},
        {0, 1, 0},
        {0, 0, 0},
        {0, 1, 0},
        {0, 0, 0},
    };
    for (int row = 0; row < 5; row++) {
        for (int col = 0; col < 3; col++) {
            unsigned char pixel = mem.screen[row][4 + col];
            assert(pixel == (expected_colon[row][col] ? 9 : 0));
        }
    }
}

static void test_font_has_glyph_is_one_only_for_a_drawn_glyph(void) {
    assert(p8_font_has_glyph('a'));
    assert(p8_font_has_glyph('A'));
    assert(p8_font_has_glyph('0'));
    assert(p8_font_has_glyph(' '));
    assert(p8_font_has_glyph(':'));
    assert(p8_font_has_glyph('?'));
    assert(p8_font_has_glyph('v') && "v has a glyph, the wrong one: a separate rule refuses it");

    assert(!p8_font_has_glyph('&'));
    assert(!p8_font_has_glyph('"'));
    assert(!p8_font_has_glyph('\\'));
    assert(!p8_font_has_glyph('#'));
    assert(!p8_font_has_glyph('\t'));
    assert(!p8_font_has_glyph('\n'));
    assert(!p8_font_has_glyph((char)0xc3));
}

static void paint_background(p8_vram_t *mem) {
    for (int y = 0; y < P8_SCREEN_HEIGHT; y++) {
        for (int x = 0; x < P8_SCREEN_WIDTH; x++) mem->screen[y][x] = (unsigned char)(1 + (x + 3 * y) % 15);
    }
}

static p8_vram_t outlined_mem;
static p8_vram_t nine_pass_mem;

static void assert_outlined_is_nine_prints(const p8_graphics_t *gfx, const char *str, int x, int y, int col) {
    static const int offsets[8][2] = { {-1, -1}, {0, -1}, {1, -1}, {-1, 0}, {1, 0}, {-1, 1}, {0, 1}, {1, 1} };
    paint_background(&outlined_mem);
    paint_background(&nine_pass_mem);

    p8_print_outlined(gfx, &outlined_mem, str, x, y, col);
    for (int i = 0; i < 8; i++) p8_print(gfx, &nine_pass_mem, str, x + offsets[i][0], y + offsets[i][1], 0);
    p8_print(gfx, &nine_pass_mem, str, x, y, col);

    assert(memcmp(outlined_mem.screen, nine_pass_mem.screen, sizeof outlined_mem.screen) == 0);
}

static void test_print_outlined_is_the_nine_prints_for_every_character(void) {
    p8_graphics_t gfx;
    p8_vram_init(&outlined_mem);
    p8_vram_init(&nine_pass_mem);
    p8_gfx_init(&gfx);
    for (int code = 1; code < 256; code++) {
        char str[4] = { 'a', (char)code, 'b', '\0' };
        assert_outlined_is_nine_prints(&gfx, str, 40, 50, 7);
    }
    assert_outlined_is_nine_prints(&gfx, "a b@ ~", 10, 10, 12);
    assert_outlined_is_nine_prints(&gfx, "   ", 10, 10, 12);
    assert_outlined_is_nine_prints(&gfx, "", 10, 10, 12);
    assert_outlined_is_nine_prints(&gfx, "zero", 10, 10, 0);
}

static void test_print_outlined_is_the_nine_prints_at_every_edge_and_camera(void) {
    static const int xs[] = { -14, -13, -12, -9, -8, -7, -4, -1, 0, 1, 70, 148, 149, 150, 156, 158, 159, 160, 161 };
    static const int ys[] = { -7, -6, -5, -4, -1, 0, 1, 60, 113, 114, 115, 118, 119, 120, 121 };
    static const int cameras[][2] = { {0, 0}, {-5, 3}, {37, -21}, {-160, 0}, {0, 120} };
    p8_graphics_t gfx;
    p8_vram_init(&outlined_mem);
    p8_vram_init(&nine_pass_mem);
    p8_gfx_init(&gfx);
    for (size_t c = 0; c < sizeof cameras / sizeof cameras[0]; c++) {
        p8_camera(&gfx, cameras[c][0], cameras[c][1]);
        for (size_t i = 0; i < sizeof xs / sizeof xs[0]; i++) {
            for (size_t j = 0; j < sizeof ys / sizeof ys[0]; j++) {
                assert_outlined_is_nine_prints(&gfx, "w8%", xs[i] + cameras[c][0], ys[j] + cameras[c][1], 9);
            }
        }
    }
}

static void test_print_outlined_shows_its_outline_over_a_coloured_background(void) {
    p8_graphics_t gfx;
    p8_vram_init(&outlined_mem);
    p8_gfx_init(&gfx);
    paint_background(&outlined_mem);

    p8_print_outlined(&gfx, &outlined_mem, "a", 10, 20, 7);

    assert(outlined_mem.screen[19][11] == 0 && "above the top lit pixel");
    assert(outlined_mem.screen[20][10] == 0 && "beside the top lit pixel, inside the cell");
    assert(outlined_mem.screen[22][9] == 0 && "left of the glyph");
    assert(outlined_mem.screen[22][13] == 0 && "the gap column");
    assert(outlined_mem.screen[25][12] == 0 && "below the bottom row");
    assert(outlined_mem.screen[21][11] == 0 && "an unlit pixel inside the glyph");
    assert(outlined_mem.screen[20][11] == 7);
    assert(outlined_mem.screen[18][11] != 0 && "two pixels away keeps the background");
    assert(outlined_mem.screen[22][14] != 0);
}

static void fill_screen_pattern(p8_vram_t *mem) {
    for (int y = 0; y < P8_SCREEN_HEIGHT; y++) {
        for (int x = 0; x < P8_SCREEN_WIDTH; x++) mem->screen[y][x] = (unsigned char)((x * 3 + y * 5) % 16);
    }
}

static void draw_bundle_both_ways(const p8_segment_t *segments, int count, int col, int camera_x, int camera_y) {
    static p8_vram_t batched;
    static p8_vram_t single;
    p8_graphics_t gfx;
    p8_vram_init(&batched);
    p8_vram_init(&single);
    p8_gfx_init(&gfx);
    p8_camera(&gfx, camera_x, camera_y);
    fill_screen_pattern(&batched);
    fill_screen_pattern(&single);

    p8_lines(&gfx, &batched, segments, count, col);
    for (int i = 0; i < count; i++) {
        p8_line(&gfx, &single, segments[i].x1, segments[i].y1, segments[i].x2, segments[i].y2, col);
    }

    assert(memcmp(batched.screen, single.screen, sizeof batched.screen) == 0);
}

static void test_lines_is_its_lines_drawn_one_by_one_at_every_edge_and_camera(void) {
    static const int cameras[][2] = { { 0, 0 }, { 37, -21 }, { -64, 90 } };
    for (int c = 0; c < 3; c++) {
        for (int x = -37; x <= P8_SCREEN_WIDTH + 5; x += 14) {
            for (int y = -20; y <= P8_SCREEN_HEIGHT + 5; y += 13) {
                int wx = x + cameras[c][0];
                int wy = y + cameras[c][1];
                p8_segment_t bundle[4] = {
                    { wx - 1, wy - 1, wx + 29, wy + 13 }, { wx - 1, wy + 2, wx + 29, wy + 16 },
                    { wx + 2, wy - 1, wx + 32, wy + 13 }, { wx + 2, wy + 2, wx + 32, wy + 16 },
                };
                draw_bundle_both_ways(bundle, 4, 0x1d, cameras[c][0], cameras[c][1]);
            }
        }
    }
}

static void test_lines_draws_overlapping_segments_and_odd_counts_like_its_lines(void) {
    static const p8_segment_t crossing[] = {
        { 0, 0, 127, 127 }, { 127, 0, 0, 127 }, { 64, -5, 64, 140 }, { 10, 10, 10, 10 }, { -300, 60, 400, 61 },
    };
    draw_bundle_both_ways(crossing, 5, 9, 0, 0);
    draw_bundle_both_ways(crossing, 3, 0x2c, 11, -7);
}

static void draw_points_both_ways(const p8_point_t *points, int count, int camera_x, int camera_y) {
    static p8_vram_t batched;
    static p8_vram_t single;
    p8_graphics_t gfx;
    p8_vram_init(&batched);
    p8_vram_init(&single);
    p8_gfx_init(&gfx);
    p8_camera(&gfx, camera_x, camera_y);
    fill_screen_pattern(&batched);
    fill_screen_pattern(&single);

    p8_psets(&gfx, &batched, points, count);
    for (int i = 0; i < count; i++) p8_pset(&gfx, &single, points[i].x, points[i].y, points[i].col);

    assert(memcmp(batched.screen, single.screen, sizeof batched.screen) == 0);
}

static void test_psets_is_its_psets_drawn_one_by_one_at_every_edge_and_camera(void) {
    static const int cameras[][2] = { { 0, 0 }, { 37, -21 }, { -64, 90 } };
    enum { EDGE_POINTS = 2 * (P8_SCREEN_WIDTH + 4) + 2 * (P8_SCREEN_HEIGHT + 4) };
    static p8_point_t points[EDGE_POINTS];
    for (int c = 0; c < 3; c++) {
        int count = 0;
        for (int x = -2; x < P8_SCREEN_WIDTH + 2; x++) {
            points[count++] = (p8_point_t){ x + cameras[c][0], -1 + (x & 1) + cameras[c][1], x & 15 };
            points[count++] = (p8_point_t){ x + cameras[c][0], P8_SCREEN_HEIGHT - (x & 1) + cameras[c][1], (x + 3) & 15 };
        }
        for (int y = -2; y < P8_SCREEN_HEIGHT + 2; y++) {
            points[count++] = (p8_point_t){ -1 + (y & 1) + cameras[c][0], y + cameras[c][1], y & 15 };
            points[count++] = (p8_point_t){ P8_SCREEN_WIDTH - (y & 1) + cameras[c][0], y + cameras[c][1], (y + 5) & 15 };
        }
        assert(count == EDGE_POINTS);
        draw_points_both_ways(points, count, cameras[c][0], cameras[c][1]);
    }
}

static void test_psets_on_the_same_pixel_keep_the_last_colour_like_its_psets(void) {
    static const p8_point_t stacked[] = { { 10, 10, 3 }, { 11, 10, 4 }, { 10, 10, 9 }, { 300, 10, 2 }, { 11, 10, 12 } };
    draw_points_both_ways(stacked, 5, 0, 0);
    draw_points_both_ways(stacked, 3, 5, -2);
}

static void test_psets_with_no_point_draws_nothing(void) {
    p8_vram_t mem;
    p8_graphics_t gfx;
    p8_vram_init(&mem);
    p8_gfx_init(&gfx);
    fill_screen_pattern(&mem);
    static p8_vram_t before;
    memcpy(&before, &mem, sizeof before);

    p8_psets(&gfx, &mem, NULL, 0);

    assert(memcmp(&before, &mem, sizeof before) == 0);
}

static void test_lines_with_no_segment_draws_nothing(void) {
    p8_vram_t mem;
    p8_graphics_t gfx;
    p8_vram_init(&mem);
    p8_gfx_init(&gfx);
    fill_screen_pattern(&mem);
    static p8_vram_t before;
    memcpy(&before, &mem, sizeof before);

    p8_lines(&gfx, &mem, NULL, 0, 7);

    assert(memcmp(&before, &mem, sizeof before) == 0);
}

int main(void) {
    test_cls_clears_whole_screen_to_zero();
    test_pset_plots_pixel_at_coordinate();
    test_pset_off_screen_is_clipped_silently();
    test_camera_offset_shifts_subsequent_pset();
    test_camera_reset_restores_zero_offset();
    test_vram_init_default_transparency_is_color_0_only();
    test_palt_marks_color_transparent_and_opaque();
    test_draw_palette_write_overrides_a_previous_palt();
    test_palt_out_of_range_col_is_noop_and_queries_as_opaque();
    test_palt_affects_a_subsequent_transparent_color_draw();
    test_line_draws_diagonal_pixels();
    test_line_zero_length_draws_single_pixel();
    test_rect_draws_unfilled_outline();
    test_rect_single_pixel_draws_one_point();
    test_rectfill_fills_every_pixel_in_bounds();
    test_circ_draws_unfilled_outline();
    test_circ_zero_radius_draws_single_pixel();
    test_circfill_fills_the_disc();
    test_circfill_zero_radius_draws_single_pixel();
    test_fillp_pattern_alters_rectfill_output();
    test_sget_reads_known_pixel_from_loaded_sheet();
    test_sget_out_of_range_returns_zero();
    test_spr_default_draws_1x1_sprite_at_world_position();
    test_spr_with_explicit_width_height_draws_multi_block_sprite();
    test_spr_skips_pixels_marked_transparent_by_palt();
    test_spr_draws_source_colors_through_the_draw_palette_remap();
    test_mset_writes_base_region_cell();
    test_mset_outside_32x32_changes_no_cell();
    test_mset_out_of_range_is_noop();
    test_map_draws_cells_via_sprite_drawing_logic();
    test_map_draws_the_same_screen_as_one_spr_per_cell();
    test_map_treats_sprite_0_cells_as_empty();
    test_map_and_mset_round_trip_against_real_sheet();
    test_print_draws_known_glyph_pixels();
    test_print_advances_cursor_by_cell_width_between_chars();
    test_print_unmapped_character_draws_placeholder_block();
    test_print_space_is_blank();
    test_print_respects_camera_offset();
    test_print_digit_and_punctuation_glyphs();
    test_print_outlined_is_the_nine_prints_for_every_character();
    test_print_outlined_is_the_nine_prints_at_every_edge_and_camera();
    test_print_outlined_shows_its_outline_over_a_coloured_background();
    test_font_has_glyph_is_one_only_for_a_drawn_glyph();

    test_lines_is_its_lines_drawn_one_by_one_at_every_edge_and_camera();
    test_lines_draws_overlapping_segments_and_odd_counts_like_its_lines();
    test_lines_with_no_segment_draws_nothing();
    test_psets_is_its_psets_drawn_one_by_one_at_every_edge_and_camera();
    test_psets_on_the_same_pixel_keep_the_last_colour_like_its_psets();
    test_psets_with_no_point_draws_nothing();
    printf("test_gfx: all tests passed\n");
    return 0;
}
