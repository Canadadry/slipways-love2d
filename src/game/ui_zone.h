#ifndef GAME_UI_ZONE_H
#define GAME_UI_ZONE_H

#include <stddef.h>

#include "engine/allocator.h"

typedef enum {
    SLOT_KIND_BUTTON,
    SLOT_KIND_MODAL_BACKDROP,
    SLOT_KIND_MESSAGE_BOX,
    SLOT_KIND_TOAST,
    SLOT_KIND_ROW,
    SLOT_KIND_EPHEMERALS,
    SLOT_KIND_COUNT
} slot_kind_t;

#define UI_EPHEMERALS_RESERVE 8

const allocator_t *ui_zone(slot_kind_t slot_kind);

const allocator_t *ui_zone_slot(slot_kind_t slot_kind);

int ui_zone_live(slot_kind_t slot_kind);

int ui_zone_max_live(slot_kind_t slot_kind);
size_t ui_zone_max_bytes(slot_kind_t slot_kind);
size_t ui_zone_used(void);
size_t ui_zone_max_used(void);

void ui_zone_record_ephemerals(int count);
int ui_zone_max_ephemerals(void);

int ui_zone_count(slot_kind_t slot_kind);
size_t ui_zone_slot_size(slot_kind_t slot_kind);
size_t ui_zone_reserved(void);

#endif
