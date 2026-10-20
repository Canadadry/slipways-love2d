#include "game/mouse_button.h"

#include <assert.h>
#include <stddef.h>

void mouse_button_update(mouse_button_t *button, int down) {
    int was_held = 0;

    assert(button != NULL && "mouse_button_update dereferences the button");

    if (button->has_held) {
        was_held = button->held;
    }

    button->has_held = 1;
    button->held = down;
    button->pressed = down && !was_held;
    button->released = was_held && !down;
}
