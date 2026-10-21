#include "game/loading_screen.c"
#include "table_reference.h"

void table_reference_game_loading_screen(void) {
    const char *module = "game.loading_screen";
    table_reference_int(module, LOADING_SCREEN_FRAMES, "LOADING_SCREEN_FRAMES");
    table_reference_string(module, LOADING_TEXT, "LOADING_TEXT");
    table_reference_int(module, (long long)sizeof LOADING_TEXT, "SIZEOF.LOADING_TEXT");
    table_reference_int(module, (long long)sizeof((p8_vram_t *)0)->draw_palette, "SIZEOF[\"p8_vram_t.draw_palette\"]");
}
