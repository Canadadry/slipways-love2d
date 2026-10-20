#include "devtools/dump.h"

#include "p8/gfx.h"

#include <assert.h>

static const char HEX_DIGITS[] = "0123456789abcdef";

int dump_pixels(const unsigned char *pixels, FILE *out) {
    if (fprintf(out, "%d %d\n", P8_SCREEN_WIDTH, P8_SCREEN_HEIGHT) < 0) {
        return 0;
    }

    for (int y = 0; y < P8_SCREEN_HEIGHT; y++) {
        char line[P8_SCREEN_WIDTH + 1];
        for (int x = 0; x < P8_SCREEN_WIDTH; x++) {
            unsigned char pixel = pixels[y * P8_SCREEN_WIDTH + x];
            assert(pixel <= 0x0f && "one 4-bit color index per byte");
            line[x] = HEX_DIGITS[pixel];
        }
        line[P8_SCREEN_WIDTH] = '\n';
        if (fwrite(line, 1, sizeof(line), out) != sizeof(line)) {
            return 0;
        }
    }

    return 1;
}

int dump_screen(const p8_vram_t *mem, FILE *out) {
    return dump_pixels(&mem->screen[0][0], out);
}
