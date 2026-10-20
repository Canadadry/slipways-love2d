#include <assert.h>
#include <math.h>
#include <stdbool.h>
#include <stdio.h>

#include "../tools/fake_love/canvas.h"

#define PIXEL_BYTES (CANVAS_WIDTH * CANVAS_HEIGHT * 4)

static void assert_filled(const canvas_store_t *store, int canvas, const unsigned char rgba[4]) {
    const unsigned char *pixels = canvas_read_pixels(store, canvas);
    assert(pixels != NULL);
    for (int i = 0; i < PIXEL_BYTES; i++) {
        if (pixels[i] != rgba[i % 4]) {
            fprintf(stderr, "canvas %d byte %d: expected %02X, got %02X\n", canvas, i, rgba[i % 4], pixels[i]);
        }
        assert(pixels[i] == rgba[i % 4]);
    }
}

static const unsigned char TRANSPARENT_BLACK[4] = {0, 0, 0, 0};
static const unsigned char CLEAR_TEAL[4] = {0x33, 0x66, 0x99, 0x00};

static void test_a_new_canvas_is_transparent_black_and_counts_from_zero(void) {
    canvas_store_t *store = canvas_create_store();
    assert(canvas_create(store) == 0);
    assert_filled(store, 0, TRANSPARENT_BLACK);
    canvas_free_store(store);
}

static void test_handles_follow_creation_order_and_the_seventeenth_is_refused(void) {
    canvas_store_t *store = canvas_create_store();
    for (int expected = 0; expected < CANVAS_MAX_COUNT; expected++) {
        assert(canvas_create(store) == expected);
    }
    assert(canvas_create(store) == CANVAS_NONE);
    assert(canvas_create(store) == CANVAS_NONE);
    assert_filled(store, CANVAS_MAX_COUNT - 1, TRANSPARENT_BLACK);
    canvas_free_store(store);
}

static void test_a_canvas_not_created_has_no_pixels(void) {
    canvas_store_t *store = canvas_create_store();
    assert(canvas_read_pixels(store, 0) == NULL);
    canvas_create(store);
    assert(canvas_read_pixels(store, 1) == NULL);
    assert(canvas_read_pixels(store, CANVAS_NONE) == NULL);
    canvas_free_store(store);
}

static const unsigned char RED[4] = {0xFF, 0x00, 0x00, 0xFF};
static const unsigned char BLUE[4] = {0x00, 0x00, 0xFF, 0xFF};
static const unsigned char CLEAR_GREEN[4] = {0x00, 0xFF, 0x00, 0x00};

static void test_clearing_the_screen_changes_no_canvas(void) {
    canvas_store_t *store = canvas_create_store();
    canvas_create(store);
    assert(canvas_get_target(store) == CANVAS_NONE);
    canvas_clear(store, RED);
    assert_filled(store, 0, TRANSPARENT_BLACK);
    canvas_free_store(store);
}

static void test_clearing_the_target_fills_it_and_leaves_the_other_canvas(void) {
    canvas_store_t *store = canvas_create_store();
    int first = canvas_create(store);
    int second = canvas_create(store);
    canvas_select(store, first);
    assert(canvas_get_target(store) == first);
    canvas_clear(store, BLUE);
    assert_filled(store, first, BLUE);
    assert_filled(store, second, TRANSPARENT_BLACK);

    canvas_select(store, second);
    assert(canvas_get_target(store) == second);
    canvas_clear(store, RED);
    assert_filled(store, first, BLUE);
    assert_filled(store, second, RED);

    canvas_select(store, CANVAS_NONE);
    assert(canvas_get_target(store) == CANVAS_NONE);
    canvas_clear(store, RED);
    assert_filled(store, first, BLUE);

    canvas_select(store, first);
    canvas_clear(store, CLEAR_GREEN);
    assert_filled(store, first, CLEAR_GREEN);
    assert_filled(store, second, RED);
    canvas_free_store(store);
}

static const unsigned char OPAQUE_WHITE[4] = {0xFF, 0xFF, 0xFF, 0xFF};

static void assert_pixel(const canvas_store_t *store, int canvas, int x, int y, const unsigned char rgba[4]) {
    const unsigned char *pixel = canvas_read_pixels(store, canvas) + 4 * (y * CANVAS_WIDTH + x);
    for (int channel = 0; channel < 4; channel++) {
        if (pixel[channel] != rgba[channel]) {
            fprintf(stderr, "canvas %d pixel (%d, %d) byte %d: expected %02X, got %02X\n", canvas, x, y, channel,
                    rgba[channel], pixel[channel]);
        }
        assert(pixel[channel] == rgba[channel]);
    }
}

/* Every pixel of `canvas` is `background` except (x, y), which is `rgba`. */
static void assert_one_pixel(const canvas_store_t *store, int canvas, int x, int y, const unsigned char rgba[4],
                             const unsigned char background[4]) {
    for (int row = 0; row < CANVAS_HEIGHT; row++) {
        for (int column = 0; column < CANVAS_WIDTH; column++) {
            assert_pixel(store, canvas, column, row, column == x && row == y ? rgba : background);
        }
    }
}

static void test_a_plot_lights_one_pixel_of_the_target_in_opaque_white(void) {
    canvas_store_t *store = canvas_create_store();
    int first = canvas_create(store);
    int second = canvas_create(store);
    canvas_select(store, first);
    canvas_plot(store, 37, 81);
    assert_one_pixel(store, first, 37, 81, OPAQUE_WHITE, TRANSPARENT_BLACK);
    assert_filled(store, second, TRANSPARENT_BLACK);

    canvas_plot(store, 37, 81);
    assert_one_pixel(store, first, 37, 81, OPAQUE_WHITE, TRANSPARENT_BLACK);
    canvas_free_store(store);
}

static void test_a_plot_reaches_every_corner(void) {
    canvas_store_t *store = canvas_create_store();
    canvas_select(store, canvas_create(store));
    canvas_plot(store, 0, 0);
    canvas_plot(store, CANVAS_WIDTH - 1, 0);
    canvas_plot(store, 0, CANVAS_HEIGHT - 1);
    canvas_plot(store, CANVAS_WIDTH - 1, CANVAS_HEIGHT - 1);
    assert_pixel(store, 0, 0, 0, OPAQUE_WHITE);
    assert_pixel(store, 0, CANVAS_WIDTH - 1, 0, OPAQUE_WHITE);
    assert_pixel(store, 0, 0, CANVAS_HEIGHT - 1, OPAQUE_WHITE);
    assert_pixel(store, 0, CANVAS_WIDTH - 1, CANVAS_HEIGHT - 1, OPAQUE_WHITE);
    assert_pixel(store, 0, 1, 0, TRANSPARENT_BLACK);
    canvas_free_store(store);
}

static void test_a_plot_over_a_transparent_colour_replaces_all_four_bytes(void) {
    canvas_store_t *store = canvas_create_store();
    canvas_select(store, canvas_create(store));
    canvas_clear(store, CLEAR_TEAL);
    canvas_plot(store, 10, 2);
    assert_one_pixel(store, 0, 10, 2, OPAQUE_WHITE, CLEAR_TEAL);
    canvas_free_store(store);
}

static void test_a_plot_on_the_screen_changes_no_canvas(void) {
    canvas_store_t *store = canvas_create_store();
    canvas_create(store);
    canvas_plot(store, 30, 2);
    assert_filled(store, 0, TRANSPARENT_BLACK);
    canvas_free_store(store);
}

static void assert_rectangle(const canvas_store_t *store, int canvas, int left, int top, int right, int bottom,
                             const unsigned char rgba[4], const unsigned char background[4]) {
    for (int row = 0; row < CANVAS_HEIGHT; row++) {
        for (int column = 0; column < CANVAS_WIDTH; column++) {
            bool inside = column >= left && column <= right && row >= top && row <= bottom;
            assert_pixel(store, canvas, column, row, inside ? rgba : background);
        }
    }
}

static void test_a_range_fill_covers_exactly_the_inclusive_range_of_the_target(void) {
    canvas_store_t *store = canvas_create_store();
    int first = canvas_create(store);
    int second = canvas_create(store);
    canvas_select(store, first);
    canvas_fill_range(store, 10, 7, 14, 12);
    assert_rectangle(store, first, 10, 7, 14, 12, OPAQUE_WHITE, TRANSPARENT_BLACK);
    assert_filled(store, second, TRANSPARENT_BLACK);
    canvas_free_store(store);
}

static void test_a_range_fill_reaches_the_whole_canvas_and_a_single_pixel(void) {
    canvas_store_t *store = canvas_create_store();
    canvas_select(store, canvas_create(store));
    canvas_fill_range(store, CANVAS_WIDTH - 1, CANVAS_HEIGHT - 1, CANVAS_WIDTH - 1, CANVAS_HEIGHT - 1);
    assert_one_pixel(store, 0, CANVAS_WIDTH - 1, CANVAS_HEIGHT - 1, OPAQUE_WHITE, TRANSPARENT_BLACK);
    canvas_fill_range(store, 0, 0, CANVAS_WIDTH - 1, CANVAS_HEIGHT - 1);
    assert_filled(store, 0, OPAQUE_WHITE);
    canvas_free_store(store);
}

static void test_a_range_fill_over_a_transparent_colour_replaces_all_four_bytes(void) {
    canvas_store_t *store = canvas_create_store();
    canvas_select(store, canvas_create(store));
    canvas_clear(store, CLEAR_TEAL);
    canvas_fill_range(store, 0, 30, CANVAS_WIDTH - 1, 31);
    assert_rectangle(store, 0, 0, 30, CANVAS_WIDTH - 1, 31, OPAQUE_WHITE, CLEAR_TEAL);
    canvas_free_store(store);
}

static void test_a_range_fill_on_the_screen_changes_no_canvas(void) {
    canvas_store_t *store = canvas_create_store();
    canvas_create(store);
    canvas_fill_range(store, 0, 0, CANVAS_WIDTH - 1, CANVAS_HEIGHT - 1);
    assert_filled(store, 0, TRANSPARENT_BLACK);
    canvas_free_store(store);
}

static const unsigned char PALETTE_PINK[4] = {0xFF, 0x00, 0x4D, 0xFF};
static const unsigned char PALETTE_YELLOW[4] = {0xFF, 0xEC, 0x27, 0xFF};

static void test_a_set_colour_is_what_a_plot_and_a_range_fill_write(void) {
    static const unsigned char CLEAR_PURPLE[4] = {0x80, 0x40, 0xBF, 0x00};
    canvas_store_t *store = canvas_create_store();
    canvas_select(store, canvas_create(store));
    canvas_clear(store, CLEAR_PURPLE);
    canvas_set_color(store, PALETTE_PINK);
    canvas_fill_range(store, 10, 30, 39, 49);
    assert_rectangle(store, 0, 10, 30, 39, 49, PALETTE_PINK, CLEAR_PURPLE);
    canvas_set_color(store, PALETTE_YELLOW);
    canvas_plot(store, 15, 35);
    canvas_plot(store, 70, 5);
    assert_pixel(store, 0, 15, 35, PALETTE_YELLOW);
    assert_pixel(store, 0, 70, 5, PALETTE_YELLOW);
    assert_pixel(store, 0, 16, 35, PALETTE_PINK);
    assert_pixel(store, 0, 71, 5, CLEAR_PURPLE);
    canvas_free_store(store);
}

static void test_the_colour_survives_selecting_canvases_the_screen_and_a_clear(void) {
    canvas_store_t *store = canvas_create_store();
    int first = canvas_create(store);
    int other = canvas_create(store);
    canvas_set_color(store, PALETTE_PINK);
    canvas_select(store, other);
    canvas_clear(store, RED);
    canvas_select(store, CANVAS_NONE);
    canvas_select(store, first);
    canvas_clear(store, BLUE);
    canvas_plot(store, 3, 4);
    assert_one_pixel(store, first, 3, 4, PALETTE_PINK, BLUE);
    canvas_free_store(store);
}

static void test_a_new_store_plots_opaque_white_again(void) {
    canvas_store_t *coloured = canvas_create_store();
    canvas_set_color(coloured, PALETTE_PINK);
    canvas_free_store(coloured);
    canvas_store_t *store = canvas_create_store();
    canvas_select(store, canvas_create(store));
    canvas_plot(store, 0, 0);
    assert_one_pixel(store, 0, 0, 0, OPAQUE_WHITE, TRANSPARENT_BLACK);
    canvas_free_store(store);
}

static void assert_byte(double component, int expected) {
    int got = canvas_convert_clear_component(component);
    if (got != expected) {
        fprintf(stderr, "%.17g: expected %d, got %d\n", component, expected, got);
    }
    assert(got == expected);
}

static void assert_draw_byte(double component, int expected, const char *likely_cause) {
    int got = canvas_convert_draw_component(component);
    if (got != expected) {
        fprintf(stderr, "draw %.17g: expected %d, got %d (%s)\n", component, expected, got, likely_cause);
    }
    assert(got == expected);
}

static void test_a_drawing_byte_over_255_comes_back_as_that_byte(void) {
    static const int bytes[] = {0, 1, 7, 128, 255};
    for (int i = 0; i < (int)(sizeof bytes / sizeof bytes[0]); i++) {
        assert_draw_byte(bytes[i] / 255.0, bytes[i], "byte/255 must give the byte");
    }
}

static void test_a_drawing_component_is_truncated_not_rounded(void) {
    assert_draw_byte(0.5, 127, "the drawing conversion probably rounds as clear does");
    assert(canvas_convert_clear_component(0.5) == 128);
    assert_draw_byte((2 + 0.5) / 255, 2, "the drawing conversion probably rounds");
    assert_draw_byte(0.3, 76, "the drawing conversion probably rounds");
    assert_draw_byte((5 - 0.5) / 255, 4, "the drawing conversion probably rounds");
    assert_draw_byte((5 - 1e-6) / 255, 4, "the drawing conversion probably rounds");
    assert_draw_byte((255 + 0.999) / 255, 255, "a component above 1 must clamp");
}

static void test_a_drawing_component_outside_zero_to_one_is_clamped(void) {
    assert_draw_byte(2, 255, "a component above 1 must clamp");
    assert_draw_byte(-1, 0, "a component below 0 must clamp");
}

static void test_a_drawing_component_is_multiplied_in_single_precision(void) {
    double one_float_step_below = 1 - 0x1p-30;
    assert_draw_byte(128 / 255.0 * one_float_step_below, 127, "one float step below 128/255");
    assert_draw_byte(1 / 255.0 * one_float_step_below, 0, "one float step below 1/255");
    assert_draw_byte(7 / 255.0 * one_float_step_below, 7, "the product was probably taken in double precision");
}

static void test_every_byte_over_255_comes_back_as_that_byte(void) {
    for (int k = 0; k <= 255; k++) {
        assert_byte(k / 255.0, k);
    }
}

static void test_a_tie_rounds_to_even_in_single_precision(void) {
    assert_byte(0.5 / 255, 0);
    assert_byte(1.5 / 255, 2);
    assert_byte(2.5 / 255, 2);
    assert_byte(127.5 / 255, 128);
    assert_byte(254.5 / 255, 254);
}

static void test_other_values_round_to_nearest(void) {
    assert_byte(0.1, 26);
    assert_byte(0.3, 76);
    assert_byte(0.5, 128);
}

static void test_a_value_outside_zero_to_one_is_clamped(void) {
    assert_byte(-1, 0);
    assert_byte(2, 255);
    assert_byte(-0.0, 0);
    assert_byte(1e300, 255);
    assert_byte(-1e300, 0);
}

static void assert_tint(int color, int texel, int expected, const char *likely_cause) {
    int got = canvas_tint_component((unsigned char)color, (unsigned char)texel);
    if (got != expected) {
        fprintf(stderr, "tint %d x %d: expected %d, got %d (%s)\n", color, texel, expected, got, likely_cause);
    }
    assert(got == expected);
}

static void test_a_tint_rounds_the_exact_product_over_255_as_measured(void) {
    assert_tint(1, 128, 1, "the tint probably floors the product");
    assert_tint(152, 229, 137, "the tint probably uses the blender's 8-bit multiply");
    assert_tint(253, 191, 190, "the tint probably uses the blender's 8-bit multiply");
    assert_tint(127, 37, 18, "measured in LÖVE 11.5");
    assert_tint(7, 233, 6, "measured in LÖVE 11.5");
    for (int byte = 0; byte < 256; byte++) {
        assert_tint(255, byte, byte, "white must leave the texel");
        assert_tint(byte, 255, byte, "a white texel must give the colour");
        assert_tint(0, byte, 0, "black must give black");
    }
    for (int color = 0; color < 256; color++) {
        for (int texel = 0; texel < 256; texel++) {
            assert_tint(color, texel, (int)floor(color * texel / 255.0 + 0.5), "exact rounding of c t / 255");
        }
    }
}

/* A 3x2 source: row 0 is opaque, transparent, opaque; row 1 transparent,
 * opaque, transparent, with RGB bytes the canvas never holds. */
static const unsigned char CHECKER[3 * 2 * 4] = {
    0x25, 0x69, 0x01, 0xFF, 0xAA, 0xBB, 0xCC, 0x00, 0xFF, 0xF1, 0xE8, 0xFF,
    0x11, 0x22, 0x33, 0x00, 0x1D, 0x2B, 0x53, 0xFF, 0x44, 0x55, 0x66, 0x00,
};
static const canvas_source_t CHECKER_SOURCE = {CHECKER, 3, 2};
static const canvas_area_t WHOLE_CHECKER = {0, 0, 3, 2};

/* Each opaque texel of `area` of CHECKER at its pixel from (x, y); every
 * other pixel is `background`. */
static void assert_drawn(const canvas_store_t *store, const canvas_area_t *area, int x, int y,
                         const unsigned char background[4]) {
    for (int row = 0; row < CANVAS_HEIGHT; row++) {
        for (int column = 0; column < CANVAS_WIDTH; column++) {
            int i = column - x;
            int j = row - y;
            const unsigned char *expected = background;
            if (i >= 0 && i < area->width && j >= 0 && j < area->height) {
                const unsigned char *texel = CHECKER + 4 * ((area->row + j) * 3 + area->column + i);
                if (texel[3] == 0xFF) {
                    expected = texel;
                }
            }
            assert_pixel(store, 0, column, row, expected);
        }
    }
}

static void test_a_draw_writes_opaque_texels_and_leaves_transparent_ones(void) {
    canvas_store_t *store = canvas_create_store();
    canvas_select(store, canvas_create(store));
    canvas_clear(store, CLEAR_TEAL);
    canvas_draw_texels(store, &CHECKER_SOURCE, &WHOLE_CHECKER, 50, 60);
    assert_drawn(store, &WHOLE_CHECKER, 50, 60, CLEAR_TEAL);
    assert_pixel(store, 0, 51, 60, CLEAR_TEAL);
    assert_pixel(store, 0, 50, 60, CHECKER);
    canvas_free_store(store);
}

static void test_a_draw_tints_opaque_texels_with_the_current_colour(void) {
    static const unsigned char TINT[4] = {0x7F, 0xFF, 0x3F, 0xFF};
    canvas_store_t *store = canvas_create_store();
    canvas_select(store, canvas_create(store));
    canvas_clear(store, CLEAR_TEAL);
    canvas_set_color(store, TINT);
    canvas_draw_texels(store, &CHECKER_SOURCE, &WHOLE_CHECKER, 0, 0);
    static const unsigned char first[4] = {0x12, 0x69, 0x00, 0xFF};
    static const unsigned char navy[4] = {0x0E, 0x2B, 0x15, 0xFF};
    assert_pixel(store, 0, 0, 0, first);
    assert_pixel(store, 0, 1, 0, CLEAR_TEAL);
    assert_pixel(store, 0, 1, 1, navy);
    assert_pixel(store, 0, 0, 1, CLEAR_TEAL);
    canvas_free_store(store);
}

static void test_a_draw_reads_its_area_inside_a_larger_source(void) {
    static const canvas_area_t lower_right = {1, 1, 2, 1};
    static const canvas_area_t upper_left = {0, 0, 1, 1};
    canvas_store_t *store = canvas_create_store();
    canvas_select(store, canvas_create(store));
    canvas_clear(store, CLEAR_TEAL);
    canvas_draw_texels(store, &CHECKER_SOURCE, &lower_right, 10, 20);
    assert_drawn(store, &lower_right, 10, 20, CLEAR_TEAL);
    assert_pixel(store, 0, 10, 20, CHECKER + 4 * 4);
    canvas_clear(store, CLEAR_TEAL);
    canvas_draw_texels(store, &CHECKER_SOURCE, &upper_left, CANVAS_WIDTH - 1, CANVAS_HEIGHT - 1);
    assert_drawn(store, &upper_left, CANVAS_WIDTH - 1, CANVAS_HEIGHT - 1, CLEAR_TEAL);
    canvas_free_store(store);
}

static void test_a_draw_clips_at_every_canvas_edge(void) {
    static const int places[][2] = {
        {-2, -1}, {CANVAS_WIDTH - 2, CANVAS_HEIGHT - 2}, {-3, 0}, {CANVAS_WIDTH, 0}, {0, CANVAS_HEIGHT},
        {(1 << 20) - 1, 0}, {-((1 << 20) - 1), 0}, {0, (1 << 20) - 1}, {0, -((1 << 20) - 1)},
    };
    canvas_store_t *store = canvas_create_store();
    canvas_select(store, canvas_create(store));
    for (int k = 0; k < (int)(sizeof places / sizeof places[0]); k++) {
        canvas_clear(store, CLEAR_TEAL);
        canvas_draw_texels(store, &CHECKER_SOURCE, &WHOLE_CHECKER, places[k][0], places[k][1]);
        assert_drawn(store, &WHOLE_CHECKER, places[k][0], places[k][1], CLEAR_TEAL);
    }
    canvas_clear(store, CLEAR_TEAL);
    canvas_draw_texels(store, &CHECKER_SOURCE, &WHOLE_CHECKER, -1, -1);
    assert_pixel(store, 0, 0, 0, CHECKER + 4 * 4);
    canvas_draw_texels(store, &CHECKER_SOURCE, &WHOLE_CHECKER, CANVAS_WIDTH - 2, CANVAS_HEIGHT - 1);
    assert_pixel(store, 0, CANVAS_WIDTH - 2, CANVAS_HEIGHT - 1, CHECKER);
    canvas_free_store(store);
}

static void test_a_draw_on_the_screen_changes_no_canvas(void) {
    canvas_store_t *store = canvas_create_store();
    canvas_create(store);
    canvas_draw_texels(store, &CHECKER_SOURCE, &WHOLE_CHECKER, 0, 0);
    assert_filled(store, 0, TRANSPARENT_BLACK);
    canvas_free_store(store);
}

int main(void) {
    test_a_new_canvas_is_transparent_black_and_counts_from_zero();
    test_handles_follow_creation_order_and_the_seventeenth_is_refused();
    test_a_canvas_not_created_has_no_pixels();
    test_clearing_the_screen_changes_no_canvas();
    test_clearing_the_target_fills_it_and_leaves_the_other_canvas();
    test_a_plot_lights_one_pixel_of_the_target_in_opaque_white();
    test_a_plot_reaches_every_corner();
    test_a_plot_over_a_transparent_colour_replaces_all_four_bytes();
    test_a_plot_on_the_screen_changes_no_canvas();
    test_a_range_fill_covers_exactly_the_inclusive_range_of_the_target();
    test_a_range_fill_reaches_the_whole_canvas_and_a_single_pixel();
    test_a_range_fill_over_a_transparent_colour_replaces_all_four_bytes();
    test_a_range_fill_on_the_screen_changes_no_canvas();
    test_every_byte_over_255_comes_back_as_that_byte();
    test_a_tie_rounds_to_even_in_single_precision();
    test_other_values_round_to_nearest();
    test_a_value_outside_zero_to_one_is_clamped();
    test_a_set_colour_is_what_a_plot_and_a_range_fill_write();
    test_the_colour_survives_selecting_canvases_the_screen_and_a_clear();
    test_a_new_store_plots_opaque_white_again();
    test_a_drawing_byte_over_255_comes_back_as_that_byte();
    test_a_drawing_component_is_truncated_not_rounded();
    test_a_drawing_component_outside_zero_to_one_is_clamped();
    test_a_drawing_component_is_multiplied_in_single_precision();
    test_a_tint_rounds_the_exact_product_over_255_as_measured();
    test_a_draw_writes_opaque_texels_and_leaves_transparent_ones();
    test_a_draw_tints_opaque_texels_with_the_current_colour();
    test_a_draw_reads_its_area_inside_a_larger_source();
    test_a_draw_clips_at_every_canvas_edge();
    test_a_draw_on_the_screen_changes_no_canvas();
    puts("test_fake_love_canvas: ok");
    return 0;
}
