#include "game/pad_build.h"

#include "engine/dispatch_context.h"
#include "engine/entity_registry.h"
#include "game/cart.h"
#include "game/main_economy.h"
#include "game/pad_panel.h"
#include "game/structure_icon.h"
#include "p8/sprite_sheet.h"
#include "p8/gfx.h"
#include "p8/input.h"
#include "p8/vram.h"

#include <assert.h>
#include <stdio.h>

static p8_vram_t memory;
static p8_graphics_t graphics;
static p8_input_t input;

static void load_cart(void) {
    p8_vram_init(&memory);
    p8_gfx_init(&graphics);
    p8_sprite_sheet_load(&memory);
    p8_input_init(&input);
    dispatch_context_set_gfx(&graphics);
    dispatch_context_set_mem(&memory);
    dispatch_context_set_input(&input);
    cart_load();
}

static void refresh_everything(void) {
    const entity_list_t *refresh = entities_by_message(ENTITY_MESSAGE_REFRESH);
    for (int i = 0; i < refresh->count; i++) {
        if (refresh->items[i]->class->refresh != NULL) {
            refresh->items[i]->class->refresh(refresh->items[i]);
        }
    }
}

static void settle(void) {
    for (int i = 0; i < 20; i++) {
        cart_update60();
    }
}

static void start_title(void) {
    cart_init();
    settle();
}

/* From the title, or from a game through a new title: two titles in a row would hold two help screens. */
static void start_game(void) {
    if (main_economy_typed() != NULL) {
        start_title();
    }
    cart_start_game(NULL);
    settle();
}

static int focus_x(const entity_t *icon) {
    int x, y;
    pad_panel_focus_point(icon, &x, &y);
    return x;
}

static void count_icons(int *revealed, int *hidden, entity_t **rightmost) {
    *revealed = 0;
    *hidden = 0;
    *rightmost = NULL;
    const entity_list_t *bucket = entities_by_message(ENTITY_MESSAGE_HIT_TEST);
    for (int i = 0; i < bucket->count; i++) {
        entity_t *e = bucket->items[i];
        if (structure_icon_is_revealed(e)) {
            (*revealed)++;
            if (*rightmost == NULL || focus_x(e) > focus_x(*rightmost)) {
                *rightmost = e;
            }
        } else if (e->class == structure_icon_class()) {
            (*hidden)++;
        }
    }
}

static entity_t *basic_structure_icon(void) {
    int revealed, hidden;
    entity_t *rightmost;
    count_icons(&revealed, &hidden, &rightmost);
    assert(rightmost != NULL);
    return rightmost;
}

static void test_init_focuses_nothing(void) {
    pad_build_t b;
    pad_build_init(&b);
    assert(pad_build_icon(&b) == NULL);
}

static void test_focus_is_null_with_no_revealed_icon(void) {
    assert(main_economy_typed() == NULL);
    int revealed, hidden;
    entity_t *rightmost;
    count_icons(&revealed, &hidden, &rightmost);
    assert(revealed == 0);
    pad_build_t b;
    pad_build_init(&b);
    assert(pad_build_focus(&b) == NULL);
    assert(pad_build_icon(&b) == NULL);
}

static void test_focus_moves_a_missing_focus_to_the_basic_structure(void) {
    start_game();
    pad_build_t b;
    pad_build_init(&b);
    entity_t *basic = basic_structure_icon();
    assert(pad_build_focus(&b) == basic);
    assert(pad_build_icon(&b) == basic);
    assert(pad_build_focus(&b) == basic);
}

static void test_pick_with_nothing_focused_moves_nothing(void) {
    start_game();
    pad_build_t b;
    pad_build_init(&b);
    pad_build_pick(&b, -1);
    pad_build_pick(&b, 1);
    assert(pad_build_icon(&b) == NULL);
}

static void test_pick_walks_every_revealed_icon_in_decreasing_x_and_stops_at_both_ends(void) {
    static char *const techs[2] = { "slipgates", "ascension" };
    start_game();
    for (int i = 0; i < 2; i++) {
        pointer_list_add(&main_economy_typed()->invented, techs[i]);
    }
    refresh_everything();
    int revealed, hidden;
    entity_t *rightmost;
    count_icons(&revealed, &hidden, &rightmost);
    assert(revealed == 4 && hidden == 3);
    pad_build_t b;
    pad_build_init(&b);
    entity_t *visited[4];
    visited[0] = pad_build_focus(&b);
    assert(visited[0] == rightmost);
    pad_build_pick(&b, 1);
    assert(pad_build_icon(&b) == rightmost);
    for (int i = 1; i < revealed; i++) {
        pad_build_pick(&b, -1);
        visited[i] = pad_build_icon(&b);
        assert(structure_icon_is_revealed(visited[i]) && focus_x(visited[i]) < focus_x(visited[i - 1]));
    }
    pad_build_pick(&b, -1);
    assert(pad_build_icon(&b) == visited[revealed - 1]);
    for (int i = revealed - 2; i >= 0; i--) {
        pad_build_pick(&b, 1);
        assert(pad_build_icon(&b) == visited[i]);
    }
    pad_build_pick(&b, 1);
    assert(pad_build_icon(&b) == rightmost);
}

static void test_a_focus_from_an_old_game_is_stale_in_a_new_one(void) {
    start_game();
    pad_build_t b;
    pad_build_init(&b);
    entity_t *old_basic = pad_build_focus(&b);
    pad_build_pick(&b, -1);
    entity_t *old_processor = pad_build_icon(&b);
    assert(old_processor != NULL && old_processor != old_basic);
    start_game();
    assert(pad_build_icon(&b) == NULL);
    entity_t *basic = basic_structure_icon();
    assert(pad_build_focus(&b) == basic);
    assert(pad_build_icon(&b) == basic);
}

static void test_init_closes_the_bar(void) {
    pad_build_t b;
    pad_build_init(&b);
    assert(!pad_build_is_open(&b));
}

static void test_open_and_close_keep_the_focus(void) {
    start_game();
    pad_build_t b;
    pad_build_init(&b);
    pad_build_open(&b);
    assert(pad_build_is_open(&b));
    pad_build_focus(&b);
    pad_build_pick(&b, -1);
    entity_t *picked = pad_build_icon(&b);
    assert(picked != NULL && picked != basic_structure_icon());
    pad_build_close(&b);
    assert(!pad_build_is_open(&b) && pad_build_icon(&b) == picked);
    pad_build_open(&b);
    assert(pad_build_is_open(&b) && pad_build_icon(&b) == picked);
}

int main(void) {
    load_cart();
    start_title();
    test_init_focuses_nothing();
    test_init_closes_the_bar();
    test_focus_is_null_with_no_revealed_icon();
    test_focus_moves_a_missing_focus_to_the_basic_structure();
    test_pick_with_nothing_focused_moves_nothing();
    test_pick_walks_every_revealed_icon_in_decreasing_x_and_stops_at_both_ends();
    test_a_focus_from_an_old_game_is_stale_in_a_new_one();
    test_open_and_close_keep_the_focus();
    printf("test_pad_build: ok\n");
    return 0;
}
