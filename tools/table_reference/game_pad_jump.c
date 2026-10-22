#include "game/pad_jump.c"
#include "table_reference.h"

void table_reference_game_pad_jump(void) {
    const char *module = "game.pad_jump";
    table_reference_int(module, PAD_JUMP_REPEAT_DELAY, "PAD_JUMP_REPEAT_DELAY");
    table_reference_int(module, PAD_JUMP_REPEAT_PERIOD, "PAD_JUMP_REPEAT_PERIOD");
    table_reference_int(module, PAD_JUMP_SCROLL_MARGIN, "PAD_JUMP_SCROLL_MARGIN");
    table_reference_int(module, PAD_JUMP_SCROLL_MARGIN_BOTTOM, "PAD_JUMP_SCROLL_MARGIN_BOTTOM");
    table_reference_int(module, PAD_JUMP_SCROLL_SPEED, "PAD_JUMP_SCROLL_SPEED");
    table_reference_int(module, PAD_JUMP_DIRECTIONS, "PAD_JUMP_DIRECTIONS");
    for (int i = 0; i < PAD_JUMP_DIRECTIONS; i++) {
        table_reference_int(module, directions_by_priority[i], "directions_by_priority[%d]", i);
    }
}
