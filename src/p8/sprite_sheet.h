#ifndef P8_SPRITE_SHEET_H
#define P8_SPRITE_SHEET_H

#include "p8/vram.h"

#define P8_SPRITE_SHEET_PACKED_SIZE (P8_SPRITE_SHEET_WIDTH * P8_SPRITE_SHEET_HEIGHT / 2)

/* Writes only mem->sprite_sheet. */
void p8_sprite_sheet_load(p8_vram_t *mem);

/* P8_SPRITE_SHEET_PACKED_SIZE bytes, static, never freed. Pixel (x, y) is in
 * byte y * 64 + x / 2: low nibble for an even x, high nibble for an odd x. */
const unsigned char *p8_sprite_sheet_packed(void);

#endif
