#ifndef ENGINE_GAME_ZONE_H
#define ENGINE_GAME_ZONE_H

#include <stddef.h>

#include "engine/allocator.h"

const allocator_t *game_zone(void);
void game_zone_flip(void);
int game_zone_pending(void);
void game_zone_release(void);
void game_zone_reset(void);
size_t game_zone_used(void);
size_t game_zone_max(void);

const allocator_t *game_zone_load_zone(void);
size_t load_zone_used(void);
size_t game_zone_load_zone_max(void);

#endif
