#include "game/row_image.h"

#include <assert.h>
#include <stdint.h>
#include <string.h>

#include "game/hud.h"
#include "game/image_slot.h"
#include "p8/image.h"

enum { ROW_IMAGE_HEIGHT = P8_SPRITE_SIZE, ROW_IMAGE_KEY_CAP = LAYOUT_ROW_KEYS_CAP, SPRITE_COUNT = 256, BLOCK_COUNT = 16 };

/* One entry a sprite item: sprite number, palette block, x offset. */
typedef struct {
    const p8_vram_t *mem;
    uint32_t hash;
    int count;
    int extent;
    uint32_t entries[ROW_IMAGE_KEY_CAP];
} row_image_key_t;

typedef struct {
    row_image_key_t key;
    unsigned long stamp;
} row_image_slot_t;

static row_image_slot_t g_slots[IMAGE_SLOT_ROW_COUNT];
static unsigned long g_stamp;

static int row_image_width(void) {
    return p8_image_size(IMAGE_SLOT_ROW_FIRST).width;
}

static uint32_t entry_of(const layout_row_item_t *item) {
    return (uint32_t)p8_fixed_flr_int(item->sprite_id) | (uint32_t)p8_fixed_flr_int(item->sprite_palette_block) << 8
         | (uint32_t)p8_fixed_flr_int(item->offset.x) << 12;
}

/* Fills key from the row; returns 0 when the row cannot be copied exactly. */
static int key_of(const layout_row_t *row, const p8_vram_t *mem, row_image_key_t *key) {
    key->mem = mem;
    key->count = 0;
    key->extent = 0;
    if (row == NULL || row->placed_items == NULL) {
        return 0;
    }
    for (int i = 0; i < row->placed_items_count; i++) {
        const layout_row_item_t *item = &row->placed_items[i];
        if (item->kind == LAYOUT_ROW_ITEM_NONE) {
            continue;
        }
        if (item->kind != LAYOUT_ROW_ITEM_SPRITE) {
            return 0;
        }
        int sprite = p8_fixed_flr_int(item->sprite_id);
        int block = p8_fixed_flr_int(item->sprite_palette_block);
        int x = p8_fixed_flr_int(item->offset.x);
        if (p8_fixed_from_int(x) != item->offset.x || item->offset.y != 0 || x < 0
            || x + P8_SPRITE_SIZE > row_image_width() || sprite < 0 || sprite >= SPRITE_COUNT || block < 0
            || block >= BLOCK_COUNT) {
            return 0;
        }
        assert(key->count < ROW_IMAGE_KEY_CAP && "a row holds at most LAYOUT_ROW_KEYS_CAP items");
        key->entries[key->count++] = entry_of(item);
        if (x + P8_SPRITE_SIZE > key->extent) {
            key->extent = x + P8_SPRITE_SIZE;
        }
    }
    if (key->count == 0) {
        return 0;
    }
    uint32_t hash = 2166136261u ^ (uint32_t)(uintptr_t)mem;
    for (int i = 0; i < key->count; i++) {
        hash = (hash ^ key->entries[i]) * 16777619u;
    }
    key->hash = hash;
    return 1;
}

int row_image_cacheable(const layout_row_t *row) {
    row_image_key_t key;
    return key_of(row, NULL, &key);
}

static int same_key(const row_image_key_t *a, const row_image_key_t *b) {
    return a->hash == b->hash && a->mem == b->mem && a->count == b->count
        && memcmp(a->entries, b->entries, (size_t)a->count * sizeof a->entries[0]) == 0;
}

/* Draws no visual random number and reads no game state: only the row's own items, each at its
 * whole offset, so at (offset, 0) whatever the caller's alignment. */
static void row_image_build_draw(p8_graphics_t *gfx, p8_vram_t *mem, void *context) {
    layout_row_draw_at(gfx, mem, context, vec_make(0, 0), 0);
}

static int slot_for(const row_image_key_t *key, int *built) {
    int oldest = 0;
    for (int i = 0; i < IMAGE_SLOT_ROW_COUNT; i++) {
        if (g_slots[i].key.mem != NULL && same_key(&g_slots[i].key, key)) {
            *built = 1;
            return i;
        }
        if (g_slots[i].stamp < g_slots[oldest].stamp) {
            oldest = i;
        }
    }
    *built = 0;
    return oldest;
}

void row_image_draw_at(p8_graphics_t *gfx, p8_vram_t *mem, const layout_row_t *row, vec_t p, p8_fixed_t a) {
    row_image_key_t key;
    if (!key_of(row, mem, &key)) {
        layout_row_draw_at(gfx, mem, row, p, a);
        return;
    }
    int built;
    int i = slot_for(&key, &built);
    if (!built) {
        p8_image_build(gfx, mem, IMAGE_SLOT_ROW_FIRST + i, key.extent, ROW_IMAGE_HEIGHT, row_image_build_draw, (void *)row);
        g_slots[i].key = key;
    }
    g_slots[i].stamp = ++g_stamp;
    vec_t origin = vec_sub(p, vec_make(p8_fixed_mul(a, row->width), 0));
    p8_image_draw(gfx, mem, IMAGE_SLOT_ROW_FIRST + i, p8_fixed_flr_int(origin.x), p8_fixed_flr_int(origin.y));
    hud_select_palette_block_default(mem);
}
