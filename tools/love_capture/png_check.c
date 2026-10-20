#define STB_IMAGE_IMPLEMENTATION
#define STBI_ONLY_PNG
#define STBI_NO_LINEAR
#define STBI_NO_HDR
#include "stb_image.h"

#include <stdio.h>
#include <stdlib.h>

int main(int argc, char **argv) {
    if (argc != 5) {
        fprintf(stderr, "usage: png_check <file.png> <width> <height> <RRGGBB>\n");
        return 2;
    }
    const char *path = argv[1];
    int expected_width = atoi(argv[2]);
    int expected_height = atoi(argv[3]);
    unsigned long rgb = strtoul(argv[4], NULL, 16);
    unsigned char expected[4] = {(rgb >> 16) & 0xFF, (rgb >> 8) & 0xFF, rgb & 0xFF, 0xFF};

    int width, height, channels;
    unsigned char *pixels = stbi_load(path, &width, &height, &channels, 4);
    if (pixels == NULL) {
        fprintf(stderr, "png_check: %s: %s\n", path, stbi_failure_reason());
        return 1;
    }
    int ok = 1;
    if (width != expected_width || height != expected_height) {
        fprintf(stderr, "png_check: %s is %dx%d, expected %dx%d\n", path, width, height,
                expected_width, expected_height);
        ok = 0;
    }
    for (int i = 0; ok && i < width * height; i++) {
        const unsigned char *p = pixels + 4 * i;
        if (p[0] != expected[0] || p[1] != expected[1] || p[2] != expected[2] ||
            p[3] != expected[3]) {
            fprintf(stderr, "png_check: %s pixel (%d, %d) is %02X%02X%02X alpha %d, expected %s alpha 255\n",
                    path, i % width, i / width, p[0], p[1], p[2], p[3], argv[4]);
            ok = 0;
        }
    }
    stbi_image_free(pixels);
    if (ok)
        printf("%s: %dx%d, every pixel %s alpha 255\n", path, width, height, argv[4]);
    return ok ? 0 : 1;
}
