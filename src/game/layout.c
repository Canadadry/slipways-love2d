#include "game/layout.h"

#include <assert.h>
#include <stddef.h>
#include <string.h>

#include "engine/dispatch_context.h"
#include "game/hud.h"
#include "game/text_image.h"

static int g_layout_up = 0;
static uint32_t g_row_serial;
static const entity_class_t layout_row_class_def;

layout_row_scalar_t layout_row_scalar_nothing(void) {
    layout_row_scalar_t scalar = {.kind = LAYOUT_ROW_SCALAR_NOTHING};
    return scalar;
}

layout_row_scalar_t layout_row_scalar_fixed(p8_fixed_t x) {
    layout_row_scalar_t scalar = {.kind = LAYOUT_ROW_SCALAR_FIXED, .value = x};
    return scalar;
}

layout_row_cell_t layout_row_cell_nothing(void) {
    layout_row_cell_t cell = {.kind = LAYOUT_ROW_CELL_NOTHING};
    return cell;
}

layout_row_cell_t layout_row_cell_fixed(p8_fixed_t x) {
    layout_row_cell_t cell = {.kind = LAYOUT_ROW_CELL_FIXED, .as.fixed = x};
    return cell;
}

layout_row_cell_t layout_row_cell_string(const char *s) {
    layout_row_cell_t cell = {.kind = LAYOUT_ROW_CELL_STRING, .as.text = s};

    assert(s != NULL && "layout_row_cell_string: no producer writes a NULL string; its kind would have no reader in row_measure_item");
    return cell;
}

layout_row_cell_t layout_row_cell_tuple(layout_row_tuple_t tuple) {
    layout_row_cell_t cell = {.kind = LAYOUT_ROW_CELL_TUPLE, .as.tuple = tuple};

    assert(tuple.count >= 0 && tuple.count <= LAYOUT_ROW_TUPLE_CAP && "layout_row_cell_tuple: row_measure_item reads four scalars at most");
    return cell;
}

static layout_row_scalar_t tuple_scalar_of_item(const data_item_t *item) {
    assert(item->row == NULL && item->text == NULL &&
           "a tuple holds numbers only; a text one needs a pointer in the cell -- stop, measure first");
    if (item->is_false || item->is_nil) {
        return layout_row_scalar_nothing();
    }
    return layout_row_scalar_fixed(item->is_fixed ? item->fixed : p8_fixed_from_int(item->number));
}

layout_row_cell_t layout_row_cell_row(const data_row_t *row) {
    layout_row_tuple_t tuple = {0};
    int i;

    if (row == NULL) {
        return layout_row_cell_nothing();
    }
    if (row->has_resource) {
        tuple.count = 2;
        tuple.scalars[0] = layout_row_scalar_fixed(row->resource);
        tuple.scalars[1] = layout_row_scalar_fixed(p8_fixed_from_int(5));
        return layout_row_cell_tuple(tuple);
    }
    assert(row->count <= LAYOUT_ROW_TUPLE_CAP && "layout_row_cell_row: a tuple has four scalars at most; row_measure_item never read a fifth -- stop, do not widen");
    tuple.count = row->count;
    for (i = 0; i < row->count; i++) {
        tuple.scalars[i] = tuple_scalar_of_item(&row->items[i]);
    }
    return layout_row_cell_tuple(tuple);
}

layout_row_cell_t layout_row_cell_item(const data_item_t *item) {
    if (item->row != NULL) {
        return layout_row_cell_row(item->row);
    }
    if (item->text != NULL) {
        return layout_row_cell_string(item->text);
    }
    if (item->is_false || item->is_nil) {
        return layout_row_cell_nothing();
    }
    if (item->is_fixed) {
        return layout_row_cell_fixed(item->fixed);
    }
    return layout_row_cell_fixed(p8_fixed_from_int(item->number));
}

static layout_row_cell_t row_scalar_item(const data_item_t *item) {
    assert(item->row == NULL && "a tuple or a line holds scalar items only; a row inside one has no reader here");
    return layout_row_cell_item(item);
}

static const char *row_copy_text(const allocator_t *zone, const char *s) {
    size_t n = strlen(s) + 1;
    char *copy = (char *)allocator_alloc(zone, n);

    memcpy(copy, s, n);
    return copy;
}

static void row_measure_item(const layout_row_t *self, layout_row_cell_t cell, layout_row_item_t *out_item,
                         p8_fixed_t *out_w, p8_fixed_t *out_spacing) {
    p8_fixed_t width = p8_fixed_from_int(6);
    p8_fixed_t palette_block = p8_fixed_from_int(0);
    int have_spacing = 0;
    p8_fixed_t spacing = p8_fixed_from_int(0);

    /* Zones never clear memory: without this, the fields a kind leaves unset hold stale bytes. */
    memset(out_item, 0, sizeof *out_item);
    if (cell.kind == LAYOUT_ROW_CELL_TUPLE) {
        layout_row_tuple_t t = cell.as.tuple;
        layout_row_scalar_t f[LAYOUT_ROW_TUPLE_CAP];
        int i;

        for (i = 0; i < LAYOUT_ROW_TUPLE_CAP; i++) {
            f[i] = i < t.count ? t.scalars[i] : layout_row_scalar_nothing();
        }
        cell = f[0].kind == LAYOUT_ROW_SCALAR_FIXED ? layout_row_cell_fixed(f[0].value) : layout_row_cell_nothing();
        if (f[1].kind == LAYOUT_ROW_SCALAR_FIXED) {
            width = f[1].value;
        }
        if (f[2].kind == LAYOUT_ROW_SCALAR_FIXED) {
            palette_block = f[2].value;
        }
        if (f[3].kind == LAYOUT_ROW_SCALAR_FIXED) {
            have_spacing = 1;
            spacing = f[3].value;
        }
    }

    if (cell.kind == LAYOUT_ROW_CELL_STRING) {
        p8_fixed_t c = p8_fixed_from_int(6);

        if (self != NULL && self->has_color) {
            c = self->color;
        }
        out_item->kind = LAYOUT_ROW_ITEM_TEXT;
        out_item->text = cell.as.text;
        out_item->color = c;
        *out_w = p8_fixed_from_int((int)strlen(cell.as.text) * 4 + 3);
        *out_spacing = p8_fixed_from_int(1);
        return;
    }

    if (cell.kind != LAYOUT_ROW_CELL_NOTHING) {
        out_item->kind = LAYOUT_ROW_ITEM_SPRITE;
        out_item->sprite_id = (cell.kind == LAYOUT_ROW_CELL_FIXED) ? cell.as.fixed : p8_fixed_from_int(0);
        out_item->sprite_palette_block = palette_block;
    } else {
        out_item->kind = LAYOUT_ROW_ITEM_NONE;
    }
    *out_w = width;
    *out_spacing = have_spacing ? spacing : p8_fixed_from_int(2);
}

static void layout_row_construct(layout_row_t *self, const layout_row_cell_t *cells, int n) {
    p8_fixed_t w = p8_fixed_from_int(0);
    p8_fixed_t fs = p8_fixed_from_int(0);
    int i;

    self->placed_items = (layout_row_item_t *)allocator_alloc(self->zone, (size_t)(n > 0 ? n : 1) * sizeof *self->placed_items);
    for (i = 0; i < n; i++) {
        p8_fixed_t w_i = p8_fixed_from_int(0);
        p8_fixed_t spacing_i = p8_fixed_from_int(0);
        p8_fixed_t x = w;

        row_measure_item(self, cells[i], &self->placed_items[i], &w_i, &spacing_i);
        if (self->placed_items[i].kind == LAYOUT_ROW_ITEM_TEXT) {
            self->placed_items[i].text = row_copy_text(self->zone, self->placed_items[i].text);
        }
        self->placed_items[i].offset = vec_make(x, p8_fixed_from_int(0));
        w = p8_fixed_add(w, p8_fixed_add(w_i, spacing_i));
        fs = spacing_i;
    }
    self->placed_items_count = n;
    self->width = p8_fixed_sub(w, fs);
}

static void row_draw_at_item_draw(const p8_graphics_t *gfx, p8_vram_t *mem, const layout_row_item_t *item,
                                   vec_t p) {
    vec_t pos = vec_add(p, item->offset);

    switch (item->kind) {
    case LAYOUT_ROW_ITEM_TEXT:
        hud_print_outlined_default(gfx, mem, item->text, p8_fixed_add(pos.x, p8_fixed_from_int(2)),
                          p8_fixed_add(pos.y, p8_fixed_from_int(1)),
                          p8_fixed_flr_int(item->color));
        break;
    case LAYOUT_ROW_ITEM_SPRITE:
        hud_select_palette_block(mem, item->sprite_palette_block);
        p8_spr_default(gfx, mem, p8_fixed_flr_int(item->sprite_id),
                           p8_fixed_flr_int(pos.x),
                           p8_fixed_flr_int(pos.y));
        hud_select_palette_block_default(mem);
        break;
    case LAYOUT_ROW_ITEM_NONE:
    default:
        break;
    }
}

void layout_row_draw_at(const p8_graphics_t *gfx, p8_vram_t *mem, const layout_row_t *self, vec_t p, p8_fixed_t a) {
    p8_fixed_t w;
    int i;

    if (self == NULL) {
        return;
    }
    assert(self->header.class == &layout_row_class_def &&
           "layout_row_draw_at: every row reaching it (a planet's or a button's layout_row, a button's side_tooltip, "
           "a tooltip's rows, a message box's layout-row article) is a layout row; anything else would be read through the wrong layout");
    w = self->width;
    p = vec_sub(p, vec_make(p8_fixed_mul(a, w), p8_fixed_from_int(0)));

    if (self->placed_items == NULL) {
        return;
    }
    for (i = 0; i < self->placed_items_count; i++) {
        row_draw_at_item_draw(gfx, mem, &self->placed_items[i], p);
    }
}

void layout_row_draw_at_text_image(p8_graphics_t *gfx, p8_vram_t *mem, const layout_row_t *self, vec_t p, p8_fixed_t a) {
    if (self == NULL) {
        return;
    }
    assert(self->header.class == &layout_row_class_def && "layout_row_draw_at_text_image: not a layout row");
    p = vec_sub(p, vec_make(p8_fixed_mul(a, self->width), p8_fixed_from_int(0)));
    if (self->placed_items == NULL) {
        return;
    }
    for (int i = 0; i < self->placed_items_count; i++) {
        const layout_row_item_t *item = &self->placed_items[i];
        if (item->kind != LAYOUT_ROW_ITEM_TEXT) {
            row_draw_at_item_draw(gfx, mem, item, p);
            continue;
        }
        vec_t pos = vec_add(p, item->offset);
        text_image_print_outlined(gfx, mem, item->text, p8_fixed_flr_int(p8_fixed_add(pos.x, p8_fixed_from_int(2))),
                                  p8_fixed_flr_int(p8_fixed_add(pos.y, p8_fixed_from_int(1))),
                                  p8_fixed_flr_int(item->color) & 0x0F);
    }
}

void layout_row_draw_at_default(const p8_graphics_t *gfx, p8_vram_t *mem, const layout_row_t *self, vec_t p) {
    layout_row_draw_at(gfx, mem, self, p, p8_fixed_from_int(0));
}

static layout_row_t *tooltip_line(const allocator_t *zone, const data_item_t *item) {
    layout_row_keys_t keys = {0};
    int i;

    if (item->text != NULL) {
        layout_row_keys_append(&keys, layout_row_cell_string(item->text));
        return layout_row_new(zone, &keys);
    }
    if (item->row == NULL) {
        return layout_row_new(zone, NULL);
    }
    assert(item->row->draw_function == DRAW_FUNCTION_NONE && "tooltip_line: a line's only named key is its color; a draw_function has no member -- stop, do not add one");
    for (i = 0; i < item->row->count; i++) {
        layout_row_keys_append(&keys, row_scalar_item(&item->row->items[i]));
    }
    keys.has_color = item->row->has_color;
    keys.color = p8_fixed_from_int(item->row->color);
    return layout_row_new(zone, &keys);
}

static void tooltip_construct(tooltip_t *self, const data_row_t *row) {
    int n = row != NULL ? row->count : 0;
    int i;

    /* Allocated even for no line: tooltip_draw_at() skips a NULL `rows` but draws an empty tooltip's frame. */
    self->rows = (layout_row_t **)allocator_alloc(self->zone, (size_t)(n > 0 ? n : 1) * sizeof *self->rows);
    for (i = 0; i < n; i++) {
        self->rows[i] = tooltip_line(self->zone, &row->items[i]);
    }
    self->rows_count = n;
}

void tooltip_draw_at(p8_graphics_t *gfx, p8_vram_t *mem, const tooltip_t *self, p8_fixed_t om,
                  p8_fixed_t a) {
    p8_fixed_t h;
    p8_fixed_t dn;
    int i;

    if (self == NULL || self->rows == NULL) {
        return;
    }

    h = p8_fixed_add(p8_fixed_mul(p8_fixed_from_int(self->rows_count), p8_fixed_from_int(8)),
                         p8_fixed_from_int(2));
    om = p8_fixed_sub(om, p8_fixed_mul(h, a));
    dn = p8_fixed_add(om, h);

    p8_fillp(gfx, 0x0F0F);
    hud_draw_bar(gfx, mem, om, dn, 0x10);
    p8_fillp_reset(gfx);

    for (i = 0; i < self->rows_count; i++) {
        vec_t pos = vec_make(p8_fixed_from_int(P8_SCREEN_WIDTH / 2), p8_fixed_add(om, p8_fixed_from_int(2)));
        layout_row_draw_at_text_image(gfx, mem, self->rows[i], pos, P8_FIXED_LIT(0x00008000, 0.5));
        om = p8_fixed_add(om, p8_fixed_from_int(8));
    }
}

static void layout_row_draw_at_method(entity_t *self, vec_t p, p8_fixed_t a) {
    layout_row_draw_at(dispatch_context_gfx(), dispatch_context_mem(), (const layout_row_t *)self, p, a);
}

static void tooltip_draw_at_method(entity_t *self, vec_t p, p8_fixed_t a) {
    (void)p;
    tooltip_draw_at(dispatch_context_gfx(), dispatch_context_mem(), (const tooltip_t *)self, p8_fixed_from_int(0), a);
}

static const entity_class_t layout_row_class_def = {
    .draw_at = layout_row_draw_at_method,
};

static void row_struct_init(layout_row_t *self, const allocator_t *zone) {
    assert(zone != NULL && "row_struct_init: a row without a zone would have nowhere to keep its placed items");
    entity_init_header(&self->header, &layout_row_class_def);
    self->color = 0;
    self->has_color = 0;
    self->background = 0;
    self->has_background = 0;
    self->background_is_bool = 0;
    self->score = 0;
    self->has_score = 0;
    self->placed_items = NULL;
    self->placed_items_count = 0;
    self->width = 0;
    self->serial = 0;
    self->zone = zone;
}

static void row_fill(layout_row_t *self, const layout_row_keys_t *keys) {
    if (++g_row_serial == 0) {
        g_row_serial = 1;
    }
    self->serial = g_row_serial;
    if (keys == NULL) {
        layout_row_construct(self, NULL, 0);
        return;
    }
    if (keys->has_color) {
        self->color = keys->color;
        self->has_color = 1;
    }
    if (keys->has_background) {
        self->background = keys->background;
        self->has_background = 1;
        self->background_is_bool = keys->background_is_bool;
    }
    if (keys->has_score) {
        self->score = keys->score;
        self->has_score = 1;
    }
    if (keys->has_pos) {
        entity_set_pos(&self->header, keys->pos);
    }
    layout_row_construct(self, keys->items, keys->count);
}

static layout_row_t *row_create(const allocator_t *zone, const layout_row_keys_t *keys) {
    layout_row_t *self;

    assert(zone != NULL && "row_create: a row without a zone would have nowhere to keep its placed items");
    self = (layout_row_t *)allocator_alloc(zone, sizeof *self);
    row_struct_init(self, zone);
    row_fill(self, keys);
    return self;
}

void layout_row_init(layout_row_t *self, const allocator_t *zone, const layout_row_keys_t *keys) {
    row_struct_init(self, zone);
    row_fill(self, keys);
}

void layout_row_keys_append(layout_row_keys_t *keys, layout_row_cell_t cell) {
    assert(keys->count >= 0 && keys->count < LAYOUT_ROW_KEYS_CAP &&
           "layout_row_keys_append: a row's cells are measured: stop, raise LAYOUT_ROW_KEYS_CAP after measuring");
    keys->items[keys->count++] = cell;
}

layout_row_keys_t layout_row_keys_of_row(const data_row_t *row) {
    layout_row_keys_t keys = {0};
    int i;

    assert(row->draw_function == DRAW_FUNCTION_NONE && !row->has_resource &&
           "layout_row_keys_of_row: a row passes positional items and a color; a draw_function or a resource has no member -- stop, do not add one");
    for (i = 0; i < row->count; i++) {
        layout_row_keys_append(&keys, layout_row_cell_item(&row->items[i]));
    }
    keys.has_color = row->has_color;
    keys.color = p8_fixed_from_int(row->color);
    return keys;
}

static const entity_class_t tooltip_class_def = {
    .draw_at = tooltip_draw_at_method,
};

static tooltip_t *tooltip_struct_alloc(const allocator_t *zone) {
    tooltip_t *self;

    assert(zone != NULL && "tooltip_struct_alloc: a tooltip without a zone would have nowhere to keep its lines");
    self = (tooltip_t *)allocator_alloc(zone, sizeof *self);

    entity_init_header(&self->header, &tooltip_class_def);
    self->zone = zone;
    self->rows = NULL;
    self->rows_count = 0;
    return self;
}

static tooltip_t *tooltip_create(const allocator_t *zone, const data_row_t *row) {
    tooltip_t *self;

    assert((row == NULL || (!row->has_color && row->draw_function == DRAW_FUNCTION_NONE)) &&
           "tooltip_create: every line is positional; a named key has no member -- stop, do not add one");
    self = tooltip_struct_alloc(zone);
    tooltip_construct(self, row);
    return self;
}

void layout_init(void) {
    layout_shutdown();
    g_layout_up = 1;
}

void layout_shutdown(void) {
    g_layout_up = 0;
}

int layout_row_ready(void) {
    return g_layout_up;
}

int tooltip_ready(void) {
    return g_layout_up;
}

const entity_class_t *layout_row_class(void) {
    return &layout_row_class_def;
}

const entity_class_t *tooltip_class(void) {
    return &tooltip_class_def;
}

layout_row_t *layout_row_new(const allocator_t *zone, const layout_row_keys_t *keys) {
    return row_create(zone, keys);
}

tooltip_t *tooltip_new(const allocator_t *zone, const data_row_t *row) {
    return tooltip_create(zone, row);
}
