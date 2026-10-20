#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "fake_love_cli.h"

#define FIXTURES FAKE_LOVE_CLI_FIXTURES
#define CAPTURE "tmp/test_fake_love_files_capture.bin"
#define STATE "tmp/test_fake_love_files_state.txt"
#define STDERR_LOG "tmp/test_fake_love_files_stderr.txt"
#define ECHO_GAME FIXTURES "disk_echo"

#define GAME_LINES                                                                     \
    "missing.bin info nil read nil Could not open file missing.bin. Does not exist.\n" \
    "readme.txt info file 12 read 12 67616d6520726561646d650a\n"                       \
    "data/level.txt info file 8 read 8 6c6576656c20310a\n"                             \
    "main.lua file\n"                                                                  \
    "data as file info nil\n"                                                          \
    "data as directory info directory nil\n"                                           \
    "data info directory nil read nil Could not open file data. Does not exist.\n"     \
    "write note.txt true nil\n"                                                        \
    "note.txt info file 5 read 5 68656c6c6f\n"                                         \
    "write empty.txt true nil\n"                                                       \
    "empty.txt info file 0 read 0 \n"                                                  \
    "write readme.txt true nil\n"                                                      \
    "readme.txt info file 4 read 4 6d696e65\n"                                         \
    "write saves/a.bin false Could not open file saves/a.bin. Does not exist.\n"       \
    "saves/a.bin info nil read nil Could not open file saves/a.bin. Does not exist.\n" \
    "write save.bin true nil\n"                                                        \
    "save.bin info file 2 read 2 00ff\n"                                               \
    "save.bin info file 2 read 2 00ff\n"                                               \
    "note.txt info file 5 read 5 68656c6c6f\n"

static void assert_state(const char *expected) {
    fake_love_cli_assert_text(STATE, expected);
}

static void assert_runs(const char *environment, const char *args) {
    int status = fake_love_cli_run(environment, args, CAPTURE, STATE, STDERR_LOG);
    if (status != 0) {
        char *err = fake_love_cli_read_file(STDERR_LOG, NULL);
        fprintf(stderr, "fake_love %s: status %d\n%s", args, status, err != NULL ? err : "");
        free(err);
        assert(!"the run must succeed");
    }
}

static void assert_file_holds(const char *path, const char *expected) {
    long size;
    char *content = fake_love_cli_read_file(path, &size);
    assert(content != NULL && (size_t)size == strlen(expected) && memcmp(content, expected, (size_t)size) == 0 &&
           "a run must leave the fixtures unchanged");
    free(content);
}

static void assert_no_side_effects(void) {
    assert(!fake_love_cli_file_exists(ECHO_GAME "/note.txt") && !fake_love_cli_file_exists("note.txt") &&
           !fake_love_cli_file_exists(ECHO_GAME "/save.bin") && !fake_love_cli_file_exists("save.bin") &&
           "the game's writes must stay in memory");
    assert_file_holds(ECHO_GAME "/readme.txt", "game readme\n");
    assert_file_holds(ECHO_GAME "/data/level.txt", "level 1\n");
    assert_file_holds(FIXTURES "disk_bindings.cfg", "aim a\nmenu b\nswap x\n");
}

static void assert_failure(const char *environment, const char *args, const char *const *needles) {
    fake_love_cli_assert_failure(environment, args, CAPTURE, STATE, STDERR_LOG, needles);
}

static void test_bare_replay_shows_only_the_game_directory(void) {
    assert_runs("", ECHO_GAME " " FIXTURES "disk_bare.json " CAPTURE " " STATE);
    assert_state("conf filesystem nil\n"
                 "arg 0\n"
                 "global arg nil\n"
                 "filesystem getInfo read write\n"
                 "save.bin info nil read nil Could not open file save.bin. Does not exist.\n"
                 "bindings.cfg info nil read nil Could not open file bindings.cfg. Does not exist.\n" GAME_LINES);
    assert_no_side_effects();
}

static void test_full_replay_seeds_the_save_and_the_bindings(void) {
    assert_runs("", ECHO_GAME " " FIXTURES "disk_full.json " CAPTURE " " STATE);
    assert_state("conf filesystem nil\n"
                 "arg 2 \"--attract-after\" \"7\"\n"
                 "global arg nil\n"
                 "filesystem getInfo read write\n"
                 "save.bin info file 4 read 4 00ff10a5\n"
                 "bindings.cfg info file 20 read 20 61696d20610a6d656e7520620a7377617020780a\n" GAME_LINES);
    assert_no_side_effects();
}

static void test_missing_bindings_fixture_runs_nothing(void) {
    const char *const needles[] = {FIXTURES "disk_missing_bindings.json", "\"bindings\"",
                                   FIXTURES "absent.cfg cannot be read", NULL};
    assert_failure("", ECHO_GAME " " FIXTURES "disk_missing_bindings.json " CAPTURE " " STATE, needles);
}

#define BIG_REPLAY "tmp/test_fake_love_files_big.json"
#define BIG_FIXTURE "tmp/test_fake_love_files_big.cfg"

static void test_bindings_fixture_over_64_kib_runs_nothing(void) {
    FILE *replay = fopen(BIG_REPLAY, "wb");
    assert(replay != NULL);
    fputs("{\"input\": \"pad\", \"bindings\": \"test_fake_love_files_big.cfg\", \"frames\": [{\"pad\": []}]}",
          replay);
    assert(fclose(replay) == 0);
    FILE *fixture = fopen(BIG_FIXTURE, "wb");
    assert(fixture != NULL);
    for (int i = 0; i < 65537; i++)
        fputc('#', fixture);
    assert(fclose(fixture) == 0);
    const char *const needles[] = {BIG_REPLAY, "\"bindings\"", BIG_FIXTURE " is over 65536 bytes", NULL};
    assert_failure("", ECHO_GAME " " BIG_REPLAY " " CAPTURE " " STATE, needles);
    remove(BIG_REPLAY);
    remove(BIG_FIXTURE);
}

#define SIZES_GAME FIXTURES "disk_sizes"

/* The save counts are the ones test_fake_love_replay reads back. */
static void test_e2e_replays_reach_the_game(void) {
    long bindings_size;
    char *bindings = fake_love_cli_read_file("tests/e2e/unbound_enter_aim.cfg", &bindings_size);
    assert(bindings != NULL);
    free(bindings);
    char expected[256];
    snprintf(expected, sizeof expected,
             "arg 0\n"
             "save.bin info 4186 read 4186 4186\n"
             "bindings.cfg info %ld read %ld %ld\n",
             bindings_size, bindings_size, bindings_size);
    assert_runs("", SIZES_GAME " tests/e2e/pad_unbound_enter_aim.json " CAPTURE " " STATE);
    assert_state(expected);
    assert_runs("", SIZES_GAME " tests/e2e/attract.json " CAPTURE " " STATE);
    assert_state("arg 2 \"--attract-after\" \"3\"\n"
                 "save.bin info 3921 read 3921 3921\n"
                 "bindings.cfg info nil read nil\n");
}

#define MISUSE FIXTURES "disk_misuse " FIXTURES "disk_bare.json " CAPTURE " " STATE

static void test_filesystem_misuse_is_fatal(void) {
    const char *const path[] = {"love.load: ", "main.lua:5: love.filesystem.read: invalid path \"../x\"", NULL};
    assert_failure("FAKE_LOVE_MISUSE='dot dot'", MISUSE, path);
    const char *const no_name[] = {"main.lua:7: love.filesystem.read", NULL};
    assert_failure("FAKE_LOVE_MISUSE='no name'", MISUSE, no_name);
    const char *const number_data[] = {"main.lua:9: love.filesystem.write", "number", NULL};
    assert_failure("FAKE_LOVE_MISUSE='number data'", MISUSE, number_data);
    const char *const bogus_filter[] = {"main.lua:11: love.filesystem.getInfo", "bogus", NULL};
    assert_failure("FAKE_LOVE_MISUSE='bogus filter'", MISUSE, bogus_filter);
    const char *const size_form[] = {"main.lua:13: love.filesystem.read", NULL};
    assert_failure("FAKE_LOVE_MISUSE='size form'", MISUSE, size_form);
}

int main(void) {
    test_bare_replay_shows_only_the_game_directory();
    test_full_replay_seeds_the_save_and_the_bindings();
    test_e2e_replays_reach_the_game();
    test_missing_bindings_fixture_runs_nothing();
    test_bindings_fixture_over_64_kib_runs_nothing();
    test_filesystem_misuse_is_fatal();
    remove(CAPTURE);
    remove(STATE);
    remove(STDERR_LOG);
    printf("test_fake_love_files: all tests passed\n");
    return 0;
}
