#include "game/structure_icon.c"
#include "reference_class.h"
#include "table_reference.h"

void table_reference_game_structure_icon(void) {
    const char *module = "game.structure_icon";
    reference_class(module, "structure_icon_kind_view_class_def", &structure_icon_kind_view_class_def);
    reference_class(module, "structure_icon_class_def", &structure_icon_class_def);
    table_reference_int(module, (long long)sizeof(structure_icon_t), "SIZEOF.structure_icon_t");
}
