#define _POSIX_C_SOURCE 200809L

#include "devtools/state_format.h"

#include "devtools/replay_source.h"
#include "devtools/state.h"
#include "engine/dispatch_context.h"
#include "game/cart.h"
#include "game/input_step.h"
#include "game/save_state.h"
#include "p8/sprite_sheet.h"
#include "p8/draw_backend.h"
#include "p8/draw_meter.h"
#include "p8/gpu_sizes.h"
#include "p8/image.h"
#include "p8/gfx.h"
#include "p8/input.h"
#include "p8/vram.h"

#include <assert.h>
#include <stdio.h>
#include <string.h>

enum { TEXT_CAP = 1 << 16 };

typedef struct {
    char text[TEXT_CAP];
    size_t length;
    int lines;
} collected_t;

static void collect_line(void *context, const char *line) {
    collected_t *collected = context;
    size_t length = strlen(line);
    assert(length > 0 && line[length - 1] == '\n' && "a sink call carries one whole line");
    assert(memchr(line, '\n', length - 1) == NULL && "a sink call carries one line only");
    assert(collected->length + length < TEXT_CAP);
    memcpy(collected->text + collected->length, line, length + 1);
    collected->length += length;
    collected->lines++;
}

static collected_t g_sink_text;
static char g_file_text[TEXT_CAP];

static const state_sink_t *fresh_sink(void) {
    static state_sink_t sink = { collect_line, &g_sink_text };
    g_sink_text.length = 0;
    g_sink_text.lines = 0;
    g_sink_text.text[0] = '\0';
    return &sink;
}

static FILE *open_file_text(void) {
    memset(g_file_text, 0, sizeof g_file_text);
    FILE *out = fmemopen(g_file_text, sizeof g_file_text, "w");
    assert(out != NULL);
    return out;
}

static void assert_sink_matches_file(FILE *out) {
    assert(fclose(out) == 0);
    assert(strlen(g_file_text) == g_sink_text.length);
    assert(memcmp(g_file_text, g_sink_text.text, g_sink_text.length) == 0 && "the sink and the FILE text differ");
}

/* pad_slipway's game without cart_draw(): drawing is most of the cost, and both printers see the same state. */
static void play_short_fixture(void) {
    static replay_source_t source;
    assert(replay_source_open(&source, "tests/e2e/pad_slipway.json") == REPLAY_SOURCE_LOADED);
    assert(replay_source_seed_storage(&source));
    p8_input_t *input = &source.recording;
    dispatch_context_set_input(input);
    cart_load();
    cart_init();
    input_step_t step;
    input_step_init(&step, &bindings_default);
    for (int i = 0; i < input->frame_count; i++) {
        input_step_run(&step, &input->frames[input->current_frame]);
        cart_update60();
        if (i < input->frame_count - 1) p8_input_step(input);
    }
}

static void test_state_lines_equal_the_file_text(void) {
    FILE *out = open_file_text();
    state_write(out);
    state_format(fresh_sink());
    assert_sink_matches_file(out);
    assert(strstr(g_sink_text.text, "\nplanet ") != NULL && "the fixture reaches a galaxy");
    assert(strstr(g_sink_text.text, "\ncap entity_table ") != NULL);
}

static void test_save_summary_lines_equal_the_file_text(void) {
    static save_state_t captured;
    save_state_capture(&captured);
    FILE *out = open_file_text();
    state_write_save_summary(out, &captured, "save: ");
    state_format_save_summary(fresh_sink(), &captured, "save: ");
    assert_sink_matches_file(out);
    assert(g_sink_text.lines == 4);
}

static void test_stopped_line(void) {
    state_format_stopped(fresh_sink(), 37);
    assert(strcmp(g_sink_text.text, "stopped p8_fatal frame 37\n") == 0);
}

static void test_cheat_activations_line(void) {
    state_format_cheat_activations(fresh_sink(), 2);
    assert(strcmp(g_sink_text.text, "cheat activations 2\n") == 0);
}


static void one_sprite(p8_graphics_t *build_gfx, p8_vram_t *build_mem, void *context) {
    (void)context;
    p8_spr(build_gfx, build_mem, 1, 0, 0, 1, 1);
}

static void test_gpu_lines_carry_the_meter_peaks_and_the_limits(void) {
    static p8_vram_t mem;
    static p8_graphics_t gfx;
    p8_vram_init(&mem);
    p8_gfx_init(&gfx);
    const p8_draw_backend_t *previous = p8_draw_meter_install();
    p8_fillp(&gfx, 0xA5A5);
    p8_rectfill(&gfx, &mem, 0, 0, 5, 5, 0x15);
    p8_spr(&gfx, &mem, 1, 0, 0, 1, 1);
    p8_image_build(&gfx, &mem, 0, 16, 16, one_sprite, NULL);
    p8_draw_meter_frame_end();
    p8_cls(&mem);
    p8_draw_meter_frame_end();
    p8_draw_backend_set(previous);
    p8_draw_meter_counts_t last = p8_draw_meter_last_frame();
    p8_draw_meter_counts_t peak = p8_draw_meter_peak();
    assert(last.bytes < peak.bytes && peak.cluts == 1 && peak.patterns == 1);
    assert(last.build_bytes == 0 && peak.build_bytes > 0);

    state_format(fresh_sink());
    char expected[256];
    const char *budget = strstr(g_sink_text.text, "\nbudget zones ");
    assert(budget != NULL);
    const char *gpu = strstr(g_sink_text.text, "\nzone gpu_frame ");
    assert(gpu != NULL && gpu > budget && strchr(budget + 1, '\n') == gpu && "right after the budget line");
    snprintf(expected, sizeof expected, "\nzone gpu_frame %lu %lu %d\n", last.bytes, peak.bytes, GPU_FRAME_BUFFER_BYTES);
    assert(strncmp(gpu, expected, strlen(expected)) == 0);
    const char *build = strstr(g_sink_text.text, "\nzone gpu_build ");
    assert(build != NULL && strchr(gpu + 1, '\n') == build && "right after the gpu_frame line");
    snprintf(expected, sizeof expected, "\nzone gpu_build 0 %lu %d\n", peak.build_bytes, GPU_BUILD_BUFFER_BYTES);
    assert(strncmp(build, expected, strlen(expected)) == 0);
    snprintf(expected, sizeof expected, "\ncap gpu_cluts - 1 %d\ncap gpu_patterns - 1 %d\n", GPU_CLUT_SLOTS, GPU_PATTERN_SLOTS);
    size_t length = strlen(expected);
    assert(g_sink_text.length >= length);
    assert(strcmp(g_sink_text.text + g_sink_text.length - length, expected) == 0 && "the last two lines");
}

int main(void) {
    static p8_vram_t memory;
    static p8_graphics_t graphics;
    p8_vram_init(&memory);
    p8_gfx_init(&graphics);
    p8_sprite_sheet_load(&memory);
    dispatch_context_set_gfx(&graphics);
    dispatch_context_set_mem(&memory);
    play_short_fixture();
    test_state_lines_equal_the_file_text();
    test_save_summary_lines_equal_the_file_text();
    test_stopped_line();
    test_cheat_activations_line();
    test_gpu_lines_carry_the_meter_peaks_and_the_limits();
    printf("test_state_format: ok\n");
    return 0;
}
