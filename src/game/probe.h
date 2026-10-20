#ifndef GAME_PROBE_H
#define GAME_PROBE_H

#include "game/game_data.h"
#include "game/particle.h"
#include "game/targeting.h"
#include "engine/entity_class.h"
#include "engine/vec.h"
#include "engine/allocator.h"

typedef struct {
    entity_t header;
    const probe_archetype_t *archetype;
    const allocator_t *grab_arena;
    emitter_t emitter;
    targeting_t targeting;
} probe_t;

void probe_init(void);

void probe_shutdown(void);

int probe_ready(void);

void probe_init_instance(probe_t *self, const entity_class_t *cls, const probe_archetype_t *archetype);

typedef struct {
    p8_fixed_t state_time;
    int has_angle_slices;
    p8_fixed_t angle_slices;
    int has_base_radius;
    p8_fixed_t base_radius;
    int has_speed_factor;
    p8_fixed_t speed_factor;
} probe_keys_t;

probe_t *probe_new(const probe_keys_t *keys);

particle_t *probe_spawn_particle(entity_t *self);

void probe_release_method(entity_t *self);

#endif
