/* The button `button_t`, read and written on its members and driven through
 * the real opening of a button list, cart_start_game, the tech tree screen,
 * the galaxy menu, update_entities, draw_layers and the on_click call, on
 * title screens and games built directly: no events file. */

#define _POSIX_C_SOURCE 200809L

#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>

#include "p8/sprite_sheet.h"
#include "engine/entity_id.h"
#include "engine/bootstrap.h"
#include "game/cart.h"
#include "game/data_init.h"
#include "engine/dispatch_context.h"
#include "game/economy_score.h"
#include "engine/entity.h"
#include "engine/entity_registry.h"
#include "p8/fixed.h"
#include "game/main_economy.h"
#include "engine/geom.h"
#include "p8/gfx.h"
#include "game/help.h"
#include "p8/input.h"
#include "game/layout.h"
#include "game/map_tables.h"
#include "p8/vram.h"
#include "game/menu.h"
#include "game/message_box.h"
#include "game/planet.h"
#include "game/planet_display.h"
#include "game/research.h"
#include "game/research_display.h"
#include "game/score_panel.h"
#include "game/slipway.h"
#include "p8/str.h"
#include "engine/entity_class.h"
#include "game/tech_gate.h"
#include "game/ui.h"
#include "game/ui_button.h"
#include "engine/vec.h"
#include "engine/frame_zone.h"
#include "engine/game_zone.h"
#include "game/ui_zone.h"
#include "devtools/largest_slots.h"
#include "devtools/state_format.h"
#include "heap_allocator.h"
#include "game/button_image.h"
#include "game/hud.h"
#include "game/image_slot.h"
#include "p8/draw_backend.h"

static mouse_pos_t mouse_world_only(vec_t world) {
    return (mouse_pos_t){.world = world, .has = MOUSE_POS_HAS_WORLD};
}

static const vec_t *slide_target_ref(const button_t *b) {
    return (b->has & BUTTON_HAS_SLIDE_TARGET) ? &b->slide_target : NULL;
}

static const vec_t *pos_ref(const entity_t *e) {
    return entity_has_pos(e) ? &e->pos : NULL;
}

static const vec_t *map_pos_ref(const entity_t *e) {
    return entity_has_map_pos(e) ? &e->map_pos : NULL;
}


/* Research project 12, a colony that needs two resources and exports one;
 * project 3, a project with an output and no input, which exports to any
 * planet a slipway joins it to. */
#define COLONY 12
#define EXPORTER 3

/* The tag class instances carried, which the frozen hashes fold: any
 * other value moves every *_HASH. */
#define FROZEN_INSTANCE_TAG 0x9E5C1451u

/* The tag entity structs carried, which the frozen hashes fold: any other
 * value moves every *_HASH. */
#define FROZEN_STRUCT_TAG 0x9E5C57C7u

#define TITLE_HASH 0x9D11B07Au
#define START_GAME_HASH 0x01F0B8FBu
#define PAUSE_HASH 0x25F95A65u
#define RESEARCH_HASH 0x7A0E4C6Au
#define PROJECTS_PANEL_HASH 0xE76A67ACu
#define INSPECTION_PANEL_HASH 0xBF3F72EFu
#define SCORE_PANEL_HASH 0xAB294E51u
#define DIFFICULTY_HASH 0x5178903Bu
#define UPDATE_HASH 0xD2A611E7u
#define LEFT_PRESS_HASH 0x6F6294FBu
#define REFRESH_HASH 0x0FA6ACC7u
#define PROJECTS_PANEL_ON_CLICK_HASH 0xCB3A928Cu
#define TECH_TREE_SCREEN_ON_CLICK_HASH 0x7ED641D9u
#define DRAW_HASH 0x1A156945u
#define BUTTON_MEMBERS_HASH 0xC3E845D7u

static p8_vram_t memory;
static p8_graphics_t graphics;
static p8_input_t input;

static uint32_t fnv(uint32_t h, uint32_t word) {
    int i;
    for (i = 0; i < 4; i++) {
        h ^= (word >> (8 * i)) & 0xFFu;
        h *= 16777619u;
    }
    return h;
}

static uint32_t fnv_str(uint32_t h, const char *s) {
    for (; *s != '\0'; s++) {
        h = fnv(h, (uint32_t)(unsigned char)*s);
    }
    return fnv(h, 0xFFu);
}

static int entity_list_rank_in(const entity_list_t *list, const void *e) {
    int i;
    for (i = 0; list != NULL && i < list->count; i++) {
        if (list->items[i] == e) {
            return i;
        }
    }
    return -1;
}

static int entity_list_listed(const entity_list_t *list, const void *e) {
    return entity_list_rank_in(list, e) >= 0;
}

static int id_list_rank_in(const entity_id_list_t *list, const void *e) {
    int i;
    for (i = 0; list != NULL && i < list->count; i++) {
        if (entity_id_list_at(list, i) == e) {
            return i;
        }
    }
    return -1;
}

static const entity_class_t *class_of(const void *e) {
    return ((const entity_t *)e)->class;
}

static int is_button(const void *e) {
    return e != NULL && class_of(e) == button_class();
}

static vec_t vec_of(const vec_t *v) {
    assert(v != NULL);
    return *v;
}

static uint32_t fold_vec(uint32_t h, const vec_t *p) {
    if (p == NULL) {
        return fnv(h, 0xDEADu);
    }
    h = fnv(h, (uint32_t)p->x);
    return fnv(h, (uint32_t)p->y);
}

/* A function by the order it was first seen in the scenario: its address
 * is not stable from one build to the next. */
static button_fn_t seen_fns[64];
static int seen_count;

static uint32_t fn_index(button_fn_t fn) {
    int i;
    for (i = 0; i < seen_count; i++) {
        if (seen_fns[i] == fn) {
            return (uint32_t)i;
        }
    }
    assert(seen_count < 64);
    seen_fns[seen_count] = fn;
    return (uint32_t)seen_count++;
}

static button_t *button_of(const void *e) {
    assert(class_of(e) == button_class());
    return (button_t *)e;
}

static int is_difficulty_copy_of(const void *e) {
    const button_t *dk = economy_score_difficulty();
    const button_t *nj = button_of(e);

    return dk != NULL && dk != nj && dk->name == nj->name && dk->has_trade == nj->has_trade && dk->trade == nj->trade &&
           dk->has_type_bias == nj->has_type_bias && dk->type_bias == nj->type_bias;
}

/* 0 absent or false, 2 true. */
static uint32_t is_hovered_state(const void *e) {
    const button_t *b = button_of(e);
    return b->header.is_hovered ? 2u : 0u;
}

/* 0 absent, 1 false, 3 a row. */
static uint32_t side_tooltip_state(const void *e) {
    const button_t *b = button_of(e);
    if (!b->has_side_tooltip) {
        return 0;
    }
    return b->side_tooltip == NULL ? 1u : 3u;
}

/* 0 absent, 1 false, 3 a function. */
static uint32_t on_click_state(const void *e) {
    const button_t *b = button_of(e);
    if (!b->has_on_click) {
        return 0;
    }
    return b->on_click == NULL ? 1u : 3u;
}

/* A research project folds by the tag of the class-instance form these
 * values were frozen on, whatever its form now. */
static int in_projects(const void *e) {
    int k;
    for (k = 1; k <= research_projects_count(); k++) {
        if (research_projects_at(k) == e) {
            return 1;
        }
    }
    return 0;
}

static int is_layout_row(const void *e) {
    return e != NULL &&
           ((const entity_t *)e)->class == layout_row_class();
}

static int row_count(const void *lq) {
    return ((const layout_row_t *)lq)->placed_items_count;
}

static layout_row_item_t row_item(const void *lq, int i) {
    assert(i >= 0 && i < row_count(lq));
    return ((const layout_row_t *)lq)->placed_items[i];
}

static uint32_t fold_ref(uint32_t h, const void *ref) {
    if (ref == NULL) {
        return fnv(h, 0xDEADu);
    }
    if (entity_list_listed(live_entities(), ref)) {
        return fnv(fnv(h, 0x4Eu), (uint32_t)entity_list_rank_in(live_entities(), ref));
    }
    if (in_projects(ref)) {
        return fnv(fnv(h, 0xEEEEu), FROZEN_INSTANCE_TAG);
    }
    return fnv(fnv(h, 0xEEEEu), FROZEN_STRUCT_TAG);
}

/* The words the frozen hashes fold before a fixed, a boolean and a string: the 0xF1, 0xB0
 * and 0x57 of a member of one kind are constant by the struct's shape,
 * kept so that the hashes still prove the typed reads. */
static uint32_t fold_fixed(uint32_t h, p8_fixed_t f) {
    return fnv(fnv(h, 0xF1u), (uint32_t)f);
}

static uint32_t fold_bool(uint32_t h, int b) {
    return fnv(fnv(h, 0xB0u), (uint32_t)b);
}

static uint32_t fold_string(uint32_t h, const char *s) {
    return fnv_str(fnv(h, 0x57u), s);
}

/* An absent member folds as 0xDEAD, the word frozen for an absent value. */
static uint32_t fold_fixed_or_nil(uint32_t h, int has, p8_fixed_t f) {
    return has ? fold_fixed(h, f) : fnv(h, 0xDEADu);
}

static uint32_t fold_string_or_nil(uint32_t h, const char *s) {
    return s != NULL ? fold_string(h, s) : fnv(h, 0xDEADu);
}

/* A member read as present (bit 1) or absent (bit 0), then its value. */
static uint32_t fold_opt_fixed(uint32_t h, int has, p8_fixed_t f) {
    return has ? fold_fixed(fnv(h, 1u), f) : fnv(h, 0u);
}

static uint32_t fold_opt_string(uint32_t h, const char *s) {
    return s != NULL ? fold_string(fnv(h, 1u), s) : fnv(h, 0u);
}

static uint32_t fold_opt_ref(uint32_t h, const void *ref) {
    return ref != NULL ? fold_ref(fnv(h, 1u), ref) : fnv(h, 0u);
}

/* A placed item as drawn: its kind, then its text and color or its sprite
 * and palette block, then its offset. */
static uint32_t fold_placed_item(uint32_t h, layout_row_item_t item) {
    h = fnv(h, (uint32_t)item.kind);
    switch (item.kind) {
    case LAYOUT_ROW_ITEM_TEXT:
        h = fold_fixed(fold_string(h, item.text), item.color);
        break;
    case LAYOUT_ROW_ITEM_SPRITE:
        h = fold_fixed(fold_fixed(h, item.sprite_id), item.sprite_palette_block);
        break;
    default:
        break;
    }
    return fold_fixed(h, item.offset.x);
}

/* A row by its width and its placed items. */
static uint32_t fold_layout_row(uint32_t h, const layout_row_t *lq) {
    int i;

    assert(is_layout_row(lq));
    h = fold_fixed(h, lq->width);
    for (i = 0; i < row_count(lq); i++) {
        h = fnv(h, (uint32_t)(i + 1));
        h = fold_placed_item(h, row_item(lq, i));
    }
    return h;
}

/* Absent, false, or what it holds: a row, a tooltip struct. A row folds by
 * the tag of the class-instance form these values were frozen on. */
static uint32_t fold_built(uint32_t h, const void *ref) {
    if (ref == NULL) {
        return fnv(h, 0xA0u);
    }
    if (is_layout_row(ref)) {
        return fold_layout_row(fnv(fnv(h, 0xA2u), FROZEN_INSTANCE_TAG), ref);
    }
    h = fnv(fnv(h, 0xA2u), FROZEN_STRUCT_TAG);
    return h;
}

static uint32_t fold_built_side_tooltip(uint32_t h, const button_t *b) {
    if (b->has_side_tooltip && b->side_tooltip == NULL) {
        return fold_bool(fnv(h, 0xA1u), 0);
    }
    return fold_built(h, b->has_side_tooltip ? b->side_tooltip : NULL);
}

static uint32_t fold_hit_box(uint32_t h, const button_t *b) {
    static const int dy[] = {-2, -1, 0, 5, 9, 10, 11};
    box_t kq = b->header.hit_box;
    int mb = p8_fixed_flr_int(b->width);
    int dx[6];
    size_t i;
    size_t j;

    assert(entity_has(&b->header, ENTITY_HAS_HIT_BOX));
    dx[0] = -1;
    dx[1] = 0;
    dx[2] = 1;
    dx[3] = mb - 1;
    dx[4] = mb;
    dx[5] = mb + 1;
    for (i = 0; i < 6; i++) {
        for (j = 0; j < sizeof dy / sizeof dy[0]; j++) {
            vec_t p = vec_make(p8_fixed_from_int(dx[i]), p8_fixed_from_int(dy[j]));
            h = fnv(h, (uint32_t)box_contains(kq, p));
        }
    }
    return h;
}

static const entity_id_list_t *main_ui_ephemerals(void) {
    return main_ui() == NULL ? NULL : &main_ui()->ephemerals;
}

/* A button as button_new and button_construct left it: every construction
 * key, what button_construct built, the methods and where entity_register
 * registered it. */
static uint32_t fold_button(uint32_t hash, const void *entity) {
    static const entity_message_t messages[] = {ENTITY_MESSAGE_DRAW, ENTITY_MESSAGE_DRAW_OVERLAY, ENTITY_MESSAGE_HIT_TEST, ENTITY_MESSAGE_REFRESH, ENTITY_MESSAGE_NEW_YEAR};
    const button_t *button = button_of(entity);
    size_t index;

    assert(button->header.state == ENTITY_STATE_NORMAL);
    hash = fold_fixed(hash, button->header.state_time);
    hash = fold_fixed(hash, button->header.layer);
    hash = fold_fixed(hash, button->width);
    hash = fold_hit_box(hash, button);
    hash = fold_vec(hash, pos_ref(&button->header));
    hash = fold_vec(hash, slide_target_ref(button));
    hash = fold_built(hash, button->layout_row);
    hash = fold_built_side_tooltip(hash, button);
    hash = fold_built(hash, button->tooltip);
    if (!button_of(button)->has_on_click) {
        hash = fnv(hash, 0x34u);
    } else if (button_of(button)->on_click == NULL) {
        hash = fnv(fnv(fnv(hash, 0x33u), 0xB0u), 0u);
    } else {
        hash = fnv(fnv(fnv(hash, 0x33u), 0xFCu), fn_index(button_of(button)->on_click));
    }
    if (button_of(button)->refresh != NULL) {
        hash = fnv(fnv(fnv(hash, 1u), 0xFCu), fn_index(button_of(button)->refresh));
    } else {
        hash = fnv(hash, 0u);
    }
    hash = fold_opt_string(hash, button->name);
    hash = fold_opt_fixed(hash, button->has_trade, button->trade);
    hash = fold_opt_fixed(hash, button->has_type_bias, button->type_bias);
    hash = fold_opt_ref(hash, entity_get(button->project_planet));
    hash = fold_opt_ref(hash, research_project_get(button->project));
    hash = fold_opt_fixed(hash, button->has_project_cost, button->project_cost);
    if (button->technology != NULL) {
        const technologies_entry_t *t = button->technology;
        assert(t == map_tables_technologies_lookup(t->name));
        hash = fold_fixed(fnv(hash, 1u), p8_fixed_from_int(t->icon));
    } else {
        hash = fnv(hash, 0u);
    }
    hash = fnv(hash, is_hovered_state(button));
    assert(class_of(button)->states[ENTITY_STATE_NORMAL] != NULL && class_of(button)->draw != NULL && class_of(button)->draw_overlay != NULL &&
           class_of(button)->left_press != NULL && class_of(button)->hit_test != NULL);
    assert(class_of(button)->accept_drop == NULL && class_of(button)->new_year == NULL);
    hash = fnv(hash, (uint32_t)entity_list_listed(live_entities(), button));
    for (index = 0; index < sizeof messages / sizeof messages[0]; index++) {
        hash = fnv(hash, (uint32_t)entity_list_listed(entities_by_message(messages[index]), button));
    }
    return fnv(hash, (uint32_t)id_list_rank_in(main_ui_ephemerals(), button));
}

static uint32_t fold_ephemerals(uint32_t h, const char *label) {
    const entity_id_list_t *le = main_ui_ephemerals();
    int i;

    assert(le != NULL);
    printf("[%s] %d button(s) in main_ui.ephemerals\n", label, le->count);
    h = fnv(h, (uint32_t)le->count);
    for (i = 0; i < le->count; i++) {
        void *b = entity_id_list_at(le, i);
        printf("[%s] button %d: on_click %u, side_tooltip %u, tooltip %u\n", label, i, on_click_state(b), side_tooltip_state(b),
               button_of(b)->tooltip != NULL ? 3u : 0u);
        h = fold_button(h, b);
    }
    return h;
}

static int same_vec(const vec_t *a, const vec_t *b) {
    return a != NULL && b != NULL && a->x == b->x && a->y == b->y;
}

/* The position every row of one button list starts from: each button holds
 * a copy of it. */
static void check_shared_pos(void) {
    const entity_id_list_t *le = main_ui_ephemerals();
    int i;
    for (i = 1; le != NULL && i < le->count; i++) {
        assert(same_vec(pos_ref(&button_of(entity_id_list_at(le, i))->header), pos_ref(&button_of(entity_id_list_at(le, 0))->header)));
    }
}

static void check(const char *label, uint32_t got, uint32_t want) {
    printf("[%s] 0x%08Xu\n", label, (unsigned)got);
    fflush(stdout);
    assert(got == want);
}

static void check_int(const char *label, int got, int want) {
    printf("[%s] %d\n", label, got);
    fflush(stdout);
    assert(got == want);
}

/* A cart_init on the title screen, with no frame: the previous help gives
 * back its help screen list, the only one, before the new help takes it. */
static void leave_help_screen(void) {
    const entity_list_t *hv = live_entities();
    for (int i = 0; i < hv->count; i++) {
        if (hv->items[i]->class == help_screen_class()) {
            entity_unregister(hv->items[i]);
            cart_flush_exit_queue();
            return;
        }
    }
}

static void start_title(void) {
    p8_input_init(&input);
    dispatch_context_set_input(&input);
    leave_help_screen();
    cart_init();
}

/* A game started as cart_start_game starts it, with no difficulty picked:
 * a NULL difficulty reads as difficulty 0. */
static void start_game(void) {
    start_title();
    cart_flush_exit_queue();
    cart_start_game(NULL);
    cart_flush_exit_queue();
}

/* The nth (from 1) of the title's four buttons, built in order from the
 * difficulty options: the buttons among the hit_test entities. */
static void *title_button(int i) {
    const entity_list_t *hd = entities_by_message(ENTITY_MESSAGE_HIT_TEST);
    int k;

    for (k = 0; hd != NULL && k < hd->count; k++) {
        void *e = hd->items[k];
        if (is_button(e) && --i == 0) {
            return e;
        }
    }
    assert(0 && "no such title button");
    return NULL;
}

static planet_t *as_planet(void *e) {
    assert(planet_is_family(e));
    return e;
}

static vec_t pos_of(void *e) {
    return vec_of(map_pos_ref(&as_planet(e)->header));
}

static p8_fixed_t dist2(vec_t a, vec_t b) {
    vec_t d = vec_sub(a, b);
    return p8_fixed_add(p8_fixed_mul(d.x, d.x), p8_fixed_mul(d.y, d.y));
}

static research_project_t *project(int k) {
    assert(research_projects_at(k) != NULL);
    return research_projects_at(k);
}

/* A generated planet colonized by COLONY, its nearest one by EXPORTER, the
 * two joined by a slipway paid by its drop as a drag pays it (the slipway's
 * drag reads the hovered planet off `main_ui()`), then left to settle;
 * returns the colony. */
static void *built_world(void) {
    const entity_list_t *jos;
    slipway_keys_t slipway_keys = {0};
    vec_t mp;
    void *qu;
    void *re = NULL;
    slipway_t *jc;
    int i;

    start_game();
    jos = entities_by_tag(TAG_PLANET);
    assert(jos != NULL && jos->count > 1);
    qu = jos->items[0];
    for (i = 1; i < jos->count; i++) {
        void *e = jos->items[i];
        if (re == NULL || dist2(pos_of(e), pos_of(qu)) < dist2(pos_of(re), pos_of(qu))) {
            re = e;
        }
    }
    planet_apply_project(qu, project(COLONY));
    planet_apply_project(re, project(EXPORTER));

    slipway_keys.start = entity_id_link(qu);
    jc = slipway_new(&slipway_keys);
    mp = vec_add(vec_mul(pos_of(re), p8_fixed_from_int(128)),
                       vec_make(p8_fixed_from_int(1), p8_fixed_from_int(1)));
    main_ui()->hovered = entity_id_link(re);
    slipway_class()->drag(&jc->header, mouse_world_only(mp));
    main_ui()->hovered = entity_id_link(NULL);
    while (jc->header.state_time < p8_fixed_from_int(15)) {
        update_entities();
    }
    slipway_class()->drop(&jc->header, re);
    for (i = 0; i < 120; i++) {
        update_entities();
    }
    return qu;
}

static void one_frame(void) {
    cart_update60();
    cart_draw();
}

/* A generated planet discovered by planet_roll_type and clicked: its
 * projects panel. */
static void known_panel(void) {
    void *p;

    start_game();
    p = entities_by_tag(TAG_PLANET)->items[0];
    planet_roll_type(p);
    (void)planet_left_press(p);
    one_frame();
}

/* built_world's colony with its inspection panel up. */
static void built_panel(void) {
    planet_inspection_panel(built_world());
    one_frame();
}

/* The tech tree screen, opened on a started game holding 5 research
 * points: some techs affordable, some too dear. */
static void tech_screen(void) {
    start_game();
    main_economy_typed()->research_points = p8_fixed_from_int(5);
    menu_show_tech_tree_screen();
    one_frame();
}

/* The game's end: the score panel's update once the end status is
 * "final score". */
static void game_end(void) {
    entity_t *ju;

    start_game();
    main_economy_typed()->end_status = "final score";
    main_economy_typed()->has_end_status = 1;
    ju = (entity_t *)main_score_panel();
    assert(ju->class->states[ENTITY_STATE_NORMAL] != NULL);
    ju->class->states[ENTITY_STATE_NORMAL](ju, p8_fixed_from_int(0));
    one_frame();
}

/* The button's `on_click` callback, called as a click calls it. */
static void call_on_click(void *e) {
    button_t *b = e;

    assert(b->on_click != NULL);
    b->on_click(b);
}

/* The galaxy menu buttons cart_start_game built: the only layer 11
 * entities with a `layout_row` among the hit_test entities right after it. */
static int galaxy_menu_buttons(void *out[4]) {
    const entity_list_t *hd = entities_by_message(ENTITY_MESSAGE_HIT_TEST);
    int n = 0;
    int i;
    for (i = 0; hd != NULL && i < hd->count; i++) {
        void *e = hd->items[i];
        if (is_button(e) && button_of(e)->header.layer == p8_fixed_from_int(11) && button_of(e)->layout_row != NULL) {
            assert(n < 4);
            out[n++] = e;
        }
    }
    return n;
}

/* The help screen empties the ephemerals of `main_ui()` right after the
 * title's button list opens: its buttons are the hit_test entities that are
 * buttons. */
static void test_title(void) {
    uint32_t h = 2166136261u;
    const entity_list_t *hd;
    void *first = NULL;
    int n = 0;
    int i;

    start_title();
    hd = entities_by_message(ENTITY_MESSAGE_HIT_TEST);
    for (i = 0; i < hd->count; i++) {
        void *e = hd->items[i];
        if (is_button(e)) {
            if (first == NULL) {
                first = e;
            }
            assert(same_vec(pos_ref(&button_of(e)->header), pos_ref(&button_of(first)->header)));
            h = fold_button(h, e);
            n++;
        }
    }
    check_int("title buttons", n, 4);
    check("title", h, TITLE_HASH);
}

static void test_start_game(void) {
    uint32_t h = 2166136261u;
    void *ix[4];
    int i;

    start_game();
    check_int("start_game buttons", galaxy_menu_buttons(ix), 4);
    for (i = 0; i < 4; i++) {
        assert(id_list_rank_in(main_ui_ephemerals(), ix[i]) < 0);
        assert((button_of(ix[i])->refresh != NULL) == (i == 1));
        h = fold_button(h, ix[i]);
    }
    check("start_game", h, START_GAME_HASH);
}

static void test_pause(void) {
    uint32_t h = 2166136261u;
    void *ix[4];

    start_game();
    assert(galaxy_menu_buttons(ix) == 4);
    call_on_click(ix[3]);
    check_shared_pos();
    h = fold_ephemerals(h, "pause");
    check("pause", h, PAUSE_HASH);
}

static button_t *galaxy_menu_entry(int i) {
    void *ix[4];
    int k;

    assert(galaxy_menu_buttons(ix) == 4);
    for (k = 0; k < 4; k++) {
        if (button_of(ix[k])->on_click == galaxy_menu_on_click(i)) {
            return button_of(ix[k]);
        }
    }
    assert(0 && "galaxy_menu_entry: no galaxy menu button carries that on_click");
    return NULL;
}

static int at(const vec_t *p, int x, int y) {
    return p != NULL && p->x == p8_fixed_from_int(x) && p->y == p8_fixed_from_int(y);
}

/* The pause button sits 11 lines above the bottom edge, the score button
 * centered at the top, the next-year button 10 pixels from the right edge; the pause menu opens 10 lines above the pause
 * button, then climbs by 11. */
static void test_bar_buttons_and_pause_menu_anchors(void) {
    const entity_id_list_t *le;
    int k;

    start_game();
    assert(at(pos_ref(&galaxy_menu_entry(4)->header), 0, P8_SCREEN_HEIGHT - 11));
    assert(at(pos_ref(&galaxy_menu_entry(3)->header), P8_SCREEN_WIDTH / 2 - 17, 0));
    assert(at(pos_ref(&galaxy_menu_entry(1)->header), P8_SCREEN_WIDTH - 10, 0));
    {
        button_t *next_year = galaxy_menu_entry(1);
        int touches = 0;
        int y;

        p8_camera(&graphics, 0, 0);
        p8_cls(&memory);
        class_of(next_year)->draw(&next_year->header, entity_pos(&next_year->header));
        for (y = 0; y < 11; y++) {
            touches |= memory.screen[y][P8_SCREEN_WIDTH - 1] != 0;
        }
        assert(touches);
    }
    call_on_click(galaxy_menu_entry(4));
    le = main_ui_ephemerals();
    assert(le != NULL && le->count == 4);
    for (k = 0; k < le->count; k++) {
        assert(at(pos_ref(&button_of(entity_id_list_at(le, k))->header), 0, P8_SCREEN_HEIGHT - 21));
        assert(at(slide_target_ref(button_of(entity_id_list_at(le, k))), 0, P8_SCREEN_HEIGHT - 21 - 11 * k));
    }
}

static void invent(char *rr) {
    assert(main_economy_typed() != NULL);
    pointer_list_add(&main_economy_typed()->invented, rr);
}

static void test_research(void) {
    uint32_t h = 2166136261u;
    const entity_id_list_t *le;
    const tooltip_t *jb;
    const layout_row_t *line;
    int unaffordable = 0;
    int i;

    start_game();
    invent("slipgates");
    menu_show_tech_tree_screen();
    check_shared_pos();
    le = main_ui_ephemerals();
    for (i = 0; i < le->count; i++) {
        if (on_click_state(entity_id_list_at(le, i)) == 1) {
            unaffordable++;
        }
    }
    printf("[research] %d unaffordable tech(s)\n", unaffordable);
    assert(unaffordable >= 1);
    assert(!button_of(entity_id_list_at(le, le->count - 1))->has_on_click && button_of(entity_id_list_at(le, le->count - 1))->tooltip != NULL);
    jb = button_of(entity_id_list_at(le, le->count - 1))->tooltip;
    assert(jb->rows_count == 1);
    line = jb->rows[0];
    assert(row_count(line) == 1 && row_item(line, 0).kind == LAYOUT_ROW_ITEM_TEXT);
    assert(strcmp(row_item(line, 0).text, "slipgates") == 0);
    h = fold_ephemerals(h, "research");
    check("research", h, RESEARCH_HASH);
}



static void test_projects_panel(void) {
    uint32_t h = 2166136261u;
    const entity_id_list_t *le;
    int with_planet = 0;
    int i;

    known_panel();
    le = main_ui_ephemerals();
    for (i = 0; i < le->count; i++) {
        with_planet += entity_get(button_of(entity_id_list_at(le, i))->project_planet) != NULL;
    }
    printf("[projects_panel] %d row(s) carry project_planet\n", with_planet);
    assert(with_planet >= 1);
    h = fold_ephemerals(h, "projects_panel");
    check("projects_panel", h, PROJECTS_PANEL_HASH);
}

static void test_inspection_panel(void) {
    uint32_t h = 2166136261u;
    built_panel();
    h = fold_ephemerals(h, "inspection_panel");
    check("inspection_panel", h, INSPECTION_PANEL_HASH);
}

/* The score panel's two buttons, whose results nothing keeps: the layer 15
 * entities with a `layout_row`. */
static void test_score_panel(void) {
    uint32_t h = 2166136261u;
    const entity_list_t *hd;
    int n = 0;
    int i;

    game_end();
    hd = entities_by_message(ENTITY_MESSAGE_HIT_TEST);
    for (i = 0; i < hd->count; i++) {
        void *e = hd->items[i];
        if (is_button(e) && button_of(e)->header.layer == p8_fixed_from_int(15) && button_of(e)->layout_row != NULL) {
            n++;
            h = fold_button(h, e);
        }
    }
    check_int("score_panel buttons", n, 2);
    check("score_panel", h, SCORE_PANEL_HASH);
}

/* The difficulty is the button cart_start_game got: its `type_bias` sizes
 * the galaxy. */
static void test_difficulty(void) {
    uint32_t h = 2166136261u;
    int i;

    for (i = 1; i <= 4; i++) {
        void *nj;
        const entity_list_t *jo;
        int j;

        /* The frame between two games: each `reset_world()` hands its ~90
         * planets to the exit queue, which only a frame empties. */
        cart_flush_exit_queue();
        start_title();
        cart_flush_exit_queue();
        nj = title_button(i);
        cart_start_game(nj);
        assert(is_difficulty_copy_of(nj));
        h = fold_fixed_or_nil(h, button_of(nj)->has_type_bias, button_of(nj)->type_bias);
        h = fold_fixed_or_nil(h, button_of(nj)->has_trade, button_of(nj)->trade);
        h = fold_string_or_nil(h, button_of(nj)->name);
        jo = entities_by_tag(TAG_PLANET);
        printf("[difficulty] option %d: %d planet(s)\n", i, jo->count);
        h = fnv(h, (uint32_t)jo->count);
        for (j = 0; j < jo->count; j++) {
            h = fold_vec(h, pos_ref(&as_planet(jo->items[j])->header));
        }
    }
    check("difficulty", h, DIFFICULTY_HASH);
}

static uint32_t screen_hash(void) {
    uint32_t h = 2166136261u;
    for (int y = 0; y < P8_SCREEN_HEIGHT; y++) {
        for (int x = 0; x < P8_SCREEN_WIDTH; x++) {
            h ^= (uint32_t)memory.screen[y][x];
            h *= 16777619u;
        }
    }
    return h;
}

static vec_t vec_at(int x, int y) {
    return vec_make(p8_fixed_from_int(x), p8_fixed_from_int(y));
}

/* A button of `keys` whose layout row holds the one text given, and whose
 * side tooltip, when its text is not NULL, holds that text in color 13. */
static void *build(button_keys_t keys, const char *layout_row_text, const char *h_text) {
    layout_row_keys_append(&keys.layout_row_keys, layout_row_cell_string(layout_row_text));
    if (h_text != NULL) {
        layout_row_keys_append(&keys.side_tooltip_row, layout_row_cell_string(h_text));
        keys.side_tooltip_row.has_color = 1;
        keys.side_tooltip_row.color = p8_fixed_from_int(13);
        keys.has_side_tooltip = 1;
        keys.has_side_tooltip_row = 1;
    }
    return button_new(&keys);
}

static button_keys_t button_at(int x, int y) {
    button_keys_t keys = {0};

    keys.has_pos = 1;
    keys.pos = vec_at(x, y);
    return keys;
}

static button_keys_t keyed_on_click(button_keys_t keys, button_fn_t my) {
    keys.has_on_click = 1;
    keys.on_click = my;
    return keys;
}

/* button_update moves the position 0.4 of the way to `slide_target`, then
 * jumps the last unit; the button copied both vectors from its keys. */
static void test_update(void) {
    uint32_t h = 2166136261u;
    void *b;
    int steps = 0;

    start_game();
    {
        button_keys_t keys = button_at(3, 4);

        keys.has_slide_target = 1;
        keys.slide_target = vec_at(40, -20);
        layout_row_keys_append(&keys.layout_row_keys, layout_row_cell_string("go"));
        b = button_new(&keys);
        keys.pos = vec_at(90, 90);
        keys.slide_target = vec_at(-90, -90);
        assert(at(pos_ref(&button_of(b)->header), 3, 4) && at(slide_target_ref(button_of(b)), 40, -20));
    }
    while (steps < 40) {
        vec_t before = vec_of(pos_ref(&button_of(b)->header));
        class_of(b)->states[ENTITY_STATE_NORMAL](b, p8_fixed_from_int(0));
        steps++;
        h = fold_vec(h, pos_ref(&button_of(b)->header));
        if (vec_of(pos_ref(&button_of(b)->header)).x == before.x && vec_of(pos_ref(&button_of(b)->header)).y == before.y) {
            break;
        }
    }
    assert(vec_of(pos_ref(&button_of(b)->header)).x == p8_fixed_from_int(40) && vec_of(pos_ref(&button_of(b)->header)).y == p8_fixed_from_int(-20));
    printf("[update] %d step(s)\n", steps);
    h = fnv(h, (uint32_t)steps);
    check("update", h, UPDATE_HASH);
}

static int on_click_calls;
static void *on_click_self;

static void counting_on_click(button_t *self) {
    on_click_calls++;
    on_click_self = self;
}

static void test_left_press(void) {
    uint32_t h = 2166136261u;
    const button_keys_t none = {0};
    down_t r;
    void *with_on_click;
    void *with_false;

    start_game();
    with_on_click = build(keyed_on_click(none, counting_on_click), "a", NULL);
    with_false = build(keyed_on_click(none, NULL), "b", NULL);

    r = class_of(with_on_click)->left_press(with_on_click, (mouse_pos_t){0});
    assert(r.stop && entity_get(r.held) == NULL);
    assert(on_click_calls == 1 && on_click_self == with_on_click);
    r = class_of(with_false)->left_press(with_false, (mouse_pos_t){0});
    assert(r.stop && entity_get(r.held) == NULL);
    assert(on_click_calls == 1);
    assert(on_click_state(with_false) == 1);
    h = fnv(h, (uint32_t)on_click_calls);
    h = fold_button(h, with_on_click);
    h = fold_button(h, with_false);
    check("left_press", h, LEFT_PRESS_HASH);
}

static uint32_t layout_row_first(const void *b) {
    const layout_row_t *lq = button_of(b)->layout_row;
    layout_row_item_t v;
    assert(lq != NULL && row_count(lq) > 0);
    v = row_item(lq, 0);
    assert(v.kind == LAYOUT_ROW_ITEM_SPRITE);
    return (uint32_t)p8_fixed_flr_int(v.sprite_id);
}

/* The second galaxy menu button's `refresh` rebuilds its `layout_row`: 24
 * when a tech is affordable. */
static void test_refresh(void) {
    uint32_t h = 2166136261u;
    void *ix[4];
    available_technologies_kept_t cb;
    void *before;
    int affordable;
    int i;

    start_game();
    assert(galaxy_menu_buttons(ix) == 4);
    for (i = 0; i < 4; i++) {
        assert(class_of(ix[i])->has_refresh(ix[i]) == (i == 1));
    }
    before = button_of(ix[1])->layout_row;
    class_of(ix[1])->refresh(ix[1]);
    cb = tech_gate_available_technologies(0);
    affordable = cb.count > 0;
    assert(button_of(ix[1])->layout_row != before);
    assert(layout_row_first(ix[1]) == (affordable ? 24u : 37u));
    h = fnv(h, layout_row_first(ix[1]));
    h = fold_fixed(h, button_of(ix[1])->layout_row->width);
    h = fold_fixed(h, button_of(ix[1])->width);
    check("refresh", h, REFRESH_HASH);
}

/* The `on_click` of a projects panel row builds on its `project_planet`. */
static void test_projects_panel_on_click(void) {
    uint32_t h = 2166136261u;
    const entity_id_list_t *le;
    int i;

    known_panel();
    le = main_ui_ephemerals();
    for (i = 0; i < le->count; i++) {
        void *b = entity_id_list_at(le, i);
        const planet_t *pt;
        if (entity_get(button_of(b)->project_planet) == NULL) {
            continue;
        }
        pt = (const planet_t *)entity_get(button_of(b)->project_planet);
        h = fold_string_or_nil(h, entity_state_name(pt->header.state));
        h = fnv(h, (uint32_t)live_entities()->count);
        call_on_click(b);
        printf("[projects_panel on_click] row %d: planet state=%s, %d entities\n", i,
               pt->header.state == ENTITY_STATE_SETTLED ? "settled" : "not settled", live_entities()->count);
        h = fold_string_or_nil(h, entity_state_name(pt->header.state));
        h = fnv(h, (uint32_t)live_entities()->count);
        break;
    }
    assert(i < le->count);
    check("projects_panel_on_click", h, PROJECTS_PANEL_ON_CLICK_HASH);
}

/* The `on_click` of a tech tree screen row invents its `technology`. */
static void test_tech_tree_screen_on_click(void) {
    uint32_t h = 2166136261u;
    const entity_id_list_t *le;
    const pointer_list_t *invented;
    const char *last;
    int i;

    tech_screen();
    le = main_ui_ephemerals();
    for (i = 0; i < le->count; i++) {
        if (on_click_state(entity_id_list_at(le, i)) == 3) {
            break;
        }
    }
    assert(i < le->count);
    assert(main_economy_typed() != NULL);
    invented = &main_economy_typed()->invented;
    h = fnv(h, (uint32_t)invented->count);
    call_on_click(entity_id_list_at(le, i));
    invented = &main_economy_typed()->invented;
    h = fnv(h, (uint32_t)invented->count);
    assert(invented->count >= 1);
    last = invented->items[invented->count - 1];
    h = fold_string(h, last);
    printf("[tech_tree_screen on_click] row %d invented %s\n", i, last);
    check("tech_tree_screen_on_click", h, TECH_TREE_SCREEN_ON_CLICK_HASH);
}

static uint32_t draw_alone(void *b) {
    p8_camera(&graphics, 0, 0);
    p8_cls(&memory);
    class_of(b)->draw(b, entity_pos(&button_of(b)->header));
    class_of(b)->draw_overlay(b, (vec_t){0, 0});
    return screen_hash();
}

/* One button at a time on a cleared screen: `on_click` absent, present,
 * hovered, with a price side tooltip, a tooltip shown only when hovered, and
 * a side tooltip left false (a free building of the projects panel). */
static void test_draw(void) {
    uint32_t hash = 2166136261u;
    button_keys_t free_keys = keyed_on_click(button_at(10, 20), counting_on_click);
    void *plain;
    void *with_on_click;
    void *priced;
    void *tooltip;
    void *free;
    uint32_t px[7];
    size_t i;

    start_game();
    plain = build(button_at(10, 20), "plain", NULL);
    with_on_click = build(keyed_on_click(button_at(10, 20), counting_on_click), "plain", NULL);
    priced = build(keyed_on_click(button_at(10, 20), counting_on_click), "plain", "12$");
    free_keys.has_side_tooltip = 1;
    free_keys.side_tooltip_is_false = 1;
    free = build(free_keys, "plain", NULL);
    tooltip = build(button_at(10, 20), "plain", NULL);
    ((button_t *)tooltip)->tooltip = tooltip_new(&heap_allocator, &icon_tooltips_table[0]);

    px[0] = draw_alone(plain);
    px[1] = draw_alone(with_on_click);
    button_of(with_on_click)->header.is_hovered = 1;
    px[2] = draw_alone(with_on_click);
    px[3] = draw_alone(priced);
    px[4] = draw_alone(tooltip);
    button_of(tooltip)->header.is_hovered = 1;
    px[5] = draw_alone(tooltip);
    px[6] = draw_alone(free);
    assert(side_tooltip_state(free) == 1 && px[6] == px[1]);
    assert(px[0] != px[1] && px[1] != px[2] && px[1] != px[3] && px[4] == px[0] && px[5] != px[4]);
    hash = fold_button(hash, free);
    for (i = 0; i < 7; i++) {
        hash = fnv(hash, px[i]);
    }
    check("draw", hash, DRAW_HASH);
}

/* Today's direct draw of a button, call for call: the oracle the image cache must match. */
static void direct_button_draw(p8_graphics_t *gfx, p8_vram_t *mem, const button_t *self, vec_t draw_pos) {
    p8_fixed_t width = self->width;
    p8_fixed_t right = p8_fixed_sub(p8_fixed_add(draw_pos.x, width), p8_fixed_from_int(1));
    p8_fixed_t bottom = p8_fixed_add(draw_pos.y, p8_fixed_from_int(9));
    int has_on_click = self->on_click != NULL;
    int hovered = self->header.is_hovered || (main_ui() != NULL && ui_pad_tab(main_ui()) == &self->header);
    int left_pixel = p8_fixed_flr_int(draw_pos.x);
    int top_pixel = p8_fixed_flr_int(draw_pos.y);
    int right_pixel = p8_fixed_flr_int(right);
    int bottom_pixel = p8_fixed_flr_int(bottom);

    p8_rect(gfx, mem, left_pixel - 1, top_pixel, right_pixel + 1, bottom_pixel + 1, 1);
    p8_rectfill(gfx, mem, left_pixel, top_pixel, right_pixel, bottom_pixel, has_on_click ? (hovered ? 12 : 13) : 5);
    p8_rectfill(gfx, mem, left_pixel, top_pixel, right_pixel, top_pixel, has_on_click ? 6 : 5);
    if (self->layout_row != NULL) {
        layout_row_draw_at(gfx, mem, self->layout_row, vec_add(draw_pos, vec_at(1, 2)), 0);
    }
    if (self->side_tooltip != NULL) {
        const layout_row_t *c = self->side_tooltip;
        int bg = 2;
        if (c->has_background && !c->background_is_bool) {
            bg = p8_fixed_flr_int(c->background);
        }
        p8_rectfill(gfx, mem, right_pixel + 3, top_pixel + 1, p8_fixed_flr_int(p8_fixed_add(right, c->width)) + 4,
                    bottom_pixel, bg);
        layout_row_draw_at(gfx, mem, c, vec_add(draw_pos, vec_make(p8_fixed_add(width, p8_fixed_from_int(3)),
                                                                   p8_fixed_from_int(2))), 0);
    }
}

static int g_button_builds;
static int g_button_copies;
static const p8_draw_backend_t *g_software;
static p8_draw_backend_t g_counting;

static int is_button_slot(int slot) {
    return slot >= IMAGE_SLOT_BUTTON_ICON_FIRST && slot < IMAGE_SLOT_BUTTON_WIDE_FIRST + IMAGE_SLOT_BUTTON_WIDE_COUNT;
}

static void count_image_begin(int slot, int w, int h) {
    g_button_builds += is_button_slot(slot);
    g_software->image_begin(slot, w, h);
}

static void count_image_draw(const p8_graphics_t *gfx, p8_vram_t *mem, int slot, int x, int y) {
    g_button_copies += is_button_slot(slot);
    g_software->image_draw(gfx, mem, slot, x, y);
}

static void count_button_images(void) {
    g_software = p8_draw_backend_get();
    g_counting = *g_software;
    g_counting.image_begin = count_image_begin;
    g_counting.image_draw = count_image_draw;
    p8_draw_backend_set(&g_counting);
    g_button_builds = 0;
    g_button_copies = 0;
}

static void stripes(p8_vram_t *mem) {
    for (int y = 0; y < P8_SCREEN_HEIGHT; y++) {
        for (int x = 0; x < P8_SCREEN_WIDTH; x++) {
            mem->screen[y][x] = (unsigned char)(1 + (x * 3 + y) % 15);
        }
    }
}

/* Draws b through button_draw and through the oracle from the same screen, camera, palette and
 * pen, then compares what they leave. */
static void assert_button_draws_like_direct(void *b, vec_t pos, int camera_x, int camera_y) {
    static p8_vram_t direct;
    p8_graphics_t direct_gfx;
    p8_camera(&graphics, camera_x, camera_y);
    graphics.pen_color = 9;
    stripes(&memory);
    direct = memory;
    direct_gfx = graphics;
    p8_draw_backend_set(g_software);
    direct_button_draw(&direct_gfx, &direct, button_of(b), pos);
    p8_draw_backend_set(&g_counting);

    button_draw(&graphics, &memory, button_of(b), pos);

    assert(graphics.camera_x == camera_x && graphics.camera_y == camera_y && "the camera is back");
    assert(graphics.pen_color == direct_gfx.pen_color && "the same pen after");
    assert(memcmp(direct.draw_palette, memory.draw_palette, sizeof direct.draw_palette) == 0 && "the same palette after");
    if (memcmp(direct.screen, memory.screen, sizeof direct.screen) != 0) {
        fprintf(stderr, "screens differ: button at (0x%08x, 0x%08x) camera (%d, %d)\n", (unsigned)pos.x,
                (unsigned)pos.y, camera_x, camera_y);
        assert(0);
    }
}

static layout_row_cell_t sprite_cell_of_width(int sprite, int block, int width) {
    layout_row_tuple_t tuple = {0};
    tuple.count = 3;
    tuple.scalars[0] = layout_row_scalar_fixed(p8_fixed_from_int(sprite));
    tuple.scalars[1] = layout_row_scalar_fixed(p8_fixed_from_int(width));
    tuple.scalars[2] = layout_row_scalar_fixed(p8_fixed_from_int(block));
    return layout_row_cell_tuple(tuple);
}

static layout_row_cell_t sprite_cell(int sprite, int block) {
    return sprite_cell_of_width(sprite, block, 7);
}

/* A sprite laid out 2 wide: its pixels run past the border, the image's rightmost column. */
static void *overhanging_sprite_button(void) {
    button_keys_t keys = keyed_on_click(button_at(0, 0), counting_on_click);
    layout_row_keys_append(&keys.layout_row_keys, layout_row_cell_string("ok"));
    layout_row_keys_append(&keys.layout_row_keys, sprite_cell_of_width(16, 0, 2));
    return button_new(&keys);
}

static void *icon_button(int sprite) {
    button_keys_t keys = keyed_on_click(button_at(0, 0), counting_on_click);
    layout_row_keys_append(&keys.layout_row_keys, layout_row_cell_fixed(p8_fixed_from_int(sprite)));
    return button_new(&keys);
}

static void *text_and_sprite_button(void) {
    button_keys_t keys = keyed_on_click(button_at(0, 0), counting_on_click);
    layout_row_keys_append(&keys.layout_row_keys, layout_row_cell_string("normal"));
    layout_row_keys_append(&keys.layout_row_keys, sprite_cell(24, 2));
    return button_new(&keys);
}

/* A price and an income: two texts and three sprites beside the button, on a background colour. */
static void *side_tooltip_button(void) {
    button_keys_t keys = keyed_on_click(button_at(0, 0), counting_on_click);
    layout_row_keys_append(&keys.layout_row_keys, layout_row_cell_string("trade hub"));
    layout_row_keys_append(&keys.side_tooltip_row, layout_row_cell_string("12"));
    layout_row_keys_append(&keys.side_tooltip_row, sprite_cell(37, 1));
    layout_row_keys_append(&keys.side_tooltip_row, layout_row_cell_string("+3"));
    layout_row_keys_append(&keys.side_tooltip_row, sprite_cell(24, 0));
    layout_row_keys_append(&keys.side_tooltip_row, sprite_cell(40, 3));
    keys.side_tooltip_row.has_color = 1;
    keys.side_tooltip_row.color = p8_fixed_from_int(13);
    keys.side_tooltip_row.has_background = 1;
    keys.side_tooltip_row.background = p8_fixed_from_int(1);
    keys.has_side_tooltip = 1;
    keys.has_side_tooltip_row = 1;
    return button_new(&keys);
}

static void assert_kinds_draw_like_direct(void *const *kinds, int count, vec_t pos, int camera_x, int camera_y) {
    for (int k = 0; k < count; k++) {
        button_image_frame_begin();
        hud_select_palette_block_default(&memory);
        assert_button_draws_like_direct(kinds[k], pos, camera_x, camera_y);
    }
}

/* Every kind of button the game draws, through the image cache, at whole and fractional
 * positions, at every edge and under three cameras. */
static void test_every_kind_of_button_draws_through_an_image_like_the_direct_draw(void) {
    static const int xs[] = {-120, -60, -1, 0, 1, 40, 100, 120, 127, 140};
    static const int ys[] = {-11, -10, -1, 0, 1, 60, 117, 118, 127};
    static const int cameras[][2] = {{0, 0}, {-13, 21}, {218, 276}};
    static const p8_fixed_t fractions[] = {0, P8_FIXED_LIT(0x00008000, 0.5), P8_FIXED_LIT(0x0000e666, 0.9)};
    void *kinds[8];

    start_game();
    count_button_images();
    kinds[0] = icon_button(24);
    kinds[1] = icon_button(37);
    button_of(kinds[1])->header.is_hovered = 1;
    kinds[2] = text_and_sprite_button();
    kinds[3] = build(button_at(0, 0), "no on-click", NULL);
    kinds[4] = side_tooltip_button();
    kinds[5] = side_tooltip_button();
    button_of(kinds[5])->header.is_hovered = 1;
    kinds[6] = build(keyed_on_click(button_at(0, 0), counting_on_click), "plain", "12$");
    kinds[7] = overhanging_sprite_button();
    for (size_t c = 0; c < sizeof cameras / sizeof cameras[0]; c++) {
        for (size_t i = 0; i < sizeof xs / sizeof xs[0]; i++) {
            for (size_t j = 0; j < sizeof ys / sizeof ys[0]; j++) {
                p8_fixed_t f = fractions[(i + j) % 3];
                vec_t pos = vec_make(p8_fixed_add(p8_fixed_from_int(xs[i] + cameras[c][0]), f),
                                     p8_fixed_add(p8_fixed_from_int(ys[j] + cameras[c][1]), fractions[(i + 2 * j) % 3]));
                assert_kinds_draw_like_direct(kinds, 8, pos, cameras[c][0], cameras[c][1]);
            }
        }
    }
    printf("[button images] %d builds, %d copies\n", g_button_builds, g_button_copies);
    assert(g_button_builds == 8 && "each kind is built once");
    assert(g_button_copies == 8 * 3 * 10 * 9 && "then copied every frame");
}

/* A row refilled in place (new contents, same address) rebuilds on that frame; an unchanged button
 * copies. */
static void test_a_refilled_row_rebuilds_and_an_unchanged_button_copies(void) {
    layout_row_keys_t keys = {0};
    void *b;

    start_game();
    count_button_images();
    b = icon_button(24);
    button_image_frame_begin();
    hud_select_palette_block_default(&memory);
    assert_button_draws_like_direct(b, vec_at(20, 30), 0, 0);
    button_image_frame_begin();
    assert_button_draws_like_direct(b, vec_at(21, 30), 0, 0);
    assert(g_button_builds == 1 && g_button_copies == 2);

    layout_row_keys_append(&keys, layout_row_cell_fixed(p8_fixed_from_int(37)));
    layout_row_init(button_of(b)->layout_row, button_of(b)->slot, &keys);
    button_image_frame_begin();
    assert_button_draws_like_direct(b, vec_at(21, 30), 0, 0);
    assert(g_button_builds == 2 && "a new row at the same address is a new image");
    button_of(b)->header.is_hovered = 1;
    button_image_frame_begin();
    assert_button_draws_like_direct(b, vec_at(21, 30), 0, 0);
    assert(g_button_builds == 3 && "a hover change is a new image");
}

/* What an image cannot copy exactly draws directly, and the same. */
static void test_what_an_image_cannot_copy_exactly_draws_directly(void) {
    button_keys_t fractional = keyed_on_click(button_at(0, 0), counting_on_click);
    layout_row_tuple_t tuple = {0};
    void *b;
    char label[16];

    start_game();
    count_button_images();
    b = text_and_sprite_button();
    hud_select_palette_block_default(&memory);
    p8_fillp(&graphics, 0x5A5A);
    assert_button_draws_like_direct(b, vec_at(20, 30), 0, 0);
    p8_fillp_reset(&graphics);
    hud_select_palette_block(&memory, p8_fixed_from_int(8));
    assert_button_draws_like_direct(b, vec_at(20, 30), 0, 0);
    assert(g_button_builds == 0 && g_button_copies == 0 && "a fill pattern or a remapped palette draws directly");

    tuple.count = 2;
    tuple.scalars[0] = layout_row_scalar_fixed(p8_fixed_from_int(24));
    tuple.scalars[1] = layout_row_scalar_fixed(P8_FIXED_LIT(0x00068000, 6.5));
    layout_row_keys_append(&fractional.layout_row_keys, layout_row_cell_tuple(tuple));
    layout_row_keys_append(&fractional.layout_row_keys, layout_row_cell_string("half"));
    b = button_new(&fractional);
    hud_select_palette_block_default(&memory);
    assert_button_draws_like_direct(b, vec_at(20, 30), 0, 0);
    assert_button_draws_like_direct(b, vec_make(P8_FIXED_LIT(0x00148000, 20.5), p8_fixed_from_int(30)), 0, 0);
    assert(g_button_builds == 0 && "a fractional width or offset draws directly");

    b = build(button_at(0, 0), "a label far too long to copy", NULL);
    assert(button_of(b)->width > p8_fixed_from_int(BUTTON_IMAGE_WIDE_WIDTH - 2));
    assert_button_draws_like_direct(b, vec_at(2, 30), 0, 0);
    assert(g_button_builds == 0 && "a box over the widest slot draws directly");
    button_of(b)->width = p8_fixed_from_int(60);

    button_image_frame_begin();
    for (int k = 0; k <= IMAGE_SLOT_BUTTON_WIDE_COUNT; k++) {
        layout_row_keys_t keys = {0};
        snprintf(label, sizeof label, "button %d", k);
        layout_row_keys_append(&keys, layout_row_cell_string(label));
        layout_row_init(button_of(b)->layout_row, &heap_allocator, &keys);
        assert_button_draws_like_direct(b, vec_at(10, k * 11), 0, 0);
    }
    assert(g_button_builds == IMAGE_SLOT_BUTTON_WIDE_COUNT && g_button_copies == IMAGE_SLOT_BUTTON_WIDE_COUNT
           && "the button past a full pool draws directly");
}

/* The first and last screen rows whose column 0 is no longer `background`;
 * -1 if none. */
static void painted_rows(int background, int *first, int *last) {
    int y;

    *first = -1;
    *last = -1;
    for (y = 0; y < P8_SCREEN_HEIGHT; y++) {
        if (memory.screen[y][0] != background) {
            if (*first < 0) {
                *first = y;
            }
            *last = y;
        }
    }
}

/* A hovered button's tooltip of n lines rests on the bottom edge: its
 * band runs from H - 1 - (8 n + 2) to H - 1. */
static void test_hovered_tooltip_rests_on_the_bottom_edge(void) {
    void *b;
    int n;
    int first;
    int last;

    start_game();
    b = build(button_at(10, 20), "plain", NULL);
    button_of(b)->tooltip = tooltip_new(&heap_allocator, &icon_tooltips_table[0]);
    button_of(b)->header.is_hovered = 1;
    n = button_of(b)->tooltip->rows_count;
    p8_camera(&graphics, 0, 0);
    p8_rectfill(&graphics, &memory, 0, 0, P8_SCREEN_WIDTH - 1, P8_SCREEN_HEIGHT - 1, 7);
    class_of(b)->draw_overlay(b, (vec_t){0, 0});
    painted_rows(7, &first, &last);
    printf("[hovered_tooltip] %d line(s), rows %d-%d\n", n, first, last);
    assert(last == P8_SCREEN_HEIGHT - 1);
    assert(first == P8_SCREEN_HEIGHT - 1 - (8 * n + 2));
}

static void test_nil_keys(void) {
    button_keys_t b = {0};
    void *nil_keys;
    void *no_keys;

    start_game();
    no_keys = button_new(&b);
    b.has_side_tooltip = 1;
    b.has_tooltip = 1;
    nil_keys = button_new(&b);
    assert(!button_of(no_keys)->has_side_tooltip && button_of(no_keys)->tooltip == NULL);
    assert(!button_of(nil_keys)->has_side_tooltip && button_of(nil_keys)->side_tooltip == NULL);
    assert(button_of(nil_keys)->tooltip == NULL);
}

/* How many button structs the live entities hold. */
static int all_buttons_are_structs(void) {
    const entity_list_t *hv = live_entities();
    int n = 0;
    int i;
    for (i = 0; i < hv->count; i++) {
        n += is_button(hv->items[i]);
    }
    return n;
}

/* Built by any producer, a button is a struct with the five methods of its
 * class. */
static void test_button_is_a_struct(void) {
    void *ix[4];
    void *nj;
    int i;

    start_title();
    check_int("struct title", all_buttons_are_structs(), 4);
    nj = title_button(2);
    cart_flush_exit_queue();
    cart_start_game(nj);
    assert(is_button(nj) && is_difficulty_copy_of(nj));
    assert(galaxy_menu_buttons(ix) == 4);
    check_int("struct start_game", all_buttons_are_structs(), 4);
    for (i = 0; i < 4; i++) {
        assert(class_of(ix[i])->has_refresh(ix[i]) == (i == 1));
        if (i == 1) {
            class_of(ix[i])->refresh(ix[i]);
        }
    }
    call_on_click(ix[3]);
    check_int("struct pause", all_buttons_are_structs(), 8);
    /* As in the game: the pause menu closes, and its buttons go, a frame
     * before the tech tree screen opens. */
    ui_clear_ephemerals(main_ui());
    cart_flush_exit_queue();
    menu_show_tech_tree_screen();
    check_int("struct research", all_buttons_are_structs(), 9);
    {
        const button_keys_t empty = {0};
        void *bare = button_new(&empty);
        assert(is_button(bare) && button_of(bare)->header.layer == p8_fixed_from_int(11) && !button_of(bare)->has_on_click);
    }
}

/* The kind words the frozen hashes fold before a value, FUNCTION_KIND the one
 * of a callback (the constants that fold `on_click` and `refresh`). Any other
 * value moves them. */
enum { FIXED_KIND = 0, STRING_KIND = 1, BOOL_KIND = 2, REF_KIND = 3, FUNCTION_KIND = 4 };

static int is_of_class(const void *ref, const entity_class_t *cls) {
    return ref != NULL && ((const entity_t *)ref)->class == cls;
}

/* `on_click` or `refresh` from its member, in the bytes the frozen values
 * folded: absent, false, or a function by first sight. */
static uint32_t fold_button_fn(uint32_t h, int present, button_fn_t fn) {
    if (!present) {
        return fnv(h, 0u);
    }
    if (fn == NULL) {
        return fold_bool(fnv(fnv(h, 1u), (uint32_t)BOOL_KIND), 0);
    }
    return fnv(fnv(fnv(fnv(h, 1u), (uint32_t)FUNCTION_KIND), 0xFCu), fn_index(fn));
}

/* A present member: bit 1, then the word of its kind. Both are constant by
 * the struct's shape for a member of one kind, kept so that
 * BUTTON_MEMBERS_HASH still proves the typed reads. */
static uint32_t fold_kind(uint32_t h, uint32_t kind) {
    return fnv(fnv(h, 1u), (uint32_t)kind);
}

/* A present reference: a row by content, a tooltip, planet or research
 * project struct by class, anything else by its tag. */
static uint32_t fold_member_ref(uint32_t h, const void *ref) {
    h = fold_kind(h, REF_KIND);
    if (is_layout_row(ref)) {
        return fold_layout_row(fnv(h, 0xA2u), ref);
    }
    if (is_of_class(ref, tooltip_class())) {
        return fnv(h, 0xB1u);
    }
    if (is_of_class(ref, planet_class())) {
        return fnv(h, 0xB2u);
    }
    if (is_of_class(ref, research_project_class())) {
        return fnv(h, 0xB3u);
    }
    return fnv(fnv(h, 0xB5u), FROZEN_STRUCT_TAG);
}

static uint32_t fold_member_opt_ref(uint32_t h, const void *ref) {
    return ref != NULL ? fold_member_ref(h, ref) : fnv(h, 0u);
}

static uint32_t fold_member_vec(uint32_t h, const vec_t *v) {
    if (v == NULL) {
        return fnv(h, 0u);
    }
    return fold_vec(fold_kind(h, REF_KIND), v);
}

static uint32_t fold_member_fixed(uint32_t h, int has, p8_fixed_t f) {
    return has ? fold_fixed(fold_kind(h, FIXED_KIND), f) : fnv(h, 0u);
}

/* The 17 members and the two callbacks, in the order the frozen constants
 * folded them: `on_click` after `slide_target`, `refresh` after
 * `is_hovered`. */
static uint32_t fold_button_members(uint32_t h, const void *e) {
    const button_t *b = button_of(e);

    h = fold_member_fixed(h, 1, b->header.layer);
    h = fold_member_opt_ref(h, b->layout_row);
    h = fold_member_fixed(h, 1, b->width);
    h = entity_has(&b->header, ENTITY_HAS_HIT_BOX) ? fold_hit_box(fold_kind(h, REF_KIND), b) : fnv(h, 0u);
    if (b->has_side_tooltip && b->side_tooltip == NULL) {
        h = fold_bool(fold_kind(h, BOOL_KIND), 0);
    } else {
        h = fold_member_opt_ref(h, b->has_side_tooltip ? b->side_tooltip : NULL);
    }
    h = fold_member_opt_ref(h, b->tooltip);
    h = fold_member_vec(h, pos_ref(&b->header));
    h = fold_member_vec(h, slide_target_ref(b));
    h = fold_button_fn(h, b->has_on_click, b->on_click);
    h = (b->header.is_hovered != 0) ? fold_bool(fold_kind(h, BOOL_KIND), b->header.is_hovered) : fnv(h, 0u);
    h = fold_button_fn(h, b->refresh != NULL, b->refresh);
    h = b->name != NULL ? fold_string(fold_kind(h, STRING_KIND), b->name) : fnv(h, 0u);
    h = fold_member_fixed(h, b->has_trade, b->trade);
    h = fold_member_fixed(h, b->has_type_bias, b->type_bias);
    if (b->technology != NULL) {
        const technologies_entry_t *t = b->technology;
        assert(t == map_tables_technologies_lookup(t->name));
        h = fold_fixed(fnv(fold_kind(h, REF_KIND), 0xB4u), p8_fixed_from_int(t->icon));
    } else {
        h = fnv(h, 0u);
    }
    h = fold_member_opt_ref(h, entity_get(b->project_planet));
    h = fold_member_opt_ref(h, research_project_get(b->project));
    return fold_member_fixed(h, b->has_project_cost, b->project_cost);
}

static uint32_t fold_ephemerals_members(uint32_t h) {
    const entity_id_list_t *le = main_ui_ephemerals();
    int i;

    assert(le != NULL);
    h = fnv(h, (uint32_t)le->count);
    for (i = 0; i < le->count; i++) {
        h = fold_button_members(h, entity_id_list_at(le, i));
    }
    return h;
}

/* One button list: one position value for all, a `slide_target` each. */
static void check_one_open_box(void *const *b, int n) {
    int i;
    int j;

    for (i = 0; i < n; i++) {
        assert(same_vec(pos_ref(&button_of(b[i])->header), pos_ref(&button_of(b[0])->header)));
        for (j = 0; j < i; j++) {
            assert(slide_target_ref(button_of(b[i])) != NULL && slide_target_ref(button_of(b[i])) != slide_target_ref(button_of(b[j])));
        }
    }
}

static void check_ephemerals_one_open_box(void) {
    const entity_id_list_t *le = main_ui_ephemerals();
    void *b[32];
    int i;

    assert(le != NULL && le->count <= 32);
    for (i = 0; i < le->count; i++) {
        b[i] = entity_id_list_at(le, i);
    }
    check_one_open_box(b, le->count);
}

/* `is_hovered` true, false, or absent (`present` 0), which reads as
 * false. */
static void write_is_hovered(button_t *b, int present, int value) {
    b->header.is_hovered = present ? value : 0;
}

/* `is_hovered` takes true, false and absent; `layout_row` a row or NULL.
 * The frozen values hold 23 more words, all 0, constant by the struct's
 * shape, kept so that BUTTON_MEMBERS_HASH still proves the typed
 * reads. */
static uint32_t fold_written(uint32_t hash, void *entity, void *other) {
    button_t *button = button_of(entity);
    box_t hit_box_before = button->header.hit_box;
    p8_fixed_t width = button->width;
    layout_row_t *layout_row;
    int index;

    assert(entity_has(&button->header, ENTITY_HAS_HIT_BOX));
    write_is_hovered(button, 1, 1);
    assert(button->header.is_hovered == 1);
    hash = fold_button_members(hash, entity);
    write_is_hovered(button, 1, 0);
    assert(button->header.is_hovered == 0);
    hash = fold_button_members(hash, entity);
    write_is_hovered(button, 1, 1);
    write_is_hovered(button, 1, 0);
    assert(button->header.is_hovered == 0);
    hash = fold_button_members(hash, entity);
    write_is_hovered(button, 1, 1);
    write_is_hovered(button, 0, 0);
    assert(!button->header.is_hovered);
    write_is_hovered(button, 1, 0);
    write_is_hovered(button, 0, 0);
    assert(!button->header.is_hovered);
    write_is_hovered(button, 0, 0);
    assert(!button->header.is_hovered);
    hash = fold_button_members(hash, entity);

    layout_row = button_of(other)->layout_row;
    assert(layout_row != NULL);
    button->layout_row = layout_row;
    hash = fold_button_members(hash, entity);
    layout_row = layout_row_new(&heap_allocator, NULL);
    button->layout_row = layout_row;
    hash = fold_button_members(hash, entity);
    button->layout_row = NULL;
    assert(button->header.hit_box.left == hit_box_before.left && button->header.hit_box.top == hit_box_before.top &&
           button->header.hit_box.right == hit_box_before.right && button->header.hit_box.bottom == hit_box_before.bottom);
    assert(button->width == width);
    hash = fold_button_members(hash, entity);
    button->layout_row = layout_row;

    write_is_hovered(button, 1, 1);
    for (index = 0; index < 15 + 8; index++) {
        hash = fnv(hash, 0u);
    }
    assert(button->header.is_hovered == 1);
    return fold_button_members(hash, entity);
}

static void *built(button_keys_t keys) {
    return button_new(&keys);
}

/* Folded, then out of the live entities and its slot given back, as a
 * frame would: the buttons fold_keys builds one after another hold no slot
 * at once. */
static uint32_t fold_and_drop(uint32_t h, button_t *b) {
    h = fold_button_members(h, b);
    entity_unregister(&b->header);
    cart_flush_exit_queue();
    return h;
}

/* Each optional key unset, the fixed ones and `on_click` given at zero, a
 * side tooltip false, a side tooltip and a tooltip key present but nil, a
 * side tooltip copied from a layout row (written on the member: no key
 * builds it), a row's keys as the layout row's source. */
static uint32_t fold_keys(uint32_t hash, layout_row_t *source_row) {
    static layout_row_t side_tooltip;
    const button_keys_t none = {0};
    button_keys_t keys;
    layout_row_keys_t from = {0};
    button_t *button;
    int index;

    for (index = 0; index < 12; index++) {
        button = built(none);
        assert(button->name == NULL && !button->has_trade && !button->has_type_bias && !button->has_project_cost);
        assert(pos_ref(&button->header) == NULL && slide_target_ref(button) == NULL && button->refresh == NULL && !button->has_on_click && button->technology == NULL);
        assert(entity_get(button->project_planet) == NULL && research_project_get(button->project) == NULL);
        hash = fold_and_drop(hash, button);
    }
    keys = none;
    keys.has_trade = 1;
    button = built(keys);
    assert(button->has_trade && button->trade == 0);
    hash = fold_and_drop(hash, button);
    keys = none;
    keys.has_type_bias = 1;
    button = built(keys);
    assert(button->has_type_bias && button->type_bias == 0);
    hash = fold_and_drop(hash, button);
    keys = none;
    keys.has_project_cost = 1;
    button = built(keys);
    assert(button->has_project_cost && button->project_cost == 0);
    hash = fold_and_drop(hash, button);
    for (index = 0; index < 2; index++) {
        button = built(keyed_on_click(none, NULL));
        assert(on_click_state(button) == 1);
        hash = fold_and_drop(hash, button);
    }
    keys = none;
    keys.has_side_tooltip = 1;
    keys.side_tooltip_is_false = 1;
    button = built(keys);
    assert(button->has_side_tooltip && button->side_tooltip == NULL);
    hash = fold_and_drop(hash, button);

    keys = none;
    keys.has_side_tooltip = 1;
    button = built(keys);
    assert(!button->has_side_tooltip && button->side_tooltip == NULL);
    hash = fold_and_drop(hash, button);
    keys = none;
    keys.has_tooltip = 1;
    button = built(keys);
    assert(button->tooltip == NULL);
    hash = fold_and_drop(hash, button);
    keys = none;
    button = built(keys);
    assert(button->layout_row != NULL && is_layout_row(button->layout_row));
    hash = fold_and_drop(hash, button);

    keys = none;
    button = built(keys);
    side_tooltip = *source_row;
    button->side_tooltip = &side_tooltip;
    button->has_side_tooltip = 1;
    assert(button->has_side_tooltip && is_layout_row(button->side_tooltip) && button->side_tooltip != source_row);
    hash = fold_and_drop(hash, button);

    layout_row_keys_append(&from, layout_row_cell_string("from a list"));
    layout_row_keys_append(&from, layout_row_cell_fixed(p8_fixed_from_int(3)));
    keys = none;
    keys.layout_row_keys = from;
    button = built(keys);
    assert(button->layout_row != NULL && is_layout_row(button->layout_row) && row_count(button->layout_row) == 2);
    return fold_and_drop(hash, button);
}

static uint32_t members_title_and_game(void) {
    const button_keys_t empty = {0};
    uint32_t hash = 2166136261u;
    const entity_list_t *hit_test_list;
    void *title[4];
    void *galaxy_menu[4];
    void *fresh[3];
    void *chosen_option;
    void *ordinary_button;
    void *on_click_false;
    int count = 0;
    int index;
    size_t key_index;

    start_title();
    hit_test_list = entities_by_message(ENTITY_MESSAGE_HIT_TEST);
    for (index = 0; index < hit_test_list->count; index++) {
        void *e = hit_test_list->items[index];
        if (is_button(e)) {
            assert(count < 4);
            title[count++] = e;
            assert(button_of(e)->name != NULL);
            assert(button_of(e)->header.layer == help_screen_archetype.back_color);
            hash = fold_button_members(hash, e);
        }
    }
    check_int("members title", count, 4);
    check_one_open_box(title, 4);

    chosen_option = title[1];
    assert(button_of(chosen_option)->on_click == help_start_game_field_value());
    assert(button_of(chosen_option)->has_trade && button_of(chosen_option)->has_type_bias);
    printf("[members] difficulty: trade %d, type_bias %d/65536\n", p8_fixed_flr_int(button_of(chosen_option)->trade), (int)button_of(chosen_option)->type_bias);
    hash = fold_button_members(hash, chosen_option);
    cart_flush_exit_queue();
    cart_start_game(chosen_option);
    assert(is_difficulty_copy_of(chosen_option));
    /* The title's buttons go at the next frame, as in the game. */
    cart_flush_exit_queue();
    assert(galaxy_menu_buttons(galaxy_menu) == 4);
    for (index = 0; index < 4; index++) {
        assert((button_of(galaxy_menu[index])->refresh != NULL) == (index == 1));
        hash = fold_button_members(hash, galaxy_menu[index]);
    }

    fresh[0] = button_new(NULL);
    fresh[1] = button_new(&empty);
    for (key_index = 0; key_index < 2; key_index++) {
        const button_t *b = button_of(fresh[key_index]);
        const layout_row_t *lq = b->layout_row;
        assert(lq != NULL && is_layout_row(lq) && row_count(lq) == 0);
        assert(b->header.layer == p8_fixed_from_int(11));
        assert(b->width == p8_fixed_add(lq->width, p8_fixed_from_int(2)));
        assert(entity_has(&b->header, ENTITY_HAS_HIT_BOX));
        assert(!b->has_side_tooltip && b->tooltip == NULL && pos_ref(&b->header) == NULL && slide_target_ref(b) == NULL && !b->header.is_hovered);
        assert(b->name == NULL && !b->has_trade && !b->has_type_bias && b->technology == NULL);
        assert(entity_get(b->project_planet) == NULL && research_project_get(b->project) == NULL && !b->has_project_cost);
        assert(on_click_state(fresh[key_index]) == 0 && button_of(fresh[key_index])->refresh == NULL);
        hash = fold_button_members(hash, fresh[key_index]);
    }

    call_on_click(galaxy_menu[3]);
    check_ephemerals_one_open_box();
    printf("[members] pause_menu: %d row(s)\n", main_ui_ephemerals()->count);
    hash = fold_ephemerals_members(hash);

    fresh[2] = button_new(NULL);
    hash = fold_written(hash, galaxy_menu[0], galaxy_menu[1]);
    hash = fold_written(hash, fresh[2], galaxy_menu[2]);
    assert(button_of(galaxy_menu[2])->layout_row != NULL);
    hash = fold_keys(hash, button_of(galaxy_menu[2])->layout_row);

    ordinary_button = build(button_at(10, 20), "plain", NULL);
    on_click_false = build(keyed_on_click(button_at(10, 20), NULL), "plain", NULL);
    assert(draw_alone(on_click_false) == draw_alone(ordinary_button));
    hash = fnv(hash, draw_alone(on_click_false));

    /* 10 words constant by the struct's shape, kept for the frozen hash. */
    for (key_index = 0; key_index < 10; key_index++) {
        hash = fnv(hash, 0u);
    }
    return hash;
}

static uint32_t members_research(void) {
    const entity_id_list_t *le;
    uint32_t h = 2166136261u;
    int dear = 0;
    int affordable = 0;
    int i;

    for (i = 0; i < TECHNOLOGIES_COUNT; i++) {
        assert(technologies_table[i].tooltip_rows != NULL);
    }
    /* The frozen hash folded the count of the old technologies table: each
     * record under its rank and under its name. */
    h = fnv(h, (uint32_t)(2 * TECHNOLOGIES_COUNT));

    tech_screen();
    le = main_ui_ephemerals();
    for (i = 0; i < le->count; i++) {
        void *b = entity_id_list_at(le, i);

        if (on_click_state(b) == 0) {
            continue;
        }
        if (on_click_state(b) == 1) {
            assert(button_of(b)->technology == NULL);
            dear++;
        } else {
            const technologies_entry_t *t = button_of(b)->technology;
            const layout_row_t *lq = button_of(b)->layout_row;
            assert(on_click_state(b) == 3 && lq != NULL);
            assert(row_count(lq) >= 2 && row_item(lq, 1).kind == LAYOUT_ROW_ITEM_TEXT);
            assert(t != NULL && t == map_tables_technologies_lookup(row_item(lq, 1).text));
            affordable++;
        }
        assert(button_of(b)->has_side_tooltip && is_layout_row(button_of(b)->side_tooltip));
    }
    printf("[members] tech_tree_screen: %d row(s), %d too dear, %d affordable\n", le->count, dear, affordable);
    assert(dear >= 1 && affordable >= 1);
    return fold_ephemerals_members(h);
}

static uint32_t members_projects_panel(void) {
    const entity_id_list_t *le;
    const void *pt = NULL;
    uint32_t h = 2166136261u;
    int rows = 0;
    int free_rows = 0;
    int i;

    known_panel();
    le = main_ui_ephemerals();
    for (i = 0; i < le->count; i++) {
        const button_t *b = button_of(entity_id_list_at(le, i));

        if (entity_get(b->project_planet) == NULL) {
            continue;
        }
        if (pt == NULL) {
            pt = entity_get(b->project_planet);
        }
        assert(entity_get(b->project_planet) == pt);
        assert(is_of_class(pt, planet_class()) && entity_list_listed(entities_by_tag(TAG_PLANET), pt));
        assert(research_project_get(b->project) != NULL && is_of_class(research_project_get(b->project), research_project_class()));
        assert(b->has_project_cost);
        assert(b->on_click != NULL);
        assert(b->tooltip != NULL && is_of_class(b->tooltip, tooltip_class()));
        if (b->project_cost > 0) {
            assert(b->has_side_tooltip && is_layout_row(b->side_tooltip));
        } else {
            assert(b->has_side_tooltip && b->side_tooltip == NULL);
            free_rows++;
        }
        rows++;
    }
    printf("[members] projects_panel: %d row(s), %d free\n", rows, free_rows);
    assert(rows >= 1);
    return fold_ephemerals_members(h);
}

static uint32_t members_inspection_panel(void) {
    built_panel();
    return fold_ephemerals_members(2166136261u);
}

static uint32_t members_score_panel(void) {
    uint32_t h = 2166136261u;
    const entity_list_t *hd;
    int n = 0;
    int i;

    game_end();
    hd = entities_by_message(ENTITY_MESSAGE_HIT_TEST);
    for (i = 0; i < hd->count; i++) {
        void *e = hd->items[i];
        if (is_button(e) && button_of(e)->header.layer == p8_fixed_from_int(15) && button_of(e)->layout_row != NULL) {
            n++;
            h = fold_button_members(h, e);
        }
    }
    assert(n == 2);
    return h;
}

/* A part runs in its own child forked from the fresh state, as
 * in_fresh_child does, and hands its hash back through a pipe. */
static uint32_t in_fresh_part(uint32_t (*part)(void)) {
    int fd[2];
    pid_t child;
    int status;
    uint32_t got = 0;
    ssize_t n;

    assert(pipe(fd) == 0);
    fflush(stdout);
    child = fork();
    assert(child >= 0);
    if (child == 0) {
        close(fd[0]);
        seen_count = 0;
        got = part();
        n = write(fd[1], &got, sizeof got);
        assert(n == (ssize_t)sizeof got);
        fflush(stdout);
        _exit(0);
    }
    close(fd[1]);
    n = read(fd[0], &got, sizeof got);
    close(fd[0]);
    assert(waitpid(child, &status, 0) == child);
    assert(WIFEXITED(status) && WEXITSTATUS(status) == 0);
    assert(n == (ssize_t)sizeof got);
    return got;
}

static void test_button_members_by_name(void) {
    uint32_t h = 2166136261u;

    h = fnv(h, in_fresh_part(members_title_and_game));
    h = fnv(h, in_fresh_part(members_research));
    h = fnv(h, in_fresh_part(members_projects_panel));
    h = fnv(h, in_fresh_part(members_inspection_panel));
    h = fnv(h, in_fresh_part(members_score_panel));
    check("button_members", h, BUTTON_MEMBERS_HASH);
}

/* A free building of the projects panel has a side tooltip that is false,
 * not the empty row of a present but absent key. */
static void test_projects_panel_free_row(void) {
    const entity_id_list_t *le;
    research_project_t *b = NULL;
    planet_t *pt = NULL;
    int found = 0;
    int i;

    known_panel();
    le = main_ui_ephemerals();
    for (i = 0; i < le->count && b == NULL; i++) {
        b = research_project_get(button_of(entity_id_list_at(le, i))->project);
        pt = (planet_t *)entity_get(button_of(entity_id_list_at(le, i))->project_planet);
    }
    assert(b != NULL && pt != NULL);
    b->has_base_cost = 1;
    b->base_cost = 0;
    planet_projects_panel(pt);
    le = main_ui_ephemerals();
    for (i = 0; i < le->count; i++) {
        const button_t *row = button_of(entity_id_list_at(le, i));

        if (research_project_get(row->project) == b) {
            assert(row->project_cost == 0 && row->has_side_tooltip && row->side_tooltip == NULL);
            found = 1;
        }
    }
    assert(found);
}

/* What the fourth galaxy menu button's `on_click` hands the button list as
 * its frame: a struct without method, for as long as the pause menu lives. */
static void test_pause_menu_open_box_is_a_struct(void) {
    const entity_t *o;
    size_t i;

    start_game();
    o = pause_menu_open_box();
    assert(o != NULL && o->class == pause_menu_open_box_class());
    assert(o->class->draw == NULL && o->class->draw_overlay == NULL && o->class->hit_test == NULL && o->class->new_year == NULL &&
           o->class->refresh == NULL && o->class->left_press == NULL && o->class->accept_drop == NULL);
    assert(o->class->drag == NULL && o->class->drop == NULL && o->class->attach_slipway == NULL && o->class->offer_trade == NULL && o->class->draw_at == NULL &&
           o->class->spawn_particle == NULL);
    for (i = 0; i < STATE_COUNT; i++) {
        assert(o->class->states[i] == NULL);
    }

    menu_shutdown();
    assert(pause_menu_open_box() == NULL);
    menu_init();
    assert(pause_menu_open_box() == o);
}

/* A button's layer is the frame's back color when the frame has
 * one, else the button's own. */
static void test_open_box_layer_only_from_back_color(void) {
    open_box_frame_t frame = {0};
    button_keys_t row = {0};

    start_game();
    message_box_open_box(&frame, &row, 1);
    assert(main_ui_ephemerals()->count == 1 && button_of(entity_id_list_at(main_ui_ephemerals(), 0))->header.layer == p8_fixed_from_int(11));
    frame.has_back_color = 1;
    frame.back_color = p8_fixed_from_int(14);
    message_box_open_box(&frame, &row, 1);
    assert(main_ui_ephemerals()->count == 1 && button_of(entity_id_list_at(main_ui_ephemerals(), 0))->header.layer == p8_fixed_from_int(14));
}

/* The frame holds its description and positions as values: rewriting it
 * after the box opened changes no button. The box starts at the fixed
 * position, and each row slides one row step further. */
static void test_open_box_copies_its_frame(void) {
    open_box_frame_t frame = {0};
    button_keys_t row = {0};
    char text[] = "earth-like";
    vec_t at = vec_make(p8_fixed_from_int(10), p8_fixed_from_int(20));
    vec_t step = vec_make(0, p8_fixed_from_int(11));
    const button_t *description;
    const button_t *first;

    start_game();
    open_box_frame_set_description(&frame, text);
    frame.has_fixed_pos = 1;
    frame.fixed_pos = at;
    frame.has_row_step = 1;
    frame.row_step = step;
    frame.has_pos = 1;
    frame.pos = vec_make(p8_fixed_from_int(99), p8_fixed_from_int(99));
    strcpy(text, "XXXXXXXXXX");
    message_box_open_box(&frame, &row, 1);
    memset(&frame, 'X', sizeof frame);
    assert(main_ui_ephemerals()->count == 2);
    description = button_of(entity_id_list_at(main_ui_ephemerals(), 0));
    first = button_of(entity_id_list_at(main_ui_ephemerals(), 1));
    assert(description->layout_row->placed_items_count >= 1 && strcmp(description->layout_row->placed_items[0].text, "earth-like") == 0);
    assert(description->slide_target.x == at.x && description->slide_target.y == at.y);
    assert(first->slide_target.x == at.x && first->slide_target.y == at.y + step.y);
}

/* Without a fixed position, the box starts at the frame's position. */
static void test_open_box_starts_at_its_position(void) {
    open_box_frame_t frame = {0};
    button_keys_t row = {0};
    vec_t at = vec_make(p8_fixed_from_int(30), p8_fixed_from_int(40));
    const button_t *b;

    start_game();
    frame.has_pos = 1;
    frame.pos = at;
    message_box_open_box(&frame, &row, 1);
    assert(main_ui_ephemerals()->count == 1);
    b = button_of(entity_id_list_at(main_ui_ephemerals(), 0));
    assert(b->slide_target.x == at.x && b->slide_target.y == at.y);
}

static void set_too_long_description(void) {
    open_box_frame_t frame = {0};

    open_box_frame_set_description(&frame, "level 4 (-12/-12)X");
}

/* A description past OPEN_BOX_DESCRIPTION_CAP stops on its assert. */
static void test_open_box_description_past_its_cap_aborts(void) {
    pid_t child;
    int status;

    fflush(stdout);
    child = fork();
    assert(child >= 0);
    if (child == 0) {
        assert(freopen("/dev/null", "w", stderr) != NULL);
        set_too_long_description();
        _exit(0);
    }
    assert(waitpid(child, &status, 0) == child);
    assert(WIFSIGNALED(status) && WTERMSIG(status) == SIGABRT);
}

/* tech_gate_available_technologies writes the affordable flag for every
 * entry, kept or not; `data_init()` forgets them all. It rebuilds the
 * projects in the load sub-zone, which holds one set: the reset comes
 * first, as in cart_load(). */
static void test_available_technologies_writes_every_is_affordable(void) {
    available_technologies_kept_t kept;
    p8_fixed_t jk;
    int dropped_affordable = 0;
    int i;

    start_game();
    main_economy_typed()->research_points = p8_fixed_from_int(12);
    jk = main_economy_typed()->research_points;
    kept = tech_gate_available_technologies(0);
    for (i = 0; i < TECHNOLOGIES_COUNT; i++) {
        const technologies_entry_t *t = &technologies_table[i];
        int affordable = tech_gate_research_cost(t) <= jk;
        int k;
        int is_kept = 0;

        assert(tech_gate_affordable_flags(t) == affordable);
        for (k = 0; k < kept.count; k++) {
            is_kept |= kept.items[k] == t;
        }
        dropped_affordable += affordable && !is_kept;
    }
    assert(dropped_affordable > 0);
    main_economy_typed()->research_points = p8_fixed_from_int(999);
    (void)tech_gate_available_technologies(0);
    for (i = 0; i < TECHNOLOGIES_COUNT; i++) {
        assert(tech_gate_affordable_flags(&technologies_table[i]) == 1);
    }
    game_zone_reset();
    data_init();
    for (i = 0; i < TECHNOLOGIES_COUNT; i++) {
        assert(tech_gate_affordable_flags(&technologies_table[i]) == 0);
    }
}

/* A NULL frame makes message_box_open_box a no-op. */
static void test_open_box_closed(void) {
    start_game();
    message_box_open_box(NULL, NULL, 0);
}

static void test_panels_are_structs(void) {
    known_panel();
    assert(all_buttons_are_structs() >= 2);
}

static void test_score_panel_are_structs(void) {
    game_end();
    assert(all_buttons_are_structs() >= 2);
}

/* The galaxy depends on the rnd state: run each scenario in a child forked
 * right after cart_load(), as a fresh process would start it. */
static void in_fresh_child(void (*test)(void)) {
    pid_t child;
    int status;

    fflush(stdout);
    child = fork();
    assert(child >= 0);
    if (child == 0) {
        seen_count = 0;
        test();
        fflush(stdout);
        _exit(0);
    }
    assert(waitpid(child, &status, 0) == child);
    assert(WIFEXITED(status) && WEXITSTATUS(status) == 0);
}

static int button_slots(void) {
    return ui_zone_live(SLOT_KIND_BUTTON);
}

/* The pause menu's buttons take a slot each; sliding to their
 * `slide_target` takes none; ui_clear_ephemerals takes them off, and the next
 * frame gives the slots back. */
static void test_button_slots_come_back(void) {
    void *ix[4];
    void *last;
    vec_t before;
    int base;
    int open;
    int i;

    start_game();
    one_frame();
    base = button_slots();
    assert(galaxy_menu_buttons(ix) == 4);
    call_on_click(ix[3]);
    open = button_slots();
    printf("[slots] button: %d before the pause menu, %d open\n", base, open);
    assert(open == base + main_ui_ephemerals()->count && main_ui_ephemerals()->count > 0);
    last = entity_id_list_at(main_ui_ephemerals(), main_ui_ephemerals()->count - 1);
    before = vec_of(pos_ref(&button_of(last)->header));
    for (i = 0; i < 30; i++) {
        one_frame();
        assert(button_slots() == open && "a sliding button takes no slot per tick");
    }
    assert(!same_vec(pos_ref(&button_of(last)->header), &before) && "the menu slid");
    ui_clear_ephemerals(main_ui());
    one_frame();
    assert(button_slots() == base);
}

/* `reset_world()` hands every button it drops to the exit queue: a new
 * title after a game and its pause menu holds as many slots as the first
 * title. */
static void test_button_slots_after_restart(void) {
    void *ix[4];
    int title;

    start_title();
    one_frame();
    title = button_slots();
    assert(title > 0);
    start_game();
    one_frame();
    assert(galaxy_menu_buttons(ix) == 4);
    call_on_click(ix[3]);
    one_frame();
    start_title();
    one_frame();
    assert(button_slots() == title);
}

static int row_slots(void) {
    return ui_zone_live(SLOT_KIND_ROW);
}

/* The second galaxy menu button's `refresh` rebuilds its icon row in a slot
 * of its own, given back as soon as the next `refresh` replaces it, and at
 * `reset_world()` with its button. */
static void test_galaxy_menu_button2_row_slots(void) {
    void *ix[4];
    int title;
    int once;
    int i;

    start_title();
    one_frame();
    title = row_slots();
    start_game();
    one_frame();
    assert(galaxy_menu_buttons(ix) == 4);
    class_of(ix[1])->refresh(ix[1]);
    once = row_slots();
    for (i = 0; i < 5; i++) {
        class_of(ix[1])->refresh(ix[1]);
        assert(row_slots() == once && "the replaced row gives its slot back on the spot");
    }
    entity_unregister(&button_of(ix[1])->header);
    one_frame();
    assert(row_slots() == once - 1 && "the button takes its row's slot along");
    start_title();
    one_frame();
    assert(row_slots() == title);
}

static int ephemerals_slots(void) {
    return ui_zone_live(SLOT_KIND_EPHEMERALS);
}

/* Each ephemerals list of `main_ui()` that ui_clear_ephemerals or a button
 * list makes takes a slot of its own; the list it replaces stays readable
 * until the next frame gives its slot back. */
static void test_ephemerals_slots_come_back(void) {
    void *ix[4];
    entity_id_list_t menu;
    entity_id_t first;
    int base;
    int open;
    int i;

    start_game();
    one_frame();
    base = ephemerals_slots();
    assert(galaxy_menu_buttons(ix) == 4);
    call_on_click(ix[3]);
    one_frame();
    open = ephemerals_slots();
    printf("[slots] ephemerals: %d before the pause menu, %d open\n", base, open);
    assert((main_ui()->has & UI_HAS_OWN_EPHEMERALS) && open == 1 && "the main ui's ephemerals are one list in a slot");
    for (i = 0; i < 5; i++) {
        menu = main_ui()->ephemerals;
        assert(menu.count > 0);
        first = menu.items[0];
        ui_clear_ephemerals(main_ui());
        assert(menu.items[0].index == first.index && menu.items[0].generation == first.generation &&
               "the replaced list lives until the next frame");
        call_on_click(ix[3]);
        one_frame();
        assert(ephemerals_slots() == open && "a replaced list gives its slot back");
    }
}

/* `reset_world()` drops the old `main_ui()` with its ephemerals, even a
 * list ui_clear_ephemerals made after it (a click on "restart" ends its frame
 * on the old one). */
static void test_ephemerals_slots_after_restart(void) {
    void *ix[4];
    ui_t *old;
    int title;

    start_title();
    one_frame();
    title = ephemerals_slots();
    start_game();
    one_frame();
    assert(galaxy_menu_buttons(ix) == 4);
    call_on_click(ix[3]);
    one_frame();
    old = main_ui();
    start_title();
    ui_clear_ephemerals(old);
    one_frame();
    assert(ephemerals_slots() == title);
}

static void print_line(void *context, const char *line) {
    (void)context;
    fputs(line, stdout);
}

static void test_largest_buttons_fit_their_slots(void) {
    const state_sink_t sink = {print_line, NULL};

    largest_slots_run();
    largest_slots_format(&sink);
    assert(ui_zone_max_bytes(SLOT_KIND_BUTTON) <= ui_zone_slot_size(SLOT_KIND_BUTTON));
}

static void test_project_planet_resolves_until_the_flush(void) {
    const entity_id_list_t *le;
    button_t *button = NULL;
    entity_t *planet;
    int i;

    known_panel();
    le = main_ui_ephemerals();
    for (i = 0; i < le->count && button == NULL; i++) {
        if (entity_get(button_of(entity_id_list_at(le, i))->project_planet) != NULL) {
            button = button_of(entity_id_list_at(le, i));
        }
    }
    assert(button != NULL);
    planet = entity_get(button->project_planet);
    assert(planet_is_family(planet));
    entity_unregister(planet);
    assert(entity_get(button->project_planet) == planet);
    cart_flush_exit_queue();
    assert(entity_get(button->project_planet) == NULL);
}

int main(void) {
    p8_vram_init(&memory);
    p8_gfx_init(&graphics);
    p8_sprite_sheet_load(&memory);
    p8_input_init(&input);
    dispatch_context_set_gfx(&graphics);
    dispatch_context_set_mem(&memory);
    dispatch_context_set_input(&input);

    cart_load();
    in_fresh_child(test_bar_buttons_and_pause_menu_anchors);
    in_fresh_child(test_title);
    in_fresh_child(test_start_game);
    in_fresh_child(test_project_planet_resolves_until_the_flush);
    in_fresh_child(test_pause);
    in_fresh_child(test_research);
    in_fresh_child(test_projects_panel);
    in_fresh_child(test_inspection_panel);
    in_fresh_child(test_score_panel);
    in_fresh_child(test_difficulty);
    in_fresh_child(test_update);
    in_fresh_child(test_left_press);
    in_fresh_child(test_refresh);
    in_fresh_child(test_projects_panel_on_click);
    in_fresh_child(test_tech_tree_screen_on_click);
    in_fresh_child(test_draw);
    in_fresh_child(test_every_kind_of_button_draws_through_an_image_like_the_direct_draw);
    in_fresh_child(test_a_refilled_row_rebuilds_and_an_unchanged_button_copies);
    in_fresh_child(test_what_an_image_cannot_copy_exactly_draws_directly);
    in_fresh_child(test_hovered_tooltip_rests_on_the_bottom_edge);
    in_fresh_child(test_nil_keys);
    in_fresh_child(test_projects_panel_free_row);
    in_fresh_child(test_button_is_a_struct);
    in_fresh_child(test_button_members_by_name);
    in_fresh_child(test_pause_menu_open_box_is_a_struct);
    in_fresh_child(test_open_box_closed);
    in_fresh_child(test_open_box_layer_only_from_back_color);
    in_fresh_child(test_open_box_copies_its_frame);
    in_fresh_child(test_open_box_starts_at_its_position);
    test_open_box_description_past_its_cap_aborts();
    in_fresh_child(test_available_technologies_writes_every_is_affordable);
    in_fresh_child(test_panels_are_structs);
    in_fresh_child(test_score_panel_are_structs);
    in_fresh_child(test_button_slots_come_back);
    in_fresh_child(test_button_slots_after_restart);
    in_fresh_child(test_galaxy_menu_button2_row_slots);
    in_fresh_child(test_ephemerals_slots_come_back);
    in_fresh_child(test_ephemerals_slots_after_restart);
    in_fresh_child(test_largest_buttons_fit_their_slots);

    printf("test_button: all tests passed\n");
    return 0;
}
