#include "engine/game_zone.h"

#include "engine/zone_sizes.h"

FLIP_ZONE_STORAGE(game_zone_cells, GENERATION_ZONE_BYTES);
BUMP_ZONE_STORAGE(load_zone_cells, LOAD_ZONE_BYTES);

static flip_zone_t game_zone_storage;
/* Static: an entity_list_t or pointer_list_t keeps a pointer to its zone. */
static flip_zone_allocator_t game_zone_handle;
static const allocator_t *game_zone_allocator;
static bump_zone_t load_zone_storage;
static allocator_t load_zone_handle;
static size_t load_zone_max_used;

/* Lazy: flip_zone_init and bump_zone_init poison the storage under ASan,
 * which a static initialiser would not do. */
static void game_zone_init_once(void) {
    if (game_zone_allocator != NULL) return;
    flip_zone_init(&game_zone_storage, "game", game_zone_cells, GENERATION_ZONE_BYTES);
    game_zone_allocator = allocator_from_flip_zone(&game_zone_handle, &game_zone_storage);
    bump_zone_init(&load_zone_storage, "load", load_zone_cells, LOAD_ZONE_BYTES);
    load_zone_handle = allocator_from_bump_zone(&load_zone_storage);
}

static void load_zone_record_max(void) {
    if (load_zone_storage.used > load_zone_max_used) load_zone_max_used = load_zone_storage.used;
}

const allocator_t *game_zone(void) {
    game_zone_init_once();
    return game_zone_allocator;
}

void game_zone_flip(void) {
    game_zone_init_once();
    flip_zone_flip(&game_zone_storage);
}

int game_zone_pending(void) {
    game_zone_init_once();
    return flip_zone_pending(&game_zone_storage);
}

void game_zone_release(void) {
    game_zone_init_once();
    flip_zone_release(&game_zone_storage);
}

void game_zone_reset(void) {
    game_zone_init_once();
    flip_zone_reset(&game_zone_storage);
    load_zone_record_max();
    bump_zone_reset(&load_zone_storage);
}

size_t game_zone_used(void) {
    game_zone_init_once();
    return flip_zone_used(&game_zone_storage);
}

size_t game_zone_max(void) {
    game_zone_init_once();
    return flip_zone_max(&game_zone_storage);
}

const allocator_t *game_zone_load_zone(void) {
    game_zone_init_once();
    return &load_zone_handle;
}

size_t load_zone_used(void) {
    game_zone_init_once();
    return load_zone_storage.used;
}

size_t game_zone_load_zone_max(void) {
    game_zone_init_once();
    load_zone_record_max();
    return load_zone_max_used;
}
