#include "game/planet.c"
#include "reference_class.h"
#include "table_reference.h"

void table_reference_game_planet(void) {
    const char *module = "game.planet";
    table_reference_int(module, PLANET_ID_SIZE, "PLANET_ID_SIZE");
    table_reference_int(module, PLANET_ROW_SIGNATURE_NEEDS, "PLANET_ROW_SIGNATURE_NEEDS");
    table_reference_int(module, PLANET_HAS_TARGETING, "PLANET_HAS_TARGETING");
    table_reference_int(module, PLANET_HAS_EMITTER, "PLANET_HAS_EMITTER");
    table_reference_int(module, PLANET_HAS_PRODUCTION, "PLANET_HAS_PRODUCTION");
    table_reference_int(module, PLANET_HAS_NEEDS, "PLANET_HAS_NEEDS");
    table_reference_int(module, PLANET_HAS_INCOMING_ROUTES, "PLANET_HAS_INCOMING_ROUTES");
    table_reference_int(module, PLANET_HAS_OUTGOING_ROUTES, "PLANET_HAS_OUTGOING_ROUTES");
    table_reference_int(module, PLANET_HAS_ID, "PLANET_HAS_ID");
    reference_class(module, "planet_class_def", &planet_class_def);
    table_reference_int(module, (long long)sizeof(planet_t), "SIZEOF.planet_t");
    table_reference_int(module, (long long)sizeof(need_t), "SIZEOF.need_t");
    table_reference_int(module, (long long)sizeof(void *), "SIZEOF[\"void *\"]");
}
