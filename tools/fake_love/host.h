#ifndef FAKE_LOVE_HOST_H
#define FAKE_LOVE_HOST_H

#include <stdbool.h>
#include <stddef.h>

typedef struct host host_t;

/* NULL only when memory runs out. The caller releases it with host_close. */
host_t *host_new(void);
void host_close(host_t *host);

/* `jit.version` of the running LuaJIT. */
const char *host_runtime_version(host_t *host);

/* Each call below returns false on failure and leaves the reason in
 * host_error, valid until the next call. */
bool host_open(host_t *host, const char *game_dir);
bool host_load(host_t *host);
/* Calls the pad callbacks for the changes from the previous frame's `held`
 * (nothing held before frame 0), then love.update and love.draw. */
bool host_frame(host_t *host, size_t frame, unsigned held);
/* `*text` is owned by the host, valid until the next call. */
bool host_debug_state(host_t *host, const char **text, size_t *length);

const char *host_error(const host_t *host);

#endif
