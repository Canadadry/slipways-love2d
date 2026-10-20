/* The slipway, read and written on its members and driven through the
 * real update_entities, draw_layers and main user interface, in games built
 * directly: no events file. */

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
#include "p8/draw_backend.h"
#include "engine/entity_id.h"
#include "engine/bootstrap.h"
#include "game/cart.h"
#include "engine/dispatch_context.h"
#include "engine/entity.h"
#include "engine/entity_registry.h"
#include "engine/entity_geom.h"
#include "p8/fixed.h"
#include "game/economy.h"
#include "game/main_economy.h"
#include "p8/gfx.h"
#include "p8/input.h"
#include "p8/vram.h"
#include "game/planet.h"
#include "game/planet_display.h"
#include "game/planet_family.h"
#include "game/planet_family_trade.h"
#include "game/probe.h"
#include "game/research.h"
#include "game/scroll.h"
#include "game/slipway.h"
#include "game/slipway_display.h"
#include "game/visual_random.h"
#include "engine/entity_class.h"
#include "game/ui.h"
#include "engine/vec.h"
#include "game/wormhole.h"
#include "engine/game_zone.h"

static vec_t point(int x, int y) {
    return vec_make(p8_fixed_from_int(x), p8_fixed_from_int(y));
}

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

#define ATTACH_DISTANCE_BITS 0x00000106
#define DROP_HASH 0xEC58A304u
#define DRAG_HASH 0xCD175E6Bu
#define OTHER_END_HASH 0x19ADC88Cu
#define DRAW_HASH 0x7479AA4Bu
#define TARGETING_TRACK_HASH 0xB930691Fu
#define TOO_FAR_HASH 0x1D7E9DEDu
#define MOUSE_POINT_HASH 0x4C9D6519u

static p8_vram_t memory;
static p8_graphics_t graphics;
static p8_input_t input;

static const int want_trail_colors[20] = {0, 0, 7, 7, 10, 10, 9, 9, 9, 9, 9, 9, 9, 9, 4, 4, 4, 2, 2, 1};

static const entity_class_t *class_of(const void *e) {
    return ((const entity_t *)e)->class;
}

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

static slipway_t *as_slipway(const void *e) {
    assert(e != NULL && class_of(e) == slipway_class());
    return (slipway_t *)e;
}

static planet_t *as_planet(const void *e) {
    assert(planet_is_family(e));
    return (planet_t *)e;
}

static vec_t vec_of(const vec_t *v) {
    assert(v != NULL);
    return *v;
}

/* 0 absent, 1 false, 2 true. */
static uint32_t is_dead_state(const slipway_t *jc) {
    if (!jc->header.is_dead) {
        return 0;
    }
    return jc->header.is_dead ? 2u : 1u;
}

static int is_throwaway(const void *e) {
    return e != NULL &&
           ((const entity_t *)e)->class == slipway_mouse_point_class();
}

static int is_too_far(const void *e) {
    return e != NULL &&
           ((const entity_t *)e)->class == slipway_too_far_class() &&
           ((const slipway_too_far_t *)e)->status_text != NULL && strcmp(((const slipway_too_far_t *)e)->status_text, "-too far-") == 0;
}

static slipway_too_far_t *as_too_far(const void *e) {
    assert(is_too_far(e));
    return (slipway_too_far_t *)e;
}

/* The mouse point's `pos` and `map_pos`, NULL when absent. */
static const vec_t *mouse_point_pos(const void *re) {
    assert(is_throwaway(re));
    return pos_ref(&((const slipway_mouse_point_t *)re)->header);
}

static const vec_t *mouse_point_map_pos(const void *re) {
    assert(is_throwaway(re));
    return map_pos_ref(&((const slipway_mouse_point_t *)re)->header);
}

/* The word the frozen hashes fold before a boolean: 0xB0, then 0 or 1; any
 * other moves every *_HASH. */
static uint32_t fold_bool(uint32_t h, int b) {
    return fnv(fnv(h, 0xB0u), b ? 1u : 0u);
}

static uint32_t fold_ref(uint32_t h, const void *ref) {
    if (ref == NULL) {
        return fnv(h, 0xDEADu);
    }
    if (entity_list_listed(live_entities(), ref)) {
        return fnv(fnv(h, 0x4Eu), (uint32_t)entity_list_rank_in(live_entities(), ref));
    }
    /* The frozen hashes folded the too-far struct as a class-less throwaway. */
    return fnv(h, is_too_far(ref) || is_throwaway(ref) ? 0x7Au : 0xEEEEu);
}

/* A member as the frozen hashes fold it: 0xDEAD when absent; the 0xF1 of a
 * fixed and the 0x57 of a string are constant by the struct's shape, kept
 * so that the hashes still prove the typed reads. */
static uint32_t fold_fixed(uint32_t h, int has, p8_fixed_t f) {
    return has ? fnv(fnv(h, 0xF1u), (uint32_t)f) : fnv(h, 0xDEADu);
}

static uint32_t fold_string(uint32_t h, const char *s) {
    return s != NULL ? fnv_str(fnv(h, 0x57u), s) : fnv(h, 0xDEADu);
}

static uint32_t fold_vec(uint32_t h, const vec_t *p) {
    if (p == NULL) {
        return fnv(h, 0xDEADu);
    }
    h = fnv(h, (uint32_t)p->x);
    return fnv(h, (uint32_t)p->y);
}

enum { END_ABSENT, END_PLANET, END_THROWAWAY, END_OTHER };

static int end_kind(const void *jc) {
    const void *re = slipway_end(as_slipway(jc));
    if (re == NULL) {
        return END_ABSENT;
    }
    if (entity_list_listed(entities_by_tag(TAG_NODE), re)) {
        return END_PLANET;
    }
    if (is_throwaway(re)) {
        return END_THROWAWAY;
    }
    return END_OTHER;
}

enum { TARGET_ABSENT, TARGET_FALSE, TARGET_PLANET, TARGET_SLIPWAY, TARGET_STATUS_TEXT, TARGET_OTHER };

static int target_kind(const void *jc) {
    const void *et = targeting_target(&as_slipway(jc)->targeting);
    if (et == NULL) {
        return TARGET_ABSENT;
    }
    if (entity_list_listed(entities_by_tag(TAG_NODE), et)) {
        return TARGET_PLANET;
    }
    if (entity_list_listed(entities_by_tag(TAG_SLIPWAY), et)) {
        return TARGET_SLIPWAY;
    }
    if (is_too_far(et)) {
        return TARGET_STATUS_TEXT;
    }
    return TARGET_OTHER;
}

enum { TARGETED_BY_ABSENT, TARGETED_BY_SLIPWAY, TARGETED_BY_PROBE, TARGETED_BY_WORMHOLE, TARGETED_BY_OTHER };

static int holder_kind(const void *fo) {
    if (fo == NULL) {
        return TARGETED_BY_ABSENT;
    }
    if (entity_list_listed(entities_by_tag(TAG_SLIPWAY), fo)) {
        return TARGETED_BY_SLIPWAY;
    }
    if (entity_list_listed(entities_by_tag(TAG_PROBE), fo)) {
        return TARGETED_BY_PROBE;
    }
    if (entity_list_listed(entities_by_tag(TAG_WORMHOLE), fo)) {
        return TARGETED_BY_WORMHOLE;
    }
    return TARGETED_BY_OTHER;
}

static int pointer_list_count(const pointer_list_t *l) {
    return l == NULL ? -1 : l->count;
}

static int id_list_count(const entity_id_list_t *l) {
    return l == NULL ? -1 : l->count;
}

/* The count, then each served planet's identifier. */
static uint32_t fold_served_planets(uint32_t h, const void *jc) {
    const slipway_served_planets_t *hg = &as_slipway(jc)->served_planets;
    int i;
    h = fnv(h, (uint32_t)hg->count);
    for (i = 0; i < hg->count; i++) {
        h = fnv_str(h, hg->ids[i]);
        h = fold_bool(h, 1);
    }
    return h;
}

static uint32_t fold_slipway(uint32_t h, const void *e) {
    const slipway_t *jc = as_slipway(e);

    h = fold_fixed(h, jc->targeting.has_cost, jc->targeting.cost);
    h = fold_fixed(h, entity_has(&jc->header, ENTITY_HAS_INFRASPACE), jc->header.infraspace);
    h = fold_fixed(h, 1, jc->header.layer);
    h = fold_fixed(h, 1, jc->fade_in_step);
    h = fnv(h, is_dead_state(jc));
    h = fold_string(h, entity_state_name(jc->header.state));
    h = fold_fixed(h, 1, jc->header.state_time);
    h = fold_ref(h, slipway_start(jc));
    h = fnv(h, (uint32_t)end_kind(jc));
    h = fold_ref(h, slipway_end(jc));
    if (end_kind(jc) == END_THROWAWAY) {
        h = fold_vec(h, mouse_point_map_pos(slipway_end(jc)));
    }
    h = fnv(h, (uint32_t)target_kind(jc));
    h = fold_ref(h, targeting_target(&jc->targeting));
    h = fold_ref(h, entity_get(jc->targeted_by));
    h = fnv(h, (uint32_t)holder_kind(entity_get(jc->targeted_by)));
    h = fnv(h, (uint32_t)pointer_list_count(&jc->transit_dots));
    return fold_served_planets(h, jc);
}

static p8_fixed_t credits(void) {
    assert(main_economy_typed() != NULL);
    return main_economy_typed()->credits;
}

static uint32_t fold_planets(uint32_t h, int with_other_end) {
    const entity_list_t *np = entities_by_tag(TAG_NODE);
    int i;

    for (i = 0; np != NULL && i < np->count; i++) {
        void *p = np->items[i];
        const entity_id_list_t *oe = &as_planet(p)->slipways;
        h = fnv(h, (uint32_t)id_list_count(oe));
        h = fold_ref(h, entity_get(as_planet(p)->targeted_by));
        h = fnv(h, (uint32_t)holder_kind(entity_get(as_planet(p)->targeted_by)));
        if (with_other_end && oe != NULL) {
            int j;
            for (j = 0; j < oe->count; j++) {
                assert(class_of(entity_id_list_at(oe, j)) == slipway_class());
                h = fold_ref(h, slipway_other_end((const slipway_t *)entity_id_list_at(oe, j), p));
            }
        }
    }
    return h;
}

static void check(const char *label, uint32_t got, uint32_t want) {
    printf("[%s] 0x%08Xu\n", label, (unsigned)got);
    fflush(stdout);
    assert(got == want);
}

/* A game started by cart_start_game with no difficulty picked: a NULL
 * difficulty reads as difficulty 0. */
static void start_game(void) {
    p8_input_init(&input);
    dispatch_context_set_input(&input);
    cart_init();
    cart_start_game(NULL);
}

static vec_t pos_of(const void *e) {
    return vec_of(map_pos_ref(&as_planet(e)->header));
}

static p8_fixed_t dist2(vec_t a, vec_t b) {
    vec_t d = vec_sub(a, b);
    return p8_fixed_add(p8_fixed_mul(d.x, d.x), p8_fixed_mul(d.y, d.y));
}

/* A started game's first generated planet and its nearest one, both
 * colonized by a bare project. */
static void colonized_pair(void **qu, void **re) {
    const entity_list_t *jos;
    int i;

    start_game();
    jos = entities_by_tag(TAG_PLANET);
    assert(jos != NULL && jos->count > 1);
    *qu = jos->items[0];
    *re = NULL;
    for (i = 1; i < jos->count; i++) {
        void *e = jos->items[i];
        if (*re == NULL || dist2(pos_of(e), pos_of(*qu)) < dist2(pos_of(*re), pos_of(*qu))) {
            *re = e;
        }
    }
    planet_apply_project(*qu, research_project_new(NULL));
    planet_apply_project(*re, research_project_new(NULL));
}

static void *build_slipway(void *qu);
static void drag_over(void *jc, void *hovered, mouse_pos_t mouse);

/* A slipway from the start planet dragged over the end planet as
 * slipway_drag sees a drag (it reads the hovered planet off the main user
 * interface), held until its state time reaches 15, not paid. */
static void *dragged(void *qu, void *re) {
    void *jc = build_slipway(qu);
    vec_t mp = vec_add(vec_mul(pos_of(re), p8_fixed_from_int(128)), vec_make(p8_fixed_from_int(1), p8_fixed_from_int(1)));

    drag_over(jc, re, mouse_world_only(mp));
    main_ui()->hovered = entity_id_link(NULL);
    while (as_slipway(jc)->header.state_time < p8_fixed_from_int(15)) {
        update_entities();
    }
    return jc;
}

/* Two colonized planets joined by one slipway, paid by slipway_drop as a
 * drag pays it, then left to settle. */
static void world(void) {
    void *qu;
    void *re;
    void *jc;
    int i;

    colonized_pair(&qu, &re);
    jc = dragged(qu, re);
    slipway_class()->drop(jc, re);
    for (i = 0; i < SETTLE_TICKS; i++) {
        update_entities();
    }
    assert(entities_by_tag(TAG_SLIPWAY) != NULL && entities_by_tag(TAG_SLIPWAY)->count == 1);
}

static void *the_slipway(void) {
    return entities_by_tag(TAG_SLIPWAY)->items[0];
}

static void *build_slipway(void *qu) {
    slipway_keys_t keys = {0};

    keys.start = entity_id_link(qu);
    return slipway_new(&keys);
}

static vec_t vec_at(int x, int y) {
    return vec_make(p8_fixed_from_int(x), p8_fixed_from_int(y));
}

/* slipway_drag reads the hovered entity off the main user interface. */
static void drag_over(void *jc, void *hovered, mouse_pos_t mouse) {
    main_ui()->hovered = entity_id_link(hovered);
    class_of(jc)->drag(jc, mouse);
}

/* Every entity of a tag list begins with its header. */
static const entity_t *entity_of(const void *e) {
    assert(e != NULL);
    return e;
}

static void *first_with_state(tag_t tag, state_t state) {
    const entity_list_t *list = entities_by_tag(tag);
    int i;
    for (i = 0; list != NULL && i < list->count; i++) {
        const entity_t *ke = entity_of(list->items[i]);
        if (ke->state == state) {
            return list->items[i];
        }
    }
    return NULL;
}

/* A fresh slipway: the archetype's defaults, the entity's state time, the
 * start, and the transit dots and served planets slipway_construct made;
 * registered like any entity with draw and draw_overlay. */
static void check_fresh_slipway(const void *e, const void *qu) {
    const slipway_t *jc = as_slipway(e);
    const int_seq_t *k_;
    const slipway_served_planets_t *hg;
    size_t i;

    assert(jc->archetype == &slipway_archetype && jc->targeting.archetype == &slipway_archetype.targeting);
    assert(jc->header.state == ENTITY_STATE_PLACING);
    assert(jc->header.state_time == 0);
    assert(jc->header.layer == p8_fixed_from_int(2));
    assert(jc->targeting.archetype->clock_ticks == 0);
    printf("[attach_distance] 0x%08X\n", (unsigned)jc->targeting.archetype->attach_distance);
    assert(jc->targeting.archetype->attach_distance == (p8_fixed_t)ATTACH_DISTANCE_BITS);
    assert(jc->fade_in_step == p8_fixed_from_int(20));

    k_ = jc->archetype->trail_colors;
    assert(k_ == &slipway_trail_colors_seq && k_->count == 20);
    for (i = 0; i < 20; i++) {
        assert(k_->items[i] == want_trail_colors[i]);
    }
    assert(jc->header.class->tags == TAG(TAG_SLIPWAY));
    assert(slipway_start(jc) == qu);

    assert(pointer_list_count(&jc->transit_dots) == 0);
    hg = &jc->served_planets;
    assert(hg->count == 0);

    assert(slipway_end(jc) == NULL && !jc->targeting.has_cost && jc->targeting.cost == 0 && !entity_has(&jc->header, ENTITY_HAS_INFRASPACE) && targeting_target(&jc->targeting) == NULL);
    assert(entity_get(jc->targeted_by) == NULL && !jc->header.is_dead);
    assert(class_of(jc)->drag != NULL && class_of(jc)->drop != NULL);
    assert(class_of(jc)->draw_overlay != NULL && class_of(jc)->draw != NULL);
    assert(class_of(jc)->hit_test == NULL && class_of(jc)->states[ENTITY_STATE_PLACING] == NULL &&
           class_of(jc)->states[ENTITY_STATE_NORMAL] == NULL && class_of(jc)->left_press == NULL && class_of(jc)->accept_drop == NULL);

    assert(entity_list_listed(live_entities(), jc) && entity_list_listed(entities_by_message(ENTITY_MESSAGE_DRAW), jc) && entity_list_listed(entities_by_message(ENTITY_MESSAGE_DRAW_OVERLAY), jc));
    assert(entity_list_listed(entities_by_tag(TAG_SLIPWAY), jc));
    assert(!entity_list_listed(entities_by_message(ENTITY_MESSAGE_HIT_TEST), jc) && !entity_list_listed(entities_by_message(ENTITY_MESSAGE_NEW_YEAR), jc) && !entity_list_listed(entities_by_message(ENTITY_MESSAGE_REFRESH), jc));
}

static void test_construction(void) {
    void *ly;
    void *by_left_press;
    void *by_lookup;
    down_t made;

    world();
    ly = first_with_state(TAG_PLANET, ENTITY_STATE_SETTLED);
    assert(ly != NULL);
    made = class_of(ly)->left_press(ly, (mouse_pos_t){0});
    assert(made.stop && entity_get(made.held) != NULL);
    by_left_press = entity_get(made.held);
    check_fresh_slipway(by_left_press, ly);
    by_lookup = build_slipway(ly);
    check_fresh_slipway(by_lookup, ly);

    assert(as_slipway(by_left_press)->archetype == as_slipway(by_lookup)->archetype);
    assert(as_slipway(by_left_press)->header.class->tags == as_slipway(by_lookup)->header.class->tags);
    assert(as_slipway(by_left_press)->transit_dots.zone != as_slipway(by_lookup)->transit_dots.zone);
}

/* The transit-dot roll is visual only: it draws from the visual stream,
 * dot or no dot, and leaves the game stream where it was. */
static void test_transit_dot_roll_draws_from_the_visual_stream(void) {
    void *ly;
    slipway_t *jc;
    uint32_t game;
    uint32_t visual;

    world();
    ly = first_with_state(TAG_PLANET, ENTITY_STATE_SETTLED);
    assert(ly != NULL);
    jc = as_slipway(build_slipway(ly));
    game = p8_fixed_rnd_state();
    visual = visual_random_state();
    for (int i = 0; i < 20; i++) {
        slipway_spawn_transit_dot(jc, ly, NULL);
    }
    assert(p8_fixed_rnd_state() == game && "the transit-dot roll draws no game number");
    assert(visual_random_state() != visual);
}

/* Without an end, slipway_drop only sets is_dead; with one, the slipway
 * joins both planets' slipways and targeting_confirm charges its cost. */
static void test_drop(void) {
    uint32_t hash = 2166136261u;
    void *held;
    void *origin;
    void *target;
    void *refused;
    p8_fixed_t before;
    p8_fixed_t od;

    colonized_pair(&origin, &target);
    held = dragged(origin, target);
    assert(entity_list_listed(entities_by_tag(TAG_SLIPWAY), held));
    assert(slipway_start(as_slipway(held)) == origin && slipway_end(as_slipway(held)) == target);
    assert(end_kind(held) == END_PLANET && target_kind(held) == TARGET_ABSENT);
    assert(id_list_count(&as_planet(origin)->slipways) == 0 && id_list_count(&as_planet(target)->slipways) == 0);
    before = credits();
    assert(as_slipway(held)->targeting.has_cost);
    od = as_slipway(held)->targeting.cost;
    hash = fold_slipway(hash, held);

    refused = build_slipway(origin);
    class_of(refused)->drop(refused, NULL);
    assert(is_dead_state(as_slipway(refused)) == 2 && credits() == before);
    assert(id_list_count(&as_planet(origin)->slipways) == 0);
    hash = fold_slipway(hash, refused);

    class_of(held)->drop(held, target);
    assert(as_slipway(held)->fade_in_step == p8_fixed_from_int(1));
    assert(slipway_end(as_slipway(held)) == target);
    assert(is_dead_state(as_slipway(held)) == 0);
    assert(id_list_count(&as_planet(origin)->slipways) == 1 && id_list_count(&as_planet(target)->slipways) == 1);
    assert(credits() == p8_fixed_sub(before, od));
    hash = fold_slipway(hash, held);
    hash = fold_planets(hash, 1);
    hash = fnv(hash, (uint32_t)credits());
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

/* What a drop can change: the economy's figures, each planet's tier,
 * income, production and row, the slipway's state. */
static uint32_t fold_after_drop(const void *jc) {
    const economy_t *economy = main_economy_typed();
    const entity_list_t *planets = entities_by_tag(TAG_PLANET);
    uint32_t h = fold_slipway(2166136261u, jc);
    int i;

    h = fnv(fnv(h, (uint32_t)is_dead_state(as_slipway(jc))), (uint32_t)as_slipway(jc)->fade_in_step);
    h = fnv(fnv(h, (uint32_t)economy->credits), (uint32_t)economy->income);
    h = fnv(fnv(h, (uint32_t)economy->research_points), (uint32_t)economy->era);
    h = fnv(fnv(h, (uint32_t)economy->year), (uint32_t)economy->year_ticks);
    h = fnv(fnv(h, (uint32_t)economy->has), (uint32_t)economy->happiness);
    h = fnv(fnv(h, (uint32_t)economy->has_end_status), (uint32_t)economy->save_pending);
    h = economy->end_status != NULL ? fnv_str(h, economy->end_status) : fnv(h, 0u);
    h = fold_resource_map(fold_resource_map(h, &economy->production), &economy->demand);
    for (i = 0; planets != NULL && i < planets->count; i++) {
        const planet_t *planet = as_planet(planets->items[i]);
        h = fnv(fnv(h, (uint32_t)planet->development_tier), (uint32_t)planet->has_development_tier);
        h = fnv(fnv(h, (uint32_t)planet->net_income), (uint32_t)planet->has_net_income);
        h = fnv(fnv(h, (uint32_t)planet->production.has_resource), (uint32_t)planet->production.resource);
        h = fnv(h, (uint32_t)planet->production.amount);
        h = fold_row(h, planet->layout_row);
    }
    return fold_planets(h, 1);
}

/* The drop as it ran when it advanced the clock twice: the charge with its
 * own clock advance, both attaches, then one tick. */
static void drop_with_two_clock_advances(void *held, void *target) {
    slipway_t *jc = as_slipway(held);
    entity_t *start = slipway_start(jc);

    if (!targeting_confirm(held)) {
        jc->header.is_dead = 1;
        return;
    }
    jc->fade_in_step = p8_fixed_from_int(1);
    jc->end = entity_id_link(target);
    jc->end_is_point = 0;
    start->class->attach_slipway(start, held);
    class_of(target)->attach_slipway(target, held);
    economy_advance_clock(main_economy_typed(), p8_fixed_from_int(1));
}

/* A colonized pair, a dragged slipway, the year at `year_ticks` and the
 * credits at `credits` (or as they are for a negative value), then the
 * drop; run in its own child, which sends back fold_after_drop. */
static uint32_t drop_in_child(int two_advances, int year_ticks, int credits) {
    int fds[2];
    pid_t child;
    int status;
    uint32_t got = 0;

    assert(pipe(fds) == 0);
    fflush(stdout);
    child = fork();
    assert(child >= 0);
    if (child == 0) {
        void *origin;
        void *target;
        void *held;
        uint32_t h;

        close(fds[0]);
        colonized_pair(&origin, &target);
        held = dragged(origin, target);
        main_economy_typed()->year_ticks = p8_fixed_from_int(year_ticks);
        if (credits >= 0) {
            main_economy_typed()->credits = p8_fixed_from_int(credits);
        }
        if (two_advances) {
            drop_with_two_clock_advances(held, target);
        } else {
            class_of(held)->drop(held, target);
        }
        h = fold_after_drop(held);
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

/* The drop advances the clock once, after both attaches, and ends where
 * the old two advances ended: mid-year, on the tick that starts a new
 * year, and refused for want of credits. */
static void test_drop_ends_as_with_two_clock_advances(void) {
    static const int cases[][2] = {{0, -1}, {7, -1}, {15, -1}, {3, 0}};
    size_t i;

    for (i = 0; i < sizeof cases / sizeof cases[0]; i++) {
        uint32_t once = drop_in_child(0, cases[i][0], cases[i][1]);
        uint32_t twice = drop_in_child(1, cases[i][0], cases[i][1]);
        printf("[drop once] ticks %d credits %d: 0x%08X 0x%08X\n", cases[i][0], cases[i][1], (unsigned)once, (unsigned)twice);
        assert(once == twice);
    }
}

/* Beyond range the target is the too-far struct; over the pair an existing
 * slipway already joins, the target is that slipway; either way its
 * `targeted_by` is the new one. */
static void test_drag(void) {
    uint32_t h = 2166136261u;
    void *old;
    void *origin;
    void *target;
    void *far;
    void *twin;

    world();
    old = the_slipway();
    origin = slipway_start(as_slipway(old));
    target = slipway_end(as_slipway(old));

    far = build_slipway(origin);
    drag_over(far, NULL, mouse_world_only(point(-640, -640)));
    assert(end_kind(far) == END_THROWAWAY && target_kind(far) == TARGET_STATUS_TEXT);
    assert(entity_get(as_too_far(targeting_target(&as_slipway(far)->targeting))->targeted_by) == far);
    h = fold_slipway(h, far);

    twin = build_slipway(origin);
    drag_over(twin, target, mouse_world_only(vec_of(pos_ref(&as_planet(target)->header))));
    assert(end_kind(twin) == END_PLANET && slipway_end(as_slipway(twin)) == target);
    assert(target_kind(twin) == TARGET_SLIPWAY && targeting_target(&as_slipway(twin)->targeting) == old);
    assert(entity_get(as_slipway(old)->targeted_by) == twin);
    h = fold_slipway(h, twin);
    h = fold_slipway(h, old);
    check("drag", h, DRAG_HASH);
}

static void test_drag_takes_infraspace_from_its_start(void) {
    void *from_planet;
    void *from_infraspace_gate;

    world();
    from_planet = build_slipway(planet_new(NULL));
    drag_over(from_planet, NULL, mouse_world_only(point(-640, -640)));
    assert(!entity_has(&as_slipway(from_planet)->header, ENTITY_HAS_INFRASPACE));
    from_infraspace_gate = build_slipway(infraspace_gate_new(NULL));
    drag_over(from_infraspace_gate, NULL, mouse_world_only(point(-640, -640)));
    assert(entity_has(&as_slipway(from_infraspace_gate)->header, ENTITY_HAS_INFRASPACE) &&
           as_slipway(from_infraspace_gate)->header.infraspace == infraspace_gate_archetype.infraspace);
}

/* A NULL end falls back to the start. */
static void test_other_end(void) {
    uint32_t h = 2166136261u;
    void *old;
    void *origin;
    void *target;
    void *fresh;

    world();
    old = the_slipway();
    origin = slipway_start(as_slipway(old));
    target = slipway_end(as_slipway(old));
    fresh = build_slipway(origin);

    assert(slipway_other_end(old, origin) == target);
    assert(slipway_other_end(old, target) == origin);
    assert(slipway_other_end(fresh, origin) == origin);
    assert(slipway_other_end(fresh, target) == origin);
    h = fold_ref(h, slipway_other_end(old, origin));
    h = fold_ref(h, slipway_other_end(old, target));
    h = fold_ref(h, slipway_other_end(fresh, origin));
    h = fold_ref(h, slipway_other_end(fresh, NULL));
    check("other_end", h, OTHER_END_HASH);
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

/* Under the game's camera: the slipway alone on a cleared screen,
 * `targeted_by` cleared after drawing; then a planet whose `targeted_by` is
 * a slipway, circled at the attach distance's radius. */
static void test_draw(void) {
    uint32_t hash = 2166136261u;
    uint32_t blank;
    uint32_t plain;
    uint32_t circled;
    vec_t main_camera_pos;
    void *old;
    void *origin;
    int i;

    world();
    old = the_slipway();
    origin = slipway_start(as_slipway(old));
    assert(main_camera_typed() != NULL);
    main_camera_pos = vec_of(pos_ref(&main_camera_typed()->header));
    p8_camera(&graphics, p8_fixed_flr_int(main_camera_pos.x), p8_fixed_flr_int(main_camera_pos.y));
    p8_cls(&memory);
    blank = screen_hash();

    for (i = 0; i < 3; i++) {
        as_slipway(old)->targeted_by = entity_id_of(old);
        p8_cls(&memory);
        class_of(old)->draw(old, (vec_t){0, 0});
        assert(entity_get(as_slipway(old)->targeted_by) == NULL);
        assert(screen_hash() != blank);
        hash = fnv(hash, screen_hash());
        hash = fold_slipway(hash, old);
    }

    p8_cls(&memory);
    class_of(origin)->draw(origin, entity_pos(&as_planet(origin)->header));
    plain = screen_hash();
    as_planet(origin)->targeted_by = entity_id_of(old);
    p8_cls(&memory);
    class_of(origin)->draw(origin, entity_pos(&as_planet(origin)->header));
    circled = screen_hash();
    assert(entity_get(as_planet(origin)->targeted_by) == NULL);
    assert(circled != plain);
    hash = fnv(hash, plain);
    hash = fnv(hash, circled);
    check("draw", hash, DRAW_HASH);
}

enum { SPIED_BATCHES = 8 };

typedef unsigned char p8_screen_copy_t[P8_SCREEN_HEIGHT][P8_SCREEN_WIDTH];

typedef struct {
    int count;
    int col;
    p8_segment_t segments[4];
} spied_batch_t;

static spied_batch_t spied[SPIED_BATCHES];
static int spied_batches;
static int spied_single_lines;

static void spy_lines(const p8_graphics_t *gfx, p8_vram_t *mem, const p8_segment_t *segments, int count, int col) {
    assert(spied_batches < SPIED_BATCHES && count <= 4);
    spied[spied_batches].count = count;
    spied[spied_batches].col = col;
    memcpy(spied[spied_batches].segments, segments, (size_t)count * sizeof *segments);
    spied_batches++;
    p8_gfx_software_backend()->lines(gfx, mem, segments, count, col);
}

static void spy_line(const p8_graphics_t *gfx, p8_vram_t *mem, int x1, int y1, int x2, int y2, int col) {
    spied_single_lines++;
    p8_gfx_software_backend()->line(gfx, mem, x1, y1, x2, y2, col);
}

static const p8_draw_backend_t *spy_on_lines(void) {
    static p8_draw_backend_t spy;
    spy = *p8_gfx_software_backend();
    spy.lines = spy_lines;
    spy.line = spy_line;
    spied_batches = 0;
    spied_single_lines = 0;
    return p8_draw_backend_set(&spy);
}

static void assert_offsets(const spied_batch_t *batch, int i, int dx, int dy) {
    const p8_segment_t *first = &batch->segments[0];
    const p8_segment_t *s = &batch->segments[i];
    assert(s->x1 - first->x1 == dx && s->y1 - first->y1 == dy);
    assert(s->x2 - first->x2 == dx && s->y2 - first->y2 == dy);
}

/* Each bundle is one lines call: dx outer, dy inner, as the old p8_line loop drew them. */
static void test_draw_issues_one_lines_call_per_bundle_with_its_lines_in_order(void) {
    void *old;

    world();
    old = the_slipway();
    const p8_draw_backend_t *previous = spy_on_lines();
    p8_cls(&memory);
    slipway_draw(&graphics, &memory, as_slipway(old));
    p8_draw_backend_set(previous);

    assert(spied_batches == 2 && spied_single_lines == 0);
    for (int b = 0; b < 2; b++) {
        int step = b == 0 ? 3 : 1;
        assert(spied[b].count == 4);
        assert_offsets(&spied[b], 1, 0, step);
        assert_offsets(&spied[b], 2, step, 0);
        assert_offsets(&spied[b], 3, step, step);
    }
    assert(spied[1].segments[0].x1 - spied[0].segments[0].x1 == 1 && "border from -1, line from 0");
}

static void test_draw_line_flushes_every_four_segments_and_the_rest(void) {
    void *old;
    static p8_screen_copy_t batched;

    world();
    old = the_slipway();
    vec_t camera = vec_of(pos_ref(&main_camera_typed()->header));
    p8_camera(&graphics, p8_fixed_flr_int(camera.x), p8_fixed_flr_int(camera.y));
    p8_cls(&memory);
    const p8_draw_backend_t *previous = spy_on_lines();
    slipway_draw_line(&graphics, &memory, slipway_line_ends(as_slipway(old)), 0, 2, 1, 7);
    p8_draw_backend_set(previous);
    memcpy(batched, memory.screen, sizeof batched);
    assert(screen_hash() != 2166136261u);

    p8_cls(&memory);
    for (int b = 0; b < spied_batches; b++) {
        for (int i = 0; i < spied[b].count; i++) {
            const p8_segment_t *s = &spied[b].segments[i];
            p8_line(&graphics, &memory, s->x1, s->y1, s->x2, s->y2, spied[b].col);
        }
    }
    assert(memcmp(batched, memory.screen, sizeof batched) == 0 && "the old loop's single lines give the same pixels");

    assert(spied_batches == 3 && spied_single_lines == 0);
    assert(spied[0].count == 4 && spied[1].count == 4 && spied[2].count == 1);
    assert(spied[0].col == 7 && spied[2].col == 7);
    assert_offsets(&spied[0], 3, 1, 0);
    assert(spied[2].segments[0].x1 - spied[0].segments[0].x1 == 2 && spied[2].segments[0].y1 - spied[0].segments[0].y1 == 2);
}

/* Whole offsets on floored ends give the segments the per-segment fixed adds and floors gave. */
static void test_draw_line_segments_equal_the_fixed_point_offsets_on_any_end(void) {
    static const int32_t raws[] = {0x00000000, 0x00008000, 0x0003c000, 0x00140001, -0x00000001, -0x00004000,
                                   -0x00010000, -0x0001c000, -0x00c80000, 0x0000ffff, -0x0000ffff, 0x7ffc0000};
    static const int ranges[][3] = {{-1, 2, 3}, {0, 1, 1}, {0, 2, 1}};
    const size_t n = sizeof raws / sizeof raws[0];

    world();
    for (size_t i = 0; i < n; i++) {
        for (size_t r = 0; r < sizeof ranges / sizeof ranges[0]; r++) {
            slipway_line_ends_t ends;
            int l = ranges[r][0], h = ranges[r][1], step = ranges[r][2];
            int segment = 0;
            ends.start.x = (p8_fixed_t)raws[i];
            ends.start.y = (p8_fixed_t)raws[(i + 3) % n];
            ends.end.x = (p8_fixed_t)raws[(i + 5) % n];
            ends.end.y = (p8_fixed_t)raws[(i + 7) % n];

            const p8_draw_backend_t *previous = spy_on_lines();
            slipway_draw_line(&graphics, &memory, ends, l, h, step, 9);
            p8_draw_backend_set(previous);

            for (int dx = l; dx <= h; dx += step) {
                for (int dy = l; dy <= h; dy += step) {
                    const spied_batch_t *batch = &spied[segment / 4];
                    const p8_segment_t *s = &batch->segments[segment % 4];
                    assert(segment / 4 < spied_batches && segment % 4 < batch->count && batch->col == 9);
                    assert(s->x1 == p8_fixed_flr_int(p8_fixed_add(ends.start.x, p8_fixed_from_int(dx))));
                    assert(s->y1 == p8_fixed_flr_int(p8_fixed_add(ends.start.y, p8_fixed_from_int(dy))));
                    assert(s->x2 == p8_fixed_flr_int(p8_fixed_add(ends.end.x, p8_fixed_from_int(dx))));
                    assert(s->y2 == p8_fixed_flr_int(p8_fixed_add(ends.end.y, p8_fixed_from_int(dy))));
                    segment++;
                }
            }
            assert(spied_batches == (segment + 3) / 4 && spied[spied_batches - 1].count == (segment - 1) % 4 + 1);
        }
    }
}

/* The end points as the per-call computation gave them: each end wobbles on its own period. */
static slipway_line_ends_t ends_as_before(const slipway_t *self) {
    p8_fixed_t t = self->header.state_time;
    slipway_line_ends_t ends;
    ends.start = vec_add(slipway_end_pos(slipway_start(self)),
                         vec_from_polar(P8_FIXED_LIT(0x00018000, 1.5), p8_fixed_div(t, p8_fixed_from_int(126))));
    ends.end = vec_add(slipway_end_pos(slipway_end(self)),
                       vec_from_polar(P8_FIXED_LIT(0x00018000, 1.5), p8_fixed_div(t, p8_fixed_from_int(176))));
    return ends;
}

/* Both bundles of a draw start from the end points computed once, at every state time. */
static void test_draw_bundles_start_from_the_line_ends_at_every_state_time(void) {
    static const int times[] = {0, 1, 37, 63, 126, 176, 500, 2771, 9001};
    void *old;

    world();
    old = the_slipway();
    for (size_t i = 0; i < sizeof times / sizeof times[0]; i++) {
        slipway_t *self = as_slipway(old);
        self->header.state_time = p8_fixed_from_int(times[i]);
        slipway_line_ends_t want = ends_as_before(self);
        slipway_line_ends_t got = slipway_line_ends(self);
        assert(got.start.x == want.start.x && got.start.y == want.start.y);
        assert(got.end.x == want.end.x && got.end.y == want.end.y);

        const p8_draw_backend_t *previous = spy_on_lines();
        p8_cls(&memory);
        slipway_draw(&graphics, &memory, self);
        p8_draw_backend_set(previous);
        assert(spied_batches == 2);
        for (int b = 0; b < 2; b++) {
            int l = b == 0 ? -1 : 0;
            const p8_segment_t *first = &spied[b].segments[0];
            assert(first->x1 == p8_fixed_flr_int(p8_fixed_add(want.start.x, p8_fixed_from_int(l))));
            assert(first->y1 == p8_fixed_flr_int(p8_fixed_add(want.start.y, p8_fixed_from_int(l))));
            assert(first->x2 == p8_fixed_flr_int(p8_fixed_add(want.end.x, p8_fixed_from_int(l))));
            assert(first->y2 == p8_fixed_flr_int(p8_fixed_add(want.end.y, p8_fixed_from_int(l))));
        }
    }
}

/* Beyond range: draw draws the line under the game's camera, draw_overlay
 * writes the status text at the mouse where one with another target draws
 * a sprite and one without writes its cost, and drop onto a real planet is
 * refused without spending or joining anything. */
static void test_too_far(void) {
    uint32_t hash = 2166136261u;
    uint32_t empty;
    uint32_t too_far;
    uint32_t sprite;
    uint32_t cost;
    vec_t main_camera_pos;
    vec_t mouse;
    p8_fixed_t fade_in_step;
    p8_fixed_t before;
    void *old_slipway;
    void *start_planet;
    void *target;
    void *far_slipway;
    void *second_slipway;
    void *near;
    void *end_before_drop;

    world();
    old_slipway = the_slipway();
    start_planet = slipway_start(as_slipway(old_slipway));
    target = slipway_end(as_slipway(old_slipway));
    far_slipway = build_slipway(start_planet);
    drag_over(far_slipway, NULL, mouse_world_only(point(-640, -640)));
    assert(end_kind(far_slipway) == END_THROWAWAY && target_kind(far_slipway) == TARGET_STATUS_TEXT);
    assert(target_kind(old_slipway) == TARGET_ABSENT);
    assert(main_camera_typed() != NULL);
    main_camera_pos = vec_of(pos_ref(&main_camera_typed()->header));
    p8_camera(&graphics, p8_fixed_flr_int(main_camera_pos.x), p8_fixed_flr_int(main_camera_pos.y));
    p8_cls(&memory);
    empty = screen_hash();

    class_of(far_slipway)->draw(far_slipway, (vec_t){0, 0});
    assert(screen_hash() != empty);
    hash = fnv(hash, screen_hash());

    second_slipway = build_slipway(start_planet);
    drag_over(second_slipway, target, mouse_world_only(vec_of(pos_ref(&as_planet(target)->header))));
    assert(target_kind(second_slipway) == TARGET_SLIPWAY);
    near = build_slipway(start_planet);
    drag_over(near, NULL, mouse_world_only(vec_add(vec_of(pos_ref(&as_planet(start_planet)->header)), vec_make(p8_fixed_from_int(16), 0))));
    assert(end_kind(near) == END_THROWAWAY && target_kind(near) == TARGET_ABSENT && as_slipway(near)->targeting.has_cost);
    assert(main_mouse_typed() != NULL);
    mouse = vec_of(pos_ref(&main_mouse_typed()->header));
    p8_camera(&graphics, p8_fixed_flr_int(mouse.x) - 64, p8_fixed_flr_int(mouse.y) - 64);
    p8_cls(&memory);
    class_of(far_slipway)->draw_overlay(far_slipway, (vec_t){0, 0});
    too_far = screen_hash();
    p8_cls(&memory);
    class_of(second_slipway)->draw_overlay(second_slipway, (vec_t){0, 0});
    sprite = screen_hash();
    p8_cls(&memory);
    class_of(near)->draw_overlay(near, (vec_t){0, 0});
    cost = screen_hash();
    assert(too_far != empty && sprite != empty && cost != empty);
    assert(too_far != sprite && too_far != cost);
    hash = fnv(hash, too_far);
    hash = fnv(hash, sprite);
    hash = fnv(hash, cost);

    before = credits();
    fade_in_step = as_slipway(far_slipway)->fade_in_step;
    end_before_drop = slipway_end(as_slipway(far_slipway));
    assert(id_list_count(&as_planet(start_planet)->slipways) == 1 && id_list_count(&as_planet(target)->slipways) == 1);
    class_of(far_slipway)->drop(far_slipway, target);
    assert(is_dead_state(as_slipway(far_slipway)) == 2);
    assert(credits() == before);
    assert(id_list_count(&as_planet(start_planet)->slipways) == 1 && id_list_count(&as_planet(target)->slipways) == 1);
    assert(slipway_end(as_slipway(far_slipway)) == end_before_drop && end_kind(far_slipway) == END_THROWAWAY);
    assert(as_slipway(far_slipway)->fade_in_step == fade_in_step);
    assert(target_kind(far_slipway) == TARGET_STATUS_TEXT);
    hash = fold_slipway(hash, far_slipway);
    hash = fnv(hash, (uint32_t)credits());
    check("too_far", hash, TOO_FAR_HASH);
}

static int same_vec(vec_t a, vec_t b) {
    return a.x == b.x && a.y == b.y;
}

/* The point holds a copy of the mouse vector: equal, never the same box. */
static int holds_copy_of(const vec_t *ra, vec_t m) {
    return ra != NULL && same_vec(*ra, m);
}

/* Over nothing, slipway_drag hangs the end on the slipway's own mouse
 * point, rewritten in place at each call: `pos` a copy of the mouse vector
 * it got, `map_pos` that vector over 128, registered nowhere; another
 * slipway has its own; the game zone does not grow. */
static void test_drag_mouse_point(void) {
    uint32_t hash = 2166136261u;
    vec_t first_mouse;
    vec_t second_mouse;
    vec_t first_pos;
    vec_t on_camera;
    void *start_planet;
    void *target;
    void *first;
    void *second;
    void *first_end;
    void *second_end;
    void *other_end;
    size_t used_before;
    int index;

    world();
    start_planet = slipway_start(the_slipway());
    target = slipway_end(the_slipway());
    first = build_slipway(start_planet);
    first_mouse = vec_at(-640, -640);
    drag_over(first, NULL, mouse_world_only(first_mouse));
    first_end = slipway_end(as_slipway(first));
    assert(first_end != NULL && end_kind(first) == END_THROWAWAY);
    assert(holds_copy_of(mouse_point_pos(first_end), first_mouse));
    first_pos = vec_of(mouse_point_map_pos(first_end));
    assert(same_vec(first_pos, vec_div(vec_of(mouse_point_pos(first_end)), p8_fixed_from_int(128))));
    assert(!entity_geom_is_on_screen(first_end));
    assert(!entity_list_listed(live_entities(), first_end) && !entity_list_listed(entities_by_tag(TAG_SLIPWAY), first_end) && !entity_list_listed(entities_by_tag(TAG_NODE), first_end));
    hash = fnv(hash, (uint32_t)first_pos.x);
    hash = fnv(hash, (uint32_t)first_pos.y);
    hash = fold_slipway(hash, first);

    second_mouse = vec_at(40, -24);
    drag_over(first, NULL, mouse_world_only(second_mouse));
    second_end = slipway_end(as_slipway(first));
    assert(second_end == first_end && end_kind(first) == END_THROWAWAY && "the mouse point is the slipway's own, rewritten in place");
    assert(holds_copy_of(mouse_point_pos(second_end), second_mouse));
    assert(same_vec(vec_of(mouse_point_map_pos(second_end)), vec_div(second_mouse, p8_fixed_from_int(128))));
    assert(!same_vec(vec_of(mouse_point_map_pos(second_end)), first_pos));
    hash = fold_slipway(hash, first);

    used_before = game_zone_used();
    for (index = 0; index < 10; index++) {
        drag_over(first, NULL, mouse_world_only(index % 2 ? first_mouse : second_mouse));
        assert(slipway_end(as_slipway(first)) == first_end);
    }
    assert(game_zone_used() == used_before && "a held slipway allocates nothing per drag");
    drag_over(first, NULL, mouse_world_only(second_mouse));

    second = build_slipway(start_planet);
    drag_over(second, NULL, mouse_world_only(first_mouse));
    other_end = slipway_end(as_slipway(second));
    assert(other_end != NULL && other_end != first_end && slipway_end(as_slipway(first)) == first_end);
    assert(holds_copy_of(mouse_point_pos(other_end), first_mouse) && same_vec(vec_of(mouse_point_map_pos(other_end)), first_pos));
    assert(holds_copy_of(mouse_point_pos(first_end), second_mouse) && "another slipway's drag leaves this one's point as it was");
    hash = fold_slipway(hash, second);

    drag_over(first, target, mouse_world_only(vec_of(pos_ref(&as_planet(target)->header))));
    assert(slipway_end(as_slipway(first)) == target && end_kind(first) == END_PLANET);
    assert(holds_copy_of(mouse_point_pos(other_end), first_mouse));
    hash = fold_slipway(hash, first);
    check("mouse_point", hash, MOUSE_POINT_HASH);

    on_camera = vec_add(vec_of(pos_ref(&main_camera_typed()->header)),
                              vec_make(p8_fixed_from_int(80), p8_fixed_from_int(60)));
    drag_over(second, NULL, mouse_world_only(on_camera));
    assert(entity_geom_is_on_screen(slipway_end(as_slipway(second))));
}

/* slipway_obstruction falls through to the slipway tag when no planet lies
 * on the drag: a drag from a nearby planet to just past the slipway's
 * middle crosses it. */
static void test_obstruction_crosses_a_slipway(void) {
    const entity_list_t *jos;
    void *old;
    vec_t mid;
    int i;
    int crossed = 0;

    world();
    jos = entities_by_tag(TAG_PLANET);
    old = the_slipway();
    mid = vec_div(vec_add(pos_of(slipway_start(as_slipway(old))), pos_of(slipway_end(as_slipway(old)))), p8_fixed_from_int(2));
    for (i = 0; i < jos->count && !crossed; i++) {
        void *q = jos->items[i];
        vec_t past;
        void *a;

        if (q == slipway_start(as_slipway(old)) || q == slipway_end(as_slipway(old))) {
            continue;
        }
        past = vec_add(mid, vec_div(vec_sub(mid, pos_of(q)), p8_fixed_from_int(8)));
        a = build_slipway(q);
        drag_over(a, NULL, mouse_world_only(vec_mul(past, p8_fixed_from_int(128))));
        crossed = targeting_target(&as_slipway(a)->targeting) == old;
        if (!crossed) {
            entity_unregister(a);
        }
    }
    printf("[obstruction] crossed from planet %d\n", i - 1);
    assert(crossed);
}

/* The probe's archetype with its attach distance zeroed, as the wormhole's
 * is: every point of this short slipway lies within a probe's attach
 * distance of one of its own planets, which would win. */
static probe_archetype_t g_attach_distance_zero;

/* A probe set up as probe_new sets one up, but on that archetype and in
 * the heap. */
static probe_t *probe_with_zero_attach_distance(void) {
    probe_t *probe = malloc(sizeof *probe);

    g_attach_distance_zero = probe_archetype;
    g_attach_distance_zero.targeting.attach_distance = 0;
    probe_init_instance(probe, probe_class(), &g_attach_distance_zero);
    emitter_init(&probe->header);
    entity_construct(&probe->header);
    return probe;
}

/* A probe over the slipway targets it and sets its `targeted_by`. */
static void test_targeting_track(void) {
    uint32_t h = 2166136261u;
    void *old;
    void *probe;
    vec_t qu;
    vec_t re;
    int k;

    world();
    old = the_slipway();
    probe = probe_with_zero_attach_distance();
    qu = pos_of(slipway_start(as_slipway(old)));
    re = pos_of(slipway_end(as_slipway(old)));
    for (k = 1; k < 8; k++) {
        vec_t at = vec_add(qu, vec_div(vec_mul(vec_sub(re, qu), p8_fixed_from_int(k)),
                                                         p8_fixed_from_int(8)));
        class_of(probe)->drag(probe, mouse_pos(vec_mul(at, p8_fixed_from_int(128)), point(64, 64)));
        h = fold_ref(h, targeting_target(&((probe_t *)probe)->targeting));
        if (targeting_target(&((probe_t *)probe)->targeting) == old) {
            break;
        }
    }
    printf("[targeting_track] on the slipway at %d/8\n", k);
    assert(targeting_target(&((probe_t *)probe)->targeting) == old);
    assert(entity_get(as_slipway(old)->targeted_by) == probe);
    assert(holder_kind(entity_get(as_slipway(old)->targeted_by)) == TARGETED_BY_PROBE);
    h = fold_slipway(h, old);
    h = fold_ref(h, targeting_target(&((probe_t *)probe)->targeting));
    check("targeting_track", h, TARGETING_TRACK_HASH);
}

/* targeting_track aims at the first slipway of the slipway tag under the
 * probe, not the last: a second slipway laid over the first, registered
 * after it, never wins. */
static void test_targeting_track_takes_the_first_slipway_of_entities_by_tag(void) {
    void *old;
    void *twin;
    void *probe;
    vec_t qu;
    vec_t re;
    vec_t at;

    world();
    old = the_slipway();
    twin = build_slipway(slipway_end(as_slipway(old)));
    as_slipway(twin)->end = as_slipway(old)->start;
    assert(entities_by_tag(TAG_SLIPWAY)->count == 2 && entities_by_tag(TAG_SLIPWAY)->items[0] == old &&
           entities_by_tag(TAG_SLIPWAY)->items[1] == twin);
    probe = probe_with_zero_attach_distance();
    qu = pos_of(slipway_start(as_slipway(old)));
    re = pos_of(slipway_end(as_slipway(old)));
    at = vec_div(vec_add(qu, re), p8_fixed_from_int(2));
    class_of(probe)->drag(probe, mouse_pos(vec_mul(at, p8_fixed_from_int(128)), point(64, 64)));
    assert(targeting_target(&((probe_t *)probe)->targeting) == old);
    assert(entity_get(as_slipway(old)->targeted_by) == probe && entity_get(as_slipway(twin)->targeted_by) != probe);
}

static int is_a_slipway_struct(const void *e) {
    return e != NULL &&
           ((const entity_t *)e)->class == slipway_class();
}

static void must_abort(void (*write)(void *), void *jc) {
    pid_t child;
    int status;

    fflush(stdout);
    child = fork();
    assert(child >= 0);
    if (child == 0) {
        assert(freopen("/dev/null", "w", stderr) != NULL);
        write(jc);
        _exit(0);
    }
    assert(waitpid(child, &status, 0) == child);
    assert(WIFSIGNALED(status) && WTERMSIG(status) == SIGABRT);
}

static void served_planets_add_nil(void *hg) {
    slipway_served_planets_add(hg, NULL);
}

static void served_planets_add_one_more(void *hg) {
    slipway_served_planets_add(hg, "p91");
}

static void served_planets_add_too_long(void *hg) {
    slipway_served_planets_add(hg, "s1234567");
}

/* slipway_served_planets_add keeps each identifier once and in order,
 * slipway_served_planets_has reads anything else as absent; a NULL
 * identifier, one too long for the copy and a 91st planet are refused. */
static void test_served_planets_is_a_set_of_ids(void) {
    static char ids[SLIPWAY_SERVED_PLANETS_CAP][8];
    slipway_served_planets_t hg = {0};
    char buffer[PLANET_ID_SIZE];
    int i;

    slipway_served_planets_add(&hg, "p1");
    slipway_served_planets_add(&hg, "p2");
    slipway_served_planets_add(&hg, "p1");
    assert(hg.count == 2 && strcmp(hg.ids[0], "p1") == 0 && strcmp(hg.ids[1], "p2") == 0);
    assert(slipway_served_planets_has(&hg, "p2") && !slipway_served_planets_has(&hg, "p3"));
    assert(!slipway_served_planets_has(&hg, NULL) && !slipway_served_planets_has(NULL, "p1"));
    must_abort(served_planets_add_nil, &hg);

    snprintf(buffer, sizeof buffer, "s-32768");
    slipway_served_planets_add(&hg, buffer);
    snprintf(buffer, sizeof buffer, "p9");
    assert(slipway_served_planets_has(&hg, "s-32768") && !slipway_served_planets_has(&hg, "p9") &&
           "the set holds a copy of the id, not the caller's buffer");
    must_abort(served_planets_add_too_long, &hg);

    hg.count = 0;
    for (i = 0; i < SLIPWAY_SERVED_PLANETS_CAP; i++) {
        snprintf(ids[i], sizeof ids[i], "p%d", i + 1);
        slipway_served_planets_add(&hg, ids[i]);
    }
    assert(hg.count == SLIPWAY_SERVED_PLANETS_CAP && slipway_served_planets_has(&hg, "p90"));
    must_abort(served_planets_add_one_more, &hg);
}

/* Built in a game or directly by slipway_new, a slipway is a struct with
 * its five methods. */
static void test_slipway_is_a_struct(void) {
    void *old;
    void *jc;
    slipway_keys_t keys = {0};
    void *direct;

    world();
    old = the_slipway();
    jc = build_slipway(slipway_start(as_slipway(old)));
    keys.start = as_slipway(old)->start;
    direct = slipway_new(&keys);
    assert(slipway_start(as_slipway(direct)) == slipway_start(as_slipway(old)));
    assert(is_a_slipway_struct(old) && is_a_slipway_struct(jc) && is_a_slipway_struct(direct));
    assert(class_of(jc)->drag != NULL && class_of(jc)->drop != NULL);
    assert(class_of(jc)->draw_overlay != NULL && class_of(jc)->draw != NULL);
    assert(class_of(jc)->states[ENTITY_STATE_PLACING] == NULL && class_of(jc)->states[ENTITY_STATE_NORMAL] == NULL &&
           class_of(jc)->left_press == NULL && class_of(jc)->accept_drop == NULL);
}

/* Beyond range, every slipway gets the same struct: the status text fixed,
 * `targeted_by` the last slipway_drag to write it, no method. */
static void test_too_far_is_a_struct(void) {
    void *origin;
    void *far;
    void *other;
    slipway_too_far_t *et;

    world();
    origin = slipway_start(the_slipway());
    far = build_slipway(origin);
    drag_over(far, NULL, mouse_world_only(point(-640, -640)));
    et = as_too_far(targeting_target(&as_slipway(far)->targeting));
    assert(entity_get(et->targeted_by) == far);
    assert(class_of(et)->draw_overlay == NULL && class_of(et)->draw == NULL && class_of(et)->drag == NULL);

    other = build_slipway(origin);
    drag_over(other, NULL, mouse_world_only(point(-640, -640)));
    assert(targeting_target(&as_slipway(other)->targeting) == &et->header && entity_get(et->targeted_by) == other);
    assert(strcmp(et->status_text, "-too far-") == 0);
}

/* Over nothing, the end is a mouse point struct without method; a NULL
 * mouse vector leaves `pos` absent and `map_pos` zero. */
static void test_mouse_point_is_a_struct(void) {
    void *origin;
    void *jc;
    void *re;
    vec_t p;

    world();
    origin = slipway_start(the_slipway());
    jc = build_slipway(origin);
    drag_over(jc, NULL, mouse_world_only(point(-640, -640)));
    re = slipway_end(as_slipway(jc));
    assert(re != NULL &&
           ((const entity_t *)re)->class == slipway_mouse_point_class());
    assert(mouse_point_pos(re) != NULL && mouse_point_map_pos(re) != NULL);
    assert(class_of(re)->attach_slipway == NULL && class_of(re)->drag == NULL);

    drag_over(jc, NULL, (mouse_pos_t){0});
    re = slipway_end(as_slipway(jc));
    assert(is_throwaway(re) && mouse_point_pos(re) == NULL);
    p = vec_of(mouse_point_map_pos(re));
    assert(p.x == 0 && p.y == 0);

    drag_over(jc, NULL, mouse_world_only(point(0, 0)));
    re = slipway_end(as_slipway(jc));
    assert(mouse_point_pos(re) != NULL && "a world point given as zero is not an absent one");
}

static void *g_off_start;

static void build_off_row(void *e) {
    slipway_keys_t keys = {0};

    (void)e;
    keys.start = entity_id_link(g_off_start);
    (void)slipway_new(&keys);
}

/* A start key that is not a planet or a structure stops the run. */
static void test_slipway_new_stops_off_start(void) {
    void *off[5];
    void *old;
    void *far;
    size_t i;

    world();
    old = the_slipway();
    far = build_slipway(slipway_start(as_slipway(old)));
    drag_over(far, NULL, mouse_world_only(point(-640, -640)));
    assert(is_throwaway(slipway_end(as_slipway(far))));
    off[0] = burst_new(NULL);
    off[1] = probe_new(NULL);
    off[2] = wormhole_new();
    off[3] = far;
    off[4] = slipway_end(as_slipway(far));
    for (i = 0; i < sizeof off / sizeof off[0]; i++) {
        g_off_start = off[i];
        must_abort(build_off_row, NULL);
    }
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

/* A drawn slipway's grab arena holds exactly what it was built with; once
 * placed, its transit dots grow past the arena into the game zone, never a
 * p8_fatal. */
static void test_placed_slipway_grows_past_its_arena(void) {
    slipway_t *jc;
    const allocator_t *arena;
    size_t used_before;
    int i;

    world();
    jc = as_slipway(the_slipway());
    arena = jc->transit_dots.zone;
    assert(arena_used(arena) == arena_size(arena) && "a fresh grab arena is full at close");
    used_before = game_zone_used();
    for (i = jc->transit_dots.count; i <= SLIPWAY_TRANSIT_DOTS_RESERVE; i++) {
        pointer_list_add(&jc->transit_dots, &memory);
    }
    assert(game_zone_used() == used_before + 2 * SLIPWAY_TRANSIT_DOTS_RESERVE * sizeof(void *) &&
           "past its reserve, the list grows in the game zone");
    jc->transit_dots.count = 0;
}

static void test_placed_slipway_reserves_24_transit_dots(void) {
    slipway_t *jc;

    world();
    jc = as_slipway(the_slipway());
    assert(jc->transit_dots.capacity == 24);
}

static void test_links_resolve_until_the_flush(void) {
    void *start;
    void *end;
    slipway_t *jc;

    start_game();
    start = planet_new(NULL);
    end = planet_new(NULL);
    jc = build_slipway(start);
    drag_over(jc, end, mouse_world_only(point(0, 0)));
    assert(slipway_start(jc) == start && slipway_end(jc) == end);

    entity_unregister(end);
    entity_unregister(start);
    assert(slipway_start(jc) == start && slipway_end(jc) == end);
    cart_flush_exit_queue();
    assert(slipway_start(jc) == NULL && slipway_end(jc) == NULL);
}

static void test_targeted_by_resolves_until_the_flush(void) {
    void *old;
    void *twin;
    void *end;

    world();
    old = the_slipway();
    end = slipway_end(old);
    twin = build_slipway(slipway_start(old));
    drag_over(twin, end, mouse_world_only(vec_of(pos_ref(&as_planet(end)->header))));
    assert(entity_get(as_slipway(old)->targeted_by) == twin);
    entity_unregister(twin);
    assert(entity_get(as_slipway(old)->targeted_by) == twin);
    cart_flush_exit_queue();
    assert(entity_get(as_slipway(old)->targeted_by) == NULL);
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
    in_fresh_child(test_transit_dot_roll_draws_from_the_visual_stream);
    in_fresh_child(test_construction);
    in_fresh_child(test_drop);
    in_fresh_child(test_drop_ends_as_with_two_clock_advances);
    in_fresh_child(test_drag);
    in_fresh_child(test_drag_takes_infraspace_from_its_start);
    in_fresh_child(test_other_end);
    in_fresh_child(test_draw);
    in_fresh_child(test_draw_issues_one_lines_call_per_bundle_with_its_lines_in_order);
    in_fresh_child(test_draw_line_flushes_every_four_segments_and_the_rest);
    in_fresh_child(test_draw_line_segments_equal_the_fixed_point_offsets_on_any_end);
    in_fresh_child(test_draw_bundles_start_from_the_line_ends_at_every_state_time);
    in_fresh_child(test_targeting_track);
    in_fresh_child(test_targeting_track_takes_the_first_slipway_of_entities_by_tag);
    in_fresh_child(test_too_far);
    in_fresh_child(test_drag_mouse_point);
    in_fresh_child(test_obstruction_crosses_a_slipway);
    in_fresh_child(test_slipway_is_a_struct);
    in_fresh_child(test_served_planets_is_a_set_of_ids);
    in_fresh_child(test_too_far_is_a_struct);
    in_fresh_child(test_mouse_point_is_a_struct);
    in_fresh_child(test_slipway_new_stops_off_start);
    in_fresh_child(test_placed_slipway_grows_past_its_arena);
    in_fresh_child(test_placed_slipway_reserves_24_transit_dots);
    in_fresh_child(test_links_resolve_until_the_flush);
    in_fresh_child(test_targeted_by_resolves_until_the_flush);

    printf("test_slipway: all tests passed\n");
    return 0;
}
