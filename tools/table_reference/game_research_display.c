#include "game/research_display.c"
#include "table_reference.h"

void table_reference_game_research_display(void) {
    const char *module = "game.research_display";
    table_reference_int(module, (long long)sizeof(research_project_tooltip_t), "SIZEOF.research_project_tooltip_t");
}
