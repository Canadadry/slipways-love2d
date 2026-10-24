#include <assert.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <luajit.h>

#include "disk.h"
#include "frame_capture.h"
#include "host.h"
#include "replay.h"

#define EXPECTED_VERSION_PREFIX "LuaJIT 2.1"
#define BINDINGS_MAX_BYTES 65536

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

typedef struct capture_writer {
    const char *path;
    FILE *file;
} capture_writer_t;

static bool capture_open(capture_writer_t *writer, const char *path) {
    *writer = (capture_writer_t){.path = path, .file = fopen(path, "wb")};
    if (writer->file == NULL)
        fprintf(stderr, "fake_love: %s: cannot create\n", path);
    return writer->file != NULL;
}

static bool capture_append(capture_writer_t *writer, host_t *host, size_t frame) {
    int width;
    int height;
    const unsigned char *pixels = host_read_canvas(host, 0, &width, &height);
    assert((pixels == NULL || (width == FRAME_CAPTURE_WIDTH && height == FRAME_CAPTURE_HEIGHT)) &&
           "newCanvas accepts only 160x120");
    unsigned char bytes[FRAME_CAPTURE_FRAME_SIZE];
    frame_capture_foreign_t first = {0};
    int foreign = frame_capture_pack(pixels, bytes, &first);
    if (first.no_canvas) {
        fprintf(stderr,
                "fake_love: frame f%zu: no canvas exists at the end of the frame (create the game canvas first, in "
                "love.load)\n",
                frame);
        return false;
    }
    if (foreign > 0) {
        fprintf(stderr,
                "fake_love: frame f%zu: pixel (%d, %d) of canvas 0 is %02X%02X%02X%02X, not one of the 16 palette "
                "colours (%d such pixels in the frame)\n",
                frame, first.x, first.y, first.rgba[0], first.rgba[1], first.rgba[2], first.rgba[3], foreign);
        return false;
    }
    if (fwrite(bytes, 1, sizeof bytes, writer->file) == sizeof bytes)
        return true;
    fprintf(stderr, "fake_love: %s: cannot write\n", writer->path);
    return false;
}

static bool capture_close(capture_writer_t *writer) {
    bool closed = writer->file == NULL || fclose(writer->file) == 0;
    writer->file = NULL;
    if (!closed)
        fprintf(stderr, "fake_love: %s: cannot write\n", writer->path);
    return closed;
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

static bool load_game(host_t *host, const replay_t *replay) {
    int attract_after = replay_attract_after(replay);
    if (attract_after == 0)
        return host_load(host, NULL, 0);
    char threshold[16];
    snprintf(threshold, sizeof threshold, "%d", attract_after);
    const char *const load_arguments[] = {"--attract-after", threshold};
    return host_load(host, load_arguments, 2);
}

static host_frame_input_t read_frame_input(const replay_t *replay, size_t frame, host_touch_t *touches) {
    host_frame_input_t input = {.held = replay_frame_held(replay, frame), .touches = touches};
    input.resized = replay_frame_resize(replay, frame, &input.width, &input.height) != 0;
    const replay_touch_t *replay_touches = replay_frame_touches(replay, frame, &input.touch_count);
    assert(input.touch_count <= HOST_TOUCH_MAX && "the replay reader caps a frame at 10 touches");
    for (size_t i = 0; i < input.touch_count; i++)
        touches[i] = (host_touch_t){replay_touches[i].id, replay_touches[i].x, replay_touches[i].y};
    replay_mouse_t mouse = replay_frame_mouse(replay, frame);
    input.mouse = (host_mouse_t){mouse.x, mouse.y, mouse.left != 0};
    return input;
}

static bool run_game(host_t *host, const arguments_t *arguments, const replay_t *replay, disk_t *disk,
                     capture_writer_t *capture) {
    int width, height;
    replay_window(replay, &width, &height);
    host_set_window(host, width, height);
    bool ok = host_open(host, arguments->game_dir, disk) && load_game(host, replay);
    for (size_t frame = 0; ok && frame < replay_frame_count(replay); frame++) {
        host_touch_t touches[HOST_TOUCH_MAX];
        const host_frame_input_t input = read_frame_input(replay, frame, touches);
        ok = host_frame(host, frame, &input);
        if (!ok)
            break;
        if (!capture_append(capture, host, frame))
            return false;
    }
    if (!ok) {
        fprintf(stderr, "fake_love: %s\n", host_error(host));
        return false;
    }
    if (!capture_close(capture))
        return false;
    return arguments->state_path == NULL || write_state(host, arguments->state_path);
}

/* The fixture's bytes in a buffer the caller frees; NULL after printing the
 * refusal. */
static unsigned char *read_bindings(const char *replay_path, const char *name, size_t *length) {
    const char *slash = strrchr(replay_path, '/');
    int directory_length = slash != NULL ? (int)(slash - replay_path) : 1;
    const char *directory = slash != NULL ? replay_path : ".";
    char path[4096];
    snprintf(path, sizeof path, "%.*s/%s", directory_length, directory, name);
    FILE *file = fopen(path, "rb");
    unsigned char *bytes = malloc(BINDINGS_MAX_BYTES + 1);
    *length = file != NULL && bytes != NULL ? fread(bytes, 1, BINDINGS_MAX_BYTES + 1, file) : 0;
    bool readable = file != NULL && bytes != NULL && !ferror(file);
    if (file != NULL)
        fclose(file);
    if (!readable || *length > BINDINGS_MAX_BYTES) {
        if (readable)
            fprintf(stderr, "fake_love: %s: \"bindings\": %s is over %d bytes\n", replay_path, path, BINDINGS_MAX_BYTES);
        else
            fprintf(stderr, "fake_love: %s: \"bindings\": %s cannot be read\n", replay_path, path);
        free(bytes);
        return NULL;
    }
    return bytes;
}

/* NULL after printing the refusal; else a disk the caller frees. */
static disk_t *build_disk(const arguments_t *arguments, const replay_t *replay) {
    disk_t *disk = disk_create(arguments->game_dir);
    size_t count;
    const unsigned char *save = replay_initial_save(replay, &count);
    if (save != NULL)
        disk_put(disk, "save.bin", save, count);
    const char *bindings = replay_bindings(replay);
    if (bindings == NULL)
        return disk;
    size_t length;
    unsigned char *bytes = read_bindings(arguments->replay_path, bindings, &length);
    if (bytes == NULL) {
        disk_free(disk);
        return NULL;
    }
    disk_put(disk, "bindings.cfg", bytes, length);
    free(bytes);
    return disk;
}

static bool run(const arguments_t *arguments) {
    char error[512];
    replay_t *replay = replay_read(arguments->replay_path, error, sizeof error);
    if (replay == NULL) {
        fprintf(stderr, "fake_love: %s\n", error);
        return false;
    }
    disk_t *disk = build_disk(arguments, replay);
    if (disk == NULL) {
        replay_free(replay);
        return false;
    }
    host_t *host = host_new();
    if (host == NULL)
        fprintf(stderr, "fake_love: out of memory\n");
    capture_writer_t capture = {0};
    bool ok = host != NULL && check_version(host) && capture_open(&capture, arguments->capture_path);
    if (ok && !run_game(host, arguments, replay, disk, &capture)) {
        capture_close(&capture);
        remove(arguments->capture_path);
        if (arguments->state_path != NULL)
            remove(arguments->state_path);
        ok = false;
    }
    host_close(host);
    disk_free(disk);
    replay_free(replay);
    return ok;
}

int main(int argc, char **argv) {
    arguments_t arguments;
    if (!parse_arguments(argc, argv, &arguments)) {
        fprintf(stderr, "usage: fake_love <game dir> <events.json> <capture.bin> [<state.txt>]\n");
        return 2;
    }
    return run(&arguments) ? 0 : 1;
}
