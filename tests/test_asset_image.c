#include "devtools/asset_image.h"
#include "devtools/png_writer.h"
#include "p8/gfx.h"
#include "p8/palette.h"
#include "p8/sprite_sheet.h"
#include "png_reader.h"

#include <assert.h>
#include <stdio.h>
#include <string.h>

enum {
    FILE_CAPACITY = 64 * 1024,
    SHEET_SIZE = 128,
    FONT_COLUMNS = 16,
    FONT_FIRST_CODE = 32,
    FONT_LAST_CODE = 127,
    FONT_WIDTH = 48,
    FONT_HEIGHT = 30,
    LIT = 7,
    UNLIT = 0
};

static unsigned char file_bytes[FILE_CAPACITY];
static unsigned char decoded[SHEET_SIZE * SHEET_SIZE * 3];

static size_t read_committed(const char *path) {
    FILE *file = fopen(path, "rb");
    assert(file != NULL && "run from the repository root after make asset-export");
    size_t size = fread(file_bytes, 1, sizeof file_bytes, file);
    assert(feof(file) && "committed PNG larger than FILE_CAPACITY");
    fclose(file);
    return size;
}

static void decode_committed(const char *path, int expected_width, int expected_height) {
    size_t size = read_committed(path);
    int width = 0;
    int height = 0;
    const char *error = png_reader_decode(file_bytes, size, decoded, sizeof decoded, &width, &height);
    if (error != NULL) fprintf(stderr, "%s: %s\n", path, error);
    assert(error == NULL);
    assert(width == expected_width && height == expected_height);
}

static void assert_pixel_is_color(int width, int x, int y, int color) {
    const unsigned char *pixel = &decoded[(y * width + x) * 3];
    assert(pixel[0] == P8_PALETTE_RGB[color].red);
    assert(pixel[1] == P8_PALETTE_RGB[color].green);
    assert(pixel[2] == P8_PALETTE_RGB[color].blue);
}

static int sheet_pixel(int x, int y) {
    unsigned char byte = p8_sprite_sheet_packed()[y * 64 + x / 2];
    return (x % 2 == 0) ? (byte & 0x0F) : (byte >> 4);
}

static void test_committed_sprites_png_holds_the_sheet(void) {
    decode_committed("game/sprites.png", SHEET_SIZE, SHEET_SIZE);
    for (int y = 0; y < SHEET_SIZE; y++) {
        for (int x = 0; x < SHEET_SIZE; x++) assert_pixel_is_color(SHEET_SIZE, x, y, sheet_pixel(x, y));
    }
}

static void font_cell_origin(int code, int *x, int *y) {
    *x = (code - FONT_FIRST_CODE) % FONT_COLUMNS * P8_FONT_GLYPH_WIDTH;
    *y = (code - FONT_FIRST_CODE) / FONT_COLUMNS * P8_FONT_GLYPH_HEIGHT;
}

static void test_committed_font_png_holds_every_glyph_in_its_grid_cell(void) {
    decode_committed("game/font.png", FONT_WIDTH, FONT_HEIGHT);
    for (int code = FONT_FIRST_CODE; code <= FONT_LAST_CODE; code++) {
        const unsigned char *rows = p8_font_glyph_rows((char)code);
        int left;
        int top;
        font_cell_origin(code, &left, &top);
        for (int row = 0; row < P8_FONT_GLYPH_HEIGHT; row++) {
            for (int column = 0; column < P8_FONT_GLYPH_WIDTH; column++) {
                int lit = (rows[row] >> (P8_FONT_GLYPH_WIDTH - 1 - column)) & 1;
                assert_pixel_is_color(FONT_WIDTH, left + column, top + row, lit ? LIT : UNLIT);
            }
        }
    }
}

static int is_palette_triple(const unsigned char *pixel) {
    for (int color = 0; color < P8_PALETTE_COLORS; color++) {
        const p8_rgb_t *rgb = &P8_PALETTE_RGB[color];
        if (pixel[0] == rgb->red && pixel[1] == rgb->green && pixel[2] == rgb->blue) return 1;
    }
    return 0;
}

static void test_committed_pngs_hold_only_palette_colors(void) {
    decode_committed("game/sprites.png", SHEET_SIZE, SHEET_SIZE);
    for (int i = 0; i < SHEET_SIZE * SHEET_SIZE; i++) assert(is_palette_triple(&decoded[i * 3]));
    decode_committed("game/font.png", FONT_WIDTH, FONT_HEIGHT);
    for (int i = 0; i < FONT_WIDTH * FONT_HEIGHT; i++) assert(is_palette_triple(&decoded[i * 3]));
}

static int font_cells_equal(int code, int other) {
    int left;
    int top;
    int other_left;
    int other_top;
    font_cell_origin(code, &left, &top);
    font_cell_origin(other, &other_left, &other_top);
    for (int row = 0; row < P8_FONT_GLYPH_HEIGHT; row++) {
        const unsigned char *line = &decoded[((top + row) * FONT_WIDTH + left) * 3];
        const unsigned char *other_line = &decoded[((other_top + row) * FONT_WIDTH + other_left) * 3];
        if (memcmp(line, other_line, P8_FONT_GLYPH_WIDTH * 3) != 0) return 0;
    }
    return 1;
}

static void assert_font_cell_is_color(int code, int color) {
    int left;
    int top;
    font_cell_origin(code, &left, &top);
    for (int row = 0; row < P8_FONT_GLYPH_HEIGHT; row++) {
        for (int column = 0; column < P8_FONT_GLYPH_WIDTH; column++) {
            assert_pixel_is_color(FONT_WIDTH, left + column, top + row, color);
        }
    }
}

static void test_font_grid_spot_checks(void) {
    decode_committed("game/font.png", FONT_WIDTH, FONT_HEIGHT);
    assert(font_cells_equal('v', 'U'));
    assert(font_cells_equal('V', 'U'));
    assert(font_cells_equal('a', 'A'));
    assert(!font_cells_equal('A', 'B'));
    assert_font_cell_is_color(127, LIT);
    assert_font_cell_is_color(' ', UNLIT);
    assert(font_cells_equal('#', 127));
}

static unsigned char exported_rgb[SHEET_SIZE * SHEET_SIZE * 3];
static unsigned char exported_png[FILE_CAPACITY];

static void assert_committed_equals_export(const char *path, int width, int height) {
    size_t size = png_writer_encode(exported_rgb, width, height, exported_png, sizeof exported_png);
    assert(size > 0);
    size_t committed_size = read_committed(path);
    if (committed_size != size || memcmp(file_bytes, exported_png, size) != 0) {
        fprintf(stderr, "%s is stale: run make asset-export\n", path);
    }
    assert(committed_size == size);
    assert(memcmp(file_bytes, exported_png, size) == 0);
}

static void test_committed_pngs_are_what_the_export_writes_now(void) {
    asset_image_fill_sprites(exported_rgb);
    assert_committed_equals_export("game/sprites.png", ASSET_IMAGE_SPRITES_WIDTH, ASSET_IMAGE_SPRITES_HEIGHT);
    asset_image_fill_font(exported_rgb);
    assert_committed_equals_export("game/font.png", ASSET_IMAGE_FONT_WIDTH, ASSET_IMAGE_FONT_HEIGHT);
}

int main(void) {
    test_committed_sprites_png_holds_the_sheet();
    test_committed_font_png_holds_every_glyph_in_its_grid_cell();
    test_committed_pngs_hold_only_palette_colors();
    test_font_grid_spot_checks();
    test_committed_pngs_are_what_the_export_writes_now();

    printf("test_asset_image: all tests passed\n");
    return 0;
}
