#ifndef GAME_RESEARCH_DISPLAY_H
#define GAME_RESEARCH_DISPLAY_H

#include "engine/entity_class.h"
#include "p8/fixed.h"
#include "game/layout.h"
#include "engine/pointer_list.h"
#include "game/research.h"

/* Fills `out_keys` and returns 1; returns 0, keys untouched, when the project has no output. */
int research_project_layout_row_keys(const research_project_t *self, layout_row_keys_t *out_keys);

/* Keys of the row {name, 32, {icon_sprite, 8}}, for a project with an icon sprite. */
layout_row_keys_t research_project_icon_row_keys(const research_project_t *self);

const data_row_t *research_project_tooltip(const research_project_t *self);

/* Fills `out_keys` and returns 1; returns 0, keys untouched, when there is no output. */
int research_display_planet_resource_row_keys(const pointer_list_t *inputs, int has_output, p8_fixed_t output, p8_fixed_t amount,
                                              p8_fixed_t used, p8_fixed_t spacing, layout_row_keys_t *out_keys);

typedef struct {
    research_project_id_t items[PROJECT_RECORDS_COUNT];
    int count;
} available_projects_kept_t;

available_projects_kept_t research_display_available_projects(const entity_t *planet_instance);

#endif
