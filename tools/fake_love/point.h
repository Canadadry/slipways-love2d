#ifndef FAKE_LOVE_POINT_H
#define FAKE_LOVE_POINT_H

#include <stdbool.h>

#define POINT_OFF (-1)
#define POINT_COORDINATE_LIMIT 1048576.0

/* Finite and of magnitude below POINT_COORDINATE_LIMIT (2^20), eight times
 * below the 2^23 where llvmpipe's fixed point wraps a point onto the canvas. */
bool point_accepts_coordinate(double coordinate);

/* The pixel index along an axis of `size` pixels that a point at
 * `coordinate` lights in LÖVE 11.5 on Mesa llvmpipe, or POINT_OFF when it
 * lands outside 0 ... size - 1. Asserts size > 0 and an accepted
 * coordinate. */
int point_find_pixel(double coordinate, int size);

#endif
