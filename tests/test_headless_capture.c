/* Runs the real driver, tmp/replay, through system(): a copy of its
 * sequence here would be one more subtly different replay, and would say
 * nothing of the one `make replay` runs.
 *
 * Each run rewrites the driver's default dump, which test_frames_tool and
 * `make replay EVENTS=...` rewrite too. */

#include "devtools/dump.h"
#include "p8/input.h"
#include "p8/vram.h"
#include "devtools/screencast.h"

#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>
#include <sys/wait.h>

#define FIXTURE "tests/fixtures/screencast_capture_events.json"
#define DRIVER "./tmp/replay"
/* The default dump path of src/cmd/replay.c, when argv[3] is missing. */
#define DRIVER_DUMP "tmp/replay_dump.txt"
#define CUSTOM_DUMP "tmp/test_headless_capture_custom_dump.txt"
#define DUMP_WITNESS "witness: this dump must not be rewritten\n"
#define CAPTURE "tmp/test_headless_capture.bin"
#define STDERR_LOG "tmp/test_headless_capture_stderr.txt"
#define SAVE_HEX "tmp/test_headless_capture_save.hex"
/* A path whose parent directory does not exist: the invalid path case,
 * reached without depending on file system permissions or on a full
 * disk. */
#define BAD_CAPTURE "tmp/no_such_directory_for_capture/capture.bin"
#define TITLE_E2E "tests/e2e/pad_title.json"
#define DEFAULT_BINDINGS "bindings.default.cfg"
#define FIXTURE_DIR "tmp/test_headless_capture_fixture"
#define FIXTURE_REPLAY FIXTURE_DIR "/title.json"
#define FIXTURE_CFG FIXTURE_DIR "/same.cfg"
#define FIXTURE_CAPTURE FIXTURE_DIR "/title.bin"
#define FIXTURE_DEPS FIXTURE_DIR "/title.bin.d"
#define FIXTURE_STDERR FIXTURE_DIR "/stderr.txt"
#define TITLE_CAPTURE FIXTURE_DIR "/original.bin"

/* One NUL past the end so strstr() works on text outputs; `*out_len`
 * excludes it. The caller frees. */
static char *read_whole_file(const char *path, size_t *out_len) {
    FILE *file = fopen(path, "rb");
    assert(file != NULL);
    assert(fseek(file, 0, SEEK_END) == 0);
    long size = ftell(file);
    assert(size > 0);
    rewind(file);

    char *buf = malloc((size_t)size + 1);
    assert(buf != NULL);
    assert(fread(buf, 1, (size_t)size, file) == (size_t)size);
    fclose(file);
    buf[size] = '\0';

    *out_len = (size_t)size;
    return buf;
}

static void stale_save_hex(void) {
    FILE *stale = fopen(SAVE_HEX, "w");
    assert(stale != NULL);
    assert(fputs("00\n", stale) >= 0);
    assert(fclose(stale) == 0);
}

/* The fixture never acts: the storage ends empty, so a fixture cut from it
 * would be stale. */
static void no_record_fails_and_removes_the_save_hex(int status) {
    assert(WIFEXITED(status) && WEXITSTATUS(status) == 1);
    assert(fopen(SAVE_HEX, "r") == NULL && "a stale file is removed, never kept");
    size_t log_len;
    char *log = read_whole_file(STDERR_LOG, &log_len);
    assert(strstr(log, "no stored record") != NULL && strstr(log, SAVE_HEX) != NULL);
    free(log);
    remove(STDERR_LOG);
}

/* Through the real dumper, not a decoder written here: a test that rebuilt
 * the dump format would compare its own idea of it with the driver's.
 *
 * The caller frees. NOT NUL-terminated, unlike read_whole_file: only
 * memcmp over `*out_len` is valid on it. */
static char *dump_text_of_pixels(const unsigned char *pixels, size_t *out_len) {
    p8_vram_t mem;
    p8_vram_init(&mem);
    memcpy(mem.screen, pixels, sizeof(mem.screen));

    FILE *tmp = tmpfile();
    assert(tmp != NULL);
    assert(dump_screen(&mem, tmp));
    long size = ftell(tmp);
    assert(size > 0);
    rewind(tmp);

    char *buf = malloc((size_t)size);
    assert(buf != NULL);
    assert(fread(buf, 1, (size_t)size, tmp) == (size_t)size);
    fclose(tmp);

    *out_len = (size_t)size;
    return buf;
}

/* Read by the driver's own loader rather than hard-coded, so the count
 * follows the fixture if it changes. */
static int fixture_frame_count(void) {
    p8_input_t input;
    p8_input_init(&input);
    assert(p8_input_load(&input, FIXTURE));
    int count = input.frame_count;
    p8_input_free(&input);
    return count;
}

static void test_capture_holds_exactly_the_replayed_frames(void) {
    remove(CAPTURE);
    assert(system(DRIVER " " FIXTURE " " CAPTURE " > /dev/null") == 0);

    screencast_reader_t reader;
    assert(screencast_reader_open(&reader, CAPTURE) == SCREENCAST_OK);
    assert(screencast_frame_count(&reader) == fixture_frame_count());
    screencast_reader_close(&reader);

    remove(CAPTURE);
}

/* The last frame is the only possible point of comparison: the driver's
 * text dump describes that frame alone. */
static void test_last_captured_frame_matches_the_text_dump(void) {
    remove(CAPTURE);
    assert(system(DRIVER " " FIXTURE " " CAPTURE " > /dev/null") == 0);

    size_t dump_len;
    char *dump = read_whole_file(DRIVER_DUMP, &dump_len);

    screencast_reader_t reader;
    assert(screencast_reader_open(&reader, CAPTURE) == SCREENCAST_OK);
    int last = screencast_frame_count(&reader) - 1;
    assert(last > 0);

    unsigned char last_pixels[SCREENCAST_FRAME_PIXELS];
    assert(screencast_read_frame(&reader, last, last_pixels));

    /* The second-to-last frame must differ from the last, or a capture
     * shifted by one frame would pass the comparison below by accident. */
    unsigned char previous_pixels[SCREENCAST_FRAME_PIXELS];
    assert(screencast_read_frame(&reader, last - 1, previous_pixels));
    assert(memcmp(previous_pixels, last_pixels, sizeof(last_pixels)) != 0);
    screencast_reader_close(&reader);

    size_t captured_len;
    char *captured = dump_text_of_pixels(last_pixels, &captured_len);

    assert(captured_len == dump_len);
    assert(memcmp(captured, dump, dump_len) == 0);

    free(captured);
    free(dump);
    remove(CAPTURE);
}

static void test_capturing_changes_nothing_the_replay_produces(void) {
    remove(DRIVER_DUMP);
    stale_save_hex();
    remove(STDERR_LOG);
    no_record_fails_and_removes_the_save_hex(
        system("SAVE_HEX=" SAVE_HEX " " DRIVER " " FIXTURE " > /dev/null 2> " STDERR_LOG));
    size_t without_len;
    char *without = read_whole_file(DRIVER_DUMP, &without_len);

    remove(DRIVER_DUMP);
    remove(CAPTURE);
    assert(system(DRIVER " " FIXTURE " " CAPTURE " > /dev/null") == 0);
    size_t with_len;
    char *with = read_whole_file(DRIVER_DUMP, &with_len);

    assert(with_len == without_len);
    assert(memcmp(with, without, without_len) == 0);

    free(with);
    free(without);
    remove(CAPTURE);
}

/* Fatal, unlike session_recorder_record_frame which drops a frame: nobody
 * is playing, and a driver that went on would return 0 and let `make e2e`
 * hash the capture of an earlier replay. The message is asserted because
 * it names the path, the only way to fix a mistyped one. */
static void test_unopenable_capture_path_is_fatal_and_names_the_path(void) {
    remove(STDERR_LOG);
    assert(system(DRIVER " " FIXTURE " " BAD_CAPTURE " > /dev/null 2> " STDERR_LOG) != 0);

    size_t log_len;
    char *log = read_whole_file(STDERR_LOG, &log_len);
    assert(strstr(log, BAD_CAPTURE) != NULL);

    free(log);
    remove(STDERR_LOG);
}

/* Variant replays run side by side must not overwrite each other's dump:
 * with argv[3] the dump goes there, and the default path -- read by this
 * test and by test_frames_tool -- keeps its witness. */
static void test_third_argument_redirects_the_text_dump(void) {
    remove(DRIVER_DUMP);
    assert(system(DRIVER " " FIXTURE " > /dev/null") == 0);
    size_t default_len;
    char *default_dump = read_whole_file(DRIVER_DUMP, &default_len);

    FILE *witness = fopen(DRIVER_DUMP, "w");
    assert(witness != NULL);
    assert(fputs(DUMP_WITNESS, witness) >= 0);
    assert(fclose(witness) == 0);

    remove(CUSTOM_DUMP);
    remove(CAPTURE);
    assert(system(DRIVER " " FIXTURE " " CAPTURE " " CUSTOM_DUMP " > /dev/null") == 0);

    size_t custom_len;
    char *custom = read_whole_file(CUSTOM_DUMP, &custom_len);
    assert(custom_len == default_len);
    assert(memcmp(custom, default_dump, default_len) == 0);

    size_t untouched_len;
    char *untouched = read_whole_file(DRIVER_DUMP, &untouched_len);
    assert(untouched_len == strlen(DUMP_WITNESS));
    assert(memcmp(untouched, DUMP_WITNESS, untouched_len) == 0);

    free(untouched);
    free(custom);
    free(default_dump);
    remove(CUSTOM_DUMP);
    remove(CAPTURE);
    remove(DRIVER_DUMP);
}

static void write_text(const char *path, const char *text, size_t length) {
    FILE *out = fopen(path, "wb");
    assert(out != NULL);
    assert(fwrite(text, 1, length, out) == length);
    assert(fclose(out) == 0);
}

/* FIXTURE_REPLAY becomes TITLE_E2E with `member` as a header line after "input". */
static void write_title_naming(const char *member) {
    size_t length;
    char *text = read_whole_file(TITLE_E2E, &length);
    const char *input = "\"input\": \"pad\",\n";
    char *after = strstr(text, input);
    assert(after != NULL && "pad_title.json starts with the input member");
    after += strlen(input);
    FILE *out = fopen(FIXTURE_REPLAY, "wb");
    assert(out != NULL);
    assert(fwrite(text, 1, (size_t)(after - text), out) == (size_t)(after - text));
    assert(fprintf(out, "  \"bindings\": \"%s\",\n", member) > 0);
    size_t rest = length - (size_t)(after - text);
    assert(fwrite(after, 1, rest, out) == rest);
    assert(fclose(out) == 0);
    free(text);
}

/* FIXTURE_CFG becomes the default bindings with `from` replaced by `to`, or a plain copy when from is NULL. */
static void write_fixture_cfg(const char *from, const char *to) {
    size_t length;
    char *text = read_whole_file(DEFAULT_BINDINGS, &length);
    char *found = from != NULL ? strstr(text, from) : NULL;
    assert(from == NULL || (found != NULL && strlen(from) == strlen(to)));
    if (found != NULL) memcpy(found, to, strlen(to));
    write_text(FIXTURE_CFG, text, length);
    free(text);
}

static int files_equal(const char *a, const char *b) {
    size_t a_length, b_length;
    char *a_text = read_whole_file(a, &a_length);
    char *b_text = read_whole_file(b, &b_length);
    int equal = a_length == b_length && memcmp(a_text, b_text, a_length) == 0;
    free(a_text);
    free(b_text);
    return equal;
}

static void capture_the_original_title(void) {
    mkdir(FIXTURE_DIR, 0777);
    remove(TITLE_CAPTURE);
    assert(system(DRIVER " " TITLE_E2E " " TITLE_CAPTURE " " FIXTURE_DIR "/original.dump.txt > /dev/null") == 0);
}

static void run_fixture_replay(void) {
    remove(FIXTURE_CAPTURE);
    assert(system(DRIVER " " FIXTURE_REPLAY " " FIXTURE_CAPTURE " " FIXTURE_DIR "/title.dump.txt > /dev/null") == 0);
}

/* pad_title chooses its row with button 5: bound to 8, the title picks nothing. */
static void test_a_changed_fixture_reaches_the_game(void) {
    write_title_naming("same.cfg");
    write_fixture_cfg("title.choose_row = 5", "title.choose_row = 8");
    run_fixture_replay();
    assert(!files_equal(FIXTURE_CAPTURE, TITLE_CAPTURE));
}

static void test_a_default_identical_fixture_plays_the_same_game(void) {
    write_title_naming("same.cfg");
    write_fixture_cfg(NULL, NULL);
    run_fixture_replay();
    assert(files_equal(FIXTURE_CAPTURE, TITLE_CAPTURE));
}

/* Returns the driver's exit status after a run that must fail before the capture opens. */
static int run_refused_fixture_replay(const char *expected_in_stderr) {
    remove(FIXTURE_CAPTURE);
    remove(FIXTURE_STDERR);
    int status = system(DRIVER " " FIXTURE_REPLAY " " FIXTURE_CAPTURE " " FIXTURE_DIR "/title.dump.txt > /dev/null 2> "
                        FIXTURE_STDERR);
    assert(fopen(FIXTURE_CAPTURE, "r") == NULL && "a refused replay never opens its capture");
    size_t length;
    char *log = read_whole_file(FIXTURE_STDERR, &length);
    assert(strstr(log, expected_in_stderr) != NULL);
    free(log);
    return status;
}

static void test_a_personal_recording_is_refused(void) {
    write_title_naming("personal");
    int status = run_refused_fixture_replay("recorded with a personal bindings.cfg");
    assert(WIFEXITED(status) && WEXITSTATUS(status) == 1);
}

static void test_a_missing_fixture_is_refused_naming_its_path(void) {
    write_title_naming("absent.cfg");
    int status = run_refused_fixture_replay(FIXTURE_DIR "/absent.cfg");
    assert(WIFEXITED(status) && WEXITSTATUS(status) == 1);
}

static void test_replay_deps_names_the_fixture_of_the_capture(void) {
    write_title_naming("same.cfg");
    write_fixture_cfg(NULL, NULL);
    remove(FIXTURE_DEPS);
    remove(FIXTURE_CAPTURE);
    assert(system("REPLAY_DEPS=" FIXTURE_DEPS " " DRIVER " " FIXTURE_REPLAY " " FIXTURE_CAPTURE " " FIXTURE_DIR
                  "/title.dump.txt > /dev/null") == 0);
    static const char expected[] = FIXTURE_CAPTURE ": " FIXTURE_CFG "\n" FIXTURE_CFG ":\n";
    size_t length;
    char *deps = read_whole_file(FIXTURE_DEPS, &length);
    assert(length == strlen(expected) && memcmp(deps, expected, length) == 0);
    free(deps);
}

static void test_replay_deps_without_a_fixture_removes_a_stale_file(void) {
    write_title_naming("same.cfg");
    write_fixture_cfg(NULL, NULL);
    write_text(FIXTURE_DEPS, "stale:\n", 7);
    assert(system("REPLAY_DEPS=" FIXTURE_DEPS " " DRIVER " " TITLE_E2E " " FIXTURE_CAPTURE " " FIXTURE_DIR
                  "/title.dump.txt > /dev/null") == 0);
    assert(fopen(FIXTURE_DEPS, "r") == NULL && "a replay without a fixture leaves no prerequisite");
}

static void test_replay_deps_requires_a_capture_path(void) {
    int status = system("REPLAY_DEPS=" FIXTURE_DEPS " " DRIVER " " TITLE_E2E " > /dev/null 2>&1");
    assert(WIFEXITED(status) && WEXITSTATUS(status) == 2);
}

static void remove_fixture_files(void) {
    static const char *const files[] = {FIXTURE_REPLAY, FIXTURE_CFG, FIXTURE_CAPTURE, FIXTURE_DEPS, FIXTURE_STDERR,
                                        TITLE_CAPTURE, FIXTURE_DIR "/title.dump.txt", FIXTURE_DIR "/original.dump.txt"};
    for (size_t i = 0; i < sizeof files / sizeof files[0]; i++) remove(files[i]);
    rmdir(FIXTURE_DIR);
}

#define DISPLAY_DIR "tmp/test_headless_capture_display"

static void read_frame(const char *path, int index, unsigned char pixels[SCREENCAST_FRAME_PIXELS]) {
    screencast_reader_t reader;
    assert(screencast_reader_open(&reader, path) == SCREENCAST_OK);
    assert(screencast_read_frame(&reader, index, pixels));
    screencast_reader_close(&reader);
}

/* A display entry at frame F shows the picture drawn at F-2, unfiltered even next to a masked
 * capture. */
static void test_a_display_capture_writes_the_frame_drawn_two_frames_earlier(void) {
    assert(system("rm -rf " DISPLAY_DIR " && mkdir -p " DISPLAY_DIR) == 0);
    FILE *list = fopen(DISPLAY_DIR "/list.txt", "w");
    assert(list != NULL);
    fputs("replay pad_title\nall 10 all\ndisplay 12 display\nprint 13 cls,print\n", list);
    fclose(list);
    assert(system("CAPTURE_LIST=" DISPLAY_DIR "/list.txt CAPTURE_DIR=" DISPLAY_DIR " " DRIVER " " TITLE_E2E " "
                  DISPLAY_DIR "/whole.bin " DISPLAY_DIR "/dump.txt > /dev/null") == 0);

    static unsigned char drawn[SCREENCAST_FRAME_PIXELS], displayed[SCREENCAST_FRAME_PIXELS],
        later[SCREENCAST_FRAME_PIXELS];
    read_frame(DISPLAY_DIR "/whole.bin", 10, drawn);
    read_frame(DISPLAY_DIR "/pc_frames.bin", 1, displayed);
    read_frame(DISPLAY_DIR "/whole.bin", 12, later);
    assert(memcmp(drawn, later, sizeof drawn) != 0 && "the test needs frames 10 and 12 to differ");
    assert(memcmp(drawn, displayed, sizeof drawn) == 0);

    size_t length;
    char *script = read_whole_file(DISPLAY_DIR "/capture_script.txt", &length);
    assert(strcmp(script, "10 03ff\n12 8000\n13 0101\n") == 0);
    free(script);
    char *counts = read_whole_file(DISPLAY_DIR "/counts.txt", &length);
    const char *at_10 = strstr(counts, "10 print ");
    const char *at_12 = strstr(counts, "12 print ");
    assert(at_10 != NULL && at_12 != NULL);
    assert(strncmp(at_10 + 3, at_12 + 3, strcspn(at_10, "\n") - 3) == 0 && "a display entry counts F-2's calls");
    free(counts);
}

int main(void) {
    test_capture_holds_exactly_the_replayed_frames();
    test_last_captured_frame_matches_the_text_dump();
    test_capturing_changes_nothing_the_replay_produces();
    test_unopenable_capture_path_is_fatal_and_names_the_path();
    test_third_argument_redirects_the_text_dump();
    capture_the_original_title();
    test_a_changed_fixture_reaches_the_game();
    test_a_default_identical_fixture_plays_the_same_game();
    test_a_personal_recording_is_refused();
    test_a_missing_fixture_is_refused_naming_its_path();
    test_replay_deps_names_the_fixture_of_the_capture();
    test_replay_deps_without_a_fixture_removes_a_stale_file();
    test_replay_deps_requires_a_capture_path();
    test_a_display_capture_writes_the_frame_drawn_two_frames_earlier();
    remove_fixture_files();
    printf("test_headless_capture: all tests passed\n");
    return 0;
}
