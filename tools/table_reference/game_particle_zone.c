#include "game/particle_zone.c"
#include "table_reference.h"

void table_reference_game_particle_zone(void) {
    const char *module = "game.particle_zone";
    table_reference_int(module, PARTICLE_SLOT_PARTICLE, "PARTICLE_SLOT_PARTICLE");
    table_reference_int(module, PARTICLE_SLOT_LIST, "PARTICLE_SLOT_LIST");
    table_reference_int(module, PARTICLE_SLOT_HELP_LIST, "PARTICLE_SLOT_HELP_LIST");
    table_reference_int(module, PARTICLE_SLOT_BURST, "PARTICLE_SLOT_BURST");
    table_reference_int(module, PARTICLE_SLOT_TRANSIT_DOT, "PARTICLE_SLOT_TRANSIT_DOT");
    table_reference_int(module, PARTICLE_SLOT_COUNT, "PARTICLE_SLOT_COUNT");
    table_reference_int(module, (long long)PARTICLE_SLOT, "PARTICLE_SLOT");
    table_reference_int(module, (long long)TRANSIT_DOTS_SLOT, "TRANSIT_DOTS_SLOT");
    table_reference_int(module, PARTICLES_CAPACITY, "PARTICLES_CAPACITY");
    table_reference_int(module, PARTICLES_HELP_SCREEN_CAPACITY, "PARTICLES_HELP_SCREEN_CAPACITY");
    for (int i = 0; i < PARTICLE_SLOT_COUNT; i++) {
        table_reference_string(module, SLOT_KINDS[i].zone_name, "SLOT_KINDS[%d].zone_name", i);
        if (SLOT_KINDS[i].slot_name != NULL) {
            table_reference_string(module, SLOT_KINDS[i].slot_name, "SLOT_KINDS[%d].slot_name", i);
        }
        table_reference_int(module, (long long)SLOT_KINDS[i].size, "SLOT_KINDS[%d].size", i);
        table_reference_int(module, (long long)SLOT_KINDS[i].count, "SLOT_KINDS[%d].count", i);
        for (size_t j = 0; j < SLOT_KINDS[i].count; j++) {
            table_reference_int(module, SLOT_KINDS[i].taken[j], "SLOT_KINDS[%d].taken[%zu]", i, j);
        }
    }
    table_reference_int(module, (long long)sizeof(pointer_list_t), "SIZEOF.pointer_list_t");
    table_reference_int(module, (long long)sizeof(void *), "SIZEOF[\"void *\"]");
}
