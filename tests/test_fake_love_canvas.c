#include <assert.h>
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
    test_every_byte_over_255_comes_back_as_that_byte();
    test_a_tie_rounds_to_even_in_single_precision();
    test_other_values_round_to_nearest();
    test_a_value_outside_zero_to_one_is_clamped();
    puts("test_fake_love_canvas: ok");
    return 0;
}
