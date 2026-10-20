/* The processor, read and written on its struct members and the generated
 * defaults (game/game_data.h), and driven through the real
 * structure_icon_left_press, its class methods and update_entities, in
 * games started directly: no events file. */

#define _POSIX_C_SOURCE 200809L

#include <assert.h>
#include <signal.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>

#include "p8/sprite_sheet.h"
#include "engine/bootstrap.h"
#include "game/cart.h"
#include "engine/dispatch_context.h"
#include "game/economy_score.h"
#include "game/economy_tiers.h"
#include "engine/entity.h"
#include "engine/entity_registry.h"
#include "p8/fixed.h"
#include "game/main_economy.h"
#include "p8/gfx.h"
#include "p8/input.h"
#include "p8/vram.h"
#include "game/planet.h"
#include "game/planet_family.h"
#include "game/planet_family_trade.h"
#include "game/research.h"
#include "game/structure_icon.h"
#include "engine/entity_id.h"
#include "game/trade.h"
#include "engine/game_zone.h"
#include "engine/entity_class.h"
#include "game/ui_button.h"
#include "engine/vec.h"
#include "heap_allocator.h"

static const vec_t *pos_ref(const entity_t *e) {
    return entity_has_pos(e) ? &e->pos : NULL;
}

static const vec_t *map_pos_ref(const entity_t *e) {
    return entity_has_map_pos(e) ? &e->map_pos : NULL;
}


static int pos_copies(const vec_t *ra, vec_t mp) {
    return ra != NULL && ra->x == mp.x && ra->y == mp.y;
}


/* The tag class instances carried, which the frozen hashes fold: any
 * other value moves every *_HASH. */
#define FROZEN_INSTANCE_TAG 0x9E5C1451u

/* The tag entity structs carried, which the frozen hashes fold: any other
 * value moves every *_HASH. */
#define FROZEN_STRUCT_TAG 0x9E5C57C7u

#define BUILT_HASH 0x2F891586u
#define LIFE_HASH 0x754B6F4Cu

/* The tag planet_rebuild_trades's list carried as a tagged list, which the
 * frozen hashes fold: any other value moves every *_HASH. */
#define FROZEN_LIST_TAG 0x9E5C1157u

static p8_vram_t memory;
static p8_graphics_t graphics;
static p8_input_t input;

static const entity_message_t messages[] = {ENTITY_MESSAGE_DRAW, ENTITY_MESSAGE_DRAW_OVERLAY, ENTITY_MESSAGE_HIT_TEST, ENTITY_MESSAGE_NEW_YEAR, ENTITY_MESSAGE_REFRESH};
static const tag_t tags[] = {TAG_PLANET, TAG_NODE};

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

static uint32_t fold_absent(uint32_t h) {
    return fnv(h, 0u);
}

static uint32_t fold_fixed_member(uint32_t h, p8_fixed_t v) {
    return fnv(fnv(fnv(h, 1u), 0xF1u), (uint32_t)v);
}

static uint32_t fold_bool_member(uint32_t h, int b) {
    return fnv(fnv(fnv(h, 1u), 0xB0u), (uint32_t)(b != 0));
}

/* A NULL string folds as absent. */
static uint32_t fold_string_member(uint32_t h, const char *s) {
    return s == NULL ? fold_absent(h) : fnv_str(fnv(fnv(h, 1u), 0x57u), s);
}

static uint32_t fold_is_planet(uint32_t h, const planet_t *jo) {
    return jo->archetype->is_planet ? fold_fixed_member(h, p8_fixed_from_int(1)) : fold_bool_member(h, 0);
}

static uint32_t fold_vec(uint32_t h, const vec_t *p) {
    if (p == NULL) {
        return fnv(h, 0xDEADu);
    }
    h = fnv(h, (uint32_t)p->x);
    return fnv(h, (uint32_t)p->y);
}

static int is_targeting_track_target(const void *e) {
    return e != NULL &&
           ((const entity_t *)e)->class == targeting_track_target_class();
}

static uint32_t fold_ref(uint32_t h, const void *ref) {
    if (ref == NULL) {
        return fnv(h, 0xDEADu);
    }
    if (entity_list_listed(live_entities(), ref)) {
        return fnv(fnv(h, 0x4Eu), (uint32_t)entity_list_rank_in(live_entities(), ref));
    }
    /* The frozen hashes folded the INSTANCE tag the throwaway had. */
    if (is_targeting_track_target(ref)) {
        return fnv(fnv(h, 0xEEEEu), FROZEN_INSTANCE_TAG);
    }
    return fnv(fnv(h, 0xEEEEu), FROZEN_STRUCT_TAG);
}

/* The item count of a list member, -1 when NULL. */
static int id_list_count(const entity_id_list_t *l) {
    return l == NULL ? -1 : l->count;
}

static uint32_t fold_production(uint32_t h, const planet_t *jo) {
    const production_t *k = planet_production_read(jo);

    if (k == NULL) {
        return fnv(h, 0xA0u);
    }
    h = k->has_resource ? fold_fixed_member(h, k->resource) : fold_absent(h);
    h = fold_fixed_member(h, k->amount);
    return fnv(h, (uint32_t)(k == planet_shared_production()));
}

/* The entity as a planet of the laboratory's lineage (asserted). */
static planet_t *as_structure(void *e) {
    const entity_t *h = e;

    assert(h != NULL && (h->class->lineage & LINEAGE(LINEAGE_STRUCTURE)));
    return e;
}

static uint32_t fold_structure(uint32_t h, const planet_t *qh) {
    return qh->archetype->is_structure ? fold_fixed_member(h, p8_fixed_from_int(1)) : fold_bool_member(h, 0);
}

/* 1 no target, 3 a target: the bytes the frozen hashes fold. */
static uint32_t target_state(const planet_t *qh) {
    return targeting_target(&qh->targeting) == NULL ? 1u : 3u;
}

/* The laboratory's target as its targeting holds it: false or the target. */
static uint32_t fold_target(uint32_t h, const planet_t *qh) {
    switch (target_state(qh)) {
    case 1u:
        return fold_bool_member(h, 0);
    default:
        return fold_ref(fnv(h, 1u), targeting_target(&qh->targeting));
    }
}

static void check(const char *label, uint32_t got, uint32_t want) {
    printf("[%s] 0x%08Xu\n", label, (unsigned)got);
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

/* A game started from the title screen as a click on its second
 * difficulty button starts it: button_left_press runs the on_click that
 * help_screen_construct stamped, cart_request_start_game on that button, and
 * the updates through the loading screen generate the game. The trade
 * income reads the difficulty's trade, which a NULL difficulty lacks. */
static void clicked_game(void) {
    const entity_list_t *hd;
    int buttons = 0;
    int i;

    p8_input_init(&input);
    dispatch_context_set_input(&input);
    cart_init();
    hd = entities_by_message(ENTITY_MESSAGE_HIT_TEST);
    for (i = 0; i < hd->count; i++) {
        if (((const entity_t *)hd->items[i])->class == button_class() && ++buttons == 2) {
            (void)button_left_press((button_t *)hd->items[i]);
            break;
        }
    }
    while (cart_loading()) cart_update60();
    assert(buttons == 2 && economy_score_difficulty() != NULL);
}

static mouse_pos_t mouse_pos(vec_t world, vec_t screen) {
    return (mouse_pos_t){.world = world, .screen = screen, .has = MOUSE_POS_HAS_WORLD | MOUSE_POS_HAS_SCREEN};
}

static vec_t vec_xy(int x, int y) {
    return vec_make(p8_fixed_from_int(x), p8_fixed_from_int(y));
}

static const entity_class_t *class_of(const void *e) {
    return ((const entity_t *)e)->class;
}

/* The members a planet of the laboratory's tree holds: draw, draw_overlay,
 * hit_test, new_year, refresh, left_press, accept_drop, drag, drop,
 * attach_slipway, offer_trade and the vanishing and settled states;
 * spawn_particle aside, no other. */
static void assert_structure_shape(const entity_class_t *c) {
    assert(c->draw != NULL && c->draw_overlay != NULL && c->hit_test != NULL && c->new_year != NULL && c->refresh != NULL && c->left_press != NULL &&
           c->accept_drop != NULL && c->states[ENTITY_STATE_VANISHING] != NULL && c->states[ENTITY_STATE_SETTLED] != NULL);
    assert(c->has_refresh == NULL && c->states[ENTITY_STATE_NORMAL] == NULL &&
           c->states[ENTITY_STATE_PLACING] == NULL && c->states[ENTITY_STATE_UNFOLDING] == NULL);
    assert(c->drag != NULL && c->drop != NULL && c->attach_slipway != NULL && c->offer_trade != NULL && c->draw_at == NULL);
}

/* The class holds the reference class's very members but attach_slipway,
 * offer_trade and spawn_particle; draw and the settled state included when
 * with_draw and with_update_settled are set. */
static void assert_members_of(const entity_class_t *c, const entity_class_t *ref, int with_draw,
                              int with_update_settled) {
    assert(c->draw_overlay == ref->draw_overlay && c->hit_test == ref->hit_test && c->new_year == ref->new_year && c->refresh == ref->refresh && c->has_refresh == ref->has_refresh &&
           c->left_press == ref->left_press && c->accept_drop == ref->accept_drop);
    assert(c->states[ENTITY_STATE_VANISHING] == ref->states[ENTITY_STATE_VANISHING] && c->drag == ref->drag && c->drop == ref->drop);
    assert(!with_draw || c->draw == ref->draw);
    assert(!with_update_settled || c->states[ENTITY_STATE_SETTLED] == ref->states[ENTITY_STATE_SETTLED]);
}

/* The structure icon whose kind is the shop's second, the processor. */
static void *processor_icon(void) {
    const entity_list_t *hb = entities_by_message(ENTITY_MESSAGE_REFRESH);
    int i;
    for (i = 0; hb != NULL && i < hb->count; i++) {
        void *e = hb->items[i];
        if (class_of(e) == structure_icon_class() && ((const structure_icon_t *)e)->kind == processor_kind()) {
            return e;
        }
    }
    return NULL;
}

/* A left press on the processor's icon, as the shop does. */
static void *build_processor(void) {
    void *icon = processor_icon();
    down_t made;

    assert(icon != NULL);
    made = class_of(icon)->left_press(icon, (mouse_pos_t){0});
    assert(made.stop && entity_get(made.held) != NULL);
    return entity_get(made.held);
}

/* A processor is the only entity whose name is the processor's own row. */
static int is_processor(const void *e) {
    const planet_t *jo = e != NULL && (class_of(e)->lineage & LINEAGE(LINEAGE_STRUCTURE)) ? e : NULL;
    return jo != NULL && jo->archetype->name == &processor_name_row;
}

/* A processor as structure_icon_left_press and planet_construct left it:
 * its scalars (trade_multiplier, a planet field, overridden by the
 * processor's archetype), which of the laboratory's and the planet's
 * members are set, its methods (the laboratory chain's very functions, the
 * settled update included), which default tables it shares with the
 * processor, laboratory and planet archetypes, and where entity_register
 * filed it -- never under the structure tag. */
static uint32_t fold_processor(uint32_t hash, void *entity) {
    const planet_t *structure = as_structure(entity);
    const planet_t *planet = structure;
    size_t index;

    hash = fold_string_member(hash, planet_id_or_null(planet));
    hash = fold_string_member(hash, entity_state_name(planet->header.state));
    hash = fold_fixed_member(hash, planet->header.state_time);
    hash = fold_structure(hash, structure);
    hash = fold_fixed_member(hash, structure->archetype->project_index);
    hash = fold_fixed_member(hash, structure->sprite);
    hash = fold_fixed_member(hash, structure->targeting.cost);
    hash = fold_fixed_member(hash, planet->archetype->trade_multiplier);
    hash = fold_string_member(hash, entity_state_name(structure->targeting.archetype->confirmed_state));
    hash = fold_fixed_member(hash, structure->targeting.archetype->clock_ticks);
    hash = fold_fixed_member(hash, structure->targeting.archetype->attach_distance);
    hash = planet->has_development_tier ? fold_fixed_member(hash, planet->development_tier) : fold_absent(hash);
    hash = fold_is_planet(hash, planet);
    hash = fold_fixed_member(hash, planet->archetype->layer);
    hash = fold_fixed_member(hash, planet->header.shake);
    hash = fold_fixed_member(hash, planet->archetype->back_color);
    hash = fold_vec(hash, &planet->archetype->row_step);
    /* The 1s of project_index, is_structure, cost, clock_ticks, attach_distance,
     * sprite, is_planet, trade_multiplier, layer, shake and back_color: constant
     * by the struct's shape, kept so that BUILT_HASH still proves the typed reads. */
    hash = fnv(hash, 1u);
    hash = fnv(hash, (uint32_t)(structure->archetype->name != NULL));
    hash = fnv(hash, (uint32_t)(structure->targeting.archetype->confirmed_state != ENTITY_STATE_NONE));
    hash = fnv(hash, 1u);
    hash = fnv(hash, 1u);
    hash = fnv(hash, 1u);
    hash = fnv(hash, 1u);
    hash = fnv(hash, 1u);
    hash = fnv(hash, (uint32_t)(structure->archetype->accepted_resources != NULL));
    hash = fnv(hash, targeting_target(&structure->targeting) != NULL ? 1u : 0u);
    hash = fnv(hash, 1u);
    hash = fnv(hash, 1u);
    hash = fnv(hash, 1u);
    hash = fnv(hash, 1u);
    hash = fnv(hash, 1u);
    hash = fnv(hash, 1u);
    hash = fnv(hash, (uint32_t)(planet->archetype->ring_colors != NULL));
    hash = fnv(hash, (uint32_t)(planet->archetype->mood_rows != NULL));
    hash = fnv(hash, (uint32_t)(planet->archetype->made_resources != NULL));
    hash = fnv(hash, (uint32_t)(planet_production_read(planet) != NULL));
    hash = fnv(hash, 1u);
    hash = fnv(hash, 1u);
    hash = fnv(hash, (uint32_t)(planet->archetype->spacer != NULL));
    hash = fnv(hash, (uint32_t)(planet_id_or_null(planet) != NULL));
    hash = fnv(hash, (uint32_t)(map_pos_ref(&planet->header) != NULL));
    hash = fnv(hash, (uint32_t)(pos_ref(&planet->header) != NULL));
    hash = fnv(hash, (uint32_t)(planet->slipways.zone != NULL));
    hash = fnv(hash, (uint32_t)(research_project_get(planet->project) != NULL));
    hash = fnv(hash, (uint32_t)(planet->description != NULL));
    hash = fnv(hash, (uint32_t)((planet->has & PLANET_HAS_NEEDS) != 0));
    hash = fnv(hash, (uint32_t)(planet->planet_type != NULL));
    hash = fnv(hash, (uint32_t)(planet->header.fold_anchor != 0));
    hash = fnv(hash, (uint32_t)(planet->layout_row != NULL));
    hash = fnv(hash, (uint32_t)(entity_get(planet->targeted_by) != NULL));
    hash = fnv(hash, (uint32_t)planet->has_production_bonus);
    hash = fnv(hash, (uint32_t)(planet->header.is_dead != 0));
    hash = fnv(hash, (uint32_t)planet->has_net_income);
    hash = fnv(hash, (uint32_t)((planet->has & PLANET_HAS_INCOMING_ROUTES) != 0));
    hash = fnv(hash, (uint32_t)((planet->has & PLANET_HAS_OUTGOING_ROUTES) != 0));
    hash = fnv(hash, (uint32_t)planet->has_development_tier);
    hash = fnv(hash, (uint32_t)(planet->header.is_hovered != 0));
    assert_structure_shape(class_of(entity));
    assert_members_of(class_of(entity), structure_class(), 1, 1);
    assert(class_of(entity)->attach_slipway == structure_class()->attach_slipway && class_of(entity)->offer_trade == structure_class()->offer_trade &&
           class_of(entity)->spawn_particle == NULL && planet_is_family(entity));
    assert(planet->header.class->tags == (TAG(TAG_PLANET) | TAG(TAG_NODE)));
    assert(structure->archetype->name == &processor_name_row && structure->archetype->name != &structure_name_row);
    assert(structure->archetype->accepted_resources == &structure_accepted_resources_seq);
    assert(planet->archetype == &processor_archetype);
    assert(planet->archetype == &processor_archetype);
    assert(planet->archetype->spacer == &planet_spacer_row && planet->archetype->ring_colors == &planet_ring_colors_seq && planet->archetype->mood_rows == planet_mood_rows_rows &&
           planet->archetype->made_resources == &planet_made_resources_seq);
    assert(planet->has & PLANET_HAS_PRODUCTION);
    assert(research_projects_at(4) != NULL);
    assert(research_project_get(planet->project) == research_projects_at(4));
    hash = fold_production(hash, planet);
    hash = fnv(hash, (uint32_t)id_list_count(&planet->slipways));
    hash = fnv(hash, (uint32_t)((planet->has & PLANET_HAS_NEEDS) ? planet->needs.count : -1));
    hash = fnv(hash, (uint32_t)entity_list_rank_in(live_entities(), entity));
    for (index = 0; index < sizeof messages / sizeof messages[0]; index++) {
        hash = fnv(hash, (uint32_t)entity_list_rank_in(entities_by_message(messages[index]), entity));
    }
    for (index = 0; index < sizeof tags / sizeof tags[0]; index++) {
        hash = fnv(hash, (uint32_t)entity_list_rank_in(entities_by_tag(tags[index]), entity));
    }
    assert(!entity_list_listed(entities_by_tag(TAG_STRUCTURE), entity));
    return hash;
}

/* Two processors built by the shop's left press on its second icon. */
static void test_built(void) {
    uint32_t hash = 2166136261u;
    void *a;
    void *b;

    start_game();
    a = build_processor();
    b = build_processor();
    assert(a != b && is_processor(a) && is_processor(b));
    hash = fold_processor(hash, a);
    hash = fold_processor(hash, b);
    check("built", hash, BUILT_HASH);
}

static p8_fixed_t credits(void) {
    assert(main_economy_typed() != NULL);
    return main_economy_typed()->credits;
}

static void set_credits(int n) {
    main_economy_typed()->credits = p8_fixed_from_int(n);
}

static uint32_t fold_pos_y(uint32_t h, const planet_t *qh) {
    const vec_t *ra = pos_ref(&qh->header);
    return ra == NULL ? fnv(h, 0xDEADu) : fnv(h, (uint32_t)ra->y);
}

/* update_entities once, then what the laboratory's settled update left. */
static uint32_t fold_update_entities(uint32_t h, void *e) {
    const planet_t *qh = as_structure(e);

    h = fold_pos_y(h, qh);
    update_entities();
    h = fold_fixed_member(h, qh->header.state_time);
    return fold_pos_y(h, qh);
}

/* planet_rebuild_trades's list, folded as fold_ref folded the tagged list
 * it was: never a live entity, so the tag branch. */
static uint32_t fold_rebuild_trades(uint32_t h, const trade_route_list_t *ib) {
    if (ib == NULL) {
        return fnv(h, 0xDEADu);
    }
    return fnv(fnv(h, 0xEEEEu), FROZEN_LIST_TAG);
}

/* What economy_tiers_compute_tier (no trading hub flag, is_planet false),
 * planet_rebuild_trades, planet_is_colonized and economy_score_planet_upkeep
 * (is_structure false) give on a processor. */
static uint32_t fold_economy(uint32_t h, void *e) {
    trade_route_list_t *ib = planet_rebuild_trades(e);
    h = fnv(h, (uint32_t)economy_tiers_compute_tier(e));
    h = fold_rebuild_trades(h, ib);
    h = fnv(fnv(h, 0xB0u), planet_is_colonized(e) ? 1u : 0u);
    h = fnv(h, (uint32_t)economy_score_planet_upkeep(e));
    return as_structure(e)->has_development_tier ? fold_fixed_member(h, as_structure(e)->development_tier) : fold_absent(h);
}

/* The first planet's outgoing routes holding one export to the second, as
 * planet_rebuild_trades leaves them: a trade whose planets start at the
 * first and end at the second. */
static void export_to(void *pt, void *oz) {
    trade_keys_t keys = {0};
    planet_t *exporter = pt;
    trade_route_t t;

    entity_id_list_init(&keys.planets, &heap_allocator);
    entity_id_list_add(&keys.planets, entity_id_of(pt));
    entity_id_list_add(&keys.planets, entity_id_of(oz));
    t = trade_new(&keys);
    assert(planet_is_family(pt));
    trade_route_list_init(&exporter->outgoing_routes, &heap_allocator);
    trade_route_list_add(&exporter->outgoing_routes, &t);
    exporter->has |= PLANET_HAS_OUTGOING_ROUTES;
}

/* The trade income of a processor and of a bare planet, at the same tier,
 * each exporting to the same bare planet (is_planet true): only the
 * exporter's trade multiplier tells them apart. */
static uint32_t fold_trade_income(uint32_t h, void *processor) {
    planet_t *bare = planet_new(NULL);
    planet_t *oz = planet_new(NULL);
    const planet_t *from = as_structure(processor);
    p8_fixed_t processor_trade_income;
    p8_fixed_t bare_trade_income;

    assert(oz->archetype->is_planet);
    bare->development_tier = from->has_development_tier ? from->development_tier : 0;
    bare->has_development_tier = from->has_development_tier;
    export_to(processor, oz);
    export_to(bare, oz);
    processor_trade_income = economy_score_trade_income(processor);
    bare_trade_income = economy_score_trade_income(&bare->header);
    printf("[trade_income] processor %d, bare planet %d\n", (int)processor_trade_income, (int)bare_trade_income);
    assert(processor_trade_income != bare_trade_income && 2 * processor_trade_income == bare_trade_income);
    h = fnv(h, (uint32_t)processor_trade_income);
    return fnv(h, (uint32_t)bare_trade_income);
}

/* The drag method (targeting_track) in the panel then the drop method
 * (targeting_confirm) with the credits; update_entities in settled (the
 * laboratory's settled update); the economy reads on it; the trade income
 * with an export to a planet whose is_planet is true; update_entities on a
 * processor still in placing. */
static void test_life(void) {
    uint32_t hash = 2166136261u;
    void *placed;
    void *idle;
    vec_t mp;
    int index;

    clicked_game();

    placed = build_processor();
    mp = vec_xy(7680, 7552);
    class_of(placed)->drag(placed, mouse_pos(mp, vec_xy(64, 64)));
    assert(pos_copies(pos_ref(&as_structure(placed)->header), mp));
    hash = fold_vec(hash, map_pos_ref(&as_structure(placed)->header));
    hash = fnv(hash, target_state(as_structure(placed)));
    hash = fold_target(hash, as_structure(placed));
    set_credits(100);
    assert(class_of(placed)->drop == targeting_confirm_method);
    hash = targeting_confirm(placed) ? fnv(fnv(hash, 0xB0u), 1u) : fold_ref(hash, NULL);
    hash = fold_string_member(hash, entity_state_name(as_structure(placed)->header.state));
    hash = fold_fixed_member(hash, as_structure(placed)->header.state_time);
    hash = fnv(hash, (uint32_t)credits());
    assert(as_structure(placed)->header.state == ENTITY_STATE_SETTLED);

    hash = fold_economy(hash, placed);
    for (index = 0; index < 3; index++) {
        hash = fold_update_entities(hash, placed);
    }
    hash = fold_economy(hash, placed);
    hash = fold_trade_income(hash, placed);

    idle = build_processor();
    hash = fold_string_member(hash, entity_state_name(as_structure(idle)->header.state));
    hash = fold_update_entities(hash, idle);
    check("life", hash, LIFE_HASH);
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

static void build_with_unknown_key(void *e) {
    planet_keys_t keys = {0};
    (void)e;
    keys.map_pos = vec_xy(1, 1);
    keys.has_map_pos = 1;
    (void)processor_new(&keys);
}

static int is_a_processor_struct(const void *e) {
    return e != NULL &&
           ((const entity_t *)e)->class == processor_class();
}

/* Built by structure_icon_left_press or processor_new, a processor is a
 * struct whose class is its own but holds the laboratory's very entries; a
 * construction key other than planet_id stops the run. */
static void test_processor_is_a_struct(void) {
    const entity_class_t *cls = processor_class();
    const entity_class_t *qh = structure_class();
    void *a;

    start_game();
    a = build_processor();
    assert(is_a_processor_struct(a) && cls != qh && entity_list_listed(entities_by_tag(TAG_PLANET), a) && !entity_list_listed(entities_by_tag(TAG_STRUCTURE), a));
    assert_structure_shape(cls);
    assert_members_of(cls, qh, 1, 1);
    assert(cls->attach_slipway == qh->attach_slipway && cls->offer_trade == qh->offer_trade && cls->spawn_particle == qh->spawn_particle);

    must_abort(build_with_unknown_key, NULL);
    {
        planet_t *bare = processor_new(NULL);
        assert(is_a_processor_struct(bare) && planet_id_or_null(bare) == NULL && bare->slipways.zone != NULL &&
               entity_list_listed(entities_by_tag(TAG_PLANET), bare));
    }
}

/* The galaxy depends on the rnd state: run each scenario in a child forked
 * right after cart_load(), as a fresh process would start it. */
static pid_t spawn(void (*test)(void)) {
    pid_t child;

    fflush(stdout);
    child = fork();
    assert(child >= 0);
    if (child == 0) {
        test();
        fflush(stdout);
        _exit(0);
    }
    return child;
}

static void reap(pid_t child) {
    int status;
    assert(waitpid(child, &status, 0) == child);
    assert(WIFEXITED(status) && WEXITSTATUS(status) == 0);
}

static void in_fresh_child(void (*test)(void)) {
    reap(spawn(test));
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
    in_fresh_child(test_built);
    in_fresh_child(test_life);
    in_fresh_child(test_processor_is_a_struct);

    printf("test_processor: all tests passed\n");
    return 0;
}
