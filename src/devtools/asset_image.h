#ifndef DEVTOOLS_ASSET_IMAGE_H
#define DEVTOOLS_ASSET_IMAGE_H

#include "p8/gfx.h"
#include "p8/vram.h"

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

#endif
