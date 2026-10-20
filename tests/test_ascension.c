/* The ascension, read and written on its struct members and the generated
 * defaults (game/game_data.h), and driven through the real
 * structure_icon_left_press, its class methods, update_entities and
 * draw_layers, in games started directly: no events file. */

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
#include "game/particle.h"
#include "game/ui_zone.h"
#include "game/planet.h"
#include "game/planet_family.h"
#include "game/planet_family_trade.h"
#include "game/probe.h"
#include "game/research.h"
#include "game/structure_icon.h"
#include "game/scroll.h"
#include "engine/entity_class.h"
#include "engine/vec.h"

/* The emitter's particle list, NULL without EMITTER_HAS_PARTICLES. */
static pointer_list_t *particles_of(const emitter_t *emitter) {
    return (emitter->has & EMITTER_HAS_PARTICLES) ? (pointer_list_t *)&emitter->particles : NULL;
}

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

#define BUILT_HASH 0xA5EBBBC3u
#define LIFE_HASH 0x25450044u
#define HEIRS_HASH 0x45C774C4u

/* The tag planet_rebuild_trades's list carried as a tagged list, which the
 * frozen hashes fold: any other value moves every *_HASH. */
#define FROZEN_LIST_TAG 0x9E5C1157u

static p8_vram_t memory;
static p8_graphics_t graphics;
static p8_input_t input;

static const entity_message_t messages[] = {ENTITY_MESSAGE_DRAW, ENTITY_MESSAGE_DRAW_OVERLAY, ENTITY_MESSAGE_HIT_TEST, ENTITY_MESSAGE_NEW_YEAR, ENTITY_MESSAGE_REFRESH};
static const tag_t tags[] = {TAG_ASCENSION, TAG_PLANET, TAG_NODE};

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
static int pointer_list_size(const pointer_list_t *l) {
    return l == NULL ? -1 : l->count;
}

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

/* The entity as an ascension, a protostar or a synthesizer (asserted). */
static planet_t *as_ascension(void *e) {
    const entity_class_t *c = ((const entity_t *)e)->class;
    assert(c == ascension_class() || c == protostar_class() || c == synthesizer_class());
    return e;
}

static uint32_t fold_quarter_turn(uint32_t h, const planet_t *hj) {
    return hj->header.class == protostar_class() ? fold_bool_member(h, hj->emitter.archetype->quarter_turn != 0)
                                                           : fold_fixed_member(h, hj->emitter.archetype->quarter_turn);
}

/* A particle record as probe_spawn_particle returns it: pos, velocity, life. */
static uint32_t fold_particle(uint32_t h, const particle_t *q) {
    assert(q != NULL);
    assert(!q->has_acceleration);
    h = fnv(fnv(h, (uint32_t)q->pos.x), (uint32_t)q->pos.y);
    h = fnv(fnv(h, (uint32_t)q->velocity.x), (uint32_t)q->velocity.y);
    return fnv(h, (uint32_t)q->life);
}

static void check(const char *label, uint32_t got, uint32_t want) {
    printf("[%s] 0x%08Xu\n", label, (unsigned)got);
    fflush(stdout);
    assert(got == want);
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

/* A game started as cart_start_game starts it, with no difficulty picked:
 * a NULL difficulty reads as difficulty 0. */
static void start_game(void) {
    p8_input_init(&input);
    dispatch_context_set_input(&input);
    cart_init();
    cart_start_game(NULL);
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

/* The class holds the laboratory's very members, but its own draw and
 * spawn_particle (spawn_particle checked apart). */
static void assert_structure_members_but_draw(const entity_class_t *c) {
    const entity_class_t *qh = structure_class();
    int j;

    assert(c->draw != NULL && c->draw != qh->draw);
    assert(c->draw_overlay == qh->draw_overlay && c->hit_test == qh->hit_test && c->new_year == qh->new_year && c->refresh == qh->refresh && c->has_refresh == qh->has_refresh &&
           c->left_press == qh->left_press && c->accept_drop == qh->accept_drop);
    assert(c->draw_overlay != NULL && c->hit_test != NULL && c->new_year != NULL && c->refresh != NULL && c->left_press != NULL && c->accept_drop != NULL);
    assert(c->drag == qh->drag && c->drop == qh->drop && c->attach_slipway == qh->attach_slipway && c->offer_trade == qh->offer_trade && c->draw_at == NULL);
    assert(c->drag != NULL && c->drop != NULL && c->attach_slipway != NULL && c->offer_trade != NULL);
    for (j = 0; j < STATE_COUNT; j++) {
        assert(c->states[j] == qh->states[j]);
    }
    assert(c->states[ENTITY_STATE_VANISHING] != NULL && c->states[ENTITY_STATE_SETTLED] != NULL);
    assert(c->states[ENTITY_STATE_NORMAL] == NULL && c->states[ENTITY_STATE_PLACING] == NULL &&
           c->states[ENTITY_STATE_UNFOLDING] == NULL);
}

/* The structure icon whose kind is the given one. */
static void *icon_of(const entity_kind_t *kind) {
    const entity_list_t *hb = entities_by_message(ENTITY_MESSAGE_REFRESH);
    int i;
    for (i = 0; hb != NULL && i < hb->count; i++) {
        void *e = hb->items[i];
        if (class_of(e) == structure_icon_class() && ((const structure_icon_t *)e)->kind == kind) {
            return e;
        }
    }
    return NULL;
}

/* A left press on the icon of the given kind, as the shop does. */
static void *build(const entity_kind_t *kind) {
    void *icon = icon_of(kind);
    down_t made;

    assert(icon != NULL);
    made = class_of(icon)->left_press(icon, (mouse_pos_t){0});
    assert(made.stop && entity_get(made.held) != NULL);
    return entity_get(made.held);
}

static void *build_ascension(void) {
    return build(ascension_kind());
}

/* The protostar and the synthesizer each have their own name: an entity
 * whose name is the very given row is one of that kind. */
static int is_of(const void *e, const data_row_t *px) {
    const planet_t *jo = e != NULL && (class_of(e)->lineage & LINEAGE(LINEAGE_STRUCTURE)) ? e : NULL;
    return jo != NULL && jo->archetype->name == px;
}

/* An ascension as structure_icon_left_press, emitter_init and
 * planet_construct left it: its scalars (the 12 emitter defaults among
 * them), which of its own, the laboratory's and the planet's members are
 * set, its methods (the laboratory chain's very functions but its own draw
 * and the probe's spawn_particle), which default tables it shares with the
 * ascension, laboratory and planet archetypes, and where entity_register
 * filed it -- under the ascension tag, never under the structure tag. */
static uint32_t fold_ascension(uint32_t hash, void *entity) {
    const planet_t *ascension = as_ascension(entity);
    const emitter_t *emitter = &ascension->emitter;
    const planet_t *structure = ascension;
    const planet_t *planet = structure;
    size_t index;

    hash = fold_string_member(hash, planet_id_or_null(planet));
    hash = fold_string_member(hash, entity_state_name(planet->header.state));
    hash = fold_fixed_member(hash, planet->header.state_time);
    hash = fold_string_member(hash, ascension->archetype->required_tech);
    hash = fold_structure(hash, structure);
    hash = fold_fixed_member(hash, structure->archetype->project_index);
    hash = fold_fixed_member(hash, structure->sprite);
    hash = fold_fixed_member(hash, structure->targeting.cost);
    hash = fold_fixed_member(hash, emitter->speed_factor);
    hash = fold_fixed_member(hash, emitter->archetype->lifetime);
    hash = fold_fixed_member(hash, emitter->angle_slices);
    hash = fold_quarter_turn(hash, ascension);
    hash = fold_fixed_member(hash, emitter->archetype->spawn_rate);
    hash = fold_fixed_member(hash, emitter->base_radius);
    hash = fold_fixed_member(hash, emitter->matrix_xx);
    hash = fold_fixed_member(hash, emitter->matrix_xy);
    hash = fold_fixed_member(hash, emitter->matrix_yx);
    hash = fold_fixed_member(hash, emitter->matrix_yy);
    hash = fold_string_member(hash, entity_state_name(structure->targeting.archetype->confirmed_state));
    hash = fold_fixed_member(hash, structure->targeting.archetype->clock_ticks);
    hash = fold_fixed_member(hash, structure->targeting.archetype->attach_distance);
    hash = planet->has_development_tier ? fold_fixed_member(hash, planet->development_tier) : fold_absent(hash);
    hash = fold_is_planet(hash, planet);
    hash = fold_fixed_member(hash, planet->archetype->trade_multiplier);
    hash = fold_fixed_member(hash, planet->archetype->layer);
    hash = fold_fixed_member(hash, planet->header.shake);
    hash = fold_fixed_member(hash, planet->archetype->back_color);
    hash = fold_vec(hash, &planet->archetype->row_step);
    /* The 1s of the ten fixed emitter defaults, project_index, is_structure,
     * cost, clock_ticks, attach_distance, sprite, is_planet, trade_multiplier,
     * layer, shake, back_color: constant by the shape, so BUILT_HASH proves the reads. */
    hash = fnv(hash, (uint32_t)(ascension->archetype->required_tech != NULL));
    hash = fnv(hash, 1u);
    hash = fnv(hash, 1u);
    hash = fnv(hash, 1u);
    hash = fnv(hash, 1u);
    hash = fnv(hash, 1u);
    hash = fnv(hash, (uint32_t)(ascension->emitter.trail_colors != NULL));
    hash = fnv(hash, 1u);
    hash = fnv(hash, 1u);
    hash = fnv(hash, 1u);
    hash = fnv(hash, 1u);
    hash = fnv(hash, 1u);
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
    assert(pointer_list_size(particles_of(emitter)) == 0);
    assert_structure_members_but_draw(class_of(entity));
    assert(class_of(entity)->draw == ascension_class()->draw && planet_is_family(entity));
    assert(class_of(entity)->spawn_particle == probe_spawn_particle);
    assert(planet->header.class->tags == (TAG(TAG_ASCENSION) | TAG(TAG_PLANET) | TAG(TAG_NODE)) && structure->archetype->name == &ascension_name_row && emitter->trail_colors == &ascension_trail_colors_seq);
    assert(planet->header.class->tags != (TAG(TAG_STRUCTURE) | TAG(TAG_PLANET) | TAG(TAG_NODE)));
    assert(structure->archetype->name != &structure_name_row);
    assert(structure->archetype->accepted_resources == &structure_accepted_resources_seq);
    assert(planet->archetype == &ascension_archetype);
    assert(planet->archetype == &ascension_archetype);
    assert(planet->archetype->spacer == &planet_spacer_row && planet->archetype->ring_colors == &planet_ring_colors_seq && planet->archetype->mood_rows == planet_mood_rows_rows &&
           planet->archetype->made_resources == &planet_made_resources_seq);
    assert(planet->has & PLANET_HAS_PRODUCTION);
    assert(research_projects_at(2) != NULL);
    assert(research_project_get(planet->project) == research_projects_at(2));
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
    assert(entity_list_listed(entities_by_tag(TAG_ASCENSION), entity) && !entity_list_listed(entities_by_tag(TAG_STRUCTURE), entity));
    return hash;
}

/* Two ascensions built by the shop's left press on its seventh icon, each
 * with a particle list of its own. */
static void test_built(void) {
    uint32_t hash = 2166136261u;
    void *a;
    void *b;

    start_game();
    a = build_ascension();
    b = build_ascension();
    assert(a != b && is_of(a, &ascension_name_row) && is_of(b, &ascension_name_row));
    assert(particles_of(&as_ascension(a)->emitter) != NULL && particles_of(&as_ascension(a)->emitter)->zone != particles_of(&as_ascension(b)->emitter)->zone);
    hash = fold_ascension(hash, a);
    hash = fold_ascension(hash, b);
    check("built", hash, BUILT_HASH);
}

static p8_fixed_t credits(void) {
    assert(main_economy_typed() != NULL);
    return main_economy_typed()->credits;
}

static void set_credits(int n) {
    main_economy_typed()->credits = p8_fixed_from_int(n);
}

static uint32_t fold_pos_y(uint32_t h, const planet_t *hj) {
    const vec_t *ra = pos_ref(&hj->header);
    return ra == NULL ? fnv(h, 0xDEADu) : fnv(h, (uint32_t)ra->y);
}

/* update_entities once, then what the laboratory's settled update left. */
static uint32_t fold_update_entities(uint32_t h, void *e) {
    const planet_t *hj = as_ascension(e);

    h = fold_pos_y(h, hj);
    update_entities();
    h = fold_fixed_member(h, hj->header.state_time);
    return fold_pos_y(h, hj);
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
 * (is_structure false) give on an ascension. */
static uint32_t fold_economy(uint32_t h, void *e) {
    trade_route_list_t *ib = planet_rebuild_trades(e);
    h = fnv(h, (uint32_t)economy_tiers_compute_tier(e));
    h = fold_rebuild_trades(h, ib);
    h = fnv(fnv(h, 0xB0u), planet_is_colonized(e) ? 1u : 0u);
    h = fnv(h, (uint32_t)economy_score_planet_upkeep(e));
    return as_ascension(e)->has_development_tier ? fold_fixed_member(h, as_ascension(e)->development_tier) : fold_absent(h);
}

/* draw_layers(ENTITY_MESSAGE_DRAW) once: the screen it leaves and what ascension_draw left in the particle list. */
static uint32_t fold_draw(uint32_t h, void *e) {
    draw_layers(ENTITY_MESSAGE_DRAW);
    h = fnv(h, screen_hash());
    return fnv(h, (uint32_t)pointer_list_size(particles_of(&as_ascension(e)->emitter)));
}

/* The drag method (targeting_track) on screen then the drop method
 * (targeting_confirm) with the credits; the economy reads; update_entities
 * in settled (the laboratory's settled update); spawn_particle
 * (probe_spawn_particle, quarter_turn true) and emitter_spawn; draw through
 * draw_layers at development tier 1, then 2 (emitter_update moves and draws
 * the particles); update_entities on an ascension still in placing. */
static void test_life(void) {
    uint32_t hash = 2166136261u;
    void *placed;
    void *idle;
    vec_t mouse_world;
    const vec_t *main_camera_pos;
    int index;

    start_game();

    placed = build_ascension();
    assert(main_camera_typed() != NULL);
    main_camera_pos = pos_ref(&main_camera_typed()->header);
    assert(main_camera_pos != NULL);
    mouse_world = vec_add(*main_camera_pos, vec_xy(28, 104));
    class_of(placed)->drag(placed, mouse_pos(mouse_world, vec_xy(64, 64)));
    assert(pos_copies(pos_ref(&as_ascension(placed)->header), mouse_world));
    hash = fold_vec(hash, map_pos_ref(&as_ascension(placed)->header));
    hash = fnv(hash, target_state(as_ascension(placed)));
    hash = fold_target(hash, as_ascension(placed));
    set_credits(200);
    assert(class_of(placed)->drop == targeting_confirm_method);
    hash = targeting_confirm(placed) ? fnv(fnv(hash, 0xB0u), 1u) : fold_ref(hash, NULL);
    hash = fold_string_member(hash, entity_state_name(as_ascension(placed)->header.state));
    hash = fold_fixed_member(hash, as_ascension(placed)->header.state_time);
    hash = fnv(hash, (uint32_t)credits());
    assert(as_ascension(placed)->header.state == ENTITY_STATE_SETTLED);

    hash = fold_economy(hash, placed);
    for (index = 0; index < 3; index++) {
        hash = fold_update_entities(hash, placed);
    }

    hash = fold_particle(hash, class_of(placed)->spawn_particle(placed));
    emitter_spawn(placed, P8_FIXED_LIT(0x00028000, 2.5));
    hash = fnv(hash, (uint32_t)pointer_list_size(particles_of(&as_ascension(placed)->emitter)));
    assert(pointer_list_size(particles_of(&as_ascension(placed)->emitter)) >= 2);

    hash = fold_draw(hash, placed);
    hash = fold_draw(hash, placed);
    as_ascension(placed)->development_tier = p8_fixed_from_int(2);
    as_ascension(placed)->has_development_tier = 1;
    for (index = 0; index < 4; index++) {
        hash = fold_draw(hash, placed);
    }
    hash = fold_economy(hash, placed);

    idle = build_ascension();
    hash = fold_string_member(hash, entity_state_name(as_ascension(idle)->header.state));
    hash = fold_update_entities(hash, idle);
    check("life", hash, LIFE_HASH);
}

/* A protostar as planet_apply_project builds it (map_pos, pos, planet_id)
 * or a synthesizer by a left press on the shop's sixth icon, each a struct
 * under its own class holding the ascension's draw and spawn_particle. */
static uint32_t fold_heir(uint32_t h, void *e, const entity_class_t *cls) {
    const planet_t *hj;

    assert(e != NULL &&
           ((const entity_t *)e)->class == cls);
    hj = as_ascension(e);
    assert(is_of(e, cls == protostar_class() ? &protostar_name_row : &synthesizer_name_row));
    assert(class_of(e)->draw == ascension_class()->draw);
    assert(class_of(e)->spawn_particle == ascension_class()->spawn_particle);
    assert(pointer_list_size(particles_of(&hj->emitter)) == 0);
    assert(hj->archetype->is_structure == 0);
    h = fold_string_member(h, planet_id_or_null(hj));
    h = fold_string_member(h, entity_state_name(hj->header.state));
    h = fold_structure(h, hj);
    h = fold_string_member(h, hj->archetype->required_tech);
    h = fold_fixed_member(h, hj->targeting.cost);
    h = fold_quarter_turn(h, hj);
    h = fold_fixed_member(h, hj->archetype->project_index);
    h = fold_fixed_member(h, hj->sprite);
    h = fold_fixed_member(h, hj->emitter.speed_factor);
    h = fold_fixed_member(h, hj->emitter.angle_slices);
    h = fold_fixed_member(h, hj->emitter.base_radius);
    h = fnv(h, (uint32_t)entity_list_rank_in(entities_by_tag(TAG_ASCENSION), e));
    return fold_particle(h, class_of(e)->spawn_particle(e));
}

static void test_heirs(void) {
    uint32_t h = 2166136261u;
    planet_keys_t keys = {0};
    void *pm;
    void *mx;

    start_game();
    mx = build(synthesizer_kind());
    keys.map_pos = vec_xy(40, 50);
    keys.has_map_pos = 1;
    keys.pos = vec_xy(7700, 7600);
    keys.has_pos = 1;
    planet_keys_set_id(&keys, "s77");
    pm = protostar_new(&keys);

    assert(strcmp(as_ascension(pm)->archetype->required_tech, "ascension") == 0 && as_ascension(pm)->emitter.archetype->quarter_turn == 0);
    assert(as_ascension(pm)->targeting.cost == p8_fixed_from_int(60));
    assert(strcmp(as_ascension(mx)->archetype->required_tech, "void synthesis") == 0);
    h = fold_heir(h, pm, protostar_class());
    h = fold_heir(h, mx, synthesizer_class());
    check("heirs", h, HEIRS_HASH);
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
    (void)ascension_new(&keys);
}

static int is_an_ascension_struct(const void *e) {
    return e != NULL &&
           ((const entity_t *)e)->class == ascension_class();
}

/* Built by structure_icon_left_press or ascension_new, an ascension is a
 * struct whose class holds the laboratory's very entries but its own draw
 * and the probe's spawn_particle; a construction key other than planet_id
 * stops the run. The planet, laboratory, trading hub and processor classes
 * have no spawn_particle. */
static void test_ascension_is_a_struct(void) {
    const entity_class_t *const no_spawn_particle[] = {planet_class(), structure_class(),
                                                 trading_hub_class(), processor_class()};
    const entity_class_t *cls = ascension_class();
    const entity_class_t *qh = structure_class();
    void *a;
    size_t i;

    start_game();
    a = build_ascension();
    assert(is_an_ascension_struct(a) && cls != qh && entity_list_listed(entities_by_tag(TAG_ASCENSION), a) && !entity_list_listed(entities_by_tag(TAG_STRUCTURE), a));
    assert_structure_members_but_draw(cls);
    assert(cls->spawn_particle == probe_spawn_particle && qh->spawn_particle == NULL);
    for (i = 0; i < sizeof no_spawn_particle / sizeof no_spawn_particle[0]; i++) {
        assert(no_spawn_particle[i]->spawn_particle == NULL);
    }

    must_abort(build_with_unknown_key, NULL);
    {
        planet_t *bare = ascension_new(NULL);
        assert(is_an_ascension_struct(bare) && planet_id_or_null(bare) == NULL && bare->slipways.zone != NULL &&
               pointer_list_size(particles_of(&bare->emitter)) == 0 && entity_list_listed(entities_by_tag(TAG_ASCENSION), bare));
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

/* What an emitter holds in the particle zone, by slot kind. */
typedef struct {
    int particle;
    int particles;
    int particles_help_screen;
    int rows;
} held_t;

static held_t held_now(void) {
    held_t h;
    h.particle = particle_zone_live(PARTICLE_SLOT_PARTICLE);
    h.particles = particle_zone_live(PARTICLE_SLOT_LIST);
    h.particles_help_screen = particle_zone_live(PARTICLE_SLOT_HELP_LIST);
    h.rows = ui_zone_live(SLOT_KIND_ROW);
    return h;
}

/* An ascension that leaves (entity_unregister, then the exit queue's
 * flush) gives back its particles, its particle list and its row, and
 * keeps its struct. */
static void test_leaving_gives_back_its_particles(void) {
    held_t before;
    held_t after;
    planet_t *e;

    start_game();
    cart_flush_exit_queue();
    before = held_now();
    e = as_ascension(build_ascension());
    emitter_spawn(&e->header, p8_fixed_from_int(5));
    class_of(e)->refresh(&e->header);
    assert(particles_of(&e->emitter)->count == 5);
    assert(held_now().particle == before.particle + 5 && held_now().particles == before.particles + 1);
    entity_unregister(&e->header);
    cart_flush_exit_queue();
    after = held_now();
    assert(after.particle == before.particle && after.particles == before.particles && after.rows == before.rows);
    assert(particles_of(&e->emitter) == NULL && e->layout_row_slot == NULL);
}

/* A second structure dropped on the very point of a settled one is
 * refused: no spend, the dragged one killed. */
static void test_drop_on_a_settled_structure_point_is_refused(void) {
    void *settled;
    void *second;
    vec_t mouse_world;
    p8_fixed_t before;

    start_game();
    mouse_world = vec_add(*pos_ref(&main_camera_typed()->header), vec_xy(28, 104));
    set_credits(200);
    settled = build_ascension();
    class_of(settled)->drag(settled, mouse_pos(mouse_world, vec_xy(64, 64)));
    assert(targeting_confirm(settled) == 1);
    assert(as_ascension(settled)->header.state == ENTITY_STATE_SETTLED);

    second = build_ascension();
    class_of(second)->drag(second, mouse_pos(mouse_world, vec_xy(64, 64)));
    before = credits();
    assert(targeting_confirm(second) == 0);
    assert(as_ascension(second)->header.is_dead);
    assert(credits() == before);
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
    in_fresh_child(test_heirs);
    in_fresh_child(test_ascension_is_a_struct);
    in_fresh_child(test_leaving_gives_back_its_particles);
    in_fresh_child(test_drop_on_a_settled_structure_point_is_refused);

    printf("test_ascension: all tests passed\n");
    return 0;
}
