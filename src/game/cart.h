#ifndef GAME_CART_H
#define GAME_CART_H

#include "engine/entity_registry.h"
#include "game/save_state.h"
#include "game/ui_button.h"

void cart_load(void);

void cart_flush_exit_queue(void);

void cart_shutdown(void);

void cart_init(void);

void cart_return_to_title(void);

int cart_title_shown(void);

void cart_cut_save(void);

#define CART_NO_DEMO_FRAME (-1)

void cart_set_demo_frame(int frame);

void cart_init_method(button_t *self);

extern const entity_registry_reserve_t cart_game_registry_reserve;

void cart_start_game(button_t *chosen_option);

/* The title's difficulty buttons: copies the option's value, then the press frame and the next
   draw the loading screen and the frame after generates the galaxy. NULL starts with no difficulty. */
void cart_request_start_game(button_t *chosen_option);

int cart_loading(void);

void cart_resume_game(const save_state_t *state);

void cart_update60(void);

void cart_draw(void);

#endif
