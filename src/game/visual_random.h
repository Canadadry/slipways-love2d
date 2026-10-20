#ifndef VISUAL_RANDOM_H
#define VISUAL_RANDOM_H

#include "p8/fixed.h"

#include <stdint.h>

/* The visual stream: random draws for on-screen-only effects. No game
 * decision may read it, and the save does not hold it. It is seeded once at
 * boot and never reseeded. */
p8_fixed_t visual_random_draw(p8_fixed_t x);
p8_fixed_t visual_random_draw_one(void);

uint32_t visual_random_state(void);
void visual_random_set_state(uint32_t state);

#endif
