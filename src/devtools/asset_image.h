#ifndef DEVTOOLS_ASSET_IMAGE_H
#define DEVTOOLS_ASSET_IMAGE_H

#include "p8/gfx.h"
#include "p8/vram.h"

#include <stddef.h>

#define ASSET_IMAGE_SPRITES_WIDTH P8_SPRITE_SHEET_WIDTH
#define ASSET_IMAGE_SPRITES_HEIGHT P8_SPRITE_SHEET_HEIGHT

/* rgb holds ASSET_IMAGE_SPRITES_WIDTH * ASSET_IMAGE_SPRITES_HEIGHT * 3 bytes, top row first. */
void asset_image_fill_sprites(unsigned char *rgb);

#define ASSET_IMAGE_FONT_COLUMNS 16
#define ASSET_IMAGE_FONT_ROWS 6
#define ASSET_IMAGE_FONT_FIRST_CODE 32
#define ASSET_IMAGE_FONT_WIDTH (ASSET_IMAGE_FONT_COLUMNS * P8_FONT_GLYPH_WIDTH)
#define ASSET_IMAGE_FONT_HEIGHT (ASSET_IMAGE_FONT_ROWS * P8_FONT_GLYPH_HEIGHT)

/* rgb holds ASSET_IMAGE_FONT_WIDTH * ASSET_IMAGE_FONT_HEIGHT * 3 bytes, top row first. Code c sits
   in column (c - 32) % 16, row (c - 32) / 16; lit pixels are colour 7, the rest colour 0. */
void asset_image_fill_font(unsigned char *rgb);

#define ASSET_IMAGE_PACKED_LUA_CAPACITY (56 * 1024)

/* Writes game/p8/sprite_sheet_packed.lua's text, NUL-terminated, into text: the packed sprite sheet
   as a Lua table indexed from 0. Returns its length without the NUL, or 0 when capacity is too small. */
size_t asset_image_format_packed_lua(char *text, size_t capacity);

#endif
