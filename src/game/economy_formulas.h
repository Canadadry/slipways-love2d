#ifndef GAME_ECONOMY_FORMULAS_H
#define GAME_ECONOMY_FORMULAS_H

#include "p8/fixed.h"
#include "game/map_tables.h"
#include "engine/vec.h"

enum { RESOURCE_MAP_CAP = RESOURCE_CODE_COUNT + 3 };

typedef struct {
    int count;
    struct {
        int resource;
        p8_fixed_t value;
    } entries[RESOURCE_MAP_CAP];
} resource_map_t;

int resource_map_get(const resource_map_t *map, p8_fixed_t qe, p8_fixed_t *out);
void resource_map_set(resource_map_t *map, p8_fixed_t qe, p8_fixed_t v);

void economy_formulas_fill_resource_map(resource_map_t *out, p8_fixed_t jm);

p8_fixed_t economy_formulas_route_base_cost(vec_t from_pos, vec_t to_pos);

p8_fixed_t economy_formulas_cost_scale(void);

#endif
