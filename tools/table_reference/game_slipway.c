#include "game/slipway.c"
#include "reference_class.h"
#include "table_reference.h"

void table_reference_game_slipway(void) {
    const char *module = "game.slipway";
    table_reference_int(module, SLIPWAY_SERVED_PLANETS_CAP, "SLIPWAY_SERVED_PLANETS_CAP");
    table_reference_int(module, SLIPWAY_TRANSIT_DOTS_RESERVE, "SLIPWAY_TRANSIT_DOTS_RESERVE");
    table_reference_int(module, (long long)SLIPWAY_CONSTRUCTION_BYTES, "SLIPWAY_CONSTRUCTION_BYTES");
    reference_class(module, "slipway_too_far_class_def", &slipway_too_far_class_def);
    reference_header(module, "g_slipway_too_far", &g_slipway_too_far.header, "slipway_too_far_class_def",
                     &slipway_too_far_class_def);
    table_reference_string(module, g_slipway_too_far.status_text, "g_slipway_too_far.status_text");
    table_reference_int(module, g_slipway_too_far.targeted_by.index, "g_slipway_too_far.targeted_by.index");
    table_reference_int(module, g_slipway_too_far.targeted_by.generation, "g_slipway_too_far.targeted_by.generation");
    reference_class(module, "slipway_mouse_point_class_def", &slipway_mouse_point_class_def);
    reference_class(module, "slipway_class_def", &slipway_class_def);
    table_reference_int(module, (long long)sizeof(slipway_t), "SIZEOF.slipway_t");
    table_reference_int(module, (long long)sizeof(void *), "SIZEOF[\"void *\"]");
}
