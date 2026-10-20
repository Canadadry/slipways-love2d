#include "game/input_step.h"

#include "engine/dispatch_context.h"
#include "game/cart.h"

#include <assert.h>
#include <string.h>

void input_step_init(input_step_t *step, const bindings_t *bindings) {
    memset(step, 0, sizeof *step);
    cheat_init(&step->cheat);
    pad_nav_init(&step->pad_nav, bindings);
    step->session_bindings = *bindings;
    attract_init(&step->attract, ATTRACT_AFTER_DEFAULT);
}

void input_step_set_attract_after(input_step_t *step, int frames) {
    attract_init(&step->attract, frames);
}

void input_step_switch_bindings(input_step_t *step, input_step_bindings_t which) {
    assert((which == INPUT_STEP_BINDINGS_SESSION || which == INPUT_STEP_BINDINGS_DEFAULT)
           && "input_step_switch_bindings: which is the session or the default table");
    bindings_t session = step->session_bindings;
    attract_t attract = step->attract;
    input_step_init(step, which == INPUT_STEP_BINDINGS_DEFAULT ? &bindings_default : &session);
    step->session_bindings = session;
    step->attract = attract;
}

void input_step_start_record(input_step_t *step, const save_state_t *state) {
    assert(step != NULL && state != NULL);
    input_step_switch_bindings(step, INPUT_STEP_BINDINGS_DEFAULT);
    cart_resume_game(state);
}

void input_step_return_to_title(input_step_t *step) {
    assert(step != NULL);
    cart_return_to_title();
    input_step_switch_bindings(step, INPUT_STEP_BINDINGS_SESSION);
}

int input_step_run(input_step_t *step, const p8_input_frame_t *raw) {
    int buttons;
    attract_action_t action = attract_frame(&step->attract, cart_title_shown(), raw->pad_buttons, &buttons);
    if (action == ATTRACT_ACTION_START) {
        input_step_start_record(step, attract_demo_record());
        cart_cut_save();
    } else if (action == ATTRACT_ACTION_RETURN) {
        input_step_return_to_title(step);
    }
    memset(&step->played_frame, 0, sizeof step->played_frame);
    step->played_frame.pad_buttons = buttons;
    pad_nav_step(&step->pad_nav, buttons, &step->played_frame);
    /* Set every frame, not at init: the struct may have been copied since. */
    step->played.frames = &step->played_frame;
    step->played.frame_count = 1;
    step->played.current_frame = 0;
    int fired = cheat_step(&step->cheat, buttons);
    dispatch_context_set_input(&step->played);
    int playing = input_step_attract_phase(step) == ATTRACT_PHASE_PLAYING;
    cart_set_demo_frame(playing ? input_step_attract_demo_frame(step) - 1 : CART_NO_DEMO_FRAME);
    return fired;
}

const p8_input_frame_t *input_step_played_frame(const input_step_t *step) {
    return &step->played_frame;
}

attract_phase_t input_step_attract_phase(const input_step_t *step) {
    return attract_phase(&step->attract);
}

int input_step_attract_demo_frame(const input_step_t *step) {
    return attract_demo_frame(&step->attract);
}
