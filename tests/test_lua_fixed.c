#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define DIR "tmp/test_lua_fixed_work"
#define REFERENCE DIR "/reference.txt"
#define OUT DIR "/out.txt"

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

/* The exit code is read back from a file: the raw status of system() needs
 * <sys/wait.h>. */
static int run_on(const char *reference) {
    write_file(REFERENCE, reference);
    assert(system("luajit tests/lua/test_fixed.lua " REFERENCE " > " OUT " 2>&1; echo $? > " DIR "/code") == 0);
    return atoi(read_file(DIR "/code"));
}

static int out_has(const char *text) {
    return strstr(read_file(OUT), text) != NULL;
}

static void test_a_differing_record_names_inputs_expected_and_got(void) {
    assert(run_on("p8_fixed_add 00010000 00008000 = 00018001\nend 1\n") == 1);
    assert(out_has("p8_fixed_add(0x00010000, 0x00008000): expected 0x00018001, got 0x00018000\n"));
    assert(out_has("test_fixed: p8_fixed_add: 1 of 1 records differ\n"));
}

static void test_at_most_five_differing_records_are_printed_per_name(void) {
    assert(run_on("p8_fixed_neg 00000001 = 00000001\np8_fixed_neg 00000002 = 00000002\n"
                  "p8_fixed_neg 00000003 = 00000003\np8_fixed_neg 00000004 = 00000004\n"
                  "p8_fixed_neg 00000005 = 00000005\np8_fixed_neg 00000006 = 00000006\nend 6\n") == 1);
    assert(out_has("p8_fixed_neg(0x00000005)"));
    assert(!out_has("p8_fixed_neg(0x00000006)"));
    assert(out_has("test_fixed: p8_fixed_neg: 6 of 6 records differ\n"));
}

static void test_a_name_without_records_is_a_missing_section(void) {
    assert(run_on("# comment\nP8_FIXED_SHIFT = 00000010\nend 1\n") == 1);
    assert(out_has("test_fixed: P8_FIXED_DIV0_POS: section missing from the reference\n"));
    assert(out_has("test_fixed: p8_fixed_rnd_state: section missing from the reference\n"));
    assert(!out_has("P8_FIXED_SHIFT"));
}

static void test_an_unknown_name_fails_naming_the_line(void) {
    assert(run_on("P8_FIXED_SHIFT = 00000010\np8_fixed_cbrt 00000001 = 00000001\nend 2\n") == 1);
    assert(out_has("test_fixed: malformed reference line 2: unknown name p8_fixed_cbrt\n"));
}

static void test_a_wrong_word_count_or_word_fails_naming_the_line(void) {
    assert(run_on("p8_fixed_add 00000001 = 00000001\nend 1\n") == 1);
    assert(out_has("test_fixed: malformed reference line 1: p8_fixed_add takes 2 inputs and 1 outputs, each an 8-digit hex word\n"));
    assert(run_on("p8_fixed_neg 00000001 00000002 00000003\nend 1\n") == 1);
    assert(out_has("test_fixed: malformed reference line 1: p8_fixed_neg takes 1 inputs and 1 outputs, each an 8-digit hex word\n"));
    assert(run_on("p8_fixed_neg 0000001 = ffffffff\nend 1\n") == 1);
    assert(out_has("test_fixed: malformed reference line 1: p8_fixed_neg takes 1 inputs and 1 outputs, each an 8-digit hex word\n"));
    assert(run_on("\nend 0\n") == 1);
    assert(out_has("test_fixed: malformed reference line 1: unknown name nil\n"));
}

static void test_a_missing_or_wrong_trailer_or_a_line_after_it_fails(void) {
    assert(run_on("P8_FIXED_SHIFT = 00000010\n") == 1);
    assert(out_has("test_fixed: malformed reference: no trailer line\n"));
    assert(run_on("P8_FIXED_SHIFT = 00000010\nend 2\n") == 1);
    assert(out_has("test_fixed: malformed reference: trailer says 2 records, read 1\n"));
    assert(run_on("P8_FIXED_SHIFT = 00000010\nend one\n") == 1);
    assert(out_has("test_fixed: malformed reference line 2: trailer is not \"end <record count>\"\n"));
    assert(run_on("end 0\nP8_FIXED_SHIFT = 00000010\n") == 1);
    assert(out_has("test_fixed: malformed reference line 2: after the trailer\n"));
}

int main(void) {
    assert(system("mkdir -p " DIR) == 0);
    test_a_differing_record_names_inputs_expected_and_got();
    test_at_most_five_differing_records_are_printed_per_name();
    test_a_name_without_records_is_a_missing_section();
    test_an_unknown_name_fails_naming_the_line();
    test_a_wrong_word_count_or_word_fails_naming_the_line();
    test_a_missing_or_wrong_trailer_or_a_line_after_it_fails();
    puts("test_lua_fixed: ok");
    return 0;
}
