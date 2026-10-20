#ifndef GAME_PLANET_DISPLAY_H
#define GAME_PLANET_DISPLAY_H

#include "game/research.h"
#include "engine/entity_class.h"
#include "p8/gfx.h"
#include "p8/vram.h"
#include "game/planet.h"
#include "engine/vec.h"

void planet_refresh(planet_t *self);

void planet_inspection_panel(const planet_t *self);

down_t planet_accept_drop(const planet_t *self, const entity_t *s);

void planet_projects_panel(planet_t *self);

void planet_draw(p8_graphics_t *graphics, p8_vram_t *memory, planet_t *self, vec_t screen_pos);

void planet_apply_project(planet_t *self, research_project_t *research_project_instance);

#endif
