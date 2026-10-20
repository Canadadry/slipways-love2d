#ifndef GAME_PARTICLE_ZONE_H
#define GAME_PARTICLE_ZONE_H

#include <stddef.h>

#include "engine/allocator.h"
#include "engine/zone.h"

typedef enum {
    PARTICLE_SLOT_PARTICLE,
    PARTICLE_SLOT_LIST,
    PARTICLE_SLOT_HELP_LIST,
    PARTICLE_SLOT_BURST,
    PARTICLE_SLOT_TRANSIT_DOT,
    PARTICLE_SLOT_COUNT
} particle_slot_kind_t;

#define PARTICLE_SLOT ZONE_BYTES_BY_WIDTH(32u, 32u)
#define TRANSIT_DOTS_SLOT ZONE_BYTES_BY_WIDTH(24u, 24u)
#define PARTICLES_CAPACITY 158
#define PARTICLES_HELP_SCREEN_CAPACITY 653

const allocator_t *particle_zone(particle_slot_kind_t slot_kind);

const allocator_t *particle_zone_slot(particle_slot_kind_t slot_kind);

int particle_zone_live(particle_slot_kind_t slot_kind);

size_t particle_zone_used(void);
size_t particle_zone_max_used(void);

int particle_zone_count(particle_slot_kind_t slot_kind);
size_t particle_zone_slot_size(particle_slot_kind_t slot_kind);
size_t particle_zone_reserved(void);

int particle_zone_max_live(particle_slot_kind_t slot_kind);
size_t particle_zone_max_bytes(particle_slot_kind_t slot_kind);
void particle_zone_record_elements(particle_slot_kind_t slot_kind, int count);
int particle_zone_max_elements(particle_slot_kind_t slot_kind);

#endif
