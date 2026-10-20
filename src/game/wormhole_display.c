#include "game/wormhole_display.h"

#include <assert.h>
#include <stddef.h>

#include "engine/dispatch_context.h"
#include "p8/fixed.h"
#include "engine/entity_id.h"
#include "game/wormhole.h"

void wormhole_draw(p8_graphics_t *graphics, p8_vram_t *memory, wormhole_t *self, vec_t screen_pos) {
    int screen_x;
    int screen_y;

    if (self == NULL) {
        return;
    }

    screen_x = p8_fixed_flr_int(screen_pos.x);
    screen_y = p8_fixed_flr_int(screen_pos.y);

    if (self->header.state == ENTITY_STATE_PLACING) {
        const entity_t *anchor = entity_get(self->anchor);

        if (targeting_target(&self->targeting) == NULL) {
            p8_fixed_t oi = self->radius;
            if (graphics != NULL && memory != NULL) {
                p8_circ(graphics, memory, screen_x, screen_y, p8_fixed_flr_int(oi), 1);
            }
        }

        /* Outside the NULL-target guard on purpose: the line runs whether or not a target was found.
         * No anchor (the cart would index nil): no line, not one to world (0,0). */
        if (graphics != NULL && memory != NULL) {
            if (anchor != NULL) {
                vec_t cp = entity_pos(anchor);
                p8_line(graphics, memory, p8_fixed_flr_int(cp.x), p8_fixed_flr_int(cp.y), screen_x, screen_y, 1);
            }
            p8_spr_default(graphics, memory, 7, screen_x - 4, screen_y - 4);
        }
    } else {
        p8_fixed_t t = self->header.state_time;
        p8_fixed_t oi = self->radius;
        int has_color_ramp = self->archetype->color_ramp != NULL;
        int dt;

        for (dt = 0; dt <= 8; dt += 2) {
            if (t > p8_fixed_from_int(dt)) {
                p8_fixed_t sr = wormhole_reveal_radius(self, p8_fixed_sub(t, p8_fixed_from_int(dt)));
                int color = 0;

                if (has_color_ramp) {
                    p8_fixed_t ratio = p8_fixed_mul(p8_fixed_div(sr, oi), p8_fixed_from_int(8));
                    int idx = p8_fixed_flr_int(p8_fixed_flr(ratio));

                    int_seq_at(self->archetype->color_ramp->items, self->archetype->color_ramp->count, idx, &color);
                }

                if (graphics != NULL && memory != NULL) {
                    p8_circ(graphics, memory, screen_x, screen_y, p8_fixed_flr_int(sr), color);
                }
            }
        }
    }
}

void wormhole_draw_method(entity_t *self, vec_t screen_pos) {
    wormhole_draw(dispatch_context_gfx(), dispatch_context_mem(), (wormhole_t *)self, screen_pos);
}
