#include "game/help.h"

#include <assert.h>
#include <string.h>

#include "engine/dispatch_context.h"
#include "engine/entity_geom.h"
#include "engine/entity.h"
#include "game/message_box.h"
#include "game/particle.h"
#include "game/draw_functions.h"
#include "game/probe.h"
#include "engine/entity_class.h"
#include "engine/game_zone.h"
#include "p8/fixed.h"

static int g_help_screen_up = 0;

static void screen_start_game_placeholder(button_t *self) {
    (void)self;
}

static button_fn_t g_start_game_method = screen_start_game_placeholder;

button_fn_t help_start_game_field_value(void) {
    return g_start_game_method;
}

void help_set_start_game(button_fn_t fn) {
    g_start_game_method = (fn != NULL) ? fn : screen_start_game_placeholder;
}

static button_fn_t g_resume_game_method = NULL;

button_fn_t help_resume_game_field_value(void) {
    return g_resume_game_method;
}

void help_set_resume_game(button_fn_t fn) {
    g_resume_game_method = fn;
}

static int help_screen_hit_test_method(entity_t *self, mouse_pos_t mouse) {
    return entity_geom_contains_cursor(self, mouse);
}

void help_screen_construct(probe_t *self) {
    button_keys_t bs[DIFFICULTY_OPTIONS_COUNT];
    open_box_frame_t frame = {0};
    int i;

    if (self == NULL) {
        return;
    }
    for (i = 0; i < DIFFICULTY_OPTIONS_COUNT; i++) {
        const difficulty_options_entry_t *e = &difficulty_options_table[i];

        memset(&bs[i], 0, sizeof bs[i]);
        bs[i].layout_row_keys = layout_row_keys_of_row(e->layout_row);
        bs[i].name = e->name;
        bs[i].has_trade = 1;
        bs[i].trade = p8_fixed_from_int(e->trade);
        bs[i].has_type_bias = 1;
        bs[i].type_bias = e->type_bias;
        bs[i].has_on_click = 1;
        bs[i].on_click = help_start_game_field_value();
    }
    frame.has_fixed_pos = 1;
    frame.fixed_pos = self->archetype->fixed_pos;
    frame.clear_ephemerals_after = 1;
    frame.has_back_color = 1;
    frame.back_color = self->archetype->back_color;
    frame.has_row_step = 1;
    frame.row_step = self->archetype->row_step;
    frame.has_pos = entity_has_pos(&self->header);
    frame.pos = self->header.pos;
    message_box_open_box(&frame, bs, DIFFICULTY_OPTIONS_COUNT);

    /* After the difficulty rows: the pad focus opens on the first button registered, forgiving. */
    if (help_resume_game_field_value() != NULL) {
        button_keys_t resume = {0};

        resume.layout_row_keys = layout_row_keys_of_row(&resume_layout_row);
        resume.name = "resume";
        resume.has_on_click = 1;
        resume.on_click = help_resume_game_field_value();
        frame.fixed_pos = vec_sub(frame.fixed_pos, frame.row_step);
        message_box_open_box(&frame, &resume, 1);
    }
}

enum { HELP_SCREEN_ROWS_CAP = 4 };

void help_screen_draw(p8_graphics_t *gfx, p8_vram_t *mem, probe_t *self, vec_t p) {
    (void)p;

    /* The seed tick, result unused: without it every game started from boot gets the same galaxy. */
    (void)p8_fixed_rnd_default();
    emitter_update(gfx, mem, (entity_t *)self);

    if (gfx != NULL && mem != NULL) {
        const data_row_t *items[HELP_SCREEN_ROWS_CAP];
        int n = 0;
        int i;

        for (i = 0; self != NULL && self->archetype->rows != NULL && i < self->archetype->rows->count; i++) {
            assert(n < HELP_SCREEN_ROWS_CAP && "help_screen_draw: the help screen's rows hold four articles at most");
            items[n++] = self->archetype->rows->items[i].row;
        }
        draw_functions_draw_rows_default(gfx, mem, items, n);
    }
}

void help_screen_draw_method(entity_t *self, vec_t screen_pos) {
    help_screen_draw(dispatch_context_gfx(), dispatch_context_mem(), (probe_t *)self, screen_pos);
}

static const entity_class_t help_screen_class_def = {
    .draw = help_screen_draw_method,
    .draw_overlay = targeting_tooltip_method,
    .hit_test = help_screen_hit_test_method,
    .drag = targeting_track_method,
    .drop = targeting_confirm_method,
    .spawn_particle = probe_spawn_particle,
    .release = probe_release_method,
    .tags = TAG(TAG_PROBE),
    .lineage = LINEAGE(LINEAGE_PROBE),
};

static probe_t *screen_create(void) {
    probe_t *self;

    assert(g_help_screen_up && "help_init() must run first");
    self = (probe_t *)allocator_alloc(game_zone(), sizeof *self);
    probe_init_instance(self, &help_screen_class_def, &help_screen_archetype);
    self->header.has |= ENTITY_HAS_HIT_BOX;
    self->header.hit_box = self->archetype->hit_box;
    self->header.has |= ENTITY_HAS_IS_HOVERED;
    self->header.is_hovered = 0;
    help_screen_construct(self);
    emitter_init_in((entity_t *)self, PARTICLE_SLOT_HELP_LIST);
    entity_construct(&self->header);
    return self;
}

void help_init(void) {
    assert(probe_ready() && "help_init() runs after probe_init()");

    help_shutdown();

    g_help_screen_up = 1;

    help_set_start_game(NULL);
    help_set_resume_game(NULL);
}

void help_shutdown(void) {
    g_help_screen_up = 0;
    help_set_start_game(NULL);
    help_set_resume_game(NULL);
}

int help_screen_ready(void) {
    return g_help_screen_up;
}

const entity_class_t *help_screen_class(void) {
    return &help_screen_class_def;
}

probe_t *help_screen_new(void) {
    return screen_create();
}
