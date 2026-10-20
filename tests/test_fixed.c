#include "p8/fixed.h"
#include "devtools/fixed_test_double.h"

#include <assert.h>
#include <math.h>
#include <stdint.h>
#include <stdio.h>

static void test_add_representative_values(void) {
    p8_fixed_t a = fixed_test_double_from_double(2.5);
    p8_fixed_t b = fixed_test_double_from_double(1.25);
    p8_fixed_t result = p8_fixed_add(a, b);
    assert(result == fixed_test_double_from_double(3.75));
}

static void test_sub_representative_values(void) {
    p8_fixed_t a = fixed_test_double_from_double(5.5);
    p8_fixed_t b = fixed_test_double_from_double(2.25);
    p8_fixed_t result = p8_fixed_sub(a, b);
    assert(result == fixed_test_double_from_double(3.25));
}

static void test_sub_wraps_at_most_negative_raw_value(void) {
    assert(p8_fixed_sub((p8_fixed_t)INT32_MIN, (p8_fixed_t)1) == (p8_fixed_t)INT32_MAX);
}

static void test_sub_wraps_at_most_positive_raw_value(void) {
    assert(p8_fixed_sub((p8_fixed_t)INT32_MAX, (p8_fixed_t)-1) == (p8_fixed_t)INT32_MIN);
}

static void test_from_int_outside_the_integer_part_wraps(void) {
    assert(p8_fixed_from_int(32768) == (p8_fixed_t)INT32_MIN);
    assert(p8_fixed_from_int(-1) == (p8_fixed_t)0xFFFF0000u);
    assert(p8_fixed_from_int(65537) == (p8_fixed_t)0x00010000);
}

static void test_mul_representative_values(void) {
    p8_fixed_t a = fixed_test_double_from_double(2.5);
    p8_fixed_t b = fixed_test_double_from_double(4.0);
    p8_fixed_t result = p8_fixed_mul(a, b);
    assert(result == fixed_test_double_from_double(10.0));
}

static void test_mul_overflow_wraps(void) {
    /* 200*200 = 40000, which exceeds the 16-bit signed integer part's
     * range (max 32767), so the truncation wraps it to 40000 - 65536 =
     * -25536, per p8_fixed_mul's documented truncation behavior. */
    p8_fixed_t a = p8_fixed_from_int(200);
    p8_fixed_t b = p8_fixed_from_int(200);
    p8_fixed_t result = p8_fixed_mul(a, b);
    assert(result == p8_fixed_from_int(-25536));
}

static void test_add_overflow_128_squared_twice_wraps_to_min(void) {
    /* PICO-8's documented classic overflow bug: 128*128+128*128 overflows
     * the representable range and wraps to -32768.
     * Source: PICO-8 Known Bugs wiki,
     * https://pico-8.fandom.com/wiki/Known_Bugs */
    p8_fixed_t hundred_twenty_eight = p8_fixed_from_int(128);
    p8_fixed_t squared = p8_fixed_mul(hundred_twenty_eight, hundred_twenty_eight);
    assert(squared == p8_fixed_from_int(16384));

    p8_fixed_t result = p8_fixed_add(squared, squared);
    assert(result == p8_fixed_from_int(-32768));
}

static void test_div_representative_value(void) {
    p8_fixed_t a = fixed_test_double_from_double(10.0);
    p8_fixed_t b = fixed_test_double_from_double(4.0);
    p8_fixed_t result = p8_fixed_div(a, b);
    assert(result == fixed_test_double_from_double(2.5));
}

static void test_div_negative_dividend_nonzero_divisor(void) {
    /* -10 / 4 = -2.5. Guards against the numerator going back to a signed
     * left shift, which is undefined for a negative value however right the
     * result looks on x86. */
    p8_fixed_t a = p8_fixed_from_int(-10);
    p8_fixed_t b = p8_fixed_from_int(4);
    p8_fixed_t result = p8_fixed_div(a, b);
    assert(result == fixed_test_double_from_double(-2.5));
}

static void test_div_by_zero_positive_dividend_saturates_positive(void) {
    /* Per the PICO-8 manual, division by zero saturates to 0x7fff.ffff
     * when the dividend is non-negative.
     * Source: https://www.lexaloffle.com/pico8_manual.txt */
    p8_fixed_t a = p8_fixed_from_int(5);
    p8_fixed_t result = p8_fixed_div(a, p8_fixed_from_int(0));
    assert(result == P8_FIXED_DIV0_POS);
}

static void test_div_by_zero_negative_dividend_saturates_negative(void) {
    /* Per the PICO-8 manual, division by zero saturates to -0x7fff.ffff
     * when the dividend is negative.
     * Source: https://www.lexaloffle.com/pico8_manual.txt */
    p8_fixed_t a = p8_fixed_from_int(-5);
    p8_fixed_t result = p8_fixed_div(a, p8_fixed_from_int(0));
    assert(result == P8_FIXED_DIV0_NEG);
}

static void test_div_zero_by_zero_saturates_positive(void) {
    /* 0 divided by 0 is treated as non-negative (0 is not negative),
     * so it saturates to +0x7fff.ffff, since this exact sub-case isn't
     * separately documented. */
    p8_fixed_t result = p8_fixed_div(p8_fixed_from_int(0), p8_fixed_from_int(0));
    assert(result == P8_FIXED_DIV0_POS);
}

static void test_neg_representative_value(void) {
    p8_fixed_t a = fixed_test_double_from_double(3.5);
    p8_fixed_t result = p8_fixed_neg(a);
    assert(result == fixed_test_double_from_double(-3.5));
}

static void test_neg_most_negative_value_wraps_to_itself(void) {
    /* Unary - is 0 - x under the same two's-complement wraparound rule as
     * +/-, so negating the most-negative representable value (-32768.0,
     * raw INT32_MIN) wraps rather than overflowing: 0 - INT32_MIN, computed
     * in unsigned 32-bit arithmetic, wraps back around to INT32_MIN
     * itself. */
    p8_fixed_t most_negative = (p8_fixed_t)INT32_MIN;
    assert(most_negative == p8_fixed_from_int(-32768));

    p8_fixed_t result = p8_fixed_neg(most_negative);
    assert(result == most_negative);
}

static void test_flr_positive_truncates_fraction(void) {
    /* flr truncates toward negative infinity on the fixed value: 3.75
     * floors to 3.0. */
    p8_fixed_t x = fixed_test_double_from_double(3.75);
    assert(p8_fixed_flr(x) == p8_fixed_from_int(3));
}

static void test_flr_negative_rounds_toward_negative_infinity(void) {
    /* -3.25 floors to -4.0, not -3.0 -- toward negative infinity, not
     * toward zero. */
    p8_fixed_t x = fixed_test_double_from_double(-3.25);
    assert(p8_fixed_flr(x) == p8_fixed_from_int(-4));
}

static void test_flr_exact_integer_is_unchanged(void) {
    p8_fixed_t x = p8_fixed_from_int(5);
    assert(p8_fixed_flr(x) == p8_fixed_from_int(5));
}

static void test_flr_int_matches_probe_block_b(void) {
    /* Block B (rounding) of tools/pico8_reference_probe_out.txt: copied
     * from the probe, never derived. */
    static const struct { uint32_t raw; int32_t expected; } probe[] = {
        {0xFFFE8000u, -2}, {0xFFFF8000u, -1}, {0xFFFF199Au, -1},
        {0xFFFF0000u, -1}, {0xFFFD8000u, -3}, {0x00018000u, 1},
        {0x00008000u, 0},  {0x0000E666u, 0},  {0x00010000u, 1},
        {0x00000000u, 0},
    };
    for (size_t i = 0; i < sizeof probe / sizeof probe[0]; i++) {
        assert(p8_fixed_flr_int((p8_fixed_t)probe[i].raw) == probe[i].expected);
    }
}

static void test_lit_is_the_raw_probe_value(void) {
    assert(P8_FIXED_LIT(0x0000451e, 0.27) == 17694);
    assert(P8_FIXED_LIT(0x000a8000, 10.5) == (p8_fixed_t)0x000A8000u);
    assert(P8_FIXED_LIT(0x0000fd70, 0.99) == 0xFD70);
    assert(P8_FIXED_LIT(0x00008000, 0.5) == fixed_test_double_from_double(0.5));
}

/* Compiles only if the macro drops its second argument unevaluated. */
static void test_lit_never_evaluates_its_literal(void) {
    assert(P8_FIXED_LIT(0x00018000, never_declared_anywhere) == 0x18000);
}

static void test_flr_int_extremes(void) {
    assert(p8_fixed_flr_int((p8_fixed_t)0x7FFFFFFFu) == 32767);
    assert(p8_fixed_flr_int((p8_fixed_t)0x80000000u) == -32768);
}

static void test_flr_int_raw_minus_one_is_minus_one_not_zero(void) {
    assert(p8_fixed_flr_int((p8_fixed_t)0xFFFFFFFFu) == -1);
}

static void test_mod_positive_operands(void) {
    /* 45 % 42 == 3: the shape trading_hub_update_settled's `% 42` needs. */
    p8_fixed_t a = p8_fixed_from_int(45);
    p8_fixed_t b = p8_fixed_from_int(42);
    assert(p8_fixed_mod(a, b) == p8_fixed_from_int(3));
}

static void test_mod_fractional_operand(void) {
    /* dividend - flr(dividend / divisor) * divisor: 5.5 % 2 == 1.5. */
    p8_fixed_t a = fixed_test_double_from_double(5.5);
    p8_fixed_t b = p8_fixed_from_int(2);
    assert(p8_fixed_mod(a, b) == fixed_test_double_from_double(1.5));
}

static void test_mod_negative_dividend_floors(void) {
    /* PICO-8's floor-based modulo (not C's truncating "%"): -1 % 3 == 2, not
     * -1, since flr(-1/3)==-1 and -1-(-1*3)==2. */
    p8_fixed_t a = p8_fixed_from_int(-1);
    p8_fixed_t b = p8_fixed_from_int(3);
    assert(p8_fixed_mod(a, b) == p8_fixed_from_int(2));
}

static void test_mod_result_less_than_divisor_when_exact(void) {
    /* An exact multiple leaves a zero remainder. */
    p8_fixed_t a = p8_fixed_from_int(84);
    p8_fixed_t b = p8_fixed_from_int(42);
    assert(p8_fixed_mod(a, b) == p8_fixed_from_int(0));
}

static void test_abs_negative_value(void) {
    p8_fixed_t x = fixed_test_double_from_double(-3.5);
    assert(p8_fixed_abs(x) == fixed_test_double_from_double(3.5));
}

static void test_abs_positive_value_is_unchanged(void) {
    p8_fixed_t x = fixed_test_double_from_double(3.5);
    assert(p8_fixed_abs(x) == fixed_test_double_from_double(3.5));
}

static void test_abs_zero_is_zero(void) {
    assert(p8_fixed_abs(p8_fixed_from_int(0)) == p8_fixed_from_int(0));
}

static void test_abs_most_negative_value_wraps_to_itself(void) {
    /* Same two's-complement caveat as neg: the most-negative representable
     * value's abs wraps rather than growing. */
    p8_fixed_t most_negative = (p8_fixed_t)INT32_MIN;
    assert(p8_fixed_abs(most_negative) == most_negative);
}

static void test_sgn_positive_value_is_one(void) {
    p8_fixed_t x = fixed_test_double_from_double(3.5);
    assert(p8_fixed_sgn(x) == p8_fixed_from_int(1));
}

static void test_sgn_negative_value_is_negative_one(void) {
    p8_fixed_t x = fixed_test_double_from_double(-3.5);
    assert(p8_fixed_sgn(x) == p8_fixed_from_int(-1));
}

static void test_sgn_zero_is_one_not_zero(void) {
    /* PICO-8's documented sgn(0) == 1, not 0.
     * Source: PICO-8 Sgn wiki, https://pico-8.fandom.com/wiki/Sgn */
    p8_fixed_t result = p8_fixed_sgn(p8_fixed_from_int(0));
    assert(result == p8_fixed_from_int(1));
}

static void test_mid_middle_argument_already_sorted(void) {
    p8_fixed_t a = p8_fixed_from_int(1);
    p8_fixed_t b = p8_fixed_from_int(5);
    p8_fixed_t c = p8_fixed_from_int(10);
    assert(p8_fixed_mid(a, b, c) == b);
}

static void test_mid_is_order_independent(void) {
    /* The median of three values does not depend on the order they are
     * passed in, so every call-site convention clamps the same. */
    p8_fixed_t lo = p8_fixed_from_int(0);
    p8_fixed_t hi = p8_fixed_from_int(10);
    p8_fixed_t v = p8_fixed_from_int(4);
    p8_fixed_t expected = v; /* already within the bounds */

    assert(p8_fixed_mid(lo, hi, v) == expected);
    assert(p8_fixed_mid(hi, lo, v) == expected);
    assert(p8_fixed_mid(v, lo, hi) == expected);
    assert(p8_fixed_mid(lo, v, hi) == expected);
    assert(p8_fixed_mid(hi, v, lo) == expected);
    assert(p8_fixed_mid(v, hi, lo) == expected);
}

static void test_mid_clamps_value_below_range(void) {
    p8_fixed_t lo = p8_fixed_from_int(0);
    p8_fixed_t hi = p8_fixed_from_int(10);
    p8_fixed_t v = p8_fixed_from_int(-5);
    assert(p8_fixed_mid(lo, hi, v) == lo);
}

static void test_mid_clamps_value_above_range(void) {
    p8_fixed_t lo = p8_fixed_from_int(0);
    p8_fixed_t hi = p8_fixed_from_int(10);
    p8_fixed_t v = p8_fixed_from_int(50);
    assert(p8_fixed_mid(lo, hi, v) == hi);
}

static void test_band_positive_raw_operands(void) {
    /* `band` combines the raw 32-bit representations, not the human
     * values: 0x00060000 AND 0x00030000 is 0x00020000. It matches integer
     * AND here only because the fractional bits are all zero. */
    p8_fixed_t a = p8_fixed_from_int(6);
    p8_fixed_t b = p8_fixed_from_int(3);
    assert(p8_fixed_band(a, b) == p8_fixed_from_int(2));
}

static void test_band_negative_raw_operand(void) {
    /* -1 is raw 0xFFFFFFFF (all bits set). AND with any value returns
     * that value unchanged. */
    p8_fixed_t all_ones = (p8_fixed_t)0xFFFFFFFFu;
    p8_fixed_t x = fixed_test_double_from_double(2.5);
    assert(p8_fixed_band(all_ones, x) == x);
}

static void test_bor_positive_raw_operands(void) {
    p8_fixed_t a = p8_fixed_from_int(4);
    p8_fixed_t b = p8_fixed_from_int(1);
    assert(p8_fixed_bor(a, b) == p8_fixed_from_int(5));
}

static void test_bor_negative_raw_operand(void) {
    /* OR with all-bits-set (-1) always yields all-bits-set, regardless of
     * the other operand. */
    p8_fixed_t all_ones = (p8_fixed_t)0xFFFFFFFFu;
    p8_fixed_t x = fixed_test_double_from_double(2.5);
    assert(p8_fixed_bor(all_ones, x) == all_ones);
}

static void test_shl_positive_raw_operand(void) {
    /* p8_fixed_shl shifts the raw representation: by 4, times sixteen on
     * an integer value. */
    p8_fixed_t x = p8_fixed_from_int(5);
    assert(p8_fixed_shl(x, 4) == p8_fixed_from_int(80));
}

static void test_shl_negative_raw_operand(void) {
    /* Same shift UB as in `p8_fixed_div`: shifting a negative signed
     * value must go through the unsigned reinterpretation. -1, raw
     * 0xFFFFFFFF, shifted by 1 is 0xFFFFFFFE. */
    p8_fixed_t x = (p8_fixed_t)0xFFFFFFFFu;
    p8_fixed_t result = p8_fixed_shl(x, 1);
    assert(result == (p8_fixed_t)0xFFFFFFFEu);
}

static void test_shl_by_zero_is_unchanged(void) {
    p8_fixed_t x = fixed_test_double_from_double(2.5);
    assert(p8_fixed_shl(x, 0) == x);
}

static void test_sqrt_perfect_square(void) {
    p8_fixed_t x = p8_fixed_from_int(4);
    assert(p8_fixed_sqrt(x) == p8_fixed_from_int(2));
}

static void test_sqrt_perfect_square_fractional(void) {
    p8_fixed_t x = fixed_test_double_from_double(0.25);
    assert(p8_fixed_sqrt(x) == fixed_test_double_from_double(0.5));
}

static void test_sqrt_zero_is_zero(void) {
    assert(p8_fixed_sqrt(p8_fixed_from_int(0)) == p8_fixed_from_int(0));
}

static void test_sqrt_negative_value_is_zero(void) {
    /* As of PICO-8 0.2.6b, sqrt of a negative value returns 0.
     * Source: PICO-8 Sqrt wiki, https://pico-8.fandom.com/wiki/Sqrt */
    p8_fixed_t x = p8_fixed_from_int(-4);
    assert(p8_fixed_sqrt(x) == p8_fixed_from_int(0));
}

static void test_sqrt_matches_probe_block_e(void) {
    /* Block E (sqrt) of tools/pico8_reference_probe_out.txt: copied from
     * the probe, never derived. */
    static const struct { uint32_t raw; uint32_t expected; } probe[] = {
        {0x00004000u, 0x00008000u}, {0x00008000u, 0x0000B504u},
        {0x0000C000u, 0x0000DDB3u}, {0x00010000u, 0x00010000u},
        {0x00018000u, 0x00013988u}, {0x00020000u, 0x00016A09u},
        {0x00030000u, 0x0001BB67u}, {0x00040000u, 0x00020000u},
        {0x00060000u, 0x00027311u}, {0x00080000u, 0x0002D413u},
        {0x000C0000u, 0x000376CFu}, {0x00100000u, 0x00040000u},
        {0x00180000u, 0x0004E623u}, {0x00200000u, 0x0005A827u},
        {0x00400000u, 0x00080000u}, {0x00800000u, 0x000B504Fu},
    };
    for (size_t i = 0; i < sizeof probe / sizeof probe[0]; i++) {
        assert(p8_fixed_sqrt((p8_fixed_t)probe[i].raw) == (p8_fixed_t)probe[i].expected);
    }
}

static void test_pow075_matches_probe_block_d(void) {
    /* Block D (pow075) of tools/pico8_reference_probe_out.txt: copied from
     * the probe, never derived. */
    static const struct { uint32_t raw; uint32_t expected; } probe[] = {
        {0x00004000u, 0x00005A82u}, {0x00008000u, 0x00009836u},
        {0x0000C000u, 0x0000CE4Fu}, {0x00010000u, 0x00010000u},
        {0x00018000u, 0x00015AFAu}, {0x00020000u, 0x0001AE87u},
        {0x00030000u, 0x0002478Au}, {0x00040000u, 0x0002D412u},
        {0x00060000u, 0x0003D568u}, {0x00080000u, 0x0004C1BBu},
        {0x000C0000u, 0x00067288u}, {0x00100000u, 0x00080000u},
        {0x00180000u, 0x000AD7D7u}, {0x00200000u, 0x000D744Au},
        {0x00400000u, 0x0016A098u}, {0x00800000u, 0x00260DF0u},
    };
    for (size_t i = 0; i < sizeof probe / sizeof probe[0]; i++) {
        assert(p8_fixed_pow075((p8_fixed_t)probe[i].raw) == (p8_fixed_t)probe[i].expected);
    }
}

static void test_pow075_zero_is_zero(void) {
    assert(p8_fixed_pow075(p8_fixed_from_int(0)) == p8_fixed_from_int(0));
}

static void test_pow075_negative_value_is_zero(void) {
    assert(p8_fixed_pow075(p8_fixed_from_int(-16)) == p8_fixed_from_int(0));
    assert(p8_fixed_pow075((p8_fixed_t)0xFFFFC000u) == p8_fixed_from_int(0));
}

/* rnd is asserted on its documented contract only: PICO-8's own PRNG is not
 * published, so no drawn value has a reference to compare against. */

static void test_rnd_zero_is_zero(void) {
    /* rnd(0) returns 0. */
    assert(p8_fixed_rnd(p8_fixed_from_int(0)) == p8_fixed_from_int(0));
}

static void test_rnd_result_in_documented_range(void) {
    /* rnd(x) returns a value in [0, x). Sampled repeatedly (the generator
     * is stateful) to exercise more than one draw, asserting only the
     * documented range property, never a specific drawn value. */
    p8_fixed_t x = p8_fixed_from_int(10);
    for (int i = 0; i < 200; i++) {
        p8_fixed_t result = p8_fixed_rnd(x);
        assert(result >= p8_fixed_from_int(0));
        assert(result < x);
    }
}

static void test_rnd_default_behaves_as_rnd_one(void) {
    /* rnd() (no argument) behaves as rnd(1): its result is in the same
     * documented range [0, 1) that rnd(1) itself is bound by. */
    p8_fixed_t one = p8_fixed_from_int(1);
    for (int i = 0; i < 200; i++) {
        p8_fixed_t result = p8_fixed_rnd_default();
        assert(result >= p8_fixed_from_int(0));
        assert(result < one);
    }
}

static void test_rnd_state_set_is_read_back(void) {
    static const uint32_t values[] = {0u, 1u, 0x12345678u, 0xFFFFFFFFu};
    uint32_t saved = p8_fixed_rnd_state();
    for (unsigned i = 0; i < sizeof values / sizeof values[0]; i++) {
        p8_fixed_set_rnd_state(values[i]);
        assert(p8_fixed_rnd_state() == values[i]);
    }
    p8_fixed_set_rnd_state(saved);
}

static void test_rnd_draw_changes_the_state(void) {
    uint32_t before = p8_fixed_rnd_state();
    p8_fixed_rnd(p8_fixed_from_int(1));
    assert(p8_fixed_rnd_state() != before);
}

static void test_rnd_restored_state_replays_the_same_draws(void) {
    static const int32_t arguments[] = {1, 10, -3, 0, 128, 7, -1, 2};
    enum { DRAWS = 64 };
    p8_fixed_t first[DRAWS];
    uint32_t saved = p8_fixed_rnd_state();
    for (int i = 0; i < DRAWS; i++) {
        first[i] = p8_fixed_rnd(p8_fixed_from_int(arguments[i % 8]));
    }
    uint32_t after_first = p8_fixed_rnd_state();
    p8_fixed_set_rnd_state(saved);
    for (int i = 0; i < DRAWS; i++) {
        assert(p8_fixed_rnd(p8_fixed_from_int(arguments[i % 8])) == first[i]);
    }
    assert(p8_fixed_rnd_state() == after_first);
}

/* sin/cos are bit-exact only at the 64 probed points, every 1/64 turn. */
static void test_sin_matches_probe_block_e(void) {
    /* Block E (sin) of tools/pico8_reference_probe_out.txt: copied from the
     * probe, never derived. The arguments step by 1/64 turn, raw 0x400. */
    static const struct { uint32_t raw; uint32_t expected; } probe[] = {
        {0x00000000u, 0x00000000u}, {0x00000400u, 0xFFFFE6E8u}, {0x00000800u, 0xFFFFCE0Fu},
        {0x00000C00u, 0xFFFFB5B0u}, {0x00001000u, 0xFFFF9E08u}, {0x00001400u, 0xFFFF8753u},
        {0x00001800u, 0xFFFF71C6u}, {0x00001C00u, 0xFFFF5D98u}, {0x00002000u, 0xFFFF4AFBu},
        {0x00002400u, 0xFFFF3A1Cu}, {0x00002800u, 0xFFFF2B25u}, {0x00002C00u, 0xFFFF1E3Au},
        {0x00003000u, 0xFFFF137Du}, {0x00003400u, 0xFFFF0B06u}, {0x00003800u, 0xFFFF04EBu},
        {0x00003C00u, 0xFFFF013Cu}, {0x00004000u, 0xFFFF0000u}, {0x00004400u, 0xFFFF013Cu},
        {0x00004800u, 0xFFFF04EBu}, {0x00004C00u, 0xFFFF0B06u}, {0x00005000u, 0xFFFF137Du},
        {0x00005400u, 0xFFFF1E3Au}, {0x00005800u, 0xFFFF2B25u}, {0x00005C00u, 0xFFFF3A1Cu},
        {0x00006000u, 0xFFFF4AFBu}, {0x00006400u, 0xFFFF5D98u}, {0x00006800u, 0xFFFF71C6u},
        {0x00006C00u, 0xFFFF8753u}, {0x00007000u, 0xFFFF9E08u}, {0x00007400u, 0xFFFFB5B0u},
        {0x00007800u, 0xFFFFCE0Fu}, {0x00007C00u, 0xFFFFE6E8u}, {0x00008000u, 0x00000000u},
        {0x00008400u, 0x00001918u}, {0x00008800u, 0x000031F1u}, {0x00008C00u, 0x00004A50u},
        {0x00009000u, 0x000061F8u}, {0x00009400u, 0x000078ADu}, {0x00009800u, 0x00008E3Au},
        {0x00009C00u, 0x0000A268u}, {0x0000A000u, 0x0000B505u}, {0x0000A400u, 0x0000C5E4u},
        {0x0000A800u, 0x0000D4DBu}, {0x0000AC00u, 0x0000E1C6u}, {0x0000B000u, 0x0000EC83u},
        {0x0000B400u, 0x0000F4FAu}, {0x0000B800u, 0x0000FB15u}, {0x0000BC00u, 0x0000FEC4u},
        {0x0000C000u, 0x00010000u}, {0x0000C400u, 0x0000FEC4u}, {0x0000C800u, 0x0000FB15u},
        {0x0000CC00u, 0x0000F4FAu}, {0x0000D000u, 0x0000EC83u}, {0x0000D400u, 0x0000E1C6u},
        {0x0000D800u, 0x0000D4DBu}, {0x0000DC00u, 0x0000C5E4u}, {0x0000E000u, 0x0000B505u},
        {0x0000E400u, 0x0000A268u}, {0x0000E800u, 0x00008E3Au}, {0x0000EC00u, 0x000078ADu},
        {0x0000F000u, 0x000061F8u}, {0x0000F400u, 0x00004A50u}, {0x0000F800u, 0x000031F1u},
        {0x0000FC00u, 0x00001918u},
    };
    for (size_t i = 0; i < sizeof probe / sizeof probe[0]; i++) {
        assert(p8_fixed_sin((p8_fixed_t)probe[i].raw) == (p8_fixed_t)probe[i].expected);
    }
}

static void test_cos_matches_probe_block_e(void) {
    /* Block E (cos) of tools/pico8_reference_probe_out.txt: copied from the
     * probe, never derived. The arguments step by 1/64 turn, raw 0x400. */
    static const struct { uint32_t raw; uint32_t expected; } probe[] = {
        {0x00000000u, 0x00010000u}, {0x00000400u, 0x0000FEC4u}, {0x00000800u, 0x0000FB15u},
        {0x00000C00u, 0x0000F4FAu}, {0x00001000u, 0x0000EC83u}, {0x00001400u, 0x0000E1C6u},
        {0x00001800u, 0x0000D4DBu}, {0x00001C00u, 0x0000C5E4u}, {0x00002000u, 0x0000B505u},
        {0x00002400u, 0x0000A268u}, {0x00002800u, 0x00008E3Au}, {0x00002C00u, 0x000078ADu},
        {0x00003000u, 0x000061F8u}, {0x00003400u, 0x00004A50u}, {0x00003800u, 0x000031F1u},
        {0x00003C00u, 0x00001918u}, {0x00004000u, 0x00000000u}, {0x00004400u, 0xFFFFE6E8u},
        {0x00004800u, 0xFFFFCE0Fu}, {0x00004C00u, 0xFFFFB5B0u}, {0x00005000u, 0xFFFF9E08u},
        {0x00005400u, 0xFFFF8753u}, {0x00005800u, 0xFFFF71C6u}, {0x00005C00u, 0xFFFF5D98u},
        {0x00006000u, 0xFFFF4AFBu}, {0x00006400u, 0xFFFF3A1Cu}, {0x00006800u, 0xFFFF2B25u},
        {0x00006C00u, 0xFFFF1E3Au}, {0x00007000u, 0xFFFF137Du}, {0x00007400u, 0xFFFF0B06u},
        {0x00007800u, 0xFFFF04EBu}, {0x00007C00u, 0xFFFF013Cu}, {0x00008000u, 0xFFFF0000u},
        {0x00008400u, 0xFFFF013Cu}, {0x00008800u, 0xFFFF04EBu}, {0x00008C00u, 0xFFFF0B06u},
        {0x00009000u, 0xFFFF137Du}, {0x00009400u, 0xFFFF1E3Au}, {0x00009800u, 0xFFFF2B25u},
        {0x00009C00u, 0xFFFF3A1Cu}, {0x0000A000u, 0xFFFF4AFBu}, {0x0000A400u, 0xFFFF5D98u},
        {0x0000A800u, 0xFFFF71C6u}, {0x0000AC00u, 0xFFFF8753u}, {0x0000B000u, 0xFFFF9E08u},
        {0x0000B400u, 0xFFFFB5B0u}, {0x0000B800u, 0xFFFFCE0Fu}, {0x0000BC00u, 0xFFFFE6E8u},
        {0x0000C000u, 0x00000000u}, {0x0000C400u, 0x00001918u}, {0x0000C800u, 0x000031F1u},
        {0x0000CC00u, 0x00004A50u}, {0x0000D000u, 0x000061F8u}, {0x0000D400u, 0x000078ADu},
        {0x0000D800u, 0x00008E3Au}, {0x0000DC00u, 0x0000A268u}, {0x0000E000u, 0x0000B505u},
        {0x0000E400u, 0x0000C5E4u}, {0x0000E800u, 0x0000D4DBu}, {0x0000EC00u, 0x0000E1C6u},
        {0x0000F000u, 0x0000EC83u}, {0x0000F400u, 0x0000F4FAu}, {0x0000F800u, 0x0000FB15u},
        {0x0000FC00u, 0x0000FEC4u},
    };
    for (size_t i = 0; i < sizeof probe / sizeof probe[0]; i++) {
        assert(p8_fixed_cos((p8_fixed_t)probe[i].raw) == (p8_fixed_t)probe[i].expected);
    }
}

static const uint32_t sin_cos_sample_fractions[] = {
    0x0001u, 0x1234u, 0x3FFFu, 0x4001u, 0x5555u, 0x8001u, 0x9ABCu, 0xC3A1u, 0xFFFFu,
};

static void test_sin_and_cos_period_is_one_turn_for_any_whole_part(void) {
    static const int32_t whole_parts[] = {-32768, -1, 1, 2, 100, 32767};
    for (size_t w = 0; w < sizeof whole_parts / sizeof whole_parts[0]; w++) {
        for (size_t f = 0; f < sizeof sin_cos_sample_fractions / sizeof sin_cos_sample_fractions[0]; f++) {
            p8_fixed_t frac = (p8_fixed_t)sin_cos_sample_fractions[f];
            p8_fixed_t x = p8_fixed_add(p8_fixed_from_int(whole_parts[w]), frac);
            assert(p8_fixed_sin(x) == p8_fixed_sin(frac));
            assert(p8_fixed_cos(x) == p8_fixed_cos(frac));
        }
    }
}

static void test_sin_is_odd_and_cos_is_even(void) {
    for (size_t f = 0; f < sizeof sin_cos_sample_fractions / sizeof sin_cos_sample_fractions[0]; f++) {
        p8_fixed_t x = (p8_fixed_t)sin_cos_sample_fractions[f];
        p8_fixed_t minus_x = p8_fixed_neg(x);
        assert(p8_fixed_sin(minus_x) == p8_fixed_neg(p8_fixed_sin(x)));
        assert(p8_fixed_cos(minus_x) == p8_fixed_cos(x));
    }
}

/* The pre-table sin/cos, kept here verbatim (same 2*pi, same rounding as
 * fixed_test_double_from_double) so the generated table stays pinned to it. */
#define OLD_TWO_PI 6.28318530717958647692528676655900576839433879875021

static p8_fixed_t old_round_to_fixed(double d) {
    double scaled = d * 65536.0;
    int64_t raw = (int64_t)(scaled >= 0 ? scaled + 0.5 : scaled - 0.5);
    return (p8_fixed_t)(uint32_t)raw;
}

static double old_radians(p8_fixed_t x) {
    return ((double)x / 65536.0) * OLD_TWO_PI;
}

static void test_sin_and_cos_match_old_computation_on_every_fraction(void) {
    for (uint32_t f = 0; f <= 0xFFFFu; f++) {
        p8_fixed_t x = (p8_fixed_t)f;
        assert(p8_fixed_sin(x) == old_round_to_fixed(-sin(old_radians(x))));
        assert(p8_fixed_cos(x) == old_round_to_fixed(cos(old_radians(x))));
    }
}

static void test_sin_zero_is_zero(void) {
    assert(p8_fixed_sin(p8_fixed_from_int(0)) == p8_fixed_from_int(0));
}

static void test_cos_zero_is_one(void) {
    assert(p8_fixed_cos(p8_fixed_from_int(0)) == p8_fixed_from_int(1));
}

static void test_sin_quarter_turn_sign_is_inverted(void) {
    /* A quarter turn is a documented identity: standard math has
     * sin(pi/2) == 1, but PICO-8's sin() inverts the sign for
     * screen-space y, so sin(0.25 turns) == -1 exactly -- this pins down
     * the sign-inversion convention itself, not an arbitrary computed
     * decimal. */
    p8_fixed_t quarter_turn = fixed_test_double_from_double(0.25);
    assert(p8_fixed_sin(quarter_turn) == p8_fixed_from_int(-1));
}

static void test_cos_quarter_turn_is_zero(void) {
    /* Another documented identity: cos(pi/2) == 0, unaffected by sin's
     * sign inversion since cos follows the standard convention. */
    p8_fixed_t quarter_turn = fixed_test_double_from_double(0.25);
    assert(p8_fixed_cos(quarter_turn) == p8_fixed_from_int(0));
}

static void test_cos_half_turn_is_negative_one(void) {
    p8_fixed_t half_turn = fixed_test_double_from_double(0.5);
    assert(p8_fixed_cos(half_turn) == p8_fixed_from_int(-1));
}

static void test_sin_and_cos_periodic_at_whole_turn_boundary(void) {
    /* A full turn (1.0) is the same rotation as no turn (0.0): sin/cos
     * must agree at that whole-turn boundary. */
    p8_fixed_t zero_turns = p8_fixed_from_int(0);
    p8_fixed_t one_turn = p8_fixed_from_int(1);
    p8_fixed_t two_turns = p8_fixed_from_int(2);

    assert(p8_fixed_sin(one_turn) == p8_fixed_sin(zero_turns));
    assert(p8_fixed_cos(one_turn) == p8_fixed_cos(zero_turns));
    assert(p8_fixed_sin(two_turns) == p8_fixed_sin(zero_turns));
    assert(p8_fixed_cos(two_turns) == p8_fixed_cos(zero_turns));
}

/* The compile-time constant agrees with the function, negatives included. */
static void test_fixed_int_constant_matches_from_int(void) {
    static const p8_fixed_t constants[] = {P8_FIXED_INT(0), P8_FIXED_INT(1), P8_FIXED_INT(-1), P8_FIXED_INT(-3),
        P8_FIXED_INT(150), P8_FIXED_INT(32767), P8_FIXED_INT(-32768)};
    static const int32_t ints[] = {0, 1, -1, -3, 150, 32767, -32768};

    for (int i = 0; i < 7; i++) {
        assert(constants[i] == p8_fixed_from_int(ints[i]));
    }
}

int main(void) {
    test_fixed_int_constant_matches_from_int();
    test_sqrt_perfect_square();
    test_sqrt_perfect_square_fractional();
    test_sqrt_zero_is_zero();
    test_sqrt_negative_value_is_zero();
    test_sqrt_matches_probe_block_e();
    test_pow075_matches_probe_block_d();
    test_pow075_zero_is_zero();
    test_pow075_negative_value_is_zero();

    test_shl_positive_raw_operand();
    test_shl_negative_raw_operand();
    test_shl_by_zero_is_unchanged();

    test_band_positive_raw_operands();
    test_band_negative_raw_operand();
    test_bor_positive_raw_operands();
    test_bor_negative_raw_operand();

    test_mid_middle_argument_already_sorted();
    test_mid_is_order_independent();
    test_mid_clamps_value_below_range();
    test_mid_clamps_value_above_range();

    test_flr_positive_truncates_fraction();
    test_flr_negative_rounds_toward_negative_infinity();
    test_flr_exact_integer_is_unchanged();
    test_flr_int_matches_probe_block_b();
    test_lit_is_the_raw_probe_value();
    test_lit_never_evaluates_its_literal();
    test_flr_int_extremes();
    test_flr_int_raw_minus_one_is_minus_one_not_zero();

    test_mod_positive_operands();
    test_mod_fractional_operand();
    test_mod_negative_dividend_floors();
    test_mod_result_less_than_divisor_when_exact();

    test_abs_negative_value();
    test_abs_positive_value_is_unchanged();
    test_abs_zero_is_zero();
    test_abs_most_negative_value_wraps_to_itself();

    test_sgn_positive_value_is_one();
    test_sgn_negative_value_is_negative_one();
    test_sgn_zero_is_one_not_zero();

    test_add_representative_values();
    test_sub_representative_values();
    test_sub_wraps_at_most_negative_raw_value();
    test_sub_wraps_at_most_positive_raw_value();
    test_from_int_outside_the_integer_part_wraps();
    test_mul_representative_values();
    test_mul_overflow_wraps();
    test_add_overflow_128_squared_twice_wraps_to_min();
    test_div_representative_value();
    test_div_negative_dividend_nonzero_divisor();
    test_div_by_zero_positive_dividend_saturates_positive();
    test_div_by_zero_negative_dividend_saturates_negative();
    test_div_zero_by_zero_saturates_positive();
    test_neg_representative_value();
    test_neg_most_negative_value_wraps_to_itself();

    test_rnd_zero_is_zero();
    test_rnd_result_in_documented_range();
    test_rnd_default_behaves_as_rnd_one();
    test_rnd_state_set_is_read_back();
    test_rnd_draw_changes_the_state();
    test_rnd_restored_state_replays_the_same_draws();
    test_sin_zero_is_zero();
    test_cos_zero_is_one();
    test_sin_quarter_turn_sign_is_inverted();
    test_cos_quarter_turn_is_zero();
    test_cos_half_turn_is_negative_one();
    test_sin_and_cos_periodic_at_whole_turn_boundary();
    test_sin_matches_probe_block_e();
    test_cos_matches_probe_block_e();
    test_sin_and_cos_period_is_one_turn_for_any_whole_part();
    test_sin_is_odd_and_cos_is_even();
    test_sin_and_cos_match_old_computation_on_every_fraction();

    printf("test_fixed: all tests passed\n");
    return 0;
}
