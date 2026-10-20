#include "p8/format.h"

#include <assert.h>
#include <limits.h>
#include <stdio.h>
#include <string.h>

enum { CAP = 64 };

static void check_int(const char *format, int value) {
    char expected[CAP];
    char actual[CAP];
    int expected_length = snprintf(expected, sizeof expected, format, value);
    int actual_length = p8_snprintf(actual, sizeof actual, format, value);
    assert(strcmp(actual, expected) == 0);
    assert(actual_length == expected_length);
}

static void check_long(const char *format, long value) {
    char expected[CAP];
    char actual[CAP];
    snprintf(expected, sizeof expected, format, value);
    p8_snprintf(actual, sizeof actual, format, value);
    assert(strcmp(actual, expected) == 0);
}

static void check_unsigned_long(const char *format, unsigned long value) {
    char expected[CAP];
    char actual[CAP];
    snprintf(expected, sizeof expected, format, value);
    p8_snprintf(actual, sizeof actual, format, value);
    assert(strcmp(actual, expected) == 0);
}

static void test_signed_conversions_match_libc(void) {
    static const int values[] = { 0, 1, -1, 7, -45, 123, 99999, -123456, 276447231, 276447232,
                                  1000000000, 2147483647, INT_MIN };
    static const char *const formats[] = { "%d", "%i", "%5d", "%-5d|", "%05d", "%02d", "%u", "%x",
                                           "%04x", "%X", "%c" };
    for (size_t f = 0; f < sizeof formats / sizeof formats[0]; f++) {
        for (size_t v = 0; v < sizeof values / sizeof values[0]; v++) {
            if (formats[f][1] == 'c' && (values[v] < 32 || values[v] > 126)) continue;
            check_int(formats[f], values[v]);
        }
    }
}

static void test_long_conversions_match_libc(void) {
    static const long values[] = { 0L, -45L, 7L, 123L, 2147483647L, -2147483647L - 1 };
    for (size_t v = 0; v < sizeof values / sizeof values[0]; v++) {
        check_long("%ld", values[v]);
        check_long("%02ld", values[v]);
    }
    static const unsigned long unsigned_values[] = { 0UL, 7UL, 12345UL, 276447232UL, 4294967295UL };
    for (size_t v = 0; v < sizeof unsigned_values / sizeof unsigned_values[0]; v++) {
        check_unsigned_long("%lu", unsigned_values[v]);
        check_unsigned_long("%04lu", unsigned_values[v]);
        check_unsigned_long("%lx", unsigned_values[v]);
    }
}

static void test_strings_and_percent_match_libc(void) {
    char actual[CAP];
    p8_snprintf(actual, sizeof actual, "[%s|%6s|%-6s|%.2s|%%]", "", "ab", "cd", "planet");
    assert(strcmp(actual, "[|    ab|cd    |pl|%]") == 0);
}

static void test_truncates_and_returns_the_full_length(void) {
    char actual[6];
    memset(actual, 'x', sizeof actual);
    int length = p8_snprintf(actual, sizeof actual, "planet %d", 42);
    assert(length == 9);
    assert(strcmp(actual, "plane") == 0);
    assert(p8_snprintf(NULL, 0, "%d", -123) == 4);
}

int main(void) {
    test_signed_conversions_match_libc();
    test_long_conversions_match_libc();
    test_strings_and_percent_match_libc();
    test_truncates_and_returns_the_full_length();
    return 0;
}
