#include "game/message_box.h"
#include "engine/entity_id.h"

#include <assert.h>
#include <stddef.h>
#include <string.h>

#include "engine/dispatch_context.h"
#include "p8/fixed.h"
#include "game/hud.h"
#include "engine/entity.h"
#include "engine/entity_registry.h"
#include "game/layout.h"
#include "game/draw_functions.h"
#include "game/scroll.h"
#include "game/ui.h"
#include "game/ui_button.h"
#include "game/ui_zone.h"

static int g_message_box_up = 0;
static int g_toast_up = 0;
static entity_id_t g_shown_message_box;

message_box_t *shown_message_box(void) {
    return (message_box_t *)entity_get(g_shown_message_box);
}

void message_box_set_shown(message_box_t *mh) {
    g_shown_message_box = entity_id_link(mh != NULL ? &mh->header : NULL);
}

void message_box_close_shown(void) {
    message_box_t *shown = shown_message_box();
    if (shown != NULL) {
        shown->header.is_dead = 1;
        message_box_set_shown(NULL);
    }
}

void message_box_construct(message_box_t *self) {
    message_box_t *previous;

    if (self == NULL) {
        return;
    }

    previous = shown_message_box();
    if (previous != NULL) {
        previous->header.is_dead = 1;
    }
    message_box_set_shown(self);
}

static void message_box_draw_overlay_method(entity_t *self, vec_t screen_pos) {
    (void)screen_pos;
    message_box_draw_overlay(dispatch_context_gfx(), dispatch_context_mem(), (message_box_t *)self);
}

static void message_box_draw_article(p8_graphics_t *gfx, p8_vram_t *mem, const message_box_article_t *article) {
    data_item_t items[MESSAGE_BOX_ARGUMENTS_CAP];
    data_row_t row = {0};
    int i;

    if (article->kind == MESSAGE_BOX_ARTICLE_SHARED_ROW) {
        const data_row_t *shared = article->as.row_of();

        if (shared != NULL) {
            draw_functions_draw_row_text_image(gfx, mem, shared);
        }
        return;
    }
    if (article->kind == MESSAGE_BOX_ARTICLE_LAYOUT_ROW) {
        vec_t p = article->as.layout.has_pos ? article->as.layout.pos : vec_make(p8_fixed_from_int(0), p8_fixed_from_int(0));
        layout_row_draw_at_text_image(gfx, mem, &article->as.layout.row, p, article->as.layout.align);
        return;
    }
    for (i = 0; i < article->as.scalars.count; i++) {
        const message_box_argument_t *argument = &article->as.scalars.arguments[i];

        switch (argument->kind) {
        case MESSAGE_BOX_ARGUMENT_TEXT:
            items[i] = (data_item_t){.text = article->as.scalars.text};
            break;
        case MESSAGE_BOX_ARGUMENT_FALSE:
            items[i] = (data_item_t){.is_false = 1};
            break;
        case MESSAGE_BOX_ARGUMENT_NIL:
            items[i] = (data_item_t){.is_nil = 1};
            break;
        case MESSAGE_BOX_ARGUMENT_NUMBER:
        default:
            items[i] = (data_item_t){.is_fixed = 1, .fixed = argument->number};
            break;
        }
    }
    row.count = article->as.scalars.count;
    row.items = items;
    row.draw_function = article->draw_function;
    draw_functions_draw_row_text_image(gfx, mem, &row);
}

void message_box_draw_overlay(p8_graphics_t *gfx, p8_vram_t *mem, message_box_t *self) {
    p8_fixed_t self_t, rh, t, nc, t3;

    if (self == NULL) {
        return;
    }

    self_t = self->header.state_time;
    rh = self->duration_scale;
    t = p8_fixed_sub(p8_fixed_div(self_t, rh), p8_fixed_from_int(10));

    if (t >= 0) {
        if (self->keep_open != NULL && self->keep_open(self)) {
            self->header.state_time = p8_fixed_mul(rh, p8_fixed_from_int(10));
            t = p8_fixed_from_int(0);
        }
    }

    t3 = p8_fixed_mul(p8_fixed_mul(t, t), t);
    nc = p8_fixed_div(p8_fixed_neg(t3), p8_fixed_from_int(9));

    if (gfx != NULL && mem != NULL) {
        int i;

        for (i = 0; i < self->articles_count; i++) {
            p8_camera(gfx, p8_fixed_flr_int(nc), 0);
            nc = p8_fixed_neg(nc);
            message_box_draw_article(gfx, mem, &self->articles[i]);
        }
        p8_camera_reset(gfx);
    }

    if (t > P8_FIXED_LIT(0x000a8000, 10.5)) {
        self->header.is_dead = 1;
        message_box_set_shown(NULL);
        assert(shown_message_box() == NULL && "message_box_draw_overlay: the closing box marks itself dead and clears the shown message box");
    }
}

static void message_box_release(entity_t *self) {
    arena_release(((message_box_t *)self)->slot);
}

static const entity_class_t message_box_class_def = {
    .draw_overlay = message_box_draw_overlay_method,
    .tags = TAG(TAG_MESSAGE_BOX),
    .release = message_box_release,
};

static message_box_article_t *message_box_next_article(message_box_t *self) {
    assert(self->articles_count < MESSAGE_BOX_ITEMS_CAP &&
           "message_box_next_article: the game builds two boxes, score_panel_new_year's (3 articles at most) and score_panel_update's (one per score row): a third needs more");
    return &self->articles[self->articles_count++];
}

void message_box_add_row(message_box_t *self, const data_row_t *row) {
    message_box_article_t *article;
    int i;

    assert(row->draw_function != DRAW_FUNCTION_DRAW_ROW && "message_box_add_row: a layout row goes through message_box_add_layout_row");
    assert(row->count <= MESSAGE_BOX_ARGUMENTS_CAP && "message_box_add_row: a draw function reads five arguments at most -- stop, do not widen");
    article = message_box_next_article(self);
    article->kind = MESSAGE_BOX_ARTICLE_SCALARS;
    article->draw_function = row->draw_function;
    article->as.scalars.count = row->count;
    article->as.scalars.text[0] = '\0';
    for (i = 0; i < row->count; i++) {
        const data_item_t *item = &row->items[i];
        message_box_argument_t *argument = &article->as.scalars.arguments[i];

        assert(item->row == NULL && "message_box_add_row: a draw function reads no nested row");
        argument->number = p8_fixed_from_int(0);
        if (item->text != NULL) {
            assert(i == 0 && "message_box_add_row: only print_outlined's first argument is read as text -- stop, do not widen");
            assert(strlen(item->text) <= MESSAGE_BOX_TEXT_CAP &&
                   "message_box_add_row: the longest article text is \"2 years remain\" -- stop, do not widen");
            strcpy(article->as.scalars.text, item->text);
            argument->kind = MESSAGE_BOX_ARGUMENT_TEXT;
        } else if (item->is_false) {
            argument->kind = MESSAGE_BOX_ARGUMENT_FALSE;
        } else if (item->is_nil) {
            argument->kind = MESSAGE_BOX_ARGUMENT_NIL;
        } else {
            argument->kind = MESSAGE_BOX_ARGUMENT_NUMBER;
            argument->number = item->is_fixed ? item->fixed : p8_fixed_from_int(item->number);
        }
    }
}

void message_box_add_layout_row(message_box_t *self, const layout_row_keys_t *keys, vec_t pos, int has_pos, p8_fixed_t align) {
    message_box_article_t *article = message_box_next_article(self);

    article->kind = MESSAGE_BOX_ARTICLE_LAYOUT_ROW;
    article->draw_function = DRAW_FUNCTION_DRAW_ROW;
    layout_row_init(&article->as.layout.row, self->slot, keys);
    article->as.layout.has_pos = has_pos;
    article->as.layout.pos = has_pos ? pos : vec_make(p8_fixed_from_int(0), p8_fixed_from_int(0));
    article->as.layout.align = align;
}

void message_box_add_shared_row(message_box_t *self, message_box_row_fn_t row_of) {
    message_box_article_t *article = message_box_next_article(self);

    assert(row_of != NULL && "message_box_add_shared_row: a shared row is read through its function at each draw");
    article->kind = MESSAGE_BOX_ARTICLE_SHARED_ROW;
    article->draw_function = DRAW_FUNCTION_NONE;
    article->as.row_of = row_of;
}

static message_box_t *message_box_create(const message_box_keys_t *keys) {
    const allocator_t *slot = ui_zone_slot(SLOT_KIND_MESSAGE_BOX);
    message_box_t *self = (message_box_t *)allocator_alloc(slot, sizeof *self);

    self->slot = slot;
    entity_init_instance(&self->header, &message_box_class_def, message_box_archetype.state, message_box_archetype.state_time);
    self->archetype = &message_box_archetype;
    entity_registry_set_layer(&self->header, self->archetype->layer);
    self->duration_scale = self->archetype->duration_scale;
    self->keep_open = NULL;
    self->header.has |= ENTITY_HAS_IS_DEAD;
    self->header.is_dead = 0;
    self->articles_count = 0;
    if (keys != NULL) {
        if (keys->has_duration_scale) {
            self->duration_scale = keys->duration_scale;
        }
        self->keep_open = keys->keep_open;
    }
    entity_construct(&self->header);
    message_box_construct(self);
    return self;
}

void message_box_init(void) {
    message_box_shutdown();
    g_message_box_up = 1;
}

void message_box_shutdown(void) {
    g_message_box_up = 0;
    message_box_set_shown(NULL);
}

int message_box_ready(void) {
    return g_message_box_up;
}

message_box_t *message_box_new(const message_box_keys_t *keys) {
    return message_box_create(keys);
}

static vec_t toast_pos_or_zero(const entity_t *e) {
    return e != NULL ? entity_pos(e) : vec_make(p8_fixed_from_int(0), p8_fixed_from_int(0));
}

/* The cart's toast shares its source's vector until its first update, drawn meanwhile (sometimes for several frames): a copy alone would miss a bob in between. */
static void toast_follow_source(toast_t *self) {
    const entity_t *source = entity_get(self->follow);

    if (source != NULL && entity_has_pos(source)) {
        entity_set_pos(&self->header, source->pos);
    }
}

void toast_construct(toast_t *self) {
    if (self == NULL) {
        return;
    }

    if (!entity_has_pos(&self->header)) {
        const mouse_t *je = main_mouse_typed();
        const camera_t *mo = main_camera_typed();
        vec_t main_mouse_pos = toast_pos_or_zero(je != NULL ? &je->header : NULL);
        vec_t main_camera_pos = toast_pos_or_zero(mo != NULL ? &mo->header : NULL);
        vec_t offset = vec_make(p8_fixed_from_int(0), p8_fixed_from_int(5));
        entity_set_pos(&self->header, vec_sub(vec_add(main_mouse_pos, main_camera_pos), offset));
    }
}

void toast_update(toast_t *self) {
    p8_fixed_t new_l;

    if (self == NULL) {
        return;
    }

    toast_follow_source(self);
    self->follow = ENTITY_ID_NONE;
    entity_set_pos(&self->header, vec_add(entity_pos(&self->header), self->archetype->velocity));

    new_l = p8_fixed_add(self->fade, P8_FIXED_LIT(0x00001999, 0.1));
    self->fade = new_l;

    self->header.is_dead = new_l >= p8_fixed_from_int(5);
}

static void toast_update_method(entity_t *self, p8_fixed_t t) {
    (void)t;
    toast_update((toast_t *)self);
}

void toast_draw(const p8_graphics_t *gfx, p8_vram_t *mem, const toast_t *self, vec_t p) {
    int color;
    p8_fixed_t a_center = P8_FIXED_LIT(0x00008000, 0.5);

    if (self == NULL || gfx == NULL || mem == NULL) {
        return;
    }

    hud_select_palette_block(mem, self->fade);

    color = p8_fixed_flr_int(self->color);

    hud_print_outlined(gfx, mem, self->text, p.x, p.y, color, a_center);

    hud_select_palette_block_default(mem);
}

static void toast_draw_method(entity_t *self, vec_t screen_pos) {
    (void)screen_pos;
    toast_follow_source((toast_t *)self);
    toast_draw(dispatch_context_gfx(), dispatch_context_mem(), (const toast_t *)self, entity_pos(self));
}

static void toast_release(entity_t *self) {
    arena_release(((toast_t *)self)->slot);
}

static const entity_class_t toast_class_def = {
    .draw = toast_draw_method,
    .states = {[ENTITY_STATE_NORMAL] = toast_update_method},
    .release = toast_release,
};

static toast_t *toast_create(const toast_keys_t *keys) {
    const allocator_t *slot = ui_zone_slot(SLOT_KIND_TOAST);
    toast_t *self = (toast_t *)allocator_alloc(slot, sizeof *self);

    self->slot = slot;

    entity_init_instance(&self->header, &toast_class_def, toast_archetype.state, toast_archetype.state_time);
    self->archetype = &toast_archetype;
    entity_registry_set_layer(&self->header, self->archetype->layer);
    self->fade = self->archetype->fade;
    self->follow = ENTITY_ID_NONE;
    self->header.has |= ENTITY_HAS_IS_DEAD;
    self->header.is_dead = 0;
    self->text[0] = '\0';
    self->color = p8_fixed_from_int(0);
    if (keys != NULL) {
        const entity_t *source = entity_get(keys->source);

        if (source != NULL && entity_has_pos(source)) {
            entity_set_pos(&self->header, source->pos);
            self->follow = keys->source;
        } else if (keys->has_pos) {
            entity_set_pos(&self->header, keys->pos);
        }
        if (keys->has_text) {
            strcpy(self->text, keys->text);
        }
        self->color = keys->color;
    }
    toast_construct(self);
    entity_construct(&self->header);
    return self;
}

void toast_init(void) {
    toast_shutdown();
    g_toast_up = 1;
}

void toast_shutdown(void) {
    g_toast_up = 0;
}

int toast_ready(void) {
    return g_toast_up;
}

void toast_keys_set_text(toast_keys_t *keys, const char *text) {
    assert(strlen(text) <= TOAST_TEXT_CAP && "toast_keys_set_text: a toast's text is a price or \"-no money-\": a longer one needs a bigger text array and toast slot");
    strcpy(keys->text, text);
    keys->has_text = 1;
}

toast_t *toast_new(const toast_keys_t *keys) {
    return toast_create(keys);
}

typedef struct {
    vec_t button_pos;
    vec_t pos;
} open_box_ctx_t;

void open_box_frame_set_description(open_box_frame_t *frame, const char *description) {
    assert(strlen(description) <= OPEN_BOX_DESCRIPTION_CAP &&
           "open_box_frame_set_description: the longest is the tech tree's \"level 4 (-12/-12)\" -- stop, measure before widening");
    strcpy(frame->description, description);
    frame->has_description = 1;
}

static button_t *open_box_step(open_box_ctx_t *ctx, const open_box_frame_t *frame, const button_keys_t *row) {
    button_keys_t b = *row;
    vec_t gq;

    b.has_pos = 1;
    b.pos = ctx->pos;
    b.has_slide_target = 1;
    b.slide_target = ctx->button_pos;

    if (frame->has_back_color) {
        b.layer = frame->back_color;
        b.has_layer = 1;
    }

    gq = frame->has_row_step ? frame->row_step : vec_make(p8_fixed_from_int(0), p8_fixed_from_int(0));
    ctx->button_pos = vec_add(ctx->button_pos, gq);

    return button_new(&b);
}

static void open_box_add(entity_id_list_t *le, int has_list, button_t *button) {
    if (has_list) {
        entity_id_list_add(le, entity_id_link(&button->header));
    }
}

void message_box_open_box(const open_box_frame_t *frame, const button_keys_t *buttons, int button_count) {
    vec_t box_pos;
    open_box_ctx_t step_context;
    entity_id_list_t ephemerals = {0};
    ui_t *dispatcher;
    button_keys_t description_row = {0};
    int index;

    if (frame == NULL) {
        return;
    }

    if (frame->has_description) {
        layout_row_keys_append(&description_row.layout_row_keys, layout_row_cell_string(frame->description));
        description_row.layout_row_keys.has_color = 1;
        description_row.layout_row_keys.color = p8_fixed_from_int(13);
    }

    if (frame->has_fixed_pos) {
        box_pos = frame->fixed_pos;
    } else if (frame->has_pos) {
        box_pos = frame->pos;
    } else {
        box_pos = vec_make(p8_fixed_from_int(0), p8_fixed_from_int(0));
    }

    step_context.button_pos = vec_add(box_pos, vec_make(p8_fixed_from_int(0), p8_fixed_from_int(0)));
    step_context.pos = box_pos;

    dispatcher = main_ui();
    ui_clear_ephemerals(dispatcher);

    if (dispatcher != NULL) {
        ephemerals = ui_ephemerals_new();
    }
    if (frame->has_description) {
        open_box_add(&ephemerals, dispatcher != NULL, open_box_step(&step_context, frame, &description_row));
    }
    for (index = 0; index < button_count; index++) {
        open_box_add(&ephemerals, dispatcher != NULL, open_box_step(&step_context, frame, &buttons[index]));
    }
    if (dispatcher != NULL) {
        ui_set_ephemerals(dispatcher, ephemerals);
    }

    if (frame->clear_ephemerals_after && dispatcher != NULL) {
        ui_set_ephemerals(dispatcher, ui_ephemerals_new());
    }
}
