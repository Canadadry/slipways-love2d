#include "image_rules.h"

#include <math.h>

#include "image_data.h"
#include "point.h"
#include "screen.h"

enum { QUAD_X, QUAD_Y, QUAD_WIDTH, QUAD_HEIGHT, QUAD_REFERENCE_WIDTH, QUAD_REFERENCE_HEIGHT };

static image_rules_verdict_t check_integer(double value) {
    if (!isfinite(value)) {
        return IMAGE_RULES_NOT_FINITE;
    }
    return floor(value) == value ? IMAGE_RULES_ACCEPTED : IMAGE_RULES_NOT_INTEGER;
}

image_rules_verdict_t image_rules_check_position(double position) {
    image_rules_verdict_t verdict = check_integer(position);
    if (verdict != IMAGE_RULES_ACCEPTED) {
        return verdict;
    }
    return point_accepts_coordinate(position) ? IMAGE_RULES_ACCEPTED : IMAGE_RULES_OUT_OF_RANGE;
}

image_rules_verdict_t image_rules_check_screen_position(double position) {
    if (!isfinite(position)) {
        return IMAGE_RULES_NOT_FINITE;
    }
    return point_accepts_coordinate(position) ? IMAGE_RULES_ACCEPTED : IMAGE_RULES_OUT_OF_RANGE;
}

image_rules_verdict_t image_rules_check_screen_scale(double scale) {
    if (!isfinite(scale)) {
        return IMAGE_RULES_NOT_FINITE;
    }
    return scale > 0 && scale <= SCREEN_MAX_SCALE ? IMAGE_RULES_ACCEPTED : IMAGE_RULES_OUT_OF_RANGE;
}

image_rules_verdict_t image_rules_check_quad(const double values[IMAGE_RULES_QUAD_VALUES], int *refused) {
    for (int index = 0; index < IMAGE_RULES_QUAD_VALUES; index++) {
        image_rules_verdict_t verdict = check_integer(values[index]);
        if (verdict != IMAGE_RULES_ACCEPTED) {
            *refused = index;
            return verdict;
        }
    }
    double x = values[QUAD_X];
    double y = values[QUAD_Y];
    double reference_width = values[QUAD_REFERENCE_WIDTH];
    double reference_height = values[QUAD_REFERENCE_HEIGHT];
    /* Reference sides first: the ranges of x and y use them, and those of
     * the width and height use x and y. */
    const struct {
        int index;
        double low;
        double high;
    } ranges[IMAGE_RULES_QUAD_VALUES] = {
        {QUAD_REFERENCE_WIDTH, 1, IMAGE_DATA_MAX_SIDE},
        {QUAD_REFERENCE_HEIGHT, 1, IMAGE_DATA_MAX_SIDE},
        {QUAD_X, 0, reference_width - 1},
        {QUAD_Y, 0, reference_height - 1},
        {QUAD_WIDTH, 1, reference_width - x},
        {QUAD_HEIGHT, 1, reference_height - y},
    };
    for (int k = 0; k < IMAGE_RULES_QUAD_VALUES; k++) {
        double value = values[ranges[k].index];
        if (value < ranges[k].low || value > ranges[k].high) {
            *refused = ranges[k].index;
            return IMAGE_RULES_OUT_OF_RANGE;
        }
    }
    return IMAGE_RULES_ACCEPTED;
}

int image_rules_find_translucent_pixel(const unsigned char *pixels, int pixel_count) {
    for (int pixel = 0; pixel < pixel_count; pixel++) {
        unsigned char alpha = pixels[4 * pixel + 3];
        if (alpha != 0x00 && alpha != 0xFF) {
            return pixel;
        }
    }
    return IMAGE_RULES_NONE;
}
