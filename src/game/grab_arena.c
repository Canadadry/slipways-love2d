#include "game/grab_arena.h"

#include <assert.h>
#include <stddef.h>

#include "engine/game_zone.h"

/* Grab arenas are recognised by this name's address, not its contents. */
static const char grab_arena_name[] = "grab";

static const allocator_t *g_grab_arena = NULL;
static const allocator_t *g_grab_arenas_free[GRAB_ARENAS_FREE_MAX];
static int g_grab_arenas_free_n = 0;

const allocator_t *grab_arena_construction_zone(void) {
    return g_grab_arena != NULL ? g_grab_arena : game_zone();
}

void grab_arena_open(size_t bytes) {
    size_t size = arena_size_for(bytes);
    int best = -1;

    assert(g_grab_arena == NULL && "a grab arena is already open");
    for (int i = 0; i < g_grab_arenas_free_n; i++) {
        size_t free_size = arena_size(g_grab_arenas_free[i]);
        if (free_size >= size && (best < 0 || free_size < arena_size(g_grab_arenas_free[best]))) {
            best = i;
        }
    }
    if (best >= 0) {
        g_grab_arena = g_grab_arenas_free[best];
        g_grab_arenas_free[best] = g_grab_arenas_free[--g_grab_arenas_free_n];
        arena_flush(g_grab_arena);
    } else {
        g_grab_arena = arena_take(game_zone(), grab_arena_name, size);
    }
}

void grab_arena_close(void) {
    arena_spill(g_grab_arena);
    g_grab_arena = NULL;
}

void grab_arena_give_back_if_placing(const allocator_t *zone, state_t state) {
    if (state != ENTITY_STATE_PLACING) {
        return;
    }
    grab_arena_give_back(zone);
}

void grab_arena_give_back(const allocator_t *zone) {
    if (zone == NULL || zone->bump == NULL || zone->bump->name != grab_arena_name) {
        return;
    }
    if (g_grab_arenas_free_n < GRAB_ARENAS_FREE_MAX) {
        g_grab_arenas_free[g_grab_arenas_free_n++] = zone;
        return;
    }
    int smallest = 0;
    for (int i = 1; i < g_grab_arenas_free_n; i++) {
        if (arena_size(g_grab_arenas_free[i]) < arena_size(g_grab_arenas_free[smallest])) {
            smallest = i;
        }
    }
    if (arena_size(g_grab_arenas_free[smallest]) < arena_size(zone)) {
        g_grab_arenas_free[smallest] = zone;
    }
}

void grab_arena_forget_free(void) {
    g_grab_arenas_free_n = 0;
}
