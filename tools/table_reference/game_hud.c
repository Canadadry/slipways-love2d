#include "game/hud.c"
#include "table_reference.h"

void table_reference_game_hud(void) {
    const char *module = "game.hud";
    for (int block = 0; block < 16; block++) {
        for (int i = 0; i < 16; i++) {
            table_reference_int(module, palette_blocks[block][i], "palette_blocks[%d][%d]", block, i);
        }
    }
}
