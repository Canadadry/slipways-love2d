#include "game/visual_random.h"

#define VISUAL_RANDOM_BOOT_SEED 0x9E3779B9u

static uint32_t visual_state = VISUAL_RANDOM_BOOT_SEED;

p8_fixed_t visual_random_draw(p8_fixed_t x) {
    return p8_fixed_rnd_from_state(&visual_state, x);
}

p8_fixed_t visual_random_draw_one(void) {
    return p8_fixed_rnd_from_state(&visual_state, P8_FIXED_INT(1));
}

uint32_t visual_random_state(void) {
    return visual_state;
}

void visual_random_set_state(uint32_t state) {
    visual_state = state;
}
