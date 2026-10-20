#include "devtools/asset_image.h"
#include "devtools/png_writer.h"

#include <stdio.h>

enum { PATH_CAPACITY = 4096 };

static unsigned char sprites_rgb[ASSET_IMAGE_SPRITES_WIDTH * ASSET_IMAGE_SPRITES_HEIGHT * 3];
static unsigned char font_rgb[ASSET_IMAGE_FONT_WIDTH * ASSET_IMAGE_FONT_HEIGHT * 3];
static unsigned char png_bytes[ASSET_IMAGE_SPRITES_WIDTH * ASSET_IMAGE_SPRITES_HEIGHT * 3 + 4096];

static int write_png(const char *directory, const char *name, const unsigned char *rgb, int width, int height) {
    char path[PATH_CAPACITY];
    int length = snprintf(path, sizeof path, "%s/%s", directory, name);
    size_t size = png_writer_encode(rgb, width, height, png_bytes, sizeof png_bytes);
    FILE *file = (length > 0 && length < (int)sizeof path && size > 0) ? fopen(path, "wb") : NULL;
    int written = file != NULL && fwrite(png_bytes, 1, size, file) == size;
    if (file != NULL && fclose(file) != 0) written = 0;
    if (!written) {
        fprintf(stderr, "%s: cannot write\n", path);
        return 0;
    }
    printf("%s\n", path);
    return 1;
}

int main(int argc, char **argv) {
    if (argc != 2) {
        fprintf(stderr, "usage: asset_export <directory>\n");
        return 2;
    }
    asset_image_fill_sprites(sprites_rgb);
    asset_image_fill_font(font_rgb);
    if (!write_png(argv[1], "sprites.png", sprites_rgb, ASSET_IMAGE_SPRITES_WIDTH, ASSET_IMAGE_SPRITES_HEIGHT)) return 1;
    if (!write_png(argv[1], "font.png", font_rgb, ASSET_IMAGE_FONT_WIDTH, ASSET_IMAGE_FONT_HEIGHT)) return 1;
    return 0;
}
