#ifndef ENGINE_VEC_H
#define ENGINE_VEC_H

#include "p8/fixed.h"

typedef struct {
    p8_fixed_t x;
    p8_fixed_t y;
} vec_t;

static inline vec_t vec_make(p8_fixed_t x, p8_fixed_t y) {
    vec_t result;
    result.x = x;
    result.y = y;
    return result;
}

static inline vec_t vec_add(vec_t a, vec_t b) {
    return vec_make(p8_fixed_add(a.x, b.x), p8_fixed_add(a.y, b.y));
}

static inline vec_t vec_sub(vec_t a, vec_t b) {
    return vec_make(p8_fixed_sub(a.x, b.x), p8_fixed_sub(a.y, b.y));
}

static inline vec_t vec_mul(vec_t v, p8_fixed_t m) {
    return vec_make(p8_fixed_mul(v.x, m), p8_fixed_mul(v.y, m));
}

vec_t vec_div(vec_t v, p8_fixed_t d);

static inline vec_t vec_neg(vec_t v) {
    return vec_make(p8_fixed_neg(v.x), p8_fixed_neg(v.y));
}

static inline p8_fixed_t vec_dot(vec_t a, vec_t b) {
    return p8_fixed_add(p8_fixed_mul(a.x, b.x), p8_fixed_mul(a.y, b.y));
}

static inline p8_fixed_t vec_length_squared(vec_t v) {
    return vec_dot(v, v);
}

p8_fixed_t vec_length(vec_t v);

vec_t vec_normalize(vec_t v);

static inline vec_t vec_perpendicular(vec_t v) {
    return vec_make(p8_fixed_neg(v.y), v.x);
}

vec_t vec_from_polar(p8_fixed_t fy, p8_fixed_t lx);

#endif
