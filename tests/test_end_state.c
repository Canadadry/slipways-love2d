/* tools/end_state.sh, the end-state check of a replay against its committed
 * reference, on hand-written state files: no replay. */

#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define SCRIPT "sh tools/end_state.sh"
#define DIR "tmp/test_end_state_work"
#define OUT DIR "/out.txt"

#define STATE                                                                              \
    "clock year 3410 month 1\n"                                                            \
    "difficulty reasonable\n"                                                              \
    "resources credits 136\n"                                                              \
    "resources science 90\n"                                                               \
    "resources income 17\n"                                                                \
    "techs nanomaterials\n"                                                                \
    "techs space folding\n"                                                                \
    "camera world 127 234\n"                                                               \
    "planet p1 colonized planet x world 193.15 246.73 screen 66.15 12.73 visible 1\n"      \
    "planet p2 unknown planet - world 159.28 30.36 screen 32.28 -203.63 visible 0\n"       \
    "slipway p1 p2 normal\n"                                                               \
    "slipway s1 p1 infragate\n"                                                            \
    "structure s1 slipgate world 10.50 20 screen -116.50 -214 visible 0 links 2\n"         \
    "count planets 2\n"                                                                    \
    "count colonies 1\n"                                                                   \
    "count slipways 2\n"                                                                   \
    "count structures 1\n"                                                                 \
    "count techs 2\n"                                                                      \
    "count nodes 3\n"                                                                      \
    "zone game 10 500 1000\n"                                                              \
    "budget zones 795243 796632\n"                                                         \
    "cap message_box 0 1 6\n"                                                              \
    "cap bytes_button - 3000 6240\n"                                                       \
    "cheat activations 1\n"

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

/* 0 when `end_state.sh <arguments>` exits 0, 1 otherwise; stdout and stderr go to OUT. */
static int run(const char *arguments) {
    char command[1024];
    snprintf(command, sizeof command, SCRIPT " %s > " OUT " 2>&1", arguments);
    int status = system(command);
    return status == 0 ? 0 : 1;
}

static void reset(void) {
    assert(system("rm -rf " DIR " && mkdir -p " DIR) == 0);
}

typedef struct {
    const char *from, *to;
} change_t;

/* STATE with each `from` replaced by its `to` (first occurrence), in order.
 * Static buffer, overwritten by each call. */
static const char *state_with(const change_t *changes, size_t count) {
    static char text[1 << 12];
    static char next[1 << 12];
    snprintf(text, sizeof text, "%s", STATE);
    for (size_t i = 0; i < count; i++) {
        const char *at = strstr(text, changes[i].from);
        assert(at != NULL && "the replaced text is in the state");
        snprintf(next, sizeof next, "%.*s%s%s", (int)(at - text), text, changes[i].to, at + strlen(changes[i].from));
        snprintf(text, sizeof text, "%s", next);
    }
    return text;
}

#define STATE_WITH(changes) state_with(changes, sizeof changes / sizeof changes[0])

static const char NORMALIZED[] =
    "cheat activations 1\n"
    "clock year 3410 month 1\n"
    "count colonies 1\n"
    "count nodes 3\n"
    "count planets 2\n"
    "count slipways 2\n"
    "count structures 1\n"
    "count techs 2\n"
    "difficulty reasonable\n"
    "planet p1 colonized planet x world 193.15 246.73\n"
    "planet p2 unknown planet - world 159.28 30.36\n"
    "resources credits 136\n"
    "resources income 17\n"
    "resources science 90\n"
    "slipway p1 p2 normal\n"
    "slipway s1 p1 infragate\n"
    "structure s1 slipgate world 10.50 20 links 2\n"
    "techs nanomaterials\n"
    "techs space folding\n";

static void write_states(void) {
    static const change_t ignored[] = {
        { "camera world 127 234", "camera world 0 0" },
        { "screen 66.15 12.73 visible 1", "screen 193.15 246.73 visible 0" },
        { "screen -116.50 -214 visible 0", "screen 10.50 20 visible 1" },
        { "zone game 10 500 1000", "zone game 12 900 1000" },
        { "budget zones 795243 796632", "budget zones 1 2" },
        { "cap message_box 0 1 6", "cap message_box 1 3 6" },
    };
    static const change_t reordered[] = {
        { "slipway p1 p2 normal\nslipway s1 p1 infragate\n", "slipway s1 p1 infragate\nslipway p1 p2 normal\n" },
        { "techs nanomaterials\ntechs space folding\n", "techs space folding\ntechs nanomaterials\n" },
        { "clock year 3410 month 1\n", "" },
        { "cheat activations 1\n", "cheat activations 1\nclock year 3410 month 1\n" },
    };
    static const change_t changed[] = {
        { "planet x world", "planet e world" },
        { "world 159.28 30.36", "world 159.29 30.36" },
        { "p1 colonized", "p1 known" },
        { "slipway p1 p2 normal\n", "slipway p1 p2 normal\nslipway p2 s1 normal\n" },
        { "visible 0 links 2", "visible 0 links 3" },
        { "techs nanomaterials", "techs xenofoods" },
        { "count colonies 1", "count colonies 2" },
        { "difficulty reasonable", "difficulty forgiving" },
        { "cheat activations 1", "cheat activations 2" },
        { "year 3410", "year 3411" },
        { "credits 136", "credits 135" },
    };
    static const change_t unknown[] = { { "count nodes 3\n", "count nodes 3\nfuel 12\n" } };
    write_file(DIR "/a.txt", STATE);
    write_file(DIR "/ignored.txt", STATE_WITH(ignored));
    write_file(DIR "/reordered.txt", STATE_WITH(reordered));
    write_file(DIR "/changed.txt", STATE_WITH(changed));
    write_file(DIR "/unknown.txt", STATE_WITH(unknown));
    write_file(DIR "/stopped.txt", "stopped p8_fatal frame 42\n" STATE);
    write_file(DIR "/kept.ref", "planet p9 known planet x world 1 2\n");
}

static void test_normalize_keeps_game_fields_sorted(void) {
    assert(run("normalize " DIR "/a.txt") == 0);
    assert(strcmp(read_file(OUT), NORMALIZED) == 0);
}

static void test_normalize_fails_on_an_unknown_line(void) {
    assert(run("normalize " DIR "/unknown.txt") != 0);
    assert(strstr(read_file(OUT), "unknown line: fuel 12\n") != NULL);
}

static void test_baseline_writes_missing_references_only_from_clean_states(void) {
    assert(run("baseline a.json " DIR "/a.txt " DIR "/a.ref "
               "kept.json " DIR "/a.txt " DIR "/kept.ref "
               "stopped.json " DIR "/stopped.txt " DIR "/stopped.ref "
               "unknown.json " DIR "/unknown.txt " DIR "/unknown.ref "
               "absent.json " DIR "/absent.txt " DIR "/absent.ref") != 0);
    const char *out = read_file(OUT);
    assert(strstr(out, "end-state: a.json reference written " DIR "/a.ref\n") != NULL);
    assert(strstr(out, "end-state: kept.json reference kept " DIR "/kept.ref (delete it to re-baseline)\n") != NULL);
    assert(strstr(out, "end-state: stopped.json stopped\n") != NULL);
    assert(strstr(out, "end-state: unknown.json unknown line: fuel 12\n") != NULL);
    assert(strstr(out, "end-state: absent.json no state\n") != NULL);
    assert(strcmp(read_file(DIR "/a.ref"), NORMALIZED) == 0);
    assert(strcmp(read_file(DIR "/kept.ref"), "planet p9 known planet x world 1 2\n") == 0 && "byte-identical");
    assert(fopen(DIR "/stopped.ref", "r") == NULL && fopen(DIR "/unknown.ref", "r") == NULL &&
           fopen(DIR "/absent.ref", "r") == NULL);
}

static void test_check_passes_on_ignored_fields_and_line_order(void) {
    assert(run("check a.json " DIR "/a.txt " DIR "/a.ref "
               "ignored.json " DIR "/ignored.txt " DIR "/a.ref "
               "reordered.json " DIR "/reordered.txt " DIR "/a.ref") == 0);
    assert(strcmp(read_file(OUT), "end-state: 3 replays match their reference\n") == 0);
}

static void test_check_examines_every_triple_and_names_each_failure(void) {
    assert(run("check changed.json " DIR "/changed.txt " DIR "/a.ref "
               "ignored.json " DIR "/ignored.txt " DIR "/a.ref "
               "stopped.json " DIR "/stopped.txt " DIR "/a.ref "
               "absent.json " DIR "/absent.txt " DIR "/a.ref "
               "unreferenced.json " DIR "/a.txt " DIR "/absent.ref "
               "unknown.json " DIR "/unknown.txt " DIR "/a.ref") != 0);
    const char *out = read_file(OUT);
    assert(strncmp(out, "end-state: changed.json differs\n", 32) == 0);
    static const char *const shown[] = {
        "+planet p1 known planet e world 193.15 246.73\n",
        "+planet p2 unknown planet - world 159.29 30.36\n",
        "+slipway p2 s1 normal\n",
        "+structure s1 slipgate world 10.50 20 links 3\n",
        "+techs xenofoods\n",
        "+count colonies 2\n",
        "+difficulty forgiving\n",
        "+cheat activations 2\n",
        "+clock year 3411 month 1\n",
        "+resources credits 135\n",
        "-resources credits 136\n",
    };
    for (size_t i = 0; i < sizeof shown / sizeof shown[0]; i++) assert(strstr(out, shown[i]) != NULL);
    assert(strstr(out, "+count nodes") == NULL && "an equal line is not shown");
    assert(strstr(out, "end-state: stopped.json stopped\n") != NULL);
    assert(strstr(out, "end-state: absent.json no state\n") != NULL);
    assert(strstr(out, "end-state: unreferenced.json no reference\n") != NULL);
    assert(strstr(out, "end-state: unknown.json unknown line: fuel 12\n") != NULL);
    assert(strstr(out, "ignored.json") == NULL);
    assert(strstr(out, "match their reference") == NULL);
}

int main(void) {
    reset();
    write_states();
    test_normalize_keeps_game_fields_sorted();
    test_normalize_fails_on_an_unknown_line();
    test_baseline_writes_missing_references_only_from_clean_states();
    test_check_passes_on_ignored_fields_and_line_order();
    test_check_examines_every_triple_and_names_each_failure();
    assert(system("rm -rf " DIR) == 0);
    printf("test_end_state: all tests passed\n");
    return 0;
}
