#ifndef GAME_MENU_H
#define GAME_MENU_H

#include "engine/entity_class.h"
#include "game/ui_button.h"

void menu_show_tech_tree_screen(void);

button_t *galaxy_menu_button(int i);

button_fn_t galaxy_menu_on_click(int i);

/* The live HUD button whose on_click is galaxy_menu_on_click(i); NULL when there is none. */
button_t *galaxy_menu_find_button(int i);

const entity_t *pause_menu_open_box(void);
const entity_class_t *pause_menu_open_box_class(void);

/* 1 while the ui's open box is the pause menu. */
int pause_menu_shown(void);

void menu_init(void);
void menu_shutdown(void);

#endif
