#include "p8/vram.h"

#include <string.h>

static unsigned long last_map_version;

unsigned long p8_vram_next_map_version(void) {
    return ++last_map_version;
}

void p8_vram_init(p8_vram_t *mem) {
    int c;

    memset(mem, 0, sizeof(*mem));

    for (c = 0; c < 16; c++) {
        unsigned char entry = (unsigned char)c;
        if (c == 0) {
            entry = (unsigned char)(entry | P8_DRAW_PALETTE_TRANSPARENT_BIT);
        }
        mem->draw_palette[c] = entry;
    }
    mem->map_version = p8_vram_next_map_version();
}
