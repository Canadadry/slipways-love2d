/* The provisional main economy that probe_init() leaves until
 * cart_start_game() replaces it with an economy, read through
 * main_economy_placeholder() and main_economy_typed(): the cheat code typed
 * on the title screen, a new init, the init/shutdown cycle, the
 * replacement. */

#define _POSIX_C_SOURCE 200809L

#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>

#include "p8/sprite_sheet.h"
#include "game/cart.h"
#include "game/cheat.h"
#include "engine/dispatch_context.h"
#include "game/economy.h"
#include "p8/fixed.h"
#include "game/main_economy.h"
#include "p8/gfx.h"
#include "p8/input.h"
#include "p8/vram.h"
#include "game/probe.h"
#include "game/score_panel.h"
#include "game/tech_gate.h"
#include "engine/entity_class.h"

static p8_vram_t memory;
static p8_graphics_t graphics;
static p8_input_t input;

static void assert_at_rest(void) {
    const main_economy_placeholder_t *fm = main_economy_placeholder();

    assert(fm != NULL && (const void *)fm == (const void *)main_economy());
    assert(!fm->has_research_points && !fm->has_credits);
}

#define CHORD (P8_PAD_BUTTON_5 | P8_PAD_BUTTON_6)

/* Feeds frames to a fresh matcher; returns a bitmask of the frames it fired on. */
static int fired_on(const int *frames, int count) {
    cheat_t cheat;
    int fired = 0;
    int i;

    assert(count <= 30);
    cheat_init(&cheat);
    for (i = 0; i < count; i++) {
        if (cheat_step(&cheat, frames[i])) {
            fired |= 1 << i;
        }
    }
    return fired;
}

/* Four × + ○ chords, one every other frame; returns how often it fired. */
static int type_cheat(void) {
    static const int frames[] = {CHORD, 0, CHORD, 0, CHORD, 0, CHORD, 0};
    int fired = fired_on(frames, 8);
    return fired == 0 ? 0 : fired == 1 << 6 ? 1 : 2;
}

static void test_cheat_matcher(void) {
    static const int four_chords[] = {CHORD, 0, CHORD, 0, CHORD, 0, CHORD, 0};
    static const int held_chords[] = {CHORD, P8_PAD_BUTTON_5, P8_PAD_BUTTON_1, CHORD, P8_PAD_BUTTON_6, 0, CHORD, CHORD | P8_PAD_BUTTON_3, 0, CHORD};
    static const int lone_cross[] = {CHORD, 0, CHORD, 0, P8_PAD_BUTTON_5, 0, CHORD, 0, CHORD, 0};
    static const int lone_circle[] = {CHORD, 0, CHORD, 0, CHORD, 0, P8_PAD_BUTTON_6, 0, CHORD, 0, CHORD, 0, CHORD, 0, CHORD};
    static const int staggered[] = {P8_PAD_BUTTON_5, CHORD, 0, P8_PAD_BUTTON_5, CHORD, 0, P8_PAD_BUTTON_6, CHORD, 0,
                                    P8_PAD_BUTTON_6, CHORD, 0};
    static const int old_sequence[] = {P8_PAD_BUTTON_5, 0, P8_PAD_BUTTON_5, 0, P8_PAD_BUTTON_6, 0, P8_PAD_BUTTON_6, 0,
                                       P8_PAD_BUTTON_5, 0, P8_PAD_BUTTON_5, 0, P8_PAD_BUTTON_6, 0, P8_PAD_BUTTON_6, 0};
    static const int eight_chords[] = {CHORD, 0, CHORD, 0, CHORD, 0, CHORD, 0, CHORD, 0, CHORD, 0, CHORD, 0, CHORD};

    assert(fired_on(four_chords, 8) == 1 << 6);
    assert(fired_on(held_chords, 10) == 1 << 9);
    assert(fired_on(lone_cross, 10) == 0);
    assert(fired_on(lone_circle, 15) == 1 << 14);
    assert(fired_on(staggered, 12) == 0);
    assert(fired_on(old_sequence, 16) == 0);
    assert(fired_on(eight_chords, 15) == (1 << 6 | 1 << 14));
}

static void assert_cheated(void) {
    const main_economy_placeholder_t *fm = main_economy_placeholder();

    assert(fm != NULL);
    assert(fm->has_research_points && fm->research_points == p8_fixed_from_int(99));
    assert(fm->has_credits && fm->credits == p8_fixed_from_int(200));
}

static void new_game(void) {
    cart_start_game(NULL);
}

static void test_at_rest(void) {
    cart_init();
    assert_at_rest();
}

static int available_technologies_affordable(const technologies_entry_t *t) {
    (void)tech_gate_available_technologies(1);
    return tech_gate_affordable_flags(t);
}

static void test_cheat_on_title(void) {
    void *provisional;
    const technologies_entry_t *t = technologies_table;

    cart_init();
    provisional = main_economy();
    assert(main_economy_typed() == NULL && (const void *)main_economy_placeholder() == provisional);
    while (tech_gate_research_cost(t) <= p8_fixed_from_int(0) || tech_gate_research_cost(t) > p8_fixed_from_int(99)) {
        t++;
        assert(t < technologies_table + TECHNOLOGIES_COUNT);
    }
    assert(available_technologies_affordable(t) == 0);
    score_panel_refresh();
    assert(score_panel_readouts()->sub_items[2][0].is_nil && strcmp(score_panel_readouts()->sub_items[0][0].text, "0$") == 0);
    assert(type_cheat() == 1 && main_economy() == provisional);
    assert(available_technologies_affordable(t) == 1);
    score_panel_refresh();
    assert(!score_panel_readouts()->sub_items[2][0].is_nil && strcmp(score_panel_readouts()->sub_items[0][0].text, "200$") == 0);
    assert_cheated();
    assert(type_cheat() == 1 && main_economy() == provisional);
    assert_cheated();
}

static void test_new_init(void) {
    cart_init();
    assert(type_cheat() == 1);
    probe_init();
    assert_at_rest();
}

static void test_cycle(void) {
    cart_init();
    probe_shutdown();
    assert(main_economy() == NULL && main_economy_typed() == NULL);
    assert(type_cheat() == 1);
    assert(main_economy() == NULL && main_economy_typed() == NULL);
    probe_shutdown();
    assert(main_economy() == NULL);
    probe_init();
    assert_at_rest();
    probe_init();
    probe_init();
    assert_at_rest();
}

static void test_replacement(void) {
    cart_init();
    assert(type_cheat() == 1);
    new_game();
    assert(main_economy_placeholder() == NULL && (void *)main_economy_typed() == (void *)main_economy());
    assert(main_economy_typed()->research_points == p8_fixed_from_int(0));
    assert(main_economy_typed()->credits == p8_fixed_from_int(100));
    probe_init();
    assert_at_rest();
}

/* The provisional is one static struct whose descriptor has no method;
 * cart_start_game() puts an economy in its place. */
static void test_main_economy_is_a_struct(void) {
    const entity_class_t *cls = main_economy_placeholder_class();
    void *provisional;
    int j;

    cart_init();
    provisional = main_economy();
    assert(provisional != NULL);
    assert(((const entity_t *)provisional)->class == cls);
    assert(cls->draw == NULL && cls->draw_overlay == NULL && cls->hit_test == NULL && cls->new_year == NULL && cls->refresh == NULL &&
           cls->has_refresh == NULL && cls->left_press == NULL && cls->accept_drop == NULL);
    assert(cls->drag == NULL && cls->drop == NULL && cls->attach_slipway == NULL && cls->offer_trade == NULL && cls->draw_at == NULL &&
           cls->spawn_particle == NULL);
    for (j = 0; j < STATE_COUNT; j++) {
        assert(cls->states[j] == NULL);
    }

    probe_init();
    assert(main_economy() == provisional);

    new_game();
    assert(((const entity_t *)main_economy())->class == economy_class());
}

/* tech_gate_available_technologies() keeps a technology of the current tier
 * until it is invented, even when told to keep the unaffordable ones. */
static void test_available_technologies_drops_invented(void) {
    available_technologies_kept_t kept;
    const technologies_entry_t *t;
    p8_fixed_t ew;
    int i;

    cart_init();
    new_game();
    kept = tech_gate_available_technologies(1);
    assert(kept.count > 0);
    t = kept.items[0];
    ew = main_economy_typed()->era;
    economy_invent(main_economy_typed(), t);
    assert(main_economy_typed()->era == ew);
    kept = tech_gate_available_technologies(1);
    for (i = 0; i < kept.count; i++) {
        assert(kept.items[i] != t);
    }
}

/* Each scenario runs in a child forked right after cart_load(), as a fresh
 * process would start it. */
static void in_fresh_child(void (*test)(void)) {
    pid_t child;
    int status;

    fflush(stdout);
    child = fork();
    assert(child >= 0);
    if (child == 0) {
        test();
        fflush(stdout);
        _exit(0);
    }
    assert(waitpid(child, &status, 0) == child);
    assert(WIFEXITED(status) && WEXITSTATUS(status) == 0);
}

int main(void) {
    p8_vram_init(&memory);
    p8_gfx_init(&graphics);
    p8_sprite_sheet_load(&memory);
    p8_input_init(&input);
    dispatch_context_set_gfx(&graphics);
    dispatch_context_set_mem(&memory);
    dispatch_context_set_input(&input);

    cart_load();
    in_fresh_child(test_at_rest);
    in_fresh_child(test_cheat_matcher);
    in_fresh_child(test_cheat_on_title);
    in_fresh_child(test_new_init);
    in_fresh_child(test_cycle);
    in_fresh_child(test_replacement);
    in_fresh_child(test_main_economy_is_a_struct);
    in_fresh_child(test_available_technologies_drops_invented);
    printf("test_main_economy: ok\n");
    return 0;
}
