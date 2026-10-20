#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define DIR "tmp/test_button_guard_work"
#define SRC DIR "/src"
#define OUT DIR "/out.txt"
#define GUARD "sh tools/button_guard.sh " SRC

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

static int run(const char *command) {
    char line[512];
    snprintf(line, sizeof(line), "%s > " OUT " 2>&1; echo $? > " DIR "/code", command);
    assert(system(line) == 0);
    return atoi(read_file(DIR "/code"));
}

static void setup(void) {
    remove(SRC "/nav.c");
    remove(SRC "/cheat.h");
    remove(SRC "/sub/cheat.c");
    write_file(SRC "/cheat.c", "int chord(int p) { return (p & P8_PAD_BUTTON_5) && (p & P8_PAD_BUTTON_6); }\n");
    write_file(SRC "/bindings.c", "int valid(int b) { return b >= 0 && b < P8_PAD_BUTTON_COUNT; }\n"
                                  "int last(void) { return P8_PAD_BUTTON_COUNT - 1; }\n");
    write_file(SRC "/bindings.h", "enum { BINDINGS_BUTTONS = P8_PAD_BUTTON_COUNT };\n");
    write_file(SRC "/hud.c", "int hud(void) { return 1; }\n");
}

static void test_reads_only_in_the_fixed_exceptions_pass(void) {
    setup();
    assert(run(GUARD) == 0);
    assert(strstr(read_file(OUT), "button-guard files 3 reads 5\n") != NULL);
}

static void test_reads_outside_the_exceptions_fail_one_sorted_line_per_file(void) {
    setup();
    write_file(SRC "/nav.c", "int jump(int p) { return (p & P8_PAD_BUTTON_7) || (p & P8_PAD_BUTTON_8); }\n");
    write_file(SRC "/hud.c", "int hud(int p) { return p & P8_PAD_BUTTON_11; }\n");
    assert(run(GUARD) == 1);
    const char *out = read_file(OUT);
    const char *hud = strstr(out, "button_guard: " SRC "/hud.c names 1 button(s) outside cheat.c and the bindings module\n");
    const char *nav = strstr(out, "button_guard: " SRC "/nav.c names 2 button(s) outside cheat.c and the bindings module\n");
    assert(hud != NULL && nav != NULL && hud < nav);
    assert(strstr(out, "button-guard files 5 reads 8\n") != NULL);
}

static void test_only_the_exact_exception_paths_are_exempt(void) {
    setup();
    write_file(SRC "/cheat.h", "#define CHORD (P8_PAD_BUTTON_5 | P8_PAD_BUTTON_6)\n");
    write_file(SRC "/sub/cheat.c", "int chord(int p) { return p & P8_PAD_BUTTON_5; }\n");
    assert(run(GUARD) == 1);
    const char *out = read_file(OUT);
    assert(strstr(out, "button_guard: " SRC "/cheat.h names 2 button(s) outside") != NULL);
    assert(strstr(out, "button_guard: " SRC "/sub/cheat.c names 1 button(s) outside") != NULL);
}

static void test_names_in_comments_literals_and_longer_identifiers_are_not_reads(void) {
    setup();
    write_file(SRC "/hud.c", "/* P8_PAD_BUTTON_1 once\n * P8_PAD_BUTTON_2 twice */\n"
                             "int hud(void) { return 1; } // P8_PAD_BUTTON_3\n"
                             "const char *m = \"P8_PAD_BUTTON_4 \\\" P8_PAD_BUTTON_4\";\n"
                             "char quote = '\"'; /* P8_PAD_BUTTON_5 */ int x; /* \" */\n"
                             "int MY_P8_PAD_BUTTON_5 = 0; char c = 'P';\n");
    assert(run(GUARD) == 0);
    assert(strstr(read_file(OUT), "button-guard files 3 reads 5\n") != NULL);
}

static void test_tree_without_buttons_passes_and_ignores_standard_input(void) {
    setup();
    write_file(SRC "/cheat.c", "int chord(int p) { return p; }\n");
    write_file(SRC "/bindings.c", "int valid(int b) { return b >= 0; }\n");
    write_file(SRC "/bindings.h", "int valid(int b);\n");
    assert(run("printf 'P8_PAD_BUTTON_1\\n' | " GUARD) == 0);
    assert(strstr(read_file(OUT), "button-guard files 0 reads 0\n") != NULL);
}

static void test_missing_directory_is_an_error(void) {
    setup();
    assert(run("sh tools/button_guard.sh " DIR "/none") == 2);
    const char *out = read_file(OUT);
    assert(strstr(out, "directory not found: " DIR "/none\n") != NULL);
    assert(strstr(out, "button-guard ") == NULL);
}

static void test_directory_without_c_files_is_an_error(void) {
    setup();
    write_file(DIR "/empty/notes.txt", "P8_PAD_BUTTON_1\n");
    assert(run("sh tools/button_guard.sh " DIR "/empty") == 2);
    const char *out = read_file(OUT);
    assert(strstr(out, "no C file under: " DIR "/empty\n") != NULL);
    assert(strstr(out, "button-guard ") == NULL);
}

int main(void) {
    assert(system("mkdir -p " SRC "/sub " DIR "/empty") == 0);
    test_reads_only_in_the_fixed_exceptions_pass();
    test_reads_outside_the_exceptions_fail_one_sorted_line_per_file();
    test_only_the_exact_exception_paths_are_exempt();
    test_names_in_comments_literals_and_longer_identifiers_are_not_reads();
    test_tree_without_buttons_passes_and_ignores_standard_input();
    test_missing_directory_is_an_error();
    test_directory_without_c_files_is_an_error();
    assert(system("rm -rf " DIR) == 0);
    printf("test_button_guard: all tests passed\n");
    return 0;
}
