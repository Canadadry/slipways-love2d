#ifndef GAME_TEXT_IMAGE_H
#define GAME_TEXT_IMAGE_H

#include "p8/gfx.h"
#include "p8/vram.h"

/* 1 when str fits a text slot: text_image_print_outlined then copies it, unless every slot was
 * used this frame. */
int text_image_cacheable(const char *str);

/* p8_print_outlined's contract (same pixels, same draw palette after), through a text slot keyed
 * by str and col when str is cacheable and a slot is free. gfx's camera moves during a build and
 * is restored. */
void text_image_print_outlined(p8_graphics_t *gfx, p8_vram_t *mem, const char *str, int x, int y, int col);

/* Starts a draw frame: a slot copied earlier in the frame is never rebuilt before the next call. */
void text_image_frame_begin(void);

#endif
