#include "game/economy.c"
#include "reference_class.h"
#include "table_reference.h"

void table_reference_game_economy(void) {
    const char *module = "game.economy";
    reference_class(module, "economy_class_def", &economy_class_def);
    table_reference_int(module, economy_archetype.credits, "economy_archetype.credits");
    table_reference_int(module, economy_archetype.era, "economy_archetype.era");
    table_reference_int(module, economy_archetype.year_ticks, "economy_archetype.year_ticks");
    table_reference_int(module, economy_archetype.year, "economy_archetype.year");
    table_reference_int(module, (long long)sizeof(economy_t), "SIZEOF.economy_t");
}
