/* The click veil `modal_backdrop_t`, built bare (as the score panel builds
 * it) and with an `on_click` (as the galaxy menu builds it), lived through
 * the real update_entities, its left press and the hover loop of
 * `main_ui()`, in a game built directly: no events file. */

#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

#include "p8/sprite_sheet.h"
#include "engine/bootstrap.h"
#include "game/cart.h"
#include "engine/dispatch_context.h"
#include "engine/entity_registry.h"
#include "p8/fixed.h"
#include "p8/gfx.h"
#include "p8/input.h"
#include "p8/vram.h"
#include "game/score_panel_display.h"
#include "engine/entity_class.h"
#include "engine/entity_id.h"
#include "game/ui.h"
#include "game/ui_button.h"
#include "game/ui_zone.h"

#define ON_CLICK_TRUE_TICKS 6
#define ON_CLICK_TRUE_SEQUENCE_HASH 0xA6A43C27u
#define BARE_SEQUENCE_HASH 0x52BE382Au
#define HOVER_SEQUENCE_HASH 0xECF21A99u

#define LEFT_PRESS_TICK 5
#define IDLE_TICKS 30

static p8_vram_t memory;
static p8_graphics_t graphics;
static p8_input_t input;

static uint32_t fnv(uint32_t h, uint32_t word) {
    int i;
    for (i = 0; i < 4; i++) {
        h ^= (word >> (8 * i)) & 0xFFu;
        h *= 16777619u;
    }
    return h;
}

static int entity_list_listed(const entity_list_t *list, const void *e) {
    int i;
    for (i = 0; list != NULL && i < list->count; i++) {
        if (list->items[i] == e) {
            return 1;
        }
    }
    return 0;
}

/* The kind words the frozen hashes fold before a value: any other value
 * moves them. */
enum { FIXED_KIND = 0, BOOL_KIND = 2 };

static uint32_t fold_bool_member(uint32_t h, int has, int b) {
    if (!has) {
        return fnv(h, 0xDEADu);
    }
    return fnv(fnv(h, (uint32_t)BOOL_KIND), (uint32_t)b);
}

/* The kind words are constant by the struct's shape, kept so that the
 * sequence hashes still prove the typed reads. */
static uint32_t fold_veil(uint32_t h, const modal_backdrop_t *e) {
    h = fnv(fnv(h, (uint32_t)FIXED_KIND), (uint32_t)e->header.state_time);
    h = fold_bool_member(h, (e->header.is_dead != 0), e->header.is_dead);
    h = fold_bool_member(h, (e->header.is_hovered != 0), e->header.is_hovered);
    h = fnv(h, (uint32_t)entity_list_listed(live_entities(), e));
    return fnv(h, (uint32_t)entity_list_listed(entities_by_message(ENTITY_MESSAGE_HIT_TEST), e));
}

static const void *main_ui_hovered(void) {
    return ui_hovered(main_ui());
}

static int g_on_click_calls = 0;
static int g_on_click_answer = 1;

static int counting_on_click(modal_backdrop_t *self) {
    (void)self;
    g_on_click_calls++;
    return g_on_click_answer;
}

static modal_backdrop_t *veil_with_on_click(int answer) {
    modal_backdrop_keys_t keys = {0};

    g_on_click_calls = 0;
    g_on_click_answer = answer;
    keys.on_click = counting_on_click;
    return modal_backdrop_new(&keys);
}

static int left_press(void *veil) {
    down_t out = modal_backdrop_class()->left_press(veil, (mouse_pos_t){0});

    assert(entity_get(out.held) == NULL);
    return out.stop;
}

static void check(const char *label, uint32_t got, uint32_t want) {
    printf("[%s] 0x%08Xu\n", label, (unsigned)got);
    fflush(stdout);
    assert(got == want);
}

/* With an `on_click`: the click at tick LEFT_PRESS_TICK stores what it
 * returns in `is_dead`, and the next update_entities takes the veil out. */
static void test_left_press_with_a_true_on_click_removes_the_veil(void) {
    uint32_t h = 2166136261u;
    modal_backdrop_t *veil;
    int ticks = 0;

    reset_world(NULL);
    cart_flush_exit_queue();
    veil = veil_with_on_click(1);
    h = fold_veil(h, veil);
    while (entity_list_listed(live_entities(), veil)) {
        if (ticks == LEFT_PRESS_TICK) {
            assert(left_press(veil));
            assert(g_on_click_calls == 1);
            h = fold_veil(h, veil);
        }
        update_entities();
        ticks++;
        h = fold_veil(h, veil);
        assert(ticks < 1000);
    }
    assert(g_on_click_calls == 1);
    assert(!entity_list_listed(entities_by_message(ENTITY_MESSAGE_HIT_TEST), veil));
    printf("[on_click_true_sequence] ticks=%d\n", ticks);
    fflush(stdout);
    assert(ticks == ON_CLICK_TRUE_TICKS);
    check("on_click_true_sequence", h, ON_CLICK_TRUE_SEQUENCE_HASH);
}

/* Bare: a click is swallowed, `is_dead` stays 0, the veil stays. */
static void test_bare_veil_swallows_the_click_and_stays(void) {
    uint32_t h = 2166136261u;
    modal_backdrop_t *veil;
    int i;

    reset_world(NULL);
    cart_flush_exit_queue();
    veil = modal_backdrop_new(NULL);
    for (i = 0; i < IDLE_TICKS; i++) {
        if (i == LEFT_PRESS_TICK) {
            assert(left_press(veil));
            assert(!veil->header.is_dead);
        }
        update_entities();
        h = fold_veil(h, veil);
    }
    assert(entity_list_listed(live_entities(), veil) && entity_list_listed(entities_by_message(ENTITY_MESSAGE_HIT_TEST), veil));
    check("bare_sequence", h, BARE_SEQUENCE_HASH);
}

/* An `on_click` answering 0 leaves `is_dead` 0: the veil stays, as a bare
 * one. */
static void test_false_on_click_keeps_the_veil(void) {
    uint32_t h = 2166136261u;
    modal_backdrop_t *veil;
    int i;

    reset_world(NULL);
    cart_flush_exit_queue();
    veil = veil_with_on_click(0);
    for (i = 0; i < IDLE_TICKS; i++) {
        if (i == LEFT_PRESS_TICK) {
            assert(left_press(veil));
            assert(g_on_click_calls == 1);
            assert(!veil->header.is_dead);
        }
        update_entities();
        h = fold_veil(h, veil);
    }
    assert(entity_list_listed(live_entities(), veil) && entity_list_listed(entities_by_message(ENTITY_MESSAGE_HIT_TEST), veil));
    check("bare_sequence on_click_false", h, BARE_SEQUENCE_HASH);
}

static int in_layer(int layer, const void *e) {
    return entity_id_list_has(ui_hovered_layer(main_ui(), layer), entity_id_of(e));
}

/* A started game, then a bare veil: the hover loop of `main_ui()` makes it
 * `hovered` (its hit test always hits) in layer 14, marks it `is_hovered`,
 * takes the left press, and, once the veil is gone,
 * unmarks it through `previous_hovered`. */
static void test_hover_loop_on_the_veil(void) {
    uint32_t h = 2166136261u;
    down_t kn;
    entity_t *kv = NULL;
    modal_backdrop_t *veil;

    cart_init();
    cart_flush_exit_queue();
    cart_start_game(NULL);
    veil = modal_backdrop_new(NULL);

    ui_update(&memory, main_ui());
    assert(main_ui_hovered() == veil);
    assert(in_layer(14, veil) && !in_layer(15, veil));
    assert(veil->header.is_hovered);
    h = fold_veil(h, veil);

    assert(ui_send_to_hovered(main_ui(), SEND_TO_HOVERED_LEFT_PRESS, (mouse_pos_t){0}, NULL, &kv, &kn));
    assert((void *)kv == veil && kn.stop && entity_get(kn.held) == NULL);
    h = fold_veil(h, veil);

    entity_unregister(&veil->header);
    ui_update(&memory, main_ui());
    assert(main_ui_hovered() != veil && !in_layer(14, veil));
    assert(!veil->header.is_hovered);
    h = fold_veil(h, veil);
    check("hover_sequence", h, HOVER_SEQUENCE_HASH);
}

/* The veil's defaults are the class's generated defaults. */
static void test_defaults_match_the_parsed_spec(void) {
    modal_backdrop_t *veil;

    reset_world(NULL);
    cart_flush_exit_queue();
    veil = modal_backdrop_new(NULL);
    assert(veil->archetype == &modal_backdrop_archetype);
    assert(veil->archetype->layer == p8_fixed_from_int(14));
    assert(veil->archetype->hit_test == p8_fixed_from_int(1));
    assert(veil->header.state == ENTITY_STATE_NORMAL);
    assert(modal_backdrop_ready());
    modal_backdrop_shutdown();
    assert(!modal_backdrop_ready());
    modal_backdrop_init();
}

static int is_a_veil_struct(const void *e) {
    return e != NULL &&
           ((const entity_t *)e)->class == modal_backdrop_class();
}

/* Built by `modal_backdrop_new` with or without keys, a veil is a struct
 * among the live entities and under hit_test only; its hit test always
 * hits, and it has no update. */
static void test_modal_backdrop_is_a_struct_registered_under_hit_test(void) {
    const entity_message_t others[] = {ENTITY_MESSAGE_DRAW, ENTITY_MESSAGE_DRAW_OVERLAY, ENTITY_MESSAGE_NEW_YEAR, ENTITY_MESSAGE_REFRESH};
    const entity_class_t *cls = modal_backdrop_class();
    modal_backdrop_keys_t keys = {0};
    void *built;
    void *generic;
    int i;

    reset_world(NULL);
    cart_flush_exit_queue();
    keys.on_click = score_panel_toggle_on_click();
    built = modal_backdrop_new(&keys);
    generic = modal_backdrop_new(NULL);

    assert(is_a_veil_struct(built) && is_a_veil_struct(generic));
    assert(entity_list_listed(live_entities(), built) && entity_list_listed(entities_by_message(ENTITY_MESSAGE_HIT_TEST), built));
    assert(entity_list_listed(live_entities(), generic) && entity_list_listed(entities_by_message(ENTITY_MESSAGE_HIT_TEST), generic));
    for (i = 0; i < 4; i++) {
        assert(!entity_list_listed(entities_by_message(others[i]), built) && !entity_list_listed(entities_by_message(others[i]), generic));
    }
    assert(cls->hit_test == entity_registry_hit_test_always && cls->hit_test(generic, (mouse_pos_t){0}) == 1);
    assert(cls->states[ENTITY_STATE_NORMAL] == NULL);
    assert(((const modal_backdrop_t *)built)->on_click == score_panel_toggle_on_click());
}

/* A veil takes a slot; once its `on_click` sets `is_dead` and
 * update_entities takes it out, emptying the exit queue gives the slot back. */
static void test_veil_gives_its_slot_back(void) {
    modal_backdrop_t *veil;
    int base;
    int ticks = 0;

    reset_world(NULL);
    cart_flush_exit_queue();
    base = ui_zone_live(SLOT_KIND_MODAL_BACKDROP);
    veil = veil_with_on_click(1);
    assert(ui_zone_live(SLOT_KIND_MODAL_BACKDROP) == base + 1);
    assert(left_press(veil));
    while (entity_list_listed(live_entities(), veil)) {
        update_entities();
        assert(++ticks < 1000);
    }
    assert(ui_zone_live(SLOT_KIND_MODAL_BACKDROP) == base + 1 && "out of the live entities, still read this frame");
    cart_flush_exit_queue();
    assert(ui_zone_live(SLOT_KIND_MODAL_BACKDROP) == base);
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

    test_left_press_with_a_true_on_click_removes_the_veil();
    test_bare_veil_swallows_the_click_and_stays();
    test_false_on_click_keeps_the_veil();
    test_hover_loop_on_the_veil();
    test_defaults_match_the_parsed_spec();
    test_modal_backdrop_is_a_struct_registered_under_hit_test();
    test_veil_gives_its_slot_back();

    printf("test_modal_backdrop: all tests passed\n");
    return 0;
}
