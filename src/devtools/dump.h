#ifndef DEVTOOLS_DUMP_H
#define DEVTOOLS_DUMP_H

#include "p8/vram.h"

#include <stdio.h>

int dump_screen(const p8_vram_t *mem, FILE *out);

int dump_pixels(const unsigned char *pixels, FILE *out);

#endif
