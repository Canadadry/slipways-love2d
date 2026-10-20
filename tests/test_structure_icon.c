/* The structure icon, read and written on its struct members and driven
 * through the real class methods, cart_init() and cart_start_game(), in
 * games started directly: no events file. */

#define _POSIX_C_SOURCE 200809L

#include <assert.h>
#include <signal.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>

#include "p8/sprite_sheet.h"
#include "game/background.h"
#include "game/cart.h"
#include "engine/dispatch_context.h"
#include "engine/entity.h"
#include "engine/entity_registry.h"
#include "p8/fixed.h"
#include "game/main_economy.h"
#include "engine/geom.h"
#include "p8/gfx.h"
#include "p8/input.h"
#include "game/layout.h"
#include "p8/vram.h"
#include "game/planet_family.h"
#include "game/planet_family_trade.h"
#include "game/structure_icon.h"
#include "game/slipway.h"
#include "engine/entity_class.h"
#include "engine/entity_id.h"
#include "game/ui.h"
#include "game/ui_button.h"
#include "engine/vec.h"
#include "game/wormhole.h"
#include "engine/game_zone.h"
#include "heap_allocator.h"

static int same_pos(vec_t a, vec_t b) {
    return a.x == b.x && a.y == b.y;
}

static const vec_t *pos_ref(const entity_t *e) {
    return entity_has_pos(e) ? &e->pos : NULL;
}


#define CONSTRUCTION_HASH 0x4639EF5Du
#define START_HASH 0xC69B34A1u
#define DIRECTED_HASH 0xD03BC385u
#define DRAW_HASH 0xFDCE193Fu
#define RESTART_HASH 0x2A322B1Au
#define KIND_VIEW_HASH 0x7DAC5ABFu

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

static structure_icon_t *as_structure_icon(void *e) {
    assert(e != NULL && ((const entity_t *)e)->class == structure_icon_class());
    return e;
}

/* The cursor and the counter are the module's shared state, no member of
 * any icon: the same answer whatever icon asks. */
static int shared_cursor(const structure_icon_t *icon, vec_t *out) {
    (void)icon;
    *out = structure_icon_cursor();
    return structure_icon_has_cursor();
}

/* 0 while the counter is unset. */
static int shared_id(const structure_icon_t *icon, p8_fixed_t *out) {
    (void)icon;
    return structure_icon_id_counter(out);
}

static p8_fixed_t shared_id_fixed(const structure_icon_t *icon) {
    p8_fixed_t id = 0;
    int present = shared_id(icon, &id);
    assert(present);
    return id;
}

/* 0 absent, 1 false, 2 true. */
static uint32_t bool_state(int has, int b) {
    if (!has) {
        return 0;
    }
    return b ? 2 : 1;
}

static uint32_t is_revealed_state(const void *icon) {
    const structure_icon_t *ie = as_structure_icon((void *)icon);
    return bool_state(ie->has_is_revealed, ie->is_revealed);
}

static uint32_t is_hovered_state(const void *icon) {
    const structure_icon_t *ie = as_structure_icon((void *)icon);
    return bool_state((ie->header.is_hovered != 0), ie->header.is_hovered);
}

static uint32_t fold_vec(uint32_t h, const vec_t *p) {
    if (p == NULL) {
        return fnv(h, 0xDEADu);
    }
    h = fnv(h, (uint32_t)p->x);
    return fnv(h, (uint32_t)p->y);
}

/* The kind bytes the frozen constants were folded with (0xF1 a number,
 * 0x57 a string, 0xDEAD absent); a NULL string is absent. */
static uint32_t fold_fixed(uint32_t h, p8_fixed_t v) {
    return fnv(fnv(h, 0xF1u), (uint32_t)v);
}

static uint32_t fold_string(uint32_t h, const char *s) {
    return s == NULL ? fnv(h, 0xDEADu) : fnv_str(fnv(h, 0x57u), s);
}

/* An icon is the one entity listening to "refresh" whose `kind` holds a
 * kind, not an entity: there is no icon tag. */
static int is_icon(const void *e) {
    return e != NULL && ((const entity_t *)e)->class == structure_icon_class() &&
           ((const structure_icon_t *)e)->kind != NULL;
}

static int icons(void *out[], int cap) {
    const entity_list_t *hb = entities_by_message(ENTITY_MESSAGE_REFRESH);
    int n = 0;
    int i;
    for (i = 0; hb != NULL && i < hb->count; i++) {
        void *e = hb->items[i];
        if (is_icon(e)) {
            assert(n < cap);
            out[n++] = e;
        }
    }
    return n;
}

static int icon_rank(const void *e) {
    void *all[64];
    int n = icons(all, 64);
    int i;
    for (i = 0; i < n; i++) {
        if (all[i] == e) {
            return i;
        }
    }
    return -1;
}

static void fill_kind_table(const entity_kind_t *out[7]) {
    out[0] = structure_kind();
    out[1] = processor_kind();
    out[2] = slipgate_kind();
    out[3] = infraspace_gate_kind();
    out[4] = trading_hub_kind();
    out[5] = synthesizer_kind();
    out[6] = ascension_kind();
}

/* The cursor and the counter read off any icon: every icon shares
 * them. */
static uint32_t fold_shared(uint32_t h, void *icon) {
    p8_fixed_t id = 0;
    vec_t cursor;

    h = fold_vec(h, shared_cursor(as_structure_icon(icon), &cursor) ? &cursor : NULL);
    return shared_id(as_structure_icon(icon), &id) ? fold_fixed(h, id) : fnv(h, 0xDEADu);
}

/* `state_time` is always a number: its kind byte is constant by the struct's
 * shape, kept so that the frozen constants still prove the typed reads. */
static uint32_t fold_icon(uint32_t h, void *icon) {
    const structure_icon_t *ie = as_structure_icon(icon);
    h = fnv(h, is_revealed_state(ie));
    h = fold_vec(h, pos_ref(&ie->header));
    h = fnv(h, is_hovered_state(ie));
    return fold_fixed(h, ie->header.state_time);
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

static void new_game(void) {
    cart_start_game(NULL);
    cart_flush_exit_queue();
}

/* A game started as cart_start_game() starts it, with no difficulty picked:
 * a NULL difficulty reads as difficulty 0. */
static void start_game(void) {
    p8_input_init(&input);
    dispatch_context_set_input(&input);
    cart_init();
    new_game();
}

static const entity_class_t *class_of(const void *e) {
    return ((const entity_t *)e)->class;
}

static mouse_pos_t mouse_pos(vec_t world, vec_t screen) {
    return (mouse_pos_t){.world = world, .screen = screen, .has = MOUSE_POS_HAS_WORLD | MOUSE_POS_HAS_SCREEN};
}

static vec_t vec_at(int x, int y) {
    return vec_make(p8_fixed_from_int(x), p8_fixed_from_int(y));
}

static const data_row_t *icon_tooltips_row(int i) {
    assert(i < ICON_TOOLTIPS_COUNT);
    return &icon_tooltips_table[i];
}

static void *build_icon(const entity_kind_t *ke, tooltip_t *jb) {
    structure_icon_keys_t keys = {0};

    keys.kind = ke;
    keys.tooltip = jb;
    return structure_icon_new(&keys);
}

/* The hit box by which of a ring of points around its corners it holds. */
static uint32_t fold_hit_box(uint32_t h, void *icon) {
    static const int probe[][2] = {{0, 0},  {-1, 0}, {0, -1}, {8, 7},  {9, 8},  {10, 8}, {9, 9},
                                   {-1, -1}, {4, 4},  {8, 0},  {9, 0},  {0, 7},  {0, 8},  {0, 9}};
    box_t kq = as_structure_icon(icon)->archetype->hit_box;
    size_t i;

    for (i = 0; i < sizeof probe / sizeof probe[0]; i++) {
        h = fnv(h, (uint32_t)box_contains(kq, vec_at(probe[i][0], probe[i][1])));
    }
    return h;
}

/* A fresh icon: its archetype's hit box, layer, position and rest position,
 * its state and state time, the two construction keys, and nothing written
 * by the refresh or the hover loop yet. */
static uint32_t check_fresh_icon(uint32_t h, void *icon, const void *ke, const void *jb) {
    const structure_icon_t *ie = as_structure_icon(icon);

    assert(ie->header.state == ENTITY_STATE_NORMAL);
    assert(ie->header.state_time == 0);
    assert(ie->archetype == &structure_icon_archetype);
    assert(ie->archetype->layer == p8_fixed_from_int(12));
    assert(entity_has_pos(&ie->header));
    assert(pos_ref(&ie->header)->x == p8_fixed_from_int(999) && pos_ref(&ie->header)->y == p8_fixed_from_int(999));
    assert(ie->archetype->rest_cursor.x == p8_fixed_from_int(P8_SCREEN_WIDTH - 10) && ie->archetype->rest_cursor.y == p8_fixed_from_int(P8_SCREEN_HEIGHT - 8));
    assert((const void *)ie->kind == ke);
    assert((const void *)ie->tooltip == jb);
    assert(!ie->has_is_revealed && !ie->header.is_hovered);
    assert(class_of(icon)->states[ENTITY_STATE_NORMAL] == NULL && class_of(icon)->draw_overlay == NULL && class_of(icon)->new_year == NULL &&
           class_of(icon)->accept_drop == NULL);
    assert(class_of(icon)->hit_test != NULL && class_of(icon)->refresh != NULL && class_of(icon)->draw != NULL &&
           class_of(icon)->left_press != NULL);
    assert(entity_list_listed(live_entities(), icon) && entity_list_listed(entities_by_message(ENTITY_MESSAGE_DRAW), icon) && entity_list_listed(entities_by_message(ENTITY_MESSAGE_HIT_TEST), icon) &&
           entity_list_listed(entities_by_message(ENTITY_MESSAGE_REFRESH), icon));
    assert(!entity_list_listed(entities_by_message(ENTITY_MESSAGE_DRAW_OVERLAY), icon) && !entity_list_listed(entities_by_message(ENTITY_MESSAGE_NEW_YEAR), icon));

    h = fold_string(h, entity_state_name(ie->header.state));
    h = fold_fixed(h, ie->archetype->layer);
    h = fold_hit_box(h, icon);
    h = fold_vec(h, pos_ref(&ie->header));
    h = fold_vec(h, &ie->archetype->rest_cursor);
    return fold_shared(h, icon);
}

static void test_construction(void) {
    uint32_t h = 2166136261u;
    const entity_kind_t *classes[7];
    void *all[16];
    void *a;
    void *b;
    tooltip_t *tooltip_a;
    tooltip_t *tooltip_b;
    int n;
    int i;

    start_game();
    fill_kind_table(classes);
    n = icons(all, 16);
    check_int("start_game icons", n, 7);
    for (i = 0; i < n; i++) {
        const structure_icon_t *ie = as_structure_icon(all[i]);
        assert(ie->kind == classes[i]);
        assert(ie->tooltip != NULL);
        assert(ie->header.state == ENTITY_STATE_NORMAL);
        assert(ie->archetype->layer == p8_fixed_from_int(12));
        assert(class_of(all[i])->hit_test != NULL);
        assert(entity_list_listed(live_entities(), all[i]) && entity_list_listed(entities_by_message(ENTITY_MESSAGE_DRAW), all[i]) && entity_list_listed(entities_by_message(ENTITY_MESSAGE_HIT_TEST), all[i]));
        assert(!entity_list_listed(entities_by_message(ENTITY_MESSAGE_DRAW_OVERLAY), all[i]) && !entity_list_listed(entities_by_message(ENTITY_MESSAGE_NEW_YEAR), all[i]));
        h = fold_hit_box(h, all[i]);
    }

    tooltip_a = tooltip_new(&heap_allocator, icon_tooltips_row(0));
    tooltip_b = tooltip_new(&heap_allocator, icon_tooltips_row(1));
    a = build_icon(classes[0], tooltip_a);
    b = build_icon(classes[1], tooltip_b);
    h = check_fresh_icon(h, a, classes[0], tooltip_a);
    h = check_fresh_icon(h, b, classes[1], tooltip_b);
    assert(same_pos(as_structure_icon(a)->header.pos, as_structure_icon(b)->header.pos));
    assert(as_structure_icon(a)->archetype == as_structure_icon(b)->archetype);
    assert(as_structure_icon(a)->archetype == as_structure_icon(all[0])->archetype);
    assert(icon_rank(a) == 7 && icon_rank(b) == 8);
    check("construction", h, CONSTRUCTION_HASH);
}

/* cart_init() points the cursor at the rest position itself, then the
 * economy_recompute() of cart_start_game() reveals every icon whose research
 * is done, right to left from there. */
static void test_start(void) {
    uint32_t h = 2166136261u;
    void *all[16];
    int n;
    int i;
    int first = -1;

    start_game();
    n = icons(all, 16);
    for (i = 0; i < n; i++) {
        if (first < 0 && is_revealed_state(all[i]) == 2) {
            first = i;
        }
        h = fold_icon(h, all[i]);
    }
    printf("[start] first revealed icon %d\n", first);
    assert(first >= 0);
    assert(same_pos(as_structure_icon(all[first])->header.pos, as_structure_icon(all[first])->archetype->rest_cursor));
    for (i = 0; i < n; i++) {
        if (is_revealed_state(all[i]) != 2) {
            assert(!same_pos(as_structure_icon(all[i])->header.pos, as_structure_icon(all[first])->header.pos));
        }
    }
    h = fold_shared(h, all[0]);
    check("start", h, START_HASH);
}

/* The icons revealed at the start line up from the bottom-right corner,
 * 10 pixels apart, leftward. */
static void test_revealed_icons_anchor_bottom_right(void) {
    void *all[16];
    int n;
    int revealed = 0;
    int i;

    start_game();
    n = icons(all, 16);
    for (i = 0; i < n; i++) {
        const vec_t *pos = pos_ref(&as_structure_icon(all[i])->header);
        if (is_revealed_state(all[i]) != 2) {
            continue;
        }
        assert(pos->x == p8_fixed_from_int(P8_SCREEN_WIDTH - 10 - 10 * revealed));
        assert(pos->y == p8_fixed_from_int(P8_SCREEN_HEIGHT - 8));
        revealed++;
    }
    printf("[revealed_icons] %d\n", revealed);
    assert(revealed >= 2);
}

static void *first_icon_with_is_revealed(uint32_t kh);

/* A hovered icon's tooltip hangs from the top edge, whatever the height of
 * the screen. */
static void test_hovered_icon_tooltip_starts_at_the_top(void) {
    void *shown;

    start_game();
    shown = first_icon_with_is_revealed(2);
    as_structure_icon(shown)->header.is_hovered = 1;
    p8_camera(&graphics, 0, 0);
    p8_rectfill(&graphics, &memory, 0, 0, P8_SCREEN_WIDTH - 1, P8_SCREEN_HEIGHT - 1, 7);
    class_of(shown)->draw(shown, entity_pos(&as_structure_icon(shown)->header));
    assert(memory.screen[0][0] != 7);
}

static void *first_icon_with_is_revealed(uint32_t kh) {
    void *all[16];
    int n = icons(all, 16);
    int i;
    for (i = 0; i < n; i++) {
        if (is_revealed_state(all[i]) == kh) {
            return all[i];
        }
    }
    return NULL;
}

/* The refresh reveals once; the left press bumps the one counter every icon
 * shares and builds the held kind; the hit test tests the hit box around
 * `pos` in screen space. */
static void test_directed(void) {
    uint32_t hash = 2166136261u;
    void *shown;
    void *hidden;
    void *all_icons[16];
    vec_t cursor_before;
    vec_t cursor;
    vec_t pos_before;
    down_t made;
    p8_fixed_t first_id;
    vec_t icon_pos;
    int count;

    start_game();
    count = icons(all_icons, 16);
    shown = first_icon_with_is_revealed(2);
    hidden = first_icon_with_is_revealed(0);
    assert(shown != NULL && hidden != NULL);

    assert(shared_cursor(as_structure_icon(shown), &cursor_before));
    pos_before = as_structure_icon(shown)->header.pos;
    class_of(shown)->refresh(shown);
    assert(same_pos(as_structure_icon(shown)->header.pos, pos_before) && shared_cursor(as_structure_icon(shown), &cursor) && same_pos(cursor, cursor_before));
    class_of(hidden)->refresh(hidden);
    assert(is_revealed_state(hidden) == 0 && shared_cursor(as_structure_icon(hidden), &cursor) && same_pos(cursor, cursor_before));
    hash = fold_icon(hash, shown);
    hash = fold_icon(hash, hidden);
    hash = fold_shared(hash, hidden);

    first_id = shared_id_fixed(as_structure_icon(all_icons[0]));
    made = class_of(all_icons[0])->left_press(all_icons[0], (mouse_pos_t){0});
    assert(made.stop && entity_get(made.held) != NULL && planet_is_family(entity_get(made.held)));
    assert(strcmp(planet_id_or_null((const planet_t *)entity_get(made.held)), "s1") == 0);
    assert(shared_id_fixed(as_structure_icon(all_icons[count - 1])) == p8_fixed_add(first_id, p8_fixed_from_int(1)));
    hash = fold_string(hash, planet_id_or_null((const planet_t *)entity_get(made.held)));
    made = class_of(all_icons[count - 1])->left_press(all_icons[count - 1], (mouse_pos_t){0});
    assert(entity_get(made.held) != NULL && planet_is_family(entity_get(made.held)));
    assert(strcmp(planet_id_or_null((const planet_t *)entity_get(made.held)), "s2") == 0);
    assert(shared_id_fixed(as_structure_icon(all_icons[0])) == p8_fixed_add(first_id, p8_fixed_from_int(2)));
    hash = fold_string(hash, planet_id_or_null((const planet_t *)entity_get(made.held)));
    hash = fold_shared(hash, all_icons[3]);

    assert(pos_ref(&as_structure_icon(shown)->header) != NULL);
    icon_pos = *pos_ref(&as_structure_icon(shown)->header);
    assert(class_of(shown)->hit_test(shown, mouse_pos(vec_at(-500, -500), vec_add(icon_pos, vec_at(1, 1)))));
    assert(!class_of(shown)->hit_test(shown, mouse_pos(vec_add(icon_pos, vec_at(1, 1)), vec_add(icon_pos, vec_at(20, 20)))));
    assert(!class_of(hidden)->hit_test(hidden, mouse_pos(vec_at(0, 0), vec_add(icon_pos, vec_at(1, 1)))));
    {
        int dx;
        for (dx = -2; dx <= 11; dx++) {
            int hit = class_of(shown)->hit_test(shown, mouse_pos(vec_at(0, 0), vec_add(icon_pos, vec_at(dx, dx - 1))));
            hash = fnv(hash, (uint32_t)hit);
        }
    }
    check("directed", hash, DIRECTED_HASH);
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

static uint32_t draw_alone(void *icon) {
    p8_camera(&graphics, 0, 0);
    p8_cls(&memory);
    class_of(icon)->draw(icon, entity_pos(&as_structure_icon(icon)->header));
    return screen_hash();
}

/* Hidden: nothing; revealed: the sprite at `pos`; hovered (`is_hovered`):
 * the build cost and the tooltip as well. */
static void test_draw(void) {
    uint32_t hash = 2166136261u;
    uint32_t blank;
    uint32_t hidden_name;
    uint32_t shown_name;
    uint32_t hovered_name;
    void *shown;
    void *hidden;

    start_game();
    shown = first_icon_with_is_revealed(2);
    hidden = first_icon_with_is_revealed(0);
    p8_camera(&graphics, 0, 0);
    p8_cls(&memory);
    blank = screen_hash();

    hidden_name = draw_alone(hidden);
    assert(hidden_name == blank);
    as_structure_icon(hidden)->header.is_hovered = 1;
    assert(is_hovered_state(hidden) == 2);
    assert(draw_alone(hidden) == blank);
    shown_name = draw_alone(shown);
    assert(shown_name != blank);
    as_structure_icon(shown)->header.is_hovered = 1;
    assert(is_hovered_state(shown) == 2);
    hovered_name = draw_alone(shown);
    assert(hovered_name != shown_name);
    as_structure_icon(shown)->header.is_hovered = 0;
    assert(draw_alone(shown) == shown_name);

    hash = fnv(hash, hidden_name);
    hash = fnv(hash, shown_name);
    hash = fnv(hash, hovered_name);
    check("draw", hash, DRAW_HASH);
}

/* `required_tech` of each kind of fill_kind_table(), in its order: none for
 * the structure and the processor. */
static char *const kind_table_required_tech[7] = {NULL, NULL, "slipgates", "infraspace", "trade league", "void synthesis",
                                     "ascension"};

static void invent(char *rr) {
    assert(main_economy_typed() != NULL);
    pointer_list_add(&main_economy_typed()->invented, rr);
}

/* Per icon, in fill_kind_table() order: the refresh reveals it exactly when
 * its kind's `required_tech` is unlocked, and the draw shows the kind's
 * sprite, then when hovered its build cost off the targeting tooltip. */
static void test_kind_view(void) {
    uint32_t hash = 2166136261u;
    void *all[16];
    int n;
    int i;

    start_game();
    n = icons(all, 16);
    check_int("kind view icons", n, 7);
    for (i = 0; i < n; i++) {
        uint32_t before;
        class_of(all[i])->refresh(all[i]);
        before = is_revealed_state(all[i]);
        assert(before == (kind_table_required_tech[i] == NULL ? 2u : 0u));
        if (kind_table_required_tech[i] != NULL) {
            invent(kind_table_required_tech[i]);
            class_of(all[i])->refresh(all[i]);
        }
        assert(is_revealed_state(all[i]) == 2);
        hash = fnv(hash, before);
        hash = fold_icon(hash, all[i]);
    }
    hash = fold_shared(hash, all[0]);
    {
        /* The targeting tooltip prints at the main mouse, (0,0) here: its
         * label would land off screen. */
        assert(main_mouse_typed() != NULL);
        entity_set_pos(&main_mouse_typed()->header, vec_at(64, 64));
    }
    for (i = 0; i < n; i++) {
        uint32_t plain = draw_alone(all[i]);
        uint32_t hovered;
        as_structure_icon(all[i])->header.is_hovered = 1;
        hovered = draw_alone(all[i]);
        as_structure_icon(all[i])->header.is_hovered = 0;
        printf("[kind view] icon %d: 0x%08Xu / 0x%08Xu\n", i, (unsigned)plain, (unsigned)hovered);
        assert(hovered != plain);
        hash = fnv(hash, plain);
        hash = fnv(hash, hovered);
    }
    check("kind_view", hash, KIND_VIEW_HASH);
}

/* A second cart_start_game() keeps the cursor and the counter where the
 * last game left them; cart_init() puts the cursor back on the rest
 * position, and leaves the counter (only structure_icon_init() resets it). */
static void test_restart(void) {
    uint32_t h = 2166136261u;
    void *all[16];
    vec_t cursor_before;
    vec_t cursor;
    p8_fixed_t id;
    int n;
    int i;

    start_game();
    n = icons(all, 16);
    (void)class_of(all[0])->left_press(all[0], (mouse_pos_t){0});
    assert(shared_cursor(as_structure_icon(all[0]), &cursor_before));
    id = shared_id_fixed(as_structure_icon(all[0]));
    assert(id == p8_fixed_from_int(1));
    assert(!same_pos(cursor_before, as_structure_icon(all[0])->archetype->rest_cursor));

    new_game();
    n = icons(all, 16);
    check_int("restart icons", n, 7);
    assert(shared_id_fixed(as_structure_icon(all[0])) == id);
    assert(shared_cursor(as_structure_icon(all[0]), &cursor) && !same_pos(cursor, cursor_before));
    for (i = 0; i < n; i++) {
        if (is_revealed_state(all[i]) == 2) {
            assert(!same_pos(as_structure_icon(all[i])->header.pos, as_structure_icon(all[i])->archetype->rest_cursor));
        }
        h = fold_icon(h, all[i]);
    }
    h = fold_shared(h, all[0]);

    cart_init();
    assert(shared_cursor(as_structure_icon(all[0]), &cursor) && same_pos(cursor, as_structure_icon(all[0])->archetype->rest_cursor));
    assert(shared_id_fixed(as_structure_icon(all[0])) == id);
    h = fold_shared(h, all[0]);
    check("restart", h, RESTART_HASH);
}

static int is_a_structure_icon_struct(const void *e) {
    return e != NULL &&
           ((const entity_t *)e)->class == structure_icon_class();
}

static void test_reveals_step_the_cursor_in_place(void) {
    void *all[16];
    const entity_kind_t *unlocked = NULL;
    void *a;
    void *b;
    vec_t start = vec_make(p8_fixed_from_int(60), p8_fixed_from_int(70));
    vec_t cursor;
    size_t used;
    int n;
    int i;

    start_game();
    n = icons(all, 16);
    for (i = 0; i < n && unlocked == NULL; i++) {
        if (is_revealed_state(all[i]) == 2) {
            unlocked = as_structure_icon(all[i])->kind;
        }
    }
    assert(unlocked != NULL);
    a = build_icon(unlocked, tooltip_new(&heap_allocator, icon_tooltips_row(0)));
    b = build_icon(unlocked, tooltip_new(&heap_allocator, icon_tooltips_row(1)));
    structure_icon_set_cursor(start);
    used = game_zone_used();
    structure_icon_refresh(a);
    structure_icon_refresh(b);
    assert(game_zone_used() == used);
    assert(same_pos(as_structure_icon(a)->header.pos, start));
    assert(same_pos(as_structure_icon(b)->header.pos, vec_make(p8_fixed_from_int(50), p8_fixed_from_int(70))));
    assert(shared_cursor(as_structure_icon(a), &cursor) && same_pos(cursor, vec_make(p8_fixed_from_int(40), p8_fixed_from_int(70))));
    structure_icon_init();
    assert(!structure_icon_has_cursor());
    assert(same_pos(structure_icon_cursor(), vec_make(0, 0)));
}

/* Built by cart_start_game() or directly, an icon is a struct with its four
 * methods, and no other struct answers to the refresh. */
static void test_structure_icon_is_a_struct(void) {
    const entity_kind_t *kinds[7];
    void *all[16];
    void *built;
    void *others[4];
    int n;
    int i;

    start_game();
    fill_kind_table(kinds);
    n = icons(all, 16);
    built = build_icon(kinds[2], tooltip_new(&heap_allocator, icon_tooltips_row(2)));
    all[n++] = built;
    for (i = 0; i < n; i++) {
        assert(is_a_structure_icon_struct(all[i]));
        assert(class_of(all[i])->hit_test != NULL && class_of(all[i])->refresh != NULL && class_of(all[i])->draw != NULL &&
               class_of(all[i])->left_press != NULL);
        assert(class_of(all[i])->states[ENTITY_STATE_NORMAL] == NULL && class_of(all[i])->draw_overlay == NULL &&
               class_of(all[i])->new_year == NULL && class_of(all[i])->accept_drop == NULL);
    }

    {
        structure_icon_keys_t tooltip_only = {0};
        structure_icon_t *no_entity;
        p8_fixed_t first_id = shared_id_fixed(as_structure_icon(built));

        tooltip_only.tooltip = tooltip_new(&heap_allocator, NULL);
        no_entity = structure_icon_new(&tooltip_only);
        assert(is_a_structure_icon_struct(no_entity) && no_entity->kind == NULL);
        assert(shared_id_fixed(no_entity) == first_id);
        (void)class_of(no_entity)->left_press(&no_entity->header, (mouse_pos_t){0});
        assert(shared_id_fixed(as_structure_icon(built)) == p8_fixed_add(first_id, p8_fixed_from_int(1)));
    }

    others[0] = background_new();
    others[1] = modal_backdrop_new(NULL);
    others[2] = slipway_new(NULL);
    others[3] = wormhole_new();
    for (i = 0; i < 4; i++) {
        assert(class_of(others[i])->refresh == NULL);
    }
}

/* The held kind read through a struct without parent that carries only its
 * `required_tech`, `sprite`, `state` and `cost`. */
static void test_kind_view_is_a_struct(void) {
    static const int s[7] = {174, 100, 170, 78, 68, 66, 166};
    static const int od[7] = {15, 15, 10, 25, 20, 40, 60};
    const entity_kind_t *classes[7];
    void *all[16];
    structure_icon_kind_t view;
    int n;
    int i;

    start_game();
    fill_kind_table(classes);
    n = icons(all, 16);
    check_int("kind view struct icons", n, 7);
    for (i = 0; i < n; i++) {
        assert(as_structure_icon(all[i])->kind == classes[i]);
        view = structure_icon_kind_view(all[i]);
        assert(view.header.class == structure_icon_kind_view_class());
        if (kind_table_required_tech[i] == NULL) {
            assert(view.required_tech == NULL);
        } else {
            assert(view.required_tech != NULL && strcmp(view.required_tech, kind_table_required_tech[i]) == 0);
        }
        assert(view.has_sprite && view.sprite == p8_fixed_from_int(s[i]));
        assert(view.has_cost && view.cost == p8_fixed_from_int(od[i]));
        assert(view.state == ENTITY_STATE_PLACING);
    }

    {
        structure_icon_keys_t tooltip_only = {0};
        void *no_entity;

        tooltip_only.tooltip = tooltip_new(&heap_allocator, NULL);
        no_entity = structure_icon_new(&tooltip_only);
        view = structure_icon_kind_view(no_entity);
        assert(view.header.class == structure_icon_kind_view_class());
        assert(view.required_tech == NULL && !view.has_sprite && view.state == ENTITY_STATE_NONE && !view.has_cost);
    }
}

static void test_set_id_counter_is_read_back_and_bumped(void) {
    const entity_kind_t *kinds[7];
    void *built;

    start_game();
    fill_kind_table(kinds);
    built = build_icon(kinds[2], tooltip_new(&heap_allocator, icon_tooltips_row(2)));
    structure_icon_set_id_counter(p8_fixed_from_int(41));
    assert(shared_id_fixed(as_structure_icon(built)) == p8_fixed_from_int(41));
    (void)class_of(built)->left_press(built, (mouse_pos_t){0});
    assert(shared_id_fixed(as_structure_icon(built)) == p8_fixed_from_int(42));
}

static void set_id_counter_before_init(void) {
    structure_icon_shutdown();
    structure_icon_set_id_counter(p8_fixed_from_int(3));
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
        test();
        fflush(stdout);
        _exit(0);
    }
    assert(waitpid(child, &status, 0) == child);
    assert(WIFEXITED(status) && WEXITSTATUS(status) == 0);
}

static void aborts_in_child(void (*test)(void)) {
    pid_t child;
    int status;

    fflush(stdout);
    child = fork();
    assert(child >= 0);
    if (child == 0) {
        test();
        _exit(0);
    }
    assert(waitpid(child, &status, 0) == child);
    assert(WIFSIGNALED(status) && WTERMSIG(status) == SIGABRT);
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
    in_fresh_child(test_construction);
    in_fresh_child(test_start);
    in_fresh_child(test_revealed_icons_anchor_bottom_right);
    in_fresh_child(test_hovered_icon_tooltip_starts_at_the_top);
    in_fresh_child(test_directed);
    in_fresh_child(test_draw);
    in_fresh_child(test_restart);
    in_fresh_child(test_reveals_step_the_cursor_in_place);
    in_fresh_child(test_kind_view);
    in_fresh_child(test_kind_view_is_a_struct);
    in_fresh_child(test_structure_icon_is_a_struct);
    in_fresh_child(test_set_id_counter_is_read_back_and_bumped);
    aborts_in_child(set_id_counter_before_init);

    printf("test_structure_icon: all tests passed\n");
    return 0;
}
