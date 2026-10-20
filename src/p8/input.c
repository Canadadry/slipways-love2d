#include "p8/input.h"

#include <assert.h>
#include <stddef.h>

void p8_input_init(p8_input_t *input) {
    input->frames = NULL;
    input->frame_count = 0;
    input->current_frame = 0;
}

void p8_input_step(p8_input_t *input) {
    assert(input->current_frame + 1 < input->frame_count);
    input->current_frame++;
}

p8_mouse_t p8_input_read_mouse(const p8_input_t *input) {
    p8_mouse_t mouse = {0, 0, 0};
    if (input->frame_count == 0 || input->current_frame >= input->frame_count) {
        return mouse;
    }
    const p8_input_frame_t *f = &input->frames[input->current_frame];
    mouse.x = f->mouse_x;
    mouse.y = f->mouse_y;
    mouse.buttons = f->mouse_buttons;
    return mouse;
}
