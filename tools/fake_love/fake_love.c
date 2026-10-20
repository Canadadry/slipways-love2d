#include <stdbool.h>
#include <stdio.h>
#include <string.h>

#include <luajit.h>

#include "host.h"
#include "replay.h"

#define EXPECTED_VERSION_PREFIX "LuaJIT 2.1"

typedef struct {
    const char *game_dir;
    const char *replay_path;
    const char *capture_path;
    const char *state_path;
} arguments_t;

static bool parse_arguments(int argc, char **argv, arguments_t *arguments) {
    if (argc != 4 && argc != 5)
        return false;
    for (int i = 1; i < argc; i++)
        if (argv[i][0] == '-')
            return false;
    arguments->game_dir = argv[1];
    arguments->replay_path = argv[2];
    arguments->capture_path = argv[3];
    arguments->state_path = argc == 5 ? argv[4] : NULL;
    return true;
}

static bool check_version(host_t *host) {
    const char *runtime = host_runtime_version(host);
    if (runtime != NULL && strcmp(runtime, LUAJIT_VERSION) == 0 &&
        strncmp(runtime, EXPECTED_VERSION_PREFIX, strlen(EXPECTED_VERSION_PREFIX)) == 0)
        return true;
    fprintf(stderr, "fake_love: LuaJIT header %s and runtime %s must match and start with \"%s\"\n",
            LUAJIT_VERSION, runtime != NULL ? runtime : "(none)", EXPECTED_VERSION_PREFIX);
    return false;
}

static bool create_empty_file(const char *path) {
    FILE *file = fopen(path, "wb");
    if (file == NULL) {
        fprintf(stderr, "fake_love: %s: cannot create\n", path);
        return false;
    }
    fclose(file);
    return true;
}

static bool write_state(host_t *host, const char *path) {
    const char *text;
    size_t length;
    if (!host_debug_state(host, &text, &length)) {
        fprintf(stderr, "fake_love: %s\n", host_error(host));
        return false;
    }
    FILE *file = fopen(path, "wb");
    bool written = file != NULL && fwrite(text, 1, length, file) == length;
    if (file != NULL && fclose(file) != 0)
        written = false;
    if (!written)
        fprintf(stderr, "fake_love: %s: cannot write\n", path);
    return written;
}

static bool run_game(host_t *host, const arguments_t *arguments, const replay_t *replay) {
    bool ok = host_open(host, arguments->game_dir) && host_load(host);
    for (size_t frame = 0; ok && frame < replay_frame_count(replay); frame++)
        ok = host_frame(host, frame, replay_frame_held(replay, frame));
    if (!ok)
        fprintf(stderr, "fake_love: %s\n", host_error(host));
    return ok && (arguments->state_path == NULL || write_state(host, arguments->state_path));
}

static bool run(host_t *host, const arguments_t *arguments) {
    char error[512];
    replay_t *replay = replay_read(arguments->replay_path, error, sizeof error);
    if (replay == NULL) {
        fprintf(stderr, "fake_love: %s\n", error);
        return false;
    }
    bool ok = create_empty_file(arguments->capture_path);
    if (ok && !run_game(host, arguments, replay)) {
        remove(arguments->capture_path);
        if (arguments->state_path != NULL)
            remove(arguments->state_path);
        ok = false;
    }
    replay_free(replay);
    return ok;
}

int main(int argc, char **argv) {
    arguments_t arguments;
    if (!parse_arguments(argc, argv, &arguments)) {
        fprintf(stderr, "usage: fake_love <game dir> <events.json> <capture.bin> [<state.txt>]\n");
        return 2;
    }
    host_t *host = host_new();
    if (host == NULL) {
        fprintf(stderr, "fake_love: out of memory\n");
        return 1;
    }
    bool ok = check_version(host) && run(host, &arguments);
    host_close(host);
    return ok ? 0 : 1;
}
