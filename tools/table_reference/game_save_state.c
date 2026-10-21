#include "game/save_state.c"
#include "table_reference.h"

void table_reference_game_save_state(void) {
    const char *module = "game.save_state";
    table_reference_int(module, SAVE_STATE_MAX_BYTES, "SAVE_STATE_MAX_BYTES");
    table_reference_int(module, SAVE_STATE_VERSION, "SAVE_STATE_VERSION");
    table_reference_int(module, SAVE_STATE_RECENT_COUNT, "SAVE_STATE_RECENT_COUNT");
    table_reference_int(module, SAVE_STATE_NODES_CAP, "SAVE_STATE_NODES_CAP");
    table_reference_int(module, SAVE_STATE_SLIPWAYS_CAP, "SAVE_STATE_SLIPWAYS_CAP");
    table_reference_int(module, SAVE_STATE_ROUTES_CAP, "SAVE_STATE_ROUTES_CAP");
    table_reference_int(module, SAVE_STATE_ROUTE_INDICES_CAP, "SAVE_STATE_ROUTE_INDICES_CAP");
    table_reference_int(module, SAVE_STATE_NODE_SLIPWAYS_CAP, "SAVE_STATE_NODE_SLIPWAYS_CAP");
    table_reference_int(module, SAVE_STATE_NEEDS_CAP, "SAVE_STATE_NEEDS_CAP");
    table_reference_int(module, SAVE_STATE_PLANET_ID_MAX, "SAVE_STATE_PLANET_ID_MAX");
    table_reference_int(module, SAVE_NODE_PLANET, "SAVE_NODE_PLANET");
    table_reference_int(module, SAVE_NODE_PROTOSTAR, "SAVE_NODE_PROTOSTAR");
    table_reference_int(module, SAVE_NODE_STRUCTURE, "SAVE_NODE_STRUCTURE");
    table_reference_int(module, SAVE_NODE_PROCESSOR, "SAVE_NODE_PROCESSOR");
    table_reference_int(module, SAVE_NODE_SLIPGATE, "SAVE_NODE_SLIPGATE");
    table_reference_int(module, SAVE_NODE_INFRASPACE_GATE, "SAVE_NODE_INFRASPACE_GATE");
    table_reference_int(module, SAVE_NODE_TRADING_HUB, "SAVE_NODE_TRADING_HUB");
    table_reference_int(module, SAVE_NODE_SYNTHESIZER, "SAVE_NODE_SYNTHESIZER");
    table_reference_int(module, SAVE_NODE_ASCENSION, "SAVE_NODE_ASCENSION");
    table_reference_int(module, SAVE_NODE_KIND_COUNT, "SAVE_NODE_KIND_COUNT");
    table_reference_int(module, SAVE_NODE_HAS_POS, "SAVE_NODE_HAS_POS");
    table_reference_int(module, SAVE_NODE_HAS_MAP_POS, "SAVE_NODE_HAS_MAP_POS");
    table_reference_int(module, SAVE_NODE_HAS_PLANET_ID, "SAVE_NODE_HAS_PLANET_ID");
    table_reference_int(module, SAVE_NODE_HAS_NEEDS, "SAVE_NODE_HAS_NEEDS");
    table_reference_int(module, SAVE_NODE_HAS_DEVELOPMENT_TIER, "SAVE_NODE_HAS_DEVELOPMENT_TIER");
    table_reference_int(module, SAVE_NODE_HAS_PRODUCTION_BONUS, "SAVE_NODE_HAS_PRODUCTION_BONUS");
    table_reference_int(module, SAVE_NODE_HAS_PRODUCTION, "SAVE_NODE_HAS_PRODUCTION");
    table_reference_int(module, SAVE_NODE_PRODUCTION_HAS_RESOURCE, "SAVE_NODE_PRODUCTION_HAS_RESOURCE");
    table_reference_int(module, SAVE_NODE_SYNTHESIZER_SPENT, "SAVE_NODE_SYNTHESIZER_SPENT");
    table_reference_int(module, SAVE_NODE_HAS_OUTGOING_ROUTES, "SAVE_NODE_HAS_OUTGOING_ROUTES");
    table_reference_int(module, SAVE_NODE_HAS_STATE_TIME, "SAVE_NODE_HAS_STATE_TIME");
    table_reference_int(module, SAVE_NODE_KNOWN_FLAGS, "SAVE_NODE_KNOWN_FLAGS");
    table_reference_int(module, (long long)sizeof magic, "SIZEOF.magic");
    table_reference_int(module, (long long)sizeof(((save_node_t *)0)->planet_id), "SIZEOF[\"save_node_t.planet_id\"]");
    for (size_t i = 0; i < sizeof magic; i++) {
        table_reference_int(module, magic[i], "magic[%d]", (int)i);
    }
    for (int i = 0; i < 256; i++) {
        table_reference_int(module, (long long)crc32_table[i], "crc32_table[%d]", i);
    }
}
