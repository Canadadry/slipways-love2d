/* The replay driver has no default events file: without one, it prints
 * its usage and fails instead of silently replaying something. */

#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/wait.h>

#define DRIVER "./tmp/replay"
#define STDERR_LOG "tmp/test_replay_usage_stderr.txt"

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

static void assert_usage_and_failure(const char *args) {
    char cmd[256];
    remove(STDERR_LOG);
    snprintf(cmd, sizeof cmd, DRIVER "%s > /dev/null 2> " STDERR_LOG, args);
    int status = system(cmd);
    assert(status != -1 && WIFEXITED(status));
    assert(WEXITSTATUS(status) != 0 && "no events file must be a failure");
    char *err = read_whole_file(STDERR_LOG);
    assert(strstr(err, "usage") != NULL && "the failure must print the usage");
    free(err);
}

static void test_no_argument_prints_usage_and_fails(void) {
    assert_usage_and_failure("");
}

/* The driver takes no option: an argument starting with a dash is not
 * taken for an events file. */
static void test_option_prints_usage_and_fails(void) {
    assert_usage_and_failure(" --from-title tests/fixtures/title_pick_forgiving.json");
}

int main(void) {
    test_no_argument_prints_usage_and_fails();
    test_option_prints_usage_and_fails();
    remove(STDERR_LOG);
    printf("test_replay_usage: all tests passed\n");
    return 0;
}
