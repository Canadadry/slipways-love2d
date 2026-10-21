#include "game/main_economy.h"
#include "table_reference.h"

void table_reference_game_main_economy(void) {
    const char *module = "game.main_economy";
    table_reference_int(module, ECONOMY_HAS_PRODUCTION, "ECONOMY_HAS_PRODUCTION");
    table_reference_int(module, ECONOMY_HAS_DEMAND, "ECONOMY_HAS_DEMAND");
}
