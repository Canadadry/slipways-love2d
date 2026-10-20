#ifndef GAME_LAYOUT_H
#define GAME_LAYOUT_H

#include <stdint.h>

#include "p8/fixed.h"
#include "p8/gfx.h"
#include "game/map_tables.h"
#include "p8/vram.h"
#include "engine/entity_class.h"
#include "engine/vec.h"
#include "engine/allocator.h"

typedef enum {
    LAYOUT_ROW_ITEM_NONE,
    LAYOUT_ROW_ITEM_TEXT,
    LAYOUT_ROW_ITEM_SPRITE
} layout_row_item_kind_t;

typedef struct {
    layout_row_item_kind_t kind;
    const char *text;
    p8_fixed_t color;
    p8_fixed_t sprite_id;
    p8_fixed_t sprite_palette_block;
    vec_t offset;
} layout_row_item_t;

typedef enum {
    LAYOUT_ROW_CELL_NOTHING,
    LAYOUT_ROW_CELL_FIXED,
    LAYOUT_ROW_CELL_STRING,
    LAYOUT_ROW_CELL_TUPLE,
    LAYOUT_ROW_CELL_TRUE
} layout_row_cell_kind_t;

typedef enum {
    LAYOUT_ROW_SCALAR_NOTHING,
    LAYOUT_ROW_SCALAR_FIXED
} layout_row_scalar_kind_t;

typedef struct {
    layout_row_scalar_kind_t kind;
    p8_fixed_t value;
} layout_row_scalar_t;

#define LAYOUT_ROW_TUPLE_CAP 4

/* Drawn item, width, palette block, spacing: a scalar past `count` reads as nothing. */
typedef struct {
    int count;
    layout_row_scalar_t scalars[LAYOUT_ROW_TUPLE_CAP];
} layout_row_tuple_t;

typedef struct {
    layout_row_cell_kind_t kind;
    union {
        p8_fixed_t fixed;
        const char *text;
        layout_row_tuple_t tuple;
    } as;
} layout_row_cell_t;

typedef struct {
    entity_t header;
    const allocator_t *zone;
    p8_fixed_t color;
    int has_color;
    p8_fixed_t background;
    int has_background;
    int background_is_bool;
    p8_fixed_t score;
    int has_score;
    layout_row_item_t *placed_items;
    int placed_items_count;
    p8_fixed_t width;
    /* New and never 0 each time the row is filled: two rows with one serial hold the same items. */
    uint32_t serial;
} layout_row_t;

typedef struct {
    entity_t header;
    const allocator_t *zone;
    layout_row_t **rows;
    int rows_count;
} tooltip_t;

void layout_init(void);
void layout_shutdown(void);

int layout_row_ready(void);

int tooltip_ready(void);

const entity_class_t *layout_row_class(void);
const entity_class_t *tooltip_class(void);

#define LAYOUT_ROW_KEYS_CAP 16

typedef struct {
    layout_row_cell_t items[LAYOUT_ROW_KEYS_CAP];
    int count;
    int has_color;
    p8_fixed_t color;
    int has_background;
    int background_is_bool;
    p8_fixed_t background;
    int has_score;
    p8_fixed_t score;
    int has_pos;
    vec_t pos;
} layout_row_keys_t;

layout_row_t *layout_row_new(const allocator_t *zone, const layout_row_keys_t *keys);
/* Lays the row out into `self`; its placed items and their texts go in `zone`, which must outlive `self`. */
void layout_row_init(layout_row_t *self, const allocator_t *zone, const layout_row_keys_t *keys);

layout_row_scalar_t layout_row_scalar_nothing(void);
layout_row_scalar_t layout_row_scalar_fixed(p8_fixed_t x);

layout_row_cell_t layout_row_cell_nothing(void);
layout_row_cell_t layout_row_cell_fixed(p8_fixed_t x);
layout_row_cell_t layout_row_cell_string(const char *s);
layout_row_cell_t layout_row_cell_tuple(layout_row_tuple_t tuple);
/* Copies the row's scalars: the cell keeps nothing of `row`. NULL gives a nothing cell. */
layout_row_cell_t layout_row_cell_row(const data_row_t *row);
layout_row_cell_t layout_row_cell_item(const data_item_t *item);

void layout_row_keys_append(layout_row_keys_t *keys, layout_row_cell_t cell);

layout_row_keys_t layout_row_keys_of_row(const data_row_t *row);

tooltip_t *tooltip_new(const allocator_t *zone, const data_row_t *row);

void layout_row_draw_at(const p8_graphics_t *gfx, p8_vram_t *mem, const layout_row_t *self, vec_t p, p8_fixed_t a);
void layout_row_draw_at_default(const p8_graphics_t *gfx, p8_vram_t *mem, const layout_row_t *self, vec_t p);
/* layout_row_draw_at's contract (same pixels, same draw palette after), each text item through
 * text_image_print_outlined. gfx's camera moves during a build and is restored. */
void layout_row_draw_at_text_image(p8_graphics_t *gfx, p8_vram_t *mem, const layout_row_t *self, vec_t p, p8_fixed_t a);

void tooltip_draw_at(p8_graphics_t *gfx, p8_vram_t *mem, const tooltip_t *self, p8_fixed_t om, p8_fixed_t a);

#endif
