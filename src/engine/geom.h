#ifndef ENGINE_GEOM_H
#define ENGINE_GEOM_H

#include "p8/fixed.h"
#include "engine/vec.h"
#include "engine/allocator.h"

typedef enum {
    SIDE_ON_LINE = 0,
    SIDE_POSITIVE = 1,
    SIDE_NEGATIVE = -1
} side_of_line_t;

side_of_line_t geom_side_of_line(vec_t a, vec_t b, vec_t c);

int geom_segments_cross(vec_t p1, vec_t p2, vec_t q1, vec_t q2);

int geom_near_segment(vec_t a, vec_t b, vec_t c, p8_fixed_t fh);

typedef struct {
    p8_fixed_t left;
    p8_fixed_t top;
    p8_fixed_t right;
    p8_fixed_t bottom;
} box_t;

box_t *box_make(const allocator_t *zone, p8_fixed_t xl, p8_fixed_t yt, p8_fixed_t xr, p8_fixed_t yb);

/* mid(left, right, x) == x, written out so the hover loop's inline test makes no call. */
static inline int box_contains(box_t box, vec_t p) {
    return ((box.left <= p.x && p.x <= box.right) || (box.right <= p.x && p.x <= box.left)) &&
           ((box.top <= p.y && p.y <= box.bottom) || (box.bottom <= p.y && p.y <= box.top));
}

#endif
