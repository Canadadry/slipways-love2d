#include "p8/sprite_sheet.h"

#include "p8/sprite_sheet_packed.h"

typedef char sprite_sheet_packed_size_check[
    sizeof(p8_sprite_sheet_packed_bytes) == P8_SPRITE_SHEET_PACKED_SIZE ? 1 : -1];

void p8_sprite_sheet_load(p8_vram_t *mem) {
    int y;
    int x;

    for (y = 0; y < P8_SPRITE_SHEET_HEIGHT; y++) {
        for (x = 0; x < P8_SPRITE_SHEET_WIDTH; x++) {
            unsigned char byte = p8_sprite_sheet_packed_bytes[y * (P8_SPRITE_SHEET_WIDTH / 2) + x / 2];
            mem->sprite_sheet[y][x] = (unsigned char)((x % 2 == 0) ? (byte & 0x0F) : (byte >> 4));
        }
    }
}

const unsigned char *p8_sprite_sheet_packed(void) {
    return p8_sprite_sheet_packed_bytes;
}
