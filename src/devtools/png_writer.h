#ifndef DEVTOOLS_PNG_WRITER_H
#define DEVTOOLS_PNG_WRITER_H

#include <stddef.h>

size_t png_writer_size(int width, int height);

size_t png_writer_encode(const unsigned char *rgb, int width, int height, unsigned char *out, size_t capacity);

#endif
