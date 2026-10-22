#include "game/planet_family.c"
#include "reference_class.h"
#include "reference_data.h"
#include "table_reference.h"

#define MODULE "game.planet_family"

/* The kind records the kind definitions point at, listed by the game_data unit. */
static const reference_data_name_t NAMES[] = {
    {&structure_kind_record, "structure_kind_record"},
    {&trading_hub_kind_record, "trading_hub_kind_record"},
    {&processor_kind_record, "processor_kind_record"},
    {&ascension_kind_record, "ascension_kind_record"},
    {&protostar_kind_record, "protostar_kind_record"},
    {&burst_kind_record, "burst_kind_record"},
};

static const reference_data_t DATA = {MODULE, NAMES, (int)(sizeof NAMES / sizeof NAMES[0])};

static void render_kind(const char *name, const entity_kind_t *kind) {
    char path[256];
    snprintf(path, sizeof path, "%s.record", name);
    table_reference_function(MODULE, kind->new_entity != NULL, "%s.new_entity", name);
    reference_data_pointer(&DATA, kind->record, path);
}

void table_reference_game_planet_family(void) {
    reference_class(MODULE, "structure_class_def", &structure_class_def);
    reference_class(MODULE, "trading_hub_class_def", &trading_hub_class_def);
    reference_class(MODULE, "processor_class_def", &processor_class_def);
    reference_class(MODULE, "ascension_class_def", &ascension_class_def);
    reference_class(MODULE, "protostar_class_def", &protostar_class_def);
    reference_class(MODULE, "burst_class_def", &burst_class_def);
    render_kind("structure_kind_def", &structure_kind_def);
    render_kind("trading_hub_kind_def", &trading_hub_kind_def);
    render_kind("processor_kind_def", &processor_kind_def);
    render_kind("ascension_kind_def", &ascension_kind_def);
    render_kind("protostar_kind_def", &protostar_kind_def);
    render_kind("burst_kind_def", &burst_kind_def);
    for (int i = 0; i < (int)(sizeof planet_kinds / sizeof planet_kinds[0]); i++) {
        table_reference_string(MODULE, planet_kinds[i].class_name, "planet_kinds[%d].class_name", i);
        table_reference_function(MODULE, planet_kinds[i].kind != NULL, "planet_kinds[%d].kind", i);
    }
    table_reference_int(MODULE, (long long)sizeof(planet_t), "SIZEOF.planet_t");
}
