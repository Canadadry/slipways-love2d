#include "engine/entity_class.h"
#include "table_reference.h"

void table_reference_engine_entity_class(void) {
    const char *module = "engine.entity_class";
    table_reference_int(module, ENTITY_STATE_NONE, "ENTITY_STATE_NONE");
    table_reference_int(module, ENTITY_STATE_NORMAL, "ENTITY_STATE_NORMAL");
    table_reference_int(module, ENTITY_STATE_SETTLED, "ENTITY_STATE_SETTLED");
    table_reference_int(module, ENTITY_STATE_VANISHING, "ENTITY_STATE_VANISHING");
    table_reference_int(module, ENTITY_STATE_PLACING, "ENTITY_STATE_PLACING");
    table_reference_int(module, ENTITY_STATE_UNFOLDING, "ENTITY_STATE_UNFOLDING");
    table_reference_int(module, ENTITY_STATE_UNKNOWN, "ENTITY_STATE_UNKNOWN");
    table_reference_int(module, ENTITY_STATE_KNOWN, "ENTITY_STATE_KNOWN");
    table_reference_int(module, STATE_COUNT, "STATE_COUNT");
    table_reference_int(module, TAG_STRUCTURE, "TAG_STRUCTURE");
    table_reference_int(module, TAG_ASCENSION, "TAG_ASCENSION");
    table_reference_int(module, TAG_SLIPGATE, "TAG_SLIPGATE");
    table_reference_int(module, TAG_PROBE, "TAG_PROBE");
    table_reference_int(module, TAG_MESSAGE_BOX, "TAG_MESSAGE_BOX");
    table_reference_int(module, TAG_SLIPWAY, "TAG_SLIPWAY");
    table_reference_int(module, TAG_WORMHOLE, "TAG_WORMHOLE");
    table_reference_int(module, TAG_PLANET, "TAG_PLANET");
    table_reference_int(module, TAG_NODE, "TAG_NODE");
    table_reference_int(module, TAG_COUNT, "TAG_COUNT");
    table_reference_int(module, LINEAGE_PLANET, "LINEAGE_PLANET");
    table_reference_int(module, LINEAGE_STRUCTURE, "LINEAGE_STRUCTURE");
    table_reference_int(module, LINEAGE_TRADING_HUB, "LINEAGE_TRADING_HUB");
    table_reference_int(module, LINEAGE_PROBE, "LINEAGE_PROBE");
    table_reference_int(module, ENTITY_HAS_LAYER, "ENTITY_HAS_LAYER");
    table_reference_int(module, ENTITY_HAS_HIT_BOX, "ENTITY_HAS_HIT_BOX");
    table_reference_int(module, ENTITY_HAS_INFRASPACE, "ENTITY_HAS_INFRASPACE");
    table_reference_int(module, ENTITY_HAS_FOLD_ANCHOR, "ENTITY_HAS_FOLD_ANCHOR");
    table_reference_int(module, ENTITY_HAS_SHAKE, "ENTITY_HAS_SHAKE");
    table_reference_int(module, ENTITY_HAS_IS_DEAD, "ENTITY_HAS_IS_DEAD");
    table_reference_int(module, ENTITY_HAS_IS_HOVERED, "ENTITY_HAS_IS_HOVERED");
    table_reference_int(module, ENTITY_HAS_POS, "ENTITY_HAS_POS");
    table_reference_int(module, ENTITY_HAS_MAP_POS, "ENTITY_HAS_MAP_POS");
    table_reference_int(module, MOUSE_POS_HAS_WORLD, "MOUSE_POS_HAS_WORLD");
    table_reference_int(module, MOUSE_POS_HAS_SCREEN, "MOUSE_POS_HAS_SCREEN");
}
