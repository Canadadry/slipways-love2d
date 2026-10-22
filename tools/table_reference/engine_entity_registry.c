#include "engine/entity_registry.c"

#include "reference_class.h"
#include "table_reference.h"

#define MODULE "engine.entity_registry"

void table_reference_engine_entity_registry(void) {
    table_reference_int(MODULE, ENTITY_MESSAGE_DRAW, "ENTITY_MESSAGE_DRAW");
    table_reference_int(MODULE, ENTITY_MESSAGE_DRAW_OVERLAY, "ENTITY_MESSAGE_DRAW_OVERLAY");
    table_reference_int(MODULE, ENTITY_MESSAGE_HIT_TEST, "ENTITY_MESSAGE_HIT_TEST");
    table_reference_int(MODULE, ENTITY_MESSAGE_NEW_YEAR, "ENTITY_MESSAGE_NEW_YEAR");
    table_reference_int(MODULE, ENTITY_MESSAGE_REFRESH, "ENTITY_MESSAGE_REFRESH");
    table_reference_int(MODULE, ENTITY_MESSAGE_COUNT, "ENTITY_MESSAGE_COUNT");
    table_reference_int(MODULE, ENTITY_REGISTRY_LAYERS, "ENTITY_REGISTRY_LAYERS");
    reference_class(MODULE, "main_camera_placeholder_class_def", &main_camera_placeholder_class_def);
    reference_header(MODULE, "g_main_camera_placeholder", &g_main_camera_placeholder.header,
                     "main_camera_placeholder_class_def", &main_camera_placeholder_class_def);
}
