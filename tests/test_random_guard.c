#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define DIR "tmp/test_random_guard_work"
#define SRC DIR "/src"
#define LIST DIR "/list.txt"
#define OUT DIR "/out.txt"
#define GUARD "sh tools/random_guard.sh " LIST " " SRC

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
static int run_guard(void) {
    assert(system(GUARD " > " OUT " 2>&1; echo $? > " DIR "/code") == 0);
    return atoi(read_file(DIR "/code"));
}

/* Spawns no process, the directories are made once by main: every guard run
 * costs a few milliseconds. hud.c names the families only in comments and
 * literals. */
static void setup(void) {
    write_file(SRC "/game/galaxy.c",
               "int roll(void) { return p8_fixed_rnd(4) + p8_fixed_rnd(4); }\n");
    write_file(SRC "/game/particle.c", "int spark(void) { return visual_random_draw_one(); }\n");
    write_file(SRC "/game/hud.c", "/* p8_fixed_rnd(1) once\n * visual_random_draw_one() twice */\n"
                                  "int hud(void) { return 1; } // p8_fixed_rnd_state()\n"
                                  "const char *name = \"p8_fixed_rnd \\\" p8_fixed_rnd\";\n"
                                  "char quote = '\"'; /* p8_fixed_rnd */ int x; /* \" */\n");
    write_file(LIST, "# comment\n\n"
                     SRC "/game/galaxy.c game 2 | the galaxy roll\n"
                     SRC "/game/particle.c visual 1 | the sparks\n");
}

static void test_clean_tree_passes_with_names_in_comments(void) {
    setup();
    assert(run_guard() == 0);
    assert(strstr(read_file(OUT), "random-guard files 2 calls 3\n") != NULL);
}

static void test_draw_in_unlisted_file_after_a_comment_fails(void) {
    setup();
    write_file(SRC "/game/hud.c", "/* a\n b */ int hud(void) { return p8_fixed_rnd_default(); }\n");
    assert(run_guard() == 1);
    assert(strstr(read_file(OUT), SRC "/game/hud.c uses the game family 1 time(s), not listed\n") != NULL);
    assert(strstr(read_file(OUT), "random_guard: if intended, edit " LIST " by hand\n") != NULL);
}

static void test_empty_list_is_an_error_not_a_pass(void) {
    setup();
    write_file(LIST, "# only a comment\n\n");
    assert(run_guard() == 2);
    assert(strstr(read_file(OUT), "empty list\n") != NULL);
}

static void test_count_changes_in_listed_files_fail_family_by_family(void) {
    setup();
    write_file(SRC "/game/galaxy.c", "int roll(void) { return p8_fixed_rnd(4) + p8_fixed_rnd(4); }\n"
                                     "void load(void) { p8_fixed_set_rnd_state(1); }\n"
                                     "int own(void) { return p8_fixed_rnd_from_state(0, 1); }\n");
    write_file(SRC "/game/particle.c", "int spark(void) { return 0; }\n");
    assert(run_guard() == 1);
    const char *out = read_file(OUT);
    assert(strstr(out, SRC "/game/particle.c no longer uses the visual family, listed 1\n") != NULL);
    assert(strstr(out, SRC "/game/galaxy.c uses the game family 3 time(s), listed 2\n") != NULL);
    assert(strstr(out, SRC "/game/galaxy.c uses the generator family 1 time(s), not listed\n") != NULL);
}

static void test_invalid_list_lines_are_errors(void) {
    setup();
    write_file(LIST, SRC "/game/galaxy.c game 2\n"
                     SRC "/game/galaxy.c game 2 |  \n"
                     SRC "/game/galaxy.c dice 2 | the galaxy roll\n"
                     SRC "/game/particle.c visual 0 | the sparks\n"
                     SRC "/game/hud.c 2 | the hud\n"
                     SRC "/game/gone.c game 2 | deleted\n"
                     SRC "/game/galaxy.c game 2 | the galaxy roll, again\n");
    assert(run_guard() == 2);
    const char *out = read_file(OUT);
    assert(strstr(out, "line without a reason: " SRC "/game/galaxy.c game 2\n") != NULL);
    assert(strstr(out, "empty reason: " SRC "/game/galaxy.c\n") != NULL);
    assert(strstr(out, "unknown family dice: " SRC "/game/galaxy.c\n") != NULL);
    assert(strstr(out, "count not a positive integer: " SRC "/game/particle.c visual 0") != NULL);
    assert(strstr(out, "not `<path> <family> <count> | <reason>`: " SRC "/game/hud.c 2") != NULL);
    assert(strstr(out, "listed file missing: " SRC "/game/gone.c\n") != NULL);
    assert(strstr(out, "duplicate line: " SRC "/game/galaxy.c game\n") != NULL);
    assert(strstr(out, "random-guard ") == NULL);
}

static void test_missing_directory_is_an_error(void) {
    setup();
    assert(system("sh tools/random_guard.sh " LIST " " DIR "/none > " OUT " 2>&1; echo $? > " DIR "/code") == 0);
    assert(atoi(read_file(DIR "/code")) == 2);
    assert(strstr(read_file(OUT), "directory not found: " DIR "/none\n") != NULL);
}

int main(void) {
    assert(system("mkdir -p " SRC "/game") == 0);
    test_clean_tree_passes_with_names_in_comments();
    test_draw_in_unlisted_file_after_a_comment_fails();
    test_empty_list_is_an_error_not_a_pass();
    test_count_changes_in_listed_files_fail_family_by_family();
    test_invalid_list_lines_are_errors();
    test_missing_directory_is_an_error();
    assert(system("rm -rf " DIR) == 0);
    printf("test_random_guard: all tests passed\n");
    return 0;
}
