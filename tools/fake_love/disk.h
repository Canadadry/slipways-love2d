#ifndef FAKE_LOVE_DISK_H
#define FAKE_LOVE_DISK_H

#include <stddef.h>

typedef struct disk disk_t;

typedef enum { DISK_MISSING, DISK_FILE, DISK_DIRECTORY } disk_kind_t;

/* `game_directory` may be NULL; the disk keeps its own copy. The caller
 * releases the result with disk_free. */
disk_t *disk_create(const char *game_directory);
void disk_free(disk_t *disk);

/* NULL when `name` is valid, else a static string naming the problem. */
const char *disk_check_name(const char *name);

/* Asserts a valid name. Copies the bytes over any in-memory file of that
 * name. NULL on success; a static reason for a name holding '/'. */
const char *disk_put(disk_t *disk, const char *name, const void *bytes, size_t length);

/* Asserts a valid name. Memory first, then the game directory. NULL when
 * missing or a directory, else a buffer of `length` bytes the caller frees. */
unsigned char *disk_read(const disk_t *disk, const char *name, size_t *length);

/* Asserts a valid name. `size` is set for a file only; DISK_DIRECTORY
 * comes from the game directory alone. */
disk_kind_t disk_info(const disk_t *disk, const char *name, size_t *size);

#endif
