#include "engine/entity.h"

#include <assert.h>
#include <stddef.h>
#include <string.h>

#include "engine/entity_id.h"
#include "engine/entity_registry.h"
#include "p8/fixed.h"

static int g_entity_up = 0;

void entity_init(void) {
    entity_shutdown();
    g_entity_up = 1;
}

void entity_shutdown(void) {
    g_entity_up = 0;
}

int entity_ready(void) {
    return g_entity_up;
}

void entity_set_state(entity_t *self, state_t state) {
    if (self == NULL) {
        return;
    }
    self->state = state;
    self->state_time = p8_fixed_from_int(0);
}

void entity_init_instance(entity_t *self, const entity_class_t *cls, state_t state, p8_fixed_t t) {
    entity_init_header(self, cls);
    self->state = state;
    self->state_time = t;
}

const char *entity_state_name(state_t state) {
    static const char *const names[STATE_COUNT] = {
        [ENTITY_STATE_NORMAL] = "normal",
        [ENTITY_STATE_SETTLED] = "settled",
        [ENTITY_STATE_VANISHING] = "vanishing",
        [ENTITY_STATE_PLACING] = "placing",
        [ENTITY_STATE_UNFOLDING] = "unfolding",
        [ENTITY_STATE_UNKNOWN] = "unknown",
        [ENTITY_STATE_KNOWN] = "known",
    };

    return names[state];
}

void entity_construct(entity_t *self) {
    entity_id_take(self);
    entity_register(self);
}
