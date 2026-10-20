#ifndef FAKE_LOVE_REPLAY_H
#define FAKE_LOVE_REPLAY_H

#include <stddef.h>

typedef struct replay replay_t;

/* On failure: NULL, and `error` names the file, the byte offset and the
 * problem. On success the caller releases the result with replay_free. */
replay_t *replay_read(const char *path, char *error, size_t error_size);
replay_t *replay_parse(const char *name, const char *text, size_t length, char *error, size_t error_size);

size_t replay_frame_count(const replay_t *replay);
void replay_free(replay_t *replay);

#endif
