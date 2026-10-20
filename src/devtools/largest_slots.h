#ifndef DEVTOOLS_LARGEST_SLOTS_H
#define DEVTOOLS_LARGEST_SLOTS_H

#include "devtools/state_format.h"

/* Needs a loaded cart, the gfx, vram and input set in the dispatch context;
   starts a game of its own, which stays up when it returns. */
void largest_slots_run(void);

/* One line per proven-ceiling slot, `bytes_<slot> <peak> <size>`. */
void largest_slots_format(const state_sink_t *sink);

#endif
