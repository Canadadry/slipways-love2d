#ifndef ENGINE_EXIT_QUEUE_H
#define ENGINE_EXIT_QUEUE_H

#include "engine/entity_class.h"
#include "engine/allocator.h"

#define EXIT_QUEUE_CAP 315

void exit_queue_put(entity_t *e);

void exit_queue_put_arena(const allocator_t *arena);

int exit_queue_count(void);
int exit_queue_max(void);

void exit_queue_flush(void);

#endif
