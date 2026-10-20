#ifndef FAKE_LOVE_POINT_H
#define FAKE_LOVE_POINT_H

#include <stdbool.h>

#define POINT_OFF (-1)
#define POINT_COORDINATE_LIMIT 1048576.0

/* Finite and of magnitude below POINT_COORDINATE_LIMIT (2^20), eight times
 * below the 2^23 where llvmpipe's fixed point wraps a point onto the canvas. */
bool point_accepts_coordinate(double coordinate);

/* An accepted coordinate whose sum with `offset` is accepted too; false
 * when the offset itself is not accepted. */
bool point_accepts_translated(double coordinate, int offset);

/* The pixel index along an axis of `size` pixels that a point at
 * `coordinate`, translated by the integer `offset`, lights in LÖVE 11.5 on
 * Mesa llvmpipe, or POINT_OFF when it lands outside 0 ... size - 1. Asserts
 * size > 0 and an accepted translated coordinate. */
int point_find_pixel(double coordinate, int offset, int size);

/* Whether a rectangle edge at `start` with the signed `extent`, translated
 * by the integer `offset`, fills any pixel along an axis of `size` pixels in
 * LÖVE 11.5 on Mesa llvmpipe; if so the filled pixels are *first ... *last,
 * clipped to the canvas, and the outputs are left untouched otherwise.
 * Asserts size > 0, an accepted translated start and a finite extent of
 * magnitude below 2^21. */
bool point_find_span(double start, double extent, int offset, int size, int *first, int *last);

#endif
