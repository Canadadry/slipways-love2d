#include "game/draw_functions.c"
#include "table_reference.h"

void table_reference_game_draw_functions(void) {
    const char *module = "game.draw_functions";
    for (int id = 0; id < DRAW_FUNCTION_COUNT; id++) {
        table_reference_function(module, draw_functions_by_id[id] != NULL, "draw_functions_by_id[%d]", id);
    }
}
