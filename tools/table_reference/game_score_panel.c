#include "game/score_panel.c"
#include "reference_class.h"
#include "table_reference.h"

void table_reference_game_score_panel(void) {
    const char *module = "game.score_panel";
    table_reference_int(module, SCORE_PANEL_LAYOUT_BLOCK_HAS_FIXED_POS, "SCORE_PANEL_LAYOUT_BLOCK_HAS_FIXED_POS");
    table_reference_int(module, SCORE_PANEL_LAYOUT_BLOCK_HAS_ROW_STEP, "SCORE_PANEL_LAYOUT_BLOCK_HAS_ROW_STEP");
    table_reference_int(module, SCORE_PANEL_TEXT_CAP, "SCORE_PANEL_TEXT_CAP");
    reference_class(module, "score_panel_layout_block_class_def", &score_panel_layout_block_class_def);
    reference_header(module, "g_score_panel_layout_block", &g_score_panel_layout_block.header,
                     "score_panel_layout_block_class_def", &score_panel_layout_block_class_def);
    table_reference_int(module, g_score_panel_layout_block.has, "g_score_panel_layout_block.has_");
    table_reference_int(module, g_score_panel_layout_block.fixed_pos.x, "g_score_panel_layout_block.fixed_pos.x");
    table_reference_int(module, g_score_panel_layout_block.fixed_pos.y, "g_score_panel_layout_block.fixed_pos.y");
    table_reference_int(module, g_score_panel_layout_block.row_step.x, "g_score_panel_layout_block.row_step.x");
    table_reference_int(module, g_score_panel_layout_block.row_step.y, "g_score_panel_layout_block.row_step.y");
    table_reference_int(module, g_score_panel_layout_block.back_color, "g_score_panel_layout_block.back_color");
    reference_class(module, "score_panel_class_def", &score_panel_class_def);
    table_reference_int(module, (long long)sizeof(score_panel_t), "SIZEOF.score_panel_t");
}
