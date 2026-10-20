#include "game/pad_back.h"

#include "engine/entity_id.h"
#include "game/pad_target.h"

void pad_back_init(pad_back_t *back) {
    *back = (pad_back_t){.last_acted = ENTITY_ID_NONE};
}

void pad_back_record(pad_back_t *back, const entity_t *e) {
    if (e != NULL) {
        back->last_acted = entity_id_link(e);
    }
}

entity_t *pad_back_target(const pad_back_t *back, const entity_t *hovered) {
    entity_t *e = entity_get(back->last_acted);
    return pad_target_is_candidate(e) && e != hovered ? e : NULL;
}
