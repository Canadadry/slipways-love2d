#ifndef FAKE_LOVE_PNG_FILE_H
#define FAKE_LOVE_PNG_FILE_H

#include <stddef.h>

/* NULL on success, with `*pixels` row-major RGBA8 the caller frees with
 * free(); else a static reason, `*pixels` left unset. */
const char *png_file_load(const char *path, unsigned char **pixels, int *width, int *height);

/* NULL on success, with `*pixels` row-major RGBA8 the caller frees with
 * free(); else a reason valid until the next call, `*pixels` left unset.
 * Accepts only what LÖVE 11.5 was measured to decode to the same bytes:
 * 8-bit RGB or RGBA, sides 1 to `max_side`. 16-bit (LÖVE makes rgba16),
 * grey and palette are refused on purpose, and so is every file whose CRC,
 * Adler-32 or image data length LÖVE checks and stb_image does not. */
const char *png_file_decode(const unsigned char *bytes, size_t length, int max_side, unsigned char **pixels, int *width, int *height);

#endif
