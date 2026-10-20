#include <assert.h>
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
    static const unsigned char CLEAR_TEAL[4] = {0x33, 0x66, 0x99, 0x00};
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
    static const unsigned char CLEAR_TEAL[4] = {0x33, 0x66, 0x99, 0x00};
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

static void assert_byte(double component, int expected) {
    int got = canvas_convert_component(component);
    if (got != expected) {
        fprintf(stderr, "%.17g: expected %d, got %d\n", component, expected, got);
    }
    assert(got == expected);
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
    puts("test_fake_love_canvas: ok");
    return 0;
}
