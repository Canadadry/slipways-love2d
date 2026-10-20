/* The wormhole, read through its struct and driven through its class's
 * methods and update_entities, in a game built directly: no events file. */

#define _POSIX_C_SOURCE 200809L

#include <assert.h>
#include <signal.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>

#include "p8/sprite_sheet.h"
#include "game/background.h"
#include "engine/entity_id.h"
#include "engine/bootstrap.h"
#include "game/cart.h"
#include "game/economy.h"
#include "engine/dispatch_context.h"
#include "engine/entity.h"
#include "engine/entity_registry.h"
#include "p8/fixed.h"
#include "game/main_economy.h"
#include "p8/gfx.h"
#include "p8/input.h"
#include "p8/vram.h"
#include "game/planet_display.h"
#include "game/planet_family.h"
#include "game/planet_family_trade.h"
#include "game/planet.h"
#include "game/research.h"
#include "game/score_panel.h"
#include "game/probe.h"
#include "game/slipway.h"
#include "game/targeting.h"
#include "engine/entity_class.h"
#include "game/ui.h"
#include "engine/vec.h"
#include "game/wormhole.h"
#include "engine/zone.h"
#include "engine/game_zone.h"

static mouse_pos_t mouse_pos(vec_t world, vec_t screen) {
    return (mouse_pos_t){.world = world, .screen = screen, .has = MOUSE_POS_HAS_WORLD | MOUSE_POS_HAS_SCREEN};
}

static mouse_pos_t mouse_world_only(vec_t world) {
    return (mouse_pos_t){.world = world, .has = MOUSE_POS_HAS_WORLD};
}

static const vec_t *pos_ref(const entity_t *e) {
    return entity_has_pos(e) ? &e->pos : NULL;
}

static const vec_t *map_pos_ref(const entity_t *e) {
    return entity_has_map_pos(e) ? &e->map_pos : NULL;
}


/* Ticks after the slipway is paid, long enough for the colonizing and
 * paying effects to leave the live entities: update_entities skips the
 * entity after one it takes out. */
#define SETTLE_TICKS 120

#define ANCHORS_HASH 0xC6D8AF55u
#define DRAG_HASH 0x9DAA980Cu
#define UPDATE_PLACING_TICKS 21
#define UPDATE_PLACING_HASH 0xE16D1D52u
#define DROP_HASH 0x86F499FFu
#define UPDATE_UNFOLDING_HASH 0x85EB1EC6u
#define UPDATE_UNFOLDING_EXIT_TICK 27
#define DRAW_HASH 0xA2BAB777u

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

static uint32_t fnv_str(uint32_t h, const char *s) {
    for (; *s != '\0'; s++) {
        h = fnv(h, (uint32_t)(unsigned char)*s);
    }
    return fnv(h, 0xFFu);
}

static int entity_list_rank_in(const entity_list_t *list, const void *e) {
    int i;
    for (i = 0; list != NULL && i < list->count; i++) {
        if (list->items[i] == e) {
            return i;
        }
    }
    return -1;
}

static int entity_list_listed(const entity_list_t *list, const void *e) {
    return entity_list_rank_in(list, e) >= 0;
}

static int is_state(const entity_t *ke, state_t want) {
    return ke->state == want;
}

static wormhole_t *as_wormhole(void *e) {
    assert(e != NULL &&
           ((const entity_t *)e)->class == wormhole_class());
    return e;
}

static slipway_t *as_slipway(void *e) {
    assert(e != NULL &&
           ((const entity_t *)e)->class == slipway_class());
    return e;
}

static planet_t *as_planet(void *e) {
    assert(planet_is_family(e));
    return e;
}

static uint32_t fold_vec(uint32_t h, const vec_t *p) {
    if (p == NULL) {
        return fnv(h, 0xDEADu);
    }
    h = fnv(h, (uint32_t)p->x);
    return fnv(h, (uint32_t)p->y);
}

/* 0 absent or false, 2 true. */
static uint32_t is_dead_state(const wormhole_t *mz) {
    return mz->header.is_dead ? 2 : 0;
}

static uint32_t fold_string(uint32_t h, const char *s) {
    if (s == NULL) {
        return fnv(h, 0xDEADu);
    }
    return fnv_str(h, s);
}

enum { TARGET_ABSENT, TARGET_FALSE, TARGET_TOO_FAR, TARGET_SLIPWAY, TARGET_PLANET, TARGET_THROWAWAY, TARGET_OTHER };

static int is_targeting_track_target(const void *e) {
    return e != NULL &&
           ((const entity_t *)e)->class == targeting_track_target_class();
}

/* The throwaway target's targeted_by, NULL when absent. */
static const void *throwaway_targeted_by(const void *t) {
    assert(is_targeting_track_target(t));
    return entity_get(((const targeting_track_target_t *)t)->targeted_by);
}

static int target_kind(const wormhole_t *mz) {
    const void *et = targeting_target(&mz->targeting);

    if (et == NULL) {
        return TARGET_FALSE;
    }
    if (et == (const void *)wormhole_too_far_target_wrapper()) {
        return TARGET_TOO_FAR;
    }
    if (entity_list_listed(entities_by_tag(TAG_SLIPWAY), et)) {
        return TARGET_SLIPWAY;
    }
    if (entity_list_listed(entities_by_tag(TAG_PLANET), et)) {
        return TARGET_PLANET;
    }
    if (is_targeting_track_target(et)) {
        return TARGET_THROWAWAY;
    }
    return TARGET_OTHER;
}

static p8_fixed_t credits(void) {
    const economy_t *hy = main_economy_typed();
    assert(hy != NULL);
    return hy->credits;
}

static void check(const char *label, uint32_t got, uint32_t want) {
    printf("[%s] 0x%08Xu\n", label, (unsigned)got);
    fflush(stdout);
    assert(got == want);
}

static void check_int(const char *label, int got, int want) {
    printf("[%s] %d\n", label, got);
    fflush(stdout);
    assert(got == want);
}

/* A game started as cart_start_game starts it, with no difficulty picked:
 * a NULL difficulty reads as difficulty 0. */
static void start_game(void) {
    p8_input_init(&input);
    dispatch_context_set_input(&input);
    cart_init();
    cart_start_game(NULL);
}

static vec_t pos_of(void *e) {
    const planet_t *jo = as_planet(e);
    assert(map_pos_ref(&jo->header) != NULL);
    return *map_pos_ref(&jo->header);
}

static p8_fixed_t dist2(vec_t a, vec_t b) {
    vec_t d = vec_sub(a, b);
    return p8_fixed_add(p8_fixed_mul(d.x, d.x), p8_fixed_mul(d.y, d.y));
}

/* Two generated planets colonized and joined by a paid slipway, the rest
 * of the galaxy left unknown, then left to settle. The slipway's drag reads
 * the hovered planet off ui_hovered(main_ui()). */
static void world(void) {
    const entity_list_t *jos;
    slipway_keys_t slipway_keys = {0};
    vec_t mp;
    void *qu;
    void *re = NULL;
    slipway_t *jc;
    int i;

    start_game();
    jos = entities_by_tag(TAG_PLANET);
    assert(jos != NULL && jos->count > 1);
    qu = jos->items[0];
    for (i = 1; i < jos->count; i++) {
        void *e = jos->items[i];
        if (re == NULL || dist2(pos_of(e), pos_of(qu)) < dist2(pos_of(re), pos_of(qu))) {
            re = e;
        }
    }
    planet_apply_project(qu, research_project_new(NULL));
    planet_apply_project(re, research_project_new(NULL));

    slipway_keys.start = entity_id_link(qu);
    jc = slipway_new(&slipway_keys);
    mp = vec_add(vec_mul(pos_of(re), p8_fixed_from_int(128)),
                       vec_make(p8_fixed_from_int(1), p8_fixed_from_int(1)));
    main_ui()->hovered = entity_id_link(re);
    slipway_class()->drag(&jc->header, mouse_world_only(mp));
    main_ui()->hovered = entity_id_link(NULL);
    while (jc->header.state_time < p8_fixed_from_int(15)) {
        update_entities();
    }
    slipway_class()->drop(&jc->header, re);
    assert((void *)slipway_end(jc) == re);
    for (i = 0; i < SETTLE_TICKS; i++) {
        update_entities();
    }
}

static wormhole_t *build_wormhole(void) {
    return wormhole_new();
}

static vec_t point(int x, int y) {
    return vec_make(p8_fixed_from_int(x), p8_fixed_from_int(y));
}

static void drag(void *mz, vec_t world, vec_t screen) {
    wormhole_class()->drag(mz, mouse_pos(world, screen));
}

static void drop(void *mz) {
    wormhole_class()->drop(mz, NULL);
}

/* update_entities skips the entity after one it takes out of the live
 * entities: count ticks by state time, not by calls. */
static void until_state_time(const wormhole_t *mz, int t) {
    while (mz->header.state_time < p8_fixed_from_int(t)) {
        update_entities();
    }
}

static planet_t *first_planet_with_state(state_t state) {
    const entity_list_t *list = entities_by_tag(TAG_PLANET);
    int i;
    for (i = 0; list != NULL && i < list->count; i++) {
        planet_t *jo = as_planet(list->items[i]);
        if (is_state(&jo->header, state)) {
            return jo;
        }
    }
    return NULL;
}

/* The middle of the slipway, in world units. */
static vec_t slipway_mid(slipway_t *jc) {
    return vec_mul(vec_div(vec_add(pos_of(slipway_start(jc)), pos_of(slipway_end(jc))), p8_fixed_from_int(2)),
                         p8_fixed_from_int(128));
}

/* A fresh wormhole: the archetype's defaults, a zero state time, nothing
 * written yet but its anchors list, registered like any entity with draw
 * and draw_overlay. */
static void check_fresh_wormhole(void *e) {
    const wormhole_t *mz = as_wormhole(e);
    const int_seq_t *kk;
    const wormhole_too_far_target_t *jj;
    static const int want_color_ramp[9] = {7, 7, 6, 13, 5, 1, 1, 1, 0};
    int i;

    assert(is_state(&mz->header, ENTITY_STATE_PLACING));
    assert(mz->header.state_time == 0);
    assert(mz->targeting.archetype->attach_distance == 0);
    assert(mz->targeting.archetype->confirmed_state == ENTITY_STATE_UNFOLDING);
    assert(mz->archetype->spawn_snd == p8_fixed_from_int(52));
    assert(mz->radius == 0);
    assert(mz->targeting.cost == p8_fixed_from_int(3));
    assert(mz->targeting.archetype->clock_ticks == p8_fixed_from_int(1));
    assert(mz->archetype->layer == p8_fixed_from_int(5));
    assert(entity_has(&mz->header, ENTITY_HAS_LAYER) && mz->header.layer == mz->archetype->layer);

    kk = mz->archetype->color_ramp;
    assert(kk == &wormhole_color_ramp_seq && kk->count == 9);
    for (i = 0; i < 9; i++) {
        assert(kk->items[i] == want_color_ramp[i]);
    }
    assert(mz->header.class->tags == TAG(TAG_WORMHOLE));
    assert(mz->has & WORMHOLE_HAS_TOO_FAR_TARGET);
    jj = wormhole_too_far_target_wrapper();
    assert(jj != NULL && jj->status_text != NULL && strcmp(jj->status_text, "-too far-") == 0);

    assert(pos_ref(&mz->header) == NULL && map_pos_ref(&mz->header) == NULL && targeting_target(&mz->targeting) == NULL && entity_get(mz->anchor) == NULL && !mz->header.is_dead);
    assert(mz->anchors.zone != NULL);

    assert(entity_list_listed(live_entities(), mz) && entity_list_listed(entities_by_message(ENTITY_MESSAGE_DRAW), mz) && entity_list_listed(entities_by_message(ENTITY_MESSAGE_DRAW_OVERLAY), mz));
    assert(entity_list_listed(entities_by_tag(TAG_WORMHOLE), mz));
    assert(!entity_list_listed(entities_by_message(ENTITY_MESSAGE_HIT_TEST), mz) && !entity_list_listed(entities_by_message(ENTITY_MESSAGE_NEW_YEAR), mz) && !entity_list_listed(entities_by_message(ENTITY_MESSAGE_REFRESH), mz));
}

static void test_construction(void) {
    down_t made;
    const entity_list_t *mzs;
    int before;

    start_game();
    assert(entities_by_tag(TAG_PROBE) == NULL || entities_by_tag(TAG_PROBE)->count == 0);
    mzs = entities_by_tag(TAG_WORMHOLE);
    before = mzs == NULL ? 0 : mzs->count;
    made = background_left_press(NULL);
    assert(made.stop && entity_get(made.held) != NULL);
    assert(entity_list_listed(entities_by_tag(TAG_PROBE), entity_get(made.held)));
    mzs = entities_by_tag(TAG_WORMHOLE);
    assert((mzs == NULL ? 0 : mzs->count) == before);

    made = background_left_press(NULL);
    assert(made.stop && entity_get(made.held) != NULL);
    check_fresh_wormhole(entity_get(made.held));
    check_fresh_wormhole(build_wormhole());
    check_fresh_wormhole(wormhole_new());
}

static uint32_t fold_anchors(uint32_t h, const wormhole_t *mz) {
    const entity_id_list_t *ox;
    int i;

    assert(mz->anchors.zone != NULL);
    ox = &mz->anchors;
    h = fnv(h, (uint32_t)ox->count);
    for (i = 0; i < ox->count; i++) {
        h = fnv(h, (uint32_t)entity_list_rank_in(live_entities(), entity_id_list_at(ox, i)));
    }
    return h;
}

/* The anchors list holds the live entities with a non-zero fold_anchor,
 * in their order, never a wormhole. */
static void test_anchors(void) {
    uint32_t h = 2166136261u;
    wormhole_t *first;
    wormhole_t *second;
    planet_t *unlit = NULL;
    probe_t *probe;
    planet_t *lit;
    const entity_list_t *jos;
    int i;

    world();
    jos = entities_by_tag(TAG_PLANET);
    for (i = 0; unlit == NULL && jos != NULL && i < jos->count; i++) {
        planet_t *jo = as_planet(jos->items[i]);
        if (jo->header.fold_anchor == 0) {
            unlit = jo;
        }
    }
    assert(unlit != NULL);
    first = build_wormhole();
    h = fold_anchors(h, first);
    second = build_wormhole();
    h = fold_anchors(h, second);
    assert(second->anchors.zone != NULL);
    assert(!entity_id_list_has(&second->anchors, entity_id_of(&first->header)) &&
           !entity_id_list_has(&second->anchors, entity_id_of(&unlit->header)));
    printf("[anchors] %d entries\n", second->anchors.count);
    assert(second->anchors.count > 0);
    check("anchors", h, ANCHORS_HASH);

    /* A probe's fold_anchor is 1, from its archetype: it joins the anchors
     * list. */
    probe = probe_new(NULL);
    assert(entity_id_list_has(&build_wormhole()->anchors, entity_id_of(&probe->header)));

    lit = structure_new(NULL);
    lit->header.fold_anchor = p8_fixed_from_int(1);
    assert(entity_id_list_has(&build_wormhole()->anchors, entity_id_of(&lit->header)));
}

/* Bytes a registry has taken from the generation: capacities 4, 8, ... up
 * to its own, each growth leaving the old block dead. */
static size_t registry_bytes(const entity_list_t *list) {
    size_t bytes = 0;
    int k;
    for (k = 4; list != NULL && k <= list->capacity; k *= 2) {
        bytes += ZONE_CELLS((size_t)k * sizeof(entity_t *)) * sizeof(zone_cell_t);
    }
    return bytes;
}

static size_t generation_but_registries(void) {
    return game_zone_used() - registry_bytes(live_entities()) - registry_bytes(entities_by_message(ENTITY_MESSAGE_DRAW)) -
           registry_bytes(entities_by_message(ENTITY_MESSAGE_DRAW_OVERLAY)) - registry_bytes(entities_by_tag(TAG_WORMHOLE));
}

/* A built wormhole takes one grab arena, its anchors list reserved to the
 * exact count; once given back, the next wormhole takes that arena again. */
static void test_arena(void) {
    wormhole_t *mz;
    size_t before;
    int i;

    world();
    cart_flush_exit_queue();
    assert(!game_zone_pending());
    build_wormhole();
    before = generation_but_registries();
    mz = build_wormhole();
    printf("[arena] generation +%lu, anchors %d/%d\n", (unsigned long)(generation_but_registries() - before),
           mz->anchors.count, mz->anchors.capacity);
    assert(generation_but_registries() == before + arena_size(mz->anchors.zone));
    assert(arena_used(mz->anchors.zone) == arena_size(mz->anchors.zone) && "the arena holds the wormhole exactly");
    assert(mz->anchors.count > 0 && mz->anchors.count == mz->anchors.capacity);
    for (i = 1; i < mz->anchors.count; i++) {
        assert(entity_list_rank_in(live_entities(), entity_id_list_at(&mz->anchors, i - 1)) <
               entity_list_rank_in(live_entities(), entity_id_list_at(&mz->anchors, i)));
    }

    drop(mz);
    assert(mz->header.is_dead);
    update_entities();
    assert(!entity_list_listed(live_entities(), mz));
    cart_flush_exit_queue();
    before = generation_but_registries();
    build_wormhole();
    assert(generation_but_registries() == before && "a dead wormhole gives its grab arena back to the next grab");
}

/* The targeted_by of the wormhole's target, a slipway or the throwaway. */
static const void *target_targeted_by(const wormhole_t *mz) {
    entity_t *et = targeting_target(&mz->targeting);

    assert(et != NULL);
    if (et->class == slipway_class()) {
        return entity_get(((const slipway_t *)et)->targeted_by);
    }
    return throwaway_targeted_by(et);
}

static uint32_t fold_drag(uint32_t h, const wormhole_t *mz, int want_kind) {
    const entity_t *et = targeting_target(&mz->targeting);
    int kind = target_kind(mz);

    printf("[drag] target kind %d\n", kind);
    fflush(stdout);
    assert(kind == want_kind);
    h = fold_vec(h, pos_ref(&mz->header));
    h = fold_vec(h, map_pos_ref(&mz->header));
    h = fnv(h, (uint32_t)kind);
    if (et != NULL) {
        if (et->class == wormhole_too_far_target_class() && ((const wormhole_too_far_target_t *)et)->status_text != NULL) {
            h = fnv_str(h, ((const wormhole_too_far_target_t *)et)->status_text);
        }
        if (et->class == slipway_too_far_class() && ((const slipway_too_far_t *)et)->status_text != NULL) {
            h = fnv_str(h, ((const slipway_too_far_t *)et)->status_text);
        }
        if (kind == TARGET_SLIPWAY || kind == TARGET_THROWAWAY) {
            assert(target_targeted_by(mz) == mz);
        }
    }
    assert(entity_get(mz->anchor) != NULL);
    return fnv(h, (uint32_t)entity_list_rank_in(live_entities(), entity_get(mz->anchor)));
}

/* A quarter of the slipway's length past its start end: near that
 * colonized anchor, off the slipway's segment. */
static vec_t near_anchor(void) {
    const entity_list_t *jcs = entities_by_tag(TAG_SLIPWAY);
    const slipway_t *jc;
    vec_t qu;
    vec_t re;

    assert(jcs != NULL && jcs->count == 1);
    jc = as_slipway(jcs->items[0]);
    qu = pos_of(slipway_start(jc));
    re = pos_of(slipway_end(jc));
    return vec_mul(vec_add(qu, vec_div(vec_sub(qu, re), p8_fixed_from_int(4))),
                         p8_fixed_from_int(128));
}

static void test_drag(void) {
    uint32_t h = 2166136261u;
    wormhole_t *mz;
    const entity_list_t *jcs;
    vec_t mid;

    world();
    mz = build_wormhole();
    jcs = entities_by_tag(TAG_SLIPWAY);
    assert(jcs != NULL && jcs->count > 0);
    mid = slipway_mid(as_slipway(jcs->items[0]));
    drag(mz, mid, point(64, 64));
    h = fold_drag(h, mz, TARGET_SLIPWAY);
    drag(mz, point(-640, -640), point(64, 64));
    h = fold_drag(h, mz, TARGET_TOO_FAR);
    drag(mz, near_anchor(), point(64, 64));
    h = fold_drag(h, mz, TARGET_FALSE);
    drag(mz, near_anchor(), point(2, 2));
    h = fold_drag(h, mz, TARGET_THROWAWAY);
    check("drag", h, DRAG_HASH);
}

/* The placing state grows the radius each update_entities tick; nothing
 * sets is_dead. */
static void test_update_placing_ticks(void) {
    uint32_t h = 2166136261u;
    wormhole_t *mz;
    int i;

    world();
    mz = build_wormhole();
    for (i = 0; i < UPDATE_PLACING_TICKS; i++) {
        update_entities();
        assert(is_dead_state(mz) == 0);
        assert(mz->header.state_time == p8_fixed_from_int(i + 1));
        h = fnv(h, (uint32_t)mz->radius);
    }
    assert(entity_list_listed(live_entities(), mz));
    check("update_placing", h, UPDATE_PLACING_HASH);
}

static uint32_t fold_drop(uint32_t h, const wormhole_t *mz) {
    h = fold_string(h, entity_state_name(mz->header.state));
    h = fnv(h, (uint32_t)mz->header.state_time);
    h = fnv(h, is_dead_state(mz));
    return fnv(h, (uint32_t)credits());
}

/* Before tick 15 targeting_confirm never runs; after it, a set target
 * refuses, an unset one pays the cost and turns to unfolding, and too few
 * credits refuse. */
static void test_drop(void) {
    uint32_t hash = 2166136261u;
    p8_fixed_t before;
    wormhole_t *early;
    wormhole_t *targeted;
    wormhole_t *paid;
    wormhole_t *broke;

    world();
    before = credits();

    early = build_wormhole();
    drop(early);
    assert(is_dead_state(early) == 2 && credits() == before);
    hash = fold_drop(hash, early);

    targeted = build_wormhole();
    until_state_time(targeted, 15);
    drag(targeted, point(-640, -640), point(64, 64));
    assert(target_kind(targeted) == TARGET_TOO_FAR);
    drop(targeted);
    assert(is_dead_state(targeted) == 2 && credits() == before);
    hash = fold_drop(hash, targeted);

    paid = build_wormhole();
    until_state_time(paid, 15);
    drag(paid, near_anchor(), point(64, 64));
    assert(target_kind(paid) == TARGET_FALSE);
    drop(paid);
    assert(is_state(&paid->header, ENTITY_STATE_UNFOLDING) && paid->header.state_time == 0 && is_dead_state(paid) == 0);
    assert(credits() == p8_fixed_sub(before, p8_fixed_from_int(3)));
    hash = fold_drop(hash, paid);

    broke = build_wormhole();
    until_state_time(broke, 15);
    drag(broke, near_anchor(), point(64, 64));
    main_economy_typed()->credits = p8_fixed_from_int(2);
    drop(broke);
    assert(is_state(&broke->header, ENTITY_STATE_PLACING) && is_dead_state(broke) == 2);
    assert(credits() == p8_fixed_from_int(2));
    hash = fold_drop(hash, broke);
    check("drop", hash, DROP_HASH);
}

static uint32_t fold_resource_map(uint32_t h, const resource_map_t *map) {
    int i;

    h = fnv(h, (uint32_t)map->count);
    for (i = 0; i < map->count; i++) {
        h = fnv(fnv(h, (uint32_t)map->entries[i].resource), (uint32_t)map->entries[i].value);
    }
    return h;
}

static uint32_t fold_row(uint32_t h, const layout_row_t *row) {
    int i;

    if (row == NULL) {
        return fnv(h, 0xFFFFFFFFu);
    }
    h = fnv(fnv(h, (uint32_t)row->placed_items_count), (uint32_t)row->width);
    for (i = 0; i < row->placed_items_count; i++) {
        const layout_row_item_t *item = &row->placed_items[i];
        h = fnv(fnv(h, (uint32_t)item->kind), (uint32_t)item->color);
        h = fnv(fnv(h, (uint32_t)item->sprite_id), (uint32_t)item->sprite_palette_block);
        h = fnv(fnv(h, (uint32_t)item->offset.x), (uint32_t)item->offset.y);
        h = item->text != NULL ? fnv_str(h, item->text) : fnv(h, 0u);
    }
    return h;
}

/* What the score panel shows: each readout's text or number. */
static uint32_t fold_score_panel_readouts(uint32_t h) {
    const score_panel_readouts_t *readouts = score_panel_readouts();
    int i;

    assert(readouts != NULL);
    for (i = 0; i < 7; i++) {
        const data_item_t *item = &readouts->sub_items[i][0];
        h = fnv(fnv(h, (uint32_t)item->is_nil), (uint32_t)item->is_fixed);
        h = fnv(h, (uint32_t)item->fixed);
        h = item->text != NULL ? fnv_str(h, item->text) : fnv(h, 0u);
    }
    return h;
}

/* What a launch can change: the wormhole's state, the economy's figures,
 * what the score panel shows, each planet's tier, income, production and row. */
static uint32_t fold_after_launch(const wormhole_t *mz) {
    const economy_t *economy = main_economy_typed();
    const entity_list_t *planets = entities_by_tag(TAG_PLANET);
    uint32_t h = fold_drop(2166136261u, mz);
    int i;

    h = fnv(fnv(h, (uint32_t)economy->credits), (uint32_t)economy->income);
    h = fnv(fnv(h, (uint32_t)economy->research_points), (uint32_t)economy->era);
    h = fnv(fnv(h, (uint32_t)economy->year), (uint32_t)economy->year_ticks);
    h = fnv(fnv(h, (uint32_t)economy->has), (uint32_t)economy->happiness);
    h = fnv(fnv(h, (uint32_t)economy->has_end_status), (uint32_t)economy->save_pending);
    h = economy->end_status != NULL ? fnv_str(h, economy->end_status) : fnv(h, 0u);
    h = fold_resource_map(fold_resource_map(h, &economy->production), &economy->demand);
    h = fold_score_panel_readouts(h);
    for (i = 0; planets != NULL && i < planets->count; i++) {
        const planet_t *planet = as_planet(planets->items[i]);
        h = fnv(fnv(h, (uint32_t)planet->development_tier), (uint32_t)planet->has_development_tier);
        h = fnv(fnv(h, (uint32_t)planet->net_income), (uint32_t)planet->has_net_income);
        h = fnv(fnv(h, (uint32_t)planet->production.has_resource), (uint32_t)planet->production.resource);
        h = fnv(h, (uint32_t)planet->production.amount);
        h = fold_row(h, planet->layout_row);
    }
    return h;
}

/* The launch as it ran with a full recompute: the charge, then the clock. */
static void launch_with_full_recompute(wormhole_t *mz) {
    if (mz->header.state_time < p8_fixed_from_int(15)) {
        mz->header.is_dead = 1;
        return;
    }
    if (!targeting_confirm_without_clock(&mz->header)) {
        mz->header.is_dead = 1;
        return;
    }
    economy_advance_clock(main_economy_typed(), p8_fixed_from_int(1));
    mz->header.is_dead = 0;
}

/* The world, a wormhole held `state_time` ticks near an anchor, the year at
 * `year_ticks`, the credits at `credits` (or as they are for a negative
 * value), then the launch; run in its own child, which sends back
 * fold_after_launch. */
static uint32_t launch_in_child(int full_recompute, int state_time, int year_ticks, int credits) {
    int fds[2];
    pid_t child;
    int status;
    uint32_t got = 0;

    assert(pipe(fds) == 0);
    fflush(stdout);
    child = fork();
    assert(child >= 0);
    if (child == 0) {
        wormhole_t *mz;
        uint32_t h;

        close(fds[0]);
        world();
        mz = build_wormhole();
        until_state_time(mz, state_time);
        drag(mz, near_anchor(), point(64, 64));
        main_economy_typed()->year_ticks = p8_fixed_from_int(year_ticks);
        if (credits >= 0) {
            main_economy_typed()->credits = p8_fixed_from_int(credits);
        }
        economy_recompute(main_economy_typed());
        if (full_recompute) {
            launch_with_full_recompute(mz);
        } else {
            drop(mz);
        }
        h = fold_after_launch(mz);
        assert(write(fds[1], &h, sizeof h) == (ssize_t)sizeof h);
        fflush(stdout);
        _exit(0);
    }
    close(fds[1]);
    assert(read(fds[0], &got, sizeof got) == (ssize_t)sizeof got);
    close(fds[0]);
    assert(waitpid(child, &status, 0) == child);
    assert(WIFEXITED(status) && WEXITSTATUS(status) == 0);
    return got;
}

/* A launch ends where the full recompute ended: mid-year, on the tick that
 * starts a new year, on the one that costs the last credits, refused for
 * want of them, and before tick 15. */
static void test_launch_ends_as_with_a_full_recompute(void) {
    static const int cases[][3] = {{15, 0, -1}, {15, 7, -1}, {15, 15, -1}, {15, 3, 3}, {15, 3, 2}, {3, 3, -1}};
    size_t i;

    for (i = 0; i < sizeof cases / sizeof cases[0]; i++) {
        uint32_t launch = launch_in_child(0, cases[i][0], cases[i][1], cases[i][2]);
        uint32_t full = launch_in_child(1, cases[i][0], cases[i][1], cases[i][2]);
        printf("[launch] state time %d ticks %d credits %d: 0x%08X 0x%08X\n", cases[i][0], cases[i][1], cases[i][2], (unsigned)launch,
               (unsigned)full);
        assert(launch == full);
    }
}

static int unknown_count(void) {
    const entity_list_t *jo = entities_by_tag(TAG_PLANET);
    int n = 0;
    int i;
    for (i = 0; jo != NULL && i < jo->count; i++) {
        if (is_state(&as_planet(jo->items[i])->header, ENTITY_STATE_UNKNOWN)) {
            n++;
        }
    }
    return n;
}

/* A paid wormhole put on an unknown planet: the unfolding state reveals
 * around it until a state time past 25 takes it out of the live entities. */
static wormhole_t *paid_wormhole_on_unknown(void) {
    wormhole_t *mz = build_wormhole();
    planet_t *unknown = first_planet_with_state(ENTITY_STATE_UNKNOWN);

    assert(unknown != NULL);
    until_state_time(mz, 15);
    drag(mz, near_anchor(), point(64, 64));
    assert(target_kind(mz) == TARGET_FALSE);
    drop(mz);
    assert(is_state(&mz->header, ENTITY_STATE_UNFOLDING));
    assert(entity_has_map_pos(&unknown->header));
    entity_set_map_pos(&mz->header, unknown->header.map_pos);
    return mz;
}

static void test_update_unfolding_ticks(void) {
    uint32_t h = 2166136261u;
    wormhole_t *mz;
    int tick = 0;
    int revealed = 0;

    world();
    mz = paid_wormhole_on_unknown();
    while (entity_list_listed(live_entities(), mz)) {
        int before = unknown_count();
        update_entities();
        tick++;
        assert(tick < 100);
        revealed += before - unknown_count();
        h = fnv(h, (uint32_t)(before - unknown_count()));
        h = fnv(h, is_dead_state(mz));
        h = fnv(h, (uint32_t)entity_list_listed(entities_by_message(ENTITY_MESSAGE_DRAW), mz));
        h = fnv(h, (uint32_t)entity_list_listed(entities_by_tag(TAG_WORMHOLE), mz));
    }
    printf("[unfolding] left live_entities after %d ticks, %d planets revealed\n", tick, revealed);
    assert(revealed > 0);
    assert(!entity_list_listed(entities_by_message(ENTITY_MESSAGE_DRAW), mz) && !entity_list_listed(entities_by_message(ENTITY_MESSAGE_DRAW_OVERLAY), mz) && !entity_list_listed(entities_by_tag(TAG_WORMHOLE), mz));
    check_int("update_unfolding_exit_tick", tick, UPDATE_UNFOLDING_EXIT_TICK);
    check("update_unfolding", h, UPDATE_UNFOLDING_HASH);
}

static uint32_t screen_hash(void) {
    uint32_t h = 2166136261u;
    for (int y = 0; y < P8_SCREEN_HEIGHT; y++) {
        for (int x = 0; x < P8_SCREEN_WIDTH; x++) {
            h ^= (uint32_t)memory.screen[y][x];
            h *= 16777619u;
        }
    }
    return h;
}

/* The draw method is given the screen centre rather than pos, which is in
 * world coordinates, off the screen without a camera. */
static uint32_t draw(void *mz) {
    const entity_class_t *cls = ((const entity_t *)mz)->class;

    p8_gfx_init(&graphics);
    p8_cls(&memory);
    cls->draw(mz, vec_make(p8_fixed_from_int(64), p8_fixed_from_int(64)));
    cls->draw_overlay(mz, (vec_t){0, 0});
    return screen_hash();
}

/* Ring, anchor line and sprite while placing, the "-too far-" label of the
 * too-far target, the coloured rings while unfolding. */
static void test_draw(void) {
    uint32_t hash = 2166136261u;
    uint32_t blank;
    wormhole_t *bare;
    wormhole_t *far;
    wormhole_t *mz;
    int t;

    world();
    p8_gfx_init(&graphics);
    p8_cls(&memory);
    blank = screen_hash();

    bare = build_wormhole();
    until_state_time(bare, 5);
    drag(bare, near_anchor(), point(64, 64));
    assert(target_kind(bare) == TARGET_FALSE);
    assert(draw(bare) != blank);
    hash = fnv(hash, draw(bare));

    far = build_wormhole();
    until_state_time(far, 5);
    drag(far, point(-640, -640), point(64, 64));
    assert(target_kind(far) == TARGET_TOO_FAR);
    assert(draw(far) != draw(bare));
    hash = fnv(hash, draw(far));

    mz = paid_wormhole_on_unknown();
    for (t = 1; t <= 20; t++) {
        update_entities();
        if (t == 4 || t == 9 || t == 20) {
            uint32_t d = draw(mz);
            assert(d != blank);
            hash = fnv(hash, d);
        }
    }
    check("draw", hash, DRAW_HASH);
}

/* Released over a colonized planet or a slipgate, accept_drop looks for a
 * held slipway's start, which a wormhole never has: stopped, nothing held. */
static void test_accept_drop(void) {
    wormhole_t *mz;
    planet_t *ly;
    void *nr;
    planet_keys_t keys = {0};
    down_t r;

    world();
    mz = build_wormhole();
    ly = first_planet_with_state(ENTITY_STATE_SETTLED);
    assert(ly != NULL);
    r = planet_accept_drop(ly, &mz->header);
    assert(r.stop && entity_get(r.held) == NULL);

    keys.map_pos = ly->header.map_pos;
    keys.has_map_pos = entity_has_map_pos(&ly->header);
    keys.pos = ly->header.pos;
    keys.has_pos = entity_has_pos(&ly->header);
    nr = slipgate_new(&keys);
    r = slipgate_accept_drop(nr, &mz->header);
    assert(r.stop && entity_get(r.held) == NULL);
}

/* The wormhole's too-far target: status_text and nothing else, never
 * written. targeting_track puts targeted_by on the target it has just
 * computed, never on the too-far target a previous drag left there. */
static void check_too_far_target_untouched(void) {
    const wormhole_too_far_target_t *wrapper = wormhole_too_far_target_wrapper();

    assert(wrapper != NULL);
    assert(wrapper->status_text != NULL && strcmp(wrapper->status_text, "-too far-") == 0);
}

static void check_targeted_by_is(const wormhole_t *mz) {
    assert(target_targeted_by(mz) == mz);
}

static void test_too_far_target_wrapper(void) {
    void *wrapper;
    wormhole_t *wormhole;
    wormhole_t *other;
    const entity_list_t *jcs;
    vec_t mid;

    world();
    wrapper = (void *)wormhole_too_far_target_wrapper();
    check_too_far_target_untouched();
    assert(((const entity_t *)wrapper)->class->draw == NULL &&
           ((const entity_t *)wrapper)->class->draw_overlay == NULL &&
           ((const entity_t *)wrapper)->class->drag == NULL);

    wormhole = build_wormhole();
    drag(wormhole, point(-640, -640), point(64, 64));
    assert(target_kind(wormhole) == TARGET_TOO_FAR);
    check_too_far_target_untouched();

    jcs = entities_by_tag(TAG_SLIPWAY);
    assert(jcs != NULL && jcs->count > 0);
    mid = slipway_mid(as_slipway(jcs->items[0]));
    drag(wormhole, mid, point(64, 64));
    assert(target_kind(wormhole) == TARGET_SLIPWAY);
    check_targeted_by_is(wormhole);
    check_too_far_target_untouched();

    drag(wormhole, near_anchor(), point(2, 2));
    assert(target_kind(wormhole) == TARGET_THROWAWAY);
    check_targeted_by_is(wormhole);
    check_too_far_target_untouched();

    drag(wormhole, point(-640, -640), point(64, 64));
    assert(target_kind(wormhole) == TARGET_TOO_FAR);
    check_too_far_target_untouched();

    other = build_wormhole();
    drag(other, point(640, 640), point(64, 64));
    assert(target_kind(other) == TARGET_TOO_FAR);
    assert(targeting_target(&wormhole->targeting) != NULL && targeting_target(&other->targeting) != NULL);
    assert(targeting_target(&wormhole->targeting) == targeting_target(&other->targeting) && (void *)targeting_target(&wormhole->targeting) == wrapper);
    check_too_far_target_untouched();
}

static int is_a_struct_of(const void *e, const entity_class_t *cls) {
    return e != NULL && ((const entity_t *)e)->class == cls;
}

static int is_a_wormhole_struct(const void *e) {
    return is_a_struct_of(e, wormhole_class());
}

/* Built through the background's left_press or directly, a wormhole is a
 * struct whose class has drag, drop, draw, draw_overlay and its two state
 * methods; the too-far target is a struct too, and the wormhole has one. */
static void test_wormhole_is_a_struct(void) {
    down_t made;
    wormhole_t *mz;

    start_game();
    (void)background_left_press(NULL);
    made = background_left_press(NULL);
    mz = wormhole_new();
    assert(is_a_wormhole_struct(entity_get(made.held)) && is_a_wormhole_struct(build_wormhole()) && is_a_wormhole_struct(mz));
    assert(wormhole_class()->drag != NULL && wormhole_class()->drop != NULL);
    assert(wormhole_class()->draw_at == NULL);
    assert(wormhole_class()->states[ENTITY_STATE_PLACING] != NULL &&
           wormhole_class()->states[ENTITY_STATE_UNFOLDING] != NULL && wormhole_class()->draw != NULL &&
           wormhole_class()->draw_overlay != NULL);
    assert(wormhole_class()->states[ENTITY_STATE_NORMAL] == NULL && wormhole_class()->left_press == NULL);

    assert(is_a_struct_of(wormhole_too_far_target_wrapper(), wormhole_too_far_target_class()));
    assert(mz->has & WORMHOLE_HAS_TOO_FAR_TARGET);
}

/* The galaxy depends on the rnd state: run each scenario in a child forked
 * right after cart_load(), as a fresh process would start it. */
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

static void test_anchor_resolves_until_the_flush(void) {
    wormhole_t *mz;
    entity_t *anchor;

    world();
    mz = build_wormhole();
    drag(mz, near_anchor(), point(64, 64));
    anchor = entity_get(mz->anchor);
    assert(anchor != NULL && entity_id_list_has(&mz->anchors, entity_id_of(anchor)));
    entity_unregister(anchor);
    assert(entity_get(mz->anchor) == anchor);
    cart_flush_exit_queue();
    assert(entity_get(mz->anchor) == NULL);
}

/* A fold dragged onto its only anchor finds no anchor (one at the same
 * position is skipped): "-too far-", and no line drawn toward the origin. */
static void test_fold_on_its_only_anchor(void) {
    const entity_list_t *jos;
    planet_t *anchor;
    wormhole_t *mz;
    int k;

    start_game();
    jos = entities_by_tag(TAG_PLANET);
    assert(jos != NULL && jos->count > 0);
    anchor = as_planet(jos->items[0]);
    planet_apply_project(anchor, research_project_new(NULL));
    for (k = 0; k < SETTLE_TICKS; k++) {
        update_entities();
    }
    mz = build_wormhole();
    assert(mz->anchors.count == 1 && entity_id_list_has(&mz->anchors, entity_id_of(&anchor->header)));

    drag(mz, vec_mul(pos_of(anchor), p8_fixed_from_int(128)), point(64, 64));
    assert(entity_get(mz->anchor) == NULL);
    assert(target_kind(mz) == TARGET_TOO_FAR);

    draw(mz);
    for (k = 0; k < 56; k++) {
        assert(memory.screen[k][k] != 1 && "no anchor: no line toward the origin");
    }
}

int main(void) {
    p8_vram_init(&memory);
    p8_gfx_init(&graphics);
    p8_sprite_sheet_load(&memory);
    dispatch_context_set_gfx(&graphics);
    dispatch_context_set_mem(&memory);

    cart_load();
    in_fresh_child(test_construction);
    in_fresh_child(test_anchors);
    in_fresh_child(test_arena);
    in_fresh_child(test_drag);
    in_fresh_child(test_anchor_resolves_until_the_flush);
    in_fresh_child(test_update_placing_ticks);
    in_fresh_child(test_drop);
    in_fresh_child(test_launch_ends_as_with_a_full_recompute);
    in_fresh_child(test_update_unfolding_ticks);
    in_fresh_child(test_draw);
    in_fresh_child(test_accept_drop);
    in_fresh_child(test_too_far_target_wrapper);
    in_fresh_child(test_wormhole_is_a_struct);
    in_fresh_child(test_fold_on_its_only_anchor);

    printf("test_wormhole: all tests passed\n");
    return 0;
}
