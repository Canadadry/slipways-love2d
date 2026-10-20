/* The economy, the one main economy per game, read on its struct members:
 * its construction, its five functions (advance the clock, new year, spend,
 * recompute, invent), and the cheat code on the provisional main economy of
 * the title screen, in games built directly: no events file. */

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
#include "engine/entity_id.h"
#include "engine/bootstrap.h"
#include "game/cart.h"
#include "game/cheat.h"
#include "engine/dispatch_context.h"
#include "game/economy.h"
#include "engine/entity_registry.h"
#include "p8/fixed.h"
#include "game/main_economy.h"
#include "p8/gfx.h"
#include "p8/input.h"
#include "p8/vram.h"
#include "game/message_box.h"
#include "game/planet.h"
#include "game/planet_display.h"
#include "game/planet_family.h"
#include "game/probe.h"
#include "game/research.h"
#include "game/score_panel.h"
#include "game/slipway.h"
#include "engine/entity_class.h"
#include "game/ui.h"
#include "engine/vec.h"
#include "engine/game_zone.h"
#include "heap_allocator.h"

static mouse_pos_t mouse_world_only(vec_t world) {
    return (mouse_pos_t){.world = world, .has = MOUSE_POS_HAS_WORLD};
}

/* Project 12, a colony that needs two resources and exports one; project 3,
 * a project with an output and no inputs, which exports to any planet a
 * slipway joins it to. */
#define COLONY 12
#define EXPORTER 3

/* The tag entity structs carried, which the frozen hashes fold: any other
 * value moves every *_HASH. */
#define FROZEN_STRUCT_TAG 0x9E5C57C7u

#define CONSTRUCTION_HASH 0x8343CAA6u
#define ADVANCE_CLOCK_HASH 0xC143EEC1u
#define NEW_YEAR_HASH 0x6CD37E64u
#define SPEND_HASH 0xB07FE872u
#define RECOMPUTE_HASH 0x77BE9594u
#define INVENT_HASH 0x5EE5D720u

/* The tag `invented` carried as a tagged list, which the frozen hashes fold:
 * any other value moves every *_HASH. */
#define FROZEN_LIST_TAG 0x9E5C1157u

static p8_vram_t memory;
static p8_graphics_t graphics;
static p8_input_t input;
static cheat_t cheat;

/* NULL on the provisional main economy, as the economy functions take it. */
static economy_t *main_economy_self(void) {
    return main_economy_typed();
}

static economy_t *checked_main_economy(void) {
    assert(main_economy_typed() != NULL);
    return main_economy_typed();
}

static uint32_t fnv(uint32_t h, uint32_t word) {
    int i;
    for (i = 0; i < 4; i++) {
        h ^= (word >> (8 * i)) & 0xFFu;
        h *= 16777619u;
    }
    return h;
}

static const pointer_list_t *invented_of(const economy_t *hy) {
    return &hy->invented;
}

static uint32_t fold_string(uint32_t h, const char *s) {
    for (; *s != '\0'; s++) {
        h = fnv(h, (uint32_t)(unsigned char)*s);
    }
    return fnv(h, 0xFFu);
}

/* The kind words the frozen hashes fold before a value: any other value
 * moves them. */
enum { FIXED_KIND = 0, STRING_KIND = 1, BOOL_KIND = 2, REF_KIND = 3 };

static uint32_t fold_fixed(uint32_t h, p8_fixed_t x) {
    return fnv(fnv(h, (uint32_t)FIXED_KIND), (uint32_t)x);
}

/* The byte a table view folded before a positional key, frozen in the
 * constants that fold `production`/`demand`; the enum that gave it is
 * deleted. Changing it reddens those constants. */
enum { TABLE_INDEX_KEY = 0 };

/* `REF`, the count, then per entry the index-key kind, the resource and the
 * value: the format the constants froze. */
static uint32_t fold_resource_map(uint32_t h, const resource_map_t *map) {
    int i;

    h = fnv(h, (uint32_t)REF_KIND);
    h = fnv(h, (uint32_t)map->count);
    for (i = 0; i < map->count; i++) {
        h = fnv(h, (uint32_t)TABLE_INDEX_KEY);
        h = fnv(h, (uint32_t)map->entries[i].resource);
        h = fold_fixed(h, map->entries[i].value);
    }
    return h;
}

static uint32_t fold_absent(uint32_t h) {
    return fnv(h, 0xFFFFFFFFu);
}

/* A member of one kind: the kind word and the list's tag are constant by
 * the struct's shape, kept so that the frozen constants still prove the typed
 * reads. An unset flag or a NULL member folds as absent. */
static uint32_t fold_fixed_member(uint32_t h, int has, p8_fixed_t v) {
    return has ? fnv(fnv(h, (uint32_t)FIXED_KIND), (uint32_t)v) : fold_absent(h);
}

static uint32_t fold_list_member(uint32_t h, const pointer_list_t *l) {
    int i;

    if (l == NULL) {
        return fold_absent(h);
    }
    h = fnv(h, (uint32_t)REF_KIND);
    h = fnv(h, FROZEN_LIST_TAG);
    h = fnv(h, (uint32_t)l->count);
    for (i = 0; i < l->count; i++) {
        h = fold_string(fnv(h, (uint32_t)STRING_KIND), l->items[i]);
    }
    return h;
}

static uint32_t fold_resource_member(uint32_t h, int has, const resource_map_t *map) {
    return has ? fold_resource_map(h, map) : fold_absent(h);
}

/* No end yet is `has_end_status` with a NULL `end_status`. */
static uint32_t fold_end_status(uint32_t h, const economy_t *hy) {
    if (!hy->has_end_status) {
        return fold_absent(h);
    }
    if (hy->end_status == NULL) {
        return fnv(fnv(h, (uint32_t)BOOL_KIND), 0u);
    }
    return fold_string(fnv(h, (uint32_t)STRING_KIND), hy->end_status);
}

/* The 11 members of an economy, each with its kind and value. */
static uint32_t fold_economy(uint32_t h, const economy_t *hy) {
    h = fold_fixed_member(h, 1, hy->credits);
    h = fold_fixed_member(h, 1, hy->income);
    h = fold_fixed_member(h, 1, hy->research_points);
    h = fold_fixed_member(h, 1, hy->era);
    h = fold_list_member(h, &hy->invented);
    h = fold_fixed_member(h, 1, hy->year_ticks);
    h = fold_fixed_member(h, 1, hy->year);
    h = fold_resource_member(h, (hy->has & ECONOMY_HAS_PRODUCTION) != 0, &hy->production);
    h = fold_resource_member(h, (hy->has & ECONOMY_HAS_DEMAND) != 0, &hy->demand);
    h = fold_fixed_member(h, hy->has_happiness, hy->happiness);
    return fold_end_status(h, hy);
}

static uint32_t fold_main_economy(uint32_t h) {
    return fold_economy(h, checked_main_economy());
}

static void check(const char *label, uint32_t got, uint32_t want) {
    printf("[%s] 0x%08Xu\n", label, (unsigned)got);
    fflush(stdout);
    assert(got == want);
}

static void new_game(void) {
    cart_start_game(NULL);
}

/* A game started as cart_start_game() starts it, with no difficulty picked:
 * a NULL difficulty reads as difficulty 0. */
static void start_game(void) {
    p8_input_init(&input);
    dispatch_context_set_input(&input);
    cart_init();
    cart_start_game(NULL);
}

static planet_t *as_planet(void *e) {
    assert(planet_is_family(e));
    return e;
}

static vec_t pos_of(void *e) {
    assert(entity_has_map_pos(&as_planet(e)->header));
    return as_planet(e)->header.map_pos;
}

static p8_fixed_t dist2(vec_t a, vec_t b) {
    vec_t d = vec_sub(a, b);
    return p8_fixed_add(p8_fixed_mul(d.x, d.x), p8_fixed_mul(d.y, d.y));
}

static research_project_t *project(int k) {
    assert(research_projects_at(k) != NULL);
    return research_projects_at(k);
}

/* A generated planet colonized by COLONY, its nearest one by EXPORTER, the
 * two joined by a slipway paid by its drop as a drag pays it (the drag reads
 * the hovered planet off the main UI), then left to settle. */
static void built_world(void) {
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
    planet_apply_project(qu, project(COLONY));
    planet_apply_project(re, project(EXPORTER));

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
    for (i = 0; i < 120; i++) {
        update_entities();
    }
}

/* The descriptor every toast shares, taken off one built for that purpose;
 * it stays among the live entities. */
static const entity_class_t *toast_descriptor(void) {
    toast_keys_t keys = {0};
    const toast_t *ref;

    toast_keys_set_text(&keys, "ref");
    ref = toast_new(&keys);
    return ref->header.class;
}

/* Every live entity added since there were `from` of them: its tag, and for
 * a toast the text and color it shows. */
static uint32_t fold_new_entities(uint32_t h, int from, const entity_class_t *toast_cls) {
    const entity_list_t *hv = live_entities();
    int i;

    h = fnv(h, (uint32_t)(hv->count - from));
    for (i = from; i < hv->count; i++) {
        const void *e = hv->items[i];
        h = fnv(h, FROZEN_STRUCT_TAG);
        if (((const entity_t *)e)->class == toast_cls) {
            const toast_t *iv = e;
            h = fold_string(fnv(h, (uint32_t)STRING_KIND), iv->text);
            h = fold_fixed(h, iv->color);
        }
    }
    return h;
}

/* An economy built on its own holds the 7 members of its spec, `invented` an
 * empty list, and none of the 4 economy_recompute() writes; the main economy
 * after cart_start_game() holds all 11. Two games have two `invented` lists. */
static void test_construction(void) {
    uint32_t h = 2166136261u;
    economy_t *bare;
    const pointer_list_t *first;

    cart_init();
    bare = economy_new();
    assert(!(bare->has & ECONOMY_HAS_PRODUCTION) && !(bare->has & ECONOMY_HAS_DEMAND) && !bare->has_happiness);
    assert(!bare->has_end_status);
    h = fold_economy(h, bare);
    assert(invented_of(bare)->count == 0);
    assert(bare->credits == p8_fixed_from_int(100) && bare->year == p8_fixed_from_int(3401));

    new_game();
    assert((checked_main_economy()->has & ECONOMY_HAS_PRODUCTION) && (checked_main_economy()->has & ECONOMY_HAS_DEMAND));
    assert(checked_main_economy()->has_happiness && checked_main_economy()->has_end_status);
    h = fold_main_economy(h);
    first = invented_of(checked_main_economy());
    assert(first != invented_of(bare));
    pointer_list_add(&checked_main_economy()->invented, "a tech");

    cart_flush_exit_queue();
    new_game();
    assert(invented_of(checked_main_economy())->count == 0);
    h = fold_main_economy(h);
    check("construction", h, CONSTRUCTION_HASH);
}

static uint32_t fold_year_ticks_year(uint32_t h) {
    h = fold_fixed_member(h, 1, checked_main_economy()->year_ticks);
    return fold_fixed_member(h, 1, checked_main_economy()->year);
}

/* `year_ticks` accumulates; past 12 without "time compression", past 15
 * with it, the year turns once and `year_ticks` drops by that bound. */
static void test_advance_clock(void) {
    uint32_t h = 2166136261u;
    p8_fixed_t yr;

    start_game();
    yr = checked_main_economy()->year;
    assert(checked_main_economy()->year_ticks == p8_fixed_from_int(1));

    economy_advance_clock(main_economy_self(), p8_fixed_from_int(3));
    assert(checked_main_economy()->year_ticks == p8_fixed_from_int(4) && checked_main_economy()->year == yr);
    h = fold_year_ticks_year(h);
    economy_advance_clock(main_economy_self(), p8_fixed_from_int(8));
    assert(checked_main_economy()->year_ticks == p8_fixed_from_int(12) && checked_main_economy()->year == yr);
    h = fold_year_ticks_year(h);
    economy_advance_clock(main_economy_self(), p8_fixed_from_int(1));
    assert(checked_main_economy()->year_ticks == p8_fixed_from_int(1));
    assert(checked_main_economy()->year == p8_fixed_add(yr, p8_fixed_from_int(1)));
    h = fold_main_economy(h);

    pointer_list_add(&checked_main_economy()->invented, "time compression");
    economy_advance_clock(main_economy_self(), p8_fixed_from_int(14));
    assert(checked_main_economy()->year_ticks == p8_fixed_from_int(15));
    assert(checked_main_economy()->year == p8_fixed_add(yr, p8_fixed_from_int(1)));
    h = fold_year_ticks_year(h);
    economy_advance_clock(main_economy_self(), P8_FIXED_LIT(0x00008000, 0.5));
    assert(checked_main_economy()->year_ticks == P8_FIXED_LIT(0x00008000, 0.5));
    assert(checked_main_economy()->year == p8_fixed_add(yr, p8_fixed_from_int(2)));
    h = fold_main_economy(h);

    economy_advance_clock(NULL, p8_fixed_from_int(20));
    h = fold_main_economy(h);
    check("advance_clock", h, ADVANCE_CLOCK_HASH);
}

/* After a game with colonies: credits += income, research points += the
 * science production + 1, year + 1, and each new-year listener shows its
 * income. */
static void test_new_year(void) {
    uint32_t h = 2166136261u;
    const entity_class_t *toast_cls;
    p8_fixed_t nz;
    p8_fixed_t ge;
    p8_fixed_t jk;
    p8_fixed_t yr;
    int from;

    built_world();
    toast_cls = toast_descriptor();
    nz = checked_main_economy()->credits;
    ge = checked_main_economy()->income;
    jk = checked_main_economy()->research_points;
    yr = checked_main_economy()->year;
    h = fold_main_economy(h);

    from = live_entities()->count;
    economy_new_year(main_economy_self());
    assert(checked_main_economy()->credits == p8_fixed_add(nz, ge));
    assert(checked_main_economy()->research_points > jk);
    assert(checked_main_economy()->year == p8_fixed_add(yr, p8_fixed_from_int(1)));
    h = fold_main_economy(h);
    h = fold_new_entities(h, from, toast_cls);
    assert(live_entities()->count > from);

    economy_new_year(NULL);
    h = fold_main_economy(h);
    check("new_year", h, NEW_YEAR_HASH);
}

static uint32_t spend_step(uint32_t h, economy_t *self, int jt, int want, const entity_class_t *toast_cls) {
    int from = live_entities()->count;
    int got = economy_spend(self, p8_fixed_from_int(jt));
    assert(got == want);
    h = fnv(h, (uint32_t)got);
    h = fold_fixed_member(h, 1, checked_main_economy()->credits);
    return fold_new_entities(h, from, toast_cls);
}

/* Short of credits: 0 and "-no money-"; 0: paid, nothing shown; a cost and
 * a payment: the credits and the amount shown; a NULL economy holds none. */
static void test_spend(void) {
    uint32_t h = 2166136261u;
    const entity_class_t *toast_cls;
    int from;

    start_game();
    toast_cls = toast_descriptor();
    assert(checked_main_economy()->credits == p8_fixed_from_int(100));

    from = live_entities()->count;
    h = spend_step(h, main_economy_self(), 101, 0, toast_cls);
    assert(checked_main_economy()->credits == p8_fixed_from_int(100) && live_entities()->count == from + 1);
    assert(strcmp(((const toast_t *)live_entities()->items[from])->text, "-no money-") == 0);

    from = live_entities()->count;
    h = spend_step(h, main_economy_self(), 0, 1, toast_cls);
    assert(checked_main_economy()->credits == p8_fixed_from_int(100) && live_entities()->count == from);

    h = spend_step(h, main_economy_self(), 30, 1, toast_cls);
    assert(checked_main_economy()->credits == p8_fixed_from_int(70));
    h = spend_step(h, main_economy_self(), -20, 1, toast_cls);
    assert(checked_main_economy()->credits == p8_fixed_from_int(90));
    h = spend_step(h, main_economy_self(), 90, 1, toast_cls);
    assert(checked_main_economy()->credits == p8_fixed_from_int(0));

    h = spend_step(h, NULL, 5, 0, toast_cls);
    h = spend_step(h, NULL, 0, 1, toast_cls);
    h = spend_step(h, NULL, -3, 1, toast_cls);
    check("spend", h, SPEND_HASH);
}

static uint32_t fold_planets_net_income(uint32_t h) {
    const entity_list_t *jo = entities_by_tag(TAG_PLANET);
    int i;
    for (i = 0; jo != NULL && i < jo->count; i++) {
        const planet_t *p = as_planet(jo->items[i]);
        h = fold_fixed_member(h, p->has_net_income, p->net_income);
    }
    return h;
}

/* NULL: no end yet. */
static const char *end_status_string(void) {
    assert(checked_main_economy()->has_end_status);
    return checked_main_economy()->end_status;
}

/* The first producing planet: `needs` set and its own `production`. */
static planet_t *a_maker(void) {
    const entity_list_t *jos = entities_by_tag(TAG_PLANET);
    int i;

    for (i = 0; i < jos->count; i++) {
        planet_t *p = as_planet(jos->items[i]);
        if ((p->has & PLANET_HAS_NEEDS) && (p->has & PLANET_HAS_PRODUCTION) && p->production.has_resource) {
            return p;
        }
    }
    assert(0 && "built_world() makes a producing planet");
    return NULL;
}

/* `production`/`demand` recomputed each call -- a planet's resource moved
 * shows in `production`, a resource no planet makes any more is gone from it
 * --, `income`, `happiness`, each planet's `net_income`, and `end_status`:
 * none, "final score" at 3426, "bankrupt" with no income and under 3
 * credits; then a structure needing an unlisted resource gets it in
 * `demand`, from 1, gone once the need is. */
static void test_recompute(void) {
    uint32_t hash = 2166136261u;
    const resource_map_t *production;
    const resource_map_t *demand;
    planet_t *maker;
    planet_t *structure;
    pointer_list_t needs;
    p8_fixed_t need_amount = p8_fixed_from_int(99);
    need_t *need;
    p8_fixed_t made;
    p8_fixed_t amount;
    p8_fixed_t resource;

    built_world();
    production = &checked_main_economy()->production;
    demand = &checked_main_economy()->demand;
    assert((checked_main_economy()->has & ECONOMY_HAS_PRODUCTION) && (checked_main_economy()->has & ECONOMY_HAS_DEMAND));
    maker = a_maker();
    resource = maker->production.resource;
    assert(resource_map_get(production, resource, &made) && maker->production.amount > 0);
    maker->production.resource = p8_fixed_from_int(99);
    economy_recompute(main_economy_self());
    assert(resource_map_get(production, p8_fixed_from_int(99), &amount) && amount == maker->production.amount);
    assert(resource_map_get(production, resource, &amount) && amount == p8_fixed_sub(made, maker->production.amount));
    maker->production.resource = resource;
    economy_recompute(main_economy_self());
    assert(!resource_map_get(production, p8_fixed_from_int(99), &amount));
    assert(resource_map_get(production, resource, &amount) && amount == made);
    assert(end_status_string() == NULL);
    hash = fold_main_economy(hash);
    hash = fold_planets_net_income(hash);

    checked_main_economy()->year = p8_fixed_from_int(3426);
    economy_recompute(main_economy_self());
    assert(end_status_string() != NULL && strcmp(end_status_string(), "final score") == 0);
    hash = fold_main_economy(hash);

    checked_main_economy()->year = p8_fixed_from_int(3425);
    checked_main_economy()->credits = p8_fixed_from_int(2);
    economy_recompute(main_economy_self());
    hash = fold_main_economy(hash);
    hash = fnv(hash, (uint32_t)(end_status_string() == NULL));

    cart_flush_exit_queue();
    new_game();
    checked_main_economy()->credits = p8_fixed_from_int(2);
    economy_recompute(main_economy_self());
    assert(checked_main_economy()->income <= p8_fixed_from_int(0));
    assert(end_status_string() != NULL && strcmp(end_status_string(), "bankrupt") == 0);
    hash = fold_main_economy(hash);
    checked_main_economy()->credits = p8_fixed_from_int(3);
    economy_recompute(main_economy_self());
    assert(end_status_string() == NULL);
    hash = fold_main_economy(hash);

    economy_recompute(NULL);
    hash = fold_main_economy(hash);
    check("recompute", hash, RECOMPUTE_HASH);

    demand = &checked_main_economy()->demand;
    need = need_new(game_zone(), (resource_t){.kind = RESOURCE_FIXED, .number = need_amount});
    structure = structure_new(NULL);
    pointer_list_init(&needs, &heap_allocator);
    pointer_list_add(&needs, need);
    structure->needs = needs;
    structure->has |= PLANET_HAS_NEEDS;
    economy_recompute(main_economy_self());
    assert(resource_map_get(demand, p8_fixed_from_int(99), &amount) && amount == p8_fixed_from_int(1) - 0x2666);
    structure->has &= (uint16_t)~PLANET_HAS_NEEDS;
    economy_recompute(main_economy_self());
    assert(!resource_map_get(demand, p8_fixed_from_int(99), &amount));
}

/* A technology of that cost whose icon resource (0) is no resource code:
 * its research cost reads no demand for it. Never freed. */
static const technologies_entry_t *tech(const char *px, int od) {
    technologies_entry_t *t = malloc(sizeof *t);

    *t = (technologies_entry_t){px, 0, NULL, 0, od, NULL};
    return t;
}

/* Research points less the research cost, the name appended (nothing
 * without a name), `era` up one at each tier threshold while under 4, never
 * past it. */
static void test_invent(void) {
    static const char *const pxs[] = {"t1", "t2", "t3", "t4", "t5", "t6", "t7", "t8", "t9", "t10", "t11", "t12", "t13"};
    uint32_t h = 2166136261u;
    size_t k;

    start_game();
    checked_main_economy()->research_points = p8_fixed_from_int(500);

    economy_invent(main_economy_self(), tech(NULL, 10));
    assert(checked_main_economy()->research_points == p8_fixed_from_int(495));
    assert(invented_of(checked_main_economy())->count == 0 && checked_main_economy()->era == p8_fixed_from_int(1));
    h = fold_main_economy(h);

    for (k = 0; k < sizeof pxs / sizeof pxs[0]; k++) {
        economy_invent(main_economy_self(), tech(pxs[k], (int)k));
        assert(invented_of(checked_main_economy())->count == (int)k + 1);
        h = fnv(h, (uint32_t)checked_main_economy()->era);
        h = fnv(h, (uint32_t)checked_main_economy()->research_points);
    }
    assert(checked_main_economy()->era == p8_fixed_from_int(4));
    h = fold_main_economy(h);

    economy_invent(main_economy_self(), NULL);
    economy_invent(NULL, tech("t14", 3));
    assert(invented_of(checked_main_economy())->count == (int)(sizeof pxs / sizeof pxs[0]));
    h = fold_main_economy(h);
    check("invent", h, INVENT_HASH);
}

/* The four × + ○ chords on the title screen write the provisional; the
 * economy of the game then starts from its own spec. */
static void test_cheat_on_provisional(void) {
    int frames[16];
    const main_economy_placeholder_t *provisional;
    int fired = 0;
    int i;

    cart_init();
    provisional = main_economy_placeholder();
    assert(provisional != NULL && !provisional->has_research_points && !provisional->has_credits);

    memset(frames, 0, sizeof frames);
    for (i = 0; i < CHEAT_CHORDS; i++) {
        frames[2 * i] = P8_PAD_BUTTON_5 | P8_PAD_BUTTON_6;
    }
    cheat_init(&cheat);
    for (i = 0; i < 16; i++) {
        fired += cheat_step(&cheat, frames[i]);
    }
    assert(fired == 1 && main_economy_placeholder() == provisional);
    assert(provisional->has_research_points && provisional->research_points == p8_fixed_from_int(99));
    assert(provisional->has_credits && provisional->credits == p8_fixed_from_int(200));

    new_game();
    assert(checked_main_economy()->research_points == p8_fixed_from_int(0));
    assert(checked_main_economy()->credits == p8_fixed_from_int(100));
}

/* A built economy has nothing pending; each player action that changes the
 * economy marks it, a refused spend does not, and the economy never clears it. */
static void test_save_pending(void) {
    economy_t *self;

    start_game();
    assert(!economy_new()->save_pending);
    self = main_economy_self();

    self->save_pending = 0;
    assert(economy_spend(self, p8_fixed_from_int(101)) == 0);
    assert(!self->save_pending);
    assert(economy_spend(self, p8_fixed_from_int(0)) == 1);
    assert(self->save_pending);
    assert(economy_spend(self, p8_fixed_from_int(0)) == 1 && self->save_pending);

    self->save_pending = 0;
    assert(economy_spend(self, p8_fixed_from_int(30)) == 1 && self->save_pending);

    self->save_pending = 0;
    economy_advance_clock(self, p8_fixed_from_int(1));
    assert(self->save_pending);

    self->save_pending = 0;
    economy_new_year(self);
    assert(self->save_pending);

    self->save_pending = 0;
    self->research_points = p8_fixed_from_int(50);
    economy_invent(self, tech("t1", 1));
    assert(self->save_pending);

    self->save_pending = 0;
    economy_recompute(self);
    assert(!self->save_pending);
}

/* The game before a paid launch: figures current, then one of the four
 * cases a launch reaches, then the 3 credits the wormhole costs. */
enum { LAUNCH_MID_YEAR, LAUNCH_NEW_YEAR, LAUNCH_FINAL_YEAR, LAUNCH_BANKRUPT };

static void before_launch(int launch_case) {
    economy_t *self;

    if (launch_case == LAUNCH_BANKRUPT) {
        start_game();
    } else {
        built_world();
    }
    self = main_economy_self();
    if (launch_case == LAUNCH_NEW_YEAR || launch_case == LAUNCH_FINAL_YEAR) {
        self->year_ticks = p8_fixed_from_int(15);
    }
    if (launch_case == LAUNCH_FINAL_YEAR) {
        self->year = p8_fixed_from_int(3425);
    }
    if (launch_case == LAUNCH_BANKRUPT) {
        self->credits = p8_fixed_from_int(5);
    }
    economy_recompute(self);
    self->save_pending = 0;
    assert(economy_spend(self, p8_fixed_from_int(3)));
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
        h = item->text != NULL ? fold_string(h, item->text) : fnv(h, 0u);
    }
    return h;
}

/* One tick by economy_tick_clock, or by economy_advance_clock; run in its
 * own child, which sends back the economy, save_pending included, and what
 * the score panel shows. */
static uint32_t tick_in_child(int launch_case, int full_recompute) {
    int fds[2];
    pid_t child;
    int status;
    uint32_t got = 0;

    assert(pipe(fds) == 0);
    fflush(stdout);
    child = fork();
    assert(child >= 0);
    if (child == 0) {
        uint32_t h;

        close(fds[0]);
        before_launch(launch_case);
        if (full_recompute) {
            economy_advance_clock(main_economy_self(), p8_fixed_from_int(1));
        } else {
            economy_tick_clock(main_economy_self(), p8_fixed_from_int(1));
        }
        assert(launch_case != LAUNCH_BANKRUPT || strcmp(end_status_string(), "bankrupt") == 0);
        assert(launch_case != LAUNCH_FINAL_YEAR || strcmp(end_status_string(), "final score") == 0);
        assert(launch_case == LAUNCH_BANKRUPT || launch_case == LAUNCH_FINAL_YEAR || end_status_string() == NULL);
        h = fnv(fold_main_economy(2166136261u), (uint32_t)checked_main_economy()->save_pending);
        h = fold_score_panel_readouts(fold_planets_net_income(h));
        assert(write(fds[1], &h, sizeof h) == (ssize_t)sizeof h);
        _exit(0);
    }
    close(fds[1]);
    assert(read(fds[0], &got, sizeof got) == (ssize_t)sizeof got);
    close(fds[0]);
    assert(waitpid(child, &status, 0) == child);
    assert(WIFEXITED(status) && WEXITSTATUS(status) == 0);
    return got;
}

/* After a spend, a tick without the recompute ends where the full advance
 * ends: mid-year, on the tick that starts a new year, on the one that
 * reaches the final score, and on a spend that leaves the player bankrupt. */
static void test_tick_clock(void) {
    int launch_case;

    for (launch_case = LAUNCH_MID_YEAR; launch_case <= LAUNCH_BANKRUPT; launch_case++) {
        uint32_t tick = tick_in_child(launch_case, 0);
        uint32_t full = tick_in_child(launch_case, 1);
        printf("[tick_clock] case %d: 0x%08X 0x%08X\n", launch_case, (unsigned)tick, (unsigned)full);
        assert(tick == full);
    }
    economy_tick_clock(NULL, p8_fixed_from_int(1));
}

static void must_abort(void (*write)(void *), void *e) {
    pid_t child;
    int status;

    fflush(stdout);
    child = fork();
    assert(child >= 0);
    if (child == 0) {
        assert(freopen("/dev/null", "w", stderr) != NULL);
        write(e);
        _exit(0);
    }
    assert(waitpid(child, &status, 0) == child);
    assert(WIFSIGNALED(status) && WTERMSIG(status) == SIGABRT);
}

static void spend_on_other(void *e) {
    (void)economy_spend(e, p8_fixed_from_int(1));
}

/* The main economy is a struct whose descriptor has no method; the economy
 * takes no key, its functions refuse anything but an economy, and the
 * provisional is never handed to them. */
static void test_economy_is_a_struct(void) {
    const entity_class_t *cls = economy_class();
    economy_t *bare;
    int j;

    assert(economy_ready());
    cart_init();
    assert(main_economy() != NULL && ((const entity_t *)main_economy())->class == main_economy_placeholder_class());
    assert(main_economy_typed() == NULL);

    new_game();
    assert(main_economy() != NULL);
    assert(((const entity_t *)main_economy())->class == cls && main_economy_typed() == (void *)main_economy());
    assert(cls->draw == NULL && cls->draw_overlay == NULL && cls->hit_test == NULL && cls->new_year == NULL && cls->refresh == NULL &&
           cls->has_refresh == NULL && cls->left_press == NULL && cls->accept_drop == NULL);
    assert(cls->drag == NULL && cls->drop == NULL && cls->attach_slipway == NULL && cls->offer_trade == NULL && cls->draw_at == NULL &&
           cls->spawn_particle == NULL);
    for (j = 0; j < STATE_COUNT; j++) {
        assert(cls->states[j] == NULL);
    }

    bare = economy_new();
    assert(bare != NULL && bare->header.class == cls);

    must_abort(spend_on_other, main_ui());

    probe_shutdown();
    assert(main_economy() == NULL && main_economy_typed() == NULL);
}

/* Each scenario runs in a child forked right after cart_load(), as a fresh
 * process would start it: the rnd state is the same for all. */
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
    in_fresh_child(test_construction);
    in_fresh_child(test_advance_clock);
    in_fresh_child(test_new_year);
    in_fresh_child(test_spend);
    in_fresh_child(test_recompute);
    in_fresh_child(test_invent);
    in_fresh_child(test_save_pending);
    in_fresh_child(test_tick_clock);
    in_fresh_child(test_cheat_on_provisional);
    in_fresh_child(test_economy_is_a_struct);

    printf("test_economy: all tests passed\n");
    return 0;
}
