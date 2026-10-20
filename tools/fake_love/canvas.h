#ifndef FAKE_LOVE_CANVAS_H
#define FAKE_LOVE_CANVAS_H

#define CANVAS_WIDTH 160
#define CANVAS_HEIGHT 120
#define CANVAS_MAX_COUNT 16
#define CANVAS_NONE (-1)

typedef struct canvas_store canvas_store_t;

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

/* Writes the current colour, opaque white (LÖVE's default) until a setter
 * exists, over pixel (x, y) of the target; with the screen as target,
 * nothing. Asserts (x, y) is on the canvas. */
void canvas_plot(canvas_store_t *store, int x, int y);

/* As canvas_plot over every pixel of columns left ... right and rows
 * top ... bottom, inclusive. Asserts the range is on the canvas and not
 * empty. */
void canvas_fill_range(canvas_store_t *store, int left, int top, int right, int bottom);

/* Row-major RGBA8, owned by the store and valid until it is freed; NULL
 * for a handle that was never created. */
const unsigned char *canvas_read_pixels(const canvas_store_t *store, int canvas);

/* As LÖVE 11.5 on Mesa: to float, clamped to [0, 1], times 255 in float,
 * rounded to nearest, ties to even. Asserts the component is not NaN. */
unsigned char canvas_convert_component(double component);

#endif
