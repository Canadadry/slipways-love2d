#include "game/pad_select.h"

#include "game/pad_target.h"
#include "game/planet.h"
#include "game/research.h"

#include <assert.h>
#include <limits.h>

void pad_select_init(pad_select_t *s) {
    *s = (pad_select_t){0};
}

pad_select_event_t pad_select_step(pad_select_t *s, int down, int allowed) {
    int chain_open = s->chain_frames > 0;
    if (chain_open) {
        s->chain_frames--;
    }
    if (!allowed) {
        s->held_frames = 0;
        s->fired = down;
        return PAD_SELECT_NONE;
    }
    if (!down) {
        int tap = s->held_frames > 0 && !s->fired && s->held_frames <= PAD_SELECT_TAP_FRAMES;
        s->held_frames = 0;
        s->fired = 0;
        return tap ? PAD_SELECT_TAP : PAD_SELECT_NONE;
    }
    if (s->fired && s->held_frames == 0) {
        return PAD_SELECT_NONE;
    }
    if (s->held_frames < INT_MAX) {
        s->held_frames++;
    }
    if (!s->fired && (s->held_frames == PAD_SELECT_HOLD_FRAMES || (s->held_frames == 1 && chain_open))) {
        s->fired = 1;
        return PAD_SELECT_YEAR;
    }
    return PAD_SELECT_NONE;
}

void pad_select_year_ended(pad_select_t *s) {
    s->chain_frames = PAD_SELECT_CHAIN_FRAMES;
}

int pad_select_year_just_ended(const pad_select_t *s) {
    return s->chain_frames == PAD_SELECT_CHAIN_FRAMES;
}

static int need_is_unmet(const need_t *need) {
    return need == NULL || !need->has_satisfied || need->satisfied == 0;
}

int pad_select_needs_attention(const entity_t *e) {
    if (!pad_target_is_candidate(e) || e->state != ENTITY_STATE_SETTLED) {
        return 0;
    }
    const planet_t *planet = (const planet_t *)e;
    if (!(planet->has & PLANET_HAS_NEEDS)) {
        return 0;
    }
    for (int i = 0; i < planet->needs.count; i++) {
        if (need_is_unmet(planet->needs.items[i])) {
            return 1;
        }
    }
    return 0;
}

entity_t *pad_select_next(const entity_list_t *candidates, const entity_t *current) {
    assert(candidates != NULL && "pad_select_next: the candidate list exists, even empty");
    int start = -1;
    for (int i = 0; i < candidates->count && current != NULL; i++) {
        if (candidates->items[i] == current) {
            start = i;
            break;
        }
    }
    for (int step = 1; step <= candidates->count; step++) {
        entity_t *e = candidates->items[(start + step) % candidates->count];
        if (pad_select_needs_attention(e)) {
            return e;
        }
    }
    return NULL;
}
