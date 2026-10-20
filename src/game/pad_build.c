#include "game/pad_build.h"

#include "engine/entity_id.h"
#include "engine/entity_registry.h"
#include "game/pad_panel.h"
#include "game/structure_icon.h"

#include <limits.h>

void pad_build_init(pad_build_t *b) {
    *b = (pad_build_t){.open = 0, .icon = ENTITY_ID_NONE};
}

void pad_build_open(pad_build_t *b) {
    b->open = 1;
}

void pad_build_close(pad_build_t *b) {
    b->open = 0;
}

int pad_build_is_open(const pad_build_t *b) {
    return b->open;
}

entity_t *pad_build_icon(const pad_build_t *b) {
    entity_t *icon = entity_get(b->icon);
    return structure_icon_is_revealed(icon) ? icon : NULL;
}

static entity_t *icon_beside(int x, int step) {
    entity_t *nearest = NULL;
    int nearest_x = 0;
    const entity_list_t *bucket = entities_by_message(ENTITY_MESSAGE_HIT_TEST);
    for (int i = 0; i < bucket->count; i++) {
        entity_t *e = bucket->items[i];
        int at_x, at_y;
        if (!structure_icon_is_revealed(e)) {
            continue;
        }
        pad_panel_focus_point(e, &at_x, &at_y);
        if ((step < 0 ? at_x < x : at_x > x) && (nearest == NULL || (step < 0 ? at_x > nearest_x : at_x < nearest_x))) {
            nearest = e;
            nearest_x = at_x;
        }
    }
    return nearest;
}

entity_t *pad_build_focus(pad_build_t *b) {
    entity_t *icon = pad_build_icon(b);
    if (icon == NULL) {
        icon = icon_beside(INT_MAX, -1);
        b->icon = entity_id_link(icon);
    }
    return icon;
}

void pad_build_pick(pad_build_t *b, int step) {
    entity_t *icon = pad_build_icon(b);
    if (icon == NULL) {
        return;
    }
    int x, y;
    pad_panel_focus_point(icon, &x, &y);
    entity_t *next = icon_beside(x, step);
    if (next != NULL) {
        b->icon = entity_id_link(next);
    }
}
