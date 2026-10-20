#ifndef GAME_MESSAGE_BOX_H
#define GAME_MESSAGE_BOX_H

#include "engine/entity_class.h"
#include "game/game_data.h"
#include "p8/gfx.h"
#include "p8/vram.h"
#include "game/ui_button.h"
#include "game/layout.h"
#include "engine/vec.h"

typedef struct message_box message_box_t;

typedef int (*message_box_keep_open_fn_t)(message_box_t *self);

enum { MESSAGE_BOX_ITEMS_CAP = SCORE_ROWS_COUNT, MESSAGE_BOX_ARGUMENTS_CAP = 5, MESSAGE_BOX_TEXT_CAP = 16 };

typedef enum {
    MESSAGE_BOX_ARGUMENT_NUMBER,
    MESSAGE_BOX_ARGUMENT_FALSE,
    MESSAGE_BOX_ARGUMENT_NIL,
    MESSAGE_BOX_ARGUMENT_TEXT
} message_box_argument_kind_t;

typedef struct {
    message_box_argument_kind_t kind;
    p8_fixed_t number;
} message_box_argument_t;

typedef const data_row_t *(*message_box_row_fn_t)(void);

typedef enum {
    MESSAGE_BOX_ARTICLE_SCALARS,
    MESSAGE_BOX_ARTICLE_LAYOUT_ROW,
    MESSAGE_BOX_ARTICLE_SHARED_ROW
} message_box_article_kind_t;

typedef struct {
    message_box_article_kind_t kind;
    draw_function_t draw_function;
    union {
        struct {
            int count;
            message_box_argument_t arguments[MESSAGE_BOX_ARGUMENTS_CAP];
            char text[MESSAGE_BOX_TEXT_CAP + 1];
        } scalars;
        struct {
            layout_row_t row;
            int has_pos;
            vec_t pos;
            p8_fixed_t align;
        } layout;
        message_box_row_fn_t row_of;
    } as;
} message_box_article_t;

struct message_box {
    entity_t header;
    const allocator_t *slot;
    const message_box_archetype_t *archetype;
    p8_fixed_t duration_scale;
    message_box_keep_open_fn_t keep_open;
    message_box_article_t articles[MESSAGE_BOX_ITEMS_CAP];
    int articles_count;
};

message_box_t *shown_message_box(void);
void message_box_set_shown(message_box_t *mh);
/* Closes the shown box as its timeout does: marked dead, no box shown. Does nothing when none is shown. */
void message_box_close_shown(void);

void message_box_init(void);

void message_box_shutdown(void);

int message_box_ready(void);

typedef struct {
    int has_duration_scale;
    p8_fixed_t duration_scale;
    message_box_keep_open_fn_t keep_open;
} message_box_keys_t;

message_box_t *message_box_new(const message_box_keys_t *keys);

void message_box_add_row(message_box_t *self, const data_row_t *row);
void message_box_add_layout_row(message_box_t *self, const layout_row_keys_t *keys, vec_t pos, int has_pos, p8_fixed_t align);
/* The row is read through `row_of` at each draw (NULL draws nothing): it must live in static storage. */
void message_box_add_shared_row(message_box_t *self, message_box_row_fn_t row_of);

void message_box_construct(message_box_t *self);

void message_box_draw_overlay(p8_graphics_t *gfx, p8_vram_t *mem, message_box_t *self);

void toast_init(void);

enum { TOAST_TEXT_CAP = 16 };

typedef struct {
    entity_t header;
    const allocator_t *slot;
    entity_id_t follow;
    char text[TOAST_TEXT_CAP + 1];
    const toast_archetype_t *archetype;
    p8_fixed_t fade;
    p8_fixed_t color;
} toast_t;

void toast_shutdown(void);

int toast_ready(void);

typedef struct {
    int has_pos;
    vec_t pos;
    entity_id_t source;
    int has_text;
    char text[TOAST_TEXT_CAP + 1];
    p8_fixed_t color;
} toast_keys_t;

void toast_keys_set_text(toast_keys_t *keys, const char *text);

toast_t *toast_new(const toast_keys_t *keys);

void toast_construct(toast_t *self);
void toast_update(toast_t *self);

void toast_draw(const p8_graphics_t *gfx, p8_vram_t *mem, const toast_t *self, vec_t p);

enum { OPEN_BOX_DESCRIPTION_CAP = 17 };

typedef struct {
    int has_description;
    char description[OPEN_BOX_DESCRIPTION_CAP + 1];
    int has_fixed_pos;
    vec_t fixed_pos;
    int clear_ephemerals_after;
    int has_back_color;
    p8_fixed_t back_color;
    int has_row_step;
    vec_t row_step;
    int has_pos;
    vec_t pos;
} open_box_frame_t;

void open_box_frame_set_description(open_box_frame_t *frame, const char *description);

void message_box_open_box(const open_box_frame_t *frame, const button_keys_t *buttons, int button_count);

#endif
