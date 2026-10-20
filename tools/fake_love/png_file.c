#include "png_file.h"

#define STB_IMAGE_IMPLEMENTATION
#define STBI_ONLY_PNG
#define STBI_NO_LINEAR
#define STBI_NO_HDR
#include "stb_image.h"

const char *png_file_load(const char *path, unsigned char **pixels, int *width, int *height) {
    FILE *file = fopen(path, "rb");
    if (file == NULL)
        return "cannot be opened";
    int channels;
    unsigned char *decoded = stbi_load_from_file(file, width, height, &channels, 4);
    fclose(file);
    if (decoded == NULL)
        return stbi_failure_reason();
    *pixels = decoded;
    return NULL;
}
