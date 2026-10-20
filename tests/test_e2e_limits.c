/* tools/e2e_limits.sh, the guard of new e2e files (companion files, 180
 * frames, 600 for a listed exception, the pad header, no mouse field, the
 * .txt's "N frames.", each exception's "N frames:" and its length over 180),
 * on built events files. */

#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define SCRIPT "sh tools/e2e_limits.sh"
#define DIR "tmp/test_e2e_limits_work"
#define LOG DIR "/log.txt"
#define EXCEPTIONS DIR "/exceptions.txt"

#define OLD_MOUSE_FRAME "{ \"mouse_x\": 64, \"mouse_y\": 64, \"mouse_buttons\": 0, " \
                        "\"dpad\": [false, false, false, false, false, false] }"

static void write_file(const char *path, const char *text) {
    FILE *file = fopen(path, "w");
    assert(file != NULL);
    fputs(text, file);
    fclose(file);
}

/* Static buffer, overwritten by each call. */
static const char *read_log(void) {
    FILE *file = fopen(LOG, "rb");
    assert(file != NULL);
    static char buf[1 << 16];
    size_t len = fread(buf, 1, sizeof(buf) - 1, file);
    fclose(file);
    buf[len] = '\0';
    return buf;
}

/* A complete e2e: <name>.json opening with `header`, then `frames` copies
 * of `frame` except frame 1, which is `odd` when not NULL; <name>.txt
 * stating that count, and <name>.json.sha256. */
static void write_e2e_with(const char *name, const char *header, int frames, const char *frame, const char *odd) {
    char path[256];
    snprintf(path, sizeof path, DIR "/%s.json", name);
    FILE *file = fopen(path, "w");
    assert(file != NULL);
    fputs(header, file);
    for (int i = 0; i < frames; i++) {
        fputs("    ", file);
        fputs(i == 1 && odd != NULL ? odd : frame, file);
        fputs(i + 1 < frames ? ",\n" : "\n", file);
    }
    fputs("  ]\n}\n", file);
    fclose(file);
    snprintf(path, sizeof path, DIR "/%s.txt", name);
    char txt[256];
    snprintf(txt, sizeof txt, "What the replay does. %d frames.\n", frames);
    write_file(path, txt);
    snprintf(path, sizeof path, DIR "/%s.json.sha256", name);
    write_file(path, "0000\n");
}

#define PAD_HEADER "{\n  \"input\": \"pad\",\n  \"frames\": [\n"

static void write_e2e(const char *name, int frames, const char *odd) {
    write_e2e_with(name, PAD_HEADER, frames, "{ \"pad\": [] }", odd);
}

static int run(const char *events) {
    char cmd[1024];
    snprintf(cmd, sizeof cmd, SCRIPT " " EXCEPTIONS " %s > " LOG " 2>&1", events);
    return system(cmd);
}

static void reset(void) {
    assert(system("rm -rf " DIR " && mkdir -p " DIR) == 0);
    write_file(EXCEPTIONS, "");
}

static void test_no_e2e_passes(void) {
    reset();
    assert(run("") == 0);
}

static void test_e2e_of_180_frames_passes(void) {
    reset();
    write_e2e("short", 180, NULL);
    assert(run(DIR "/short.json") == 0);
}

static void test_e2e_over_180_frames_fails_and_is_named(void) {
    reset();
    write_e2e("short", 10, NULL);
    write_e2e("long", 181, NULL);
    assert(run(DIR "/short.json " DIR "/long.json") != 0);
    assert(strstr(read_log(), DIR "/long.json") != NULL);
    assert(strstr(read_log(), "181") != NULL);
    assert(strstr(read_log(), DIR "/short.json:") == NULL);
}

static void test_frames_are_counted_by_their_pad_key(void) {
    reset();
    write_e2e_with("pressed", PAD_HEADER, 181, "{ \"pad\": [5, 1] }", NULL);
    assert(run(DIR "/pressed.json") != 0);
    assert(strstr(read_log(), "181 frames") != NULL);
}

static void test_listed_exception_is_allowed_up_to_600_frames(void) {
    reset();
    write_e2e("long", 600, NULL);
    write_file(EXCEPTIONS, DIR "/long.json 600 frames: the game takes long to end\n");
    assert(run(DIR "/long.json") == 0);
    write_e2e("long", 601, NULL);
    write_file(EXCEPTIONS, DIR "/long.json 601 frames: the game takes long to end\n");
    assert(run(DIR "/long.json") != 0);
    assert(strstr(read_log(), DIR "/long.json") != NULL);
}

static void test_initial_save_header_line_passes(void) {
    reset();
    write_e2e_with("resume", "{\n  \"input\": \"pad\",\n  \"initial_save\": \"534c5759\",\n  \"frames\": [\n",
                   180, "{ \"pad\": [] }", NULL);
    assert(run(DIR "/resume.json") == 0);
}

static void test_missing_companion_file_fails(void) {
    reset();
    write_e2e("no_txt", 10, NULL);
    assert(system("rm " DIR "/no_txt.txt") == 0);
    assert(run(DIR "/no_txt.json") != 0);
    assert(strstr(read_log(), DIR "/no_txt.txt") != NULL);
    write_e2e("no_sha", 10, NULL);
    assert(system("rm " DIR "/no_sha.json.sha256") == 0);
    assert(run(DIR "/no_sha.json") != 0);
    assert(strstr(read_log(), DIR "/no_sha.json.sha256") != NULL);
    assert(run(DIR "/absent.json") != 0);
    assert(strstr(read_log(), DIR "/absent.json") != NULL);
}

static void test_missing_exceptions_list_fails(void) {
    reset();
    assert(system("rm " EXCEPTIONS) == 0);
    assert(run("") != 0);
}

static void test_e2e_with_a_mouse_field_fails_and_is_named(void) {
    reset();
    write_e2e("mouse_x", 10, "{ \"pad\": [], \"mouse_x\": 64 }");
    assert(run(DIR "/mouse_x.json") != 0);
    assert(strstr(read_log(), DIR "/mouse_x.json") != NULL);
    write_e2e("dpad", 10, "{ \"pad\": [], \"dpad\": [false, false, false, false] }");
    assert(run(DIR "/dpad.json") != 0);
    assert(strstr(read_log(), DIR "/dpad.json") != NULL);
}

static void test_e2e_without_the_pad_header_fails_and_is_named(void) {
    reset();
    write_e2e_with("no_header", "{\n  \"frames\": [\n", 10, "{ \"pad\": [] }", NULL);
    assert(run(DIR "/no_header.json") != 0);
    assert(strstr(read_log(), DIR "/no_header.json") != NULL);
    write_e2e_with("old_mouse", "{\n  \"frames\": [\n", 10, OLD_MOUSE_FRAME, NULL);
    assert(run(DIR "/old_mouse.json") != 0);
    assert(strstr(read_log(), DIR "/old_mouse.json") != NULL);
    write_e2e_with("mouse_header", "{\n  \"input\": \"mouse\",\n  \"frames\": [\n", 10, "{ \"pad\": [] }", NULL);
    assert(run(DIR "/mouse_header.json") != 0);
    assert(strstr(read_log(), DIR "/mouse_header.json") != NULL);
}

static void write_txt(const char *name, const char *text) {
    char path[256];
    snprintf(path, sizeof path, DIR "/%s.txt", name);
    write_file(path, text);
}

static void test_txt_stating_the_right_count_passes_even_split_across_lines(void) {
    reset();
    write_e2e("split", 12, NULL);
    write_txt("split", "A probe, then a\nfold. 12\nframes.\n\nA second paragraph: 99 frames.\n");
    assert(run(DIR "/split.json") == 0);
}

static void test_txt_stating_a_wrong_count_fails_and_is_named(void) {
    reset();
    write_e2e("before", 10, NULL);
    write_e2e("wrong", 12, NULL);
    write_txt("wrong", "A probe. 11 frames.\n");
    write_e2e("after", 14, NULL);
    assert(run(DIR "/before.json " DIR "/wrong.json " DIR "/after.json") != 0);
    assert(strstr(read_log(), "e2e: " DIR "/wrong.json: .txt says 11 frames, the file has 12\n") != NULL);
    assert(strstr(read_log(), DIR "/before.json:") == NULL && strstr(read_log(), DIR "/after.json:") == NULL);
}

static void test_txt_without_the_statement_in_its_first_paragraph_fails(void) {
    reset();
    write_e2e("none", 12, NULL);
    write_txt("none", "A probe, 12 frames (listed somewhere).\n\nThen 12 frames.\n");
    assert(run(DIR "/none.json") != 0);
    assert(strstr(read_log(), "e2e: " DIR "/none.json: .txt states no \"N frames.\"") != NULL);
    write_txt("none", "");
    assert(run(DIR "/none.json") != 0);
    assert(strstr(read_log(), "e2e: " DIR "/none.json: .txt states no \"N frames.\"") != NULL);
}

static void test_txt_with_two_statements_in_its_first_paragraph_fails(void) {
    reset();
    write_e2e("twice", 12, NULL);
    write_txt("twice", "A probe. 12 frames.\nOr 12 frames.\n");
    assert(run(DIR "/twice.json") != 0);
    assert(strstr(read_log(), DIR "/twice.json") != NULL);
}

static void test_listed_exception_stating_a_wrong_count_fails_and_is_named(void) {
    reset();
    write_e2e("short", 10, NULL);
    write_e2e("long", 300, NULL);
    write_file(EXCEPTIONS, DIR "/long.json 299 frames: the game takes long to end\n");
    assert(run(DIR "/short.json " DIR "/long.json") != 0);
    assert(strstr(read_log(), "e2e: " DIR "/long.json: " EXCEPTIONS " says 299 frames, the file has 300\n") != NULL);
    assert(strstr(read_log(), DIR "/short.json:") == NULL);
}

static void test_listed_exception_stating_no_count_fails(void) {
    reset();
    write_e2e("long", 300, NULL);
    write_file(EXCEPTIONS, DIR "/long.json the game takes long to end\n");
    assert(run(DIR "/long.json") != 0);
    assert(strstr(read_log(), "e2e: " DIR "/long.json: " EXCEPTIONS " states no \"N frames:\"\n") != NULL);
}

static void test_listed_exception_of_180_frames_or_fewer_fails(void) {
    reset();
    write_e2e("listed", 180, NULL);
    write_file(EXCEPTIONS, DIR "/listed.json 180 frames: no longer long\n");
    assert(run(DIR "/listed.json") != 0);
    assert(strstr(read_log(), "e2e: " DIR "/listed.json: listed in " EXCEPTIONS " but runs 180 frames, not over 180\n") !=
           NULL);
    write_e2e("listed", 181, NULL);
    write_file(EXCEPTIONS, DIR "/listed.json 181 frames: long again\n");
    assert(run(DIR "/listed.json") == 0);
}

static void test_listed_exception_without_its_e2e_fails(void) {
    reset();
    write_e2e("given", 10, NULL);
    write_file(EXCEPTIONS, DIR "/gone.json 300 frames: removed since\n");
    assert(run(DIR "/given.json") != 0);
    assert(strstr(read_log(), "e2e: " DIR "/gone.json: listed in " EXCEPTIONS " but not given\n") != NULL);
}

static void test_listed_exceptions_are_judged_without_any_txt(void) {
    reset();
    write_e2e("long", 300, NULL);
    assert(system("rm " DIR "/long.txt") == 0);
    write_file(EXCEPTIONS, DIR "/long.json 299 frames: the game takes long to end\n");
    assert(run(DIR "/long.json") != 0);
    assert(strstr(read_log(), EXCEPTIONS " says 299 frames, the file has 300") != NULL);
}

int main(void) {
    test_no_e2e_passes();
    test_e2e_of_180_frames_passes();
    test_e2e_over_180_frames_fails_and_is_named();
    test_frames_are_counted_by_their_pad_key();
    test_listed_exception_is_allowed_up_to_600_frames();
    test_initial_save_header_line_passes();
    test_missing_companion_file_fails();
    test_missing_exceptions_list_fails();
    test_e2e_with_a_mouse_field_fails_and_is_named();
    test_e2e_without_the_pad_header_fails_and_is_named();
    test_txt_stating_the_right_count_passes_even_split_across_lines();
    test_txt_stating_a_wrong_count_fails_and_is_named();
    test_txt_without_the_statement_in_its_first_paragraph_fails();
    test_txt_with_two_statements_in_its_first_paragraph_fails();
    test_listed_exception_stating_a_wrong_count_fails_and_is_named();
    test_listed_exception_stating_no_count_fails();
    test_listed_exception_of_180_frames_or_fewer_fails();
    test_listed_exception_without_its_e2e_fails();
    test_listed_exceptions_are_judged_without_any_txt();
    assert(system("rm -rf " DIR) == 0);
    printf("test_e2e_limits: all tests passed\n");
    return 0;
}
