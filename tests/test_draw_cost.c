#include "p8/draw_cost.h"
#include "p8/gfx.h"

#include <assert.h>
#include <stdio.h>

static p8_graphics_t gfx;
static p8_draw_cost_window_t window;

static void reset(void) {
    p8_gfx_init(&gfx);
    (void)p8_draw_cost_frame_begin(&window);
}

static void assert_cost(p8_draw_cost_t cost, unsigned long bytes, int primitives) {
    assert(cost.bytes == bytes);
    assert(cost.primitives == primitives);
}

static void test_cls_is_one_fill(void) {
    reset();
    assert_cost(p8_draw_cost_cls(), P8_GPU_FILL_BYTES, 1);
}

static void test_pset_is_one_tile_1_and_off_screen_is_free(void) {
    reset();
    assert_cost(p8_draw_cost_pset(&gfx, 0, 0), P8_GPU_TILE_1_BYTES, 1);
    assert_cost(p8_draw_cost_pset(&gfx, 159, 119), P8_GPU_TILE_1_BYTES, 1);
    assert_cost(p8_draw_cost_pset(&gfx, 160, 0), 0, 0);
    assert_cost(p8_draw_cost_pset(&gfx, 0, -1), 0, 0);
}


static void test_culling_is_judged_after_the_camera(void) {
    reset();
    p8_camera(&gfx, 100, 50);
    assert_cost(p8_draw_cost_pset(&gfx, 10, 10), 0, 0);
    assert_cost(p8_draw_cost_pset(&gfx, 100, 50), P8_GPU_TILE_1_BYTES, 1);
    assert_cost(p8_draw_cost_pset(&gfx, 259, 169), P8_GPU_TILE_1_BYTES, 1);
    assert_cost(p8_draw_cost_pset(&gfx, 260, 169), 0, 0);
}

static void test_line_is_one_line_f2_culled_on_its_bounding_box(void) {
    reset();
    assert_cost(p8_draw_cost_line(&gfx, -10, -10, 5, 200), P8_GPU_LINE_F2_BYTES, 1);
    assert_cost(p8_draw_cost_line(&gfx, 170, -10, 160, 50), 0, 0);
}

static void test_rect_is_four_tiles_and_straddling_costs_in_full(void) {
    reset();
    assert_cost(p8_draw_cost_rect(&gfx, 150, 110, 170, 130), 4 * P8_GPU_TILE_BYTES, 4);
    assert_cost(p8_draw_cost_rect(&gfx, -1, -5, -20, 30), 0, 0);
}


static void test_plain_rectfill_is_one_tile(void) {
    reset();
    assert_cost(p8_draw_cost_rectfill(&window, &gfx, 0, 0, 159, 119, 7), P8_GPU_TILE_BYTES, 1);
    assert_cost(p8_draw_cost_rectfill(&window, &gfx, 0, 120, 10, 130, 7), 0, 0);
}

static void test_circles_cost_per_midpoint_iteration(void) {
    static const int RADII[] = {0, 1, 7, 9};
    static const int ITERATIONS[] = {1, 1, 5, 6};
    for (int i = 0; i < 4; i++) {
        reset();
        int n = ITERATIONS[i];
        assert_cost(p8_draw_cost_circ(&gfx, 80, 60, RADII[i]), (unsigned long)(8 * n) * P8_GPU_TILE_1_BYTES, 8 * n);
        assert_cost(p8_draw_cost_circfill(&window, &gfx, 80, 60, RADII[i], 7), (unsigned long)(4 * n) * P8_GPU_TILE_BYTES, 4 * n);
    }
    reset();
    assert_cost(p8_draw_cost_circ(&gfx, 80, 60, -1), 0, 0);
    assert_cost(p8_draw_cost_circfill(&window, &gfx, 80, 60, -1, 7), 0, 0);
    assert_cost(p8_draw_cost_circ(&gfx, -9, 60, 9), 8 * 6 * P8_GPU_TILE_1_BYTES, 8 * 6);
    assert_cost(p8_draw_cost_circ(&gfx, -10, 60, 9), 0, 0);
}

static void test_spr_is_one_sprt_whatever_its_cells(void) {
    reset();
    assert_cost(p8_draw_cost_spr(&window, &gfx, 3, 10, 10, 1, 1), P8_GPU_SPRT_BYTES, 1);
    assert_cost(p8_draw_cost_spr(&window, &gfx, 3, 10, 10, 4, 2), P8_GPU_SPRT_BYTES, 1);
    assert_cost(p8_draw_cost_spr(&window, &gfx, 3, -8, 10, 1, 1), 0, 0);
    assert_cost(p8_draw_cost_spr(&window, &gfx, 3, -16, 10, 3, 1), P8_GPU_SPRT_BYTES, 1);
    assert_cost(p8_draw_cost_spr(&window, &gfx, 3, 10, 10, 0, 1), 0, 0);
}

static void test_print_is_one_sprt_per_glyph_but_spaces(void) {
    reset();
    assert_cost(p8_draw_cost_print(&window, &gfx, "a b!", 0, 0), 3 * P8_GPU_SPRT_BYTES, 3);
    assert_cost(p8_draw_cost_print(&window, &gfx, "   ", 0, 0), 0, 0);
    assert_cost(p8_draw_cost_print(&window, &gfx, "", 0, 0), 0, 0);
    assert_cost(p8_draw_cost_print(&window, &gfx, "ab", -6, 0), 2 * P8_GPU_SPRT_BYTES, 2);
    assert_cost(p8_draw_cost_print(&window, &gfx, "ab", -7, 0), 0, 0);
    assert_cost(p8_draw_cost_print(&window, &gfx, "ab", 0, -5), 0, 0);
}

enum { WINDOW_PAIR_BYTES = P8_GPU_DR_TPAGE_BYTES + P8_GPU_DR_TWIN_BYTES };

static void test_print_outlined_is_one_sprt_per_glyph_but_spaces_over_its_outlined_box(void) {
    reset();
    assert_cost(p8_draw_cost_print_outlined(&window, &gfx, "a b!", 0, 0), 3 * P8_GPU_SPRT_BYTES, 3);
    assert_cost(p8_draw_cost_print_outlined(&window, &gfx, "   ", 0, 0), 0, 0);
    assert_cost(p8_draw_cost_print_outlined(&window, &gfx, "", 0, 0), 0, 0);
    assert_cost(p8_draw_cost_print_outlined(&window, &gfx, "ab", -7, 0), 2 * P8_GPU_SPRT_BYTES, 2);
    assert_cost(p8_draw_cost_print_outlined(&window, &gfx, "ab", -8, 0), 0, 0);
    assert_cost(p8_draw_cost_print_outlined(&window, &gfx, "ab", 0, -5), 2 * P8_GPU_SPRT_BYTES, 2);
    assert_cost(p8_draw_cost_print_outlined(&window, &gfx, "ab", 0, -6), 0, 0);
    assert_cost(p8_draw_cost_print_outlined(&window, &gfx, "ab", P8_SCREEN_WIDTH, 0), 2 * P8_GPU_SPRT_BYTES, 2);
    assert_cost(p8_draw_cost_print_outlined(&window, &gfx, "ab", P8_SCREEN_WIDTH + 1, 0), 0, 0);
    assert_cost(p8_draw_cost_print_outlined(&window, &gfx, "ab", 0, P8_SCREEN_HEIGHT), 2 * P8_GPU_SPRT_BYTES, 2);
    assert_cost(p8_draw_cost_print_outlined(&window, &gfx, "ab", 0, P8_SCREEN_HEIGHT + 1), 0, 0);
    p8_camera(&gfx, -8, 0);
    assert_cost(p8_draw_cost_print_outlined(&window, &gfx, "ab", -15, 0), 2 * P8_GPU_SPRT_BYTES, 2);
    assert_cost(p8_draw_cost_print_outlined(&window, &gfx, "ab", -16, 0), 0, 0);
}

static void test_print_outlined_resets_the_window_after_a_pattern(void) {
    reset();
    p8_fillp(&gfx, 0xA5A5);
    assert_cost(p8_draw_cost_rectfill(&window, &gfx, 0, 0, 10, 10, 1), P8_GPU_SPRT_BYTES + WINDOW_PAIR_BYTES, 3);
    assert_cost(p8_draw_cost_print_outlined(&window, &gfx, "a", 0, 0), P8_GPU_SPRT_BYTES + WINDOW_PAIR_BYTES, 3);
    assert_cost(p8_draw_cost_print_outlined(&window, &gfx, "a", 0, 0), P8_GPU_SPRT_BYTES, 1);
}

static void test_patterned_fill_is_a_sprt_with_its_window_pair_once(void) {
    reset();
    p8_fillp(&gfx, 0xA5A5);
    assert_cost(p8_draw_cost_rectfill(&window, &gfx, 0, 0, 10, 10, 0x15), P8_GPU_SPRT_BYTES + WINDOW_PAIR_BYTES, 3);
    assert_cost(p8_draw_cost_rectfill(&window, &gfx, 20, 0, 30, 10, 0x15), P8_GPU_SPRT_BYTES, 1);
    assert_cost(p8_draw_cost_circfill(&window, &gfx, 80, 60, 7, 0x15), 4 * 5 * P8_GPU_SPRT_BYTES, 4 * 5);
    assert_cost(p8_draw_cost_rectfill(&window, &gfx, 0, 0, 10, 10, 0x16), P8_GPU_SPRT_BYTES + WINDOW_PAIR_BYTES, 3);
    p8_fillp_transparent(&gfx, 0xA5A5);
    assert_cost(p8_draw_cost_rectfill(&window, &gfx, 0, 0, 10, 10, 0x16), P8_GPU_SPRT_BYTES + WINDOW_PAIR_BYTES, 3);
    p8_fillp(&gfx, 0x0F0F);
    assert_cost(p8_draw_cost_circfill(&window, &gfx, 80, 60, 0, 0x16), 4 * P8_GPU_SPRT_BYTES + WINDOW_PAIR_BYTES, 4 + 2);
}

static void test_spr_and_print_reset_the_window_after_a_pattern(void) {
    reset();
    assert_cost(p8_draw_cost_spr(&window, &gfx, 1, 0, 0, 1, 1), P8_GPU_SPRT_BYTES, 1);
    p8_fillp(&gfx, 0xA5A5);
    assert_cost(p8_draw_cost_rectfill(&window, &gfx, 0, 0, 10, 10, 1), P8_GPU_SPRT_BYTES + WINDOW_PAIR_BYTES, 3);
    assert_cost(p8_draw_cost_print(&window, &gfx, "a", 0, 0), P8_GPU_SPRT_BYTES + WINDOW_PAIR_BYTES, 3);
    assert_cost(p8_draw_cost_spr(&window, &gfx, 1, 0, 0, 1, 1), P8_GPU_SPRT_BYTES, 1);
    assert_cost(p8_draw_cost_rectfill(&window, &gfx, 0, 0, 10, 10, 1), P8_GPU_SPRT_BYTES + WINDOW_PAIR_BYTES, 3);
    assert_cost(p8_draw_cost_spr(&window, &gfx, 1, 0, 0, 1, 1), P8_GPU_SPRT_BYTES + WINDOW_PAIR_BYTES, 3);
}

static void test_untextured_and_culled_calls_keep_the_window(void) {
    reset();
    p8_fillp(&gfx, 0xA5A5);
    assert_cost(p8_draw_cost_rectfill(&window, &gfx, 0, 0, 10, 10, 1), P8_GPU_SPRT_BYTES + WINDOW_PAIR_BYTES, 3);
    assert_cost(p8_draw_cost_line(&gfx, 0, 0, 5, 5), P8_GPU_LINE_F2_BYTES, 1);
    assert_cost(p8_draw_cost_spr(&window, &gfx, 1, -50, 0, 1, 1), 0, 0);
    assert_cost(p8_draw_cost_rectfill(&window, &gfx, 0, 0, 10, 10, 1), P8_GPU_SPRT_BYTES, 1);
}

static void test_frame_begin_returns_the_overhead_and_resets_the_window(void) {
    reset();
    p8_fillp(&gfx, 0xA5A5);
    assert_cost(p8_draw_cost_rectfill(&window, &gfx, 0, 0, 10, 10, 1), P8_GPU_SPRT_BYTES + WINDOW_PAIR_BYTES, 3);
    assert_cost(p8_draw_cost_frame_begin(&window),
                2 * P8_GPU_DR_ENV_BYTES + P8_GPU_DR_TPAGE_BYTES + P8_GPU_DR_TWIN_BYTES + P8_GPU_POLY_FT4_BYTES, 5);
    assert_cost(p8_draw_cost_rectfill(&window, &gfx, 0, 0, 10, 10, 1), P8_GPU_SPRT_BYTES + WINDOW_PAIR_BYTES, 3);
}

static void test_pattern_key_tells_pattern_colour_and_transparency_apart(void) {
    reset();
    p8_fillp(&gfx, 0xA5A5);
    unsigned long key = p8_draw_cost_pattern_key(&gfx, 0x15);
    assert(p8_draw_cost_pattern_key(&gfx, 0x115) == key && "only the colour byte counts");
    assert(p8_draw_cost_pattern_key(&gfx, 0x16) != key);
    p8_fillp_transparent(&gfx, 0xA5A5);
    assert(p8_draw_cost_pattern_key(&gfx, 0x15) != key);
    p8_fillp(&gfx, 0x0F0F);
    assert(p8_draw_cost_pattern_key(&gfx, 0x15) != key);
}

static void test_an_image_copy_is_its_page_switches_and_one_sprt_and_off_screen_is_free(void) {
    reset();
    unsigned long copy = P8_GPU_DR_TWIN_BYTES + 2 * P8_GPU_DR_TPAGE_BYTES + P8_GPU_SPRT_BYTES;
    assert_cost(p8_draw_cost_image_draw(&window, &gfx, 0, 0, 256, 256), copy, 4);
    assert_cost(p8_draw_cost_image_draw(&window, &gfx, -255, -255, 256, 256), copy, 4);
    assert_cost(p8_draw_cost_image_draw(&window, &gfx, -256, 0, 256, 256), 0, 0);
    assert_cost(p8_draw_cost_image_draw(&window, &gfx, 160, 0, 256, 256), 0, 0);
    p8_camera(&gfx, 300, 300);
    assert_cost(p8_draw_cost_image_draw(&window, &gfx, 0, 0, 256, 256), 0, 0);
    assert_cost(p8_draw_cost_image_draw(&window, &gfx, 0, 0, 301, 301), copy, 4);
}

static void test_an_image_copy_leaves_no_texture_window_and_a_culled_one_keeps_it(void) {
    reset();
    p8_fillp(&gfx, 0xA5A5);
    assert_cost(p8_draw_cost_rectfill(&window, &gfx, 0, 0, 10, 10, 1), P8_GPU_SPRT_BYTES + WINDOW_PAIR_BYTES, 3);
    (void)p8_draw_cost_image_draw(&window, &gfx, 500, 0, 16, 16);
    assert_cost(p8_draw_cost_rectfill(&window, &gfx, 0, 0, 10, 10, 1), P8_GPU_SPRT_BYTES, 1);
    (void)p8_draw_cost_image_draw(&window, &gfx, 0, 0, 16, 16);
    assert_cost(p8_draw_cost_spr(&window, &gfx, 1, 0, 0, 1, 1), P8_GPU_SPRT_BYTES, 1);
    (void)p8_draw_cost_image_draw(&window, &gfx, 0, 0, 16, 16);
    assert_cost(p8_draw_cost_rectfill(&window, &gfx, 0, 0, 10, 10, 1), P8_GPU_SPRT_BYTES + WINDOW_PAIR_BYTES, 3);
}

static void test_a_build_costs_its_clear_and_mask_its_window_switches_and_its_restore(void) {
    assert_cost(p8_draw_cost_image_begin(), P8_GPU_FILL_BYTES + P8_GPU_DR_STP_BYTES, 2);
    assert_cost(p8_draw_cost_image_window(), P8_GPU_DR_AREA_BYTES + P8_GPU_DR_OFFSET_BYTES, 2);
    assert_cost(p8_draw_cost_image_end(),
                P8_GPU_DR_AREA_BYTES + P8_GPU_DR_OFFSET_BYTES + P8_GPU_DR_STP_BYTES + P8_GPU_COMMAND_BYTES, 4);
}

int main(void) {
    test_cls_is_one_fill();
    test_pset_is_one_tile_1_and_off_screen_is_free();
    test_culling_is_judged_after_the_camera();
    test_line_is_one_line_f2_culled_on_its_bounding_box();
    test_rect_is_four_tiles_and_straddling_costs_in_full();
    test_plain_rectfill_is_one_tile();
    test_circles_cost_per_midpoint_iteration();
    test_spr_is_one_sprt_whatever_its_cells();
    test_print_is_one_sprt_per_glyph_but_spaces();
    test_patterned_fill_is_a_sprt_with_its_window_pair_once();
    test_spr_and_print_reset_the_window_after_a_pattern();
    test_print_outlined_is_one_sprt_per_glyph_but_spaces_over_its_outlined_box();
    test_print_outlined_resets_the_window_after_a_pattern();
    test_untextured_and_culled_calls_keep_the_window();
    test_frame_begin_returns_the_overhead_and_resets_the_window();
    test_pattern_key_tells_pattern_colour_and_transparency_apart();
    test_an_image_copy_is_its_page_switches_and_one_sprt_and_off_screen_is_free();
    test_an_image_copy_leaves_no_texture_window_and_a_culled_one_keeps_it();
    test_a_build_costs_its_clear_and_mask_its_window_switches_and_its_restore();
    printf("test_draw_cost: ok\n");
    return 0;
}
