#ifndef GAME_ATTRACT_H
#define GAME_ATTRACT_H

#include "game/save_state.h"

#define ATTRACT_AFTER_DEFAULT 1200

typedef enum { ATTRACT_PHASE_IDLE, ATTRACT_PHASE_PLAYING, ATTRACT_PHASE_SWALLOWING } attract_phase_t;

typedef enum { ATTRACT_ACTION_NONE, ATTRACT_ACTION_START, ATTRACT_ACTION_RETURN } attract_action_t;

typedef struct {
    int attract_after;
    int idle_frames;
    attract_phase_t phase;
    int demo_frame;
} attract_t;

void attract_init(attract_t *attract, int attract_after);

attract_action_t attract_frame(attract_t *attract, int on_title, int raw_buttons, int *played_buttons);

attract_phase_t attract_phase(const attract_t *attract);

int attract_demo_frame(const attract_t *attract);

const save_state_t *attract_demo_record(void);

#endif
