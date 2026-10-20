#include "point.h"

#include <assert.h>
#include <float.h>
#include <math.h>

_Static_assert(FLT_EVAL_METHOD == 0, "float expressions must round to float at each step, as the GPU does");

bool point_accepts_coordinate(double coordinate) {
    return isfinite(coordinate) && fabs(coordinate) < POINT_COORDINATE_LIMIT;
}

bool point_accepts_translated(double coordinate, int offset) {
    return point_accepts_coordinate(coordinate) && point_accepts_coordinate(offset) &&
           point_accepts_coordinate(coordinate + offset);
}

/* The window coordinate in 1/256 of a pixel. */
static long snap_to_window(float value, int size) {
    float half = (float)size / 2.0f;
    float scale = 2.0f / (float)size;
    /* Each rounding is its own statement: fusing the product into the sum,
     * or splitting the viewport fmaf, moves pixels along a 1/512 tie. */
    float projected = value * scale;
    float normalized = projected + -1.0f;
    float window = fmaf(normalized, half, half);
    float scaled = window * 256.0f;
    return lrintf(scaled);
}

static long divide_rounding_down(long numerator, long denominator) {
    long quotient = numerator / denominator;
    return numerator % denominator < 0 ? quotient - 1 : quotient;
}

int point_find_pixel(double coordinate, int offset, int size) {
    assert(size > 0);
    assert(point_accepts_translated(coordinate, offset));
    /* LÖVE translates each vertex in single precision; a double sum moves
     * it across a 1/512 tie. */
    float translated = (float)coordinate + (float)offset;
    long fixed = snap_to_window(translated, size);
    if (fixed < 1) {
        return POINT_OFF;
    }
    long pixel = (fixed - 1) / 256;
    return pixel < size ? (int)pixel : POINT_OFF;
}

bool point_find_span(double start, double extent, int offset, int size, int *first, int *last) {
    assert(size > 0);
    assert(point_accepts_translated(start, offset));
    assert(isfinite(extent) && fabs(extent) < 2.0 * POINT_COORDINATE_LIMIT &&
           "a translated edge below 2^20 plus an extent below 2^21 keeps every snapped value below 2^31");
    /* LÖVE adds the far edge in single precision before translating each
     * vertex; a double sum, or (start + offset) + extent, moves it across a
     * 1/512 tie. */
    float translation = (float)offset;
    float near_edge = (float)start + translation;
    float far_edge = ((float)start + (float)extent) + translation;
    long near_fixed = snap_to_window(near_edge, size);
    long far_fixed = snap_to_window(far_edge, size);
    long low = near_fixed < far_fixed ? near_fixed : far_fixed;
    long high = near_fixed < far_fixed ? far_fixed : near_fixed;
    /* Pixel i is filled when low <= 256 i + 128 < high: the top-left rule. */
    long first_filled = divide_rounding_down(low - 128 + 255, 256);
    long last_filled = divide_rounding_down(high - 128 + 255, 256) - 1;
    if (first_filled < 0) {
        first_filled = 0;
    }
    if (last_filled > size - 1) {
        last_filled = size - 1;
    }
    if (first_filled > last_filled) {
        return false;
    }
    *first = (int)first_filled;
    *last = (int)last_filled;
    return true;
}
