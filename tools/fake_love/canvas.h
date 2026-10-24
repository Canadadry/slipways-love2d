#ifndef FAKE_LOVE_CANVAS_H
#define FAKE_LOVE_CANVAS_H

#define CANVAS_WIDTH 160
#define CANVAS_HEIGHT 120
#define CANVAS_MAX_COUNT 16
#define CANVAS_NONE (-1)

typedef struct canvas_store canvas_store_t;

typedef struct canvas_source {
    const unsigned char *pixels;
    int width;
    int height;
} canvas_source_t;

typedef struct canvas_area {
    int column;
    int row;
    int width;
    int height;
} canvas_area_t;

canvas_store_t *canvas_create_store(void);
void canvas_free_store(canvas_store_t *store);

int canvas_create(canvas_store_t *store);

void canvas_select(canvas_store_t *store, int canvas);

int canvas_get_target(const canvas_store_t *store);

void canvas_clear(canvas_store_t *store, const unsigned char rgba[4]);

void canvas_set_color(canvas_store_t *store, const unsigned char rgba[4]);

void canvas_get_color(const canvas_store_t *store, unsigned char rgba[4]);

void canvas_plot(canvas_store_t *store, int x, int y);

void canvas_fill_range(canvas_store_t *store, int left, int top, int right, int bottom);

void canvas_draw_texels(canvas_store_t *store, const canvas_source_t *source, const canvas_area_t *area, int x, int y);

const unsigned char *canvas_read_pixels(const canvas_store_t *store, int canvas);

unsigned char canvas_convert_clear_component(double component);

unsigned char canvas_convert_draw_component(double component);

unsigned char canvas_tint_component(unsigned char color, unsigned char texel);

#endif
