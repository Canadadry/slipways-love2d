/* tools/love_e2e_run.sh and tools/love_e2e_report.sh, the two halves of
 * `make love-e2e`, driven through their arguments on the real tmp/fake_love. */

#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "fake_love_cli.h"

#define FIXTURES FAKE_LOVE_CLI_FIXTURES
#define DIR "tmp/test_love_e2e_work"
#define CAPTURE DIR "/replay.json.bin"
#define HASH DIR "/replay.json.sha256"
#define ERROR DIR "/replay.json.error"
#define LOG DIR "/log.txt"
#define EXPECTED DIR "/expected.bin"
#define EXPECTED_HASH DIR "/expected.sha256"
#define FRAME_SIZE 9600
#define FRAMES 3

#ifdef __APPLE__
#define SHA256_CMD "shasum -a 256"
#else
#define SHA256_CMD "sha256sum"
#endif

#define RUN "sh tools/love_e2e_run.sh '" SHA256_CMD "' tmp/fake_love "
#define OUTPUTS " " CAPTURE " " HASH " " ERROR

static const char *const WORK_FILES[] = {
    CAPTURE, HASH, ERROR, LOG, EXPECTED, EXPECTED_HASH,
    DIR "/match.ref", DIR "/match.cur", DIR "/diverge.ref", DIR "/diverge.cur", DIR "/stop.ref", DIR "/stop.error",
};

static void reset(void) {
    for (size_t i = 0; i < sizeof WORK_FILES / sizeof WORK_FILES[0]; i++)
        remove(WORK_FILES[i]);
}

static void write_text(const char *path, const char *text) {
    FILE *file = fopen(path, "w");
    assert(file != NULL);
    fputs(text, file);
    fclose(file);
}

static void write_stale_outputs(void) {
    write_text(CAPTURE, "stale capture");
    write_text(HASH, "stale hash\n");
    write_text(ERROR, "stale error\n");
}

static int run(const char *command) {
    char line[1024];
    int length = snprintf(line, sizeof line, "%s > " LOG " 2>&1", command);
    assert(length > 0 && (size_t)length < sizeof line && "the command must fit");
    return system(line);
}

static void assert_log_holds(const char *needle) {
    char *log = fake_love_cli_read_file(LOG, NULL);
    assert(log != NULL);
    if (strstr(log, needle) == NULL) {
        fprintf(stderr, "%s:\n%s\nmissing: %s\n", LOG, log, needle);
        assert(!"the log must hold the text");
    }
    free(log);
}

static void assert_log_lacks(const char *needle) {
    char *log = fake_love_cli_read_file(LOG, NULL);
    assert(log != NULL);
    if (strstr(log, needle) != NULL) {
        fprintf(stderr, "%s:\n%s\nunexpected: %s\n", LOG, log, needle);
        assert(!"the log must not hold the text");
    }
    free(log);
}

/* Allocated, NUL-terminated, newline-terminated: the caller frees it. */
static char *expected_fixed_hash(void) {
    static unsigned char frames[FRAMES * FRAME_SIZE];
    memset(frames, 0x11, sizeof frames);
    for (int frame = 0; frame < FRAMES; frame++)
        frames[frame * FRAME_SIZE] = 0x88;
    FILE *file = fopen(EXPECTED, "wb");
    assert(file != NULL);
    assert(fwrite(frames, 1, sizeof frames, file) == sizeof frames);
    fclose(file);
    assert(system(SHA256_CMD " " EXPECTED " | cut -d ' ' -f 1 > " EXPECTED_HASH) == 0);
    return fake_love_cli_read_file(EXPECTED_HASH, NULL);
}

/* Stale outputs stand before every run: none may survive it. */
static void test_a_finished_run_leaves_its_capture_and_hash_only(void) {
    reset();
    write_stale_outputs();
    assert(run(RUN FIXTURES "e2e_fixed " FIXTURES "three_frames.json" OUTPUTS) == 0);
    long size = -1;
    free(fake_love_cli_read_file(CAPTURE, &size));
    assert(size == FRAMES * FRAME_SIZE && "one frame of 9 600 bytes per replay frame");
    char *expected = expected_fixed_hash();
    fake_love_cli_assert_text(HASH, expected);
    free(expected);
    assert(!fake_love_cli_file_exists(ERROR) && "an older failure must not look current");
}

static void test_a_stopped_run_leaves_the_fake_message_only(void) {
    reset();
    write_stale_outputs();
    assert(run(RUN FIXTURES "no_main " FIXTURES "three_frames.json" OUTPUTS) == 0);
    char *error = fake_love_cli_read_file(ERROR, NULL);
    assert(error != NULL && "the error file must be written");
    assert(strncmp(error, "fake_love exited 1\n", strlen("fake_love exited 1\n")) == 0);
    assert(strstr(error, "fake_love: main.lua:") != NULL && "the fake's own message follows");
    assert(strstr(error, "stale") == NULL);
    free(error);
    assert(!fake_love_cli_file_exists(CAPTURE) && "an older capture must not look current");
    assert(!fake_love_cli_file_exists(HASH) && "an older hash must not look current");
}

#define HASH_A "aaaa000000000000000000000000000000000000000000000000000000000001\n"
#define HASH_X "ffff00000000000000000000000000000000000000000000000000000000dead\n"
#define REPORT "sh tools/love_e2e_report.sh"
#define MATCHING " match.json " DIR "/match.ref " DIR "/match.cur " DIR "/match.error"
#define DIVERGING " diverge.json " DIR "/diverge.ref " DIR "/diverge.cur " DIR "/diverge.error"
#define STOPPED " stop.json " DIR "/stop.ref " DIR "/stop.cur " DIR "/stop.error"

static void prepare_report_files(void) {
    reset();
    write_text(DIR "/match.ref", HASH_A);
    write_text(DIR "/match.cur", HASH_A);
    write_text(DIR "/diverge.ref", HASH_A);
    write_text(DIR "/diverge.cur", HASH_X);
    write_text(DIR "/stop.ref", HASH_A);
    write_text(DIR "/stop.error", "fake_love exited 1\nfake_love: frame f2: boom\n");
}

static void test_the_report_names_each_outcome_and_counts_them(void) {
    prepare_report_files();
    assert(run(REPORT MATCHING DIVERGING STOPPED) != 0);
    assert_log_holds("love-e2e: FAILED stop.json -- the fake stopped before the last frame.\n"
                     "love-e2e:   fake_love exited 1\n"
                     "love-e2e:   fake_love: frame f2: boom\n");
    assert_log_holds("frames: match.json: matches (");
    assert_log_holds("frames: DIVERGENT diverge.json");
    assert_log_lacks("stop.json: ");
    assert_log_lacks("make e2e-ref");
    assert_log_lacks("make e2e-baseline");
    assert_log_holds("love-e2e: 1 match, 1 diverge, 1 failed, of 3 replays in tests/e2e\n");
}

static void test_the_report_passes_when_every_replay_matches(void) {
    prepare_report_files();
    assert(run(REPORT MATCHING MATCHING) == 0);
    assert_log_holds("love-e2e: 2 match, 0 diverge, 0 failed, of 2 replays in tests/e2e\n");
}

static void test_a_failure_alone_fails_the_report(void) {
    prepare_report_files();
    assert(run(REPORT MATCHING STOPPED) != 0);
    assert_log_holds("love-e2e: 1 match, 0 diverge, 1 failed, of 2 replays in tests/e2e\n");
}

int main(void) {
    assert(system("rm -rf " DIR " && mkdir -p " DIR) == 0);
    test_a_finished_run_leaves_its_capture_and_hash_only();
    test_a_stopped_run_leaves_the_fake_message_only();
    test_the_report_names_each_outcome_and_counts_them();
    test_the_report_passes_when_every_replay_matches();
    test_a_failure_alone_fails_the_report();
    assert(system("rm -rf " DIR) == 0);
    printf("test_love_e2e: all tests passed\n");
    return 0;
}
