#include "devtools/record_start.h"
#include "engine/dispatch_context.h"
#include "engine/entity.h"
#include "engine/entity_registry.h"
#include "game/bindings.h"
#include "game/cart.h"
#include "game/input_step.h"
#include "game/save_state.h"
#include "game/save_storage.h"
#include "game/ui_button.h"
#include "p8/sprite_sheet.h"
#include "p8/gfx.h"
#include "p8/input.h"
#include "p8/vram.h"

#include <assert.h>
#include <stdio.h>
#include <string.h>

static const char DEMO_RECORD_PATH[] = "demo/demo_record.hex";

static uint8_t g_record[SAVE_STATE_MAX_BYTES];
static save_state_t g_decoded;
static char g_error[256];
static uint8_t g_scratch[SAVE_STATE_MAX_BYTES];

static p8_vram_t memory;
static p8_graphics_t graphics;
static p8_input_t input;
static input_step_t g_step;
static input_step_t g_fresh_step;
static save_state_t g_captured;
static uint8_t g_encoded[SAVE_STATE_MAX_BYTES];

enum { TITLE_FRAMES = 12 };

static const char FIXTURE_PATH[] = "tmp/test_record_start.hex";

static int read_demo_record(void) {
    int count = record_start_read_hex(DEMO_RECORD_PATH, g_record, (int)sizeof g_record, g_error, sizeof g_error);
    assert(count > 0 && "demo/demo_record.hex reads");
    return count;
}

static void write_fixture(const char *text) {
    FILE *file = fopen(FIXTURE_PATH, "w");
    assert(file != NULL);
    fputs(text, file);
    fclose(file);
}

static void assert_refused(const char *path, const char *reason) {
    size_t path_length = strlen(path);
    g_error[0] = '\0';
    assert(record_start_read_hex(path, g_scratch, (int)sizeof g_scratch, g_error, sizeof g_error) == 0);
    if (strncmp(g_error, path, path_length) != 0 || strncmp(g_error + path_length, ": ", 2) != 0 || strstr(g_error, reason) == NULL) {
        fprintf(stderr, "test_record_start: got \"%s\", want \"%s: ...%s...\"\n", g_error, path, reason);
        assert(0 && "the refusal names the path and the reason");
    }
}

static void assert_fixture_refused(const char *text, const char *reason) {
    write_fixture(text);
    assert_refused(FIXTURE_PATH, reason);
}

static void test_the_demo_record_reads_into_bytes_that_decode(void) {
    int count = read_demo_record();
    assert(save_state_decode(g_record, count, &g_decoded) && "the read bytes decode");
}

static void test_a_bad_hex_file_is_refused_with_its_path(void) {
    static char flipped[2 * SAVE_STATE_MAX_BYTES + 2];
    int count = read_demo_record();

    assert_refused("tmp/test_record_start_no_such_file.hex", "cannot open");
    assert_fixture_refused("abc\n", "odd number");
    assert_fixture_refused("ab0g\n", "not a hex digit");
    assert_fixture_refused("abcd\nab\n", "follows the newline");
    assert_fixture_refused("abcd", "no newline");
    assert_fixture_refused("\n", "no hex digits");
    for (int i = 0; i < count; i++) {
        uint8_t byte = i == count / 2 ? (uint8_t)(g_record[i] ^ 0x01) : g_record[i];
        snprintf(flipped + 2 * i, 3, "%02x", byte);
    }
    strcpy(flipped + 2 * count, "\n");
    assert_fixture_refused(flipped, "does not decode");
}

static void test_more_bytes_than_a_save_holds_are_refused(void) {
    static char longest[2 * (SAVE_STATE_MAX_BYTES + 1) + 2];

    memset(longest, '0', sizeof longest - 2);
    strcpy(longest + sizeof longest - 2, "\n");
    assert_fixture_refused(longest, "more than");
}

static void boot_to_title(void) {
    p8_vram_init(&memory);
    p8_gfx_init(&graphics);
    p8_sprite_sheet_load(&memory);
    p8_input_init(&input);
    dispatch_context_set_gfx(&graphics);
    dispatch_context_set_mem(&memory);
    dispatch_context_set_input(&input);
    cart_load();
    assert(save_storage_erase());
    cart_init();
    input_step_init(&g_step, &bindings_compiled);
}

static void play_frame(int buttons) {
    p8_input_frame_t raw;

    memset(&raw, 0, sizeof raw);
    raw.pad_buttons = buttons;
    input_step_run(&g_step, &raw);
    cart_update60();
    cart_draw();
}

static button_t *title_button(const char *name) {
    const entity_list_t *bucket = entities_by_message(ENTITY_MESSAGE_HIT_TEST);
    for (int i = 0; i < bucket->count; i++) {
        entity_t *e = bucket->items[i];
        if (e->class == button_class() && ((const button_t *)e)->name != NULL && strcmp(((const button_t *)e)->name, name) == 0) {
            return (button_t *)e;
        }
    }
    return NULL;
}

static int live_game_is_the_record(int count) {
    save_state_capture(&g_captured);
    return save_state_encode(&g_captured, g_encoded) == count && memcmp(g_encoded, g_record, (size_t)count) == 0;
}

/* The start and the return keep the attract state: the fresh step is given g_step's. */
static int step_is_fresh_on(input_step_bindings_t which) {
    input_step_init(&g_fresh_step, &bindings_compiled);
    g_fresh_step.attract = g_step.attract;
    input_step_switch_bindings(&g_fresh_step, which);
    return memcmp(&g_step, &g_fresh_step, sizeof g_step) == 0;
}

static int step_is_fresh(void) {
    return step_is_fresh_on(INPUT_STEP_BINDINGS_SESSION);
}

static void play_title_frames_pressing_down(void) {
    int down = bindings_mask(&bindings_compiled, BINDINGS_MODE_TITLE, BINDINGS_ACTION_MOVE_DOWN);
    assert(down != 0 && "the default table binds move_down on the title");
    for (int i = 0; i < TITLE_FRAMES; i++) {
        play_frame(i % 4 == 0 ? down : 0);
    }
    assert(!step_is_fresh() && "the presses moved the title's pad navigation");
}

static void test_a_resume_runs_the_record_on_a_fresh_input_step(void) {
    int count = read_demo_record();

    boot_to_title();
    play_title_frames_pressing_down();
    assert(record_start_resume(&g_step, g_record, count) == 1);
    assert(title_button("forgiving") == NULL && "the title is gone");
    assert(live_game_is_the_record(count) && "the live game is the record");
    assert(step_is_fresh_on(INPUT_STEP_BINDINGS_DEFAULT) && "the input step is a fresh one switched to the default table");
}

static void test_a_return_builds_the_title_and_a_second_resume_gives_the_record_again(void) {
    int count = read_demo_record();

    boot_to_title();
    assert(record_start_resume(&g_step, g_record, count) == 1);
    play_frame(0);
    record_start_return(&g_step);
    assert(title_button("forgiving") != NULL && "the title is back");
    assert(step_is_fresh() && "the input step is a fresh one back on the session's table");
    play_frame(0);
    assert(record_start_resume(&g_step, g_record, count) == 1);
    assert(live_game_is_the_record(count) && "the second start is the record again");
}

static void test_the_session_table_comes_back_after_a_return(void) {
    bindings_t swap;
    bindings_error_t error;
    int count = read_demo_record();

    assert(bindings_parse("swap.cfg", "jump.enter_aim = 8\njump.open_menu = 7\n", &swap, &error));
    boot_to_title();
    input_step_init(&g_step, &swap);
    assert(record_start_resume(&g_step, g_record, count) == 1);
    assert(memcmp(pad_nav_bindings(&g_step.pad_nav), &bindings_default, sizeof swap) == 0 && "the record plays on the default table");
    play_frame(0);
    record_start_return(&g_step);
    assert(memcmp(pad_nav_bindings(&g_step.pad_nav), &swap, sizeof swap) == 0 && "the session's table is back");
}

static void test_undecodable_bytes_leave_the_title_alone(void) {
    int count = read_demo_record();

    g_record[count / 2] ^= 0x01;
    boot_to_title();
    play_title_frames_pressing_down();
    assert(record_start_resume(&g_step, g_record, count) == 0);
    assert(title_button("forgiving") != NULL && "the title is still up");
    assert(!step_is_fresh() && "the input step is untouched");
}

int main(void) {
    test_the_demo_record_reads_into_bytes_that_decode();
    test_a_bad_hex_file_is_refused_with_its_path();
    test_more_bytes_than_a_save_holds_are_refused();
    test_a_resume_runs_the_record_on_a_fresh_input_step();
    test_a_return_builds_the_title_and_a_second_resume_gives_the_record_again();
    test_the_session_table_comes_back_after_a_return();
    test_undecodable_bytes_leave_the_title_alone();
    printf("test_record_start: all tests passed\n");
    return 0;
}
