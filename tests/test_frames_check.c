/* tools/frames_check.sh, the hash comparison of `make e2e`, on built hashes:
 * it is tested by what it must prevent. */

#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define SCRIPT "sh tools/frames_check.sh"
#define DIR "tmp/test_frames_check_work"
#define LOG DIR "/log.txt"

#define A_EVENTS "alpha_events.json"
#define A_REF DIR "/alpha.sha256"
#define A_CUR DIR "/alpha.cur"
#define B_EVENTS "beta_events.json"
#define B_REF DIR "/beta.sha256"
#define B_CUR DIR "/beta.cur"

#define ARGS " " A_EVENTS " " A_REF " " A_CUR " " B_EVENTS " " B_REF " " B_CUR

#define HASH_A "aaaa000000000000000000000000000000000000000000000000000000000001"
#define HASH_B "bbbb000000000000000000000000000000000000000000000000000000000002"
#define HASH_X "ffff00000000000000000000000000000000000000000000000000000000dead"

/* Allocated buffer, NUL-terminated: the caller frees it. */
static char *read_log(void) {
    FILE *file = fopen(LOG, "rb");
    assert(file != NULL);
    static char buf[1 << 16];
    size_t len = fread(buf, 1, sizeof(buf) - 1, file);
    fclose(file);
    buf[len] = '\0';
    char *copy = malloc(len + 1);
    assert(copy != NULL);
    memcpy(copy, buf, len + 1);
    return copy;
}

static void write_hash(const char *path, const char *hash) {
    FILE *file = fopen(path, "w");
    assert(file != NULL);
    fprintf(file, "%s\n", hash);
    fclose(file);
}

static int run_check(void) {
    return system(SCRIPT ARGS " > " LOG " 2>&1");
}

static void reset(void) {
    assert(system("rm -rf " DIR " && mkdir -p " DIR) == 0);
    write_hash(A_CUR, HASH_A);
    write_hash(B_CUR, HASH_B);
}

static void test_matching_references_pass_and_are_announced(void) {
    reset();
    write_hash(A_REF, HASH_A);
    write_hash(B_REF, HASH_B);
    assert(run_check() == 0);
    char *log = read_log();
    assert(strstr(log, A_EVENTS ": matches") != NULL);
    assert(strstr(log, B_EVENTS ": matches") != NULL);
    assert(strstr(log, "DIVERGENT") == NULL);
    free(log);
}

/* The first diverging replay does not stop the examination: the second is
 * reported too. */
static void test_every_divergent_replay_is_named_with_both_hashes(void) {
    reset();
    write_hash(A_REF, HASH_X);
    write_hash(B_REF, HASH_X);
    assert(run_check() != 0);
    char *log = read_log();
    assert(strstr(log, "DIVERGENT " A_EVENTS) != NULL);
    assert(strstr(log, "DIVERGENT " B_EVENTS) != NULL);
    assert(strstr(log, HASH_X) != NULL);
    assert(strstr(log, HASH_A) != NULL);
    assert(strstr(log, HASH_B) != NULL);
    assert(strstr(log, "make e2e-ref") != NULL);
    free(log);
}

static void test_only_the_divergent_replay_is_named(void) {
    reset();
    write_hash(A_REF, HASH_A);
    write_hash(B_REF, HASH_X);
    assert(run_check() != 0);
    char *log = read_log();
    assert(strstr(log, A_EVENTS ": matches") != NULL);
    assert(strstr(log, "DIVERGENT " B_EVENTS) != NULL);
    assert(strstr(log, "DIVERGENT " A_EVENTS) == NULL);
    free(log);
}

static void test_missing_reference_is_not_compared_and_says_how_to_set_it(void) {
    reset();
    write_hash(B_REF, HASH_B);
    assert(system("rm -f " A_CUR) == 0);
    assert(run_check() == 0);
    char *log = read_log();
    assert(strstr(log, A_REF) != NULL);
    assert(strstr(log, "make e2e-baseline") != NULL);
    assert(strstr(log, A_EVENTS ": matches") == NULL);
    assert(strstr(log, B_EVENTS ": matches") != NULL);
    free(log);
}

/* A replay that produced nothing does not pass for right. */
static void test_missing_current_hash_fails(void) {
    reset();
    write_hash(A_REF, HASH_A);
    write_hash(B_REF, HASH_B);
    assert(system("rm -f " A_CUR) == 0);
    assert(run_check() != 0);
    char *log = read_log();
    assert(strstr(log, A_CUR) != NULL);
    assert(strstr(log, A_EVENTS ": matches") == NULL);
    assert(strstr(log, B_EVENTS ": matches") != NULL);
    free(log);
}

int main(void) {
    test_matching_references_pass_and_are_announced();
    test_every_divergent_replay_is_named_with_both_hashes();
    test_only_the_divergent_replay_is_named();
    test_missing_reference_is_not_compared_and_says_how_to_set_it();
    test_missing_current_hash_fails();
    assert(system("rm -rf " DIR) == 0);
    printf("test_frames_check: all tests passed\n");
    return 0;
}
