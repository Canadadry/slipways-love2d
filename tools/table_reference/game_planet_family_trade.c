#include "game/planet_family_trade.c"
#include "reference_class.h"
#include "reference_data.h"
#include "table_reference.h"

#define MODULE "game.planet_family_trade"

/* The kind records the kind definitions point at, listed by the game_data unit. */
static const reference_data_name_t NAMES[] = {
    {&slipgate_kind_record, "slipgate_kind_record"},
    {&infraspace_gate_kind_record, "infraspace_gate_kind_record"},
    {&synthesizer_kind_record, "synthesizer_kind_record"},
};

static const reference_data_t DATA = {MODULE, NAMES, (int)(sizeof NAMES / sizeof NAMES[0])};

static void render_kind(const char *name, const entity_kind_t *kind) {
    char path[256];
    snprintf(path, sizeof path, "%s.record", name);
    table_reference_function(MODULE, kind->new_entity != NULL, "%s.new_entity", name);
    reference_data_pointer(&DATA, kind->record, path);
}

void table_reference_game_planet_family_trade(void) {
    reference_data_int_seq(&DATA, &synthesizer_synthesizable_resources_spent, "synthesizer_synthesizable_resources_spent");
    reference_class(MODULE, "slipgate_left_press_empty_class_def", &slipgate_left_press_empty_class_def);
    reference_header(MODULE, "slipgate_left_press_empty", &slipgate_left_press_empty.header,
                     "slipgate_left_press_empty_class_def", &slipgate_left_press_empty_class_def);
    reference_class(MODULE, "slipgate_class_def", &slipgate_class_def);
    reference_class(MODULE, "infraspace_gate_class_def", &infraspace_gate_class_def);
    reference_class(MODULE, "synthesizer_class_def", &synthesizer_class_def);
    render_kind("slipgate_kind_def", &slipgate_kind_def);
    render_kind("infraspace_gate_kind_def", &infraspace_gate_kind_def);
    render_kind("synthesizer_kind_def", &synthesizer_kind_def);
    table_reference_int(MODULE, (long long)sizeof(planet_t), "SIZEOF.planet_t");
}
