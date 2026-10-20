#ifndef P8_VRAM_H
#define P8_VRAM_H

#define P8_DRAW_PALETTE_TRANSPARENT_BIT 0x80

#define P8_SCREEN_WIDTH 160
#define P8_SCREEN_HEIGHT 120

#define P8_SPRITE_SHEET_WIDTH 128
#define P8_SPRITE_SHEET_HEIGHT 128

#define P8_MAP_WIDTH 32
#define P8_MAP_HEIGHT 32

typedef struct {
    unsigned char draw_palette[16];
    unsigned char sprite_sheet[P8_SPRITE_SHEET_HEIGHT][P8_SPRITE_SHEET_WIDTH];
    unsigned char map_cells[P8_MAP_HEIGHT][P8_MAP_WIDTH];
    unsigned char screen[P8_SCREEN_HEIGHT][P8_SCREEN_WIDTH];
    /* Process-wide: no two map states of any vram share a version. */
    unsigned long map_version;
} p8_vram_t;

void p8_vram_init(p8_vram_t *mem);

/* A version never returned before. */
unsigned long p8_vram_next_map_version(void);

#endif
