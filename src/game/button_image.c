#include "game/button_image.h"

#include <string.h>

#include "game/image_slot.h"

typedef struct {
    button_image_key_t key;
    int used;
    unsigned long stamp;
    unsigned long frame;
} button_image_slot_t;

/* Slots [first, first + count) of g_slots, image slot IMAGE_SLOT_BUTTON_ICON_FIRST + index. */
typedef struct {
    int first;
    int count;
} button_image_pool_t;

enum { BUTTON_IMAGE_SLOT_COUNT = IMAGE_SLOT_BUTTON_ICON_COUNT + IMAGE_SLOT_BUTTON_WIDE_COUNT };

typedef char button_image_slots_follow_each_other
    [IMAGE_SLOT_BUTTON_WIDE_FIRST == IMAGE_SLOT_BUTTON_ICON_FIRST + IMAGE_SLOT_BUTTON_ICON_COUNT ? 1 : -1];

static button_image_slot_t g_slots[BUTTON_IMAGE_SLOT_COUNT];
static const button_image_pool_t g_icon_pool = {0, IMAGE_SLOT_BUTTON_ICON_COUNT};
static const button_image_pool_t g_wide_pool = {IMAGE_SLOT_BUTTON_ICON_COUNT, IMAGE_SLOT_BUTTON_WIDE_COUNT};
static unsigned long g_stamp;
static unsigned long g_frame = 1;

void button_image_frame_begin(void) {
    g_frame++;
}

static int same_key(const button_image_key_t *a, const button_image_key_t *b) {
    return memcmp(a->words, b->words, sizeof a->words) == 0;
}

/* Returns the slot holding key (*built set), else the least recently used slot not copied this
 * frame, else -1. */
static int slot_for(button_image_pool_t pool, const button_image_key_t *key, int *built) {
    int oldest = -1;
    for (int i = pool.first; i < pool.first + pool.count; i++) {
        const button_image_slot_t *slot = &g_slots[i];
        if (slot->used && same_key(&slot->key, key)) {
            *built = 1;
            return i;
        }
        if (slot->frame != g_frame && (oldest < 0 || slot->stamp < g_slots[oldest].stamp)) {
            oldest = i;
        }
    }
    *built = 0;
    return oldest;
}

int button_image_draw(p8_graphics_t *gfx, p8_vram_t *mem, const button_image_key_t *key, int x, int y, int w, int h,
                      p8_image_draw_t draw, void *context) {
    if (w <= 0 || h <= 0 || w > BUTTON_IMAGE_WIDE_WIDTH || h > BUTTON_IMAGE_HEIGHT) {
        return 0;
    }
    int built;
    int i = slot_for(w <= BUTTON_IMAGE_ICON_WIDTH ? g_icon_pool : g_wide_pool, key, &built);
    if (i < 0) {
        return 0;
    }
    button_image_slot_t *slot = &g_slots[i];
    if (!built) {
        slot->key = *key;
        slot->used = 1;
        p8_image_build(gfx, mem, IMAGE_SLOT_BUTTON_ICON_FIRST + i, w, h, draw, context);
    }
    slot->stamp = ++g_stamp;
    slot->frame = g_frame;
    p8_image_draw(gfx, mem, IMAGE_SLOT_BUTTON_ICON_FIRST + i, x, y);
    return 1;
}
