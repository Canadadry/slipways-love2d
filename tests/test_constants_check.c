/* tools/constants_compare.sh and tools/constants_check.sh, on built sources,
 * reference and expected-gap list, with the real src/p8/fixed.c: they are
 * tested by what they must prevent. */

#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define DIR "tmp/test_constants_check_work"
#define PROBE DIR "/reference.txt"
#define EXPECTED DIR "/expected.txt"
#define WORK DIR "/gen"
#define OUT DIR "/out.txt"
#define FIXED "src/p8/fixed.c"

#define SRC_A DIR "/a.c"
#define SRC_B DIR "/b.c"
#define SRCS " " SRC_A " " SRC_B
#define COMPARE "sh tools/constants_compare.sh " PROBE " " FIXED " " WORK SRCS
#define CHECK "sh tools/constants_check.sh " EXPECTED " " PROBE " " FIXED " " WORK SRCS

/* 0.27: PICO-8 truncates to 0x451e, rounding to nearest gives 0x451f. */
#define SRC_A_TEXT "x = P8_FIXED_LIT(0x00008000, 0.5); y = P8_FIXED_LIT(0x0000451f, 0.27);\n"
#define SRC_B_TEXT "z = P8_FIXED_LIT(0x0000451f, 0.27);\n"

#define PROBE_TEXT \
    "# probe v1\n" \
    "## a literals\n" \
    "0.27 0x0000.451e\n" \
    "0.5 0x0000.8000\n" \
    "## b rounding\n" \
    "flr 0xfffe.8000 -> 0xfffe.0000\n"

/* Static buffer, NUL-terminated, overwritten by each call. */
static const char *read_file(const char *path) {
    static char buf[1 << 16];
    FILE *file = fopen(path, "rb");
    assert(file != NULL);
    size_t len = fread(buf, 1, sizeof(buf) - 1, file);
    fclose(file);
    buf[len] = '\0';
    return buf;
}

static void write_file(const char *path, const char *text) {
    FILE *file = fopen(path, "w");
    assert(file != NULL);
    fputs(text, file);
    fclose(file);
}

static void reset(void) {
    assert(system("rm -rf " DIR " && mkdir -p " DIR) == 0);
    write_file(SRC_A, SRC_A_TEXT);
    write_file(SRC_B, SRC_B_TEXT);
    write_file(PROBE, PROBE_TEXT);
    write_file(EXPECTED, "0.27\n");
}

static int run(const char *cmd) {
    char full[1024];
    snprintf(full, sizeof full, "%s > " OUT " 2>&1", cmd);
    return system(full);
}

static void test_table_marks_exact_ok_and_truncated_gap(void) {
    reset();
    assert(run(COMPARE) == 0);
    const char *out = read_file(OUT);
    assert(strstr(out, "0.5 | 0x0000.8000 | 0x0000.8000 | ok | 1\n") != NULL);
    assert(strstr(out, "0.27 | 0x0000.451e | 0x0000.451f | GAP | 2\n") != NULL);
    assert(strstr(out, "literals 2 | sites 3 | ok 1 | gaps 1\n") != NULL);
}

static void test_expected_list_equal_to_gaps_passes(void) {
    reset();
    assert(run(CHECK) == 0);
    assert(strstr(read_file(OUT), "literals 2 | sites 3 | ok 1 | gaps 1\n") != NULL);
}

/* 0.5 is exact: listing it makes the list stale. */
static void test_listed_literal_that_passes_fails_as_stale(void) {
    reset();
    write_file(EXPECTED, "0.27\n0.5\n");
    assert(run(CHECK) != 0);
    const char *out = read_file(OUT);
    assert(strstr(out, "stale list: remove 0.5") != NULL);
    assert(strstr(out, "regression") == NULL);
}

static void test_unlisted_gap_fails_as_regression(void) {
    reset();
    write_file(EXPECTED, "");
    assert(run(CHECK) != 0);
    const char *out = read_file(OUT);
    assert(strstr(out, "regression: 0.27") != NULL);
    assert(strstr(out, "stale") == NULL);
}

static void test_listed_literal_absent_from_sources_fails(void) {
    reset();
    write_file(EXPECTED, "0.27\n0.19\n");
    assert(run(CHECK) != 0);
    assert(strstr(read_file(OUT), "0.19") != NULL);
}

static void test_literal_absent_from_block_a_fails(void) {
    reset();
    write_file(SRC_B, SRC_B_TEXT "w = P8_FIXED_LIT(0x000030a3, 0.19);\n");
    assert(run(COMPARE) != 0);
    assert(strstr(read_file(OUT), "0.19") != NULL);
    assert(run(CHECK) != 0);
}

static void test_missing_block_a_fails(void) {
    reset();
    write_file(PROBE, "# probe v1\n0.27 0x0000.451e\n0.5 0x0000.8000\n");
    assert(run(COMPARE) != 0);
    assert(strstr(read_file(OUT), "block A") != NULL);
}

/* The block after block A has a line of the same shape: read as a literal,
 * it would make 0.27 exact. */
static void test_block_after_a_is_not_read_as_literals(void) {
    reset();
    write_file(PROBE, PROBE_TEXT "## c tostr\n0.27 0x0000.451f\n");
    assert(run(COMPARE) == 0);
    assert(strstr(read_file(OUT), "0.27 | 0x0000.451e | 0x0000.451f | GAP | 2\n") != NULL);
}

static void write_sources(const char *a, const char *b) {
    write_file(SRC_A, a);
    write_file(SRC_B, b);
}

static void test_macro_sites_are_read_from_the_sources(void) {
    reset();
    write_sources("x = P8_FIXED_LIT(0x00008000, 0.5);\n",
                  "y = f(P8_FIXED_LIT(0x0000451f, 0.27), 2);\n");
    assert(run(COMPARE) == 0);
    const char *out = read_file(OUT);
    assert(strstr(out, "0.5 | 0x0000.8000 | 0x0000.8000 | ok | 1\n") != NULL);
    assert(strstr(out, "0.27 | 0x0000.451e | 0x0000.451f | GAP | 1\n") != NULL);
    assert(strstr(out, "literals 2 | sites 2 | ok 1 | gaps 1\n") != NULL);
}

/* A right site does not cover its wrong sibling. */
static void test_two_sites_of_one_literal_with_two_values_are_gap(void) {
    reset();
    write_sources("x = P8_FIXED_LIT(0x0000451e, 0.27);\n",
                  "y = P8_FIXED_LIT(0x0000451f, 0.27);\n");
    assert(run(COMPARE) == 0);
    const char *out = read_file(OUT);
    assert(strstr(out, "0.27 | 0x0000.451e | 0x0000.451e,0x0000.451f | GAP | 2\n") != NULL);
    assert(strstr(out, "literals 1 | sites 2 | ok 0 | gaps 1\n") != NULL);
}

static void test_malformed_macro_call_fails_naming_its_file(void) {
    static const char *const bad[] = {
        "x = P8_FIXED_LIT(0x000451e, 0.27);\n",
        "x = P8_FIXED_LIT(0x0000451e, lit);\n",
        "x = P8_FIXED_LIT(0x0000451E, 0.27);\n",
        "x = P8_FIXED_LIT(17694, 0.27);\n",
    };
    for (size_t i = 0; i < sizeof bad / sizeof bad[0]; i++) {
        reset();
        write_sources("x = P8_FIXED_LIT(0x00008000, 0.5);\n", bad[i]);
        assert(run(COMPARE) != 0);
        assert(strstr(read_file(OUT), SRC_B ":1") != NULL);
        assert(run(CHECK) != 0);
    }
}

static void test_macro_literal_absent_from_block_a_fails(void) {
    reset();
    write_sources("x = P8_FIXED_LIT(0x00008000, 0.5);\n",
                  "y = P8_FIXED_LIT(0x000030a3, 0.19);\n");
    assert(run(COMPARE) != 0);
    assert(strstr(read_file(OUT), "0.19") != NULL);
}

static void test_no_constant_under_any_form_fails(void) {
    reset();
    write_sources("x = 1;\n", "y = 2;\n");
    write_file(EXPECTED, "");
    assert(run(COMPARE) != 0);
    assert(strstr(read_file(OUT), "no constant") != NULL);
    assert(run(CHECK) != 0);
}

static void test_no_source_fails(void) {
    reset();
    assert(run("sh tools/constants_compare.sh " PROBE " " FIXED " " WORK) != 0);
    assert(strstr(read_file(OUT), "no source") != NULL);
}

static void test_unlisted_macro_gap_fails_as_regression(void) {
    reset();
    write_sources("x = P8_FIXED_LIT(0x00008000, 0.5);\n",
                  "y = P8_FIXED_LIT(0x0000451f, 0.27);\n");
    assert(run(CHECK) == 0);
    assert(strstr(read_file(OUT), "literals 2 | sites 2 | ok 1 | gaps 1\n") != NULL);
    write_file(EXPECTED, "");
    assert(run(CHECK) != 0);
    assert(strstr(read_file(OUT), "regression: 0.27") != NULL);
}

int main(void) {
    test_table_marks_exact_ok_and_truncated_gap();
    test_expected_list_equal_to_gaps_passes();
    test_listed_literal_that_passes_fails_as_stale();
    test_unlisted_gap_fails_as_regression();
    test_listed_literal_absent_from_sources_fails();
    test_literal_absent_from_block_a_fails();
    test_missing_block_a_fails();
    test_block_after_a_is_not_read_as_literals();
    test_macro_sites_are_read_from_the_sources();
    test_two_sites_of_one_literal_with_two_values_are_gap();
    test_malformed_macro_call_fails_naming_its_file();
    test_macro_literal_absent_from_block_a_fails();
    test_no_constant_under_any_form_fails();
    test_no_source_fails();
    test_unlisted_macro_gap_fails_as_regression();
    assert(system("rm -rf " DIR) == 0);
    printf("test_constants_check: all tests passed\n");
    return 0;
}
