#include "game/trade.h"
#include "table_reference.h"

void table_reference_game_trade(void) {
    const char *module = "game.trade";
    table_reference_int(module, RESOURCE_NIL, "RESOURCE_NIL");
    table_reference_int(module, RESOURCE_FALSE, "RESOURCE_FALSE");
    table_reference_int(module, RESOURCE_FIXED, "RESOURCE_FIXED");
    table_reference_int(module, (long long)sizeof(trade_route_t), "SIZEOF.trade_route_t");
}
