#include "game/wormhole.c"
#include "reference_class.h"
#include "table_reference.h"

void table_reference_game_wormhole(void) {
    const char *module = "game.wormhole";
    table_reference_int(module, WORMHOLE_HAS_TOO_FAR_TARGET, "WORMHOLE_HAS_TOO_FAR_TARGET");
    reference_class(module, "wormhole_too_far_target_class_def", &wormhole_too_far_target_class_def);
    reference_header(module, "g_wormhole_too_far_target_wrapper", &g_wormhole_too_far_target_wrapper.header,
                     "wormhole_too_far_target_class_def", &wormhole_too_far_target_class_def);
    table_reference_string(module, g_wormhole_too_far_target_wrapper.status_text,
                           "g_wormhole_too_far_target_wrapper.status_text");
    reference_class(module, "wormhole_class_def", &wormhole_class_def);
    table_reference_int(module, (long long)sizeof(wormhole_t), "SIZEOF.wormhole_t");
    table_reference_int(module, (long long)sizeof(entity_id_t), "SIZEOF.entity_id_t");
}
