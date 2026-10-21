#include "game/scroll.c"
#include "reference_class.h"
#include "table_reference.h"

void table_reference_game_scroll(void) {
    const char *module = "game.scroll";
    reference_class(module, "camera_class_def", &camera_class_def);
    table_reference_int(module, (long long)sizeof(camera_t), "SIZEOF.camera_t");
}
