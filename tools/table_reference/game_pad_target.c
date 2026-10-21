#include "game/pad_target.h"
#include "table_reference.h"

void table_reference_game_pad_target(void) {
    const char *module = "game.pad_target";
    table_reference_int(module, PAD_TARGET_PERPENDICULAR_WEIGHT, "PAD_TARGET_PERPENDICULAR_WEIGHT");
}
