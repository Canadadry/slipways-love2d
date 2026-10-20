#define _POSIX_C_SOURCE 200809L

#include "disk.h"

#include <assert.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>

typedef struct {
    char *name;
    unsigned char *bytes;
    size_t length;
} memory_file_t;

struct disk {
    char *game_directory;
    memory_file_t *files;
    size_t file_count;
};

static void *allocate(size_t size) {
    void *memory = malloc(size > 0 ? size : 1);
    assert(memory != NULL && "fake disk: out of memory");
    return memory;
}

static unsigned char *copy_bytes(const void *bytes, size_t length) {
    unsigned char *copy = allocate(length);
    if (length > 0) {
        memcpy(copy, bytes, length);
    }
    return copy;
}

disk_t *disk_create(const char *game_directory) {
    disk_t *disk = allocate(sizeof(*disk));
    *disk = (disk_t){0};
    if (game_directory != NULL) {
        disk->game_directory = (char *)copy_bytes(game_directory, strlen(game_directory) + 1);
    }
    return disk;
}

void disk_free(disk_t *disk) {
    if (disk == NULL) {
        return;
    }
    for (size_t i = 0; i < disk->file_count; i++) {
        free(disk->files[i].name);
        free(disk->files[i].bytes);
    }
    free(disk->files);
    free(disk->game_directory);
    free(disk);
}

const char *disk_check_name(const char *name) {
    if (name[0] == '\0') {
        return "empty path";
    }
    if (name[0] == '/') {
        return "absolute path";
    }
    if (strpbrk(name, "\\:") != NULL) {
        return "backslash or colon in path";
    }
    for (const char *element = name;; element++) {
        size_t length = strcspn(element, "/");
        if (length == 0) {
            return "empty path element";
        }
        if ((length == 1 && element[0] == '.') || (length == 2 && element[0] == '.' && element[1] == '.')) {
            return "'.' or '..' path element";
        }
        element += length;
        if (*element == '\0') {
            return NULL;
        }
    }
}

static memory_file_t *find_memory_file(const disk_t *disk, const char *name) {
    for (size_t i = 0; i < disk->file_count; i++) {
        if (strcmp(disk->files[i].name, name) == 0) {
            return &disk->files[i];
        }
    }
    return NULL;
}

const char *disk_put(disk_t *disk, const char *name, const void *bytes, size_t length) {
    assert(disk_check_name(name) == NULL && "fake disk: check the name before putting it");
    if (strchr(name, '/') != NULL) {
        return "the fake disk has no directory to write into";
    }
    memory_file_t *file = find_memory_file(disk, name);
    if (file == NULL) {
        memory_file_t *grown = realloc(disk->files, (disk->file_count + 1) * sizeof(*grown));
        assert(grown != NULL && "fake disk: out of memory");
        disk->files = grown;
        file = &disk->files[disk->file_count++];
        file->name = (char *)copy_bytes(name, strlen(name) + 1);
    } else {
        free(file->bytes);
    }
    file->bytes = copy_bytes(bytes, length);
    file->length = length;
    return NULL;
}

/* NULL when the disk has no game directory; else a path the caller frees. */
static char *game_path(const disk_t *disk, const char *name) {
    if (disk->game_directory == NULL) {
        return NULL;
    }
    size_t size = strlen(disk->game_directory) + 1 + strlen(name) + 1;
    char *path = allocate(size);
    snprintf(path, size, "%s/%s", disk->game_directory, name);
    return path;
}

static disk_kind_t game_info(const disk_t *disk, const char *name, size_t *size) {
    char *path = game_path(disk, name);
    if (path == NULL) {
        return DISK_MISSING;
    }
    struct stat status;
    int failed = stat(path, &status);
    free(path);
    if (failed != 0) {
        return DISK_MISSING;
    }
    if (S_ISDIR(status.st_mode)) {
        return DISK_DIRECTORY;
    }
    if (!S_ISREG(status.st_mode)) {
        return DISK_MISSING;
    }
    *size = (size_t)status.st_size;
    return DISK_FILE;
}

/* A game file that vanishes or cannot be read whole reads as missing. */
static unsigned char *game_read(const disk_t *disk, const char *name, size_t *length) {
    size_t size = 0;
    if (game_info(disk, name, &size) != DISK_FILE) {
        return NULL;
    }
    char *path = game_path(disk, name);
    FILE *file = fopen(path, "rb");
    free(path);
    if (file == NULL) {
        return NULL;
    }
    unsigned char *bytes = allocate(size);
    size_t got = fread(bytes, 1, size, file);
    bool whole = got == size && fgetc(file) == EOF && !ferror(file);
    fclose(file);
    if (!whole) {
        free(bytes);
        return NULL;
    }
    *length = size;
    return bytes;
}

unsigned char *disk_read(const disk_t *disk, const char *name, size_t *length) {
    assert(disk_check_name(name) == NULL && "fake disk: check the name before reading it");
    *length = 0;
    const memory_file_t *file = find_memory_file(disk, name);
    if (file == NULL) {
        return game_read(disk, name, length);
    }
    *length = file->length;
    return copy_bytes(file->bytes, file->length);
}

disk_kind_t disk_info(const disk_t *disk, const char *name, size_t *size) {
    assert(disk_check_name(name) == NULL && "fake disk: check the name before asking about it");
    const memory_file_t *file = find_memory_file(disk, name);
    if (file == NULL) {
        return game_info(disk, name, size);
    }
    *size = file->length;
    return DISK_FILE;
}
