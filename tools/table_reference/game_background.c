#include "game/background.c"
#include "reference_class.h"
#include "table_reference.h"

void table_reference_game_background(void) {
    const char *module = "game.background";
    table_reference_int(module, BACKGROUND_GRID_EDGE, "BACKGROUND_GRID_EDGE");
    reference_class(module, "background_class_def", &background_class_def);
    table_reference_int(module, (long long)sizeof(background_t), "SIZEOF.background_t");
    table_reference_int(module, (long long)sizeof((p8_vram_t *)0)->draw_palette,
                        "SIZEOF[\"p8_vram_t.draw_palette\"]");
}
