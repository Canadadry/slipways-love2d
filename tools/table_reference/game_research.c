#include "game/research.c"
#include "reference_class.h"
#include "table_reference.h"

void table_reference_game_research(void) {
    const char *module = "game.research";
    reference_class(module, "research_project_class_def", &research_project_class_def);
    table_reference_int(module, RESEARCH_PROJECT_ID_NONE.index, "RESEARCH_PROJECT_ID_NONE.index");
    table_reference_int(module, RESEARCH_PROJECT_ID_NONE.epoch, "RESEARCH_PROJECT_ID_NONE.epoch");
    table_reference_int(module, (long long)sizeof(need_t), "SIZEOF.need_t");
}
