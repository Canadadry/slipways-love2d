#include "game/grab_arena.c"
#include "table_reference.h"

void table_reference_game_grab_arena(void) {
    const char *module = "game.grab_arena";
    table_reference_int(module, GRAB_ARENAS_FREE_MAX, "GRAB_ARENAS_FREE_MAX");
    table_reference_string(module, grab_arena_name, "grab_arena_name");
}
