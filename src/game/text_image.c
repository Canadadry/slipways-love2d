#include "game/text_image.h"

#include <string.h>

#include "game/image_slot.h"
#include "p8/image.h"

/* The outline's pixel on each side of the glyphs: 4 a character plus 1 wide, 7 high. */
enum { OUTLINE = 1, TEXT_IMAGE_HEIGHT = P8_FONT_GLYPH_HEIGHT + 2 * OUTLINE, TEXT_CAP = 31 };

typedef struct {
    int col;
    int length;
    char text[TEXT_CAP + 1];
} text_image_key_t;

typedef struct {
    text_image_key_t key;
    unsigned long stamp;
    unsigned long frame;
} text_image_slot_t;

static text_image_slot_t g_slots[IMAGE_SLOT_TEXT_COUNT];
static unsigned long g_stamp;
static unsigned long g_frame = 1;

static int image_width(int length) {
    return length * P8_FONT_CELL_WIDTH + OUTLINE;
}

static int text_length(const char *str) {
    size_t length = strlen(str);
    return length > TEXT_CAP ? -1 : (int)length;
}

int text_image_cacheable(const char *str) {
    int length = text_length(str);
    return length > 0 && image_width(length) <= p8_image_size(IMAGE_SLOT_TEXT_FIRST).width;
}

void text_image_frame_begin(void) {
    g_frame++;
}

static int same_key(const text_image_key_t *a, const text_image_key_t *b) {
    return a->col == b->col && a->length == b->length && memcmp(a->text, b->text, (size_t)a->length) == 0;
}

/* Returns the slot holding key (*built set), else the least recently used slot not copied this
 * frame, else -1. */
static int slot_for(const text_image_key_t *key, int *built) {
    int oldest = -1;
    for (int i = 0; i < IMAGE_SLOT_TEXT_COUNT; i++) {
        if (g_slots[i].key.length > 0 && same_key(&g_slots[i].key, key)) {
            *built = 1;
            return i;
        }
        if (g_slots[i].frame != g_frame && (oldest < 0 || g_slots[i].stamp < g_slots[oldest].stamp)) {
            oldest = i;
        }
    }
    *built = 0;
    return oldest;
}

/* Draws no visual random number and reads no game state. */
static void text_image_build_draw(p8_graphics_t *gfx, p8_vram_t *mem, void *context) {
    const text_image_key_t *key = context;
    p8_print_outlined(gfx, mem, key->text, OUTLINE, OUTLINE, key->col);
}

void text_image_print_outlined(p8_graphics_t *gfx, p8_vram_t *mem, const char *str, int x, int y, int col) {
    if (!text_image_cacheable(str)) {
        p8_print_outlined(gfx, mem, str, x, y, col);
        return;
    }
    text_image_key_t key = {0};
    key.col = col;
    key.length = text_length(str);
    memcpy(key.text, str, (size_t)key.length);
    int built;
    int i = slot_for(&key, &built);
    if (i < 0) {
        p8_print_outlined(gfx, mem, str, x, y, col);
        return;
    }
    if (!built) {
        g_slots[i].key = key;
        p8_image_build(gfx, mem, IMAGE_SLOT_TEXT_FIRST + i, image_width(key.length), TEXT_IMAGE_HEIGHT,
                       text_image_build_draw, &g_slots[i].key);
    }
    g_slots[i].stamp = ++g_stamp;
    g_slots[i].frame = g_frame;
    p8_image_draw(gfx, mem, IMAGE_SLOT_TEXT_FIRST + i, x - OUTLINE, y - OUTLINE);
}
