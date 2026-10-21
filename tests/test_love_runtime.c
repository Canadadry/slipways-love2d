/* The scene of a fixture game directory (tests/fixtures/love_runtime/ or one
 * symlinking its main.lua) played by the C runtime and by game/p8/ with the
 * LÖVE backend in tmp/fake_love: every frame must be the same, pixel for
 * pixel. Usage: test_love_runtime <fixture dir> <scene frame count>. */

#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "fake_love_cli.h"
#include "p8/fatal.h"
#include "p8/gfx.h"
#include "p8/image.h"
#include "p8/sprite_sheet.h"
#include "p8/vram.h"

#define FRAME_SIZE (P8_SCREEN_WIDTH * P8_SCREEN_HEIGHT / 2)
#define MAX_SCENE_FRAMES 16
#define MAX_PATH 256
#define MAX_ARGUMENTS 64
#define MAX_TEXT 256
#define MAX_BUILD_CALLS 64

static unsigned char expected[MAX_SCENE_FRAMES][FRAME_SIZE];

static const char *game_dir;
static char scene[MAX_PATH];
static char work_dir[MAX_PATH / 2];
static char replay_path[MAX_PATH];
static char capture_path[MAX_PATH];
static char state_path[MAX_PATH];
static char log_path[MAX_PATH];

static int scene_line;

typedef struct {
    int line;
    char operation[24];
    int values[MAX_ARGUMENTS];
    int count;
    bool has_text;
    char text[MAX_TEXT];
} scene_call_t;

/* The calls between "build" and "end", replayed by draw_build for each window. */
static scene_call_t build_calls[MAX_BUILD_CALLS];
static int build_count;

void p8_fatal(const char *message) {
    fprintf(stderr, "%s:%d: p8_fatal: %s\n", scene, scene_line, message);
    exit(1);
}

static void scene_fail(const char *message, const char *token) {
    fprintf(stderr, "%s:%d: %s%s%s\n", scene, scene_line, message, token != NULL ? ": " : "", token != NULL ? token : "");
    exit(1);
}

/* Decimal or 0x hex, with an optional minus sign: never octal, as Lua's
 * tonumber reads "010" as ten. */
static int parse_integer(const char *token) {
    const char *digits = token[0] == '-' ? token + 1 : token;
    int base = (digits[0] == '0' && (digits[1] == 'x' || digits[1] == 'X')) ? 16 : 10;
    char *end;
    long value = strtol(token, &end, base);
    if (end == token || *end != '\0')
        scene_fail("not an integer", token);
    return (int)value;
}

static int split(char *line, int values[MAX_ARGUMENTS], char **operation) {
    *operation = strtok(line, " \t");
    int count = 0;
    for (char *token = strtok(NULL, " \t"); token != NULL; token = strtok(NULL, " \t")) {
        if (count == MAX_ARGUMENTS)
            scene_fail("too many arguments", NULL);
        values[count++] = parse_integer(token);
    }
    return count;
}

static int hex_digit(char c) {
    if (c >= '0' && c <= '9')
        return c - '0';
    if (c >= 'a' && c <= 'f')
        return c - 'a' + 10;
    if (c >= 'A' && c <= 'F')
        return c - 'A' + 10;
    return -1;
}

/* A print line is "print x y c text": the text is everything after the one
 * space following c, with \xNN for a byte (#, \, 127 and above, but never
 * 00) and no other escape. Cuts the line before the text and writes the
 * decoded text into text; returns 0 when the line is not a print. */
static int split_text(char *line, char text[MAX_TEXT]) {
    char *p = line + strspn(line, " \t");
    size_t length = strcspn(p, " \t");
    if (!((length == 5 && strncmp(p, "print", 5) == 0) || (length == 14 && strncmp(p, "print_outlined", 14) == 0)))
        return 0;
    for (int token = 0; token < 4; token++) {
        if (token > 0)
            p += strspn(p, " \t");
        p += strcspn(p, " \t");
    }
    if (*p != ' ')
        scene_fail("print takes x y c, one space, then the text", NULL);
    *p++ = '\0';
    int n = 0;
    while (*p != '\0') {
        if (n == MAX_TEXT - 1)
            scene_fail("text too long", NULL);
        if (*p != '\\') {
            text[n++] = *p++;
            continue;
        }
        int high = p[1] == 'x' ? hex_digit(p[2]) : -1;
        int low = high >= 0 ? hex_digit(p[3]) : -1;
        if (low < 0 || (high == 0 && low == 0))
            scene_fail("a backslash in text must start \\xNN, NN not 00", NULL);
        text[n++] = (char)(high * 16 + low);
        p += 4;
    }
    text[n] = '\0';
    return 1;
}

static void require_count(const char *operation, int count, int expected_count) {
    if (count != expected_count)
        scene_fail("wrong argument count for", operation);
}

static void pack_screen(const p8_vram_t *mem, unsigned char frame[FRAME_SIZE]) {
    for (int y = 0; y < P8_SCREEN_HEIGHT; y++)
        for (int x = 0; x < P8_SCREEN_WIDTH; x += 2)
            frame[(y * P8_SCREEN_WIDTH + x) / 2] = (unsigned char)(mem->screen[y][x] | mem->screen[y][x + 1] << 4);
}

static void play_call(p8_graphics_t *gfx, p8_vram_t *mem, const char *operation, const int *v, int count,
                      const char *text) {
    if (strcmp(operation, "cls") == 0) {
        require_count(operation, count, 0);
        p8_cls(mem);
    } else if (strcmp(operation, "camera") == 0) {
        require_count(operation, count, 2);
        p8_camera(gfx, v[0], v[1]);
    } else if (strcmp(operation, "pset") == 0) {
        require_count(operation, count, 3);
        p8_pset(gfx, mem, v[0], v[1], v[2]);
    } else if (strcmp(operation, "psets") == 0) {
        if (count % 3 != 0)
            scene_fail("psets takes x y c triples", NULL);
        p8_point_t points[MAX_ARGUMENTS / 3];
        for (int i = 0; i < count / 3; i++)
            points[i] = (p8_point_t){v[3 * i], v[3 * i + 1], v[3 * i + 2]};
        p8_psets(gfx, mem, points, count / 3);
    } else if (strcmp(operation, "line") == 0) {
        require_count(operation, count, 5);
        p8_line(gfx, mem, v[0], v[1], v[2], v[3], v[4]);
    } else if (strcmp(operation, "lines") == 0) {
        if (count % 4 != 1)
            scene_fail("lines takes a colour then x1 y1 x2 y2 quadruples", NULL);
        p8_segment_t segments[MAX_ARGUMENTS / 4];
        for (int i = 0; i < count / 4; i++)
            segments[i] = (p8_segment_t){v[1 + 4 * i], v[2 + 4 * i], v[3 + 4 * i], v[4 + 4 * i]};
        p8_lines(gfx, mem, segments, count / 4, v[0]);
    } else if (strcmp(operation, "rect") == 0) {
        require_count(operation, count, 5);
        p8_rect(gfx, mem, v[0], v[1], v[2], v[3], v[4]);
    } else if (strcmp(operation, "circ") == 0) {
        require_count(operation, count, 4);
        p8_circ(gfx, mem, v[0], v[1], v[2], v[3]);
    } else if (strcmp(operation, "circfill") == 0) {
        require_count(operation, count, 4);
        p8_circfill(gfx, mem, v[0], v[1], v[2], v[3]);
    } else if (strcmp(operation, "fillp") == 0) {
        require_count(operation, count, 1);
        p8_fillp(gfx, (unsigned int)v[0]);
    } else if (strcmp(operation, "fillp_transparent") == 0) {
        require_count(operation, count, 1);
        p8_fillp_transparent(gfx, (unsigned int)v[0]);
    } else if (strcmp(operation, "fillp_reset") == 0) {
        require_count(operation, count, 0);
        p8_fillp_reset(gfx);
    } else if (strcmp(operation, "rectfill") == 0) {
        require_count(operation, count, 5);
        p8_rectfill(gfx, mem, v[0], v[1], v[2], v[3], v[4]);
    } else if (strcmp(operation, "palt") == 0) {
        require_count(operation, count, 2);
        p8_palt(mem, v[0], v[1]);
    } else if (strcmp(operation, "palette") == 0) {
        require_count(operation, count, 2);
        if (v[0] < 0 || v[0] > 15)
            scene_fail("palette takes a colour 0..15", NULL);
        mem->draw_palette[v[0]] = (unsigned char)v[1];
    } else if (strcmp(operation, "spr") == 0) {
        require_count(operation, count, 5);
        p8_spr(gfx, mem, v[0], v[1], v[2], v[3], v[4]);
    } else if (strcmp(operation, "mset") == 0) {
        require_count(operation, count, 3);
        p8_mset(mem, v[0], v[1], v[2]);
    } else if (strcmp(operation, "map") == 0) {
        require_count(operation, count, 6);
        p8_map(gfx, mem, v[0], v[1], v[2], v[3], v[4], v[5]);
    } else if (strcmp(operation, "print") == 0) {
        require_count(operation, count, 3);
        p8_print(gfx, mem, text, v[0], v[1], v[2]);
    } else if (strcmp(operation, "print_outlined") == 0) {
        require_count(operation, count, 3);
        p8_print_outlined(gfx, mem, text, v[0], v[1], v[2]);
    } else if (strcmp(operation, "image") == 0) {
        require_count(operation, count, 3);
        p8_image_draw(gfx, mem, v[0], v[1], v[2]);
    } else {
        scene_fail("unknown operation", operation);
    }
}

static void draw_build(p8_graphics_t *gfx, p8_vram_t *mem, void *context) {
    (void)context;
    for (int i = 0; i < build_count; i++) {
        const scene_call_t *call = &build_calls[i];
        scene_line = call->line;
        play_call(gfx, mem, call->operation, call->values, call->count, call->has_text ? call->text : NULL);
    }
}

static void record_build_call(const char *operation, const int *values, int count, bool has_text, const char *text) {
    if (build_count == MAX_BUILD_CALLS)
        scene_fail("more calls in a build than MAX_BUILD_CALLS", NULL);
    if (strlen(operation) >= sizeof build_calls[0].operation)
        scene_fail("unknown operation", operation);
    scene_call_t *call = &build_calls[build_count++];
    call->line = scene_line;
    strcpy(call->operation, operation);
    memcpy(call->values, values, sizeof call->values);
    call->count = count;
    call->has_text = has_text;
    if (has_text)
        strcpy(call->text, text);
}

/* Returns the number of frames the scene ends. */
static int play_scene_in_c(void) {
    char *text = fake_love_cli_read_file(scene, NULL);
    assert(text != NULL && "the scene file must exist");
    static p8_vram_t mem;
    p8_graphics_t gfx;
    p8_vram_init(&mem);
    p8_gfx_init(&gfx);
    p8_sprite_sheet_load(&mem);
    int frames = 0;
    bool open_frame = false;
    int build_slot = -1, build_width = 0, build_height = 0, build_line = 0;
    scene_line = 0;
    for (char *rest = text; *rest != '\0';) {
        char *line = rest;
        char *newline = strchr(rest, '\n');
        rest = newline != NULL ? newline + 1 : rest + strlen(rest);
        if (newline != NULL)
            *newline = '\0';
        scene_line++;
        char *comment = strchr(line, '#');
        if (comment != NULL)
            *comment = '\0';
        char text[MAX_TEXT];
        int has_text = split_text(line, text);
        int values[MAX_ARGUMENTS];
        char *operation;
        int count = split(line, values, &operation);
        if (operation == NULL)
            continue;
        if (strcmp(operation, "build") == 0) {
            require_count(operation, count, 3);
            if (build_slot >= 0)
                scene_fail("build inside a build", NULL);
            build_slot = values[0];
            build_width = values[1];
            build_height = values[2];
            build_line = scene_line;
            build_count = 0;
            open_frame = true;
            continue;
        }
        if (strcmp(operation, "end") == 0) {
            require_count(operation, count, 0);
            if (build_slot < 0)
                scene_fail("end outside a build", NULL);
            int end_line = scene_line;
            scene_line = build_line;
            p8_image_build(&gfx, &mem, build_slot, build_width, build_height, draw_build, NULL);
            scene_line = end_line;
            build_slot = -1;
            continue;
        }
        if (build_slot >= 0) {
            if (strcmp(operation, "frame") == 0)
                scene_fail("frame inside a build", NULL);
            record_build_call(operation, values, count, has_text, text);
            continue;
        }
        if (strcmp(operation, "frame") == 0) {
            require_count(operation, count, 0);
            if (frames == MAX_SCENE_FRAMES)
                scene_fail("more frames than MAX_SCENE_FRAMES", NULL);
            pack_screen(&mem, expected[frames++]);
            open_frame = false;
            continue;
        }
        play_call(&gfx, &mem, operation, values, count, has_text ? text : NULL);
        open_frame = true;
    }
    free(text);
    if (build_slot >= 0)
        scene_fail("build without end", NULL);
    if (open_frame)
        scene_fail("calls after the last frame", NULL);
    return frames;
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
    unsigned char byte = frame[(y * P8_SCREEN_WIDTH + x) / 2];
    return x % 2 == 0 ? byte & 0x0F : byte >> 4;
}

/* Returns the number of frames that differ, each one reported. */
static int compare_frames(const unsigned char *capture, int frames) {
    int differing_frames = 0;
    for (int frame = 0; frame < frames; frame++) {
        const unsigned char *lua = capture + (size_t)frame * FRAME_SIZE;
        int differing = 0;
        int first_x = 0, first_y = 0;
        for (int y = 0; y < P8_SCREEN_HEIGHT; y++) {
            for (int x = 0; x < P8_SCREEN_WIDTH; x++) {
                if (pixel(expected[frame], x, y) == pixel(lua, x, y))
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
        fprintf(stderr, "test_love_runtime: %s: frame %d: first difference at (%d, %d): C %d, Lua %d; %d pixels differ\n",
                game_dir, frame, first_x, first_y, pixel(expected[frame], first_x, first_y), pixel(lua, first_x, first_y),
                differing);
    }
    return differing_frames;
}

static void test_the_lua_runtime_draws_every_scene_frame_as_the_c_runtime(int scene_frames) {
    int frames = play_scene_in_c();
    assert(frames == scene_frames && "the scene must end exactly the frame count given on the command line");
    write_replay(frames);
    char args[3 * MAX_PATH];
    snprintf(args, sizeof args, "%s %s %s", game_dir, replay_path, capture_path);
    int status = fake_love_cli_run("", args, capture_path, state_path, log_path);
    if (status != 0) {
        char *log = fake_love_cli_read_file(log_path, NULL);
        fprintf(stderr, "test_love_runtime: %s: tmp/fake_love exited %d:\n%s", game_dir, status, log != NULL ? log : "");
        free(log);
        assert(!"the scene program must run every frame");
    }
    long size = 0;
    unsigned char *capture = (unsigned char *)fake_love_cli_read_file(capture_path, &size);
    assert(capture != NULL && size == (long)frames * FRAME_SIZE && "one capture frame per scene frame");
    int differing_frames = compare_frames(capture, frames);
    free(capture);
    assert(differing_frames == 0 && "every Lua frame must equal the C frame");
}

int main(int argc, char **argv) {
    assert(argc == 3 && "usage: test_love_runtime <fixture dir> <scene frame count>");
    game_dir = argv[1];
    const char *base = strrchr(game_dir, '/') != NULL ? strrchr(game_dir, '/') + 1 : game_dir;
    snprintf(scene, sizeof scene, "%s/scene.txt", game_dir);
    int length = snprintf(work_dir, sizeof work_dir, "tmp/test_%s_work", base);
    assert(length > 0 && (size_t)length < sizeof work_dir && "the fixture directory name must fit");
    snprintf(replay_path, sizeof replay_path, "%s/replay.json", work_dir);
    snprintf(capture_path, sizeof capture_path, "%s/capture.bin", work_dir);
    snprintf(state_path, sizeof state_path, "%s/state.txt", work_dir);
    snprintf(log_path, sizeof log_path, "%s/stderr.txt", work_dir);
    char command[3 * MAX_PATH];
    snprintf(command, sizeof command, "rm -rf %s && mkdir -p %s", work_dir, work_dir);
    assert(system(command) == 0);
    test_the_lua_runtime_draws_every_scene_frame_as_the_c_runtime(atoi(argv[2]));
    snprintf(command, sizeof command, "rm -rf %s", work_dir);
    assert(system(command) == 0);
    printf("test_love_runtime: %s: all tests passed\n", game_dir);
    return 0;
}
