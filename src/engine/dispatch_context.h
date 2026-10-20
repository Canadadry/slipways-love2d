#ifndef ENGINE_DISPATCH_CONTEXT_H
#define ENGINE_DISPATCH_CONTEXT_H

#include "p8/gfx.h"
#include "p8/input.h"
#include "p8/vram.h"

void dispatch_context_set_gfx(p8_graphics_t *gfx);
p8_graphics_t *dispatch_context_gfx(void);

void dispatch_context_set_mem(p8_vram_t *mem);
p8_vram_t *dispatch_context_mem(void);

void dispatch_context_set_input(const p8_input_t *input);
const p8_input_t *dispatch_context_input(void);

#endif
