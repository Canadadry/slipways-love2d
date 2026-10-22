#include "game/pad_cycle.h"
#include "table_reference.h"

void table_reference_game_pad_cycle(void) {
    const char *module = "game.pad_cycle";
    table_reference_int(module, PAD_CYCLE_CLOSED, "PAD_CYCLE_CLOSED");
    table_reference_int(module, PAD_CYCLE_TECH, "PAD_CYCLE_TECH");
    table_reference_int(module, PAD_CYCLE_SCORE, "PAD_CYCLE_SCORE");
}
