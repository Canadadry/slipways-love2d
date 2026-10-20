/* tools/float_guard.sh, the body of `make float-guard`, on built sources:
 * it is tested by what it must prevent, a floating-point file that the
 * guard would let through. */

#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define DIR "tmp/test_float_guard_work"
#define FLOATING DIR "/floating.c"
#define INTEGER DIR "/integer.c"
#define OUT DIR "/out.txt"
#define GUARD "sh tools/float_guard.sh cc '-std=c99 -Wall -Wextra -Werror' " DIR "/obj"

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

/* The exit code of the script, read back from a file: the raw status of
 * system() cannot be decoded without <sys/wait.h>, outside C99. */
static int run_guard(const char *sources) {
    char cmd[1024];
    snprintf(cmd, sizeof cmd, GUARD " %s > " OUT " 2>&1; echo $? > " DIR "/code", sources);
    assert(system(cmd) == 0);
    return atoi(read_file(DIR "/code"));
}

static void setup(void) {
    assert(system("rm -rf " DIR " && mkdir -p " DIR) == 0);
    write_file(FLOATING, "double half(int x) { return x / 2.0; }\n");
    write_file(INTEGER, "int half(int x) { return x / 2; }\n");
}

static void test_floating_file_is_listed_and_fails_the_guard(void) {
    setup();
    assert(run_guard(FLOATING " " INTEGER) == 1);
    const char *out = read_file(OUT);
    assert(strstr(out, "failed " FLOATING "\n") != NULL);
    assert(strstr(out, INTEGER) == NULL);
    assert(strstr(out, "float-guard files 2 failing 1\n") != NULL);
}

static void test_integer_file_alone_passes_the_guard(void) {
    setup();
    assert(run_guard(INTEGER) == 0);
    const char *out = read_file(OUT);
    assert(strstr(out, "\nfailed ") == NULL);
    assert(strstr(out, "float-guard files 1 failing 0\n") != NULL);
}

static void test_every_failing_file_is_listed_sorted(void) {
    setup();
    write_file(DIR "/another.c", "float third(void) { return 3.0f; }\n");
    assert(run_guard(FLOATING " " INTEGER " " DIR "/another.c") == 1);
    const char *out = read_file(OUT);
    const char *another = strstr(out, "failed " DIR "/another.c\n");
    const char *floating = strstr(out, "failed " FLOATING "\n");
    assert(another != NULL && floating != NULL && another < floating);
    assert(strstr(out, "float-guard files 3 failing 2\n") != NULL);
}

static void test_no_source_is_an_error_not_a_pass(void) {
    setup();
    assert(run_guard("") == 2);
}

static void test_two_runs_give_identical_reports(void) {
    setup();
    run_guard(FLOATING " " INTEGER);
    assert(system("cp " OUT " " DIR "/one.txt") == 0);
    run_guard(FLOATING " " INTEGER);
    assert(system("cmp -s " OUT " " DIR "/one.txt") == 0);
}

/* One guard run for three traps, each guard run being the cost of this test:
 * a path with spaces, two sources sharing a flat name ("with space_sub_x_y.c"),
 * and a warning flag the guard's own probe must satisfy too. */
static void test_awkward_sources_and_cflags_still_catch_floating_point(void) {
    setup();
    assert(system("mkdir -p '" DIR "/with space/sub_x' '" DIR "/with space/sub'") == 0);
    write_file(DIR "/with space/sub_x/y.c", "double half(int x);\ndouble half(int x) { return x / 2.0; }\n");
    write_file(DIR "/with space/sub/x_y.c", "int half(int x);\nint half(int x) { return x / 2; }\n");
    assert(system("sh tools/float_guard.sh cc '-std=c99 -Wmissing-prototypes -Werror' " DIR "/obj '" DIR
                  "/with space/sub_x/y.c' '" DIR "/with space/sub/x_y.c' > " OUT " 2>&1; echo $? > " DIR
                  "/code") == 0);
    assert(atoi(read_file(DIR "/code")) == 1);
    const char *out = read_file(OUT);
    assert(strstr(out, "failed " DIR "/with space/sub_x/y.c\n") != NULL);
    assert(strstr(out, "float-guard files 2 failing 1\n") != NULL);
}

/* The fake compiler drops -mgeneral-regs-only, so that GCC too takes the
 * disassembly path, the only one that runs objdump. No new executable: macOS
 * scans one on its first run, which costs this test 0.3 s; sh runs the
 * compiler, objdump is a link to true. */
static void test_objdump_printing_nothing_fails_the_guard(void) {
    setup();
    assert(system("mkdir -p " DIR "/bin && ln -s /usr/bin/true " DIR "/bin/objdump") == 0);
    write_file(DIR "/fake-cc", "for arg; do shift; [ \"$arg\" = -mgeneral-regs-only ] || set -- \"$@\" \"$arg\"; done\n"
                               "exec cc \"$@\"\n");
    assert(system("PATH=" DIR "/bin:$PATH sh tools/float_guard.sh 'sh " DIR "/fake-cc' -std=c99 " DIR "/obj " INTEGER
                  " > " OUT " 2>&1; echo $? > " DIR "/code") == 0);
    assert(atoi(read_file(DIR "/code")) == 2);
    assert(strstr(read_file(OUT), "failing 0") == NULL);
}

static void test_cflags_that_break_every_compile_are_an_error(void) {
    setup();
    assert(system("sh tools/float_guard.sh cc '-std=c99 -include " DIR "/missing.h' " DIR "/obj " INTEGER
                  " > " OUT " 2>&1; echo $? > " DIR "/code") == 0);
    assert(atoi(read_file(DIR "/code")) == 2);
}

int main(void) {
    test_floating_file_is_listed_and_fails_the_guard();
    test_integer_file_alone_passes_the_guard();
    test_every_failing_file_is_listed_sorted();
    test_no_source_is_an_error_not_a_pass();
    test_two_runs_give_identical_reports();
    test_awkward_sources_and_cflags_still_catch_floating_point();
    test_objdump_printing_nothing_fails_the_guard();
    test_cflags_that_break_every_compile_are_an_error();
    assert(system("rm -rf " DIR) == 0);
    printf("test_float_guard: all tests passed\n");
    return 0;
}
