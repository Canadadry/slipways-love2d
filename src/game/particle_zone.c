#include "game/particle_zone.h"

#include <assert.h>

#include "engine/pointer_list.h"
#include "engine/slot_group.h"
#include "engine/zone_sizes.h"

#define PARTICLES_SIZE ARENA_LIST_BYTES(sizeof(pointer_list_t), PARTICLES_CAPACITY, sizeof(void *))
#define PARTICLES_HELP_SCREEN_SIZE \
    ARENA_LIST_BYTES(sizeof(pointer_list_t), PARTICLES_HELP_SCREEN_CAPACITY, sizeof(void *))
#define BURST_SIZE ZONE_BYTES_BY_WIDTH(648u, 424u)
#define PARTICLES_COUNT 18
#define PARTICLES_HELP_SCREEN_COUNT 1
#define BURST_COUNT 4
#define TRANSIT_DOTS_COUNT 437
#define PARTICLE_CAPACITY 887

typedef char particle_zone_holds_help_screen_t[PARTICLE_CAPACITY >= PARTICLES_HELP_SCREEN_CAPACITY ? 1 : -1];

typedef char particle_zone_exceeded_t[
    (SLOT_ZONE_BYTES(PARTICLE_SLOT, PARTICLE_CAPACITY) + SLOT_ZONE_BYTES(PARTICLES_SIZE, PARTICLES_COUNT) +
     SLOT_ZONE_BYTES(PARTICLES_HELP_SCREEN_SIZE, PARTICLES_HELP_SCREEN_COUNT) + SLOT_ZONE_BYTES(BURST_SIZE, BURST_COUNT) +
     SLOT_ZONE_BYTES(TRANSIT_DOTS_SLOT, TRANSIT_DOTS_COUNT)
     <= PARTICLES_ZONE_BYTES) ? 1 : -1];

SLOT_ZONE_STORAGE(particle_cells, particle_taken, PARTICLE_SLOT, PARTICLE_CAPACITY);
SLOT_ZONE_STORAGE(particles_cells, particles_taken, PARTICLES_SIZE, PARTICLES_COUNT);
SLOT_ZONE_STORAGE(particles_help_screen_cells, particles_help_screen_taken, PARTICLES_HELP_SCREEN_SIZE, PARTICLES_HELP_SCREEN_COUNT);
SLOT_ZONE_STORAGE(burst_cells, burst_taken, BURST_SIZE, BURST_COUNT);
SLOT_ZONE_STORAGE(transit_dots_cells, transit_dots_taken, TRANSIT_DOTS_SLOT, TRANSIT_DOTS_COUNT);

static const slot_kind_zone_t SLOT_KINDS[PARTICLE_SLOT_COUNT] = {
    [PARTICLE_SLOT_PARTICLE] = {"particles particle", NULL, particle_cells, particle_taken,
                                    PARTICLE_SLOT, PARTICLE_CAPACITY},
    [PARTICLE_SLOT_LIST] = {"particles particles", "slot particles", particles_cells, particles_taken, PARTICLES_SIZE, PARTICLES_COUNT},
    [PARTICLE_SLOT_HELP_LIST] = {"particles particles_help_screen", "slot particles_help_screen", particles_help_screen_cells, particles_help_screen_taken, PARTICLES_HELP_SCREEN_SIZE,
                                PARTICLES_HELP_SCREEN_COUNT},
    [PARTICLE_SLOT_BURST] = {"particles burst", "slot burst", burst_cells, burst_taken, BURST_SIZE, BURST_COUNT},
    [PARTICLE_SLOT_TRANSIT_DOT] = {"particles transit_dots", NULL, transit_dots_cells, transit_dots_taken, TRANSIT_DOTS_SLOT, TRANSIT_DOTS_COUNT},
};

static slot_group_t g_group = SLOT_GROUP(SLOT_KINDS, PARTICLE_SLOT_COUNT);

const allocator_t *particle_zone(particle_slot_kind_t slot_kind) {
    return slot_group_handle(&g_group, slot_kind);
}

const allocator_t *particle_zone_slot(particle_slot_kind_t slot_kind) {
    return slot_group_slot(&g_group, slot_kind);
}

int particle_zone_live(particle_slot_kind_t slot_kind) {
    return slot_group_live(&g_group, slot_kind);
}

int particle_zone_max_live(particle_slot_kind_t slot_kind) {
    return slot_group_max_live(&g_group, slot_kind);
}

size_t particle_zone_max_bytes(particle_slot_kind_t slot_kind) {
    return slot_group_max_bytes(&g_group, slot_kind);
}

void particle_zone_record_elements(particle_slot_kind_t slot_kind, int count) {
    assert((slot_kind == PARTICLE_SLOT_LIST || slot_kind == PARTICLE_SLOT_HELP_LIST || slot_kind == PARTICLE_SLOT_TRANSIT_DOT) &&
           "particle_zone_record_elements: only a list has elements");
    slot_group_record_elements(&g_group, slot_kind, count);
}

int particle_zone_max_elements(particle_slot_kind_t slot_kind) {
    return slot_group_max_elements(&g_group, slot_kind);
}

size_t particle_zone_used(void) {
    return slot_group_used(&g_group);
}

size_t particle_zone_max_used(void) {
    return slot_group_max_used(&g_group);
}

int particle_zone_count(particle_slot_kind_t slot_kind) {
    return slot_group_count(&g_group, slot_kind);
}

size_t particle_zone_slot_size(particle_slot_kind_t slot_kind) {
    return slot_group_slot_size(&g_group, slot_kind);
}

size_t particle_zone_reserved(void) {
    return slot_group_reserved(&g_group);
}
