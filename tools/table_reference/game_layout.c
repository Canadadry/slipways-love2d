#include "game/layout.c"
#include "reference_class.h"
#include "table_reference.h"

void table_reference_game_layout(void) {
    const char *module = "game.layout";
    reference_class(module, "layout_row_class_def", &layout_row_class_def);
    reference_class(module, "tooltip_class_def", &tooltip_class_def);
    table_reference_int(module, LAYOUT_ROW_ITEM_NONE, "LAYOUT_ROW_ITEM_NONE");
    table_reference_int(module, LAYOUT_ROW_ITEM_TEXT, "LAYOUT_ROW_ITEM_TEXT");
    table_reference_int(module, LAYOUT_ROW_ITEM_SPRITE, "LAYOUT_ROW_ITEM_SPRITE");
    table_reference_int(module, LAYOUT_ROW_CELL_NOTHING, "LAYOUT_ROW_CELL_NOTHING");
    table_reference_int(module, LAYOUT_ROW_CELL_FIXED, "LAYOUT_ROW_CELL_FIXED");
    table_reference_int(module, LAYOUT_ROW_CELL_STRING, "LAYOUT_ROW_CELL_STRING");
    table_reference_int(module, LAYOUT_ROW_CELL_TUPLE, "LAYOUT_ROW_CELL_TUPLE");
    table_reference_int(module, LAYOUT_ROW_CELL_TRUE, "LAYOUT_ROW_CELL_TRUE");
    table_reference_int(module, LAYOUT_ROW_SCALAR_NOTHING, "LAYOUT_ROW_SCALAR_NOTHING");
    table_reference_int(module, LAYOUT_ROW_SCALAR_FIXED, "LAYOUT_ROW_SCALAR_FIXED");
    table_reference_int(module, LAYOUT_ROW_TUPLE_CAP, "LAYOUT_ROW_TUPLE_CAP");
    table_reference_int(module, LAYOUT_ROW_KEYS_CAP, "LAYOUT_ROW_KEYS_CAP");
    table_reference_int(module, (long long)sizeof(layout_row_item_t), "SIZEOF.layout_row_item_t");
    table_reference_int(module, (long long)sizeof(layout_row_t), "SIZEOF.layout_row_t");
    table_reference_int(module, (long long)sizeof(layout_row_t *), "SIZEOF[\"layout_row_t *\"]");
    table_reference_int(module, (long long)sizeof(tooltip_t), "SIZEOF.tooltip_t");
}
