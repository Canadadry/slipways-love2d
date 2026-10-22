#include "game/pad_panel.h"
#include "table_reference.h"

void table_reference_game_pad_panel(void) {
    const char *module = "game.pad_panel";
    table_reference_int(module, PAD_PANEL_ANY_LAYER, "PAD_PANEL_ANY_LAYER");
}
