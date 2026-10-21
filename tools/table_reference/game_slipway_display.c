#include "game/slipway_display.h"
#include "table_reference.h"

void table_reference_game_slipway_display(void) {
    const char *module = "game.slipway_display";
    table_reference_int(module, (long long)sizeof(transit_dot_t), "SIZEOF.transit_dot_t");
}
