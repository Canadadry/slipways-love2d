#ifndef GAME_MOUSE_BUTTON_H
#define GAME_MOUSE_BUTTON_H

typedef struct {
    int has_held;
    int held;
    int pressed;
    int released;
} mouse_button_t;

void mouse_button_update(mouse_button_t *button, int down);

#endif
