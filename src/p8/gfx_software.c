#include "p8/draw_backend.h"
#include "p8/image.h"

#include <stdlib.h>
#include <string.h>

/* Holds only while callers draw colours 0..15: pset, line, rect, circ and print store col unmasked, and only dump.c asserts it. */
#define IMAGE_EMPTY 0xFF

#define SLOT_PIXELS(width, height) (width) * (height),
static const int SLOT_PIXELS[P8_IMAGE_SLOT_COUNT] = {P8_IMAGE_SLOTS(SLOT_PIXELS)};

static unsigned char image_store[P8_IMAGE_PIXELS];
static p8_image_size_t image_built[P8_IMAGE_SLOT_COUNT];

/* NULL while drawing to the screen. */
static unsigned char *target;
static p8_image_size_t target_size;
static int window_x;
static int window_y;

static void software_cls(p8_vram_t *mem) {
    memset(mem->screen, 0, sizeof(mem->screen));
}

static void plot(const p8_graphics_t *gfx, p8_vram_t *mem, int x, int y, int col) {
    int sx = x - gfx->camera_x;
    int sy = y - gfx->camera_y;
    if (sx < 0 || sx >= P8_SCREEN_WIDTH || sy < 0 || sy >= P8_SCREEN_HEIGHT) {
        return;
    }
    if (target == NULL) {
        mem->screen[sy][sx] = (unsigned char)col;
        return;
    }
    int ix = window_x + sx;
    int iy = window_y + sy;
    if (ix < target_size.width && iy < target_size.height) {
        target[iy * target_size.width + ix] = (unsigned char)col;
    }
}

static void software_line(const p8_graphics_t *gfx, p8_vram_t *mem, int x1, int y1, int x2, int y2, int col) {
    int dx = abs(x2 - x1);
    int sx = x1 < x2 ? 1 : -1;
    int dy = -abs(y2 - y1);
    int sy = y1 < y2 ? 1 : -1;
    int err = dx + dy;
    int x = x1;
    int y = y1;

    for (;;) {
        plot(gfx, mem, x, y, col);
        if (x == x2 && y == y2) {
            break;
        }
        int e2 = 2 * err;
        if (e2 >= dy) {
            err += dy;
            x += sx;
        }
        if (e2 <= dx) {
            err += dx;
            y += sy;
        }
    }
}

static int fillp_bit_set(const p8_graphics_t *gfx, int x, int y) {
    int row = ((y % 4) + 4) % 4;
    int col = ((x % 4) + 4) % 4;
    int bit_index = 15 - (row * 4 + col);
    return (gfx->fillp_pattern >> bit_index) & 1;
}

/* Masked here, not in p8_pset, so devtools/dump.c's assert still catches a primitive storing an index above 0x0f. */
static int fillp_color(const p8_graphics_t *gfx, int x, int y, int col) {
    if (!fillp_bit_set(gfx, x, y)) {
        return col & 0x0F;
    }
    if (gfx->fillp_transparent) {
        return -1;
    }
    return (col >> 4) & 0x0F;
}

static int min(int a, int b) {
    return a < b ? a : b;
}

static int max(int a, int b) {
    return a < b ? b : a;
}

static void software_rect(const p8_graphics_t *gfx, p8_vram_t *mem, int x1, int y1, int x2, int y2, int col) {
    int xmin = min(x1, x2);
    int xmax = max(x1, x2);
    int ymin = min(y1, y2);
    int ymax = max(y1, y2);

    for (int x = xmin; x <= xmax; x++) {
        plot(gfx, mem, x, ymin, col);
        plot(gfx, mem, x, ymax, col);
    }
    for (int y = ymin; y <= ymax; y++) {
        plot(gfx, mem, xmin, y, col);
        plot(gfx, mem, xmax, y, col);
    }
}

static void circ_plot8(const p8_graphics_t *gfx, p8_vram_t *mem, int cx, int cy, int dx, int dy, int col) {
    plot(gfx, mem, cx + dx, cy + dy, col);
    plot(gfx, mem, cx - dx, cy + dy, col);
    plot(gfx, mem, cx + dx, cy - dy, col);
    plot(gfx, mem, cx - dx, cy - dy, col);
    plot(gfx, mem, cx + dy, cy + dx, col);
    plot(gfx, mem, cx - dy, cy + dx, col);
    plot(gfx, mem, cx + dy, cy - dx, col);
    plot(gfx, mem, cx - dy, cy - dx, col);
}

static void software_circ(const p8_graphics_t *gfx, p8_vram_t *mem, int cx, int cy, int r, int col) {
    if (r < 0) {
        return;
    }
    int x = r;
    int y = 0;
    int err = 0;

    while (x >= y) {
        circ_plot8(gfx, mem, cx, cy, x, y, col);
        y++;
        if (err <= 0) {
            err += 2 * y + 1;
        }
        if (err > 0) {
            x--;
            err -= 2 * x + 1;
        }
    }
}

static void circfill_span(const p8_graphics_t *gfx, p8_vram_t *mem, int x1, int x2, int y, int col) {
    int xmin = min(x1, x2);
    int xmax = max(x1, x2);

    for (int x = xmin; x <= xmax; x++) {
        int pattern_col = fillp_color(gfx, x, y, col);
        if (pattern_col >= 0) {
            plot(gfx, mem, x, y, pattern_col);
        }
    }
}

static void software_circfill(const p8_graphics_t *gfx, p8_vram_t *mem, int cx, int cy, int r, int col) {
    if (r < 0) {
        return;
    }
    int x = r;
    int y = 0;
    int err = 0;

    while (x >= y) {
        circfill_span(gfx, mem, cx - x, cx + x, cy + y, col);
        circfill_span(gfx, mem, cx - x, cx + x, cy - y, col);
        circfill_span(gfx, mem, cx - y, cx + y, cy + x, col);
        circfill_span(gfx, mem, cx - y, cx + y, cy - x, col);
        y++;
        if (err <= 0) {
            err += 2 * y + 1;
        }
        if (err > 0) {
            x--;
            err -= 2 * x + 1;
        }
    }
}

static unsigned char sheet_get(const p8_vram_t *mem, int x, int y) {
    if (x < 0 || x >= P8_SPRITE_SHEET_WIDTH || y < 0 || y >= P8_SPRITE_SHEET_HEIGHT) {
        return 0;
    }
    return mem->sprite_sheet[y][x];
}

static void software_spr(const p8_graphics_t *gfx, p8_vram_t *mem, int n, int x, int y, int w, int h) {
    const int sprites_per_row = P8_SPRITE_SHEET_WIDTH / P8_SPRITE_SIZE;
    int sheet_x = (n % sprites_per_row) * P8_SPRITE_SIZE;
    int sheet_y = (n / sprites_per_row) * P8_SPRITE_SIZE;
    int pixel_w = w * P8_SPRITE_SIZE;
    int pixel_h = h * P8_SPRITE_SIZE;

    for (int dy = 0; dy < pixel_h; dy++) {
        for (int dx = 0; dx < pixel_w; dx++) {
            unsigned char col = sheet_get(mem, sheet_x + dx, sheet_y + dy);
            if (mem->draw_palette[col] & P8_DRAW_PALETTE_TRANSPARENT_BIT) {
                continue;
            }
            plot(gfx, mem, x + dx, y + dy, mem->draw_palette[col] & 0x0f);
        }
    }
}

static const unsigned char FONT_ROWS_PLACEHOLDER[P8_FONT_GLYPH_HEIGHT] = {7, 7, 7, 7, 7};
static const unsigned char FONT_ROWS_SPACE[P8_FONT_GLYPH_HEIGHT] = {0, 0, 0, 0, 0};

static const unsigned char FONT_ROWS_0[P8_FONT_GLYPH_HEIGHT] = {7, 5, 5, 5, 7};
static const unsigned char FONT_ROWS_1[P8_FONT_GLYPH_HEIGHT] = {2, 6, 2, 2, 7};
static const unsigned char FONT_ROWS_2[P8_FONT_GLYPH_HEIGHT] = {7, 1, 7, 4, 7};
static const unsigned char FONT_ROWS_3[P8_FONT_GLYPH_HEIGHT] = {7, 1, 7, 1, 7};
static const unsigned char FONT_ROWS_4[P8_FONT_GLYPH_HEIGHT] = {5, 5, 7, 1, 1};
static const unsigned char FONT_ROWS_5[P8_FONT_GLYPH_HEIGHT] = {7, 4, 7, 1, 7};
static const unsigned char FONT_ROWS_6[P8_FONT_GLYPH_HEIGHT] = {7, 4, 7, 5, 7};
static const unsigned char FONT_ROWS_7[P8_FONT_GLYPH_HEIGHT] = {7, 1, 1, 1, 1};
static const unsigned char FONT_ROWS_8[P8_FONT_GLYPH_HEIGHT] = {7, 5, 7, 5, 7};
static const unsigned char FONT_ROWS_9[P8_FONT_GLYPH_HEIGHT] = {7, 5, 7, 1, 7};

static const unsigned char FONT_ROWS_A[P8_FONT_GLYPH_HEIGHT] = {2, 5, 7, 5, 5};
static const unsigned char FONT_ROWS_B[P8_FONT_GLYPH_HEIGHT] = {4, 4, 6, 5, 6};
static const unsigned char FONT_ROWS_C[P8_FONT_GLYPH_HEIGHT] = {3, 4, 4, 4, 3};
static const unsigned char FONT_ROWS_D[P8_FONT_GLYPH_HEIGHT] = {1, 1, 3, 5, 3};
static const unsigned char FONT_ROWS_E[P8_FONT_GLYPH_HEIGHT] = {7, 4, 6, 4, 7};
static const unsigned char FONT_ROWS_F[P8_FONT_GLYPH_HEIGHT] = {7, 4, 6, 4, 4};
static const unsigned char FONT_ROWS_G[P8_FONT_GLYPH_HEIGHT] = {3, 4, 5, 5, 3};
static const unsigned char FONT_ROWS_H[P8_FONT_GLYPH_HEIGHT] = {5, 5, 7, 5, 5};
static const unsigned char FONT_ROWS_I[P8_FONT_GLYPH_HEIGHT] = {7, 2, 2, 2, 7};
static const unsigned char FONT_ROWS_J[P8_FONT_GLYPH_HEIGHT] = {1, 1, 1, 5, 2};
static const unsigned char FONT_ROWS_K[P8_FONT_GLYPH_HEIGHT] = {5, 5, 6, 5, 5};
static const unsigned char FONT_ROWS_L[P8_FONT_GLYPH_HEIGHT] = {4, 4, 4, 4, 7};
static const unsigned char FONT_ROWS_M[P8_FONT_GLYPH_HEIGHT] = {5, 7, 7, 5, 5};
static const unsigned char FONT_ROWS_N[P8_FONT_GLYPH_HEIGHT] = {5, 6, 5, 5, 5};
static const unsigned char FONT_ROWS_O[P8_FONT_GLYPH_HEIGHT] = {2, 5, 5, 5, 2};
static const unsigned char FONT_ROWS_P[P8_FONT_GLYPH_HEIGHT] = {6, 5, 6, 4, 4};
static const unsigned char FONT_ROWS_Q[P8_FONT_GLYPH_HEIGHT] = {2, 5, 5, 3, 1};
static const unsigned char FONT_ROWS_R[P8_FONT_GLYPH_HEIGHT] = {6, 5, 6, 5, 5};
static const unsigned char FONT_ROWS_S[P8_FONT_GLYPH_HEIGHT] = {3, 4, 2, 1, 6};
static const unsigned char FONT_ROWS_T[P8_FONT_GLYPH_HEIGHT] = {7, 2, 2, 2, 2};
static const unsigned char FONT_ROWS_U[P8_FONT_GLYPH_HEIGHT] = {5, 5, 5, 5, 2};
static const unsigned char FONT_ROWS_W[P8_FONT_GLYPH_HEIGHT] = {5, 5, 5, 7, 5};
static const unsigned char FONT_ROWS_X[P8_FONT_GLYPH_HEIGHT] = {5, 5, 2, 5, 5};
static const unsigned char FONT_ROWS_Y[P8_FONT_GLYPH_HEIGHT] = {5, 5, 2, 2, 2};
static const unsigned char FONT_ROWS_Z[P8_FONT_GLYPH_HEIGHT] = {7, 1, 2, 4, 7};

static const unsigned char FONT_ROWS_BANG[P8_FONT_GLYPH_HEIGHT] = {2, 2, 2, 0, 2};
static const unsigned char FONT_ROWS_DOLLAR[P8_FONT_GLYPH_HEIGHT] = {2, 7, 2, 7, 2};
static const unsigned char FONT_ROWS_PERCENT[P8_FONT_GLYPH_HEIGHT] = {5, 1, 2, 4, 5};
static const unsigned char FONT_ROWS_APOSTROPHE[P8_FONT_GLYPH_HEIGHT] = {2, 2, 0, 0, 0};
static const unsigned char FONT_ROWS_LPAREN[P8_FONT_GLYPH_HEIGHT] = {1, 2, 2, 2, 1};
static const unsigned char FONT_ROWS_RPAREN[P8_FONT_GLYPH_HEIGHT] = {4, 2, 2, 2, 4};
static const unsigned char FONT_ROWS_PLUS[P8_FONT_GLYPH_HEIGHT] = {0, 2, 7, 2, 0};
static const unsigned char FONT_ROWS_COMMA[P8_FONT_GLYPH_HEIGHT] = {0, 0, 0, 2, 4};
static const unsigned char FONT_ROWS_MINUS[P8_FONT_GLYPH_HEIGHT] = {0, 0, 7, 0, 0};
static const unsigned char FONT_ROWS_PERIOD[P8_FONT_GLYPH_HEIGHT] = {0, 0, 0, 0, 2};
static const unsigned char FONT_ROWS_SLASH[P8_FONT_GLYPH_HEIGHT] = {1, 1, 2, 4, 4};
static const unsigned char FONT_ROWS_COLON[P8_FONT_GLYPH_HEIGHT] = {0, 2, 0, 2, 0};
static const unsigned char FONT_ROWS_EQUALS[P8_FONT_GLYPH_HEIGHT] = {0, 7, 0, 7, 0};
static const unsigned char FONT_ROWS_QUESTION[P8_FONT_GLYPH_HEIGHT] = {6, 1, 2, 0, 2};

const unsigned char *p8_font_glyph_rows(char character) {
    switch (character) {
        case ' ': return FONT_ROWS_SPACE;
        case '0': return FONT_ROWS_0;
        case '1': return FONT_ROWS_1;
        case '2': return FONT_ROWS_2;
        case '3': return FONT_ROWS_3;
        case '4': return FONT_ROWS_4;
        case '5': return FONT_ROWS_5;
        case '6': return FONT_ROWS_6;
        case '7': return FONT_ROWS_7;
        case '8': return FONT_ROWS_8;
        case '9': return FONT_ROWS_9;
        case 'A': case 'a': return FONT_ROWS_A;
        case 'B': case 'b': return FONT_ROWS_B;
        case 'C': case 'c': return FONT_ROWS_C;
        case 'D': case 'd': return FONT_ROWS_D;
        case 'E': case 'e': return FONT_ROWS_E;
        case 'F': case 'f': return FONT_ROWS_F;
        case 'G': case 'g': return FONT_ROWS_G;
        case 'H': case 'h': return FONT_ROWS_H;
        case 'I': case 'i': return FONT_ROWS_I;
        case 'J': case 'j': return FONT_ROWS_J;
        case 'K': case 'k': return FONT_ROWS_K;
        case 'L': case 'l': return FONT_ROWS_L;
        case 'M': case 'm': return FONT_ROWS_M;
        case 'N': case 'n': return FONT_ROWS_N;
        case 'O': case 'o': return FONT_ROWS_O;
        case 'P': case 'p': return FONT_ROWS_P;
        case 'Q': case 'q': return FONT_ROWS_Q;
        case 'R': case 'r': return FONT_ROWS_R;
        case 'S': case 's': return FONT_ROWS_S;
        case 'T': case 't': return FONT_ROWS_T;
        case 'U': case 'u': return FONT_ROWS_U;
        case 'V': case 'v': return FONT_ROWS_U;
        case 'W': case 'w': return FONT_ROWS_W;
        case 'X': case 'x': return FONT_ROWS_X;
        case 'Y': case 'y': return FONT_ROWS_Y;
        case 'Z': case 'z': return FONT_ROWS_Z;
        case '!': return FONT_ROWS_BANG;
        case '$': return FONT_ROWS_DOLLAR;
        case '%': return FONT_ROWS_PERCENT;
        case '\'': return FONT_ROWS_APOSTROPHE;
        case '(': return FONT_ROWS_LPAREN;
        case ')': return FONT_ROWS_RPAREN;
        case '+': return FONT_ROWS_PLUS;
        case ',': return FONT_ROWS_COMMA;
        case '-': return FONT_ROWS_MINUS;
        case '.': return FONT_ROWS_PERIOD;
        case '/': return FONT_ROWS_SLASH;
        case ':': return FONT_ROWS_COLON;
        case '=': return FONT_ROWS_EQUALS;
        case '?': return FONT_ROWS_QUESTION;
        default: return FONT_ROWS_PLACEHOLDER;
    }
}

int p8_font_has_glyph(char character) {
    return p8_font_glyph_rows(character) != FONT_ROWS_PLACEHOLDER;
}

static void software_print(const p8_graphics_t *gfx, p8_vram_t *mem, const char *str, int x, int y, int col) {
    int cx = x;
    for (const char *p = str; *p != '\0'; p++) {
        const unsigned char *rows = p8_font_glyph_rows(*p);
        for (int row = 0; row < P8_FONT_GLYPH_HEIGHT; row++) {
            for (int gx = 0; gx < P8_FONT_GLYPH_WIDTH; gx++) {
                int bit_index = P8_FONT_GLYPH_WIDTH - 1 - gx;
                if ((rows[row] >> bit_index) & 1) {
                    plot(gfx, mem, cx + gx, y + row, col);
                }
            }
        }
        cx += P8_FONT_CELL_WIDTH;
    }
}

static void software_print_outlined(const p8_graphics_t *gfx, p8_vram_t *mem, const char *str, int x, int y, int col) {
    for (int dy = -1; dy <= 1; dy++) {
        for (int dx = -1; dx <= 1; dx++) {
            if (dx != 0 || dy != 0) software_print(gfx, mem, str, x + dx, y + dy, 0);
        }
    }
    software_print(gfx, mem, str, x, y, col);
}

static void software_rectfill(const p8_graphics_t *gfx, p8_vram_t *mem, int x1, int y1, int x2, int y2, int col) {
    int xmin = min(x1, x2);
    int xmax = max(x1, x2);
    int ymin = min(y1, y2);
    int ymax = max(y1, y2);

    for (int y = ymin; y <= ymax; y++) {
        for (int x = xmin; x <= xmax; x++) {
            int pattern_col = fillp_color(gfx, x, y, col);
            if (pattern_col >= 0) {
                plot(gfx, mem, x, y, pattern_col);
            }
        }
    }
}

static void software_pset(const p8_graphics_t *gfx, p8_vram_t *mem, int x, int y, int col) {
    plot(gfx, mem, x, y, col);
}

static unsigned char *slot_pixels(int slot) {
    unsigned char *pixels = image_store;
    for (int before = 0; before < slot; before++) {
        pixels += SLOT_PIXELS[before];
    }
    return pixels;
}

static void software_image_begin(int slot, int w, int h) {
    target = slot_pixels(slot);
    target_size.width = w;
    target_size.height = h;
    image_built[slot] = target_size;
    memset(target, IMAGE_EMPTY, (size_t)w * (size_t)h);
}

static void software_image_window(int slot, int x, int y) {
    (void)slot;
    window_x = x;
    window_y = y;
}

static void software_image_end(int slot) {
    (void)slot;
    target = NULL;
}

static void software_image_draw(const p8_graphics_t *gfx, p8_vram_t *mem, int slot, int x, int y) {
    const unsigned char *pixels = slot_pixels(slot);
    p8_image_size_t size = image_built[slot];
    int left = x - gfx->camera_x;
    int top = y - gfx->camera_y;
    int ix_first = max(0, -left);
    int ix_end = min(size.width, P8_SCREEN_WIDTH - left);
    int iy_first = max(0, -top);
    int iy_end = min(size.height, P8_SCREEN_HEIGHT - top);
    for (int iy = iy_first; iy < iy_end; iy++) {
        const unsigned char *row = pixels + iy * size.width;
        unsigned char *screen_row = mem->screen[top + iy];
        for (int ix = ix_first; ix < ix_end; ix++) {
            if (row[ix] != IMAGE_EMPTY) {
                screen_row[left + ix] = row[ix];
            }
        }
    }
}

static void software_lines(const p8_graphics_t *gfx, p8_vram_t *mem, const p8_segment_t *segments, int count, int col) {
    for (int i = 0; i < count; i++) {
        software_line(gfx, mem, segments[i].x1, segments[i].y1, segments[i].x2, segments[i].y2, col);
    }
}

static void software_psets(const p8_graphics_t *gfx, p8_vram_t *mem, const p8_point_t *points, int count) {
    for (int i = 0; i < count; i++) {
        software_pset(gfx, mem, points[i].x, points[i].y, points[i].col);
    }
}

static const p8_draw_backend_t SOFTWARE_BACKEND = {
    software_cls, software_pset, software_line, software_rect, software_rectfill,
    software_circ, software_circfill, software_spr, software_print, software_print_outlined,
    software_image_begin, software_image_window, software_image_end, software_image_draw,
    software_lines, software_psets,
};

const p8_draw_backend_t *p8_gfx_software_backend(void) {
    return &SOFTWARE_BACKEND;
}
