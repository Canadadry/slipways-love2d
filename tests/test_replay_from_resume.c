#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/wait.h>

#define DRIVER "./tmp/replay"
#define RECORD "demo/demo_record.hex"
#define RESUME_EVENTS "tmp/test_replay_from_resume.json"
#define STATE "tmp/test_replay_from_resume_state.txt"
#define DUMP "tmp/test_replay_from_resume_dump.txt"
#define BINDINGS_EVENTS "tmp/test_replay_from_resume_bindings.json"
#define BINDINGS_FIXTURE "tmp/test_replay_from_resume.cfg"
#define STDERR_LOG "tmp/test_replay_from_resume_stderr.txt"
#define ATTRACT_STATE "tmp/test_replay_from_resume_attract.txt"

enum { RESUME_FRAMES = 30 };

/* Allocated buffer, NUL-terminated, freed by the caller. */
static char *read_whole_file(const char *path) {
    FILE *file = fopen(path, "rb");
    assert(file != NULL);
    assert(fseek(file, 0, SEEK_END) == 0);
    long size = ftell(file);
    assert(size >= 0);
    rewind(file);
    char *buf = malloc((size_t)size + 1);
    assert(buf != NULL);
    assert(fread(buf, 1, (size_t)size, file) == (size_t)size);
    buf[size] = '\0';
    fclose(file);
    return buf;
}

static void write_resume_events(const char *path, const char *extra_member) {
    char *hex = read_whole_file(RECORD);
    hex[strcspn(hex, "\n")] = '\0';
    FILE *out = fopen(path, "w");
    assert(out != NULL);
    fprintf(out, "{\n  \"input\": \"pad\",\n  \"start\": \"resume\",\n  \"initial_save\": \"%s\",\n%s  \"frames\": [\n", hex,
            extra_member);
    for (int i = 0; i < RESUME_FRAMES; i++) {
        fprintf(out, "    { \"pad\": [] }%s\n", i < RESUME_FRAMES - 1 ? "," : "");
    }
    fputs("  ]\n}\n", out);
    fclose(out);
    free(hex);
}

static int run(const char *command) {
    int status = system(command);
    assert(status != -1 && WIFEXITED(status));
    return WEXITSTATUS(status);
}

static int assert_refused(const char *command, const char *reason) {
    char full[512];
    remove(STDERR_LOG);
    snprintf(full, sizeof full, "%s > /dev/null 2> " STDERR_LOG, command);
    int status = run(full);
    assert(status != 0 && "the driver refuses");
    char *err = read_whole_file(STDERR_LOG);
    if (strncmp(err, "replay: ", 8) != 0 || strstr(err, reason) == NULL) {
        fprintf(stderr, "test_replay_from_resume: got \"%s\", want \"replay: ...%s...\"\n", err, reason);
        assert(0 && "the refusal is a replay: message naming the reason");
    }
    free(err);
    return status;
}

static void test_a_resume_start_plays_a_running_game(void) {
    write_resume_events(RESUME_EVENTS, "");
    remove(STATE);
    assert(run("STATE=" STATE " " DRIVER " " RESUME_EVENTS " tmp/test_replay_from_resume.bin " DUMP " > /dev/null") == 0);
    char *state = read_whole_file(STATE);
    assert(strstr(state, "count colonies ") != NULL);
    assert(strstr(state, "count colonies 0\n") == NULL && "the record's colonies are there, not the title's empty map");
    free(state);
}

/* A fixture a title-start file could play: only the resume start refuses it. */
static void test_a_resume_start_with_bindings_is_refused(void) {
    assert(run("cp bindings.default.cfg " BINDINGS_FIXTURE) == 0);
    write_resume_events(BINDINGS_EVENTS, "  \"bindings\": \"test_replay_from_resume.cfg\",\n");
    assert_refused(DRIVER " " BINDINGS_EVENTS, "default bindings");
}

static void test_two_starts_reach_the_same_normalised_state(void) {
    write_resume_events(RESUME_EVENTS, "");
    remove(STATE);
    remove(STATE ".start1");
    assert(run("REPLAY_STARTS=2 STATE=" STATE " " DRIVER " " RESUME_EVENTS " > /dev/null") == 0);
    assert(run("sh tools/end_state.sh normalize " STATE ".start1 > " STATE ".start1.normal") == 0);
    assert(run("sh tools/end_state.sh normalize " STATE " > " STATE ".normal") == 0);
    char *first = read_whole_file(STATE ".start1.normal");
    char *second = read_whole_file(STATE ".normal");
    assert(strstr(first, "count colonies 0\n") == NULL && "the first start ends in the record's game");
    assert(strcmp(first, second) == 0 && "the second start ends where the first did");
    free(first);
    free(second);
}

static void test_two_starts_write_an_explicit_dump_after_no_capture(void) {
    write_resume_events(RESUME_EVENTS, "");
    remove(DUMP);
    assert(run("REPLAY_STARTS=2 " DRIVER " " RESUME_EVENTS " /dev/null " DUMP " > /dev/null") == 0);
    char *dump = read_whole_file(DUMP);
    assert(dump[0] != '\0' && "the dump goes to the path after /dev/null");
    free(dump);
}

static void test_replay_starts_refusals(void) {
    write_resume_events(RESUME_EVENTS, "");
    assert_refused("REPLAY_STARTS=3 " DRIVER " " RESUME_EVENTS, "REPLAY_STARTS");
    assert_refused("REPLAY_STARTS=2 " DRIVER " tests/fixtures/title_pick_reasonable.json", "resume");
    assert_refused("REPLAY_STARTS=2 " DRIVER " " RESUME_EVENTS " tmp/test_replay_from_resume.bin", "capture");
    assert_refused("REPLAY_STARTS=2 PAD_METRICS=tmp/test_replay_from_resume_metrics.txt " DRIVER " " RESUME_EVENTS,
                   "PAD_METRICS");
}

static void test_attract_state_refusals(void) {
    write_resume_events(RESUME_EVENTS, "");
    remove(ATTRACT_STATE ".1");
    assert(assert_refused("ATTRACT_STATE=" ATTRACT_STATE " " DRIVER " " RESUME_EVENTS " /dev/null " DUMP,
                          "ATTRACT_STATE") == 2);
    assert(assert_refused("REPLAY_STARTS=2 ATTRACT_STATE=" ATTRACT_STATE " " DRIVER
                          " tests/fixtures/title_pick_reasonable.json",
                          "ATTRACT_STATE") == 2);
    assert(fopen(ATTRACT_STATE ".1", "r") == NULL && "a refused run writes no state");
}

int main(void) {
    test_a_resume_start_plays_a_running_game();
    test_a_resume_start_with_bindings_is_refused();
    test_two_starts_reach_the_same_normalised_state();
    test_two_starts_write_an_explicit_dump_after_no_capture();
    test_replay_starts_refusals();
    test_attract_state_refusals();
    remove(RESUME_EVENTS);
    remove(STATE);
    remove(STATE ".start1");
    remove(STATE ".normal");
    remove(STATE ".start1.normal");
    remove(DUMP);
    remove(BINDINGS_EVENTS);
    remove(STDERR_LOG);
    remove(BINDINGS_FIXTURE);
    remove("tmp/test_replay_from_resume.bin");
    printf("test_replay_from_resume: all tests passed\n");
    return 0;
}
