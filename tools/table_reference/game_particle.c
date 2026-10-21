#include "game/particle.c"
#include "table_reference.h"

void table_reference_game_particle(void) {
    const char *module = "game.particle";
    table_reference_int(module, EMITTER_HAS_PARTICLES, "EMITTER_HAS_PARTICLES");
    table_reference_int(module, (long long)sizeof(particle_t), "SIZEOF.particle_t");
}
