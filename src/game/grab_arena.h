#ifndef GAME_GRAB_ARENA_H
#define GAME_GRAB_ARENA_H

#include <stddef.h>

#include "engine/allocator.h"
#include "engine/entity_class.h"

const allocator_t *grab_arena_construction_zone(void);

void grab_arena_open(size_t bytes);
void grab_arena_close(void);
#define GRAB_ARENAS_FREE_MAX 8
void grab_arena_give_back_if_placing(const allocator_t *zone, state_t state);
void grab_arena_give_back(const allocator_t *zone);
void grab_arena_forget_free(void);

#endif
