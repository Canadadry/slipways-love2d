#include <assert.h>
#include <math.h>
#include <stdio.h>

#include "../tools/fake_love/image_rules.h"

static const char *const VERDICT_NAMES[] = {"accepted", "not finite", "not an integer", "out of range"};

static void assert_position(double position, image_rules_verdict_t expected) {
    image_rules_verdict_t got = image_rules_check_position(position);
    if (got != expected) {
        fprintf(stderr, "position %.17g: expected %s, got %s\n", position, VERDICT_NAMES[expected], VERDICT_NAMES[got]);
    }
    assert(got == expected);
}

static void test_a_position_is_an_integer_of_magnitude_below_two_to_the_twenty(void) {
    assert_position(0, IMAGE_RULES_ACCEPTED);
    assert_position(-0.0, IMAGE_RULES_ACCEPTED);
    assert_position(37, IMAGE_RULES_ACCEPTED);
    assert_position(0x1p20 - 1, IMAGE_RULES_ACCEPTED);
    assert_position(-(0x1p20 - 1), IMAGE_RULES_ACCEPTED);
    assert_position(0.5, IMAGE_RULES_NOT_INTEGER);
    assert_position(1.0 / 1024, IMAGE_RULES_NOT_INTEGER);
    assert_position(-2.5, IMAGE_RULES_NOT_INTEGER);
    assert_position(0x1p20, IMAGE_RULES_OUT_OF_RANGE);
    assert_position(-0x1p20, IMAGE_RULES_OUT_OF_RANGE);
    assert_position(1e300, IMAGE_RULES_OUT_OF_RANGE);
    assert_position(NAN, IMAGE_RULES_NOT_FINITE);
    assert_position(INFINITY, IMAGE_RULES_NOT_FINITE);
    assert_position(-INFINITY, IMAGE_RULES_NOT_FINITE);
}

static void assert_scale(double scale, image_rules_verdict_t expected) {
    image_rules_verdict_t got = image_rules_check_scale(scale);
    if (got != expected) {
        fprintf(stderr, "scale %.17g: expected %s, got %s\n", scale, VERDICT_NAMES[expected], VERDICT_NAMES[got]);
    }
    assert(got == expected);
}

static void test_a_scale_is_an_integer_from_one_to_sixteen(void) {
    assert_scale(1, IMAGE_RULES_ACCEPTED);
    assert_scale(4, IMAGE_RULES_ACCEPTED);
    assert_scale(16, IMAGE_RULES_ACCEPTED);
    assert_scale(0, IMAGE_RULES_OUT_OF_RANGE);
    assert_scale(-0.0, IMAGE_RULES_OUT_OF_RANGE);
    assert_scale(-4, IMAGE_RULES_OUT_OF_RANGE);
    assert_scale(17, IMAGE_RULES_OUT_OF_RANGE);
    assert_scale(1e300, IMAGE_RULES_OUT_OF_RANGE);
    assert_scale(2.5, IMAGE_RULES_NOT_INTEGER);
    assert_scale(4 + 1.0 / 1024, IMAGE_RULES_NOT_INTEGER);
    assert_scale(NAN, IMAGE_RULES_NOT_FINITE);
    assert_scale(INFINITY, IMAGE_RULES_NOT_FINITE);
}

static void assert_quad(double x, double y, double width, double height, double reference_width,
                        double reference_height, image_rules_verdict_t expected, int expected_index) {
    const double values[IMAGE_RULES_QUAD_VALUES] = {x, y, width, height, reference_width, reference_height};
    int index = -1;
    image_rules_verdict_t got = image_rules_check_quad(values, &index);
    if (got != expected || index != expected_index) {
        fprintf(stderr, "quad (%g, %g, %g, %g, %g, %g): expected %s at %d, got %s at %d\n", x, y, width, height,
                reference_width, reference_height, VERDICT_NAMES[expected], expected_index, VERDICT_NAMES[got], index);
    }
    assert(got == expected && index == expected_index);
}

static void test_a_quad_is_a_whole_number_of_texels_inside_its_reference_size(void) {
    assert_quad(0, 0, 1, 1, 1, 1, IMAGE_RULES_ACCEPTED, -1);
    assert_quad(0, 0, 1024, 1024, 1024, 1024, IMAGE_RULES_ACCEPTED, -1);
    assert_quad(45, 25, 3, 5, 48, 30, IMAGE_RULES_ACCEPTED, -1);
    assert_quad(-0.0, 0, 3, 5, 3, 5, IMAGE_RULES_ACCEPTED, -1);
    assert_quad(46, 25, 3, 5, 48, 30, IMAGE_RULES_OUT_OF_RANGE, 2);
    assert_quad(45, 26, 3, 5, 48, 30, IMAGE_RULES_OUT_OF_RANGE, 3);
    assert_quad(48, 0, 1, 1, 48, 30, IMAGE_RULES_OUT_OF_RANGE, 0);
    assert_quad(0, 30, 1, 1, 48, 30, IMAGE_RULES_OUT_OF_RANGE, 1);
}

static void test_a_quad_refuses_empty_negative_and_outsized_values(void) {
    assert_quad(0, 0, 0, 2, 4, 4, IMAGE_RULES_OUT_OF_RANGE, 2);
    assert_quad(0, 0, -1, 2, 4, 4, IMAGE_RULES_OUT_OF_RANGE, 2);
    assert_quad(0, 0, 2, 0, 4, 4, IMAGE_RULES_OUT_OF_RANGE, 3);
    assert_quad(-1, 0, 2, 2, 4, 4, IMAGE_RULES_OUT_OF_RANGE, 0);
    assert_quad(0, -1, 2, 2, 4, 4, IMAGE_RULES_OUT_OF_RANGE, 1);
    assert_quad(0, 0, 1, 1, 0, 4, IMAGE_RULES_OUT_OF_RANGE, 4);
    assert_quad(0, 0, 1, 1, 4, 0, IMAGE_RULES_OUT_OF_RANGE, 5);
    assert_quad(0, 0, 1, 1, 1025, 4, IMAGE_RULES_OUT_OF_RANGE, 4);
    assert_quad(0, 0, 1, 1, 4, 1025, IMAGE_RULES_OUT_OF_RANGE, 5);
    assert_quad(0, 0, 1e300, 1, 4, 4, IMAGE_RULES_OUT_OF_RANGE, 2);
}

static void test_a_quad_refuses_fractions_and_non_finite_values_first(void) {
    assert_quad(0.5, 0, 2, 2, 4, 4, IMAGE_RULES_NOT_INTEGER, 0);
    assert_quad(0, 0, 2, 2, 4, 4.5, IMAGE_RULES_NOT_INTEGER, 5);
    assert_quad(0, 0, 1.0 / 1024, 2, 4, 4, IMAGE_RULES_NOT_INTEGER, 2);
    assert_quad(0, NAN, 2, 2, 4, 4, IMAGE_RULES_NOT_FINITE, 1);
    assert_quad(0, 0, 2, INFINITY, 4, 4, IMAGE_RULES_NOT_FINITE, 3);
    assert_quad(0, 0, 2, 2, -INFINITY, 4, IMAGE_RULES_NOT_FINITE, 4);
    assert_quad(-1, 0.5, 2, 2, 4, 4, IMAGE_RULES_NOT_INTEGER, 1);
}

static void test_the_first_translucent_pixel_is_found_or_none(void) {
    unsigned char pixels[6 * 4] = {0};
    for (int pixel = 0; pixel < 6; pixel++) {
        pixels[4 * pixel + 3] = pixel % 2 == 0 ? 0x00 : 0xFF;
        pixels[4 * pixel] = 0x80;
    }
    assert(image_rules_find_translucent_pixel(pixels, 6) == IMAGE_RULES_NONE);
    static const unsigned char translucent[] = {0x80, 0x01, 0xFE};
    for (int k = 0; k < 3; k++) {
        unsigned char copy[sizeof pixels];
        for (int byte = 0; byte < (int)sizeof pixels; byte++) {
            copy[byte] = pixels[byte];
        }
        copy[4 * 4 + 3] = translucent[k];
        assert(image_rules_find_translucent_pixel(copy, 6) == 4);
        copy[4 * 2 + 3] = translucent[k];
        assert(image_rules_find_translucent_pixel(copy, 6) == 2);
        assert(image_rules_find_translucent_pixel(copy, 2) == IMAGE_RULES_NONE);
    }
}

int main(void) {
    test_a_position_is_an_integer_of_magnitude_below_two_to_the_twenty();
    test_a_scale_is_an_integer_from_one_to_sixteen();
    test_a_quad_is_a_whole_number_of_texels_inside_its_reference_size();
    test_a_quad_refuses_empty_negative_and_outsized_values();
    test_a_quad_refuses_fractions_and_non_finite_values_first();
    test_the_first_translucent_pixel_is_found_or_none();
    puts("test_fake_love_image_rules: ok");
    return 0;
}
