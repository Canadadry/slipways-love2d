#define _POSIX_C_SOURCE 200809L

#include "game/pad_panel.h"

#include "engine/dispatch_context.h"
#include "engine/entity_id.h"
#include "engine/entity_id_list.h"
#include "engine/entity_registry.h"
#include "game/cart.h"
#include "game/input_step.h"
#include "game/main_economy.h"
#include "game/ui.h"
#include "game/ui_button.h"
#include "p8/sprite_sheet.h"
#include "p8/gfx.h"
#include "p8/vram.h"

#include <assert.h>
#include <signal.h>
#include <stdio.h>
#include <string.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>

static p8_vram_t memory;
static p8_graphics_t graphics;

enum { SETTLE_FRAMES = 20 };

static void run_frames(input_step_t *step, int frames) {
    for (int i = 0; i < frames; i++) {
        p8_input_frame_t raw = { 0, 0, 0, 0 };
        (void)input_step_run(step, &raw);
        cart_update60();
    }
}

static entity_t *title_button(const char *name) {
    const entity_list_t *bucket = entities_by_message(ENTITY_MESSAGE_HIT_TEST);
    for (int i = 0; i < bucket->count; i++) {
        entity_t *e = bucket->items[i];
        if (e->class == button_class() && ((const button_t *)e)->name != NULL &&
            strcmp(((const button_t *)e)->name, name) == 0) {
            return e;
        }
    }
    return NULL;
}

static void start_game(input_step_t *step) {
    cart_init();
    input_step_init(step, &bindings_compiled);
    run_frames(step, SETTLE_FRAMES);
    cart_start_game((button_t *)title_button("forgiving"));
    run_frames(step, SETTLE_FRAMES);
    assert(main_ui() != NULL && main_economy_typed() != NULL);
}

/* Ends the pair to compare with a fresh ask, then opens a new pair and asks once, so the next change meets a
 * remembered answer. */
static void assert_step_answer_is_fresh(int expected) {
    int in_step = pad_panel_open();
    pad_panel_step_end();
    int fresh = pad_panel_open();
    assert(fresh == expected);
    assert(in_step == fresh && "pad_panel_open: an answer remembered inside a step went stale");
    pad_panel_step_begin();
    assert(pad_panel_open() == expected);
}

static entity_id_list_t box_of(const entity_t *e) {
    entity_id_list_t list = ui_ephemerals_new();
    entity_id_list_add(&list, entity_id_of(e));
    return list;
}

static void every_change_inside_a_step_reaches_the_answer(void) {
    input_step_t step;
    start_game(&step);
    ui_t *ui = main_ui();
    entity_t *planet = entities_by_tag(TAG_PLANET)->items[0];
    pad_panel_step_begin();
    assert(!pad_panel_open());

    ui_set_ephemerals(ui, box_of(planet));
    assert_step_answer_is_fresh(1);
    ui_set_ephemerals(ui, ui_ephemerals_new());
    assert_step_answer_is_fresh(0);

    modal_backdrop_t *backdrop = modal_backdrop_new(&(modal_backdrop_keys_t){ 0 });
    assert_step_answer_is_fresh(1);
    entity_unregister(&backdrop->header);
    assert_step_answer_is_fresh(0);

    economy_t *economy = main_economy_typed();
    probe_set_main_economy(NULL);
    assert_step_answer_is_fresh(1);
    probe_set_main_economy(&economy->header);
    assert_step_answer_is_fresh(0);

    (void)modal_backdrop_new(&(modal_backdrop_keys_t){ 0 });
    assert_step_answer_is_fresh(1);
    ui_set_main_ui(NULL);
    assert_step_answer_is_fresh(0);
    ui_set_main_ui(ui);
    assert_step_answer_is_fresh(1);
    pad_panel_step_end();
}

static void outside_a_step_every_ask_is_fresh(void) {
    input_step_t step;
    start_game(&step);
    assert(!pad_panel_open());
    modal_backdrop_t *backdrop = modal_backdrop_new(&(modal_backdrop_keys_t){ 0 });
    assert(pad_panel_open());
    entity_unregister(&backdrop->header);
    assert(!pad_panel_open());
}

static void begin_twice(void) {
    pad_panel_step_begin();
    pad_panel_step_begin();
}

static void end_without_begin(void) {
    pad_panel_step_end();
}

static void assert_aborts(void (*body)(void), const char *what) {
    fflush(stdout);
    pid_t child = fork();
    assert(child >= 0);
    if (child == 0) {
        assert(freopen("/dev/null", "w", stderr) != NULL);
        body();
        _exit(0);
    }
    int status;
    assert(waitpid(child, &status, 0) == child);
    if (!(WIFSIGNALED(status) && WTERMSIG(status) == SIGABRT)) {
        fprintf(stderr, "test_pad_panel: %s did not assert\n", what);
        assert(0);
    }
}

static void an_unbalanced_pair_asserts(void) {
    assert_aborts(begin_twice, "a begin inside a pair");
    assert_aborts(end_without_begin, "an end outside a pair");
}

static void in_fresh_child(void (*test)(void)) {
    fflush(stdout);
    pid_t child = fork();
    assert(child >= 0);
    if (child == 0) {
        test();
        fflush(stdout);
        _exit(0);
    }
    int status;
    assert(waitpid(child, &status, 0) == child);
    assert(WIFEXITED(status) && WEXITSTATUS(status) == 0);
}

int main(void) {
    p8_vram_init(&memory);
    p8_gfx_init(&graphics);
    p8_sprite_sheet_load(&memory);
    dispatch_context_set_gfx(&graphics);
    dispatch_context_set_mem(&memory);
    cart_load();
    in_fresh_child(every_change_inside_a_step_reaches_the_answer);
    in_fresh_child(outside_a_step_every_ask_is_fresh);
    an_unbalanced_pair_asserts();
    printf("test_pad_panel: all tests passed\n");
    return 0;
}
