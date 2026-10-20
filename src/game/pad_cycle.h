#ifndef GAME_PAD_CYCLE_H
#define GAME_PAD_CYCLE_H

typedef enum { PAD_CYCLE_CLOSED, PAD_CYCLE_TECH, PAD_CYCLE_SCORE } pad_cycle_panel_t;

typedef struct {
    pad_cycle_panel_t panel;
    int landing_due;
} pad_cycle_t;

void pad_cycle_init(pad_cycle_t *cycle);

pad_cycle_panel_t pad_cycle_panel(const pad_cycle_t *cycle);

int pad_cycle_landing_due(const pad_cycle_t *cycle);

void pad_cycle_set(pad_cycle_t *cycle, pad_cycle_panel_t panel);

void pad_cycle_landed(pad_cycle_t *cycle);

pad_cycle_panel_t pad_cycle_next(pad_cycle_panel_t panel);

int pad_cycle_hud_button(pad_cycle_panel_t panel);

#endif
