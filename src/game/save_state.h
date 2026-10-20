#ifndef GAME_SAVE_STATE_H
#define GAME_SAVE_STATE_H

#include "game/game_data.h"
#include "game/gen.h"
#include "game/map_tables.h"

#include <stdint.h>

enum {
    SAVE_STATE_MAX_BYTES = 8192,
    SAVE_STATE_VERSION = 1,
    SAVE_STATE_RECENT_COUNT = 3,
    SAVE_STATE_NODES_CAP = 255,
    SAVE_STATE_SLIPWAYS_CAP = 255,
    SAVE_STATE_ROUTES_CAP = SAVE_STATE_MAX_BYTES / 3,
    SAVE_STATE_ROUTE_INDICES_CAP = SAVE_STATE_MAX_BYTES,
    SAVE_STATE_NODE_SLIPWAYS_CAP = 2 * SAVE_STATE_SLIPWAYS_CAP,
    SAVE_STATE_NEEDS_CAP = 8,
    SAVE_STATE_PLANET_ID_MAX = 7
};

typedef enum {
    SAVE_NODE_PLANET,
    SAVE_NODE_PROTOSTAR,
    SAVE_NODE_STRUCTURE,
    SAVE_NODE_PROCESSOR,
    SAVE_NODE_SLIPGATE,
    SAVE_NODE_INFRASPACE_GATE,
    SAVE_NODE_TRADING_HUB,
    SAVE_NODE_SYNTHESIZER,
    SAVE_NODE_ASCENSION,
    SAVE_NODE_KIND_COUNT
} save_node_kind_t;

enum {
    SAVE_NODE_HAS_POS = 1u << 0,
    SAVE_NODE_HAS_MAP_POS = 1u << 1,
    SAVE_NODE_HAS_PLANET_ID = 1u << 2,
    SAVE_NODE_HAS_NEEDS = 1u << 3,
    SAVE_NODE_HAS_DEVELOPMENT_TIER = 1u << 4,
    SAVE_NODE_HAS_PRODUCTION_BONUS = 1u << 5,
    SAVE_NODE_HAS_PRODUCTION = 1u << 6,
    SAVE_NODE_PRODUCTION_HAS_RESOURCE = 1u << 7,
    SAVE_NODE_SYNTHESIZER_SPENT = 1u << 8,
    SAVE_NODE_HAS_OUTGOING_ROUTES = 1u << 9,
    SAVE_NODE_HAS_STATE_TIME = 1u << 10,
    SAVE_NODE_KNOWN_FLAGS = (1u << 11) - 1u
};

typedef struct {
    int32_t x;
    int32_t y;
} save_vec_t;

typedef struct {
    int32_t credits;
    int32_t research_points;
    int32_t era;
    int32_t year;
    int32_t year_ticks;
    int invented_count;
    uint8_t invented[TECHNOLOGIES_COUNT];
} save_economy_t;

typedef struct {
    int tickets_count;
    uint8_t tickets[WEIGHTED_PICKER_TICKETS_CAP];
    uint8_t recent[SAVE_STATE_RECENT_COUNT];
    uint8_t recent_set[SAVE_STATE_RECENT_COUNT];
    int32_t multiplier;
    int32_t walk_step;
    int32_t walk_bias;
} save_type_picker_t;

typedef struct {
    int32_t step;
    int32_t bias;
} save_walk_t;

typedef struct {
    save_vec_t pos;
    save_vec_t map_pos;
    int32_t fold_anchor;
} save_probe_t;

typedef struct {
    uint8_t has_resource;
    int32_t resource;
} save_need_t;

/* planet_id holds up to 8 bytes on encode; a decoded one is NUL-terminated.
   planet_type, description_type and project are 1-based, 0 for none;
   description_type is the type whose description the planet holds, which
   terraform leaves on the old type. The route and slipway
   slices index the state's pools. */
typedef struct {
    uint8_t kind;
    uint8_t state;
    uint16_t flags;
    save_vec_t pos;
    save_vec_t map_pos;
    int32_t fold_anchor;
    int32_t infraspace;
    char planet_id[SAVE_STATE_PLANET_ID_MAX + 1];
    uint8_t planet_type;
    uint8_t description_type;
    uint8_t project;
    int need_count;
    save_need_t needs[SAVE_STATE_NEEDS_CAP];
    int32_t development_tier;
    int32_t production_bonus;
    int32_t production_resource;
    int32_t state_time;
    int first_route;
    int route_count;
    int first_slipway;
    int slipway_count;
} save_node_t;

/* Both slices index route_indices: node indices, then slipway indices. */
typedef struct {
    int first_planet;
    int planet_count;
    int first_slipway;
    int slipway_count;
    uint8_t has_resource;
    uint8_t resource_is_bool;
    int32_t resource;
} save_route_t;

typedef struct {
    uint8_t start;
    uint8_t end;
    int32_t state_time;
} save_slipway_t;

/* Every fixed-point value is its raw 16.16 int32_t; every reference is a
   0-based table index, except difficulty (1-based in difficulty_options_table).
   Tens of KiB: keep it in static storage. */
typedef struct {
    uint8_t difficulty;
    save_economy_t economy;
    uint32_t rnd_state;
    uint32_t background_seed;
    save_type_picker_t type_picker;
    save_walk_t vanish_walk;
    int32_t structure_id_counter;
    uint8_t has_probe;
    save_probe_t probe;
    save_vec_t camera_map_pos;
    int node_count;
    save_node_t nodes[SAVE_STATE_NODES_CAP];
    int slipway_count;
    save_slipway_t slipways[SAVE_STATE_SLIPWAYS_CAP];
    int route_count;
    save_route_t routes[SAVE_STATE_ROUTES_CAP];
    int route_index_count;
    uint8_t route_indices[SAVE_STATE_ROUTE_INDICES_CAP];
    int node_slipway_count;
    uint8_t node_slipways[SAVE_STATE_NODE_SLIPWAYS_CAP];
} save_state_t;

/* Asserts that a game is running. Zeroes *out first: two captures of the same
   game are equal. Reads the live game, changes nothing. */
void save_state_capture(save_state_t *out);

/* Returns the record's byte count; the caller owns buffer. Writes what it is
   given: only save_state_decode validates. */
int save_state_encode(const save_state_t *state, uint8_t buffer[SAVE_STATE_MAX_BYTES]);

/* Returns 1 for a valid record, 0 otherwise, never asserts on the bytes. On 0,
   *out is unspecified. */
int save_state_decode(const uint8_t *bytes, int count, save_state_t *out);

/* CRC-32 IEEE 802.3, reflected: "123456789" gives 0xCBF43926. */
uint32_t save_state_crc32(const uint8_t *bytes, int count);

#endif
