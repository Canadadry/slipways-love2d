#include "game/pad_nav.h"
#include "table_reference.h"

void table_reference_game_pad_nav(void) {
    const char *module = "game.pad_nav";
    table_reference_int(module, PAD_NAV_MAP_JUMP, "PAD_NAV_MAP_JUMP");
    table_reference_int(module, PAD_NAV_MAP_AIM, "PAD_NAV_MAP_AIM");
    table_reference_int(module, PAD_NAV_MAP_SLIPWAY, "PAD_NAV_MAP_SLIPWAY");
    table_reference_int(module, PAD_NAV_PLACEMENT_NONE, "PAD_NAV_PLACEMENT_NONE");
    table_reference_int(module, PAD_NAV_PLACEMENT_PRESSED, "PAD_NAV_PLACEMENT_PRESSED");
    table_reference_int(module, PAD_NAV_PLACEMENT_CARRYING, "PAD_NAV_PLACEMENT_CARRYING");
    table_reference_int(module, PAD_NAV_PLACEMENT_SUSPENDED, "PAD_NAV_PLACEMENT_SUSPENDED");
}
