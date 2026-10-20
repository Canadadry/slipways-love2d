#include "game/slipway_display.h"

#include <assert.h>
#include <stddef.h>

#include "engine/dispatch_context.h"
#include "engine/entity_geom.h"
#include "engine/entity_id.h"
#include "game/planet.h"
#include "game/slipway.h"
#include "game/visual_random.h"
#include "engine/vec.h"
#include "game/particle.h"
#include "game/particle_zone.h"

typedef char transit_dot_fills_its_slot_t[ZONE_BYTES(sizeof(transit_dot_t)) == TRANSIT_DOTS_SLOT ? 1 : -1];

static int to_int(p8_fixed_t x) {
    return p8_fixed_flr_int(x);
}

void slipway_spawn_transit_dot(slipway_t *self, const entity_t *f, const entity_t *t) {
    const char *t_id;

    if (self == NULL) {
        return;
    }
    /* The visual draw must stay before the served planets test, or the visual stream desynchronises. */
    if (visual_random_draw_one() >= P8_FIXED_LIT(0x0000147a, 0.08)) {
        return;
    }

    /* The mouse point slipway_drag hangs the end on is not a planet. */
    t_id = t != NULL && planet_is_family(t) ? planet_id_or_null((const planet_t *)t) : NULL;
    if (slipway_served_planets_has(&self->served_planets, t_id)) {
        vec_t f_pos = slipway_end_pos(f);
        vec_t t_pos = slipway_end_pos(t);
        vec_t d = vec_sub(t_pos, f_pos);
        vec_t v = vec_normalize(d);
        transit_dot_t *particle = (transit_dot_t *)allocator_alloc(particle_zone(PARTICLE_SLOT_TRANSIT_DOT), sizeof *particle);

        particle->pos = vec_add(f_pos, vec_perpendicular(v));
        particle->velocity = v;
        particle->life = vec_length(d);

        pointer_list_add(&self->transit_dots, particle);
        particle_zone_record_elements(PARTICLE_SLOT_TRANSIT_DOT, self->transit_dots.count);
    }
}

slipway_line_ends_t slipway_line_ends(const slipway_t *self) {
    p8_fixed_t t = self->header.state_time;
    slipway_line_ends_t ends;
    ends.start = vec_add(slipway_end_pos(slipway_start(self)),
                         vec_from_polar(P8_FIXED_LIT(0x00018000, 1.5), p8_fixed_div(t, p8_fixed_from_int(126))));
    ends.end = vec_add(slipway_end_pos(slipway_end(self)),
                       vec_from_polar(P8_FIXED_LIT(0x00018000, 1.5), p8_fixed_div(t, p8_fixed_from_int(176))));
    return ends;
}

static int floored_far_from_the_wrap(p8_fixed_t p) {
    int floored = p8_fixed_flr_int(p);
    assert(floored >= -32765 && floored <= 32765 &&
           "int adds of the offsets match p8_fixed_add only away from its +-32768 wrap");
    return floored;
}

void slipway_draw_line(const p8_graphics_t *gfx, p8_vram_t *mem, slipway_line_ends_t ends, int l, int h, int s, int c) {
    int x1;
    int y1;
    int x2;
    int y2;
    int dx;

    assert(s > 0 && "slipway_draw_line only ascends: a zero step hangs, a negative one draws nothing");
    assert(gfx != NULL && mem != NULL && "p8_lines dereferences the graphics state and memory: neither may be NULL here");

    x1 = floored_far_from_the_wrap(ends.start.x);
    y1 = floored_far_from_the_wrap(ends.start.y);
    x2 = floored_far_from_the_wrap(ends.end.x);
    y2 = floored_far_from_the_wrap(ends.end.y);

    p8_segment_t bundle[4];
    int count = 0;
    for (dx = l; dx <= h; dx += s) {
        int dy;
        for (dy = l; dy <= h; dy += s) {
            p8_segment_t *segment = &bundle[count++];
            segment->x1 = x1 + dx;
            segment->y1 = y1 + dy;
            segment->x2 = x2 + dx;
            segment->y2 = y2 + dy;
            if (count == 4) {
                p8_lines(gfx, mem, bundle, count, c);
                count = 0;
            }
        }
    }
    if (count > 0) {
        p8_lines(gfx, mem, bundle, count, c);
    }
}

void slipway_draw(const p8_graphics_t *graphics, p8_vram_t *memory, slipway_t *self) {
    entity_t *start_node;
    entity_t *end_node;

    if (self == NULL) {
        return;
    }

    start_node = slipway_start(self);
    end_node = slipway_end(self);

    if (entity_geom_is_on_screen(start_node) || entity_geom_is_on_screen(end_node)) {
        p8_fixed_t fade_in_step = self->fade_in_step;
        int fade_in_step_i = to_int(fade_in_step);
        p8_fixed_t fade_color = p8_fixed_from_int(0);
        int target_or_targeted_by;
        int infraspace_truthy;
        p8_fixed_t line_color;
        p8_fixed_t border_color;
        p8_fixed_t dot_color;

        assert(fade_in_step_i >= 1 && "trail colors are 1-based: a fade-in step of 0 would silently draw the slipway black");

        if (self->archetype->trail_colors != NULL) {
            int entry;
            if (int_seq_at(self->archetype->trail_colors->items, self->archetype->trail_colors->count, fade_in_step_i, &entry)) {
                fade_color = p8_fixed_from_int(entry);
            }
        }

        if (fade_in_step < p8_fixed_from_int(20)) {
            self->fade_in_step = p8_fixed_add(fade_in_step, p8_fixed_from_int(1));
        }

        target_or_targeted_by = targeting_target(&self->targeting) != NULL || entity_get(self->targeted_by) != NULL;
        infraspace_truthy = entity_has(&self->header, ENTITY_HAS_INFRASPACE);

        line_color = target_or_targeted_by ? p8_fixed_from_int(2) : (infraspace_truthy ? p8_fixed_from_int(0) : fade_color);
        border_color = infraspace_truthy ? fade_color : p8_fixed_from_int(0);
        dot_color = infraspace_truthy ? p8_fixed_from_int(5) : p8_fixed_from_int(13);

        slipway_line_ends_t ends = slipway_line_ends(self);
        slipway_draw_line(graphics, memory, ends, -1, 2, 3, to_int(border_color));
        slipway_draw_line(graphics, memory, ends, 0, 1, 1, to_int(line_color));

        {
            pointer_list_t *ts = &self->transit_dots;
            int i;
            for (i = 0; i < ts->count; i++) {
                transit_dot_t *t = ts->items[i];
                vec_t new_pos;
                p8_fixed_t new_l;

                if (t == NULL) {
                    continue;
                }

                new_pos = vec_add(t->pos, t->velocity);
                t->pos = new_pos;

                new_l = p8_fixed_sub(t->life, p8_fixed_from_int(1));
                t->life = new_l;

                /* No `else`: an expired particle is still drawn once more at its final position. */
                if (new_l <= p8_fixed_from_int(0)) {
                    /* Removing an element during this walk skips the next one; it is visited next frame. Compensating the index would change update timing and every replay. */
                    pointer_list_del(ts, t);
                    allocator_release(particle_zone(PARTICLE_SLOT_TRANSIT_DOT), (unsigned char *)t);
                }

                if (!particle_pixels_hidden()) {
                    int x = to_int(new_pos.x);
                    int y = to_int(new_pos.y);
                    p8_rectfill(graphics, memory, x, y, x + 1, y + 1, 0);
                    p8_pset(graphics, memory, x, y, to_int(dot_color));
                }
            }
        }

        slipway_spawn_transit_dot(self, start_node, end_node);
        slipway_spawn_transit_dot(self, end_node, start_node);
    }

    self->targeted_by = ENTITY_ID_NONE;
}

void slipway_draw_method(entity_t *self, vec_t screen_pos) {
    (void)screen_pos;
    slipway_draw(dispatch_context_gfx(), dispatch_context_mem(), (slipway_t *)self);
}
