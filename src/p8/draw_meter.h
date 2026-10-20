#ifndef P8_DRAW_METER_H
#define P8_DRAW_METER_H

#include "p8/draw_backend.h"

typedef struct {
    unsigned long bytes;
    /* What image builds emit, in their own buffer; bytes and primitives leave them out. */
    unsigned long build_bytes;
    int primitives;
    int cluts;
    int patterns;
} p8_draw_meter_counts_t;

/* Installs the meter around the backend active at the call and zeroes every count. Returns the
 * wrapped backend: reinstalling it with p8_draw_backend_set removes the meter. */
const p8_draw_backend_t *p8_draw_meter_install(void);

void p8_draw_meter_frame_end(void);

/* Zero until the first p8_draw_meter_frame_end. */
p8_draw_meter_counts_t p8_draw_meter_last_frame(void);

/* Each field's own peak over the frames ended since the install. */
p8_draw_meter_counts_t p8_draw_meter_peak(void);

#endif
