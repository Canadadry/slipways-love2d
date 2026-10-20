#include "game/attract.h"

#include "game/demo_data.h"

#include <assert.h>

static save_state_t g_demo_record;

void attract_init(attract_t *attract, int attract_after) {
    assert(attract_after >= 1 && "a threshold of 0 would start a demo with no cart_update60 since the return");
    attract->attract_after = attract_after;
    attract->idle_frames = 0;
    attract->phase = ATTRACT_PHASE_IDLE;
    attract->demo_frame = 0;
}

attract_action_t attract_frame(attract_t *attract, int on_title, int raw_buttons, int *played_buttons) {
    if (attract->phase == ATTRACT_PHASE_PLAYING) {
        if (raw_buttons != 0 || attract->demo_frame == demo_data_frame_count) {
            attract->phase = raw_buttons != 0 ? ATTRACT_PHASE_SWALLOWING : ATTRACT_PHASE_IDLE;
            attract->idle_frames = 0;
            *played_buttons = 0;
            return ATTRACT_ACTION_RETURN;
        }
        *played_buttons = demo_data_pad_buttons[attract->demo_frame];
        attract->demo_frame++;
        return ATTRACT_ACTION_NONE;
    }
    if (attract->phase == ATTRACT_PHASE_SWALLOWING) {
        if (raw_buttons != 0) {
            *played_buttons = 0;
            return ATTRACT_ACTION_NONE;
        }
        attract->phase = ATTRACT_PHASE_IDLE;
    }
    if (raw_buttons != 0 || !on_title) {
        attract->idle_frames = 0;
        *played_buttons = raw_buttons;
        return ATTRACT_ACTION_NONE;
    }
    if (attract->idle_frames >= attract->attract_after) {
        attract->phase = ATTRACT_PHASE_PLAYING;
        attract->demo_frame = 1;
        *played_buttons = demo_data_pad_buttons[0];
        return ATTRACT_ACTION_START;
    }
    attract->idle_frames++;
    *played_buttons = 0;
    return ATTRACT_ACTION_NONE;
}

attract_phase_t attract_phase(const attract_t *attract) {
    return attract->phase;
}

int attract_demo_frame(const attract_t *attract) {
    return attract->demo_frame;
}

const save_state_t *attract_demo_record(void) {
    int decoded = save_state_decode(demo_data_record, demo_data_record_count, &g_demo_record);
    assert(decoded && "the compiled demo record decodes: test_demo_data proves it");
    (void)decoded;
    return &g_demo_record;
}
