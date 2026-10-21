#include "game/pad_slipway.h"
#include "table_reference.h"

void table_reference_game_pad_slipway(void) {
    const char *module = "game.pad_slipway";
    table_reference_int(module, PAD_SLIPWAY_NONE, "PAD_SLIPWAY_NONE");
    table_reference_int(module, PAD_SLIPWAY_PRESSED, "PAD_SLIPWAY_PRESSED");
    table_reference_int(module, PAD_SLIPWAY_HELD, "PAD_SLIPWAY_HELD");
    table_reference_int(module, PAD_SLIPWAY_RELEASED, "PAD_SLIPWAY_RELEASED");
    table_reference_int(module, PAD_SLIPWAY_REFUSED, "PAD_SLIPWAY_REFUSED");
    table_reference_int(module, PAD_SLIPWAY_BUILT, "PAD_SLIPWAY_BUILT");
}
