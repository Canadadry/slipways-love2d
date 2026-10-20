#ifndef GAME_ECONOMY_TIERS_H
#define GAME_ECONOMY_TIERS_H

#include "engine/entity_class.h"
#include "game/planet.h"
#include "p8/fixed.h"

void economy_tiers_rebuild_incoming_routes(void);

int economy_tiers_update_tiers(void);

p8_fixed_t economy_tiers_compute_tier(const entity_t *entity);

/* Returns 0, leaving *out untouched, when the tier or the project gives no amount. */
int economy_tiers_compute_production_amount(const planet_t *planet, p8_fixed_t *out);

#endif
