/* The scene of a fixture game directory (tests/fixtures/love_runtime/ or one
 * symlinking its main.lua) played by game/p8/ with the LÖVE backend in
 * tmp/fake_love, the draw meter installed: every frame must equal the
 * fixture's expected_capture.bin, pixel for pixel, and the meter's counts its
 * expected_state.txt.
 * Usage: test_love_runtime <fixture dir> <scene frame count>. */

#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "fake_love_cli.h"

#define SCREEN_WIDTH 160
#define SCREEN_HEIGHT 120
#define FRAME_SIZE (SCREEN_WIDTH * SCREEN_HEIGHT / 2)
#define MAX_PATH 256

static const char *game_dir;
static char work_dir[MAX_PATH / 2];
static char replay_path[MAX_PATH];
static char capture_path[MAX_PATH];
static char state_path[MAX_PATH];
static char log_path[MAX_PATH];

static char *read_fixture_file(const char *name, long *size) {
    char path[MAX_PATH];
    snprintf(path, sizeof path, "%s/%s", game_dir, name);
    char *content = fake_love_cli_read_file(path, size);
    if (content == NULL)
        fprintf(stderr, "test_love_runtime: %s: cannot read\n", path);
    assert(content != NULL && "each fixture holds its frozen expected_capture.bin and expected_state.txt");
    return content;
}

static void write_replay(int frames) {
    FILE *file = fopen(replay_path, "w");
    assert(file != NULL);
    fputs("{\"input\": \"pad\", \"frames\": [", file);
    for (int i = 0; i < frames; i++)
        fputs(i == 0 ? "{\"pad\": []}" : ", {\"pad\": []}", file);
    fputs("]}\n", file);
    fclose(file);
}

static int pixel(const unsigned char *frame, int x, int y) {
    unsigned char byte = frame[(y * SCREEN_WIDTH + x) / 2];
    return x % 2 == 0 ? byte & 0x0F : byte >> 4;
}

/* Returns the number of frames that differ, each one reported. */
static int compare_frames(const unsigned char *expected, const unsigned char *capture, int frames) {
    int differing_frames = 0;
    for (int frame = 0; frame < frames; frame++) {
        const unsigned char *want = expected + (size_t)frame * FRAME_SIZE;
        const unsigned char *lua = capture + (size_t)frame * FRAME_SIZE;
        int differing = 0;
        int first_x = 0, first_y = 0;
        for (int y = 0; y < SCREEN_HEIGHT; y++) {
            for (int x = 0; x < SCREEN_WIDTH; x++) {
                if (pixel(want, x, y) == pixel(lua, x, y))
                    continue;
                if (differing++ == 0) {
                    first_x = x;
                    first_y = y;
                }
            }
        }
        if (differing == 0)
            continue;
        differing_frames++;
        fprintf(stderr,
                "test_love_runtime: %s: frame %d: first difference at (%d, %d): expected %d, Lua %d; %d pixels differ\n",
                game_dir, frame, first_x, first_y, pixel(want, first_x, first_y), pixel(lua, first_x, first_y),
                differing);
    }
    return differing_frames;
}

static void test_the_lua_runtime_draws_and_meters_every_scene_frame_as_frozen(int scene_frames) {
    long expected_size = 0;
    unsigned char *expected = (unsigned char *)read_fixture_file("expected_capture.bin", &expected_size);
    assert(expected_size == (long)scene_frames * FRAME_SIZE &&
           "expected_capture.bin must hold exactly the frame count given on the command line");
    char *expected_state = read_fixture_file("expected_state.txt", NULL);
    write_replay(scene_frames);
    char args[4 * MAX_PATH];
    snprintf(args, sizeof args, "%s %s %s %s", game_dir, replay_path, capture_path, state_path);
    int status = fake_love_cli_run("", args, capture_path, state_path, log_path);
    if (status != 0) {
        char *log = fake_love_cli_read_file(log_path, NULL);
        fprintf(stderr, "test_love_runtime: %s: tmp/fake_love exited %d:\n%s", game_dir, status, log != NULL ? log : "");
        free(log);
        assert(!"the scene program must run every frame");
    }
    long size = 0;
    unsigned char *capture = (unsigned char *)fake_love_cli_read_file(capture_path, &size);
    assert(capture != NULL && size == expected_size && "one capture frame per scene frame");
    int differing_frames = compare_frames(expected, capture, scene_frames);
    free(capture);
    free(expected);
    assert(differing_frames == 0 && "every Lua frame must equal the expected frame");
    char *state = fake_love_cli_read_file(state_path, NULL);
    assert(state != NULL && "the scene program must write its state");
    if (strcmp(state, expected_state) != 0)
        fprintf(stderr, "test_love_runtime: %s: meter counts differ:\nexpected:\n%sLua:\n%s", game_dir, expected_state,
                state);
    assert(strcmp(state, expected_state) == 0 && "the Lua meter must count as expected_state.txt");
    free(state);
    free(expected_state);
}

int main(int argc, char **argv) {
    assert(argc == 3 && "usage: test_love_runtime <fixture dir> <scene frame count>");
    game_dir = argv[1];
    const char *base = strrchr(game_dir, '/') != NULL ? strrchr(game_dir, '/') + 1 : game_dir;
    int length = snprintf(work_dir, sizeof work_dir, "tmp/test_%s_work", base);
    assert(length > 0 && (size_t)length < sizeof work_dir && "the fixture directory name must fit");
    snprintf(replay_path, sizeof replay_path, "%s/replay.json", work_dir);
    snprintf(capture_path, sizeof capture_path, "%s/capture.bin", work_dir);
    snprintf(state_path, sizeof state_path, "%s/state.txt", work_dir);
    snprintf(log_path, sizeof log_path, "%s/stderr.txt", work_dir);
    char command[3 * MAX_PATH];
    snprintf(command, sizeof command, "rm -rf %s && mkdir -p %s", work_dir, work_dir);
    assert(system(command) == 0);
    test_the_lua_runtime_draws_and_meters_every_scene_frame_as_frozen(atoi(argv[2]));
    snprintf(command, sizeof command, "rm -rf %s", work_dir);
    assert(system(command) == 0);
    printf("test_love_runtime: %s: all tests passed\n", game_dir);
    return 0;
}
