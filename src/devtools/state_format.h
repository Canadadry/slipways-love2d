#ifndef DEVTOOLS_STATE_FORMAT_H
#define DEVTOOLS_STATE_FORMAT_H

#include "game/save_state.h"

/* Each call carries one whole line, its '\n' included, in a buffer the
   formatter reuses once the call returns. */
typedef void (*state_line_fn)(void *context, const char *line);

typedef struct {
    state_line_fn write_line;
    void *context;
} state_sink_t;

void state_format(const state_sink_t *sink);

/* Reads only *state, which came out of a save_state_decode that returned 1:
   its difficulty, clock year, resources credits and count nodes lines in
   state_format's format, each after prefix. */
void state_format_save_summary(const state_sink_t *sink, const save_state_t *state, const char *prefix);

void state_format_stopped(const state_sink_t *sink, int frame);

void state_format_cheat_activations(const state_sink_t *sink, int count);

#endif
