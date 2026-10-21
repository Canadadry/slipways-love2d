#include "game/gen.h"
#include "table_reference.h"

void table_reference_game_gen(void) {
    const char *module = "game.gen";
    table_reference_int(module, WEIGHTED_PICKER_TICKETS_CAP, "WEIGHTED_PICKER_TICKETS_CAP");
    table_reference_int(module, (long long)sizeof(random_walk_t), "SIZEOF.random_walk_t");
    table_reference_int(module, (long long)sizeof(weighted_picker_t), "SIZEOF.weighted_picker_t");
}
