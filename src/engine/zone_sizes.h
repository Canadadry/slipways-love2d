#ifndef ENGINE_ZONE_SIZES_H
#define ENGINE_ZONE_SIZES_H

#include <stdint.h>

#include "engine/zone.h"

#define ENTITY_TABLE_CAP 512
/* One block from the game zone: the entity pointers, the generations, the
 * free-index stack. */
#define ENTITY_TABLE_BYTES \
    ZONE_BYTES(ENTITY_TABLE_CAP * (sizeof(void *) + 2 * sizeof(uint16_t)))

#define FRAME_ZONE_BYTES 20256u
#define UI_ZONE_BYTES 208272u
#define PARTICLES_ZONE_BYTES 73067u
#define GENERATION_ZONE_BYTES (462520u + ENTITY_TABLE_BYTES)
#define LOAD_ZONE_BYTES 24984u
#define GAME_ZONE_BYTES (GENERATION_ZONE_BYTES + LOAD_ZONE_BYTES)

#define HEADER_ID_BYTES (3720u + 32u + 304u)

#define ZONE_BUDGET_BYTES (768u * 1024u + ENTITY_TABLE_BYTES + HEADER_ID_BYTES)

#define ZONE_BUDGET_USED_BYTES \
    (FRAME_ZONE_BYTES + UI_ZONE_BYTES + PARTICLES_ZONE_BYTES + GAME_ZONE_BYTES)

typedef char zone_budget_exceeded_t[
    (ZONE_BUDGET_USED_BYTES <= ZONE_BUDGET_BYTES) ? 1 : -1];

typedef char load_zone_outside_game_zone_t[
    (LOAD_ZONE_BYTES < GAME_ZONE_BYTES &&
     GENERATION_ZONE_BYTES + LOAD_ZONE_BYTES == GAME_ZONE_BYTES) ? 1 : -1];

#endif
