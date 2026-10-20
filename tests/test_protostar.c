/* The protostar, read and written on its struct members and the generated
 * defaults (game/game_data.h), built as planet_apply_project builds it (its
 * kind on map_pos, pos and planet_id) and driven through the real class
 * methods, update_entities and draw_layers, in games started directly: no
 * events file. */

#define _POSIX_C_SOURCE 200809L

#include <assert.h>
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
#include "p8/gfx.h"
#include "p8/input.h"
#include "p8/vram.h"
#include "game/particle.h"
#include "game/ui_zone.h"
#include "game/planet.h"
#include "game/planet_family.h"
#include "game/planet_family_trade.h"
#include "game/research.h"
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



#define BUILT_HASH 0x3C0EE132u
#define LIFE_HASH 0x833D22EDu

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

/* The entity as an ascension, a protostar or a synthesizer (asserted). */
static planet_t *as_ascension(void *e) {
    const entity_class_t *c = ((const entity_t *)e)->class;
    assert(c == ascension_class() || c == protostar_class() || c == synthesizer_class());
    return e;
}

static uint32_t fold_quarter_turn(uint32_t h, const planet_t *hj) {
    return fold_bool_member(h, hj->emitter.archetype->quarter_turn != 0);
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

/* protostar_new as planet_apply_project calls it: map_pos, pos and planet_id, each only when given. */
static void *build_protostar(const vec_t *p, const vec_t *ra, const char *id) {
    planet_keys_t keys = {0};
    void *made;

    if (p != NULL) {
        keys.map_pos = *p;
        keys.has_map_pos = 1;
    }
    if (ra != NULL) {
        keys.pos = *ra;
        keys.has_pos = 1;
    }
    if (id != NULL) {
        planet_keys_set_id(&keys, id);
    }
    made = protostar_new(&keys);
    assert(made != NULL);
    return made;
}

/* The protostar has its own name: an entity whose name is the very row of
 * the protostar archetype is a protostar. */
static int is_protostar(const void *e) {
    const planet_t *jo = e != NULL && (class_of(e)->lineage & LINEAGE(LINEAGE_STRUCTURE)) ? e : NULL;
    return jo != NULL && jo->archetype->name == &protostar_name_row;
}

static int is_burst(const void *e) {
    return e != NULL && class_of(e) == burst_class() && ((const planet_t *)e)->archetype->class_name != NULL &&
           strcmp(((const planet_t *)e)->archetype->class_name, "burst") == 0;
}

/* The burst that protostar_construct spawned: registered just before the
 * protostar, at its pos plus the spawn offset; its particle list and the
 * camera's shake as burst_construct left them. */
static uint32_t fold_spawned(uint32_t h, void *e) {
    const entity_list_t *hv = live_entities();
    int at = entity_list_rank_in(hv, e);
    const planet_t *g;
    const planet_t *pm;
    const vec_t *ra;
    const vec_t *ng;
    const vec_t *gra;

    assert(at >= 1 && class_of(e) == protostar_class());
    pm = e;
    ra = pos_ref(&pm->header);
    ng = &pm->archetype->spawn_offset;
    assert(is_burst(hv->items[at - 1]));
    g = (const planet_t *)hv->items[at - 1];
    gra = pos_ref(&g->header);
    assert(gra != NULL && ng != NULL);
    if (ra != NULL) {
        assert(gra->x == p8_fixed_add(ra->x, ng->x) && gra->y == p8_fixed_add(ra->y, ng->y));
    } else {
        assert(gra->x == ng->x && gra->y == ng->y);
    }
    assert(gra != ra && gra != ng);
    h = fold_vec(h, pos_ref(&g->header));
    h = fnv(h, (uint32_t)pointer_list_size(particles_of(&g->emitter)));
    assert(main_camera_typed() != NULL);
    return fold_fixed_member(h, main_camera_typed()->header.shake);
}

/* A protostar as protostar_new, protostar_construct, emitter_init and
 * planet_construct left it: its scalars, which of its own, the
 * ascension's, the laboratory's and the planet's members are set, its
 * methods (the ascension's very functions), which default tables it shares
 * with the protostar, laboratory and planet archetypes, where
 * entity_register filed it -- under the planet, node and targeting tags,
 * never under the structure or ascension tag -- and the burst it spawned. */
static uint32_t fold_protostar(uint32_t hash, void *entity) {
    const planet_t *ascension = as_ascension(entity);
    const planet_t *protostar;
    const emitter_t *emitter = &ascension->emitter;
    const planet_t *structure = ascension;
    const planet_t *planet = structure;
    size_t index;

    assert(class_of(entity) == protostar_class());
    protostar = entity;
    hash = fold_string_member(hash, planet_id_or_null(planet));
    hash = fold_string_member(hash, entity_state_name(planet->header.state));
    hash = fold_fixed_member(hash, planet->header.state_time);
    hash = fold_string_member(hash, protostar->archetype->class_name);
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
    hash = fold_vec(hash, &protostar->archetype->spawn_offset);
    hash = fold_vec(hash, &planet->archetype->row_step);
    hash = fold_vec(hash, pos_ref(&planet->header));
    hash = fold_vec(hash, map_pos_ref(&planet->header));
    /* The 1s of the ten fixed emitter defaults, project_index, is_structure,
     * cost, clock_ticks, attach_distance, sprite, is_planet, trade_multiplier,
     * layer, shake, back_color: constant by the shape, so BUILT_HASH proves the reads. */
    hash = fnv(hash, 1u);
    hash = fnv(hash, (uint32_t)(protostar->archetype->class_name != NULL));
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
    hash = fnv(hash, (uint32_t)(particles_of(emitter) != NULL));
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
    assert_structure_shape(class_of(entity));
    assert_members_of(class_of(entity), structure_class(), 0, 1);
    assert(class_of(entity)->attach_slipway == structure_class()->attach_slipway && class_of(entity)->offer_trade == structure_class()->offer_trade &&
           planet_is_family(entity));
    assert(class_of(entity)->spawn_particle == ascension_class()->spawn_particle && class_of(entity)->spawn_particle != NULL);
    assert(class_of(entity)->draw == ascension_class()->draw);
    assert(protostar->archetype->spawn_offset.x == p8_fixed_from_int(1) && protostar->archetype->spawn_offset.y == 0);
    assert(planet->header.class->tags == (TAG(TAG_PLANET) | TAG(TAG_NODE)) && structure->archetype->name == &protostar_name_row && emitter->trail_colors == &protostar_trail_colors_seq);
    assert(planet->header.class->tags != (TAG(TAG_ASCENSION) | TAG(TAG_PLANET) | TAG(TAG_NODE)));
    assert(emitter->trail_colors != &ascension_trail_colors_seq);
    assert(structure->archetype->accepted_resources == &structure_accepted_resources_seq);
    assert(planet->archetype == &protostar_archetype);
    assert(planet->archetype == &protostar_archetype);
    assert(planet->archetype->spacer == &planet_spacer_row && planet->archetype->ring_colors == &planet_ring_colors_seq && planet->archetype->mood_rows == planet_mood_rows_rows &&
           planet->archetype->made_resources == &planet_made_resources_seq);
    assert(planet->has & PLANET_HAS_PRODUCTION);
    assert(research_projects_at(3) != NULL);
    assert(research_project_get(planet->project) == research_projects_at(3));
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
    assert(entity_list_listed(entities_by_tag(TAG_PLANET), entity) && !entity_list_listed(entities_by_tag(TAG_STRUCTURE), entity) && !entity_list_listed(entities_by_tag(TAG_ASCENSION), entity));
    return fold_spawned(hash, entity);
}

/* Two protostars built as planet_apply_project builds them, each with a
 * particle list of its own and a burst of its own. */
static void test_built(void) {
    uint32_t hash = 2166136261u;
    vec_t pa = vec_xy(40, 50);
    vec_t pos_a = vec_xy(7700, 7600);
    vec_t pb = vec_xy(-30, 12);
    vec_t pos_b = vec_xy(7650, 7720);
    void *a;
    void *b;

    start_game();
    a = build_protostar(&pa, &pos_a, "s77");
    b = build_protostar(&pb, &pos_b, "s78");
    assert(a != b && is_protostar(a) && is_protostar(b));
    assert(particles_of(&as_ascension(a)->emitter) != NULL && particles_of(&as_ascension(a)->emitter)->zone != particles_of(&as_ascension(b)->emitter)->zone);
    hash = fold_protostar(hash, a);
    hash = fold_protostar(hash, b);
    check("built", hash, BUILT_HASH);
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

/* planet_rebuild_trades's list, folded as the other planet tests' fold_ref
 * folds the tagged list it was: never a live entity, so the tag branch. */
static uint32_t fold_rebuild_trades(uint32_t h, const trade_route_list_t *ib) {
    if (ib == NULL) {
        return fnv(h, 0xDEADu);
    }
    return fnv(fnv(h, 0xEEEEu), FROZEN_LIST_TAG);
}

/* What economy_tiers_compute_tier, planet_rebuild_trades,
 * planet_is_colonized and economy_score_planet_upkeep (is_structure false)
 * give on a protostar. */
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

/* update_entities in settled (the laboratory's settled update);
 * spawn_particle (probe_spawn_particle, quarter_turn false: no rotation)
 * and emitter_spawn; draw through draw_layers at development tier 1, then
 * 2 (emitter_update moves and draws the particles); the economy reads; a
 * protostar built on a planet_id alone, its burst at the spawn offset. */
static void test_life(void) {
    uint32_t hash = 2166136261u;
    const vec_t *main_camera_pos;
    vec_t map_pos = vec_xy(64, 64);
    vec_t ra;
    void *placed;
    void *bare;
    int index;

    start_game();
    assert(main_camera_typed() != NULL);
    main_camera_pos = pos_ref(&main_camera_typed()->header);
    assert(main_camera_pos != NULL);
    ra = vec_add(*main_camera_pos, vec_xy(28, 104));
    placed = build_protostar(&map_pos, &ra, "s90");
    hash = fold_spawned(hash, placed);
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

    bare = build_protostar(NULL, NULL, "s91");
    assert(pos_ref(&as_ascension(bare)->header) == NULL && map_pos_ref(&as_ascension(bare)->header) == NULL && is_protostar(bare));
    hash = fold_string_member(hash, planet_id_or_null(as_ascension(bare)));
    hash = fold_spawned(hash, bare);
    hash = fold_update_entities(hash, bare);
    check("life", hash, LIFE_HASH);
}

static int is_a_protostar_struct(const void *e) {
    return e != NULL &&
           ((const entity_t *)e)->class == protostar_class();
}

/* Built by its kind or protostar_new, a protostar is a struct whose class
 * holds the ascension's very entries. */
static void test_protostar_is_a_struct(void) {
    const entity_class_t *cls = protostar_class();
    const entity_class_t *hj = ascension_class();
    vec_t p = vec_xy(40, 50);
    vec_t ra = vec_xy(7700, 7600);
    void *a;

    start_game();
    a = build_protostar(&p, &ra, "s77");
    assert(is_a_protostar_struct(a) && cls != hj && entity_list_listed(entities_by_tag(TAG_PLANET), a) && !entity_list_listed(entities_by_tag(TAG_ASCENSION), a));
    assert_structure_shape(cls);
    assert_members_of(cls, hj, 1, 1);
    assert(cls->attach_slipway == hj->attach_slipway && cls->offer_trade == hj->offer_trade && cls->spawn_particle == hj->spawn_particle);
    assert(protostar_ready());
    {
        planet_t *bare = protostar_new(NULL);
        assert(is_a_protostar_struct(bare) && planet_id_or_null(bare) == NULL && pos_ref(&bare->header) == NULL &&
               bare->slipways.zone != NULL && pointer_list_size(particles_of(&bare->emitter)) == 0 &&
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

/* A protostar that leaves (entity_unregister, then the exit queue's
 * flush) gives back its particles and its particle list, not those of the
 * burst it spawned. */
static void test_leaving_gives_back_its_particles(void) {
    held_t built;
    held_t after;
    vec_t at = vec_xy(64, 64);
    planet_t *e;

    start_game();
    cart_flush_exit_queue();
    e = as_ascension(build_protostar(&at, &at, "s90"));
    emitter_spawn(&e->header, p8_fixed_from_int(5));
    class_of(e)->refresh(&e->header);
    assert(particles_of(&e->emitter)->count == 5);
    built = held_now();
    entity_unregister(&e->header);
    cart_flush_exit_queue();
    after = held_now();
    assert(after.particle == built.particle - 5 && after.particles == built.particles - 1);
    assert(particles_of(&e->emitter) == NULL && e->layout_row_slot == NULL);
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
    in_fresh_child(test_protostar_is_a_struct);
    in_fresh_child(test_leaving_gives_back_its_particles);

    printf("test_protostar: all tests passed\n");
    return 0;
}
