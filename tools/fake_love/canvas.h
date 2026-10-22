#ifndef FAKE_LOVE_CANVAS_H
#define FAKE_LOVE_CANVAS_H

#define CANVAS_WIDTH 160
#define CANVAS_HEIGHT 120
#define CANVAS_MAX_COUNT 16
#define CANVAS_NONE (-1)

typedef struct canvas_store canvas_store_t;

typedef struct canvas_source {
    const unsigned char *pixels; /* row-major RGBA8 */
    int width;
    int height;
} canvas_source_t;

typedef struct canvas_area {
    int column;
    int row;
    int width;
    int height;
} canvas_area_t;

/* The caller releases the result with canvas_free_store. */
canvas_store_t *canvas_create_store(void);
void canvas_free_store(canvas_store_t *store);

/* Handles count from 0 in creation order; CANVAS_NONE once
 * CANVAS_MAX_COUNT canvases exist. A new canvas is all 0 bytes. */
int canvas_create(canvas_store_t *store);

/* Asserts a created handle or CANVAS_NONE, the screen. */
void canvas_select(canvas_store_t *store, int canvas);

/* A handle, or CANVAS_NONE for the screen. */
int canvas_get_target(const canvas_store_t *store);

/* With the screen as target, nothing: the screen is not modelled. */
void canvas_clear(canvas_store_t *store, const unsigned char rgba[4]);

/* The current colour of canvas_plot and canvas_fill_range until the next
 * call, whatever is selected or cleared meanwhile; opaque white in a new
 * store. Asserts rgba[3] is 0xFF. */
void canvas_set_color(canvas_store_t *store, const unsigned char rgba[4]);

void canvas_get_color(const canvas_store_t *store, unsigned char rgba[4]);

/* Writes the current colour over pixel (x, y) of the target; with the
 * screen as target, nothing. Asserts (x, y) is on the canvas. */
void canvas_plot(canvas_store_t *store, int x, int y);

/* As canvas_plot over every pixel of columns left ... right and rows
 * top ... bottom, inclusive. Asserts the range is on the canvas and not
 * empty. */
void canvas_fill_range(canvas_store_t *store, int left, int top, int right, int bottom);

/* Texel (area column + i, area row + j) of `source` goes to pixel
 * (x + i, y + j) of the target, skipped off the canvas. Alpha 00 leaves the
 * pixel; alpha FF writes canvas_tint_component of the current colour and the
 * texel, alpha FF. With the screen as target, nothing. Asserts the area is
 * inside the source and not empty, and every texel drawn is 00 or FF. */
void canvas_draw_texels(canvas_store_t *store, const canvas_source_t *source, const canvas_area_t *area, int x, int y);

/* Row-major RGBA8, owned by the store and valid until it is freed; NULL
 * for a handle that was never created. */
const unsigned char *canvas_read_pixels(const canvas_store_t *store, int canvas);

/* clear's rule in LÖVE 11.5 on Mesa: to float, clamped to [0, 1], times
 * 255 in float, rounded to nearest, ties to even. Asserts not NaN. */
unsigned char canvas_convert_clear_component(double component);

/* setColor's rule in LÖVE 11.5 on Mesa: to float, clamped to [0, 1], times
 * 255 in float, truncated (0.5 gives 127, clear gives 128). Asserts not NaN. */
unsigned char canvas_convert_draw_component(double component);

/* The draw tint in LÖVE 11.5 on Mesa: round(color x texel / 255), no tie
 * as 255 is odd. Not the floor (1 x 128 gives 1), nor llvmpipe's 8-bit
 * multiply, which only the blender uses (152 x 229 gives 137, not 136). */
unsigned char canvas_tint_component(unsigned char color, unsigned char texel);

#endif
