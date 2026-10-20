#include "engine/vec.h"

vec_t vec_div(vec_t v, p8_fixed_t d) {
    return vec_make(p8_fixed_div(v.x, d), p8_fixed_div(v.y, d));
}

p8_fixed_t vec_length(vec_t v) {
    return p8_fixed_sqrt(vec_length_squared(v));
}

vec_t vec_normalize(vec_t v) {
    return vec_div(v, vec_length(v));
}

vec_t vec_from_polar(p8_fixed_t fy, p8_fixed_t lx) {
    vec_t unit = vec_make(p8_fixed_cos(lx), p8_fixed_sin(lx));
    return vec_mul(unit, fy);
}
