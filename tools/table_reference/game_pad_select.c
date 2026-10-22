#include "game/pad_select.h"
#include "table_reference.h"

void table_reference_game_pad_select(void) {
    const char *module = "game.pad_select";
    table_reference_int(module, PAD_SELECT_HOLD_FRAMES, "PAD_SELECT_HOLD_FRAMES");
    table_reference_int(module, PAD_SELECT_TAP_FRAMES, "PAD_SELECT_TAP_FRAMES");
    table_reference_int(module, PAD_SELECT_CHAIN_FRAMES, "PAD_SELECT_CHAIN_FRAMES");
    table_reference_int(module, PAD_SELECT_NONE, "PAD_SELECT_NONE");
    table_reference_int(module, PAD_SELECT_TAP, "PAD_SELECT_TAP");
    table_reference_int(module, PAD_SELECT_YEAR, "PAD_SELECT_YEAR");
}
