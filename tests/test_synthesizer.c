/* The synthesizer, read and written on its struct members and the
 * generated defaults (game/game_data.h), built as the shop builds it (a
 * left press on its sixth icon, with a planet_id) and driven through the
 * real class methods, update_entities, draw_layers and a slipway's drop, in
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
#include "game/slipway.h"
#include "game/research.h"
#include "game/structure_icon.h"
#include "game/scroll.h"
#include "engine/entity_class.h"
#include "engine/vec.h"
#include "engine/game_zone.h"
#include "heap_allocator.h"

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


#define BUILT_HASH 0xAC802F72u
#define LIFE_HASH 0x6DB5E41Fu
#define SYNTHESIS_HASH 0x2DBF1611u

/* The tag planet_rebuild_trades's list carried as a tagged list, which the
 * frozen hashes fold: any other value moves every *_HASH. */
#define FROZEN_LIST_TAG 0x9E5C1157u

/* The tag entity structs carried, which the frozen hashes fold: any other
 * value moves every *_HASH. */
#define FROZEN_STRUCT_TAG 0x9E5C57C7u

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

static uint32_t fold_ref(uint32_t h, const void *ref) {
    if (ref == NULL) {
        return fnv(h, 0xDEADu);
    }
    if (entity_list_listed(live_entities(), ref)) {
        return fnv(fnv(h, 0x4Eu), (uint32_t)entity_list_rank_in(live_entities(), ref));
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

/* The entity as an ascension, a protostar or a synthesizer (asserted). */
static planet_t *as_ascension(void *e) {
    const entity_class_t *c = ((const entity_t *)e)->class;
    assert(c == ascension_class() || c == protostar_class() || c == synthesizer_class());
    return e;
}

static uint32_t fold_quarter_turn(uint32_t h, const planet_t *hj) {
    return fold_fixed_member(h, hj->emitter.archetype->quarter_turn);
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

static void *build_synthesizer(void) {
    return build(synthesizer_kind());
}

/* The synthesizer has its own name: an entity whose name is the very row
 * of the synthesizer archetype is a synthesizer. */
static int is_synthesizer(const void *e) {
    const planet_t *jo = e != NULL && (class_of(e)->lineage & LINEAGE(LINEAGE_STRUCTURE)) ? e : NULL;
    return jo != NULL && jo->archetype->name == &synthesizer_name_row;
}

/* The synthesizable resources of a synthesizer; NULL on any other struct. */
static const int_seq_t *synthesizable_resources_of(const void *e) {
    return class_of(e) == synthesizer_class() ? ((const planet_t *)e)->synthesizable_resources : NULL;
}

/* The entry count of the synthesizable resources, -1 when absent. */
static int synthesizable_resources_size(const void *e) {
    const int_seq_t *ht = synthesizable_resources_of(e);
    return ht == NULL ? -1 : ht->count;
}

static int synthesizable_resources_is_the_class_one(const void *e) {
    return synthesizable_resources_of(e) == &synthesizer_synthesizable_resources_seq;
}

/* A synthesizer as structure_icon_left_press, emitter_init and
 * planet_construct left it: its scalars (the 12 emitter defaults among
 * them), which of its own, the ascension's, the laboratory's and the
 * planet's members are set, its methods (the ascension's very functions
 * but its own attach_slipway), which default tables it shares with the
 * synthesizer, laboratory and planet archetypes, and where entity_register
 * filed it -- under the planet, node and targeting tags, never under the
 * structure or ascension tag. */
static uint32_t fold_synthesizer(uint32_t hash, void *entity) {
    const entity_class_t *ascension_cls = ascension_class();
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
    hash = fnv(hash, (uint32_t)(synthesizable_resources_of(entity) != NULL));
    hash = fnv(hash, (uint32_t)synthesizable_resources_size(entity));
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
    assert_members_of(class_of(entity), ascension_cls, 1, 1);
    assert(class_of(entity)->attach_slipway != ascension_cls->attach_slipway && class_of(entity)->attach_slipway != planet_class()->attach_slipway);
    assert(class_of(entity)->offer_trade == ascension_cls->offer_trade && class_of(entity)->spawn_particle == ascension_cls->spawn_particle && class_of(entity)->spawn_particle != NULL &&
           planet_is_family(entity));
    assert(planet->header.class->tags == (TAG(TAG_PLANET) | TAG(TAG_NODE)) && structure->archetype->name == &synthesizer_name_row && synthesizable_resources_of(entity) == &synthesizer_synthesizable_resources_seq &&
           emitter->trail_colors == &synthesizer_trail_colors_seq);
    assert(planet->header.class->tags != (TAG(TAG_ASCENSION) | TAG(TAG_PLANET) | TAG(TAG_NODE)));
    assert(emitter->trail_colors != &ascension_trail_colors_seq);
    assert(structure->archetype->accepted_resources == &structure_accepted_resources_seq);
    assert(planet->archetype == &synthesizer_archetype);
    assert(planet->archetype == &synthesizer_archetype);
    assert(planet->archetype->spacer == &planet_spacer_row && planet->archetype->ring_colors == &planet_ring_colors_seq && planet->archetype->mood_rows == planet_mood_rows_rows &&
           planet->archetype->made_resources == &planet_made_resources_seq);
    assert(planet->has & PLANET_HAS_PRODUCTION);
    assert(research_projects_at(5) != NULL);
    assert(research_project_get(planet->project) == research_projects_at(5));
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
    return hash;
}

/* Two synthesizers built by the shop's left press on its sixth icon, each
 * with a particle list of its own. */
static void test_built(void) {
    uint32_t hash = 2166136261u;
    void *a;
    void *b;

    start_game();
    a = build_synthesizer();
    b = build_synthesizer();
    assert(a != b && is_synthesizer(a) && is_synthesizer(b));
    assert(particles_of(&as_ascension(a)->emitter) != NULL && particles_of(&as_ascension(a)->emitter)->zone != particles_of(&as_ascension(b)->emitter)->zone);
    hash = fold_synthesizer(hash, a);
    hash = fold_synthesizer(hash, b);
    check("built", hash, BUILT_HASH);
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

/* What economy_tiers_compute_tier, planet_rebuild_trades,
 * planet_is_colonized and economy_score_planet_upkeep (is_structure false)
 * give on a synthesizer. */
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

/* Placed on screen by the drag method (targeting_track) and the drop method
 * (targeting_confirm); update_entities on one still in placing (no
 * placing method: nothing moves), then in settled (the laboratory's
 * settled update); spawn_particle (probe_spawn_particle, quarter_turn true)
 * and emitter_spawn; draw through draw_layers at development tier 1, then
 * 2 (emitter_update moves and draws the particles); the economy reads. */
static void test_life(void) {
    uint32_t hash = 2166136261u;
    void *placed;
    void *idle;
    vec_t mouse_world;
    const vec_t *main_camera_pos;
    int index;

    start_game();
    idle = build_synthesizer();
    hash = fold_string_member(hash, entity_state_name(as_ascension(idle)->header.state));
    hash = fold_update_entities(hash, idle);

    placed = build_synthesizer();
    assert(main_camera_typed() != NULL);
    main_camera_pos = pos_ref(&main_camera_typed()->header);
    assert(main_camera_pos != NULL);
    mouse_world = vec_add(*main_camera_pos, vec_xy(28, 104));
    class_of(placed)->drag(placed, mouse_pos(mouse_world, vec_xy(64, 64)));
    assert(pos_copies(pos_ref(&as_ascension(placed)->header), mouse_world));
    set_credits(200);
    assert(class_of(placed)->drop == targeting_confirm_method);
    hash = targeting_confirm(placed) ? fnv(fnv(hash, 0xB0u), 1u) : fold_ref(hash, NULL);
    hash = fold_string_member(hash, entity_state_name(as_ascension(placed)->header.state));
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
    check("life", hash, LIFE_HASH);
}

/* A slipway from the given start to the synthesizer, joined by its real
 * drop, which charges the player, then runs attach_slipway on both ends. */
static void *join(void *qu, void *synthesizer) {
    slipway_keys_t slipway_keys = {0};
    void *jc;

    slipway_keys.start = entity_id_link(qu);
    jc = slipway_new(&slipway_keys);
    assert(jc != NULL);
    set_credits(999);
    class_of(jc)->drop(jc, synthesizer);
    assert(entity_id_list_has(&as_ascension(synthesizer)->slipways, entity_id_of(jc)));
    return jc;
}

/* synthesizer_attach_slipway reads the other end's needs: one row per
 * resource, its satisfied field its count of suppliers. */
static void give_row(void *e, int qe) {
    need_t *row = need_new(game_zone(), (resource_t){.kind = RESOURCE_FIXED, .number = p8_fixed_from_int(qe)});
    pointer_list_t *jy = malloc(sizeof *jy);

    row->has_satisfied = 1;
    row->satisfied = p8_fixed_from_int(0);
    pointer_list_init(jy, &heap_allocator);
    pointer_list_add(jy, row);
    as_ascension(e)->needs = *jy;
    as_ascension(e)->has |= PLANET_HAS_NEEDS;
}

static uint32_t fold_attach_slipway(uint32_t h, void *e) {
    const planet_t *jo = as_ascension(e);

    h = fnv(h, (uint32_t)synthesizable_resources_is_the_class_one(e));
    h = fnv(h, (uint32_t)synthesizable_resources_size(e));
    h = fold_production(h, jo);
    h = fold_fixed_member(h, jo->header.shake);
    h = (jo->header.fold_anchor != 0) ? fold_bool_member(h, jo->header.fold_anchor) : fold_absent(h);
    return fnv(h, (uint32_t)id_list_count(&jo->slipways));
}

/* synthesizer_attach_slipway on a slipway from an ascension whose needs
 * lack a synthesizable resource: nothing but planet_attach_slipway; then on
 * one whose unsupplied row is resource 20, a synthesizable one: the
 * production's resource takes it and the synthesizable resources become a
 * fresh empty table, the class one untouched; a later slipway finds them
 * empty and leaves them as they are. A second synthesizer still starts from
 * the class table. */
static void test_synthesis(void) {
    uint32_t h = 2166136261u;
    void *home;
    void *s;
    void *other;
    const void *fresh;
    const production_t *kw;

    start_game();
    home = build(ascension_kind());
    s = build_synthesizer();
    assert(synthesizable_resources_is_the_class_one(s) && synthesizable_resources_size(s) == 4);

    give_row(home, 7);
    (void)join(home, s);
    assert(synthesizable_resources_is_the_class_one(s));
    h = fold_attach_slipway(h, s);
    h = fold_attach_slipway(h, home);

    give_row(home, 20);
    (void)join(home, s);
    assert(!synthesizable_resources_is_the_class_one(s) && synthesizable_resources_size(s) == 0);
    kw = planet_production_read(as_ascension(s));
    assert(kw->has_resource && kw->resource == p8_fixed_from_int(20));
    assert(synthesizer_synthesizable_resources_seq.count == 4);
    assert(!planet_shared_production()->has_resource || planet_shared_production()->resource != p8_fixed_from_int(20));
    h = fold_attach_slipway(h, s);
    fresh = synthesizable_resources_of(s);

    give_row(home, 21);
    (void)join(home, s);
    assert(synthesizable_resources_of(s) == fresh && synthesizable_resources_size(s) == 0);
    kw = planet_production_read(as_ascension(s));
    assert(kw->has_resource && kw->resource == p8_fixed_from_int(20));
    h = fold_attach_slipway(h, s);

    other = build_synthesizer();
    assert(synthesizable_resources_is_the_class_one(other) && synthesizable_resources_size(other) == 4);
    h = fold_attach_slipway(h, other);
    check("synthesis", h, SYNTHESIS_HASH);
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
    (void)synthesizer_new(&keys);
}

static int is_a_synthesizer_struct(const void *e) {
    return e != NULL &&
           ((const entity_t *)e)->class == synthesizer_class();
}

/* Built by structure_icon_left_press or synthesizer_new, a synthesizer is
 * a struct whose class holds the ascension's very entries but its own
 * attach_slipway; a construction key other than planet_id stops the run. */
static void test_synthesizer_is_a_struct(void) {
    const entity_class_t *cls = synthesizer_class();
    const entity_class_t *hj = ascension_class();
    void *a;

    start_game();
    a = build_synthesizer();
    assert(is_a_synthesizer_struct(a) && cls != hj && entity_list_listed(entities_by_tag(TAG_PLANET), a) && !entity_list_listed(entities_by_tag(TAG_ASCENSION), a));
    assert(cls->attach_slipway != hj->attach_slipway && cls->attach_slipway != planet_class()->attach_slipway);
    assert_structure_shape(cls);
    assert_members_of(cls, hj, 1, 1);
    assert(cls->offer_trade == hj->offer_trade && cls->spawn_particle == hj->spawn_particle);
    assert(synthesizer_ready());
    assert(ascension_ready());

    must_abort(build_with_unknown_key, NULL);
    {
        planet_t *bare = synthesizer_new(NULL);
        assert(is_a_synthesizer_struct(bare) && planet_id_or_null(bare) == NULL && bare->slipways.zone != NULL &&
               (bare->has & PLANET_HAS_PRODUCTION) && pointer_list_size(particles_of(&bare->emitter)) == 0 &&
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

/* A synthesizer that leaves (entity_unregister, then the exit queue's
 * flush) gives back its particles, its particle list and its row, and keeps
 * its struct. */
static void test_leaving_gives_back_its_particles(void) {
    held_t before;
    held_t after;
    planet_t *e;

    start_game();
    cart_flush_exit_queue();
    before = held_now();
    e = as_ascension(build_synthesizer());
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
    in_fresh_child(test_synthesis);
    in_fresh_child(test_synthesizer_is_a_struct);
    in_fresh_child(test_leaving_gives_back_its_particles);

    printf("test_synthesizer: all tests passed\n");
    return 0;
}
