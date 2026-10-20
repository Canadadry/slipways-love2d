#include "engine/exit_queue.h"

#include <assert.h>
#include <stdio.h>

#include "engine/entity_id.h"
#include "p8/fatal.h"

typedef struct {
    entity_t *entity;
    const allocator_t *arena;
} entry_t;

static entry_t g_queue[EXIT_QUEUE_CAP];
static int g_count;
static int g_max;

static void put(entry_t entry) {
    if (g_count == EXIT_QUEUE_CAP) {
        char message[64];
        snprintf(message, sizeof message, "exit queue full, limit %d", EXIT_QUEUE_CAP);
        p8_fatal(message);
    }
    g_queue[g_count++] = entry;
    if (g_count > g_max) g_max = g_count;
}

void exit_queue_put(entity_t *e) {
    entry_t entry = {e, NULL};
    assert(e != NULL);
    put(entry);
}

void exit_queue_put_arena(const allocator_t *arena) {
    entry_t entry = {NULL, arena};
    assert(arena != NULL);
    put(entry);
}

int exit_queue_count(void) {
    return g_count;
}

int exit_queue_max(void) {
    return g_max;
}

void exit_queue_flush(void) {
    for (int i = 0; i < g_count; i++) {
        entity_t *e = g_queue[i].entity;
        if (e == NULL) {
            arena_release(g_queue[i].arena);
            continue;
        }
        void (*release)(entity_t *self) = e->class->release;
        /* The slot goes before the memory: no slot may point at a returned block. */
        entity_id_release(e);
        if (release != NULL) release(e);
    }
    g_count = 0;
}
