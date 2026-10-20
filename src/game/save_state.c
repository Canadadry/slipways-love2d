#include "game/save_state.h"

#include "engine/entity_class.h"
#include "engine/entity_id.h"
#include "engine/entity_registry.h"
#include "game/background.h"
#include "game/economy_score.h"
#include "game/main_economy.h"
#include "game/planet.h"
#include "game/planet_family.h"
#include "game/planet_family_trade.h"
#include "game/probe.h"
#include "game/research.h"
#include "game/slipway.h"
#include "game/structure_icon.h"
#include "game/ui_button.h"
#include "p8/fixed.h"

#include <assert.h>
#include <string.h>

enum {
    HEADER_BYTES = 8,
    TRAILER_BYTES = 4,
    LENGTH_OFFSET = 6
};

static const uint8_t magic[4] = {'S', 'L', 'W', 'Y'};

/* Writing past SAVE_STATE_MAX_BYTES asserts. Reading past end sets failed and
   returns 0, so a parser checks failed once at the end. */
typedef struct {
    int position;
    int end;
    int failed;
} cursor_t;

static void write_u8(uint8_t *bytes, cursor_t *writer, uint32_t value) {
    assert(writer->position >= 0 && writer->position < SAVE_STATE_MAX_BYTES && "save_state_encode: the record must fit SAVE_STATE_MAX_BYTES, 8 KiB");
    bytes[writer->position++] = (uint8_t)(value & 0xFFu);
}

static void write_u16(uint8_t *bytes, cursor_t *writer, uint32_t value) {
    write_u8(bytes, writer, value);
    write_u8(bytes, writer, value >> 8);
}

static void write_u32(uint8_t *bytes, cursor_t *writer, uint32_t value) {
    write_u16(bytes, writer, value);
    write_u16(bytes, writer, value >> 16);
}

static void write_i32(uint8_t *bytes, cursor_t *writer, int32_t value) {
    write_u32(bytes, writer, (uint32_t)value);
}

static void write_vec(uint8_t *bytes, cursor_t *writer, save_vec_t vec) {
    write_i32(bytes, writer, vec.x);
    write_i32(bytes, writer, vec.y);
}

static uint32_t read_u8(const uint8_t *bytes, cursor_t *reader) {
    if (reader->position >= reader->end) {
        reader->failed = 1;
        return 0;
    }
    return bytes[reader->position++];
}

static uint32_t read_u16(const uint8_t *bytes, cursor_t *reader) {
    uint32_t low = read_u8(bytes, reader);
    return low | (read_u8(bytes, reader) << 8);
}

static uint32_t read_u32(const uint8_t *bytes, cursor_t *reader) {
    uint32_t low = read_u16(bytes, reader);
    return low | (read_u16(bytes, reader) << 16);
}

static int32_t read_i32(const uint8_t *bytes, cursor_t *reader) {
    uint32_t value = read_u32(bytes, reader);
    int32_t result;
    memcpy(&result, &value, sizeof result);
    return result;
}

static save_vec_t read_vec(const uint8_t *bytes, cursor_t *reader) {
    save_vec_t vec;
    vec.x = read_i32(bytes, reader);
    vec.y = read_i32(bytes, reader);
    return vec;
}

static void write_indices(uint8_t *bytes, cursor_t *writer, const save_state_t *state, int first, int count) {
    assert(count >= 0 && count <= 0xFF && first >= 0 && first + count <= state->route_index_count);
    write_u8(bytes, writer, (uint32_t)count);
    for (int i = 0; i < count; i++) {
        write_u8(bytes, writer, state->route_indices[first + i]);
    }
}

static void write_route(uint8_t *bytes, cursor_t *writer, const save_state_t *state, const save_route_t *route) {
    write_indices(bytes, writer, state, route->first_planet, route->planet_count);
    write_indices(bytes, writer, state, route->first_slipway, route->slipway_count);
    write_u8(bytes, writer, (uint32_t)route->has_resource | ((uint32_t)route->resource_is_bool << 1));
    if (route->has_resource) {
        write_i32(bytes, writer, route->resource);
    }
}

static void write_node(uint8_t *bytes, cursor_t *writer, const save_state_t *state, const save_node_t *node) {
    write_u8(bytes, writer, node->kind);
    write_u8(bytes, writer, node->state);
    write_u16(bytes, writer, node->flags);
    if (node->flags & SAVE_NODE_HAS_POS) {
        write_vec(bytes, writer, node->pos);
    }
    if (node->flags & SAVE_NODE_HAS_MAP_POS) {
        write_vec(bytes, writer, node->map_pos);
    }
    if (node->flags & SAVE_NODE_HAS_PLANET_ID) {
        int length = 0;
        while (length < (int)sizeof node->planet_id && node->planet_id[length] != '\0') {
            length++;
        }
        write_u8(bytes, writer, (uint32_t)length);
        for (int i = 0; i < length; i++) {
            write_u8(bytes, writer, (uint8_t)node->planet_id[i]);
        }
    }
    write_i32(bytes, writer, node->fold_anchor);
    write_i32(bytes, writer, node->infraspace);
    write_u8(bytes, writer, node->planet_type);
    write_u8(bytes, writer, node->description_type);
    write_u8(bytes, writer, node->project);
    if (node->flags & SAVE_NODE_HAS_NEEDS) {
        assert(node->need_count >= 0 && node->need_count <= 0xFF);
        write_u8(bytes, writer, (uint32_t)node->need_count);
        for (int i = 0; i < node->need_count && i < SAVE_STATE_NEEDS_CAP; i++) {
            write_u8(bytes, writer, node->needs[i].has_resource);
            if (node->needs[i].has_resource) {
                write_i32(bytes, writer, node->needs[i].resource);
            }
        }
    } else {
        assert(node->need_count == 0 && "save_state_encode: needs without SAVE_NODE_HAS_NEEDS would be dropped");
    }
    if (node->flags & SAVE_NODE_HAS_DEVELOPMENT_TIER) {
        write_i32(bytes, writer, node->development_tier);
    }
    if (node->flags & SAVE_NODE_HAS_PRODUCTION_BONUS) {
        write_i32(bytes, writer, node->production_bonus);
    }
    if (node->flags & SAVE_NODE_PRODUCTION_HAS_RESOURCE) {
        write_i32(bytes, writer, node->production_resource);
    }
    if (node->flags & SAVE_NODE_HAS_STATE_TIME) {
        write_i32(bytes, writer, node->state_time);
    }
    if (node->flags & SAVE_NODE_HAS_OUTGOING_ROUTES) {
        assert(node->route_count >= 0 && node->route_count <= 0xFFFF && node->first_route >= 0 &&
               node->first_route + node->route_count <= state->route_count);
        write_u16(bytes, writer, (uint32_t)node->route_count);
        for (int i = 0; i < node->route_count; i++) {
            write_route(bytes, writer, state, &state->routes[node->first_route + i]);
        }
    } else {
        assert(node->route_count == 0 && "save_state_encode: routes without SAVE_NODE_HAS_OUTGOING_ROUTES would be dropped");
    }
    assert(node->slipway_count >= 0 && node->slipway_count <= 0xFF && node->first_slipway >= 0 &&
           node->first_slipway + node->slipway_count <= state->node_slipway_count);
    write_u8(bytes, writer, (uint32_t)node->slipway_count);
    for (int i = 0; i < node->slipway_count; i++) {
        write_u8(bytes, writer, state->node_slipways[node->first_slipway + i]);
    }
}

static void write_payload(uint8_t *bytes, cursor_t *writer, const save_state_t *state) {
    const save_economy_t *economy = &state->economy;
    const save_type_picker_t *picker = &state->type_picker;

    assert(economy->invented_count >= 0 && economy->invented_count <= TECHNOLOGIES_COUNT);
    assert(picker->tickets_count >= 0 && picker->tickets_count <= WEIGHTED_PICKER_TICKETS_CAP);

    write_u8(bytes, writer, state->difficulty);

    write_i32(bytes, writer, economy->credits);
    write_i32(bytes, writer, economy->research_points);
    write_i32(bytes, writer, economy->era);
    write_i32(bytes, writer, economy->year);
    write_i32(bytes, writer, economy->year_ticks);
    write_u8(bytes, writer, (uint32_t)economy->invented_count);
    for (int i = 0; i < economy->invented_count; i++) {
        write_u8(bytes, writer, economy->invented[i]);
    }

    write_u32(bytes, writer, state->rnd_state);
    write_u32(bytes, writer, state->background_seed);

    write_u8(bytes, writer, (uint32_t)picker->tickets_count);
    for (int i = 0; i < picker->tickets_count; i++) {
        write_u8(bytes, writer, picker->tickets[i]);
    }
    for (int i = 0; i < SAVE_STATE_RECENT_COUNT; i++) {
        write_u8(bytes, writer, picker->recent[i]);
    }
    for (int i = 0; i < SAVE_STATE_RECENT_COUNT; i++) {
        write_u8(bytes, writer, picker->recent_set[i]);
    }
    write_i32(bytes, writer, picker->multiplier);
    write_i32(bytes, writer, picker->walk_step);
    write_i32(bytes, writer, picker->walk_bias);

    write_i32(bytes, writer, state->vanish_walk.step);
    write_i32(bytes, writer, state->vanish_walk.bias);

    write_i32(bytes, writer, state->structure_id_counter);

    write_u8(bytes, writer, state->has_probe);
    if (state->has_probe) {
        write_vec(bytes, writer, state->probe.pos);
        write_vec(bytes, writer, state->probe.map_pos);
        write_i32(bytes, writer, state->probe.fold_anchor);
    }

    write_vec(bytes, writer, state->camera_map_pos);

    assert(state->node_count >= 0 && state->node_count <= SAVE_STATE_NODES_CAP && "save_state_encode: node indices are one byte");
    write_u8(bytes, writer, (uint32_t)state->node_count);
    for (int i = 0; i < state->node_count; i++) {
        write_node(bytes, writer, state, &state->nodes[i]);
    }

    assert(state->slipway_count >= 0 && state->slipway_count <= SAVE_STATE_SLIPWAYS_CAP && "save_state_encode: slipway indices are one byte");
    write_u8(bytes, writer, (uint32_t)state->slipway_count);
    for (int i = 0; i < state->slipway_count; i++) {
        write_u8(bytes, writer, state->slipways[i].start);
        write_u8(bytes, writer, state->slipways[i].end);
        write_i32(bytes, writer, state->slipways[i].state_time);
    }
}

int save_state_encode(const save_state_t *state, uint8_t buffer[SAVE_STATE_MAX_BYTES]) {
    cursor_t writer = {0, SAVE_STATE_MAX_BYTES, 0};
    cursor_t length = {LENGTH_OFFSET, LENGTH_OFFSET + 2, 0};
    for (size_t i = 0; i < sizeof magic; i++) {
        write_u8(buffer, &writer, magic[i]);
    }
    write_u16(buffer, &writer, SAVE_STATE_VERSION);
    write_u16(buffer, &writer, 0);
    write_payload(buffer, &writer, state);
    write_u16(buffer, &length, (uint32_t)(writer.position - HEADER_BYTES));
    write_u32(buffer, &writer, save_state_crc32(buffer, writer.position));
    return writer.position;
}

static void read_economy(const uint8_t *bytes, cursor_t *reader, save_economy_t *economy) {
    economy->credits = read_i32(bytes, reader);
    economy->research_points = read_i32(bytes, reader);
    economy->era = read_i32(bytes, reader);
    economy->year = read_i32(bytes, reader);
    economy->year_ticks = read_i32(bytes, reader);
    economy->invented_count = (int)read_u8(bytes, reader);
    if (economy->invented_count > TECHNOLOGIES_COUNT) {
        reader->failed = 1;
        return;
    }
    for (int i = 0; i < economy->invented_count; i++) {
        economy->invented[i] = (uint8_t)read_u8(bytes, reader);
    }
}

static void read_type_picker(const uint8_t *bytes, cursor_t *reader, save_type_picker_t *picker) {
    picker->tickets_count = (int)read_u8(bytes, reader);
    if (picker->tickets_count > WEIGHTED_PICKER_TICKETS_CAP) {
        reader->failed = 1;
        return;
    }
    for (int i = 0; i < picker->tickets_count; i++) {
        picker->tickets[i] = (uint8_t)read_u8(bytes, reader);
    }
    for (int i = 0; i < SAVE_STATE_RECENT_COUNT; i++) {
        picker->recent[i] = (uint8_t)read_u8(bytes, reader);
    }
    for (int i = 0; i < SAVE_STATE_RECENT_COUNT; i++) {
        picker->recent_set[i] = (uint8_t)read_u8(bytes, reader);
    }
    picker->multiplier = read_i32(bytes, reader);
    picker->walk_step = read_i32(bytes, reader);
    picker->walk_bias = read_i32(bytes, reader);
}

/* Returns the slice's first index into route_indices. */
static int read_indices(const uint8_t *bytes, cursor_t *reader, save_state_t *out, int *count) {
    int first = out->route_index_count;
    *count = (int)read_u8(bytes, reader);
    if (first + *count > SAVE_STATE_ROUTE_INDICES_CAP) {
        reader->failed = 1;
        *count = 0;
        return first;
    }
    for (int i = 0; i < *count; i++) {
        out->route_indices[out->route_index_count++] = (uint8_t)read_u8(bytes, reader);
    }
    return first;
}

static void read_route(const uint8_t *bytes, cursor_t *reader, save_state_t *out, save_route_t *route) {
    uint32_t resource_flags;
    route->first_planet = read_indices(bytes, reader, out, &route->planet_count);
    route->first_slipway = read_indices(bytes, reader, out, &route->slipway_count);
    resource_flags = read_u8(bytes, reader);
    if (resource_flags & ~3u) {
        reader->failed = 1;
        return;
    }
    route->has_resource = (uint8_t)(resource_flags & 1u);
    route->resource_is_bool = (uint8_t)((resource_flags >> 1) & 1u);
    if (route->has_resource) {
        route->resource = read_i32(bytes, reader);
    }
}

static void read_node(const uint8_t *bytes, cursor_t *reader, save_state_t *out, save_node_t *node) {
    node->kind = (uint8_t)read_u8(bytes, reader);
    node->state = (uint8_t)read_u8(bytes, reader);
    node->flags = (uint16_t)read_u16(bytes, reader);
    if (node->flags & SAVE_NODE_HAS_POS) {
        node->pos = read_vec(bytes, reader);
    }
    if (node->flags & SAVE_NODE_HAS_MAP_POS) {
        node->map_pos = read_vec(bytes, reader);
    }
    if (node->flags & SAVE_NODE_HAS_PLANET_ID) {
        int length = (int)read_u8(bytes, reader);
        if (length == 0 || length > SAVE_STATE_PLANET_ID_MAX) {
            reader->failed = 1;
            return;
        }
        for (int i = 0; i < length; i++) {
            uint32_t character = read_u8(bytes, reader);
            if (character < 0x21 || character > 0x7E) {
                reader->failed = 1;
                return;
            }
            node->planet_id[i] = (char)character;
        }
    }
    node->fold_anchor = read_i32(bytes, reader);
    node->infraspace = read_i32(bytes, reader);
    node->planet_type = (uint8_t)read_u8(bytes, reader);
    node->description_type = (uint8_t)read_u8(bytes, reader);
    node->project = (uint8_t)read_u8(bytes, reader);
    if (node->flags & SAVE_NODE_HAS_NEEDS) {
        node->need_count = (int)read_u8(bytes, reader);
        if (node->need_count > SAVE_STATE_NEEDS_CAP) {
            reader->failed = 1;
            return;
        }
        for (int i = 0; i < node->need_count; i++) {
            node->needs[i].has_resource = (uint8_t)read_u8(bytes, reader);
            if (node->needs[i].has_resource) {
                node->needs[i].resource = read_i32(bytes, reader);
            }
        }
    }
    if (node->flags & SAVE_NODE_HAS_DEVELOPMENT_TIER) {
        node->development_tier = read_i32(bytes, reader);
    }
    if (node->flags & SAVE_NODE_HAS_PRODUCTION_BONUS) {
        node->production_bonus = read_i32(bytes, reader);
    }
    if (node->flags & SAVE_NODE_PRODUCTION_HAS_RESOURCE) {
        node->production_resource = read_i32(bytes, reader);
    }
    if (node->flags & SAVE_NODE_HAS_STATE_TIME) {
        node->state_time = read_i32(bytes, reader);
    }
    node->first_route = out->route_count;
    if (node->flags & SAVE_NODE_HAS_OUTGOING_ROUTES) {
        node->route_count = (int)read_u16(bytes, reader);
        if (node->first_route + node->route_count > SAVE_STATE_ROUTES_CAP) {
            reader->failed = 1;
            return;
        }
        for (int i = 0; i < node->route_count && !reader->failed; i++) {
            read_route(bytes, reader, out, &out->routes[out->route_count++]);
        }
    }
    node->first_slipway = out->node_slipway_count;
    node->slipway_count = (int)read_u8(bytes, reader);
    if (node->first_slipway + node->slipway_count > SAVE_STATE_NODE_SLIPWAYS_CAP) {
        reader->failed = 1;
        return;
    }
    for (int i = 0; i < node->slipway_count; i++) {
        out->node_slipways[out->node_slipway_count++] = (uint8_t)read_u8(bytes, reader);
    }
}

static void read_payload(const uint8_t *bytes, cursor_t *reader, save_state_t *out) {
    out->difficulty = (uint8_t)read_u8(bytes, reader);
    read_economy(bytes, reader, &out->economy);
    if (reader->failed) {
        return;
    }
    out->rnd_state = read_u32(bytes, reader);
    out->background_seed = read_u32(bytes, reader);
    read_type_picker(bytes, reader, &out->type_picker);
    if (reader->failed) {
        return;
    }
    out->vanish_walk.step = read_i32(bytes, reader);
    out->vanish_walk.bias = read_i32(bytes, reader);
    out->structure_id_counter = read_i32(bytes, reader);
    out->has_probe = (uint8_t)read_u8(bytes, reader);
    if (out->has_probe) {
        out->probe.pos = read_vec(bytes, reader);
        out->probe.map_pos = read_vec(bytes, reader);
        out->probe.fold_anchor = read_i32(bytes, reader);
    }
    out->camera_map_pos = read_vec(bytes, reader);
    out->node_count = (int)read_u8(bytes, reader);
    for (int i = 0; i < out->node_count && !reader->failed; i++) {
        read_node(bytes, reader, out, &out->nodes[i]);
    }
    out->slipway_count = (int)read_u8(bytes, reader);
    for (int i = 0; i < out->slipway_count; i++) {
        out->slipways[i].start = (uint8_t)read_u8(bytes, reader);
        out->slipways[i].end = (uint8_t)read_u8(bytes, reader);
        out->slipways[i].state_time = read_i32(bytes, reader);
    }
}

static int is_flag(uint8_t value) {
    return value == 0 || value == 1;
}

static int economy_is_valid(const save_economy_t *economy) {
    int seen[TECHNOLOGIES_COUNT] = {0};
    for (int i = 0; i < economy->invented_count; i++) {
        uint8_t tech = economy->invented[i];
        if (tech >= TECHNOLOGIES_COUNT || seen[tech]) {
            return 0;
        }
        seen[tech] = 1;
    }
    return 1;
}

static int type_picker_is_valid(const save_type_picker_t *picker) {
    for (int i = 0; i < picker->tickets_count; i++) {
        if (picker->tickets[i] >= PLANET_TYPES_COUNT) {
            return 0;
        }
    }
    for (int i = 0; i < SAVE_STATE_RECENT_COUNT; i++) {
        if (!is_flag(picker->recent_set[i]) || (picker->recent_set[i] && picker->recent[i] >= PLANET_TYPES_COUNT)) {
            return 0;
        }
    }
    return 1;
}

static int node_state_is_valid(uint8_t state) {
    return state != ENTITY_STATE_NONE && state != ENTITY_STATE_PLACING && state != ENTITY_STATE_UNFOLDING && state < STATE_COUNT;
}

static int has(const save_node_t *node, unsigned flag) {
    return (node->flags & flag) != 0;
}

/* A planet reads its clock only while vanishing; every other kind animates with it. */
static int node_has_state_time(uint8_t kind, uint8_t state) {
    return state == ENTITY_STATE_VANISHING || kind != SAVE_NODE_PLANET;
}

static int node_flags_are_valid(const save_node_t *node) {
    if (node->flags & ~(unsigned)SAVE_NODE_KNOWN_FLAGS) {
        return 0;
    }
    if (has(node, SAVE_NODE_PRODUCTION_HAS_RESOURCE) && !has(node, SAVE_NODE_HAS_PRODUCTION)) {
        return 0;
    }
    if (has(node, SAVE_NODE_SYNTHESIZER_SPENT) && node->kind != SAVE_NODE_SYNTHESIZER) {
        return 0;
    }
    return has(node, SAVE_NODE_HAS_STATE_TIME) == node_has_state_time(node->kind, node->state);
}

static int is_resource_code(int32_t resource) {
    for (int i = 0; i < RESOURCE_CODE_COUNT; i++) {
        if (resource == (int32_t)((uint32_t)resource_codes[i].resource << 16)) {
            return 1;
        }
    }
    return 0;
}

static int resources_are_valid(const save_node_t *node) {
    for (int i = 0; i < node->need_count; i++) {
        const save_need_t *need = &node->needs[i];
        if (!is_flag(need->has_resource) || (need->has_resource && !is_resource_code(need->resource))) {
            return 0;
        }
    }
    return !has(node, SAVE_NODE_PRODUCTION_HAS_RESOURCE) || is_resource_code(node->production_resource);
}

static int node_is_valid(const save_node_t *node) {
    return node->kind < SAVE_NODE_KIND_COUNT && node_state_is_valid(node->state) && node_flags_are_valid(node) &&
           node->planet_type <= PLANET_TYPES_COUNT && node->description_type <= PLANET_TYPES_COUNT && node->project <= PROJECT_RECORDS_COUNT &&
           resources_are_valid(node);
}

static int nodes_are_valid(const save_state_t *state) {
    for (int i = 0; i < state->node_count; i++) {
        if (!node_is_valid(&state->nodes[i])) {
            return 0;
        }
    }
    return 1;
}

static int indices_are_below(const save_state_t *state, int first, int count, int limit) {
    for (int i = 0; i < count; i++) {
        if (state->route_indices[first + i] >= limit) {
            return 0;
        }
    }
    return 1;
}

static int routes_are_valid(const save_state_t *state) {
    for (int i = 0; i < state->route_count; i++) {
        const save_route_t *route = &state->routes[i];
        if (!indices_are_below(state, route->first_planet, route->planet_count, state->node_count) ||
            !indices_are_below(state, route->first_slipway, route->slipway_count, state->slipway_count)) {
            return 0;
        }
    }
    return 1;
}

static int slipways_are_valid(const save_state_t *state) {
    for (int i = 0; i < state->slipway_count; i++) {
        const save_slipway_t *slipway = &state->slipways[i];
        if (slipway->start >= state->node_count || slipway->end >= state->node_count || slipway->start == slipway->end) {
            return 0;
        }
    }
    return 1;
}

static int node_slipways_are_valid(const save_state_t *state) {
    for (int i = 0; i < state->node_count; i++) {
        const save_node_t *node = &state->nodes[i];
        uint8_t seen[SAVE_STATE_SLIPWAYS_CAP] = {0};
        for (int k = 0; k < node->slipway_count; k++) {
            uint8_t index = state->node_slipways[node->first_slipway + k];
            if (index >= state->slipway_count || seen[index] ||
                (state->slipways[index].start != i && state->slipways[index].end != i)) {
                return 0;
            }
            seen[index] = 1;
        }
    }
    return 1;
}

static int state_is_valid(const save_state_t *state) {
    return state->difficulty >= 1 && state->difficulty <= DIFFICULTY_OPTIONS_COUNT && economy_is_valid(&state->economy) &&
           type_picker_is_valid(&state->type_picker) && is_flag(state->has_probe) && nodes_are_valid(state) && slipways_are_valid(state) && routes_are_valid(state) &&
           node_slipways_are_valid(state);
}

int save_state_decode(const uint8_t *bytes, int count, save_state_t *out) {
    cursor_t reader = {sizeof magic, count, 0};
    cursor_t trailer = {count - TRAILER_BYTES, count, 0};
    if (count < HEADER_BYTES + TRAILER_BYTES || count > SAVE_STATE_MAX_BYTES) {
        return 0;
    }
    if (memcmp(bytes, magic, sizeof magic) != 0) {
        return 0;
    }
    if (read_u16(bytes, &reader) != SAVE_STATE_VERSION) {
        return 0;
    }
    if ((int)read_u16(bytes, &reader) != count - HEADER_BYTES - TRAILER_BYTES) {
        return 0;
    }
    reader.end = count - TRAILER_BYTES;
    if (read_u32(bytes, &trailer) != save_state_crc32(bytes, reader.end)) {
        return 0;
    }
    memset(out, 0, sizeof *out);
    read_payload(bytes, &reader, out);
    return !reader.failed && reader.position == reader.end && state_is_valid(out);
}

static const uint32_t crc32_table[256] = {
    0x00000000u, 0x77073096u, 0xEE0E612Cu, 0x990951BAu, 0x076DC419u, 0x706AF48Fu,
    0xE963A535u, 0x9E6495A3u, 0x0EDB8832u, 0x79DCB8A4u, 0xE0D5E91Eu, 0x97D2D988u,
    0x09B64C2Bu, 0x7EB17CBDu, 0xE7B82D07u, 0x90BF1D91u, 0x1DB71064u, 0x6AB020F2u,
    0xF3B97148u, 0x84BE41DEu, 0x1ADAD47Du, 0x6DDDE4EBu, 0xF4D4B551u, 0x83D385C7u,
    0x136C9856u, 0x646BA8C0u, 0xFD62F97Au, 0x8A65C9ECu, 0x14015C4Fu, 0x63066CD9u,
    0xFA0F3D63u, 0x8D080DF5u, 0x3B6E20C8u, 0x4C69105Eu, 0xD56041E4u, 0xA2677172u,
    0x3C03E4D1u, 0x4B04D447u, 0xD20D85FDu, 0xA50AB56Bu, 0x35B5A8FAu, 0x42B2986Cu,
    0xDBBBC9D6u, 0xACBCF940u, 0x32D86CE3u, 0x45DF5C75u, 0xDCD60DCFu, 0xABD13D59u,
    0x26D930ACu, 0x51DE003Au, 0xC8D75180u, 0xBFD06116u, 0x21B4F4B5u, 0x56B3C423u,
    0xCFBA9599u, 0xB8BDA50Fu, 0x2802B89Eu, 0x5F058808u, 0xC60CD9B2u, 0xB10BE924u,
    0x2F6F7C87u, 0x58684C11u, 0xC1611DABu, 0xB6662D3Du, 0x76DC4190u, 0x01DB7106u,
    0x98D220BCu, 0xEFD5102Au, 0x71B18589u, 0x06B6B51Fu, 0x9FBFE4A5u, 0xE8B8D433u,
    0x7807C9A2u, 0x0F00F934u, 0x9609A88Eu, 0xE10E9818u, 0x7F6A0DBBu, 0x086D3D2Du,
    0x91646C97u, 0xE6635C01u, 0x6B6B51F4u, 0x1C6C6162u, 0x856530D8u, 0xF262004Eu,
    0x6C0695EDu, 0x1B01A57Bu, 0x8208F4C1u, 0xF50FC457u, 0x65B0D9C6u, 0x12B7E950u,
    0x8BBEB8EAu, 0xFCB9887Cu, 0x62DD1DDFu, 0x15DA2D49u, 0x8CD37CF3u, 0xFBD44C65u,
    0x4DB26158u, 0x3AB551CEu, 0xA3BC0074u, 0xD4BB30E2u, 0x4ADFA541u, 0x3DD895D7u,
    0xA4D1C46Du, 0xD3D6F4FBu, 0x4369E96Au, 0x346ED9FCu, 0xAD678846u, 0xDA60B8D0u,
    0x44042D73u, 0x33031DE5u, 0xAA0A4C5Fu, 0xDD0D7CC9u, 0x5005713Cu, 0x270241AAu,
    0xBE0B1010u, 0xC90C2086u, 0x5768B525u, 0x206F85B3u, 0xB966D409u, 0xCE61E49Fu,
    0x5EDEF90Eu, 0x29D9C998u, 0xB0D09822u, 0xC7D7A8B4u, 0x59B33D17u, 0x2EB40D81u,
    0xB7BD5C3Bu, 0xC0BA6CADu, 0xEDB88320u, 0x9ABFB3B6u, 0x03B6E20Cu, 0x74B1D29Au,
    0xEAD54739u, 0x9DD277AFu, 0x04DB2615u, 0x73DC1683u, 0xE3630B12u, 0x94643B84u,
    0x0D6D6A3Eu, 0x7A6A5AA8u, 0xE40ECF0Bu, 0x9309FF9Du, 0x0A00AE27u, 0x7D079EB1u,
    0xF00F9344u, 0x8708A3D2u, 0x1E01F268u, 0x6906C2FEu, 0xF762575Du, 0x806567CBu,
    0x196C3671u, 0x6E6B06E7u, 0xFED41B76u, 0x89D32BE0u, 0x10DA7A5Au, 0x67DD4ACCu,
    0xF9B9DF6Fu, 0x8EBEEFF9u, 0x17B7BE43u, 0x60B08ED5u, 0xD6D6A3E8u, 0xA1D1937Eu,
    0x38D8C2C4u, 0x4FDFF252u, 0xD1BB67F1u, 0xA6BC5767u, 0x3FB506DDu, 0x48B2364Bu,
    0xD80D2BDAu, 0xAF0A1B4Cu, 0x36034AF6u, 0x41047A60u, 0xDF60EFC3u, 0xA867DF55u,
    0x316E8EEFu, 0x4669BE79u, 0xCB61B38Cu, 0xBC66831Au, 0x256FD2A0u, 0x5268E236u,
    0xCC0C7795u, 0xBB0B4703u, 0x220216B9u, 0x5505262Fu, 0xC5BA3BBEu, 0xB2BD0B28u,
    0x2BB45A92u, 0x5CB36A04u, 0xC2D7FFA7u, 0xB5D0CF31u, 0x2CD99E8Bu, 0x5BDEAE1Du,
    0x9B64C2B0u, 0xEC63F226u, 0x756AA39Cu, 0x026D930Au, 0x9C0906A9u, 0xEB0E363Fu,
    0x72076785u, 0x05005713u, 0x95BF4A82u, 0xE2B87A14u, 0x7BB12BAEu, 0x0CB61B38u,
    0x92D28E9Bu, 0xE5D5BE0Du, 0x7CDCEFB7u, 0x0BDBDF21u, 0x86D3D2D4u, 0xF1D4E242u,
    0x68DDB3F8u, 0x1FDA836Eu, 0x81BE16CDu, 0xF6B9265Bu, 0x6FB077E1u, 0x18B74777u,
    0x88085AE6u, 0xFF0F6A70u, 0x66063BCAu, 0x11010B5Cu, 0x8F659EFFu, 0xF862AE69u,
    0x616BFFD3u, 0x166CCF45u, 0xA00AE278u, 0xD70DD2EEu, 0x4E048354u, 0x3903B3C2u,
    0xA7672661u, 0xD06016F7u, 0x4969474Du, 0x3E6E77DBu, 0xAED16A4Au, 0xD9D65ADCu,
    0x40DF0B66u, 0x37D83BF0u, 0xA9BCAE53u, 0xDEBB9EC5u, 0x47B2CF7Fu, 0x30B5FFE9u,
    0xBDBDF21Cu, 0xCABAC28Au, 0x53B39330u, 0x24B4A3A6u, 0xBAD03605u, 0xCDD70693u,
    0x54DE5729u, 0x23D967BFu, 0xB3667A2Eu, 0xC4614AB8u, 0x5D681B02u, 0x2A6F2B94u,
    0xB40BBE37u, 0xC30C8EA1u, 0x5A05DF1Bu, 0x2D02EF8Du,
};

uint32_t save_state_crc32(const uint8_t *bytes, int count) {
    uint32_t crc = 0xFFFFFFFFu;
    for (int i = 0; i < count; i++) {
        crc = crc32_table[(crc ^ bytes[i]) & 0xFFu] ^ (crc >> 8);
    }
    return crc ^ 0xFFFFFFFFu;
}

static save_vec_t vec_of(vec_t vec) {
    save_vec_t out;
    out.x = vec.x;
    out.y = vec.y;
    return out;
}

static uint8_t difficulty_index(void) {
    const button_t *difficulty = economy_score_difficulty();
    assert(difficulty != NULL && difficulty->name != NULL && "save_state_capture: a running game has a difficulty");
    for (int i = 0; i < DIFFICULTY_OPTIONS_COUNT; i++) {
        if (strcmp(difficulty_options_table[i].name, difficulty->name) == 0) {
            return (uint8_t)(i + 1);
        }
    }
    assert(0 && "save_state_capture: the difficulty is not a difficulty_options_table entry");
    return 0;
}

static uint8_t technology_index(const char *name) {
    for (int i = 0; i < TECHNOLOGIES_COUNT; i++) {
        if (strcmp(technologies_table[i].name, name) == 0) {
            return (uint8_t)i;
        }
    }
    assert(0 && "save_state_capture: an invented tech is not a technologies_table entry");
    return 0;
}

static uint8_t planet_type_index(const planet_types_entry_t *type) {
    assert(type >= planet_types_table && type < planet_types_table + PLANET_TYPES_COUNT &&
           "save_state_capture: a planet type is not a planet_types_table entry");
    return (uint8_t)(type - planet_types_table);
}

/* The 1-based planet_types_table entry that owns the description pointer
   (the current type first, terraform leaves the old one's); 0 for NULL. */
static uint8_t description_type_index(const planet_t *planet) {
    if (planet->description == NULL) {
        return 0;
    }
    if (planet->planet_type != NULL && planet->description == planet->planet_type->description) {
        return (uint8_t)(planet_type_index(planet->planet_type) + 1);
    }
    for (int i = 0; i < PLANET_TYPES_COUNT; i++) {
        if (planet->description == planet_types_table[i].description) {
            return (uint8_t)(i + 1);
        }
    }
    assert(0 && "save_state_capture: a planet description is not a planet_types_table entry's");
    return 0;
}

static void capture_economy(save_economy_t *out) {
    const economy_t *economy = main_economy_typed();
    assert(economy != NULL && "save_state_capture: a running game has an economy");
    assert(economy->invented.count <= TECHNOLOGIES_COUNT);
    out->credits = economy->credits;
    out->research_points = economy->research_points;
    out->era = economy->era;
    out->year = economy->year;
    out->year_ticks = economy->year_ticks;
    out->invented_count = economy->invented.count;
    for (int i = 0; i < economy->invented.count; i++) {
        out->invented[i] = technology_index((const char *)economy->invented.items[i]);
    }
}

static void capture_type_picker(save_type_picker_t *out) {
    const weighted_picker_t *picker = planet_type_picker();
    const random_walk_t *walk;
    assert(picker != NULL && "save_state_capture: a running game has a type picker");
    walk = weighted_picker_walk(picker);
    assert(walk != NULL && "save_state_capture: the type picker draws from a random walk");
    out->tickets_count = picker->tickets_count;
    for (int i = 0; i < picker->tickets_count; i++) {
        out->tickets[i] = planet_type_index(picker->tickets[i]);
    }
    for (int i = 0; i < SAVE_STATE_RECENT_COUNT; i++) {
        out->recent_set[i] = picker->recent_set[i] ? 1 : 0;
        out->recent[i] = picker->recent_set[i] ? planet_type_index(picker->recent[i]) : 0;
    }
    out->multiplier = picker->multiplier;
    out->walk_step = walk->step;
    out->walk_bias = walk->bias;
}

/* The tag's entities in registration order; 0 entities when the tag has no list. */
static int tagged_count(tag_t tag) {
    const entity_list_t *list = entities_by_tag(tag);
    return list != NULL ? list->count : 0;
}

static entity_t *tagged_at(tag_t tag, int index) {
    return entities_by_tag(tag)->items[index];
}

static void capture_probe(save_state_t *out) {
    for (int i = 0; i < tagged_count(TAG_PROBE); i++) {
        const entity_t *e = tagged_at(TAG_PROBE, i);
        if (e->class != probe_class() || e->is_dead || e->state != ENTITY_STATE_SETTLED) {
            continue;
        }
        out->has_probe = 1;
        out->probe.pos = vec_of(entity_pos(e));
        out->probe.map_pos = vec_of(entity_map_pos(e));
        out->probe.fold_anchor = e->fold_anchor;
        return;
    }
}

static int is_saved(const entity_t *e) {
    return !e->is_dead && e->state != ENTITY_STATE_PLACING;
}

static uint8_t node_kind(const entity_class_t *cls) {
    const entity_class_t *classes[SAVE_NODE_KIND_COUNT];
    classes[SAVE_NODE_PLANET] = planet_class();
    classes[SAVE_NODE_PROTOSTAR] = protostar_class();
    classes[SAVE_NODE_STRUCTURE] = structure_class();
    classes[SAVE_NODE_PROCESSOR] = processor_class();
    classes[SAVE_NODE_SLIPGATE] = slipgate_class();
    classes[SAVE_NODE_INFRASPACE_GATE] = infraspace_gate_class();
    classes[SAVE_NODE_TRADING_HUB] = trading_hub_class();
    classes[SAVE_NODE_SYNTHESIZER] = synthesizer_class();
    classes[SAVE_NODE_ASCENSION] = ascension_class();
    for (int i = 0; i < SAVE_NODE_KIND_COUNT; i++) {
        if (cls == classes[i]) {
            return (uint8_t)i;
        }
    }
    assert(0 && "save_state_capture: a TAG_NODE entity is not one of the nine node classes");
    return 0;
}

static uint8_t project_index(research_project_id_t id) {
    const research_project_t *project = research_project_get(id);
    if (project == NULL) {
        return 0;
    }
    for (int k = 1; k <= research_projects_count(); k++) {
        if (research_projects_at(k) == project) {
            return (uint8_t)k;
        }
    }
    assert(0 && "save_state_capture: a planet's project is not in the research project table");
    return 0;
}

/* The ids of the captured nodes and slipways, in save order. */
typedef struct {
    entity_id_t nodes[SAVE_STATE_NODES_CAP];
    int node_count;
    entity_id_t slipways[SAVE_STATE_SLIPWAYS_CAP];
    int slipway_count;
} captured_ids_t;

static uint8_t index_of(const entity_id_t *ids, int count, entity_id_t id) {
    for (int i = 0; i < count; i++) {
        if (entity_id_equal(ids[i], id)) {
            return (uint8_t)i;
        }
    }
    assert(0 && "save_state_capture: a route, slipway or slipway list names an entity the save did not capture");
    return 0;
}

static void capture_node_fields(save_node_t *node, const planet_t *planet) {
    const entity_t *header = &planet->header;
    const char *id = planet_id_or_null(planet);

    node->kind = node_kind(header->class);
    node->state = (uint8_t)header->state;
    if (entity_has_pos(header)) {
        node->flags |= SAVE_NODE_HAS_POS;
        node->pos = vec_of(header->pos);
    }
    if (entity_has_map_pos(header)) {
        node->flags |= SAVE_NODE_HAS_MAP_POS;
        node->map_pos = vec_of(header->map_pos);
    }
    if (id != NULL) {
        assert(strlen(id) <= SAVE_STATE_PLANET_ID_MAX);
        node->flags |= SAVE_NODE_HAS_PLANET_ID;
        memcpy(node->planet_id, id, strlen(id));
    }
    node->fold_anchor = header->fold_anchor;
    node->infraspace = header->infraspace;
    if (planet->planet_type != NULL) {
        node->planet_type = (uint8_t)(planet_type_index(planet->planet_type) + 1);
    }
    node->description_type = description_type_index(planet);
    node->project = project_index(planet->project);
    if (planet->has & PLANET_HAS_NEEDS) {
        assert(planet->needs.count <= SAVE_STATE_NEEDS_CAP);
        node->flags |= SAVE_NODE_HAS_NEEDS;
        node->need_count = planet->needs.count;
        for (int i = 0; i < planet->needs.count; i++) {
            const need_t *need = planet->needs.items[i];
            assert(need != NULL);
            node->needs[i].has_resource = need->row.has_resource ? 1 : 0;
            node->needs[i].resource = need->row.has_resource ? need->row.resource : 0;
        }
    }
    if (planet->has_development_tier) {
        node->flags |= SAVE_NODE_HAS_DEVELOPMENT_TIER;
        node->development_tier = planet->development_tier;
    }
    if (planet->has_production_bonus) {
        node->flags |= SAVE_NODE_HAS_PRODUCTION_BONUS;
        node->production_bonus = planet->production_bonus;
    }
    if (planet->has & PLANET_HAS_PRODUCTION) {
        node->flags |= SAVE_NODE_HAS_PRODUCTION;
        if (planet->production.has_resource) {
            node->flags |= SAVE_NODE_PRODUCTION_HAS_RESOURCE;
            node->production_resource = planet->production.resource;
        }
    }
    if (planet->synthesizable_resources != planet->archetype->synthesizable_resources) {
        node->flags |= SAVE_NODE_SYNTHESIZER_SPENT;
    }
    if (node_has_state_time(node->kind, node->state)) {
        node->flags |= SAVE_NODE_HAS_STATE_TIME;
        node->state_time = header->state_time;
    }
}

static int add_route_indices(save_state_t *out, const entity_id_list_t *list, const entity_id_t *ids, int count, int *added) {
    int first = out->route_index_count;
    assert(list->count <= 0xFF && first + list->count <= SAVE_STATE_ROUTE_INDICES_CAP);
    for (int i = 0; i < list->count; i++) {
        out->route_indices[out->route_index_count++] = index_of(ids, count, list->items[i]);
    }
    *added = list->count;
    return first;
}

static void capture_node_links(save_state_t *out, save_node_t *node, const planet_t *planet, const captured_ids_t *ids) {
    node->first_route = out->route_count;
    if (planet->has & PLANET_HAS_OUTGOING_ROUTES) {
        int count = trade_route_list_count(&planet->outgoing_routes);
        node->flags |= SAVE_NODE_HAS_OUTGOING_ROUTES;
        assert(out->route_count + count <= SAVE_STATE_ROUTES_CAP);
        for (int i = 0; i < count; i++) {
            const trade_route_t *route = trade_route_list_at(&planet->outgoing_routes, i);
            save_route_t *saved = &out->routes[out->route_count++];
            saved->first_planet = add_route_indices(out, &route->planets, ids->nodes, ids->node_count, &saved->planet_count);
            saved->first_slipway = add_route_indices(out, &route->slipways, ids->slipways, ids->slipway_count, &saved->slipway_count);
            saved->has_resource = route->has_resource ? 1 : 0;
            saved->resource_is_bool = route->resource_is_bool ? 1 : 0;
            saved->resource = route->has_resource ? route->resource : 0;
        }
        node->route_count = count;
    }
    node->first_slipway = out->node_slipway_count;
    assert(planet->slipways.count <= 0xFF && out->node_slipway_count + planet->slipways.count <= SAVE_STATE_NODE_SLIPWAYS_CAP);
    for (int i = 0; i < planet->slipways.count; i++) {
        out->node_slipways[out->node_slipway_count++] = index_of(ids->slipways, ids->slipway_count, planet->slipways.items[i]);
    }
    node->slipway_count = planet->slipways.count;
}

static void capture_world(save_state_t *out, captured_ids_t *ids) {
    for (int i = 0; i < tagged_count(TAG_NODE); i++) {
        const entity_t *e = tagged_at(TAG_NODE, i);
        if (is_saved(e)) {
            assert(ids->node_count < SAVE_STATE_NODES_CAP && "save_state_capture: node indices are one byte");
            ids->nodes[ids->node_count++] = entity_id_link(e);
        }
    }
    for (int i = 0; i < tagged_count(TAG_SLIPWAY); i++) {
        const entity_t *e = tagged_at(TAG_SLIPWAY, i);
        const slipway_t *slipway = (const slipway_t *)e;
        save_slipway_t *saved;
        if (!is_saved(e) || slipway->end_is_point) {
            continue;
        }
        assert(ids->slipway_count < SAVE_STATE_SLIPWAYS_CAP && "save_state_capture: slipway indices are one byte");
        ids->slipways[ids->slipway_count++] = entity_id_link(e);
        saved = &out->slipways[out->slipway_count++];
        saved->start = index_of(ids->nodes, ids->node_count, slipway->start);
        saved->end = index_of(ids->nodes, ids->node_count, slipway->end);
        saved->state_time = e->state_time;
    }
    for (int i = 0; i < ids->node_count; i++) {
        const planet_t *planet = (const planet_t *)entity_get(ids->nodes[i]);
        save_node_t *node = &out->nodes[out->node_count++];
        capture_node_fields(node, planet);
        capture_node_links(out, node, planet, ids);
    }
}

void save_state_capture(save_state_t *out) {
    captured_ids_t ids;
    const random_walk_t *vanish_walk = planet_vanish_roll();
    const background_t *background = background_live();
    p8_fixed_t counter;

    assert(vanish_walk != NULL && "save_state_capture: a running game has a vanish walk");
    assert(background != NULL && "save_state_capture: a running game has a background");
    memset(out, 0, sizeof *out);
    memset(&ids, 0, sizeof ids);
    out->difficulty = difficulty_index();
    capture_economy(&out->economy);
    out->rnd_state = p8_fixed_rnd_state();
    out->background_seed = background->seed;
    capture_type_picker(&out->type_picker);
    out->vanish_walk.step = vanish_walk->step;
    out->vanish_walk.bias = vanish_walk->bias;
    out->structure_id_counter = structure_icon_id_counter(&counter) ? counter : 0;
    capture_probe(out);
    out->camera_map_pos = vec_of(main_camera()->map_pos);
    capture_world(out, &ids);
}
