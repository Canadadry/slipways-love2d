#ifndef DEVTOOLS_STATE_H
#define DEVTOOLS_STATE_H

#include <stdio.h>

#include "game/save_state.h"

void state_write(FILE *out);

void state_write_save_summary(FILE *out, const save_state_t *state, const char *prefix);

void state_write_cheat_activations(FILE *out, int count);

void state_write_on_exit(const char *path, const int *frame);
void state_release(void);

#endif
