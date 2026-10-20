#ifndef TESTS_PNG_READER_H
#define TESTS_PNG_READER_H

#include <stddef.h>

/* Decodes only what png_writer emits: 8-bit RGB, stored deflate blocks,
   row filter 0. Returns NULL and fills rgb (top row first), width and
   height, or returns a static message naming the first violation; rgb is
   then unspecified. */
const char *png_reader_decode(const unsigned char *bytes, size_t size, unsigned char *rgb, size_t rgb_capacity, int *width, int *height);

#endif
