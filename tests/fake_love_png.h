#ifndef TESTS_FAKE_LOVE_PNG_H
#define TESTS_FAKE_LOVE_PNG_H

/* Writes `rgba`, row-major RGBA8, as an 8-bit RGBA PNG with stored deflate
 * blocks; asserts the file is written. */
void fake_love_png_write(const char *path, const unsigned char *rgba, int width, int height);

#endif
