#include "p8/fixed.h"
#include "p8/quarter_wave.h"

p8_fixed_t p8_fixed_div(p8_fixed_t a, p8_fixed_t b) {
    if (b == 0) {
        return a < 0 ? P8_FIXED_DIV0_NEG : P8_FIXED_DIV0_POS;
    }
    /* Left-shifting a negative signed value is undefined behavior: shift in uint64_t. */
    int64_t numerator = (int64_t)((uint64_t)(int64_t)a << P8_FIXED_SHIFT);
    int64_t quotient = numerator / b;
    return (p8_fixed_t)(uint32_t)quotient;
}

p8_fixed_t p8_fixed_mid(p8_fixed_t a, p8_fixed_t b, p8_fixed_t c) {
    if ((a <= b && b <= c) || (c <= b && b <= a)) {
        return b;
    }
    if ((b <= a && a <= c) || (c <= a && a <= b)) {
        return a;
    }
    return c;
}

p8_fixed_t p8_fixed_mod(p8_fixed_t a, p8_fixed_t b) {
    return p8_fixed_sub(a, p8_fixed_mul(p8_fixed_flr(p8_fixed_div(a, b)), b));
}

p8_fixed_t p8_fixed_shl(p8_fixed_t x, int32_t n) {
    if (n < 0 || n >= 32) {
        return 0;
    }
    return (p8_fixed_t)((uint32_t)x << (uint32_t)n);
}

static uint32_t isqrt_u64(uint64_t n) {
    uint64_t lo = 0;
    uint64_t hi = 0xFFFFFFFFu;
    while (lo < hi) {
        uint64_t mid = lo + (hi - lo + 1) / 2;
        if (mid * mid <= n) {
            lo = mid;
        } else {
            hi = mid - 1;
        }
    }
    return (uint32_t)lo;
}

p8_fixed_t p8_fixed_sqrt(p8_fixed_t x) {
    if (x < 0) {
        return 0;
    }
    uint64_t scaled = (uint64_t)(uint32_t)x << P8_FIXED_SHIFT;
    return (p8_fixed_t)isqrt_u64(scaled);
}

p8_fixed_t p8_fixed_pow075(p8_fixed_t x) {
    p8_fixed_t s = p8_fixed_sqrt(x);
    return p8_fixed_mul(s, p8_fixed_sqrt(s));
}

#define P8_RND_LCG_MULTIPLIER 1664525u
#define P8_RND_LCG_INCREMENT 1013904223u
#define P8_RND_DEFAULT_SEED 0x2545F491u

static uint32_t rnd_state = P8_RND_DEFAULT_SEED;

p8_fixed_t p8_fixed_rnd_from_state(uint32_t *state, p8_fixed_t x) {
    if (x == 0) {
        return 0;
    }
    uint32_t magnitude = (x < 0) ? (uint32_t)(-(int64_t)x) : (uint32_t)x;
    *state = *state * P8_RND_LCG_MULTIPLIER + P8_RND_LCG_INCREMENT;
    uint64_t scaled = ((uint64_t)magnitude * (uint64_t)*state) >> 32;
    p8_fixed_t result = (p8_fixed_t)(uint32_t)scaled;
    return (x < 0) ? p8_fixed_neg(result) : result;
}

p8_fixed_t p8_fixed_rnd(p8_fixed_t x) {
    return p8_fixed_rnd_from_state(&rnd_state, x);
}

uint32_t p8_fixed_rnd_state(void) {
    return rnd_state;
}

void p8_fixed_set_rnd_state(uint32_t state) {
    rnd_state = state;
}

p8_fixed_t p8_fixed_rnd_default(void) {
    return p8_fixed_rnd(p8_fixed_from_int(1));
}

static p8_fixed_t quarter_wave_at(uint32_t f) {
    uint32_t quadrant = (f >> 14) & 3u;
    uint32_t r = f & 0x3FFFu;
    int32_t v = (quadrant & 1u) ? p8_quarter_wave[0x4000u - r] : p8_quarter_wave[r];
    return (quadrant & 2u) ? -v : v;
}

p8_fixed_t p8_fixed_cos(p8_fixed_t x) {
    return quarter_wave_at(((uint32_t)x + 0x4000u) & 0xFFFFu);
}

p8_fixed_t p8_fixed_sin(p8_fixed_t x) {
    return -quarter_wave_at((uint32_t)x & 0xFFFFu);
}
