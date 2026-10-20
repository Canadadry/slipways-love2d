#include "game/draw_functions.h"

#include <assert.h>

#include "game/hud.h"
#include "p8/str.h"
#include "engine/frame_zone.h"

static p8_graphics_t *g_draw_functions_gfx = NULL;
static p8_vram_t *g_draw_functions_mem = NULL;

static const data_item_t *draw_functions_arg(const data_row_t *e, int i) {
    const data_item_t *item;

    if (i > e->count) {
        return NULL;
    }
    item = &e->items[i - 1];
    assert(item->row == NULL && "a row drawn by draw_functions holds no nested row");
    return item;
}

static int is_number(const data_row_t *e, int i) {
    const data_item_t *item = draw_functions_arg(e, i);
    if (item == NULL || item->text != NULL) {
        return 0;
    }
    if (item->is_false || item->is_nil) {
        return 0;
    }
    return 1;
}

static p8_fixed_t number(const data_row_t *e, int i) {
    const data_item_t *item = draw_functions_arg(e, i);
    if (!is_number(e, i)) {
        return p8_fixed_from_int(0);
    }
    return item->is_fixed ? item->fixed : p8_fixed_from_int(item->number);
}

static void draw_bar_entry(const data_row_t *e) {
    p8_fixed_t y1 = number(e, 1);
    int col;
    col = p8_fixed_flr_int(number(e, 3));
    if (is_number(e, 2)) {
        hud_draw_bar(g_draw_functions_gfx, g_draw_functions_mem, y1, number(e, 2), col);
    } else {
        hud_draw_bar_y1(g_draw_functions_gfx, g_draw_functions_mem, y1, col);
    }
}

typedef void (*print_outlined_fn_t)(p8_graphics_t *gfx, p8_vram_t *mem, const char *t, p8_fixed_t x, p8_fixed_t y, int c,
                                    p8_fixed_t a);

static void print_outlined_direct(p8_graphics_t *gfx, p8_vram_t *mem, const char *t, p8_fixed_t x, p8_fixed_t y, int c,
                                  p8_fixed_t a) {
    hud_print_outlined(gfx, mem, t, x, y, c, a);
}

/* `a1` is often a number: substituting "" for one blanks the year and tech counter of the top bar. */
static void print_outlined_with(const data_row_t *e, print_outlined_fn_t print) {
    const data_item_t *a1 = draw_functions_arg(e, 1);
    char *owned = is_number(e, 1) ? p8_tostr_fixed(frame_zone(), number(e, 1)) : NULL;
    const char *t = a1 != NULL && a1->text != NULL ? a1->text : (owned != NULL ? owned : "");
    p8_fixed_t x = number(e, 2);
    p8_fixed_t y = number(e, 3);
    int c = p8_fixed_flr_int(number(e, 4));
    p8_fixed_t a = is_number(e, 5) ? number(e, 5) : p8_fixed_from_int(0);
    print(g_draw_functions_gfx, g_draw_functions_mem, t, x, y, c, a);
}

static void print_outlined_entry(const data_row_t *e) {
    print_outlined_with(e, print_outlined_direct);
}

static void print_outlined_text_image_entry(const data_row_t *e) {
    print_outlined_with(e, hud_print_outlined_text_image);
}

static void draw_help_frame_entry(const data_row_t *e) {
    hud_draw_help_frame(g_draw_functions_gfx, g_draw_functions_mem, number(e, 1), number(e, 2), number(e, 3));
}

static void spr_entry(const data_row_t *e) {
    int n = p8_fixed_flr_int(number(e, 1));
    int x = p8_fixed_flr_int(number(e, 2));
    int y = p8_fixed_flr_int(number(e, 3));
    if (is_number(e, 4) && is_number(e, 5)) {
        int w = p8_fixed_flr_int(number(e, 4));
        int h = p8_fixed_flr_int(number(e, 5));
        p8_spr(g_draw_functions_gfx, g_draw_functions_mem, n, x, y, w, h);
    } else {
        p8_spr_default(g_draw_functions_gfx, g_draw_functions_mem, n, x, y);
    }
}

static const draw_functions_fn_t draw_functions_by_id[DRAW_FUNCTION_COUNT] = {
    [DRAW_FUNCTION_NONE] = NULL,
    [DRAW_FUNCTION_DRAW_BAR] = draw_bar_entry,
    [DRAW_FUNCTION_PRINT_OUTLINED] = print_outlined_entry,
    [DRAW_FUNCTION_DRAW_HELP_FRAME] = draw_help_frame_entry,
    [DRAW_FUNCTION_SPR] = spr_entry,
};

static void draw_rows_dispatch_one(p8_graphics_t *gfx, p8_vram_t *mem, const data_row_t *e, draw_functions_fn_t print_outlined) {
    draw_functions_fn_t fn;

    assert((unsigned)e->draw_function < DRAW_FUNCTION_COUNT && "a row's draw_function is one of draw_function_t");
    assert(e->draw_function != DRAW_FUNCTION_DRAW_ROW && "a layout row is drawn by its message box");
    fn = e->draw_function == DRAW_FUNCTION_PRINT_OUTLINED ? print_outlined : draw_functions_by_id[e->draw_function];
    g_draw_functions_gfx = gfx;
    g_draw_functions_mem = mem;

    if (fn != NULL) {
        fn(e);
    }
}

void draw_functions_draw_row(p8_graphics_t *gfx, p8_vram_t *mem, const data_row_t *row) {
    draw_rows_dispatch_one(gfx, mem, row, print_outlined_entry);
}

void draw_functions_draw_row_text_image(p8_graphics_t *gfx, p8_vram_t *mem, const data_row_t *row) {
    draw_rows_dispatch_one(gfx, mem, row, print_outlined_text_image_entry);
}

void draw_functions_draw_rows_default(p8_graphics_t *gfx, p8_vram_t *mem, const data_row_t *const *rb, int n) {
    int i;

    for (i = 0; i < n; i++) {
        draw_rows_dispatch_one(gfx, mem, rb[i], print_outlined_entry);
    }
}
