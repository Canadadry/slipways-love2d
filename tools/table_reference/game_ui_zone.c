#include "game/ui_zone.c"
#include "table_reference.h"

void table_reference_game_ui_zone(void) {
    const char *module = "game.ui_zone";
    table_reference_int(module, SLOT_KIND_BUTTON, "SLOT_KIND_BUTTON");
    table_reference_int(module, SLOT_KIND_MODAL_BACKDROP, "SLOT_KIND_MODAL_BACKDROP");
    table_reference_int(module, SLOT_KIND_MESSAGE_BOX, "SLOT_KIND_MESSAGE_BOX");
    table_reference_int(module, SLOT_KIND_TOAST, "SLOT_KIND_TOAST");
    table_reference_int(module, SLOT_KIND_ROW, "SLOT_KIND_ROW");
    table_reference_int(module, SLOT_KIND_EPHEMERALS, "SLOT_KIND_EPHEMERALS");
    table_reference_int(module, SLOT_KIND_COUNT, "SLOT_KIND_COUNT");
    table_reference_int(module, UI_EPHEMERALS_RESERVE, "UI_EPHEMERALS_RESERVE");
    for (int i = 0; i < SLOT_KIND_COUNT; i++) {
        table_reference_string(module, SLOT_KINDS[i].zone_name, "SLOT_KINDS[%d].zone_name", i);
        table_reference_string(module, SLOT_KINDS[i].slot_name, "SLOT_KINDS[%d].slot_name", i);
        table_reference_int(module, (long long)SLOT_KINDS[i].size, "SLOT_KINDS[%d].size", i);
        table_reference_int(module, (long long)SLOT_KINDS[i].count, "SLOT_KINDS[%d].count", i);
        for (size_t j = 0; j < SLOT_KINDS[i].count; j++) {
            table_reference_int(module, SLOT_KINDS[i].taken[j], "SLOT_KINDS[%d].taken[%zu]", i, j);
        }
    }
    table_reference_int(module, (long long)sizeof(entity_id_list_t), "SIZEOF.entity_id_list_t");
    table_reference_int(module, (long long)sizeof(entity_id_t), "SIZEOF.entity_id_t");
}
