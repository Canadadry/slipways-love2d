#ifndef DEVTOOLS_DRAW_FILTER_H
#define DEVTOOLS_DRAW_FILTER_H

#include "p8/draw_backend.h"

/* The mask's bit order, shared with the capture list and the capture script. */
typedef enum {
    DRAW_FILTER_CLS,
    DRAW_FILTER_PSET,
    DRAW_FILTER_LINE,
    DRAW_FILTER_RECT,
    DRAW_FILTER_RECTFILL,
    DRAW_FILTER_CIRC,
    DRAW_FILTER_CIRCFILL,
    DRAW_FILTER_SPR,
    DRAW_FILTER_PRINT,
    DRAW_FILTER_PATTERN,
    DRAW_FILTER_FAMILY_COUNT
} draw_filter_family_t;

#define DRAW_FILTER_BIT(family) (1u << (family))
#define DRAW_FILTER_ALL ((1u << DRAW_FILTER_FAMILY_COUNT) - 1u)

/* Wraps the backend active at the call, mask DRAW_FILTER_ALL, counts zeroed. Returns the wrapped
 * backend: reinstalling it with p8_draw_backend_set removes the filter. */
const p8_draw_backend_t *draw_filter_install(void);

/* From the next call on. A patterned rectfill/circfill passes only with its shape bit and
 * DRAW_FILTER_PATTERN; an image copy only with every family drawn into the image; the mask must hold
 * DRAW_FILTER_CLS. */
void draw_filter_set_mask(unsigned mask);

/* Zeroes the counts; the mask stays. */
void draw_filter_frame_end(void);

/* Calls since the last frame end whose cost-model bytes are above 0, masked or not; a patterned
 * fill counts under its shape and under DRAW_FILTER_PATTERN, an image copy under each family of its
 * image, a call inside an image build under none. */
int draw_filter_count(draw_filter_family_t family);

#endif
