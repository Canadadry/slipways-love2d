#include "game/save_load.h"

#include "engine/entity.h"
#include "engine/entity_id.h"
#include "engine/entity_id_list.h"
#include "engine/entity_registry.h"
#include "engine/game_zone.h"
#include "game/economy_tiers.h"
#include "game/galaxy.h"
#include "game/grab_arena.h"
#include "game/main_economy.h"
#include "game/planet.h"
#include "game/planet_family.h"
#include "game/planet_family_trade.h"
#include "game/probe.h"
#include "game/research.h"
#include "game/slipway.h"
#include "game/structure_icon.h"
#include "game/trade.h"
#include "p8/fixed.h"

#include <assert.h>
#include <stddef.h>

static vec_t vec_of(save_vec_t saved) {
    return vec_make(saved.x, saved.y);
}

static int has(const save_node_t *node, unsigned flag) {
    return (node->flags & flag) != 0;
}

void save_load_difficulty(const save_state_t *state, button_t *out) {
    const difficulty_options_entry_t *option;
    assert(state != NULL && out != NULL);
    option = &difficulty_options_table[state->difficulty - 1];
    *out = (button_t){0};
    out->name = option->name;
    out->has_trade = 1;
    out->trade = p8_fixed_from_int(option->trade);
    out->has_type_bias = 1;
    out->type_bias = option->type_bias;
}

void save_load_background_seed(const save_state_t *state) {
    assert(state != NULL);
    p8_fixed_set_rnd_state(state->background_seed);
}

static void load_needs(planet_t *planet, const save_node_t *node) {
    const allocator_t *zone = grab_arena_construction_zone();
    pointer_list_t needs;

    if (!has(node, SAVE_NODE_HAS_NEEDS)) {
        planet->has &= (uint16_t)~PLANET_HAS_NEEDS;
        return;
    }
    pointer_list_init(&needs, zone);
    if (node->need_count > 0) {
        pointer_list_init_reserve(&needs, zone, node->need_count);
    }
    for (int i = 0; i < node->need_count; i++) {
        resource_t resource = {RESOURCE_NIL, 0};
        if (node->needs[i].has_resource) {
            resource.kind = RESOURCE_FIXED;
            resource.number = node->needs[i].resource;
        }
        pointer_list_add(&needs, need_new(zone, resource));
    }
    planet->needs = needs;
    planet->has |= PLANET_HAS_NEEDS;
}

static void load_production(planet_t *planet, const save_node_t *node) {
    if (!has(node, SAVE_NODE_HAS_PRODUCTION)) {
        planet->has &= (uint16_t)~PLANET_HAS_PRODUCTION;
        return;
    }
    planet->production.has_resource = has(node, SAVE_NODE_PRODUCTION_HAS_RESOURCE);
    planet->production.resource = planet->production.has_resource ? node->production_resource : 0;
    /* The routes rebuild of the first recompute reads the amount before update_tiers sets it. */
    planet->production.amount = p8_fixed_from_int(0);
    economy_tiers_compute_production_amount(planet, &planet->production.amount);
    planet->has |= PLANET_HAS_PRODUCTION;
}

static planet_t *(*const node_constructors[SAVE_NODE_KIND_COUNT])(const planet_keys_t *) = {
    [SAVE_NODE_PLANET] = planet_new,
    [SAVE_NODE_PROTOSTAR] = protostar_new,
    [SAVE_NODE_STRUCTURE] = structure_new,
    [SAVE_NODE_PROCESSOR] = processor_new,
    [SAVE_NODE_SLIPGATE] = slipgate_new,
    [SAVE_NODE_INFRASPACE_GATE] = infraspace_gate_new,
    [SAVE_NODE_TRADING_HUB] = trading_hub_new,
    [SAVE_NODE_SYNTHESIZER] = synthesizer_new,
    [SAVE_NODE_ASCENSION] = ascension_new,
};

/* Planets and protostars take their position as keys; the structure
   constructors assert that their keys hold none, so it is set after. */
static planet_t *construct_node(const save_node_t *node) {
    int positions_are_keys = node->kind == SAVE_NODE_PLANET || node->kind == SAVE_NODE_PROTOSTAR;
    planet_keys_t keys = {0};
    planet_t *planet;

    if (has(node, SAVE_NODE_HAS_PLANET_ID)) {
        planet_keys_set_id(&keys, node->planet_id);
    }
    if (positions_are_keys && has(node, SAVE_NODE_HAS_POS)) {
        keys.pos = vec_of(node->pos);
        keys.has_pos = 1;
    }
    if (positions_are_keys && has(node, SAVE_NODE_HAS_MAP_POS)) {
        keys.map_pos = vec_of(node->map_pos);
        keys.has_map_pos = 1;
    }
    planet = node_constructors[node->kind](&keys);
    if (!positions_are_keys && has(node, SAVE_NODE_HAS_POS)) {
        entity_set_pos(&planet->header, vec_of(node->pos));
    }
    if (!positions_are_keys && has(node, SAVE_NODE_HAS_MAP_POS)) {
        entity_set_map_pos(&planet->header, vec_of(node->map_pos));
    }
    return planet;
}

static planet_t *load_node(const save_node_t *node) {
    planet_t *planet = construct_node(node);

    entity_set_state(&planet->header, (state_t)node->state);
    if (has(node, SAVE_NODE_HAS_STATE_TIME)) {
        planet->header.state_time = node->state_time;
    }
    planet->planet_type = node->planet_type != 0 ? &planet_types_table[node->planet_type - 1] : NULL;
    if (node->project != 0) {
        planet_install_project(planet, research_projects_at(node->project));
    }
    /* After planet_install_project, which clears it. */
    planet->description = node->description_type != 0 ? planet_types_table[node->description_type - 1].description : NULL;
    load_needs(planet, node);
    planet->has_development_tier = has(node, SAVE_NODE_HAS_DEVELOPMENT_TIER);
    planet->development_tier = planet->has_development_tier ? node->development_tier : 0;
    planet->has_production_bonus = has(node, SAVE_NODE_HAS_PRODUCTION_BONUS);
    planet->production_bonus = planet->has_production_bonus ? node->production_bonus : 0;
    planet->header.fold_anchor = node->fold_anchor;
    if (node->infraspace != 0) {
        planet->header.infraspace = node->infraspace;
    }
    return planet;
}

/* Unregistered rather than marked dead: the release waits for the next
   frame's exit-queue flush, so the first update never removes an entity
   mid-walk and skips the next one. */
static void remove_bursts(void) {
    const entity_list_t *live = live_entities();
    for (int i = live->count - 1; i >= 0; i--) {
        if (live->items[i]->class == burst_class()) {
            entity_unregister(live->items[i]);
        }
    }
}

/* A probe is only ever saved settled: a held one exists before the first
   action only, when nothing is pending. */
static void load_probe(const save_probe_t *saved) {
    probe_t *probe = probe_new(NULL);
    entity_set_state(&probe->header, ENTITY_STATE_SETTLED);
    entity_set_pos(&probe->header, vec_of(saved->pos));
    entity_set_map_pos(&probe->header, vec_of(saved->map_pos));
    probe->header.fold_anchor = saved->fold_anchor;
}

static void load_slipways(const save_state_t *state, const entity_id_t *node_ids, entity_id_t *out) {
    for (int i = 0; i < state->slipway_count; i++) {
        slipway_keys_t keys = {.start = node_ids[state->slipways[i].start]};
        slipway_t *slipway = slipway_new(&keys);
        slipway_connect(slipway, entity_get(node_ids[state->slipways[i].end]));
        slipway->header.state_time = state->slipways[i].state_time;
        out[i] = entity_id_link(&slipway->header);
    }
}

/* Node-major, each list in its saved order: every list comes out as saved. */
static void load_attach_order(const save_state_t *state, const entity_id_t *node_ids, const entity_id_t *slipway_ids) {
    for (int i = 0; i < state->node_count; i++) {
        const save_node_t *node = &state->nodes[i];
        entity_t *header = entity_get(node_ids[i]);
        assert(header->class->attach_slipway != NULL && "save_load: every saved node takes slipways");
        for (int k = 0; k < node->slipway_count; k++) {
            header->class->attach_slipway(header, entity_get(slipway_ids[state->node_slipways[node->first_slipway + k]]));
        }
    }
}

/* Attaching sets the fold anchor and restarts the shake, as a protostar's
   archetype does on construction; a synthesizer's attach reads needs not
   satisfied yet, so may make another resource and spend itself when the
   saved game did not. */
static void undo_attach_side_effects(const save_state_t *state, const entity_id_t *node_ids) {
    for (int i = 0; i < state->node_count; i++) {
        const save_node_t *node = &state->nodes[i];
        planet_t *planet = (planet_t *)entity_get(node_ids[i]);
        planet->header.fold_anchor = node->fold_anchor;
        planet->header.shake = 0;
        load_production(planet, node);
        if (node->kind == SAVE_NODE_SYNTHESIZER) {
            synthesizer_set_spent(planet, has(node, SAVE_NODE_SYNTHESIZER_SPENT));
        }
    }
}

/* A new list in the game zone: the route outlives the frame. */
static entity_id_list_t route_id_list(const save_state_t *state, int first, int count, const entity_id_t *ids) {
    entity_id_list_t list;
    entity_id_list_init(&list, game_zone());
    for (int i = 0; i < count; i++) {
        entity_id_list_add(&list, ids[state->route_indices[first + i]]);
    }
    return list;
}

/* Never recomputed: a partner chosen long ago stays the same. */
static void load_outgoing_routes(const save_state_t *state, const entity_id_t *node_ids, const entity_id_t *slipway_ids) {
    for (int i = 0; i < state->node_count; i++) {
        const save_node_t *node = &state->nodes[i];
        planet_t *planet = (planet_t *)entity_get(node_ids[i]);
        if (!has(node, SAVE_NODE_HAS_OUTGOING_ROUTES)) {
            continue;
        }
        trade_route_list_init(&planet->outgoing_routes, game_zone());
        planet->has |= PLANET_HAS_OUTGOING_ROUTES;
        for (int k = 0; k < node->route_count; k++) {
            const save_route_t *saved = &state->routes[node->first_route + k];
            trade_keys_t keys = {0};
            trade_route_t route;
            keys.planets = route_id_list(state, saved->first_planet, saved->planet_count, node_ids);
            keys.slipways = route_id_list(state, saved->first_slipway, saved->slipway_count, slipway_ids);
            keys.has_resource = saved->has_resource;
            keys.resource_is_bool = saved->resource_is_bool;
            keys.resource = saved->resource;
            route = trade_new(&keys);
            trade_route_list_add(&planet->outgoing_routes, &route);
        }
    }
}

static void load_rolls(const save_state_t *state) {
    const save_type_picker_t *saved = &state->type_picker;
    weighted_picker_t *picker;
    random_walk_t *walk;
    random_walk_t *vanish_walk;

    galaxy_install_rolls();
    picker = planet_type_picker();
    walk = weighted_picker_walk(picker);
    vanish_walk = planet_vanish_roll();
    assert(picker != NULL && walk != NULL && vanish_walk != NULL);

    picker->tickets_count = saved->tickets_count;
    for (int i = 0; i < saved->tickets_count; i++) {
        picker->tickets[i] = &planet_types_table[saved->tickets[i]];
    }
    for (int i = 0; i < SAVE_STATE_RECENT_COUNT; i++) {
        picker->recent_set[i] = saved->recent_set[i];
        picker->recent[i] = saved->recent_set[i] ? &planet_types_table[saved->recent[i]] : NULL;
    }
    picker->multiplier = saved->multiplier;
    walk->step = saved->walk_step;
    walk->bias = saved->walk_bias;
    vanish_walk->step = state->vanish_walk.step;
    vanish_walk->bias = state->vanish_walk.bias;
}

void save_load_world(const save_state_t *state) {
    static entity_id_t node_ids[SAVE_STATE_NODES_CAP];
    static entity_id_t slipway_ids[SAVE_STATE_SLIPWAYS_CAP];

    assert(state != NULL);
    for (int i = 0; i < state->node_count; i++) {
        node_ids[i] = entity_id_link(&load_node(&state->nodes[i])->header);
    }
    /* A protostar's constructor bursts; the save never holds a burst. */
    remove_bursts();
    /* Before the slipways: the probe and infraspace slipways share a layer, drawn in registration order. */
    if (state->has_probe) {
        load_probe(&state->probe);
    }
    load_slipways(state, node_ids, slipway_ids);
    load_attach_order(state, node_ids, slipway_ids);
    undo_attach_side_effects(state, node_ids);
    load_outgoing_routes(state, node_ids, slipway_ids);
    load_rolls(state);
}

static void refresh_structure_icons(void) {
    const entity_list_t *refreshed = entities_by_message(ENTITY_MESSAGE_REFRESH);
    for (int i = 0; refreshed != NULL && i < refreshed->count; i++) {
        if (refreshed->items[i]->class == structure_icon_class()) {
            structure_icon_refresh((structure_icon_t *)refreshed->items[i]);
        }
    }
}

/* One tech at a time, in saved order: an icon takes the shared cursor slot
   when revealed. Never economy_invent: it spends research points and
   recomputes routes against a half-restored list. */
static void load_invented(economy_t *economy, const save_economy_t *saved) {
    refresh_structure_icons();
    for (int i = 0; i < saved->invented_count; i++) {
        pointer_list_add(&economy->invented, (void *)technologies_table[saved->invented[i]].name);
        refresh_structure_icons();
    }
}

void save_load_globals(const save_state_t *state) {
    economy_t *economy = main_economy_typed();
    entity_t *camera = main_camera();

    assert(state != NULL);
    assert(economy != NULL && "save_load_globals: the economy is built first");
    economy->credits = state->economy.credits;
    economy->research_points = state->economy.research_points;
    economy->era = state->economy.era;
    economy->year = state->economy.year;
    economy->year_ticks = state->economy.year_ticks;
    load_invented(economy, &state->economy);

    structure_icon_set_id_counter(state->structure_id_counter);

    entity_set_map_pos(camera, vec_of(state->camera_map_pos));
    entity_set_pos(camera, vec_of(state->camera_map_pos));
    /* A protostar's burst shook it; the save never holds a shake. */
    camera->shake = 0;

    /* Last: the constructors before it may draw from the RNG. */
    p8_fixed_set_rnd_state(state->rnd_state);
}
