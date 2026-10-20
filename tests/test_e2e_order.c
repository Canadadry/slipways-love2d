/* tools/e2e_order.sh, the ranking of replays by lines executed, driven with
 * a stub coverage script that copies a canned lines.txt per events name. */

#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define DIR "tmp/test_e2e_order_work"
#define STUB DIR "/coverage_stub.sh"
#define WORK DIR "/work"
#define OUT DIR "/order.txt"
#define LOG DIR "/log.txt"

static void write_file(const char *path, const char *text) {
    FILE *file = fopen(path, "w");
    assert(file != NULL);
    fputs(text, file);
    fclose(file);
}

/* Static buffer, overwritten by each call; NULL when the file is missing. */
static const char *read_file(const char *path) {
    FILE *file = fopen(path, "rb");
    if (file == NULL) return NULL;
    static char buf[1 << 16];
    size_t len = fread(buf, 1, sizeof(buf) - 1, file);
    fclose(file);
    buf[len] = '\0';
    return buf;
}

/* The stub takes coverage.sh's arguments and a single events file, copies
 * DIR/<events base name>.lines to <out dir>/lines.txt, and fails when that
 * canned file is missing. */
static void reset(void) {
    assert(system("rm -rf " DIR " && mkdir -p " DIR) == 0);
    write_file(STUB, "set -u\n"
                     "out_dir=$3\n"
                     "shift 4\n"
                     "canned=" DIR "/$(basename \"$1\").lines\n"
                     "[ -f \"$canned\" ] || { echo \"coverage: $1 stopped\"; exit 1; }\n"
                     "mkdir -p \"$out_dir\"\n"
                     "cp \"$canned\" \"$out_dir/lines.txt\"\n");
}

/* `hit` of `total` rows of lines.txt are hit. */
static void write_lines(const char *name, int hit, int total) {
    char path[256];
    snprintf(path, sizeof path, DIR "/%s.json.lines", name);
    FILE *file = fopen(path, "w");
    assert(file != NULL);
    for (int i = 0; i < total; i++) fprintf(file, "src/game/x.c:%d %d\n", i + 1, i < hit ? 1 : 0);
    fclose(file);
}

static int output_is(const char *expected) {
    const char *text = read_file(OUT);
    assert(text != NULL);
    return strcmp(text, expected) == 0;
}

static int run(const char *names) {
    char cmd[1024];
    snprintf(cmd, sizeof cmd,
             "sh tools/e2e_order.sh " STUB " replay obj " WORK " 'a.c b.c' " OUT " %s > " LOG " 2>&1", names);
    return system(cmd);
}

static void test_replays_come_out_by_lines_ascending_with_base_names(void) {
    reset();
    write_lines("big", 30, 30);
    write_lines("small", 10, 10);
    write_lines("middle", 20, 20);
    assert(run("some/dir/big.json some/dir/small.json other/middle.json") == 0);
    assert(output_is("small.json 10\nmiddle.json 20\nbig.json 30\n"));
}

static void test_equal_counts_are_ordered_by_name(void) {
    reset();
    write_lines("b", 10, 10);
    write_lines("c", 5, 5);
    write_lines("a", 10, 10);
    assert(run(DIR "/b.json " DIR "/c.json " DIR "/a.json") == 0);
    assert(output_is("c.json 5\na.json 10\nb.json 10\n"));
}

static void test_the_count_excludes_rows_not_hit(void) {
    reset();
    write_lines("half", 7, 20);
    write_lines("none", 0, 20);
    assert(run(DIR "/half.json " DIR "/none.json") == 0);
    assert(output_is("none.json 0\nhalf.json 7\n"));
}

static void test_a_failed_run_is_named_and_leaves_the_output_untouched(void) {
    reset();
    write_lines("first", 10, 10);
    write_lines("third", 30, 30);
    write_file(OUT, "previous order\n");
    assert(run(DIR "/first.json " DIR "/second.json " DIR "/third.json") != 0);
    assert(strstr(read_file(LOG), "second.json") != NULL);
    assert(output_is("previous order\n"));
}

static void test_a_rerun_gives_the_same_file(void) {
    reset();
    write_lines("x", 3, 9);
    write_lines("y", 3, 4);
    write_lines("z", 1, 1);
    assert(run(DIR "/x.json " DIR "/y.json " DIR "/z.json") == 0);
    char first[256];
    snprintf(first, sizeof first, "%s", read_file(OUT));
    assert(run(DIR "/z.json " DIR "/y.json " DIR "/x.json") == 0);
    assert(output_is(first));
    assert(output_is("z.json 1\nx.json 3\ny.json 3\n"));
}

int main(void) {
    test_replays_come_out_by_lines_ascending_with_base_names();
    test_equal_counts_are_ordered_by_name();
    test_the_count_excludes_rows_not_hit();
    test_a_failed_run_is_named_and_leaves_the_output_untouched();
    test_a_rerun_gives_the_same_file();
    assert(system("rm -rf " DIR) == 0);
    printf("test_e2e_order: all tests passed\n");
    return 0;
}
