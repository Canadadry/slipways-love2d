#include "game/save_load.c"
#include "table_reference.h"

void table_reference_game_save_load(void) {
    const char *module = "game.save_load";
    for (int i = 0; i < SAVE_NODE_KIND_COUNT; i++) {
        table_reference_function(module, node_constructors[i] != NULL, "node_constructors[%d]", i);
    }
}
