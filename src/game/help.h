#ifndef GAME_HELP_H
#define GAME_HELP_H

#include "p8/gfx.h"
#include "p8/vram.h"
#include "game/probe.h"
#include "engine/entity_class.h"
#include "game/ui_button.h"
#include "engine/vec.h"

void help_init(void);
void help_shutdown(void);

int help_screen_ready(void);
const entity_class_t *help_screen_class(void);

probe_t *help_screen_new(void);

button_fn_t help_start_game_field_value(void);
void help_set_start_game(button_fn_t fn);

/* NULL (the value after help_init and help_shutdown) means the title has no resume row. */
button_fn_t help_resume_game_field_value(void);
void help_set_resume_game(button_fn_t fn);

void help_screen_construct(probe_t *self);

void help_screen_draw(p8_graphics_t *gfx, p8_vram_t *mem, probe_t *self, vec_t p);

void help_screen_draw_method(entity_t *self, vec_t screen_pos);

#endif
