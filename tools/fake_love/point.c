#include "point.h"

#include <assert.h>
#include <float.h>
#include <math.h>

_Static_assert(FLT_EVAL_METHOD == 0, "float expressions must round to float at each step, as the GPU does");

bool point_accepts_coordinate(double coordinate) {
    return isfinite(coordinate) && fabs(coordinate) < POINT_COORDINATE_LIMIT;
}

int point_find_pixel(double coordinate, int size) {
    assert(size > 0);
    assert(point_accepts_coordinate(coordinate));
    float half = (float)size / 2.0f;
    float value = (float)coordinate;
    float scale = 2.0f / (float)size;
    /* Each rounding is its own statement: fusing the product into the sum,
     * or splitting the viewport fmaf, moves pixels along a 1/512 tie. */
    float projected = value * scale;
    float normalized = projected + -1.0f;
    float window = fmaf(normalized, half, half);
    float scaled = window * 256.0f;
    long fixed = lrintf(scaled);
    if (fixed < 1) {
        return POINT_OFF;
    }
    long pixel = (fixed - 1) / 256;
    return pixel < size ? (int)pixel : POINT_OFF;
}
