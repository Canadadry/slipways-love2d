#ifndef FAKE_LOVE_PNG_FILE_H
#define FAKE_LOVE_PNG_FILE_H

/* NULL on success, with `*pixels` row-major RGBA8 the caller frees with
 * free(); else a static reason, `*pixels` left unset. */
const char *png_file_load(const char *path, unsigned char **pixels, int *width, int *height);

#endif
