#ifndef GAME_ECONOMY_H
#define GAME_ECONOMY_H

#include "game/economy_formulas.h"
#include "p8/fixed.h"
#include "game/main_economy.h"
#include "game/map_tables.h"
#include "engine/entity_class.h"

extern const economy_archetype_t economy_archetype;

void economy_init(void);
void economy_shutdown(void);

int economy_ready(void);
const entity_class_t *economy_class(void);

economy_t *economy_new(void);

void economy_advance_clock(economy_t *self, p8_fixed_t hx);

/* economy_advance_clock without its recompute's figures, for a caller that
 * changed only the credits since the last one: the end status and the refresh
 * listeners are brought up to date. A new year still recomputes in full. */
void economy_tick_clock(economy_t *self, p8_fixed_t hx);

void economy_new_year(economy_t *self);

int economy_spend(economy_t *self, p8_fixed_t jt);

void economy_recompute(economy_t *self);

void economy_invent(economy_t *self, const technologies_entry_t *t);

void economy_total_production(resource_map_t *out);
void economy_crowding_penalty(resource_map_t *out);

#endif
