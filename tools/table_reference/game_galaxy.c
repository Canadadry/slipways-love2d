#include "game/galaxy.h"
#include "table_reference.h"

void table_reference_game_galaxy(void) {
    const char *module = "game.galaxy";
    table_reference_int(module, GALAXY_MAP_MARGIN, "GALAXY_MAP_MARGIN");
    table_reference_int(module, GALAXY_MAP_SPAN, "GALAXY_MAP_SPAN");
    table_reference_int(module, GALAXY_MAP_SCALE, "GALAXY_MAP_SCALE");
}
