#ifndef GAME_PLANET_FAMILY_TRADE_H
#define GAME_PLANET_FAMILY_TRADE_H

#include "game/planet_trade.h"
#include "engine/entity_class.h"

void planet_family_trade_init(void);

void planet_family_trade_shutdown(void);

int slipgate_ready(void);
const entity_kind_t *slipgate_kind(void);
const entity_class_t *slipgate_class(void);

planet_t *slipgate_new(const planet_keys_t *keys);

int infraspace_gate_ready(void);
const entity_kind_t *infraspace_gate_kind(void);
const entity_class_t *infraspace_gate_class(void);

planet_t *infraspace_gate_new(const planet_keys_t *keys);

int synthesizer_ready(void);
const entity_kind_t *synthesizer_kind(void);
const entity_class_t *synthesizer_class(void);

planet_t *synthesizer_new(const planet_keys_t *keys);

int slipgate_takes_slipway(const planet_t *self);

down_t slipgate_accept_drop(const planet_t *self, const entity_t *s);

down_t slipgate_left_press(planet_t *self);

void slipgate_refresh(planet_t *self);

void slipgate_offer_trade(planet_t *self, trade_offer_t *offer);

void synthesizer_attach_slipway(planet_t *self, entity_t *slipway);

void synthesizer_set_spent(planet_t *self, int spent);

#endif
