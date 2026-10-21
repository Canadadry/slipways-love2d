#include "game/targeting.h"
#include "table_reference.h"

void table_reference_game_targeting(void) {
    const char *module = "game.targeting";
    table_reference_int(module, TARGETING_TARGET_NONE, "TARGETING_TARGET_NONE");
    table_reference_int(module, TARGETING_TARGET_ENTITY, "TARGETING_TARGET_ENTITY");
    table_reference_int(module, TARGETING_TARGET_TRACK_THROWAWAY, "TARGETING_TARGET_TRACK_THROWAWAY");
    table_reference_int(module, TARGETING_TARGET_SLIPWAY_TOO_FAR, "TARGETING_TARGET_SLIPWAY_TOO_FAR");
    table_reference_int(module, TARGETING_TARGET_WORMHOLE_TOO_FAR, "TARGETING_TARGET_WORMHOLE_TOO_FAR");
}
