#include "engine/dispatch_context.h"

#include <stddef.h>

static p8_graphics_t *g_gfx = NULL;
static p8_vram_t *g_mem = NULL;
static const p8_input_t *g_input = NULL;

void dispatch_context_set_gfx(p8_graphics_t *gfx) {
    g_gfx = gfx;
}

p8_graphics_t *dispatch_context_gfx(void) {
    return g_gfx;
}

void dispatch_context_set_mem(p8_vram_t *mem) {
    g_mem = mem;
}

p8_vram_t *dispatch_context_mem(void) {
    return g_mem;
}

void dispatch_context_set_input(const p8_input_t *input) {
    g_input = input;
}

const p8_input_t *dispatch_context_input(void) {
    return g_input;
}
