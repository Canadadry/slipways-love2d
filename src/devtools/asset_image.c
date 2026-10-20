#include "devtools/asset_image.h"

#include "p8/palette.h"
#include "p8/sprite_sheet.h"

static void put_color(unsigned char *pixel, int color) {
    pixel[0] = P8_PALETTE_RGB[color].red;
    pixel[1] = P8_PALETTE_RGB[color].green;
    pixel[2] = P8_PALETTE_RGB[color].blue;
}

void asset_image_fill_sprites(unsigned char *rgb) {
    const unsigned char *packed = p8_sprite_sheet_packed();
    for (int y = 0; y < ASSET_IMAGE_SPRITES_HEIGHT; y++) {
        for (int x = 0; x < ASSET_IMAGE_SPRITES_WIDTH; x++) {
            unsigned char byte = packed[(y * ASSET_IMAGE_SPRITES_WIDTH + x) / 2];
            int color = (x % 2 == 0) ? (byte & 0x0F) : (byte >> 4);
            put_color(&rgb[(y * ASSET_IMAGE_SPRITES_WIDTH + x) * 3], color);
        }
    }
}

void asset_image_fill_font(unsigned char *rgb) {
    for (int cell = 0; cell < ASSET_IMAGE_FONT_COLUMNS * ASSET_IMAGE_FONT_ROWS; cell++) {
        const unsigned char *rows = p8_font_glyph_rows((char)(ASSET_IMAGE_FONT_FIRST_CODE + cell));
        int left = cell % ASSET_IMAGE_FONT_COLUMNS * P8_FONT_GLYPH_WIDTH;
        int top = cell / ASSET_IMAGE_FONT_COLUMNS * P8_FONT_GLYPH_HEIGHT;
        for (int row = 0; row < P8_FONT_GLYPH_HEIGHT; row++) {
            for (int column = 0; column < P8_FONT_GLYPH_WIDTH; column++) {
                int lit = (rows[row] >> (P8_FONT_GLYPH_WIDTH - 1 - column)) & 1;
                put_color(&rgb[((top + row) * ASSET_IMAGE_FONT_WIDTH + left + column) * 3], lit ? 7 : 0);
            }
        }
    }
}
