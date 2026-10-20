#include "game/background.h"
#include "engine/entity_id.h"

#include <assert.h>
#include <stddef.h>
#include <string.h>

#include "engine/dispatch_context.h"
#include "engine/entity_registry.h"
#include "p8/fixed.h"
#include "game/gen.h"
#include "engine/entity.h"
#include "game/probe.h"
#include "engine/vec.h"
#include "game/wormhole.h"
#include "engine/frame_zone.h"
#include "engine/game_zone.h"
#include "game/image_slot.h"
#include "p8/image.h"

#define BACKGROUND_IMAGE_SIDE (P8_MAP_WIDTH * P8_SPRITE_SIZE)

static int g_background_up = 0;

static int background_to_int(p8_fixed_t x) {
    return p8_fixed_flr_int(x);
}

static void background_draw_method(entity_t *self, vec_t screen_pos) {
    (void)screen_pos;
    background_draw(dispatch_context_gfx(), dispatch_context_mem(), (const background_t *)self);
}

static down_t background_left_press_method(entity_t *self, mouse_pos_t mouse) {
    (void)mouse;
    return background_left_press((background_t *)self);
}

static const entity_class_t background_class_def = {
    .draw = background_draw_method,
    .hit_test = entity_registry_hit_test_always,
    .left_press = background_left_press_method,
};

static background_t *background_create(void) {
    background_t *self = (background_t *)allocator_alloc(game_zone(), sizeof *self);

    entity_init_instance(&self->header, &background_class_def, background_archetype.state, background_archetype.state_time);
    self->archetype = &background_archetype;
    entity_registry_set_layer(&self->header, self->archetype->layer);
    self->header.has |= ENTITY_HAS_IS_HOVERED;
    self->header.is_hovered = 0;
    self->seed = p8_fixed_rnd_state();
    background_construct(dispatch_context_mem(), self);
    entity_construct(&self->header);
    return self;
}

void background_init(void) {
    background_shutdown();
    g_background_up = 1;
}

void background_shutdown(void) {
    g_background_up = 0;
}

int background_ready(void) {
    return g_background_up;
}

const entity_class_t *background_class(void) {
    return &background_class_def;
}

background_t *background_new(void) {
    return background_create();
}

const background_t *background_live(void) {
    const entity_list_t *live = live_entities();
    for (int i = 0; i < live->count; i++) {
        const entity_t *candidate = live->items[i];
        if (candidate->class == &background_class_def) {
            return (const background_t *)candidate;
        }
    }
    return NULL;
}

void background_construct(p8_vram_t *memory, background_t *self) {
    p8_fixed_t row_values[32];
    int row_values_set[32];
    p8_fixed_t value = p8_fixed_from_int(0);
    random_walk_t *walk;
    const int_seq_t *pl = self != NULL && self->archetype->tile_choices != NULL ? self->archetype->tile_choices : &background_tile_choices_seq;
    int x, y;

    if (memory == NULL) {
        return;
    }

    for (x = 0; x < 32; x++) {
        row_values_set[x] = 0;
    }

    walk = random_walk_new(frame_zone(), P8_FIXED_LIT(0x00001999, 0.1), p8_fixed_from_int(0));

    for (y = 0; y <= 31; y++) {
        for (x = 0; x <= 31; x++) {
            p8_fixed_t prev = row_values_set[x] ? row_values[x] : p8_fixed_from_int(1);
            p8_fixed_t step =
                p8_fixed_sub(p8_fixed_mul(random_walk_next(walk), p8_fixed_from_int(8)), p8_fixed_from_int(4));
            p8_fixed_t roll;
            int tile;

            value = p8_fixed_add(step, p8_fixed_mul(p8_fixed_add(value, prev), P8_FIXED_LIT(0x00008000, 0.5)));
            row_values[x] = value;
            row_values_set[x] = 1;

            roll = p8_fixed_rnd_default();
            if (roll < P8_FIXED_LIT(0x00000106, 0.004)) {
                tile = 122 + background_to_int(p8_fixed_rnd(p8_fixed_from_int(5)));
            } else {
                p8_fixed_t idx_f = p8_fixed_flr(p8_fixed_add(value, p8_fixed_rnd(p8_fixed_from_int(4))));
                int idx = background_to_int(idx_f);

                tile = 0;
                int_seq_at(pl->items, pl->count, idx, &tile);
            }

            p8_mset(memory, x, y, tile);
        }
    }
}

typedef struct {
    int built;
    const p8_vram_t *mem;
    unsigned long map_version;
    unsigned char draw_palette[sizeof ((p8_vram_t *)0)->draw_palette];
} background_image_key_t;

static background_image_key_t g_background_image_key;

static void background_draw_map(p8_graphics_t *gfx, p8_vram_t *mem, void *context) {
    (void)context;
    p8_map(gfx, mem, 0, 0, 0, 0, P8_MAP_WIDTH, P8_MAP_HEIGHT);
}

static void background_draw_map_image(p8_graphics_t *gfx, p8_vram_t *mem) {
    background_image_key_t *key = &g_background_image_key;

    if (!key->built || key->mem != mem || key->map_version != mem->map_version
        || memcmp(key->draw_palette, mem->draw_palette, sizeof key->draw_palette) != 0) {
        p8_image_build(gfx, mem, IMAGE_SLOT_BACKGROUND, BACKGROUND_IMAGE_SIDE, BACKGROUND_IMAGE_SIDE,
                       background_draw_map, NULL);
        key->built = 1;
        key->mem = mem;
        key->map_version = mem->map_version;
        memcpy(key->draw_palette, mem->draw_palette, sizeof key->draw_palette);
    }
    p8_image_draw(gfx, mem, IMAGE_SLOT_BACKGROUND, 0, 0);
}

void background_draw(p8_graphics_t *gfx, p8_vram_t *mem, const background_t *self) {
    int xy;

    (void)self;

    if (gfx == NULL || mem == NULL) {
        return;
    }

    background_draw_map_image(gfx, mem);
    p8_fillp(gfx, 0xA5A5u);
    for (xy = 3; xy <= BACKGROUND_GRID_EDGE; xy += 32) {
        p8_rectfill(gfx, mem, xy, 0, xy, BACKGROUND_GRID_EDGE, 1);
        p8_rectfill(gfx, mem, 0, xy, BACKGROUND_GRID_EDGE, xy, 1);
    }
    p8_fillp_reset(gfx);
    p8_rect(gfx, mem, 0, 0, BACKGROUND_GRID_EDGE, BACKGROUND_GRID_EDGE, 5);
}

down_t background_left_press(background_t *self) {
    const entity_list_t *g_tag;
    int count;
    entity_t *made;
    down_t out;

    (void)self;

    g_tag = entities_by_tag(TAG_PROBE);
    count = (g_tag != NULL) ? g_tag->count : 0;
    if (count > 0) {
        assert(wormhole_ready() && "wormhole_init() must run before background_left_press builds a wormhole");
        made = (entity_t *)wormhole_new();
    } else {
        made = (entity_t *)probe_new(NULL);
    }
    out.stop = made != NULL;
    out.held = entity_id_link(made);
    return out;
}
