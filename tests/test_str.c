#include "p8/str.h"
#include "heap_allocator.h"

#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static void test_sub_first_character(void) {
    char *r = p8_sub(&heap_allocator, "hello", 1, 1);
    assert(strcmp(r, "h") == 0);
    free(r);
}

static void test_sub_last_character(void) {
    char *r = p8_sub(&heap_allocator, "hello", 5, 5);
    assert(strcmp(r, "o") == 0);
    free(r);
}

static void test_sub_full_string(void) {
    char *r = p8_sub(&heap_allocator, "hello", 1, 5);
    assert(strcmp(r, "hello") == 0);
    free(r);
}

static void test_sub_middle_range(void) {
    char *r = p8_sub(&heap_allocator, "hello", 2, 4);
    assert(strcmp(r, "ell") == 0);
    free(r);
}

static void test_sub_empty_range_when_j_less_than_i(void) {
    char *r = p8_sub(&heap_allocator, "hello", 3, 2);
    assert(strcmp(r, "") == 0);
    free(r);
}

static void test_tostr_small_positive_integer(void) {
    char *s = p8_tostr_fixed(&heap_allocator, p8_fixed_from_int(3));
    assert(strcmp(s, "3") == 0);
    free(s);
}

static void test_tostr_zero(void) {
    char *s = p8_tostr_fixed(&heap_allocator, p8_fixed_from_int(0));
    assert(strcmp(s, "0") == 0);
    free(s);
}

static void test_tostr_negative_integer(void) {
    char *s = p8_tostr_fixed(&heap_allocator, p8_fixed_from_int(-5));
    assert(strcmp(s, "-5") == 0);
    free(s);
}

static p8_fixed_t raw_fixed(uint32_t raw) {
    return (p8_fixed_t)raw;
}

static void assert_tostr_raw(uint32_t raw, const char *expected) {
    char *s = p8_tostr_fixed(&heap_allocator, raw_fixed(raw));
    if (strcmp(s, expected) != 0) {
        fprintf(stderr, "tostr 0x%08x: got \"%s\", expected \"%s\"\n",
                (unsigned)raw, s, expected);
    }
    assert(strcmp(s, expected) == 0);
    free(s);
}

static void test_tostr_matches_probe_block_c(void) {
    assert_tostr_raw(0xfffe8000u, "-1.5");
    assert_tostr_raw(0x00010000u, "1");
    assert_tostr_raw(0x00000000u, "0");
    assert_tostr_raw(0x00002666u, "0.15");
    assert_tostr_raw(0x0000451eu, "0.27");
    assert_tostr_raw(0x00001999u, "0.1");
    assert_tostr_raw(0x00028000u, "2.5");
    assert_tostr_raw(0x00640000u, "100");
    assert_tostr_raw(0x03e80000u, "1000");
    assert_tostr_raw(0x80000000u, "-32768");
    assert_tostr_raw(0x7fff0000u, "32767");
    assert_tostr_raw(0x00005555u, "0.3333");
    assert_tostr_raw(0x0000aaaau, "0.6667");
}

static void test_tostr_below_0_0001_keeps_port_output(void) {
    char *s = p8_tostr_fixed(&heap_allocator, raw_fixed(0xfffffffau));
    assert(strcmp(s, "-0.0001") == 0
           && "PICO-8 prints \"-0\" for raw -6; the port keeps \"-0.0001\"");
    free(s);
}

static void test_tostr_exact_ties_round_half_to_even(void) {
    assert_tostr_raw(0x00000800u, "0.0312");
    assert_tostr_raw(0x00001800u, "0.0938");
}

/* The former tostr, byte for byte, kept as the oracle of the exhaustive
 * test: host glibc "%.4f" rounds the exact binary value half to even. */
static void old_tostr_percent_4f_for_test(int32_t raw, char *buf, size_t size) {
    double d = raw / 65536.0;
    snprintf(buf, size, "%.4f", d);
    char *dot = strchr(buf, '.');
    if (dot) {
        char *end = buf + strlen(buf) - 1;
        while (end > dot && *end == '0') {
            *end-- = '\0';
        }
        if (end == dot) {
            *end = '\0';
        }
    }
}

static int check_against_old_tostr(int32_t raw) {
    char old[64];
    old_tostr_percent_4f_for_test(raw, old, sizeof old);
    char *s = p8_tostr_fixed(&heap_allocator, raw);
    int same = strcmp(s, old) == 0;
    if (!same) {
        fprintf(stderr, "tostr 0x%08x: old \"%s\", new \"%s\"\n",
                (unsigned)(uint32_t)raw, old, s);
    }
    free(s);
    return same;
}

static int is_exact_4_decimal_tie(int32_t raw) {
    uint32_t mag = raw < 0 ? 0u - (uint32_t)raw : (uint32_t)raw;
    return (((mag & 0xFFFFu) * 625u) & 4095u) == 2048u;
}

static void test_tostr_matches_old_percent_4f_exhaustively(void) {
    static const int32_t int_parts[] = {
        0, 1, 2, 9, 10, 99, 100, 999, 1000, 9999, 10000, 32766, 32767,
        -1, -2, -10, -32767, -32768,
    };
    const size_t n_int_parts = sizeof int_parts / sizeof int_parts[0];
    long inputs = 0;
    long ties = 0;

    for (size_t i = 0; i < n_int_parts; i++) {
        for (uint32_t f = 0; f <= 0xFFFFu; f++) {
            int32_t raw = (int32_t)(((uint32_t)int_parts[i] << 16) | f);
            assert(check_against_old_tostr(raw));
            ties += is_exact_4_decimal_tie(raw);
            inputs++;
        }
    }
    for (int32_t raw = -1; raw >= -65535; raw--) {
        assert(check_against_old_tostr(raw));
        ties += is_exact_4_decimal_tie(raw);
        inputs++;
    }

    assert(inputs == 1245183);
    /* 16 ties per integer part, the negative pure fractions counting as one. */
    assert(ties == 16 * (long)(n_int_parts + 1));
}

int main(void) {
    test_sub_first_character();
    test_sub_last_character();
    test_sub_full_string();
    test_sub_middle_range();
    test_sub_empty_range_when_j_less_than_i();
    test_tostr_small_positive_integer();
    test_tostr_zero();
    test_tostr_negative_integer();
    test_tostr_matches_probe_block_c();
    test_tostr_below_0_0001_keeps_port_output();
    test_tostr_exact_ties_round_half_to_even();
    test_tostr_matches_old_percent_4f_exhaustively();

    printf("test_str: all tests passed\n");
    return 0;
}
