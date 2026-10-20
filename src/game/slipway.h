#ifndef GAME_SLIPWAY_H
#define GAME_SLIPWAY_H

#include "p8/fixed.h"
#include "game/game_data.h"
#include "engine/entity.h"
#include "game/targeting.h"
#include "engine/pointer_list.h"
#include "engine/entity_class.h"
#include "game/planet.h"

enum { SLIPWAY_SERVED_PLANETS_CAP = 90 };

enum { SLIPWAY_TRANSIT_DOTS_RESERVE = 24 };

typedef struct {
    int count;
    char ids[SLIPWAY_SERVED_PLANETS_CAP][PLANET_ID_SIZE];
} slipway_served_planets_t;

/* Copies id; the caller keeps its own string. */
void slipway_served_planets_add(slipway_served_planets_t *hg, const char *id);

int slipway_served_planets_has(const slipway_served_planets_t *hg, const char *id);

typedef struct {
    entity_t header;
} slipway_mouse_point_t;

typedef struct {
    entity_t header;
    const slipway_archetype_t *archetype;
    p8_fixed_t fade_in_step;
    entity_id_t start;
    pointer_list_t transit_dots;
    slipway_served_planets_t served_planets;
    entity_id_t end;
    int end_is_point;
    entity_id_t targeted_by;
    targeting_t targeting;
    slipway_mouse_point_t point;
} slipway_t;

#define SLIPWAY_CONSTRUCTION_BYTES \
    (ZONE_BYTES(sizeof(slipway_t)) + ZONE_BYTES(SLIPWAY_TRANSIT_DOTS_RESERVE * sizeof(void *)))

typedef struct {
    entity_t header;
    const char *status_text;
    entity_id_t targeted_by;
} slipway_too_far_t;

void slipway_init(void);

void slipway_shutdown(void);

int slipway_ready(void);

vec_t slipway_end_map_pos(const entity_t *end);
vec_t slipway_end_pos(const entity_t *end);

typedef struct {
    entity_id_t start;
} slipway_keys_t;

slipway_t *slipway_new(const slipway_keys_t *keys);

/* NULL once the planet was flushed. */
entity_t *slipway_start(const slipway_t *self);
/* The planet, the slipway's own mouse point, or NULL. */
entity_t *slipway_end(const slipway_t *self);

entity_t *slipway_too_far(void);

void slipway_construct(slipway_t *self);

void slipway_drag(slipway_t *self, mouse_pos_t mouse);

void slipway_drop(slipway_t *self, entity_t *end_planet);

/* Joins a built slipway to end as a confirmed drag would, without paying,
   advancing the clock, attaching either end or touching the fade. */
void slipway_connect(slipway_t *self, entity_t *end);

entity_t *slipway_obstruction(slipway_t *self);

const entity_class_t *slipway_too_far_class(void);

const entity_class_t *slipway_mouse_point_class(void);

entity_t *slipway_other_end(const slipway_t *self, const entity_t *planet_instance);

void slipway_draw_overlay(slipway_t *self);

void slipway_draw_method(entity_t *self, vec_t screen_pos);

#endif
