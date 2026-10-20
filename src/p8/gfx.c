#include "p8/gfx.h"

#include "p8/draw_backend.h"

#include <assert.h>
#include <stddef.h>

static const p8_draw_backend_t *active_backend = NULL;

static const p8_draw_backend_t *backend(void) {
    return active_backend != NULL ? active_backend : p8_gfx_software_backend();
}

const p8_draw_backend_t *p8_draw_backend_set(const p8_draw_backend_t *new_backend) {
    const p8_draw_backend_t *previous = backend();
    active_backend = new_backend;
    return previous;
}

const p8_draw_backend_t *p8_draw_backend_get(void) {
    return backend();
}

void p8_gfx_init(p8_graphics_t *gfx) {
    gfx->camera_x = 0;
    gfx->camera_y = 0;
    gfx->fillp_pattern = 0;
    gfx->fillp_transparent = 0;
    gfx->pen_color = 6;
}

void p8_cls(p8_vram_t *mem) {
    backend()->cls(mem);
}

void p8_pset(const p8_graphics_t *gfx, p8_vram_t *mem, int x, int y, int col) {
    backend()->pset(gfx, mem, x, y, col);
}

void p8_psets(const p8_graphics_t *gfx, p8_vram_t *mem, const p8_point_t *points, int count) {
    assert(count >= 0 && (count == 0 || points != NULL) && "p8_psets: a negative count or a NULL list");
    backend()->psets(gfx, mem, points, count);
}

void p8_camera(p8_graphics_t *gfx, int x, int y) {
    gfx->camera_x = x;
    gfx->camera_y = y;
}

void p8_camera_reset(p8_graphics_t *gfx) {
    p8_camera(gfx, 0, 0);
}

void p8_palt(p8_vram_t *mem, int col, int transparent) {
    if (col < 0 || col > 15) {
        return;
    }
    if (transparent) {
        mem->draw_palette[col] = (unsigned char)(mem->draw_palette[col] | P8_DRAW_PALETTE_TRANSPARENT_BIT);
    } else {
        mem->draw_palette[col] = (unsigned char)(mem->draw_palette[col] & ~P8_DRAW_PALETTE_TRANSPARENT_BIT);
    }
}

int p8_gfx_is_transparent(const p8_vram_t *mem, int col) {
    if (col < 0 || col > 15) {
        return 0;
    }
    return (mem->draw_palette[col] & P8_DRAW_PALETTE_TRANSPARENT_BIT) != 0;
}

void p8_line(const p8_graphics_t *gfx, p8_vram_t *mem, int x1, int y1, int x2, int y2, int col) {
    backend()->line(gfx, mem, x1, y1, x2, y2, col);
}

void p8_lines(const p8_graphics_t *gfx, p8_vram_t *mem, const p8_segment_t *segments, int count, int col) {
    assert(count >= 0 && (count == 0 || segments != NULL) && "p8_lines: a negative count or a NULL list");
    backend()->lines(gfx, mem, segments, count, col);
}

void p8_rect(const p8_graphics_t *gfx, p8_vram_t *mem, int x1, int y1, int x2, int y2, int col) {
    backend()->rect(gfx, mem, x1, y1, x2, y2, col);
}

void p8_rectfill(const p8_graphics_t *gfx, p8_vram_t *mem, int x1, int y1, int x2, int y2, int col) {
    backend()->rectfill(gfx, mem, x1, y1, x2, y2, col);
}

void p8_circ(const p8_graphics_t *gfx, p8_vram_t *mem, int cx, int cy, int r, int col) {
    backend()->circ(gfx, mem, cx, cy, r, col);
}

void p8_circfill(const p8_graphics_t *gfx, p8_vram_t *mem, int cx, int cy, int r, int col) {
    backend()->circfill(gfx, mem, cx, cy, r, col);
}

void p8_fillp(p8_graphics_t *gfx, unsigned int pattern) {
    gfx->fillp_pattern = pattern & 0xFFFFu;
    gfx->fillp_transparent = 0;
}

void p8_fillp_transparent(p8_graphics_t *gfx, unsigned int pattern) {
    gfx->fillp_pattern = pattern & 0xFFFFu;
    gfx->fillp_transparent = 1;
}

void p8_fillp_reset(p8_graphics_t *gfx) {
    p8_fillp(gfx, 0);
}

unsigned char p8_sget(const p8_vram_t *mem, int x, int y) {
    if (x < 0 || x >= P8_SPRITE_SHEET_WIDTH || y < 0 || y >= P8_SPRITE_SHEET_HEIGHT) {
        return 0;
    }
    return mem->sprite_sheet[y][x];
}

void p8_spr(const p8_graphics_t *gfx, p8_vram_t *mem, int n, int x, int y, int w, int h) {
    backend()->spr(gfx, mem, n, x, y, w, h);
}

void p8_spr_default(const p8_graphics_t *gfx, p8_vram_t *mem, int n, int x, int y) {
    p8_spr(gfx, mem, n, x, y, 1, 1);
}

static int map_cell_in_range(int x, int y) {
    return x >= 0 && x < P8_MAP_WIDTH && y >= 0 && y < P8_MAP_HEIGHT;
}

void p8_mset(p8_vram_t *mem, int x, int y, int val) {
    if (!map_cell_in_range(x, y)) {
        return;
    }
    mem->map_cells[y][x] = (unsigned char)val;
    mem->map_version = p8_vram_next_map_version();
}

static unsigned char map_cell_get(const p8_vram_t *mem, int x, int y) {
    if (!map_cell_in_range(x, y)) {
        return 0;
    }
    return mem->map_cells[y][x];
}

static int floor_div_sprite(int a) {
    return a >= 0 ? a / P8_SPRITE_SIZE : -((-a + P8_SPRITE_SIZE - 1) / P8_SPRITE_SIZE);
}

static int clamp_cells(int i, int count) {
    return i < 0 ? 0 : (i > count ? count : i);
}

/* Half-open [*first, *end) of the cells 0..count-1 whose 8-pixel span, drawn from origin, meets 0..extent-1 after camera. */
static void visible_cells(int origin, int camera, int extent, int count, int *first, int *end) {
    *first = clamp_cells(floor_div_sprite(camera - origin), count);
    *end = clamp_cells(floor_div_sprite(camera - origin + extent - 1) + 1, count);
}

void p8_map(const p8_graphics_t *gfx, p8_vram_t *mem, int cel_x, int cel_y, int sx, int sy, int cel_w, int cel_h) {
    int c_first, c_end, r_first, r_end;
    visible_cells(sx, gfx->camera_x, P8_SCREEN_WIDTH, cel_w, &c_first, &c_end);
    visible_cells(sy, gfx->camera_y, P8_SCREEN_HEIGHT, cel_h, &r_first, &r_end);
    for (int r = r_first; r < r_end; r++) {
        for (int c = c_first; c < c_end; c++) {
            unsigned char sprite_n = map_cell_get(mem, cel_x + c, cel_y + r);
            if (sprite_n == 0) {
                continue;
            }
            p8_spr(gfx, mem, sprite_n, sx + c * P8_SPRITE_SIZE, sy + r * P8_SPRITE_SIZE, 1, 1);
        }
    }
}

void p8_print(const p8_graphics_t *gfx, p8_vram_t *mem, const char *str, int x, int y, int col) {
    backend()->print(gfx, mem, str, x, y, col);
}

void p8_print_outlined(const p8_graphics_t *gfx, p8_vram_t *mem, const char *str, int x, int y, int col) {
    backend()->print_outlined(gfx, mem, str, x, y, col);
}
