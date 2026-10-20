#ifndef GAME_GALAXY_H
#define GAME_GALAXY_H

#include "p8/fixed.h"

/* Planets lie in [margin, margin + span) on both axes, in map units; a planet's position is its map position times the scale. */
#define GALAXY_MAP_MARGIN P8_FIXED_LIT(0x00002000, 0.125)
#define GALAXY_MAP_SPAN P8_FIXED_LIT(0x0002c000, 2.75)
#define GALAXY_MAP_SCALE 128

void galaxy_populate(p8_fixed_t planet_count);

/* The type picker and vanish walk of a new game, for the current difficulty.
   Draws nothing from the RNG: a resume builds them the same way. */
void galaxy_install_rolls(void);

#endif
