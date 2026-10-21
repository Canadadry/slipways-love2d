#include "game/pad_direction.h"
#include "table_reference.h"

void table_reference_game_pad_direction(void) {
    const char *module = "game.pad_direction";
    table_reference_int(module, PAD_DIRECTION_NONE, "PAD_DIRECTION_NONE");
    table_reference_int(module, PAD_DIRECTION_UP, "PAD_DIRECTION_UP");
    table_reference_int(module, PAD_DIRECTION_DOWN, "PAD_DIRECTION_DOWN");
    table_reference_int(module, PAD_DIRECTION_LEFT, "PAD_DIRECTION_LEFT");
    table_reference_int(module, PAD_DIRECTION_RIGHT, "PAD_DIRECTION_RIGHT");
}
