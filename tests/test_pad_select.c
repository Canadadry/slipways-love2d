#include "game/pad_select.h"

#include "engine/dispatch_context.h"
#include "engine/game_zone.h"
#include "game/cart.h"
#include "game/layout.h"
#include "game/planet.h"
#include "game/planet_display.h"
#include "game/planet_family.h"
#include "game/research.h"
#include "game/ui.h"
#include "game/ui_button.h"
#include "p8/sprite_sheet.h"
#include "p8/gfx.h"
#include "p8/input.h"
#include "p8/vram.h"

#include <assert.h>
#include <stdio.h>
#include <string.h>

static p8_vram_t memory;
static p8_graphics_t graphics;
static p8_input_t input;

/* Steps `frames` frames with Select down and allowed; returns how many of them gave YEAR, and the frame of the last. */
static int hold(pad_select_t *s, int frames, int *year_frame) {
    int years = 0;
    for (int frame = 1; frame <= frames; frame++) {
        pad_select_event_t event = pad_select_step(s, 1, 1);
        assert(event != PAD_SELECT_TAP);
        if (event == PAD_SELECT_YEAR) {
            years++;
            if (year_frame != NULL) {
                *year_frame = frame;
            }
        }
    }
    return years;
}

static pad_select_event_t release(pad_select_t *s) {
    return pad_select_step(s, 0, 1);
}

static void test_a_hold_of_29_frames_released_does_nothing(void) {
    pad_select_t s;
    pad_select_init(&s);
    assert(hold(&s, 29, NULL) == 0);
    assert(release(&s) == PAD_SELECT_NONE);
}

static void test_a_hold_ends_one_year_on_its_30th_frame(void) {
    pad_select_t s;
    pad_select_init(&s);
    int year_frame = 0;
    assert(hold(&s, 30, &year_frame) == 1);
    assert(year_frame == PAD_SELECT_HOLD_FRAMES);
    assert(release(&s) == PAD_SELECT_NONE);
}

static void test_a_hold_of_90_frames_ends_one_year(void) {
    pad_select_t s;
    pad_select_init(&s);
    assert(hold(&s, 90, NULL) == 1);
    assert(release(&s) == PAD_SELECT_NONE);
}

static void test_a_release_within_10_frames_is_a_tap_on_its_release_frame(void) {
    pad_select_t s;
    pad_select_init(&s);
    assert(hold(&s, 1, NULL) == 0);
    assert(release(&s) == PAD_SELECT_TAP);
    assert(release(&s) == PAD_SELECT_NONE);
    assert(hold(&s, PAD_SELECT_TAP_FRAMES, NULL) == 0);
    assert(release(&s) == PAD_SELECT_TAP);
}

static void test_a_release_at_11_frames_does_nothing(void) {
    pad_select_t s;
    pad_select_init(&s);
    assert(hold(&s, PAD_SELECT_TAP_FRAMES + 1, NULL) == 0);
    assert(release(&s) == PAD_SELECT_NONE);
}

static void test_a_press_begun_while_not_allowed_stays_inert_until_released(void) {
    pad_select_t s;
    pad_select_init(&s);
    assert(pad_select_step(&s, 1, 0) == PAD_SELECT_NONE);
    assert(hold(&s, 60, NULL) == 0);
    assert(release(&s) == PAD_SELECT_NONE);
    assert(hold(&s, 1, NULL) == 0);
    assert(pad_select_step(&s, 1, 0) == PAD_SELECT_NONE);
    assert(release(&s) == PAD_SELECT_NONE);
    assert(hold(&s, PAD_SELECT_HOLD_FRAMES, NULL) == 1);
}

static void test_a_tap_released_while_not_allowed_is_dropped(void) {
    pad_select_t s;
    pad_select_init(&s);
    assert(hold(&s, 3, NULL) == 0);
    assert(pad_select_step(&s, 0, 0) == PAD_SELECT_NONE);
    assert(release(&s) == PAD_SELECT_NONE);
}

static void test_frames_not_allowed_without_a_press_block_nothing_later(void) {
    pad_select_t s;
    pad_select_init(&s);
    assert(pad_select_step(&s, 0, 0) == PAD_SELECT_NONE);
    assert(hold(&s, 2, NULL) == 0);
    assert(release(&s) == PAD_SELECT_TAP);
}

/* A year from a hold, then Select up for `idle` frames: the next step is frame idle + 1 of the window. */
static void year_then_idle(pad_select_t *s, int idle) {
    pad_select_init(s);
    assert(hold(s, PAD_SELECT_HOLD_FRAMES, NULL) == 1);
    pad_select_year_ended(s);
    for (int frame = 0; frame < idle; frame++) {
        assert(release(s) == PAD_SELECT_NONE);
    }
}

static void test_a_press_on_the_chain_window_15th_frame_ends_a_year_on_its_press_frame(void) {
    pad_select_t s;
    year_then_idle(&s, PAD_SELECT_CHAIN_FRAMES - 1);
    assert(pad_select_step(&s, 1, 1) == PAD_SELECT_YEAR);
    assert(release(&s) == PAD_SELECT_NONE);
}

static void test_a_press_on_the_16th_frame_after_a_year_starts_a_normal_press(void) {
    pad_select_t s;
    year_then_idle(&s, PAD_SELECT_CHAIN_FRAMES);
    assert(pad_select_step(&s, 1, 1) == PAD_SELECT_NONE);
    assert(release(&s) == PAD_SELECT_TAP);
    pad_select_t held;
    year_then_idle(&held, PAD_SELECT_CHAIN_FRAMES);
    int year_frame = 0;
    assert(hold(&held, PAD_SELECT_HOLD_FRAMES, &year_frame) == 1 && year_frame == PAD_SELECT_HOLD_FRAMES);
}

static void test_a_chain_press_held_40_frames_ends_one_year(void) {
    pad_select_t s;
    year_then_idle(&s, 1);
    int year_frame = 0;
    assert(hold(&s, 40, &year_frame) == 1 && year_frame == 1);
    assert(release(&s) == PAD_SELECT_NONE);
}

static void test_each_chained_year_reopens_the_window(void) {
    pad_select_t s;
    year_then_idle(&s, 1);
    for (int year = 0; year < 6; year++) {
        assert(pad_select_step(&s, 1, 1) == PAD_SELECT_YEAR);
        pad_select_year_ended(&s);
        assert(release(&s) == PAD_SELECT_NONE);
    }
}

static void test_a_year_just_ended_lasts_until_the_next_step(void) {
    pad_select_t s;
    pad_select_init(&s);
    assert(!pad_select_year_just_ended(&s));
    assert(hold(&s, PAD_SELECT_HOLD_FRAMES, NULL) == 1);
    assert(!pad_select_year_just_ended(&s));
    pad_select_year_ended(&s);
    assert(pad_select_year_just_ended(&s));
    assert(pad_select_step(&s, 0, 0) == PAD_SELECT_NONE);
    assert(!pad_select_year_just_ended(&s));
}

static void test_without_a_reported_year_no_window_opens(void) {
    pad_select_t s;
    pad_select_init(&s);
    assert(hold(&s, PAD_SELECT_HOLD_FRAMES, NULL) == 1);
    assert(release(&s) == PAD_SELECT_NONE);
    assert(pad_select_step(&s, 1, 1) == PAD_SELECT_NONE);
    assert(release(&s) == PAD_SELECT_TAP);
}

static void test_the_chain_window_counts_down_on_frames_not_allowed(void) {
    pad_select_t s;
    year_then_idle(&s, 0);
    for (int frame = 0; frame < PAD_SELECT_CHAIN_FRAMES; frame++) {
        assert(pad_select_step(&s, 0, 0) == PAD_SELECT_NONE);
    }
    assert(pad_select_step(&s, 1, 1) == PAD_SELECT_NONE);
}

static void start_game(void) {
    p8_vram_init(&memory);
    p8_gfx_init(&graphics);
    p8_sprite_sheet_load(&memory);
    p8_input_init(&input);
    dispatch_context_set_gfx(&graphics);
    dispatch_context_set_mem(&memory);
    dispatch_context_set_input(&input);
    cart_load();
    cart_init();
    cart_flush_exit_queue();
    cart_start_game(NULL);
}

enum { UNMET = -1, UNCOMPUTED = -2 };

/* Each entry of satisfied is a need's satisfied count, UNMET for 0 or UNCOMPUTED for has_satisfied unset. */
static void give_needs(planet_t *p, const int *satisfied, int count) {
    pointer_list_init(&p->needs, game_zone());
    for (int i = 0; i < count; i++) {
        need_t *need = need_new(game_zone(), (resource_t){.kind = RESOURCE_NIL});
        if (satisfied[i] != UNCOMPUTED) {
            need->has_satisfied = 1;
            need->satisfied = p8_fixed_from_int(satisfied[i] == UNMET ? 0 : satisfied[i]);
        }
        pointer_list_add(&p->needs, need);
    }
    p->has |= PLANET_HAS_NEEDS;
}

static planet_t *settled(planet_t *p, const int *satisfied, int count) {
    p->header.state = ENTITY_STATE_SETTLED;
    p->has_development_tier = 1;
    p->development_tier = p8_fixed_from_int(1);
    entity_set_pos(&p->header, vec_make(p8_fixed_from_int(64), p8_fixed_from_int(64)));
    if (count > 0) {
        give_needs(p, satisfied, count);
    }
    return p;
}

/* Closes the panel before returning, so the ui zone's button slots are free for the next. */
static int panel_lists_needs(const planet_t *p) {
    planet_inspection_panel(p);
    const entity_id_list_t *ephemerals = &main_ui()->ephemerals;
    int lists = 0;
    for (int i = 0; i < ephemerals->count && !lists; i++) {
        const entity_t *button = entity_id_list_at(ephemerals, i);
        assert(button != NULL && button->class == button_class() && "the main ui's ephemerals hold buttons only");
        const layout_row_t *row = ((const button_t *)button)->layout_row;
        if (row != NULL && row->placed_items_count > 0 && row->placed_items[0].kind == LAYOUT_ROW_ITEM_TEXT &&
            strcmp(row->placed_items[0].text, "needs:") == 0) {
            lists = 1;
        }
    }
    ui_clear_ephemerals(main_ui());
    cart_flush_exit_queue();
    return lists;
}

/* The predicate on a fresh settled colony with these needs, checked against its panel's "needs:" row. */
static int colony_needs_attention(const int *satisfied, int count) {
    planet_t *colony = settled(planet_new(NULL), satisfied, count);
    int attention = pad_select_needs_attention(&colony->header);
    assert(attention == panel_lists_needs(colony));
    return attention;
}

static void test_a_settled_colony_needs_attention_exactly_when_its_panel_lists_needs(void) {
    static const int one_unmet[] = {1, UNMET};
    static const int all_met[] = {1, 2};
    static const int one_uncomputed[] = {2, UNCOMPUTED};
    assert(colony_needs_attention(one_unmet, 2) == 1);
    assert(colony_needs_attention(all_met, 2) == 0);
    assert(colony_needs_attention(one_uncomputed, 2) == 1);
    assert(colony_needs_attention(NULL, 0) == 0);
}

static void test_a_structure_with_an_unmet_need_needs_attention(void) {
    static const int one_unmet[] = {UNMET};
    planet_t *lab = settled(structure_new(NULL), one_unmet, 1);
    assert(pad_select_needs_attention(&lab->header) == 1);
    assert(panel_lists_needs(lab) == 1);
}

static void test_only_a_settled_live_candidate_needs_attention(void) {
    static const int one_unmet[] = {UNMET};
    planet_t *known = settled(planet_new(NULL), one_unmet, 1);
    known->header.state = ENTITY_STATE_KNOWN;
    planet_t *dead = settled(planet_new(NULL), one_unmet, 1);
    dead->header.is_dead = 1;
    assert(pad_select_needs_attention(&known->header) == 0);
    assert(pad_select_needs_attention(&dead->header) == 0);
    assert(pad_select_needs_attention(NULL) == 0);
}

static entity_t *colony(int needs_attention) {
    static const int one_unmet[] = {UNMET};
    static const int met[] = {1};
    return &settled(planet_new(NULL), needs_attention ? one_unmet : met, 1)->header;
}

static entity_list_t listed(entity_t *a, entity_t *b, entity_t *c, entity_t *d) {
    entity_list_t list;
    entity_list_init(&list, game_zone());
    entity_list_add(&list, a);
    entity_list_add(&list, b);
    entity_list_add(&list, c);
    entity_list_add(&list, d);
    return list;
}

static void test_next_goes_in_list_order_skips_met_ones_and_wraps(void) {
    entity_t *a = colony(1), *b = colony(0), *c = colony(1), *d = colony(0);
    entity_list_t list = listed(a, b, c, d);
    assert(pad_select_next(&list, a) == c);
    assert(pad_select_next(&list, b) == c);
    assert(pad_select_next(&list, c) == a);
    assert(pad_select_next(&list, d) == a);
}

static void test_next_from_an_unlisted_or_null_current_starts_from_the_first(void) {
    entity_t *a = colony(0), *b = colony(1), *c = colony(1), *d = colony(0);
    entity_list_t list = listed(a, b, c, d);
    entity_t *outside = colony(1);
    assert(pad_select_next(&list, outside) == b);
    assert(pad_select_next(&list, NULL) == b);
}

static void test_next_with_one_needing_attention_returns_it_even_from_itself(void) {
    entity_t *a = colony(0), *b = colony(0), *c = colony(1), *d = colony(0);
    entity_list_t list = listed(a, b, c, d);
    assert(pad_select_next(&list, c) == c);
    assert(pad_select_next(&list, a) == c);
}

static void test_next_with_none_needing_attention_is_null(void) {
    entity_t *a = colony(0), *b = colony(0), *c = colony(0), *d = colony(0);
    entity_list_t list = listed(a, b, c, d);
    assert(pad_select_next(&list, a) == NULL);
    assert(pad_select_next(&list, NULL) == NULL);
    entity_list_t empty;
    entity_list_init(&empty, game_zone());
    assert(pad_select_next(&empty, NULL) == NULL);
}

int main(void) {
    test_a_hold_of_29_frames_released_does_nothing();
    test_a_hold_ends_one_year_on_its_30th_frame();
    test_a_hold_of_90_frames_ends_one_year();
    test_a_release_within_10_frames_is_a_tap_on_its_release_frame();
    test_a_release_at_11_frames_does_nothing();
    test_a_press_begun_while_not_allowed_stays_inert_until_released();
    test_a_tap_released_while_not_allowed_is_dropped();
    test_frames_not_allowed_without_a_press_block_nothing_later();
    test_a_press_on_the_chain_window_15th_frame_ends_a_year_on_its_press_frame();
    test_a_press_on_the_16th_frame_after_a_year_starts_a_normal_press();
    test_a_chain_press_held_40_frames_ends_one_year();
    test_each_chained_year_reopens_the_window();
    test_a_year_just_ended_lasts_until_the_next_step();
    test_without_a_reported_year_no_window_opens();
    test_the_chain_window_counts_down_on_frames_not_allowed();
    start_game();
    test_a_settled_colony_needs_attention_exactly_when_its_panel_lists_needs();
    test_a_structure_with_an_unmet_need_needs_attention();
    test_only_a_settled_live_candidate_needs_attention();
    test_next_goes_in_list_order_skips_met_ones_and_wraps();
    test_next_from_an_unlisted_or_null_current_starts_from_the_first();
    test_next_with_one_needing_attention_returns_it_even_from_itself();
    test_next_with_none_needing_attention_is_null();
    printf("test_pad_select: ok\n");
    return 0;
}
