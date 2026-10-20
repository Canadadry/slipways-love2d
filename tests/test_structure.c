/* The laboratory, read on its struct members and the generated defaults
 * (game/game_data.h), and driven through the real
 * structure_icon_left_press, its class methods and update_entities. Its
 * descendants are read the same way. Games are started directly: no events
 * file. */

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
#include "engine/dispatch_context.h"
#include "game/economy_score.h"
#include "engine/entity.h"
#include "engine/entity_registry.h"
#include "p8/fixed.h"
#include "game/main_economy.h"
#include "p8/gfx.h"
#include "game/help.h"
#include "p8/input.h"
#include "p8/vram.h"
#include "game/planet.h"
#include "game/planet_display.h"
#include "game/planet_family.h"
#include "game/planet_family_trade.h"
#include "game/probe.h"
#include "game/research.h"
#include "game/structure_icon.h"
#include "game/scroll.h"
#include "game/slipway.h"
#include "engine/entity_class.h"
#include "engine/vec.h"
#include "game/wormhole.h"

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

#define BUILT_HASH 0x96428F3Bu
#define LIFE_HASH 0x37B44597u
#define DESCENDANTS_HASH 0x129E3D90u

static p8_vram_t memory;
static p8_graphics_t graphics;
static p8_input_t input;

static const entity_message_t messages[] = {ENTITY_MESSAGE_DRAW, ENTITY_MESSAGE_DRAW_OVERLAY, ENTITY_MESSAGE_HIT_TEST, ENTITY_MESSAGE_NEW_YEAR, ENTITY_MESSAGE_REFRESH};
static const tag_t tags[] = {TAG_STRUCTURE, TAG_PLANET, TAG_NODE};

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

/* The entity as a planet of the laboratory's lineage (asserted). */
static planet_t *as_structure(void *e) {
    const entity_t *h = e;

    assert(h != NULL && (h->class->lineage & LINEAGE(LINEAGE_STRUCTURE)));
    return e;
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

static uint32_t fold_structure(uint32_t h, const planet_t *qh) {
    return qh->archetype->is_structure ? fold_fixed_member(h, p8_fixed_from_int(1)) : fold_bool_member(h, 0);
}

/* 0 absent, 1 false, 2 true. */
static uint32_t is_dead_state(const planet_t *jo) {
    return jo->header.is_dead ? 2u : 0u;
}

/* 1 no target, 3 a target: the bytes the frozen hashes fold. */
static uint32_t target_state(const planet_t *qh) {
    return targeting_target(&qh->targeting) == NULL ? 1u : 3u;
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
    assert(is_targeting_track_target(ref) && "fold_ref: no test folds an entity outside the live entities that is not the throwaway target: to fold one, fold 0xEEEE then the frozen struct tag 0x9E5C57C7, as test_infraspace_gate.c does");
    return fnv(fnv(h, 0xEEEEu), FROZEN_INSTANCE_TAG);
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
    cart_flush_exit_queue();
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

/* The structure icon whose kind is the shop's first, the laboratory. */
static void *structure_icon(void) {
    const entity_list_t *hb = entities_by_message(ENTITY_MESSAGE_REFRESH);
    int i;
    for (i = 0; hb != NULL && i < hb->count; i++) {
        void *e = hb->items[i];
        if (class_of(e) == structure_icon_class() && ((const structure_icon_t *)e)->kind == structure_kind()) {
            return e;
        }
    }
    return NULL;
}

/* A left press on the laboratory's icon, as the shop does. */
static void *build_lab(void) {
    void *icon = structure_icon();
    down_t made;

    assert(icon != NULL);
    made = class_of(icon)->left_press(icon, (mouse_pos_t){0});
    assert(made.stop && entity_get(made.held) != NULL);
    return entity_get(made.held);
}

/* A laboratory as structure_icon_left_press and planet_construct left it:
 * its scalars, which of the laboratory's and the planet's members are set,
 * its methods (the very functions of the class chain), which default
 * tables it shares with the laboratory and the planet archetypes, and
 * where entity_register filed it. */
static uint32_t fold_lab(uint32_t hash, void *entity) {
    const planet_t *structure = as_structure(entity);
    const planet_t *planet = structure;
    size_t index;

    hash = fold_string_member(hash, planet_id_or_null(planet));
    hash = fold_string_member(hash, entity_state_name(planet->header.state));
    hash = fold_fixed_member(hash, planet->header.state_time);
    hash = fold_fixed_member(hash, structure->archetype->project_index);
    hash = fold_string_member(hash, entity_state_name(structure->targeting.archetype->confirmed_state));
    hash = fold_structure(hash, structure);
    hash = fold_fixed_member(hash, structure->targeting.cost);
    hash = fold_fixed_member(hash, structure->targeting.archetype->clock_ticks);
    hash = fold_fixed_member(hash, structure->targeting.archetype->attach_distance);
    hash = planet->has_development_tier ? fold_fixed_member(hash, planet->development_tier) : fold_absent(hash);
    hash = fold_fixed_member(hash, structure->sprite);
    hash = fold_is_planet(hash, planet);
    hash = fold_fixed_member(hash, planet->archetype->trade_multiplier);
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
    assert(planet->header.class->tags == (TAG(TAG_STRUCTURE) | TAG(TAG_PLANET) | TAG(TAG_NODE)) && structure->archetype->name == &structure_name_row && structure->archetype->accepted_resources == &structure_accepted_resources_seq);
    assert(planet->archetype == &structure_archetype);
    assert(planet->archetype == &structure_archetype);
    assert(planet->archetype->spacer == &planet_spacer_row && planet->archetype->ring_colors == &planet_ring_colors_seq && planet->archetype->mood_rows == planet_mood_rows_rows &&
           planet->archetype->made_resources == &planet_made_resources_seq);
    assert(planet->has & PLANET_HAS_PRODUCTION);
    assert(research_projects_at(1) != NULL);
    assert(research_project_get(planet->project) == research_projects_at(1));
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
    return hash;
}

/* Two laboratories built by the shop's left press on its first icon. */
static void test_built(void) {
    uint32_t hash = 2166136261u;
    void *a;
    void *b;

    start_game();
    a = build_lab();
    b = build_lab();
    assert(a != b);
    hash = fold_lab(hash, a);
    hash = fold_lab(hash, b);
    check("built", hash, BUILT_HASH);
}

static p8_fixed_t credits(void) {
    assert(main_economy_typed() != NULL);
    return main_economy_typed()->credits;
}

static void set_credits(int n) {
    main_economy_typed()->credits = p8_fixed_from_int(n);
}

/* What targeting_track left on a laboratory: its pos (a copy of the mouse
 * position passed), its map_pos, and what its target is. */
static uint32_t fold_held(uint32_t h, void *e, vec_t mp) {
    const planet_t *qh = as_structure(e);

    assert(pos_copies(pos_ref(&qh->header), mp));
    h = fold_vec(h, map_pos_ref(&qh->header));
    h = fnv(h, target_state(qh));
    return fold_target(h, qh);
}

static uint32_t fold_pos_y(uint32_t h, void *e) {
    const vec_t *ra = pos_ref(&as_structure(e)->header);
    assert(ra != NULL);
    return fnv(h, (uint32_t)ra->y);
}

/* The drag method (targeting_track) at an empty point, in the panel and
 * out of it, then next to a planet; the drop method (targeting_confirm)
 * with and without the credits; update_entities on a laboratory in
 * settled, which bobs, and on one still in placing, which does not. */
static void test_life(void) {
    uint32_t hash = 2166136261u;
    void *out_lab;
    void *in_lab;
    void *refused_lab;
    void *planet;
    vec_t mouse_world;
    const vec_t *planet_map_pos;
    const vec_t *pos_before;
    p8_fixed_t before;
    p8_fixed_t in_y;
    int index;

    start_game();

    out_lab = build_lab();
    mouse_world = vec_xy(7680, 7680);
    class_of(out_lab)->drag(out_lab, mouse_pos(mouse_world, vec_xy(0, 0)));
    hash = fold_held(hash, out_lab, mouse_world);
    assert(target_state(as_structure(out_lab)) == 3 && !entity_list_listed(live_entities(), targeting_target(&as_structure(out_lab)->targeting)));

    planet = entities_by_tag(TAG_PLANET)->items[0];
    assert(planet_is_family(planet));
    planet_map_pos = map_pos_ref(&((const planet_t *)planet)->header);
    assert(planet_map_pos != NULL);
    mouse_world = vec_add(vec_mul(*planet_map_pos, p8_fixed_from_int(128)), vec_xy(6, 0));
    class_of(out_lab)->drag(out_lab, mouse_pos(mouse_world, vec_xy(64, 64)));
    hash = fold_held(hash, out_lab, mouse_world);
    assert(targeting_target(&as_structure(out_lab)->targeting) == planet &&
           entity_get(((const planet_t *)planet)->targeted_by) == out_lab);

    refused_lab = build_lab();
    mouse_world = vec_xy(7680, 7680);
    class_of(refused_lab)->drag(refused_lab, mouse_pos(mouse_world, vec_xy(64, 64)));
    hash = fold_held(hash, refused_lab, mouse_world);
    assert(target_state(as_structure(refused_lab)) == 1);
    set_credits(3);
    assert(class_of(refused_lab)->drop == targeting_confirm_method);
    hash = targeting_confirm(refused_lab) ? fnv(fnv(hash, 0xB0u), 1u) : fold_ref(hash, NULL);
    hash = fold_string_member(hash, entity_state_name(as_structure(refused_lab)->header.state));
    hash = fold_fixed_member(hash, as_structure(refused_lab)->header.state_time);
    hash = fnv(hash, is_dead_state(as_structure(refused_lab)));
    hash = fnv(hash, (uint32_t)credits());
    assert(is_dead_state(as_structure(refused_lab)) == 2 && credits() == p8_fixed_from_int(3));

    in_lab = build_lab();
    mouse_world = vec_xy(7680, 7552);
    class_of(in_lab)->drag(in_lab, mouse_pos(mouse_world, vec_xy(64, 64)));
    set_credits(100);
    assert(class_of(in_lab)->drop == targeting_confirm_method);
    hash = targeting_confirm(in_lab) ? fnv(fnv(hash, 0xB0u), 1u) : fold_ref(hash, NULL);
    hash = fold_string_member(hash, entity_state_name(as_structure(in_lab)->header.state));
    hash = fold_fixed_member(hash, as_structure(in_lab)->header.state_time);
    hash = fnv(hash, is_dead_state(as_structure(in_lab)));
    hash = fnv(hash, (uint32_t)credits());
    assert(as_structure(in_lab)->header.state == ENTITY_STATE_SETTLED && credits() == p8_fixed_from_int(100 - 15));
    in_y = pos_ref(&as_structure(in_lab)->header)->y;

    pos_before = pos_ref(&as_structure(out_lab)->header);
    before = pos_before->y;
    for (index = 0; index < 4; index++) {
        hash = fold_pos_y(hash, in_lab);
        update_entities();
        hash = fold_fixed_member(hash, as_structure(in_lab)->header.state_time);
        assert(pos_ref(&as_structure(out_lab)->header) == pos_before && pos_before->y == before);
    }
    hash = fold_pos_y(hash, in_lab);
    assert(pos_ref(&as_structure(in_lab)->header)->y != in_y);
    hash = fnv(hash, (uint32_t)entity_list_listed(live_entities(), refused_lab));
    check("life", hash, LIFE_HASH);
}

enum { TRADING_HUB, PROCESSOR, ASCENSION, PROTOSTAR, SYNTHESIZER, SLIPGATE, INFRASPACE_GATE, DESCENDANTS };

static int descendant_of(const void *e) {
    if (e != NULL &&
        ((const entity_t *)e)->class == trading_hub_class()) {
        return TRADING_HUB;
    }
    if (e != NULL &&
        ((const entity_t *)e)->class == processor_class()) {
        return PROCESSOR;
    }
    if (e != NULL &&
        ((const entity_t *)e)->class == ascension_class()) {
        return ASCENSION;
    }
    if (e != NULL &&
        ((const entity_t *)e)->class == protostar_class()) {
        return PROTOSTAR;
    }
    if (e != NULL &&
        ((const entity_t *)e)->class == synthesizer_class()) {
        return SYNTHESIZER;
    }
    if (e != NULL &&
        ((const entity_t *)e)->class == slipgate_class()) {
        return SLIPGATE;
    }
    if (e != NULL &&
        ((const entity_t *)e)->class == infraspace_gate_class()) {
        return INFRASPACE_GATE;
    }
    return -1;
}

/* What a descendant inherits from the laboratory: confirmed_state,
 * clock_ticks, attach_distance, is_planet, whether its tags, name and
 * accepted resources are the laboratory's own, drag, draw_overlay and drop
 * (the laboratory's functions), and its settled update called once (the
 * y of its pos before, after, then put back). */
static uint32_t fold_descendant(uint32_t h, void *d) {
    planet_t *qh = as_structure(d);
    planet_t *nl = class_of(d) == trading_hub_class() ? (planet_t *)d : NULL;
    p8_fixed_t qw = nl != NULL ? nl->palette_block : 0;

    h = fold_string_member(h, entity_state_name(qh->targeting.archetype->confirmed_state));
    h = fold_fixed_member(h, qh->targeting.archetype->clock_ticks);
    h = fold_fixed_member(h, qh->targeting.archetype->attach_distance);
    h = fold_is_planet(h, qh);
    h = qh->has_development_tier ? fold_fixed_member(h, qh->development_tier) : fold_absent(h);
    h = fnv(h, (uint32_t)(qh->header.class->tags == (TAG(TAG_STRUCTURE) | TAG(TAG_PLANET) | TAG(TAG_NODE))));
    h = fnv(h, (uint32_t)(qh->archetype->name == &structure_name_row));
    h = fnv(h, (uint32_t)(qh->archetype->accepted_resources == &structure_accepted_resources_seq));
    assert(class_of(d)->drag == structure_class()->drag && class_of(d)->drop == structure_class()->drop);
    assert(class_of(d)->draw_overlay == structure_class()->draw_overlay);
    if (entity_has_pos(&qh->header)) {
        vec_t *v = &qh->header.pos;
        p8_fixed_t y = v->y;
        h = fnv(fnv(h, 1u), (uint32_t)y);
        class_of(d)->states[ENTITY_STATE_SETTLED](d, p8_fixed_from_int(50));
        h = fnv(h, (uint32_t)v->y);
        v->y = y;
    } else {
        class_of(d)->states[ENTITY_STATE_SETTLED](d, p8_fixed_from_int(50));
        h = fnv(h, 0u);
    }
    h = nl != NULL ? fold_fixed_member(h, nl->palette_block) : fold_absent(h);
    if (nl != NULL) {
        nl->palette_block = qw;
    }
    return h;
}

/* The shop's left press on the icon of the kind, as the player buys it,
 * then dragged by its drag method to the middle of the screen. */
static void *bought(const entity_kind_t *kind) {
    const entity_list_t *hb = entities_by_message(ENTITY_MESSAGE_REFRESH);
    const camera_t *mo = main_camera_typed();
    const vec_t *main_camera_pos;
    int i;

    assert(mo != NULL && pos_ref(&mo->header) != NULL);
    main_camera_pos = pos_ref(&mo->header);
    for (i = 0; hb != NULL && i < hb->count; i++) {
        void *icon = hb->items[i];
        if (class_of(icon) == structure_icon_class() && ((const structure_icon_t *)icon)->kind == kind) {
            down_t made = class_of(icon)->left_press(icon, (mouse_pos_t){0});
            assert(made.stop && entity_get(made.held) != NULL);
            class_of(entity_get(made.held))->drag(entity_get(made.held), mouse_pos(vec_add(*main_camera_pos, vec_xy(80, 60)), vec_xy(64, 64)));
            return entity_get(made.held);
        }
    }
    assert(0 && "no icon for that kind");
    return NULL;
}

/* The research project whose class name is the given one. */
static research_project_t *class_name_project(const char *cw) {
    int k;

    for (k = 1;; k++) {
        research_project_t *hp = research_projects_at(k);
        assert(hp != NULL);
        if (hp->class_name != NULL && strcmp(hp->class_name, cw) == 0) {
            return hp;
        }
    }
}

/* Each descendant by the road a game takes to it: the six of the shop
 * bought from their icons, the protostar turned from a generated planet by
 * planet_apply_project on the collapse project (the newest live entity). */
static void test_descendants(void) {
    static const struct {
        int descendant;
        const entity_kind_t *(*kind)(void);
    } shop[] = {{PROCESSOR, processor_kind}, {SLIPGATE, slipgate_kind}, {INFRASPACE_GATE, infraspace_gate_kind},
                {TRADING_HUB, trading_hub_kind}, {SYNTHESIZER, synthesizer_kind}, {ASCENSION, ascension_kind}};
    void *made[DESCENDANTS] = {NULL};
    uint32_t h = 2166136261u;
    void *p;
    size_t i;

    start_game();
    for (i = 0; i < sizeof shop / sizeof shop[0]; i++) {
        made[shop[i].descendant] = bought(shop[i].kind());
    }
    p = entities_by_tag(TAG_PLANET)->items[0];
    planet_roll_type(p);
    planet_apply_project(p, class_name_project("protostar"));
    made[PROTOSTAR] = live_entities()->items[live_entities()->count - 1];
    for (i = 0; i < DESCENDANTS; i++) {
        assert(made[i] != NULL && descendant_of(made[i]) == (int)i && entity_list_listed(live_entities(), made[i]));
        h = fold_descendant(fnv(h, (uint32_t)i), made[i]);
    }
    check("descendants", h, DESCENDANTS_HASH);
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
    (void)structure_new(&keys);
}

static int is_a_structure_struct(const void *e) {
    return e != NULL &&
           ((const entity_t *)e)->class == structure_class();
}

/* Built by structure_icon_left_press or structure_new, a laboratory is a
 * struct whose class holds the planet's very methods and its own drag,
 * draw_overlay, drop and settled update; a construction key other than
 * planet_id stops the run. Every descendant is a struct under its own
 * class. */
static void test_structure_is_a_struct(void) {
    const entity_kind_t *const descendant_kinds[] = {trading_hub_kind(), processor_kind(), ascension_kind(), protostar_kind(),
                                                      synthesizer_kind(), slipgate_kind(), infraspace_gate_kind()};
    const entity_class_t *lab_class = structure_class();
    void *built;
    size_t index;

    start_game();
    built = build_lab();
    assert(is_a_structure_struct(built) && entity_list_listed(entities_by_tag(TAG_STRUCTURE), built));
    assert(economy_score_planet_upkeep(built) == p8_fixed_from_int(entities_by_tag(TAG_STRUCTURE)->count));
    assert(lab_class->attach_slipway == planet_class()->attach_slipway && lab_class->offer_trade == planet_class()->offer_trade);
    assert_structure_shape(lab_class);
    assert(lab_class->hit_test == planet_class()->hit_test && lab_class->draw == planet_class()->draw &&
           lab_class->left_press == planet_class()->left_press && lab_class->refresh == planet_class()->refresh &&
           lab_class->accept_drop == planet_class()->accept_drop && lab_class->new_year == planet_class()->new_year &&
           lab_class->states[ENTITY_STATE_VANISHING] == planet_class()->states[ENTITY_STATE_VANISHING]);
    assert(lab_class->draw_overlay != planet_class()->draw_overlay && lab_class->states[ENTITY_STATE_SETTLED] != NULL);
    assert(lab_class->drag != planet_class()->drag && lab_class->drop != planet_class()->drop && lab_class->spawn_particle == NULL);

    must_abort(build_with_unknown_key, NULL);
    {
        planet_t *bare = structure_new(NULL);
        assert(is_a_structure_struct(bare) && planet_id_or_null(bare) == NULL && bare->slipways.zone != NULL && entity_list_listed(entities_by_tag(TAG_STRUCTURE), bare));
    }
    for (index = 0; index < sizeof descendant_kinds / sizeof descendant_kinds[0]; index++) {
        planet_keys_t keys = {0};
        void *d;
        planet_keys_set_id(&keys, "s99");
        d = descendant_kinds[index]->new_entity(&keys);
        if (descendant_kinds[index] == trading_hub_kind()) {
            assert(d != NULL &&
                   ((const entity_t *)d)->class == trading_hub_class());
            continue;
        }
        if (descendant_kinds[index] == processor_kind()) {
            assert(d != NULL &&
                   ((const entity_t *)d)->class == processor_class());
            continue;
        }
        if (descendant_kinds[index] == ascension_kind()) {
            assert(d != NULL &&
                   ((const entity_t *)d)->class == ascension_class());
            continue;
        }
        if (descendant_kinds[index] == protostar_kind()) {
            assert(d != NULL &&
                   ((const entity_t *)d)->class == protostar_class());
            continue;
        }
        if (descendant_kinds[index] == synthesizer_kind()) {
            assert(d != NULL &&
                   ((const entity_t *)d)->class == synthesizer_class());
            continue;
        }
        if (descendant_kinds[index] == slipgate_kind()) {
            assert(d != NULL &&
                   ((const entity_t *)d)->class == slipgate_class());
            continue;
        }
        assert(d != NULL &&
               ((const entity_t *)d)->class == infraspace_gate_class());
    }
}

static unsigned lineage_of(const entity_t *e) {
    return e->class->lineage;
}

/* The eight kinds of the laboratory's lineage are of the planet's and the
 * structure's, the trading hub alone of its own too; the burst and the
 * planet of the planet's alone, the probe and the help screen of the
 * probe's alone, the other classes of none. */
static void test_lineage_of_each_class(void) {
    const unsigned jo = LINEAGE(LINEAGE_PLANET);
    const unsigned qh = jo | LINEAGE(LINEAGE_STRUCTURE);
    const entity_kind_t *const lineage[] = {structure_kind(), trading_hub_kind(), processor_kind(), ascension_kind(),
                                                  protostar_kind(), synthesizer_kind(), slipgate_kind(), infraspace_gate_kind()};
    planet_keys_t keys = {0};
    const entity_t *e;
    size_t i;

    planet_keys_set_id(&keys, "s99");
    start_game();
    for (i = 0; i < sizeof lineage / sizeof lineage[0]; i++) {
        e = lineage[i]->new_entity(&keys);
        assert(lineage_of(e) == (lineage[i] == trading_hub_kind() ? (qh | LINEAGE(LINEAGE_TRADING_HUB)) : qh));
    }
    assert(lineage_of(burst_kind()->new_entity(&keys)) == jo);
    assert(lineage_of(&planet_new(&keys)->header) == jo);
    assert(lineage_of(&probe_new(NULL)->header) == LINEAGE(LINEAGE_PROBE));
    assert(lineage_of(&help_screen_new()->header) == LINEAGE(LINEAGE_PROBE));
    assert(lineage_of(&camera_new()->header) == 0 && lineage_of(&background_new()->header) == 0);
    assert(lineage_of(&wormhole_new()->header) == 0 && lineage_of(&slipway_new(NULL)->header) == 0);
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
    in_fresh_child(test_descendants);
    in_fresh_child(test_structure_is_a_struct);
    in_fresh_child(test_lineage_of_each_class);

    printf("test_structure: all tests passed\n");
    return 0;
}
