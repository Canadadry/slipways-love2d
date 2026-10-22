#include "game/cheat.h"
#include "table_reference.h"

void table_reference_game_cheat(void) {
    const char *module = "game.cheat";
    table_reference_int(module, CHEAT_CHORDS, "CHEAT_CHORDS");
}
