#ifndef P8_FIXED_H
#define P8_FIXED_H

#include <stdint.h>

typedef int32_t p8_fixed_t;

#define P8_FIXED_SHIFT 16

#define P8_FIXED_DIV0_POS ((p8_fixed_t)0x7FFFFFFF)
#define P8_FIXED_DIV0_NEG ((p8_fixed_t)(-0x7FFFFFFF))

#define P8_FIXED_LIT(raw, decimal) ((p8_fixed_t)(uint32_t)(raw##u))

static inline p8_fixed_t p8_fixed_from_int(int32_t i) {
    return (p8_fixed_t)((uint32_t)i << P8_FIXED_SHIFT);
}

#define P8_FIXED_INT(i) ((p8_fixed_t)((uint32_t)(int32_t)(i) << P8_FIXED_SHIFT))

/* Wrap in the unsigned type: int32_t overflow is undefined behavior. */
static inline p8_fixed_t p8_fixed_add(p8_fixed_t a, p8_fixed_t b) {
    return (p8_fixed_t)((uint32_t)a + (uint32_t)b);
}

static inline p8_fixed_t p8_fixed_sub(p8_fixed_t a, p8_fixed_t b) {
    return (p8_fixed_t)((uint32_t)a - (uint32_t)b);
}

static inline p8_fixed_t p8_fixed_mul(p8_fixed_t a, p8_fixed_t b) {
    int64_t product = (int64_t)a * (int64_t)b;
    int64_t rescaled = product >> P8_FIXED_SHIFT;
    return (p8_fixed_t)(uint32_t)rescaled;
}

p8_fixed_t p8_fixed_div(p8_fixed_t a, p8_fixed_t b);

static inline p8_fixed_t p8_fixed_neg(p8_fixed_t x) {
    return (p8_fixed_t)((uint32_t)0 - (uint32_t)x);
}

static inline p8_fixed_t p8_fixed_flr(p8_fixed_t x) {
    return (p8_fixed_t)((uint32_t)x & 0xFFFF0000u);
}

static inline int32_t p8_fixed_flr_int(p8_fixed_t x) {
    return p8_fixed_flr(x) / ((int32_t)1 << P8_FIXED_SHIFT);
}

static inline p8_fixed_t p8_fixed_abs(p8_fixed_t x) {
    return x < 0 ? p8_fixed_neg(x) : x;
}

static inline p8_fixed_t p8_fixed_sgn(p8_fixed_t x) {
    return x < 0 ? p8_fixed_from_int(-1) : p8_fixed_from_int(1);
}

p8_fixed_t p8_fixed_mid(p8_fixed_t a, p8_fixed_t b, p8_fixed_t c);

static inline p8_fixed_t p8_fixed_band(p8_fixed_t a, p8_fixed_t b) {
    return a & b;
}

static inline p8_fixed_t p8_fixed_bor(p8_fixed_t a, p8_fixed_t b) {
    return a | b;
}

p8_fixed_t p8_fixed_mod(p8_fixed_t a, p8_fixed_t b);

p8_fixed_t p8_fixed_shl(p8_fixed_t x, int32_t n);

p8_fixed_t p8_fixed_sqrt(p8_fixed_t x);

p8_fixed_t p8_fixed_pow075(p8_fixed_t x);

p8_fixed_t p8_fixed_rnd(p8_fixed_t x);

/* The LCG and scaling of p8_fixed_rnd, on the caller's state: advances
 * *state, never the global one. rnd(0) returns 0 and leaves *state as is. */
p8_fixed_t p8_fixed_rnd_from_state(uint32_t *state, p8_fixed_t x);

p8_fixed_t p8_fixed_rnd_default(void);

uint32_t p8_fixed_rnd_state(void);
void p8_fixed_set_rnd_state(uint32_t state);

p8_fixed_t p8_fixed_cos(p8_fixed_t x);

p8_fixed_t p8_fixed_sin(p8_fixed_t x);

#endif
