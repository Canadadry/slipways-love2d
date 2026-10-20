#ifndef GAME_PLANET_TRADE_H
#define GAME_PLANET_TRADE_H

#include "engine/entity_class.h"
#include "p8/fixed.h"
#include "game/planet.h"
#include "game/trade.h"

typedef struct trade_offer trade_offer_t;

typedef void (*trade_offer_accept_fn_t)(trade_offer_t *self, const trade_route_t *a);

struct trade_offer {
    trade_route_t route;
    p8_fixed_t amount;
    trade_offer_accept_fn_t accept;
    trade_route_list_t accepted_routes;
};

void planet_attach_slipway(planet_t *self, entity_t *slipway);

void planet_offer_trade(planet_t *self, trade_offer_t *offer);

/* Updates self's outgoing routes in place and returns their address, valid for the call only; NULL for a NULL planet. */
trade_route_list_t *planet_rebuild_trades(planet_t *self);

#endif
