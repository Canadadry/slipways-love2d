#include <assert.h>
#include <math.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "../tools/fake_love/image_data.h"

static void test_every_byte_over_255_converts_to_that_byte(void) {
    for (int byte = 0; byte < 256; byte++) {
        unsigned char converted = image_data_convert_component(byte / 255.0);
        if (converted != byte) {
            fprintf(stderr, "%d/255: expected %d, got %d\n", byte, byte, converted);
        }
        assert(converted == byte);
    }
}

static void assert_converts(double component, int expected, const char *suspect) {
    unsigned char converted = image_data_convert_component(component);
    if (converted != expected) {
        fprintf(stderr, "%.17g: expected %d, got %d; %s\n", component, expected, converted, suspect);
    }
    assert(converted == expected);
}

static const char *const MEASURED = "measured in LÖVE 11.5";
static const char *const TIES_TO_EVEN = "the conversion probably rounds ties to even, as clear";
static const char *const SINGLE = "the conversion probably runs in single precision";
static const char *const TRUNCATES = "the conversion probably truncates, as setColor (127)";

static void test_components_convert_half_up_in_double_as_measured(void) {
    assert_converts(0.5, 128, TRUNCATES);
    assert_converts(2.5 / 255, 3, TIES_TO_EVEN);
    assert_converts(0.3, 77, MEASURED);
    assert_converts(1.5 / 255, 2, MEASURED);
    assert_converts(0.5 / 255, 1, TIES_TO_EVEN);
    assert_converts(4.5 / 255, 5, TIES_TO_EVEN);
    assert_converts(2.49 / 255, 2, MEASURED);
    assert_converts(2.51 / 255, 3, MEASURED);
    assert_converts((5 - 1e-6) / 255, 5, TRUNCATES);
    assert_converts((0.5 - 1e-9) / 255, 0, SINGLE);
    assert_converts((1.5 - 1e-9) / 255, 1, SINGLE);
}

static void test_components_outside_zero_to_one_are_clamped(void) {
    assert_converts(2, 255, MEASURED);
    assert_converts(-1, 0, MEASURED);
    assert_converts(1e30, 255, MEASURED);
    assert_converts(-1e30, 0, MEASURED);
    assert_converts(-0.0, 0, MEASURED);
}

static void test_a_byte_reads_back_as_the_double_quotient_and_converts_back(void) {
    for (int byte = 0; byte < 256; byte++) {
        double value = image_data_read_component((unsigned char)byte);
        if (value != byte / 255.0) {
            fprintf(stderr, "byte %d: expected %.17g, got %.17g; a float quotient would be %.17g\n", byte, byte / 255.0, value, (double)((float)byte / 255.0f));
        }
        assert(value == byte / 255.0);
        assert(image_data_convert_component(value) == byte);
    }
}

static void test_a_side_is_an_integer_from_1_to_the_maximum(void) {
    assert(image_data_accepts_side(1));
    assert(image_data_accepts_side(128));
    assert(image_data_accepts_side(IMAGE_DATA_MAX_SIDE));
    assert(IMAGE_DATA_MAX_SIDE == 1024);
    assert(!image_data_accepts_side(0));
    assert(!image_data_accepts_side(-1));
    assert(!image_data_accepts_side(1025));
    assert(!image_data_accepts_side(1.5));
    assert(!image_data_accepts_side(NAN));
    assert(!image_data_accepts_side(INFINITY));
}

/* Filled with garbage first, as a fresh Lua userdata may be. */
static image_data_t *create_data(int width, int height) {
    size_t size = image_data_byte_count(width, height);
    image_data_t *data = malloc(size);
    assert(data != NULL);
    memset(data, 0xAB, size);
    image_data_init(data, width, height);
    return data;
}

static void assert_pixel(const image_data_t *data, int column, int row, const unsigned char rgba[4]) {
    unsigned char read[4];
    image_data_read_pixel(data, column, row, read);
    if (memcmp(read, rgba, 4) != 0) {
        fprintf(stderr, "pixel (%d, %d): expected %02X%02X%02X%02X, got %02X%02X%02X%02X\n", column, row, rgba[0], rgba[1], rgba[2], rgba[3], read[0], read[1], read[2], read[3]);
    }
    assert(memcmp(read, rgba, 4) == 0);
}

static const unsigned char TRANSPARENT_BLACK[4] = {0, 0, 0, 0};

static void assert_all(const image_data_t *data, const unsigned char rgba[4]) {
    for (int row = 0; row < data->height; row++) {
        for (int column = 0; column < data->width; column++) {
            assert_pixel(data, column, row, rgba);
        }
    }
}

static void test_a_new_buffer_has_its_size_and_is_transparent_black(void) {
    image_data_t *data = create_data(4, 3);
    assert(data->width == 4 && data->height == 3);
    assert_all(data, TRANSPARENT_BLACK);
    free(data);

    data = create_data(IMAGE_DATA_MAX_SIDE, IMAGE_DATA_MAX_SIDE);
    assert_pixel(data, IMAGE_DATA_MAX_SIDE - 1, IMAGE_DATA_MAX_SIDE - 1, TRANSPARENT_BLACK);
    free(data);
}

static void test_a_write_replaces_one_pixel_of_one_buffer(void) {
    image_data_t *first = create_data(4, 3);
    image_data_t *second = create_data(4, 3);
    const unsigned char white[4] = {0xFF, 0xFF, 0xFF, 0xFF};
    const unsigned char translucent[4] = {0x1A, 0x33, 0x4D, 0x80};
    const unsigned char invisible_blue[4] = {0x33, 0x66, 0x99, 0x00};

    image_data_write_pixel(first, 3, 2, white);
    image_data_write_pixel(first, 3, 2, translucent);
    image_data_write_pixel(first, 0, 1, invisible_blue);
    assert_pixel(first, 3, 2, translucent);
    assert_pixel(first, 0, 1, invisible_blue);
    for (int row = 0; row < 3; row++) {
        for (int column = 0; column < 4; column++) {
            if (!(column == 3 && row == 2) && !(column == 0 && row == 1)) {
                assert_pixel(first, column, row, TRANSPARENT_BLACK);
            }
        }
    }
    assert_all(second, TRANSPARENT_BLACK);
    free(first);
    free(second);
}

static void assert_finds(const image_data_t *data, double x, double y, int column, int row) {
    int found_column = -1;
    int found_row = -1;
    bool inside = image_data_find_pixel(data, x, y, &found_column, &found_row);
    if (!inside || found_column != column || found_row != row) {
        fprintf(stderr, "(%.17g, %.17g): expected (%d, %d), got %s (%d, %d)\n", x, y, column, row, inside ? "inside" : "outside", found_column, found_row);
    }
    assert(inside && found_column == column && found_row == row);
}

static void assert_outside(const image_data_t *data, double x, double y) {
    int column = -1;
    int row = -1;
    bool inside = image_data_find_pixel(data, x, y, &column, &row);
    if (inside) {
        fprintf(stderr, "(%.17g, %.17g): expected outside, got (%d, %d); a truncation or an integer wrap probably\n", x, y, column, row);
    }
    assert(!inside);
}

static void test_coordinates_are_floored_and_outside_pixels_are_refused(void) {
    image_data_t *data = create_data(4, 3);
    assert_finds(data, 0.99, 0, 0, 0);
    assert_finds(data, 3.9, 2.999, 3, 2);
    assert_finds(data, 4 - 1e-9, 0, 3, 0);
    assert_finds(data, -0.0, -0.0, 0, 0);
    assert_finds(data, 2.9, 1.99, 2, 1);

    const double outside[] = {-1, -0.5, -1e-300, NAN, INFINITY, -INFINITY, 2147483648.0, 4294967299.0, -4294967293.0, 1e300};
    for (size_t i = 0; i < sizeof outside / sizeof outside[0]; i++) {
        assert_outside(data, outside[i], 0);
        assert_outside(data, 0, outside[i]);
    }
    assert_outside(data, 4, 0);
    assert_outside(data, 0, 3);
    free(data);
}

int main(void) {
    test_every_byte_over_255_converts_to_that_byte();
    test_components_convert_half_up_in_double_as_measured();
    test_components_outside_zero_to_one_are_clamped();
    test_a_byte_reads_back_as_the_double_quotient_and_converts_back();
    test_a_side_is_an_integer_from_1_to_the_maximum();
    test_a_new_buffer_has_its_size_and_is_transparent_black();
    test_a_write_replaces_one_pixel_of_one_buffer();
    test_coordinates_are_floored_and_outside_pixels_are_refused();
    puts("test_fake_love_image_data: ok");
    return 0;
}
