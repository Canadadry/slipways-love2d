/* tools/state_peaks.sh, the peak table of the make state campaign, on
 * hand-written state files: no replay. */

#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define SCRIPT "sh tools/state_peaks.sh"
#define DIR "tmp/test_state_peaks_work"
#define OUT DIR "/state.txt"
#define LOG DIR "/log.txt"

static void write_file(const char *path, const char *text) {
    FILE *file = fopen(path, "w");
    assert(file != NULL);
    fputs(text, file);
    fclose(file);
}

/* Static buffer, overwritten by each call. */
static const char *read_file(const char *path) {
    FILE *file = fopen(path, "rb");
    assert(file != NULL);
    static char buf[1 << 16];
    size_t len = fread(buf, 1, sizeof(buf) - 1, file);
    fclose(file);
    buf[len] = '\0';
    return buf;
}

/* `pairs` is a list of `<events> <state>` arguments. */
static int run(const char *pairs) {
    char cmd[1024];
    snprintf(cmd, sizeof cmd, SCRIPT " " OUT " %s > " LOG " 2>&1", pairs);
    return system(cmd);
}

static void reset(void) {
    assert(system("rm -rf " DIR " && mkdir -p " DIR) == 0);
}

static void test_highest_peak_is_kept_with_its_replay_and_limit(void) {
    reset();
    write_file(DIR "/a.txt",
               "clock year 3426 month 1\n"
               "zone game 10 500 1000\n"
               "cap message_box 0 0 6\n"
               "cap bytes_button - 3000 6240\n");
    write_file(DIR "/b.txt",
               "zone game 5 300 1000\n"
               "cap message_box 1 2 6\n"
               "cap bytes_button - 3768 6240\n");
    assert(run("a.json " DIR "/a.txt b.json " DIR "/b.txt") == 0);
    const char *out = read_file(OUT);
    assert(strstr(out, "replay a.json clean\n") != NULL);
    assert(strstr(out, "replay b.json clean\n") != NULL);
    assert(strstr(out, "zone game 500 1000 a.json\n") != NULL);
    assert(strstr(out, "cap message_box 2 6 b.json\n") != NULL);
    assert(strstr(out, "cap bytes_button 3768 6240 b.json\n") != NULL);
    assert(strstr(out, "clock") == NULL);
    assert(strstr(out, "replay b.json") < strstr(out, "zone game"));
    assert(strstr(out, "zone game") < strstr(out, "cap message_box"));
}

static void test_tie_keeps_the_first_replay(void) {
    reset();
    write_file(DIR "/a.txt", "cap toast 0 13 128\n");
    write_file(DIR "/b.txt", "cap toast 0 13 128\n");
    assert(run("b.json " DIR "/b.txt a.json " DIR "/a.txt") == 0);
    assert(strstr(read_file(OUT), "cap toast 13 128 b.json\n") != NULL);
}

static void test_stopped_replay_fails_is_named_and_table_still_written(void) {
    reset();
    write_file(DIR "/a.txt", "zone game 1 100 1000\n");
    write_file(DIR "/b.txt",
               "stopped p8_fatal frame 42\n"
               "zone game 1 800 1000\n"
               "cap toast 0 10 10\n");
    assert(run("a.json " DIR "/a.txt b.json " DIR "/b.txt") != 0);
    const char *out = read_file(OUT);
    assert(strstr(out, "replay a.json clean\n") != NULL);
    assert(strstr(out, "replay b.json stopped p8_fatal frame 42\n") != NULL);
    assert(strstr(out, "zone game 800 1000 b.json\n") != NULL);
    const char *log = read_file(LOG);
    assert(strstr(log, "state: b.json stopped") != NULL);
    assert(strstr(log, "state: a.json") == NULL);
    assert(strstr(log, "state: zone game") == NULL);
    assert(strstr(log, "state: cap toast peak 10 over margin 8 (limit 10, b.json)\n") != NULL);
}

static void test_missing_state_fails(void) {
    reset();
    write_file(DIR "/a.txt", "zone game 1 100 1000\n");
    assert(run("a.json " DIR "/a.txt b.json " DIR "/absent.txt") != 0);
    assert(strstr(read_file(OUT), "replay b.json no state\n") != NULL);
    assert(strstr(read_file(LOG), "state: b.json stopped") != NULL);
}

static void test_budget_line_is_copied_and_bytes_left_printed(void) {
    reset();
    write_file(DIR "/a.txt", "zone game 1 100 1000\nbudget zones 784452 786432\n");
    write_file(DIR "/b.txt", "zone game 1 50 1000\nbudget zones 784452 786432\n");
    assert(run("a.json " DIR "/a.txt b.json " DIR "/b.txt") == 0);
    const char *out = read_file(OUT);
    const char *budget = strstr(out, "budget zones 784452 786432\n");
    assert(budget != NULL);
    assert(strstr(budget + 1, "budget zones") == NULL && "copied once");
    assert(strstr(read_file(LOG), "state: 1980 bytes left under ZONE_BUDGET_BYTES\n") != NULL);
}

static void test_no_budget_line_prints_no_bytes_left(void) {
    reset();
    write_file(DIR "/a.txt", "zone game 1 100 1000\n");
    assert(run("a.json " DIR "/a.txt") == 0);
    assert(strstr(read_file(OUT), "budget") == NULL);
    assert(strstr(read_file(LOG), "bytes left") == NULL);
}

static void test_peak_at_plan_7_sizing_passes_and_one_more_fails(void) {
    reset();
    write_file(DIR "/a.txt", "zone load 1 8 9\ncap toast 0 9 10\n");
    assert(run("a.json " DIR "/a.txt") != 0);
    const char *log = read_file(LOG);
    assert(strstr(log, "state: zone load") == NULL && "9 x 8 <= 8 x 9");
    assert(strstr(log, "state: cap toast peak 9 over margin 8 (limit 10, a.json)\n") != NULL);
}

static void test_every_failing_line_is_named_and_table_still_written(void) {
    reset();
    write_file(DIR "/a.txt", "zone game 1 100 1000\nzone ui 1 950 1000\ncap toast 0 128 128\n");
    assert(run("a.json " DIR "/a.txt") != 0);
    const char *out = read_file(OUT);
    assert(strstr(out, "replay a.json clean\n") != NULL);
    assert(strstr(out, "zone game 100 1000 a.json\n") != NULL);
    assert(strstr(out, "zone ui 950 1000 a.json\n") != NULL);
    assert(strstr(out, "cap toast 128 128 a.json\n") != NULL);
    assert(strstr(out, "over margin") == NULL);
    const char *log = read_file(LOG);
    assert(strstr(log, "state: zone ui peak 950 over margin 888 (limit 1000, a.json)\n") != NULL);
    assert(strstr(log, "state: cap toast peak 128 over margin 113 (limit 128, a.json)\n") != NULL);
    assert(strstr(log, "state: zone game") == NULL);
}

static void test_proven_ceiling_is_judged_on_its_limit(void) {
    reset();
    write_file(DIR "/a.txt", "cap particles_help_screen 1 1 1\ncap bytes_button - 6240 6240\ncap bytes_row - 96 96\n");
    assert(run("a.json " DIR "/a.txt") == 0);
    write_file(DIR "/a.txt", "cap message_box 1 1 1\n");
    assert(run("a.json " DIR "/a.txt") != 0);
    assert(strstr(read_file(LOG), "state: cap message_box peak 1 over margin 0 (limit 1, a.json)\n") != NULL);
}

static void test_fully_reserved_slot_is_judged_on_its_limit(void) {
    reset();
    write_file(DIR "/a.txt",
               "cap bytes_particles - 1384 1384\n"
               "cap bytes_particles_help_screen - 5344 5344\n"
               "cap bytes_ephemerals - 184 184\n");
    assert(run("a.json " DIR "/a.txt") == 0);
    write_file(DIR "/a.txt", "cap bytes_burst - 640 640\n");
    assert(run("a.json " DIR "/a.txt") != 0);
    assert(strstr(read_file(LOG), "state: cap bytes_burst peak 640 over margin 568 (limit 640, a.json)\n") != NULL);
}

static void test_exempt_line_over_its_limit_fails(void) {
    reset();
    write_file(DIR "/a.txt", "cap bytes_row - 97 96\n");
    assert(run("a.json " DIR "/a.txt") != 0);
    assert(strstr(read_file(LOG), "state: cap bytes_row peak 97 over margin 85 (limit 96, a.json)\n") != NULL);
}


static void test_gpu_frame_peak_over_its_margin_fails(void) {
    reset();
    write_file(DIR "/a.txt", "zone gpu_frame 100 9000 10240\nzone gpu_build 0 8000 9216\ncap gpu_cluts - 5 6\n"
                             "cap gpu_patterns - 3 4\n");
    write_file(DIR "/b.txt", "zone gpu_frame 100 9200 10240\nzone gpu_build 0 8200 9216\ncap gpu_cluts - 6 6\n"
                             "cap gpu_patterns - 4 4\n");
    assert(run("a.json " DIR "/a.txt") == 0);
    assert(run("a.json " DIR "/a.txt b.json " DIR "/b.txt") != 0);
    const char *out = read_file(OUT);
    assert(strstr(out, "zone gpu_frame 9200 10240 b.json\n") != NULL);
    const char *log = read_file(LOG);
    assert(strstr(log, "state: zone gpu_frame peak 9200 over margin 9102 (limit 10240, b.json)\n") != NULL);
    assert(strstr(log, "state: zone gpu_build peak 8200 over margin 8192 (limit 9216, b.json)\n") != NULL);
    assert(strstr(log, "state: cap gpu_cluts peak 6 over margin 5 (limit 6, b.json)\n") != NULL);
    assert(strstr(log, "state: cap gpu_patterns peak 4 over margin 3 (limit 4, b.json)\n") != NULL);
}

static void test_gpu_buffers_are_printed_after_the_bytes_left(void) {
    reset();
    write_file(DIR "/a.txt", "zone game 1 100 1000\nbudget zones 784452 786432\nzone gpu_frame 100 9000 10240\n");
    assert(run("a.json " DIR "/a.txt") == 0);
    const char *log = read_file(LOG);
    const char *left = strstr(log, "state: 1980 bytes left under ZONE_BUDGET_BYTES\n");
    assert(left != NULL);
    assert(strstr(log, "state: GPU primitive buffers 2 x 10240 = 20480 bytes, outside ZONE_BUDGET_BYTES\n")
           == left + strlen("state: 1980 bytes left under ZONE_BUDGET_BYTES\n"));
}

int main(void) {
    test_highest_peak_is_kept_with_its_replay_and_limit();
    test_tie_keeps_the_first_replay();
    test_stopped_replay_fails_is_named_and_table_still_written();
    test_missing_state_fails();
    test_budget_line_is_copied_and_bytes_left_printed();
    test_no_budget_line_prints_no_bytes_left();
    test_peak_at_plan_7_sizing_passes_and_one_more_fails();
    test_every_failing_line_is_named_and_table_still_written();
    test_proven_ceiling_is_judged_on_its_limit();
    test_fully_reserved_slot_is_judged_on_its_limit();
    test_exempt_line_over_its_limit_fails();
    test_gpu_frame_peak_over_its_margin_fails();
    test_gpu_buffers_are_printed_after_the_bytes_left();
    assert(system("rm -rf " DIR) == 0);
    printf("test_state_peaks: all tests passed\n");
    return 0;
}
