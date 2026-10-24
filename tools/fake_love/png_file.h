#ifndef FAKE_LOVE_PNG_FILE_H
#define FAKE_LOVE_PNG_FILE_H

#include <stdbool.h>
#include <stddef.h>

const char *png_file_load(const char *path, unsigned char **pixels, int *width, int *height);

const char *png_file_decode(const unsigned char *bytes, size_t length, int max_side, unsigned char **pixels, int *width, int *height);

bool png_file_write(const char *path, const unsigned char *rgba, int width, int height);

#endif
