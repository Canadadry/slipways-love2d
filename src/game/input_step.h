#ifndef GAME_INPUT_STEP_H
#define GAME_INPUT_STEP_H

#include "game/attract.h"
#include "game/cheat.h"
#include "game/pad_nav.h"
#include "game/save_state.h"
#include "p8/input.h"

typedef struct {
    cheat_t cheat;
    pad_nav_t pad_nav;
    p8_input_frame_t played_frame;
    p8_input_t played;
    bindings_t session_bindings;
    attract_t attract;
} input_step_t;

typedef enum { INPUT_STEP_BINDINGS_SESSION, INPUT_STEP_BINDINGS_DEFAULT } input_step_bindings_t;

void input_step_init(input_step_t *step, const bindings_t *bindings);

void input_step_set_attract_after(input_step_t *step, int frames);

void input_step_switch_bindings(input_step_t *step, input_step_bindings_t which);

void input_step_start_record(input_step_t *step, const save_state_t *state);

void input_step_return_to_title(input_step_t *step);

int input_step_run(input_step_t *step, const p8_input_frame_t *raw);

const p8_input_frame_t *input_step_played_frame(const input_step_t *step);

attract_phase_t input_step_attract_phase(const input_step_t *step);

int input_step_attract_demo_frame(const input_step_t *step);

#endif
