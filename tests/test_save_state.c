#include "game/save_state.h"

#include "engine/dispatch_context.h"
#include "engine/entity.h"
#include "engine/entity_class.h"
#include "engine/entity_registry.h"
#include "game/background.h"
#include "game/cart.h"
#include "game/economy_score.h"
#include "game/planet.h"
#include "game/probe.h"
#include "game/ui_button.h"
#include "p8/sprite_sheet.h"
#include "p8/fixed.h"
#include "p8/gfx.h"
#include "p8/input.h"
#include "p8/vram.h"

#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

static void test_crc32_matches_the_standard_check_value(void) {
    const char *check = "123456789";
    assert(save_state_crc32((const uint8_t *)check, (int)strlen(check)) == 0xCBF43926u);
}

static uint32_t bitwise_crc32_update(uint32_t crc, uint8_t byte) {
    crc ^= byte;
    for (int bit = 0; bit < 8; bit++) {
        crc = (crc >> 1) ^ (0xEDB88320u & (0u - (crc & 1u)));
    }
    return crc;
}

static uint32_t bitwise_crc32(const uint8_t *bytes, int count) {
    uint32_t crc = 0xFFFFFFFFu;
    for (int i = 0; i < count; i++) {
        crc = bitwise_crc32_update(crc, bytes[i]);
    }
    return crc ^ 0xFFFFFFFFu;
}

static void test_crc32_of_the_empty_buffer_is_zero(void) {
    uint8_t unused = 0;
    assert(save_state_crc32(&unused, 0) == 0u);
}

static void test_crc32_of_each_single_byte_matches_the_bitwise_definition(void) {
    for (int value = 0; value < 256; value++) {
        uint8_t byte = (uint8_t)value;
        assert(save_state_crc32(&byte, 1) == bitwise_crc32(&byte, 1));
    }
}

static void test_crc32_matches_the_bitwise_definition_at_every_length(void) {
    static uint8_t bytes[600];
    uint32_t seed = 0x12345678u;

    for (int i = 0; i < (int)sizeof bytes; i++) {
        seed = seed * 1103515245u + 12345u;
        bytes[i] = (uint8_t)(seed >> 16);
    }
    for (int count = 0; count <= (int)sizeof bytes; count++) {
        assert(save_state_crc32(bytes, count) == bitwise_crc32(bytes, count));
    }
}

static save_state_t g_state;
static save_state_t g_decoded;
static uint8_t g_bytes[SAVE_STATE_MAX_BYTES];
static uint8_t g_again[SAVE_STATE_MAX_BYTES];

static void build_valid_state(save_state_t *state) {
    memset(state, 0, sizeof *state);
    state->difficulty = 3;
    state->economy.credits = -0x00028000;
    state->economy.research_points = 0x00070000;
    state->economy.era = 0x00020000;
    state->economy.year = 0x08E80000;
    state->economy.year_ticks = 0x00001234;
    state->economy.invented_count = 3;
    state->economy.invented[0] = 18;
    state->economy.invented[1] = 0;
    state->economy.invented[2] = 7;
    state->rnd_state = 0xDEADBEEFu;
    state->background_seed = 0xCAFEF00Du;
    state->type_picker.tickets_count = WEIGHTED_PICKER_TICKETS_CAP;
    for (int i = 0; i < WEIGHTED_PICKER_TICKETS_CAP; i++) {
        state->type_picker.tickets[i] = (uint8_t)(i % PLANET_TYPES_COUNT);
    }
    state->type_picker.recent[0] = 10;
    state->type_picker.recent_set[0] = 1;
    state->type_picker.recent[1] = 4;
    state->type_picker.recent_set[1] = 1;
    state->type_picker.multiplier = 0x00020000;
    state->type_picker.walk_step = 0x00008000;
    state->type_picker.walk_bias = -0x00004000;
    state->vanish_walk.step = 0x00001000;
    state->vanish_walk.bias = 0x7FFFFFFF;
    state->structure_id_counter = 12;
    state->has_probe = 1;
    state->probe.pos.x = 0x00400000;
    state->probe.pos.y = -0x00100000;
    state->probe.map_pos.x = 0x00010000;
    state->probe.map_pos.y = 0x00020000;
    state->probe.fold_anchor = (int32_t)0x80000000u;
    state->camera_map_pos.x = -0x00200000;
    state->camera_map_pos.y = 0x00300000;
}

static int encode_decode_reencode(const save_state_t *state, int *count) {
    *count = save_state_encode(state, g_bytes);
    if (!save_state_decode(g_bytes, *count, &g_decoded)) {
        return 0;
    }
    return save_state_encode(&g_decoded, g_again) == *count && memcmp(g_bytes, g_again, (size_t)*count) == 0;
}

static void test_a_hand_built_state_round_trips(void) {
    int count;
    build_valid_state(&g_state);
    assert(encode_decode_reencode(&g_state, &count));
    assert(count <= SAVE_STATE_MAX_BYTES);
    assert(g_decoded.rnd_state == 0xDEADBEEFu && g_decoded.background_seed == 0xCAFEF00Du);
}

static void test_a_state_without_probe_round_trips(void) {
    int count;
    int with_probe;
    build_valid_state(&g_state);
    with_probe = save_state_encode(&g_state, g_bytes);
    g_state.has_probe = 0;
    memset(&g_state.probe, 0, sizeof g_state.probe);
    assert(encode_decode_reencode(&g_state, &count));
    assert(count == with_probe - 5 * 4);
}

enum { HEADER_BYTES = 8, TRAILER_BYTES = 4 };

static int encode_valid(void) {
    build_valid_state(&g_state);
    return save_state_encode(&g_state, g_bytes);
}

static void reseal(uint8_t *bytes, int count) {
    int payload = count - HEADER_BYTES - TRAILER_BYTES;
    uint32_t crc;
    bytes[6] = (uint8_t)(payload & 0xFF);
    bytes[7] = (uint8_t)(payload >> 8);
    crc = save_state_crc32(bytes, count - TRAILER_BYTES);
    for (int i = 0; i < 4; i++) {
        bytes[count - TRAILER_BYTES + i] = (uint8_t)(crc >> (8 * i));
    }
}

static int decodes(const uint8_t *bytes, int count) {
    return save_state_decode(bytes, count, &g_decoded);
}

static void test_a_flipped_byte_anywhere_is_refused(void) {
    int count = encode_valid();
    int positions[] = {2, 5, 7, HEADER_BYTES, HEADER_BYTES + 40, count - TRAILER_BYTES - 1, count - TRAILER_BYTES, count - 1};
    assert(decodes(g_bytes, count));
    for (size_t i = 0; i < sizeof positions / sizeof positions[0]; i++) {
        g_bytes[positions[i]] ^= 0x10;
        assert(!decodes(g_bytes, count));
        g_bytes[positions[i]] ^= 0x10;
    }
}

static void test_a_buffer_shorter_than_its_header_says_is_refused(void) {
    int count = encode_valid();
    assert(!decodes(g_bytes, count - 1));
    assert(!decodes(g_bytes, HEADER_BYTES + TRAILER_BYTES - 1));
    assert(!decodes(g_bytes, 0));
}

static void test_a_payload_cut_short_is_refused_even_resealed(void) {
    encode_valid();
    int cut = HEADER_BYTES + 30;
    memcpy(g_again, g_bytes, (size_t)cut);
    reseal(g_again, cut + TRAILER_BYTES);
    assert(!decodes(g_again, cut + TRAILER_BYTES));
}

static void test_a_payload_with_an_unread_byte_is_refused_even_resealed(void) {
    int count = encode_valid();
    memcpy(g_again, g_bytes, (size_t)(count - TRAILER_BYTES));
    g_again[count - TRAILER_BYTES] = 0;
    reseal(g_again, count + 1);
    assert(!decodes(g_again, count + 1));
}

static void test_a_wrong_magic_or_version_is_refused_even_resealed(void) {
    int count = encode_valid();
    g_bytes[0] = 'X';
    reseal(g_bytes, count);
    assert(!decodes(g_bytes, count));

    count = encode_valid();
    g_bytes[4] = 2;
    reseal(g_bytes, count);
    assert(!decodes(g_bytes, count));

    count = encode_valid();
    reseal(g_bytes, count);
    assert(decodes(g_bytes, count));
}

static int edited_state_decodes(void) {
    int count = save_state_encode(&g_state, g_bytes);
    return decodes(g_bytes, count);
}

static void test_each_global_out_of_range_value_is_refused(void) {
    build_valid_state(&g_state);
    g_state.difficulty = 0;
    assert(!edited_state_decodes());

    build_valid_state(&g_state);
    g_state.difficulty = DIFFICULTY_OPTIONS_COUNT + 1;
    assert(!edited_state_decodes());

    build_valid_state(&g_state);
    g_state.economy.invented[1] = TECHNOLOGIES_COUNT;
    assert(!edited_state_decodes());

    build_valid_state(&g_state);
    g_state.economy.invented[2] = g_state.economy.invented[0];
    assert(!edited_state_decodes());

    build_valid_state(&g_state);
    g_state.type_picker.tickets[5] = PLANET_TYPES_COUNT;
    assert(!edited_state_decodes());

    build_valid_state(&g_state);
    g_state.type_picker.recent[1] = PLANET_TYPES_COUNT;
    assert(!edited_state_decodes());

    build_valid_state(&g_state);
    g_state.type_picker.recent_set[2] = 2;
    assert(!edited_state_decodes());

    build_valid_state(&g_state);
    g_state.has_probe = 2;
    assert(!edited_state_decodes());
}

static void test_the_edges_of_each_global_range_are_accepted(void) {
    build_valid_state(&g_state);
    g_state.difficulty = 1;
    assert(edited_state_decodes());

    build_valid_state(&g_state);
    g_state.difficulty = DIFFICULTY_OPTIONS_COUNT;
    g_state.economy.invented_count = TECHNOLOGIES_COUNT;
    for (int i = 0; i < TECHNOLOGIES_COUNT; i++) {
        g_state.economy.invented[i] = (uint8_t)(TECHNOLOGIES_COUNT - 1 - i);
    }
    assert(edited_state_decodes());

    build_valid_state(&g_state);
    g_state.type_picker.recent[2] = 0xFF;
    g_state.type_picker.recent_set[2] = 0;
    assert(edited_state_decodes());
}

static int32_t resource(int code) {
    return (int32_t)((uint32_t)code << 16);
}

static save_node_t *add_node(save_state_t *state, save_node_kind_t kind, state_t node_state, uint16_t flags) {
    save_node_t *node = &state->nodes[state->node_count++];
    node->kind = (uint8_t)kind;
    node->state = (uint8_t)node_state;
    node->flags = flags;
    node->first_route = state->route_count;
    node->first_slipway = state->node_slipway_count;
    return node;
}

static void add_route(save_state_t *state, save_node_t *node, const uint8_t *planets, int planet_count, const uint8_t *slipways,
                      int slipway_count, uint8_t has_resource, uint8_t resource_is_bool, int32_t value) {
    save_route_t *route = &state->routes[state->route_count++];
    route->first_planet = state->route_index_count;
    route->planet_count = planet_count;
    for (int i = 0; i < planet_count; i++) {
        state->route_indices[state->route_index_count++] = planets[i];
    }
    route->first_slipway = state->route_index_count;
    route->slipway_count = slipway_count;
    for (int i = 0; i < slipway_count; i++) {
        state->route_indices[state->route_index_count++] = slipways[i];
    }
    route->has_resource = has_resource;
    route->resource_is_bool = resource_is_bool;
    route->resource = value;
    node->route_count++;
}

static void attach(save_state_t *state, save_node_t *node, uint8_t slipway) {
    state->node_slipways[state->node_slipway_count++] = slipway;
    node->slipway_count++;
}

static void add_slipway(save_state_t *state, uint8_t start, uint8_t end, int32_t state_time) {
    state->slipways[state->slipway_count].start = start;
    state->slipways[state->slipway_count].end = end;
    state->slipways[state->slipway_count].state_time = state_time;
    state->slipway_count++;
}

enum {
    PLANET_NODE = 0,
    PROTOSTAR_NODE = 1,
    STRUCTURE_NODE = 2,
    SYNTHESIZER_NODE = 7,
    ASCENSION_NODE = 8
};

/* Slipways: 0 = planet-protostar, 1 = structure-planet, 2 = structure-ascension. */
static void build_valid_world(save_state_t *state) {
    save_node_t *node;
    const uint8_t route0_planets[] = {PLANET_NODE, PROTOSTAR_NODE};
    const uint8_t route0_slipways[] = {0};
    const uint8_t route1_planets[] = {PLANET_NODE, STRUCTURE_NODE, ASCENSION_NODE};
    const uint8_t route1_slipways[] = {1, 2};
    const uint8_t route2_planets[] = {STRUCTURE_NODE};

    build_valid_state(state);

    node = add_node(state, SAVE_NODE_PLANET, ENTITY_STATE_NORMAL,
                    SAVE_NODE_HAS_POS | SAVE_NODE_HAS_MAP_POS | SAVE_NODE_HAS_PLANET_ID | SAVE_NODE_HAS_NEEDS |
                        SAVE_NODE_HAS_DEVELOPMENT_TIER | SAVE_NODE_HAS_PRODUCTION_BONUS |
                        SAVE_NODE_HAS_PRODUCTION | SAVE_NODE_PRODUCTION_HAS_RESOURCE | SAVE_NODE_HAS_OUTGOING_ROUTES);
    node->pos.x = 0x00400000;
    node->pos.y = -0x00200000;
    node->map_pos.x = 0x00050000;
    node->map_pos.y = -0x00030000;
    node->fold_anchor = 0x00018000;
    node->infraspace = -0x00010000;
    strcpy(node->planet_id, "p12");
    node->planet_type = PLANET_TYPES_COUNT;
    node->description_type = 2;
    node->need_count = 3;
    node->needs[0].has_resource = 1;
    node->needs[0].resource = resource(28);
    node->needs[1].has_resource = 0;
    node->needs[2].has_resource = 1;
    node->needs[2].resource = resource(25);
    node->development_tier = 0x00020000;
    node->production_bonus = 0x00010000;
    node->production_resource = resource(48);
    add_route(state, node, route0_planets, 2, route0_slipways, 1, 1, 0, resource(28));
    add_route(state, node, route1_planets, 3, route1_slipways, 2, 1, 1, 0x00010000);
    attach(state, node, 0);
    attach(state, node, 1);

    node = add_node(state, SAVE_NODE_PROTOSTAR, ENTITY_STATE_VANISHING, SAVE_NODE_HAS_POS | SAVE_NODE_HAS_STATE_TIME);
    node->state_time = 0x00008000;
    attach(state, node, 0);

    node = add_node(state, SAVE_NODE_STRUCTURE, ENTITY_STATE_SETTLED,
                    SAVE_NODE_HAS_POS | SAVE_NODE_HAS_PLANET_ID | SAVE_NODE_HAS_PRODUCTION | SAVE_NODE_HAS_OUTGOING_ROUTES |
                        SAVE_NODE_HAS_STATE_TIME);
    strcpy(node->planet_id, "s-32768");
    node->state_time = -0x00230000;
    node->project = PROJECT_RECORDS_COUNT;
    add_route(state, node, route2_planets, 1, NULL, 0, 0, 0, 0);
    attach(state, node, 2);
    attach(state, node, 1);

    add_node(state, SAVE_NODE_PROCESSOR, ENTITY_STATE_UNKNOWN, SAVE_NODE_HAS_MAP_POS | SAVE_NODE_HAS_OUTGOING_ROUTES | SAVE_NODE_HAS_STATE_TIME);
    add_node(state, SAVE_NODE_SLIPGATE, ENTITY_STATE_KNOWN, SAVE_NODE_HAS_NEEDS | SAVE_NODE_HAS_STATE_TIME);
    add_node(state, SAVE_NODE_INFRASPACE_GATE, ENTITY_STATE_SETTLED, SAVE_NODE_HAS_STATE_TIME);
    node = add_node(state, SAVE_NODE_TRADING_HUB, ENTITY_STATE_NORMAL, SAVE_NODE_HAS_PLANET_ID | SAVE_NODE_HAS_STATE_TIME);
    strcpy(node->planet_id, "!~");
    add_node(state, SAVE_NODE_SYNTHESIZER, ENTITY_STATE_NORMAL, SAVE_NODE_SYNTHESIZER_SPENT | SAVE_NODE_HAS_PRODUCTION | SAVE_NODE_HAS_STATE_TIME);
    node = add_node(state, SAVE_NODE_ASCENSION, ENTITY_STATE_SETTLED, SAVE_NODE_HAS_POS | SAVE_NODE_HAS_STATE_TIME);
    node->state_time = 0x7FFFFFFF;
    attach(state, node, 2);

    add_slipway(state, PLANET_NODE, PROTOSTAR_NODE, 0);
    add_slipway(state, STRUCTURE_NODE, PLANET_NODE, -0x00018000);
    add_slipway(state, STRUCTURE_NODE, ASCENSION_NODE, 0x12345678);
}

static void test_a_world_with_every_node_kind_and_flag_round_trips(void) {
    int count;
    int empty;
    build_valid_state(&g_state);
    empty = save_state_encode(&g_state, g_bytes);
    build_valid_world(&g_state);
    assert(encode_decode_reencode(&g_state, &count));
    assert(count > empty);
    assert(g_decoded.node_count == SAVE_NODE_KIND_COUNT);
    assert(g_decoded.slipway_count == 3);
    assert(g_decoded.slipways[1].state_time == -0x00018000 && g_decoded.slipways[2].state_time == 0x12345678);
    assert(g_decoded.route_count == 3);
    assert(strcmp(g_decoded.nodes[STRUCTURE_NODE].planet_id, "s-32768") == 0);
    assert(g_decoded.nodes[STRUCTURE_NODE].state_time == -0x00230000 && g_decoded.nodes[ASCENSION_NODE].state_time == 0x7FFFFFFF);
}

/* The slipway records close the payload, each ending on its state_time. */
static void test_the_last_slipway_state_time_ends_the_payload(void) {
    int count;
    build_valid_world(&g_state);
    count = save_state_encode(&g_state, g_bytes);
    assert(g_bytes[count - 8] == 0x78 && g_bytes[count - 7] == 0x56 && g_bytes[count - 6] == 0x34 && g_bytes[count - 5] == 0x12);
    assert(g_bytes[count - 9] == ASCENSION_NODE && g_bytes[count - 10] == STRUCTURE_NODE);
}

static void test_a_node_kind_outside_the_nine_is_refused(void) {
    build_valid_world(&g_state);
    g_state.nodes[ASCENSION_NODE].kind = SAVE_NODE_KIND_COUNT;
    assert(!edited_state_decodes());
}

static void test_a_state_a_saved_node_never_has_is_refused(void) {
    const state_t forbidden[] = {ENTITY_STATE_NONE, ENTITY_STATE_PLACING, ENTITY_STATE_UNFOLDING, STATE_COUNT};
    for (size_t i = 0; i < sizeof forbidden / sizeof forbidden[0]; i++) {
        build_valid_world(&g_state);
        g_state.nodes[STRUCTURE_NODE].state = (uint8_t)forbidden[i];
        assert(!edited_state_decodes());
    }
}

static void test_an_unknown_node_flag_bit_is_refused(void) {
    for (int bit = 11; bit < 16; bit++) {
        build_valid_world(&g_state);
        g_state.nodes[PROTOSTAR_NODE].flags |= (uint16_t)(1u << bit);
        assert(!edited_state_decodes());
    }
}

static void test_each_node_flag_inconsistency_is_refused(void) {
    build_valid_world(&g_state);
    g_state.nodes[PLANET_NODE].flags &= (uint16_t)~SAVE_NODE_HAS_PRODUCTION;
    assert(!edited_state_decodes());

    build_valid_world(&g_state);
    g_state.nodes[STRUCTURE_NODE].flags |= SAVE_NODE_SYNTHESIZER_SPENT;
    assert(!edited_state_decodes());

    build_valid_world(&g_state);
    g_state.nodes[PLANET_NODE].flags |= SAVE_NODE_HAS_STATE_TIME;
    assert(!edited_state_decodes());

    build_valid_world(&g_state);
    g_state.nodes[STRUCTURE_NODE].flags &= (uint16_t)~SAVE_NODE_HAS_STATE_TIME;
    assert(!edited_state_decodes());

    build_valid_world(&g_state);
    g_state.nodes[PROTOSTAR_NODE].state = ENTITY_STATE_SETTLED;
    assert(edited_state_decodes() && "a protostar's clock is saved in any state");

    build_valid_world(&g_state);
    g_state.nodes[PROTOSTAR_NODE].flags &= (uint16_t)~SAVE_NODE_HAS_STATE_TIME;
    assert(!edited_state_decodes());
}

static void test_a_bad_planet_id_is_refused(void) {
    build_valid_world(&g_state);
    g_state.nodes[PLANET_NODE].planet_id[0] = '\0';
    assert(!edited_state_decodes());

    build_valid_world(&g_state);
    memcpy(g_state.nodes[STRUCTURE_NODE].planet_id, "s-327680", SAVE_STATE_PLANET_ID_MAX + 1);
    assert(!edited_state_decodes());

    build_valid_world(&g_state);
    g_state.nodes[PLANET_NODE].planet_id[1] = ' ';
    assert(!edited_state_decodes());

    build_valid_world(&g_state);
    g_state.nodes[PLANET_NODE].planet_id[1] = 0x7F;
    assert(!edited_state_decodes());
}

static void test_a_nul_inside_an_encoded_planet_id_is_refused_even_resealed(void) {
    const uint8_t id[] = {3, 'p', '1', '2'};
    int count;
    int at = -1;
    build_valid_world(&g_state);
    count = save_state_encode(&g_state, g_bytes);
    for (int i = HEADER_BYTES; i + (int)sizeof id <= count; i++) {
        if (memcmp(&g_bytes[i], id, sizeof id) == 0) {
            at = i;
            break;
        }
    }
    assert(at >= 0);
    g_bytes[at + 2] = 0;
    reseal(g_bytes, count);
    assert(!decodes(g_bytes, count));
}

static void test_a_planet_type_or_project_out_of_range_is_refused(void) {
    build_valid_world(&g_state);
    g_state.nodes[PLANET_NODE].planet_type = PLANET_TYPES_COUNT + 1;
    assert(!edited_state_decodes());

    build_valid_world(&g_state);
    g_state.nodes[STRUCTURE_NODE].project = PROJECT_RECORDS_COUNT + 1;
    assert(!edited_state_decodes());

    build_valid_world(&g_state);
    g_state.nodes[PLANET_NODE].description_type = PLANET_TYPES_COUNT + 1;
    assert(!edited_state_decodes());

    build_valid_world(&g_state);
    g_state.nodes[PLANET_NODE].description_type = PLANET_TYPES_COUNT;
    assert(edited_state_decodes());
}

static void test_bad_needs_and_production_resources_are_refused(void) {
    build_valid_world(&g_state);
    g_state.nodes[PLANET_NODE].need_count = SAVE_STATE_NEEDS_CAP + 1;
    assert(!edited_state_decodes());

    build_valid_world(&g_state);
    g_state.nodes[PLANET_NODE].needs[1].has_resource = 2;
    assert(!edited_state_decodes());

    build_valid_world(&g_state);
    g_state.nodes[PLANET_NODE].needs[0].resource = resource(29);
    assert(!edited_state_decodes());

    build_valid_world(&g_state);
    g_state.nodes[PLANET_NODE].needs[2].resource = 25;
    assert(!edited_state_decodes());

    build_valid_world(&g_state);
    g_state.nodes[PLANET_NODE].production_resource = resource(0);
    assert(!edited_state_decodes());
}

static void test_a_full_need_list_of_every_resource_code_is_accepted(void) {
    const int codes[] = {28, 21, 18, 19, 48, 22, 20, 12, 24, 36, 33, 54, 25};
    for (size_t first = 0; first < sizeof codes / sizeof codes[0]; first += SAVE_STATE_NEEDS_CAP) {
        save_node_t *node;
        build_valid_world(&g_state);
        node = &g_state.nodes[PLANET_NODE];
        node->need_count = SAVE_STATE_NEEDS_CAP;
        for (int i = 0; i < SAVE_STATE_NEEDS_CAP; i++) {
            node->needs[i].has_resource = 1;
            node->needs[i].resource = resource(codes[(first + (size_t)i) % (sizeof codes / sizeof codes[0])]);
        }
        node->production_resource = node->needs[SAVE_STATE_NEEDS_CAP - 1].resource;
        assert(edited_state_decodes());
    }
}

static save_route_t *planet_route(int k) {
    return &g_state.routes[g_state.nodes[PLANET_NODE].first_route + k];
}

static void test_a_route_naming_an_unknown_node_or_slipway_is_refused(void) {
    build_valid_world(&g_state);
    g_state.route_indices[planet_route(1)->first_planet + 2] = SAVE_NODE_KIND_COUNT;
    assert(!edited_state_decodes());

    build_valid_world(&g_state);
    g_state.route_indices[planet_route(1)->first_slipway + 1] = 3;
    assert(!edited_state_decodes());

    build_valid_world(&g_state);
    planet_route(0)->has_resource = 4;
    assert(!edited_state_decodes());
}

static void test_a_bad_slipway_is_refused(void) {
    build_valid_world(&g_state);
    g_state.slipways[2].end = SAVE_NODE_KIND_COUNT;
    assert(!edited_state_decodes());

    build_valid_world(&g_state);
    g_state.slipways[2].start = SAVE_NODE_KIND_COUNT;
    assert(!edited_state_decodes());

    build_valid_world(&g_state);
    g_state.slipways[1].end = STRUCTURE_NODE;
    assert(!edited_state_decodes());
}

static void test_a_bad_node_slipway_list_is_refused(void) {
    build_valid_world(&g_state);
    g_state.node_slipways[g_state.nodes[ASCENSION_NODE].first_slipway] = 3;
    assert(!edited_state_decodes());

    build_valid_world(&g_state);
    g_state.node_slipways[g_state.nodes[ASCENSION_NODE].first_slipway] = 1;
    assert(!edited_state_decodes());

    build_valid_world(&g_state);
    g_state.node_slipways[g_state.nodes[STRUCTURE_NODE].first_slipway + 1] = 2;
    assert(!edited_state_decodes());
}

static p8_vram_t memory;
static p8_graphics_t graphics;
static p8_input_t input;

static void start_game_on_challenging(void) {
    button_t challenging = {0};
    p8_vram_init(&memory);
    p8_gfx_init(&graphics);
    p8_sprite_sheet_load(&memory);
    p8_input_init(&input);
    dispatch_context_set_gfx(&graphics);
    dispatch_context_set_mem(&memory);
    dispatch_context_set_input(&input);
    cart_load();
    cart_init();
    challenging.name = difficulty_options_table[2].name;
    challenging.has_trade = 1;
    challenging.trade = p8_fixed_from_int(difficulty_options_table[2].trade);
    challenging.has_type_bias = 1;
    challenging.type_bias = difficulty_options_table[2].type_bias;
    cart_start_game(&challenging);
}

static void test_a_fresh_game_captures_and_round_trips(void) {
    const entity_t *camera;
    int count;
    int again;

    start_game_on_challenging();
    save_state_capture(&g_state);

    assert(g_state.difficulty == 3);
    assert(entities_by_tag(TAG_PLANET)->count > 0);
    assert(g_state.node_count == entities_by_tag(TAG_PLANET)->count);
    assert(g_state.slipway_count == 0);
    assert(g_state.has_probe == 0);
    camera = main_camera();
    assert(g_state.camera_map_pos.x == camera->map_pos.x && g_state.camera_map_pos.y == camera->map_pos.y);
    assert(g_state.type_picker.tickets_count > 0);
    for (int i = 0; i < g_state.node_count; i++) {
        assert(g_state.nodes[i].kind == SAVE_NODE_PLANET);
        assert(g_state.nodes[i].flags & SAVE_NODE_HAS_PLANET_ID);
    }

    assert(encode_decode_reencode(&g_state, &count));

    save_state_capture(&g_state);
    again = save_state_encode(&g_state, g_again);
    assert(again == count && memcmp(g_bytes, g_again, (size_t)count) == 0);
}

static p8_vram_t redrawn;

static void test_the_captured_background_seed_redraws_the_same_tile_map(void) {
    start_game_on_challenging();
    save_state_capture(&g_state);
    assert(g_state.background_seed != g_state.rnd_state && "the game drew from the RNG after the background");

    p8_vram_init(&redrawn);
    p8_fixed_set_rnd_state(g_state.background_seed);
    background_construct(&redrawn, NULL);
    for (int y = 0; y < 32; y++) {
        for (int x = 0; x < 32; x++) {
            assert(redrawn.map_cells[y][x] == memory.map_cells[y][x]);
        }
    }
}

/* Terraform leaves the old type's description on the planet: build that directly. */
static void test_a_terraformed_planet_keeps_its_old_description(void) {
    planet_t *planet;
    int count;

    start_game_on_challenging();
    planet = (planet_t *)entities_by_tag(TAG_NODE)->items[0];
    planet->planet_type = &planet_types_table[4];
    planet->description = planet_types_table[1].description;
    save_state_capture(&g_state);
    assert(g_state.nodes[0].planet_type == 5);
    assert(g_state.nodes[0].description_type == 2);
    assert(encode_decode_reencode(&g_state, &count));
    assert(g_decoded.nodes[0].description_type == 2);

    planet->description = NULL;
    save_state_capture(&g_state);
    assert(g_state.nodes[0].description_type == 0);
}

static void test_only_a_settled_probe_is_captured(void) {
    probe_t *probe;

    start_game_on_challenging();
    probe = probe_new(NULL);
    save_state_capture(&g_state);
    assert(g_state.has_probe == 0 && "a held probe is not saved: the loader rebuilds a settled one");

    entity_set_state(&probe->header, ENTITY_STATE_SETTLED);
    save_state_capture(&g_state);
    assert(g_state.has_probe == 1);
}

int main(void) {
    test_crc32_matches_the_standard_check_value();
    test_crc32_of_the_empty_buffer_is_zero();
    test_crc32_of_each_single_byte_matches_the_bitwise_definition();
    test_crc32_matches_the_bitwise_definition_at_every_length();
    test_a_hand_built_state_round_trips();
    test_a_state_without_probe_round_trips();
    test_a_flipped_byte_anywhere_is_refused();
    test_a_buffer_shorter_than_its_header_says_is_refused();
    test_a_payload_cut_short_is_refused_even_resealed();
    test_a_payload_with_an_unread_byte_is_refused_even_resealed();
    test_a_wrong_magic_or_version_is_refused_even_resealed();
    test_each_global_out_of_range_value_is_refused();
    test_the_edges_of_each_global_range_are_accepted();
    test_a_world_with_every_node_kind_and_flag_round_trips();
    test_a_node_kind_outside_the_nine_is_refused();
    test_a_state_a_saved_node_never_has_is_refused();
    test_an_unknown_node_flag_bit_is_refused();
    test_each_node_flag_inconsistency_is_refused();
    test_a_bad_planet_id_is_refused();
    test_a_nul_inside_an_encoded_planet_id_is_refused_even_resealed();
    test_a_planet_type_or_project_out_of_range_is_refused();
    test_bad_needs_and_production_resources_are_refused();
    test_a_full_need_list_of_every_resource_code_is_accepted();
    test_the_last_slipway_state_time_ends_the_payload();
    test_a_route_naming_an_unknown_node_or_slipway_is_refused();
    test_a_bad_slipway_is_refused();
    test_a_bad_node_slipway_list_is_refused();
    test_a_fresh_game_captures_and_round_trips();
    test_the_captured_background_seed_redraws_the_same_tile_map();
    test_a_terraformed_planet_keeps_its_old_description();
    test_only_a_settled_probe_is_captured();
    printf("test_save_state: all tests passed\n");
    return 0;
}
