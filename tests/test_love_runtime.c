/* The scene of tests/fixtures/love_runtime/ played by the C runtime and by
 * game/p8/ with the LÖVE backend in tmp/fake_love: every frame must be the
 * same, pixel for pixel. */

#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "fake_love_cli.h"
#include "p8/gfx.h"
#include "p8/sprite_sheet.h"
#include "p8/vram.h"

#define GAME_DIR "tests/fixtures/love_runtime"
#define SCENE GAME_DIR "/scene.txt"
#define DIR "tmp/test_love_runtime_work"
#define REPLAY DIR "/replay.json"
#define CAPTURE DIR "/capture.bin"
#define STATE DIR "/state.txt"
#define LOG DIR "/stderr.txt"
#define FRAME_SIZE (P8_SCREEN_WIDTH * P8_SCREEN_HEIGHT / 2)
#define SCENE_FRAMES 14
#define MAX_ARGUMENTS 64

static unsigned char expected[SCENE_FRAMES][FRAME_SIZE];

static int scene_line;

static void scene_fail(const char *message, const char *token) {
    fprintf(stderr, "%s:%d: %s%s%s\n", SCENE, scene_line, message, token != NULL ? ": " : "", token != NULL ? token : "");
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

static void require_count(const char *operation, int count, int expected_count) {
    if (count != expected_count)
        scene_fail("wrong argument count for", operation);
}

static void pack_screen(const p8_vram_t *mem, unsigned char frame[FRAME_SIZE]) {
    for (int y = 0; y < P8_SCREEN_HEIGHT; y++)
        for (int x = 0; x < P8_SCREEN_WIDTH; x += 2)
            frame[(y * P8_SCREEN_WIDTH + x) / 2] = (unsigned char)(mem->screen[y][x] | mem->screen[y][x + 1] << 4);
}

static void play_call(p8_graphics_t *gfx, p8_vram_t *mem, const char *operation, const int *v, int count) {
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
    } else {
        scene_fail("unknown operation", operation);
    }
}

/* Returns the number of frames the scene ends. */
static int play_scene_in_c(void) {
    char *text = fake_love_cli_read_file(SCENE, NULL);
    assert(text != NULL && "the scene file must exist");
    static p8_vram_t mem;
    p8_graphics_t gfx;
    p8_vram_init(&mem);
    p8_gfx_init(&gfx);
    p8_sprite_sheet_load(&mem);
    int frames = 0;
    bool open_frame = false;
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
        int values[MAX_ARGUMENTS];
        char *operation;
        int count = split(line, values, &operation);
        if (operation == NULL)
            continue;
        if (strcmp(operation, "frame") == 0) {
            require_count(operation, count, 0);
            if (frames == SCENE_FRAMES)
                scene_fail("more frames than SCENE_FRAMES", NULL);
            pack_screen(&mem, expected[frames++]);
            open_frame = false;
            continue;
        }
        play_call(&gfx, &mem, operation, values, count);
        open_frame = true;
    }
    free(text);
    if (open_frame)
        scene_fail("calls after the last frame", NULL);
    return frames;
}

static void write_replay(int frames) {
    FILE *file = fopen(REPLAY, "w");
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
static int compare_frames(const unsigned char *capture) {
    int differing_frames = 0;
    for (int frame = 0; frame < SCENE_FRAMES; frame++) {
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
        fprintf(stderr, "test_love_runtime: frame %d: first difference at (%d, %d): C %d, Lua %d; %d pixels differ\n",
                frame, first_x, first_y, pixel(expected[frame], first_x, first_y), pixel(lua, first_x, first_y),
                differing);
    }
    return differing_frames;
}

static void test_the_lua_runtime_draws_every_scene_frame_as_the_c_runtime(void) {
    int frames = play_scene_in_c();
    assert(frames == SCENE_FRAMES && "the scene must end exactly SCENE_FRAMES frames");
    write_replay(frames);
    int status = fake_love_cli_run("", GAME_DIR " " REPLAY " " CAPTURE, CAPTURE, STATE, LOG);
    if (status != 0) {
        char *log = fake_love_cli_read_file(LOG, NULL);
        fprintf(stderr, "test_love_runtime: tmp/fake_love exited %d:\n%s", status, log != NULL ? log : "");
        free(log);
        assert(!"the scene program must run every frame");
    }
    long size = 0;
    unsigned char *capture = (unsigned char *)fake_love_cli_read_file(CAPTURE, &size);
    assert(capture != NULL && size == (long)SCENE_FRAMES * FRAME_SIZE && "one capture frame per scene frame");
    int differing_frames = compare_frames(capture);
    free(capture);
    assert(differing_frames == 0 && "every Lua frame must equal the C frame");
}

int main(void) {
    assert(system("rm -rf " DIR " && mkdir -p " DIR) == 0);
    test_the_lua_runtime_draws_every_scene_frame_as_the_c_runtime();
    assert(system("rm -rf " DIR) == 0);
    printf("test_love_runtime: all tests passed\n");
    return 0;
}
