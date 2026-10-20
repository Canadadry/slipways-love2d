#ifndef GAME_SAVE_STORAGE_H
#define GAME_SAVE_STORAGE_H

#include <stdint.h>

/* One opaque record of at most SAVE_STATE_MAX_BYTES bytes, never decoded
 * here. Each binary links exactly one backend that defines these three. */

/* Asserts 1 <= count <= SAVE_STATE_MAX_BYTES. Returns 1 when stored, 0 when
 * the backend failed, leaving the previous record unchanged. */
int save_storage_write(const uint8_t *bytes, int count);

/* Asserts cap >= SAVE_STATE_MAX_BYTES. Returns the record's byte count, 0
 * when there is none or it cannot be read; on 0, bytes is unspecified. */
int save_storage_read(uint8_t *bytes, int cap);

/* Returns 1 when no record remains afterwards, 0 when the backend failed. */
int save_storage_erase(void);

#endif
