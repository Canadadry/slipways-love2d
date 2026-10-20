#include "game/economy_formulas.h"

#include <assert.h>

#include "engine/entity_registry.h"
#include "game/game_data.h"
#include "game/map_tables.h"
#include "engine/vec.h"

static p8_fixed_t fixed_max(p8_fixed_t a, p8_fixed_t b) {
    return (a > b) ? a : b;
}

int resource_map_get(const resource_map_t *map, p8_fixed_t qe, p8_fixed_t *out) {
    int key = p8_fixed_flr_int(qe);
    int i;

    for (i = 0; map != NULL && i < map->count; i++) {
        if (map->entries[i].resource == key) {
            *out = map->entries[i].value;
            return 1;
        }
    }
    return 0;
}

void resource_map_set(resource_map_t *map, p8_fixed_t qe, p8_fixed_t v) {
    int key = p8_fixed_flr_int(qe);
    int i;

    for (i = 0; i < map->count; i++) {
        if (map->entries[i].resource == key) {
            map->entries[i].value = v;
            return;
        }
    }
    assert(map->count < RESOURCE_MAP_CAP &&
           "resource_map_set: a resource key outside the 13 resources and the 3 spares -- stop, do not widen");
    map->entries[map->count].resource = key;
    map->entries[map->count].value = v;
    map->count++;
}

void economy_formulas_fill_resource_map(resource_map_t *out, p8_fixed_t jm) {
    int i;

    out->count = 0;
    for (i = 0; i < RESOURCE_CODE_COUNT; i++) {
        resource_map_set(out, p8_fixed_from_int(resource_codes[i].resource), jm);
    }
}

p8_fixed_t economy_formulas_route_base_cost(vec_t from_pos, vec_t to_pos) {
    vec_t diff = vec_sub(to_pos, from_pos);
    p8_fixed_t length_squared = vec_length_squared(diff);
    p8_fixed_t x = p8_fixed_div(length_squared, game_data_route_length_factor());
    p8_fixed_t ec = p8_fixed_pow075(x);
    p8_fixed_t clamped = fixed_max(P8_FIXED_LIT(0x00008000, 0.5), ec);
    p8_fixed_t scale = p8_fixed_mul(game_data_route_cost_factor(), economy_formulas_cost_scale());
    p8_fixed_t cost = p8_fixed_flr(p8_fixed_mul(scale, clamped));
    return cost;
}

p8_fixed_t economy_formulas_cost_scale(void) {
    const entity_list_t *jc = entities_by_tag(TAG_SLIPWAY);
    int count = (jc != NULL) ? jc->count : 0;
    p8_fixed_t sq = p8_fixed_sqrt(p8_fixed_from_int(count));
    p8_fixed_t val = p8_fixed_sub(p8_fixed_mul(sq, P8_FIXED_LIT(0x0000451e, 0.27)), P8_FIXED_LIT(0x00001999, 0.1));
    return fixed_max(p8_fixed_from_int(1), val);
}
