#ifndef GAME_RESEARCH_H
#define GAME_RESEARCH_H

#include <stdint.h>

#include "p8/fixed.h"
#include "game/game_data.h"
#include "game/map_tables.h"
#include "engine/pointer_list.h"
#include "engine/entity_class.h"
#include "game/trade.h"

typedef struct {
    data_row_t row;
    int has_satisfied;
    p8_fixed_t satisfied;
} need_t;

resource_t need_resource(const need_t *i);

need_t *need_new(const allocator_t *zone, resource_t resource);

typedef struct {
    entity_t header;
    const research_project_archetype_t *archetype;
    const char *name;
    const char *planet_codes;
    p8_fixed_t base_cost;
    int has_base_cost;
    const char *production_key;
    pointer_list_t inputs;
    p8_fixed_t output;
    int has_output;
    const char *upgrade_tech;
    int has_upgrade_tech;
    p8_fixed_t byproduct;
    int has_byproduct;
    const char *required_tech;
    p8_fixed_t icon_sprite;
    int has_icon_sprite;
    const char *class_name;
    const char *next_type;
} research_project_t;

typedef struct {
    uint16_t index;
    uint16_t epoch;
} research_project_id_t;

#define RESEARCH_PROJECT_ID_NONE ((research_project_id_t){0, 0})

/* NULL for no project, and for an id taken before the projects were last
 * forgotten or the world last reset. */
research_project_t *research_project_get(research_project_id_t id);

/* RESEARCH_PROJECT_ID_NONE for NULL. */
research_project_id_t research_project_id_of(const research_project_t *project);

void research_expire_project_ids(void);

void research_init(void);

void research_shutdown(void);

int research_project_ready(void);
const entity_class_t *research_project_class(void);

const data_row_t *research_project_row_tail(void);

const char *research_project_upgrade_tech(const research_project_t *self);

void research_project_assert(const research_project_t *self);

typedef struct {
    const project_records_entry_t *record;
    const char *required_tech;
} research_project_keys_t;

research_project_t *research_project_new(const research_project_keys_t *keys);

void research_build_projects(void);

int research_projects_count(void);
research_project_t *research_projects_at(int k);
research_project_t *research_projects_find(const char *px);

const production_tables_entry_t *research_project_production(const research_project_t *self);

p8_fixed_t research_project_cost(const research_project_t *self);

#endif
