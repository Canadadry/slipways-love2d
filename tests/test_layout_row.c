/* The laid-out row, read on its struct members, its pixels and its placed
 * items: direct constructions, the keys of
 * research_display_planet_resource_row_keys() and
 * research_project_layout_row_keys() laid out by a button,
 * layout_row_draw_at() through the class and on a row laid out in place,
 * and a built planet's panel in a game built directly: no events file. */

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
#include "engine/entity_id.h"
#include "engine/bootstrap.h"
#include "game/cart.h"
#include "engine/dispatch_context.h"
#include "engine/entity_registry.h"
#include "p8/fixed.h"
#include "p8/gfx.h"
#include "p8/input.h"
#include "game/layout.h"
#include "game/text_image.h"
#include "p8/draw_backend.h"
#include "p8/vram.h"
#include "game/draw_functions.h"
#include "game/planet.h"
#include "game/planet_display.h"
#include "game/planet_family.h"
#include "game/research.h"
#include "game/research_display.h"
#include "game/slipway.h"
#include "engine/entity_class.h"
#include "game/ui.h"
#include "game/ui_button.h"
#include "engine/vec.h"
#include "engine/zone.h"
#include "engine/allocator.h"
#include "engine/frame_zone.h"
#include "game/ui_zone.h"
#include "engine/game_zone.h"
#include "devtools/largest_slots.h"
#include "devtools/state_format.h"
#include "heap_allocator.h"

static const vec_t *pos_ref(const entity_t *e) {
    return entity_has_pos(e) ? &e->pos : NULL;
}

static const vec_t *map_pos_ref(const entity_t *e) {
    return entity_has_map_pos(e) ? &e->map_pos : NULL;
}


/* Project 12, a colony that needs two resources and exports one; project
 * 3, with an output and no inputs, which exports to any planet a slipway
 * joins it to. */
#define COLONY 12
#define EXPORTER 3

#define CONSTRUCTIONS_HASH 0x9DC2BF10u
#define PLANET_RESOURCE_ROW_HASH 0x14A29E3Fu
#define RESEARCH_PROJECT_LAYOUT_ROW_HASH 0xBF1572BEu
#define CALLS_HASH 0xAF82B163u
#define PANEL_HASH 0xA6746726u

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

static const layout_row_t *as_layout_row(const void *lq) {
    assert(lq != NULL &&
           ((const entity_t *)lq)->class == layout_row_class());
    return lq;
}

static int row_count(const void *lq) {
    return ((const layout_row_t *)lq)->placed_items_count;
}

static layout_row_item_t row_item(const void *lq, int i) {
    assert(i >= 0 && i < row_count(lq));
    return ((const layout_row_t *)lq)->placed_items[i];
}

/* The kind words the frozen hashes fold before a value: any other value
 * moves them. */
enum { FIXED_KIND = 0, STRING_KIND = 1, BOOL_KIND = 2, REF_KIND = 3 };

static uint32_t fold_fixed(uint32_t h, p8_fixed_t x) {
    return fnv(fnv(h, (uint32_t)FIXED_KIND), (uint32_t)x);
}

static uint32_t fold_bool(uint32_t h, int b) {
    return fnv(fnv(h, (uint32_t)BOOL_KIND), b ? 1u : 0u);
}

/* The string's bytes, then 0xFF. */
static uint32_t fold_string(uint32_t h, const char *s) {
    h = fnv(h, (uint32_t)STRING_KIND);
    for (; *s != '\0'; s++) {
        h = fnv(h, (uint32_t)(unsigned char)*s);
    }
    return fnv(h, 0xFFu);
}

/* The frozen hashes were taken when tuples were tables of decoded data:
 * these are the bytes a table folded for its tag and its two kinds of key. */
enum { VIEW_TAG = 0x9E5C0B7Au, VIEW_INDEX_KEY = 0 };

/* A tuple folded as the frozen hashes folded the table it was: its count,
 * then each scalar under its index, a nothing one as false. */
static uint32_t fold_tuple(uint32_t h, layout_row_tuple_t t) {
    int i;

    h = fnv(fnv(h, (uint32_t)REF_KIND), (uint32_t)VIEW_TAG);
    h = fnv(h, (uint32_t)t.count);
    for (i = 0; i < t.count; i++) {
        h = fnv(fnv(h, (uint32_t)VIEW_INDEX_KEY), (uint32_t)(i + 1));
        h = t.scalars[i].kind == LAYOUT_ROW_SCALAR_FIXED ? fold_fixed(h, t.scalars[i].value) : fold_bool(h, 0);
    }
    return h;
}

/* The frozen hashes folded the value a positional was, absent and false
 * apart: a NOTHING cell folds as absent when `nil` is set (the test's keys
 * left that key out), as false otherwise (no row `src/` builds holds an
 * absent key at the top level). */
static uint32_t fold_cell(uint32_t h, layout_row_cell_t c, int nil) {
    switch (c.kind) {
    case LAYOUT_ROW_CELL_TUPLE:
        return fold_tuple(h, c.as.tuple);
    case LAYOUT_ROW_CELL_FIXED:
        return fold_fixed(h, c.as.fixed);
    case LAYOUT_ROW_CELL_STRING:
        return fold_string(h, c.as.text);
    case LAYOUT_ROW_CELL_NOTHING:
        /* The bytes the frozen hashes fold for an absent and a false key,
         * which CONSTRUCTIONS_HASH holds apart. */
        return nil ? fnv(fnv(h, 3u), 0xDEADu) : fnv(fnv(h, 2u), 0u);
    default:
        return fold_bool(h, 1);
    }
}

/* `from`: the keys the row was laid out from, one placed item per cell. A
 * set bit of `nil_at` marks the key of that index as absent, not false. */
static uint32_t fold_positionals(uint32_t h, const void *lq, const layout_row_keys_t *from, unsigned nil_at) {
    int n = row_count(lq);
    int i;
    assert(from != NULL && from->count == n);
    assert((nil_at >> n) == 0 && "nil_at: a bit past the last key");
    h = fnv(h, (uint32_t)n);
    for (i = 0; i < n; i++) {
        layout_row_cell_t c = from->items[i];
        int nil = ((nil_at >> i) & 1u) != 0;
        assert((!nil || c.kind == LAYOUT_ROW_CELL_NOTHING) && "nil_at: a bit on a cell that is not NOTHING");
        assert((c.kind != LAYOUT_ROW_CELL_STRING ||
                (row_item(lq, i).kind == LAYOUT_ROW_ITEM_TEXT && row_item(lq, i).text != c.as.text &&
                 strcmp(row_item(lq, i).text, c.as.text) == 0)) &&
               "a string cell is placed as its own copy of the text");
        h = fold_cell(h, c, nil);
    }
    return h;
}

static int placed_items_count(const void *lq) {
    assert(as_layout_row(lq)->placed_items != NULL);
    return as_layout_row(lq)->placed_items_count;
}

static p8_fixed_t width_of(const void *lq) {
    return as_layout_row(lq)->width;
}

/* A number's or a boolean's kind word and value; an unset flag folds as
 * absent. */
static uint32_t fold_fixed_member(uint32_t h, int has, p8_fixed_t v) {
    return has ? fnv(fnv(h, (uint32_t)FIXED_KIND), (uint32_t)v) : fnv(h, 0xFFFFFFFFu);
}

static uint32_t fold_background(uint32_t h, const layout_row_t *lq) {
    if (lq->has_background && lq->background_is_bool) {
        return fnv(fnv(h, (uint32_t)BOOL_KIND), (uint32_t)(lq->background != 0));
    }
    return fold_fixed_member(h, lq->has_background, lq->background);
}

/* The color, background and score as values; the position as the vector it
 * points at. */
static uint32_t fold_named(uint32_t h, const void *row) {
    const layout_row_t *lq = as_layout_row(row);
    const vec_t *ra;

    h = fold_fixed_member(h, lq->has_color, lq->color);
    h = fold_background(h, lq);
    h = fold_fixed_member(h, lq->has_score, lq->score);
    ra = pos_ref(&lq->header);
    if (ra == NULL) {
        return fnv(h, 0xFFFFFFFFu);
    }
    h = fnv(h, (uint32_t)ra->x);
    return fnv(h, (uint32_t)ra->y);
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

static void check(const char *label, uint32_t got, uint32_t want) {
    printf("[%s] 0x%08Xu\n", label, (unsigned)got);
    fflush(stdout);
    assert(got == want);
}

static vec_t vec_xy(int x, int y) {
    return vec_make(p8_fixed_from_int(x), p8_fixed_from_int(y));
}

static mouse_pos_t mouse_world_only(vec_t world) {
    return (mouse_pos_t){.world = world, .has = MOUSE_POS_HAS_WORLD};
}

static layout_row_cell_t num(int n) {
    return layout_row_cell_fixed(p8_fixed_from_int(n));
}

static layout_row_cell_t str(const char *s) {
    return layout_row_cell_string(s);
}

/* A tuple of numbers, never freed. */
static layout_row_cell_t tuple_of(int n, const int *items) {
    data_row_t *t = calloc(1, sizeof *t);
    data_item_t *it = calloc((size_t)n, sizeof *it);
    int i;
    for (i = 0; i < n; i++) {
        it[i].number = items[i];
    }
    t->count = n;
    t->items = it;
    return layout_row_cell_row(t);
}

/* layout_row_draw_at() of the row on a cleared screen, at alignment 0, 0.5
 * and 1. */
static uint32_t fold_drawn(uint32_t h, const void *lq) {
    static const p8_fixed_t as[] = {0, P8_FIXED_LIT(0x00008000, 0.5), P8_FIXED_LIT(0x00010000, 1.0)};
    size_t n;

    for (n = 0; n < sizeof as / sizeof as[0]; n++) {
        p8_gfx_init(&graphics);
        p8_cls(&memory);
        layout_row_draw_at(&graphics, &memory, lq, vec_xy(64, 60), as[n]);
        h = fnv(h, screen_hash());
    }
    return h;
}

static uint32_t fold_row_from(uint32_t h, const void *lq, const layout_row_keys_t *from, unsigned nil_at) {
    h = fold_drawn(h, lq);
    h = fnv(h, (uint32_t)width_of(lq));
    h = fnv(h, (uint32_t)placed_items_count(lq));
    h = fold_positionals(h, lq, from, nil_at);
    return fold_named(h, lq);
}

/* Builds the row from the keys (NULL: none), folds it; `nil_at` as in
 * fold_positionals. */
static uint32_t fold_built_from(uint32_t h, const char *label, const layout_row_keys_t *k, unsigned nil_at) {
    static const layout_row_keys_t no_keys;
    const void *lq = layout_row_new(&heap_allocator, k);
    uint32_t one = fold_row_from(2166136261u, lq, k != NULL ? k : &no_keys, nil_at);

    assert(entity_has_pos((const entity_t *)lq) == (k != NULL && k->has_pos));

    printf("  %s: w=%d fs=%d n=%d 0x%08X\n", label, p8_fixed_flr_int(width_of(lq)), placed_items_count(lq), row_count(lq),
           (unsigned)one);
    return fnv(h, one);
}

/* The keys of that many positionals, copied from `items`, no named key. */
static layout_row_keys_t positionals(int n, const layout_row_cell_t *items) {
    layout_row_keys_t k = {0};
    int i;

    for (i = 0; i < n; i++) {
        layout_row_keys_append(&k, items[i]);
    }
    return k;
}

static layout_row_keys_t with_color(layout_row_keys_t k, int c) {
    k.has_color = 1;
    k.color = p8_fixed_from_int(c);
    return k;
}

/* The background a number, or false for a negative one. */
static layout_row_keys_t with_background(layout_row_keys_t k, int bg) {
    k.has_background = 1;
    k.background_is_bool = bg < 0;
    k.background = bg < 0 ? 0 : p8_fixed_from_int(bg);
    return k;
}

static layout_row_keys_t with_score(layout_row_keys_t k, int score) {
    k.has_score = 1;
    k.score = p8_fixed_from_int(score);
    return k;
}

static layout_row_keys_t with_pos(layout_row_keys_t k, vec_t at) {
    k.has_pos = 1;
    k.pos = at;
    return k;
}

/* Every kind of item row_measure_item() knows, the named keys each
 * constructor passes, no keys at all, the score row the score panel
 * passes, its hole already closed, and the stars' row in index
 * order. */
static void test_constructions(void) {
    uint32_t hash = 2166136261u;
    layout_row_keys_t keys;

    {
        layout_row_cell_t a[] = {str("hello")};
        keys = positionals(1, a);
        hash = fold_built_from(hash, "string", &keys, 0u);
    }
    {
        layout_row_cell_t a[] = {str("hello")};
        keys = with_color(positionals(1, a), 13);
        hash = fold_built_from(hash, "string color", &keys, 0u);
    }
    {
        layout_row_cell_t a[] = {num(12)};
        keys = positionals(1, a);
        hash = fold_built_from(hash, "sprite", &keys, 0u);
    }
    {
        static const int f[] = {12, 10, 3, 1};
        layout_row_cell_t a[1];
        a[0] = tuple_of(4, f);
        keys = positionals(1, a);
        hash = fold_built_from(hash, "sprite_id width sprite_palette_block spacing", &keys, 0u);
        assert(layout_row_new(&heap_allocator, &keys)->placed_items[0].sprite_palette_block == p8_fixed_from_int(3));
    }
    {
        data_row_t *qe = calloc(1, sizeof *qe);
        layout_row_cell_t a[1];
        qe->has_resource = 1;
        qe->resource = p8_fixed_from_int(14);
        a[0] = layout_row_cell_row(qe);
        keys = positionals(1, a);
        hash = fold_built_from(hash, "resource", &keys, 0u);
    }
    {
        layout_row_cell_t a[] = {layout_row_cell_nothing()};
        keys = positionals(1, a);
        hash = fold_built_from(hash, "false", &keys, 0u);
    }
    {
        layout_row_cell_t a[] = {layout_row_cell_nothing()};
        keys = positionals(1, a);
        hash = fold_built_from(hash, "nothing", &keys, 1u << 0);
    }
    {
        static const int f[] = {13, 8};
        layout_row_cell_t a[] = {str("a"), num(12), layout_row_cell_nothing(), layout_row_cell_nothing(), str("bc"),
                               layout_row_cell_nothing(), num(40)};
        a[2] = tuple_of(2, f);
        keys = with_color(positionals(7, a), 9);
        hash = fold_built_from(hash, "mixed color", &keys, 1u << 5);
    }
    hash = fold_built_from(hash, "NULL", NULL, 0u);
    {
        layout_row_cell_t a[] = {str("score"), str("")};
        keys = with_color(with_pos(with_score(positionals(2, a), 0), vec_xy(64, 50)), 5);
        hash = fold_built_from(hash, "hole", &keys, 0u);
    }
    {
        layout_row_cell_t a[] = {num(19), str("x"), num(21), num(20), num(22)};
        keys = with_pos(with_score(positionals(5, a), 3), vec_xy(64, 40));
        hash = fold_built_from(hash, "disorder", &keys, 0u);
    }
    {
        layout_row_cell_t a[] = {str("3"), num(12)};
        keys = with_color(with_background(positionals(2, a), 8), 7);
        hash = fold_built_from(hash, "background", &keys, 0u);
    }
    {
        layout_row_cell_t a[] = {str("4")};
        keys = with_background(positionals(1, a), -1);
        hash = fold_built_from(hash, "background false", &keys, 0u);
    }
    check("constructions", hash, CONSTRUCTIONS_HASH);
}

static void *project(int k) {
    assert(research_projects_at(k) != NULL);
    return research_projects_at(k);
}

/* research_display_planet_resource_row_keys() on the inputs and output of
 * three projects, over a spread of amount, used and spacing that includes an
 * amount above 5, the branch no replay reaches; then with no inputs. */
static void test_planet_resource_row(void) {
    static const int ks[] = {1, 7, 20};
    static const int jqs[] = {1, 2, 4, 6, 9};
    uint32_t h = 2166136261u;
    size_t a;
    size_t b;
    p8_fixed_t o;

    for (a = 0; a < sizeof ks / sizeof ks[0]; a++) {
        const research_project_t *p = project(ks[a]);
        const pointer_list_t *is = &p->inputs;
        assert(p->has_output);
        o = p->output;
        for (b = 0; b < sizeof jqs / sizeof jqs[0]; b++) {
            layout_row_keys_t keys;
            const void *lq;

            frame_zone_reset();
            assert(research_display_planet_resource_row_keys(is, 1, o, p8_fixed_from_int(jqs[b]), p8_fixed_from_int((int)b % 3),
                                                             p8_fixed_from_int((int)b % 2), &keys));
            lq = layout_row_new(frame_zone(), &keys);
            assert(as_layout_row(lq)->has_color == (jqs[b] > 5));
            if (jqs[b] > 5) {
                assert(as_layout_row(lq)->color == p8_fixed_from_int(13));
            }
            h = fold_row_from(h, lq, &keys, 0u);
        }
    }
    {
        layout_row_keys_t keys;
        const void *lq;

        assert(research_display_planet_resource_row_keys(NULL, 1, o, p8_fixed_from_int(2), 0, p8_fixed_from_int(1), &keys));
        lq = layout_row_new(frame_zone(), &keys);
        h = fold_row_from(h, lq, &keys, 0u);
    }
    {
        layout_row_keys_t untouched = {.count = -1};

        assert(!research_display_planet_resource_row_keys(NULL, 0, o, p8_fixed_from_int(1), 0, 0, &untouched));
        assert(untouched.count == -1);
    }
    check("planet_resource_row", h, PLANET_RESOURCE_ROW_HASH);
}

static void start_game(void);


static int g_copies;
static const p8_draw_backend_t *g_software;

static void count_image_draw(const p8_graphics_t *gfx, p8_vram_t *mem, int slot, int x, int y) {
    g_copies++;
    g_software->image_draw(gfx, mem, slot, x, y);
}

static void install_copy_counter(void) {
    static p8_draw_backend_t counting;
    g_software = p8_gfx_software_backend();
    counting = *g_software;
    counting.image_draw = count_image_draw;
    g_copies = 0;
    p8_draw_backend_set(&counting);
}

/* research_project_layout_row_keys() holds the resource row, then the row
 * tail and the name: a button lays all of them out once. Then two buttons
 * with a side tooltip, its background a number and false. */
static void test_research_project_layout_row_keys(void) {
    static const int project_ranks[] = {1, 5, 12};
    uint32_t hash = 2166136261u;
    size_t allocated;

    start_game();
    for (allocated = 0; allocated < sizeof project_ranks / sizeof project_ranks[0]; allocated++) {
        research_project_t *p = project(project_ranks[allocated]);
        layout_row_keys_t keys;
        button_keys_t spec = {0};
        button_t *button;
        const void *row;

        assert(research_project_layout_row_keys(p, &keys));
        assert(keys.items[keys.count - 1].kind == LAYOUT_ROW_CELL_STRING && strcmp(keys.items[keys.count - 1].as.text, p->name) == 0);

        spec.layout_row_keys = keys;
        button = button_new(&spec);
        row = button->layout_row;
        assert(row != NULL && placed_items_count(row) == keys.count);
        hash = fold_row_from(hash, row, &keys, 0u);
        hash = fnv(hash, (uint32_t)button->width);
        p8_gfx_init(&graphics);
        p8_cls(&memory);
        button_draw(&graphics, &memory, button, vec_xy(10, 50));
        hash = fnv(hash, screen_hash());
    }

    install_copy_counter();
    for (allocated = 0; allocated < 2; allocated++) {
        layout_row_cell_t layout_row_items[] = {str("ab"), num(12)};
        layout_row_cell_t h_items[] = {str(allocated == 0 ? "3" : "12")};
        layout_row_keys_t row_keys = with_color(positionals(2, layout_row_items), 9);
        layout_row_keys_t h_t = with_background(with_color(positionals(1, h_items), 10), allocated == 0 ? 8 : -1);
        button_keys_t spec = {0};
        button_t *button;
        const void *h_;

        spec.layout_row_keys = row_keys;
        spec.has_side_tooltip = 1;
        spec.has_side_tooltip_row = 1;
        spec.side_tooltip_row = h_t;
        button = button_new(&spec);
        hash = fold_row_from(hash, button->layout_row, &row_keys, 0u);
        h_ = button->side_tooltip;
        assert(h_ != NULL);
        hash = fold_row_from(hash, h_, &h_t, 0u);
        hash = fnv(hash, (uint32_t)button->width);
        p8_gfx_init(&graphics);
        p8_cls(&memory);
        button_draw(&graphics, &memory, button, vec_xy(10, 50));
        hash = fnv(hash, screen_hash());
    }
    p8_draw_backend_set(NULL);
    assert(g_copies == 2 && "a button, its row text and its side tooltip's text included, is one image copy");
    check("research_project_layout_row_keys", hash, RESEARCH_PROJECT_LAYOUT_ROW_HASH);
}

static uint32_t drawn_by_draw_at(const void *lq, vec_t at, p8_fixed_t a) {
    p8_gfx_init(&graphics);
    p8_cls(&memory);
    layout_row_draw_at(&graphics, &memory, lq, at, a);
    return screen_hash();
}

/* The class's draw_at as a planet's draw calls it, and the same keys laid
 * out in place (as a message box holds its layout-row article), each
 * drawing what layout_row_draw_at() draws. */
static void test_calls(void) {
    static const int tuple_values[] = {13, 8};
    layout_row_cell_t cells[] = {str("hi"), num(12), layout_row_cell_nothing()};
    layout_row_keys_t keys;
    void *layout_row;
    layout_row_t in_place;
    uint32_t h = 2166136261u;
    uint32_t want;

    cells[2] = tuple_of(2, tuple_values);
    keys = with_color(positionals(3, cells), 11);
    layout_row = layout_row_new(&heap_allocator, &keys);

    want = drawn_by_draw_at(layout_row, vec_xy(64, 30), P8_FIXED_LIT(0x00008000, 0.5));
    h = fnv(h, want);
    p8_gfx_init(&graphics);
    p8_cls(&memory);
    layout_row_class()->draw_at(layout_row, vec_xy(64, 30), P8_FIXED_LIT(0x00008000, 0.5));
    assert(screen_hash() == want);

    layout_row_init(&in_place, &heap_allocator, &keys);
    assert(drawn_by_draw_at(&in_place, vec_xy(64, 30), P8_FIXED_LIT(0x00008000, 0.5)) == want);

    want = drawn_by_draw_at(layout_row, vec_xy(20, 90), 0);
    h = fnv(h, want);
    p8_gfx_init(&graphics);
    p8_cls(&memory);
    layout_row_draw_at_default(&graphics, &memory, layout_row, vec_xy(20, 90));
    assert(screen_hash() == want);

    p8_gfx_init(&graphics);
    p8_cls(&memory);
    want = screen_hash();
    layout_row_draw_at(&graphics, &memory, NULL, vec_xy(20, 90), 0);
    assert(screen_hash() == want);
    check("calls", h, CALLS_HASH);
}

BUMP_ZONE_STORAGE(panel_cells, 4096);

/* Built from eight cells in `zone`, and laid out in place from the same
 * cells with its placed items in `zone`. */
static uint32_t drawn_from_eight_cells(const allocator_t *zone) {
    layout_row_cell_t a[] = {str("ab"), num(12), str("cd"), num(17), str("cd"), num(19), str("cd"), num(21)};
    layout_row_keys_t t = with_color(positionals(8, a), 9);
    layout_row_t *lq = layout_row_new(zone, &t);
    layout_row_t in_place;

    layout_row_init(&in_place, zone, &t);
    assert(row_count(&in_place) == 8 && in_place.width == lq->width);
    assert(drawn_by_draw_at(&in_place, vec_xy(64, 60), P8_FIXED_LIT(0x00008000, 0.5)) ==
           drawn_by_draw_at(lq, vec_xy(64, 60), P8_FIXED_LIT(0x00008000, 0.5)));
    return drawn_by_draw_at(lq, vec_xy(64, 60), P8_FIXED_LIT(0x00008000, 0.5));
}

/* A row built from all eight cells draws the same in a bump zone and the
 * heap, allocated or laid out in place. */
static void test_row_of_eight_cells_in_bump_zone(void) {
    bump_zone_t bump;
    allocator_t handle;
    uint32_t want = drawn_from_eight_cells(&heap_allocator);

    bump_zone_init(&bump, "test-panel", panel_cells, 4096);
    handle = allocator_from_bump_zone(&bump);
    assert(drawn_from_eight_cells(&handle) == want);
}

BUMP_ZONE_STORAGE(stale_cells, 1024);

/* A text, a sprite and a nothing laid out in zone storage first filled
 * with `pattern`: the placed items, copied out of the zone. */
static void placed_after_fill(unsigned char pattern, layout_row_item_t *out) {
    layout_row_cell_t cells[3];
    layout_row_keys_t keys;
    bump_zone_t bump;
    allocator_t handle;
    layout_row_t *lq;

    cells[0] = str("hi");
    cells[1] = num(12);
    cells[2] = layout_row_cell_nothing();
    keys = positionals(3, cells);
    memset(stale_cells, pattern, sizeof stale_cells);
    bump_zone_init(&bump, "test-stale", stale_cells, 1024);
    handle = allocator_from_bump_zone(&bump);
    lq = layout_row_new(&handle, &keys);
    assert(lq->placed_items_count == 3);
    memcpy(out, lq->placed_items, 3 * sizeof *out);
}

/* Placed items hold no stale zone bytes: the same keys laid out over two
 * different garbage patterns give byte-identical items. */
static void test_placed_items_ignore_stale_zone_bytes(void) {
    layout_row_item_t over_a5[3];
    layout_row_item_t over_5a[3];

    placed_after_fill(0xA5, over_a5);
    placed_after_fill(0x5A, over_5a);
    assert(memcmp(over_a5, over_5a, sizeof over_a5) == 0);
}

/* A game started as cart_start_game() starts it, with no difficulty
 * picked: a NULL difficulty reads as difficulty 0. */
static void start_game(void) {
    p8_input_init(&input);
    dispatch_context_set_input(&input);
    cart_init();
    cart_flush_exit_queue();
    cart_start_game(NULL);
}

static planet_t *as_planet(void *e) {
    assert(planet_is_family(e));
    return e;
}

static vec_t pos_of(void *e) {
    const vec_t *p = map_pos_ref(&as_planet(e)->header);
    assert(p != NULL);
    return *p;
}

static p8_fixed_t dist2(vec_t a, vec_t b) {
    vec_t d = vec_sub(a, b);
    return p8_fixed_add(p8_fixed_mul(d.x, d.x), p8_fixed_mul(d.y, d.y));
}

/* A generated planet colonized by COLONY, its nearest one by EXPORTER, the
 * two joined by a slipway paid by its drop as a drag pays it (the slipway's
 * drag reads the hovered planet off the main ui's `hovered`), left to
 * settle; then the colony's panel, planet_inspection_panel(), up and drawn. */
static void built_planet_panel(void) {
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
    planet_inspection_panel(qu);
    cart_update60();
    cart_draw();
}

/* A button's layout_row or side_tooltip, NULL when absent or false. */
static uint32_t fold_button_row(uint32_t h, const layout_row_t *lq) {
    int i;

    if (lq == NULL) {
        return fnv(h, 0xDEADu);
    }
    h = fnv(h, (uint32_t)width_of(lq));
    h = fold_fixed_member(h, lq->has_color, lq->color);
    h = fold_background(h, lq);
    h = fnv(h, (uint32_t)row_count(lq));
    for (i = 0; i < row_count(lq); i++) {
        h = fnv(h, (uint32_t)row_item(lq, i).kind);
    }
    return h;
}

/* With a built planet's panel up, every button of the main ui's
 * ephemerals: its layout_row and side_tooltip; then the screen. */
static void test_planet_panel(void) {
    uint32_t h = 2166136261u;
    const entity_id_list_t *le;
    int buttons = 0;
    int i;

    built_planet_panel();
    le = &main_ui()->ephemerals;
    for (i = 0; i < le->count; i++) {
        const void *button = entity_id_list_at(le, i);
        if (button != NULL &&
            ((const entity_t *)button)->class == button_class()) {
            buttons++;
            h = fold_button_row(h, ((const button_t *)button)->layout_row);
            h = fold_button_row(h, ((const button_t *)button)->side_tooltip);
        }
    }
    printf("  panel buttons: %d\n", buttons);
    assert(buttons > 0);
    check("panel", fnv(h, screen_hash()), PANEL_HASH);
}

/* The row of the main ui's ephemerals whose first item is `head`, NULL when
 * none. */
static const layout_row_t *panel_row(const char *head) {
    const entity_id_list_t *le = &main_ui()->ephemerals;
    int i;

    for (i = 0; i < le->count; i++) {
        const entity_t *button = entity_id_list_at(le, i);
        const layout_row_t *lq;
        layout_row_item_t v;

        assert(button != NULL && button->class == button_class() && "the main ui's ephemerals hold buttons only");
        lq = ((const button_t *)button)->layout_row;

        if (lq != NULL && row_count(lq) > 0) {
            v = row_item(lq, 0);
            if (v.kind == LAYOUT_ROW_ITEM_TEXT && strcmp(v.text, head) == 0) {
                return lq;
            }
        }
    }
    return NULL;
}

/* A planet whose every want arrives lists each under "wants more:", and
 * `satisfied` times under "receives:", as the resource row
 * row_measure_item() reads: a want of a resource draws it 5 wide, a want of
 * none nothing, 6 wide. */
static void test_inspection_panel_wants_more(void) {
    p8_fixed_t ore = p8_fixed_from_int(21);
    need_t *a;
    need_t *b;
    pointer_list_t *jy = malloc(sizeof *jy);
    planet_t *planet;
    const layout_row_t *wants;
    const layout_row_t *receives;

    start_game();
    a = need_new(game_zone(), (resource_t){.kind = RESOURCE_FIXED, .number = ore});
    b = need_new(game_zone(), (resource_t){.kind = RESOURCE_NIL});
    a->has_satisfied = 1;
    a->satisfied = p8_fixed_from_int(1);
    b->has_satisfied = 1;
    b->satisfied = p8_fixed_from_int(2);
    pointer_list_init(jy, &heap_allocator);
    pointer_list_add(jy, a);
    pointer_list_add(jy, b);
    planet = planet_new(NULL);
    planet->needs = *jy;
    planet->has |= PLANET_HAS_NEEDS;
    planet->has_development_tier = 1;
    planet->development_tier = p8_fixed_from_int(1);
    entity_set_pos(&planet->header, vec_xy(64, 64));
    planet_inspection_panel(planet);

    wants = panel_row("wants more:");
    receives = panel_row("receives:");
    assert(wants != NULL && receives != NULL && panel_row("needs:") == NULL);
    assert(row_count(wants) == 4 && row_count(receives) == 5);
    assert(row_item(wants, 1).kind == LAYOUT_ROW_ITEM_SPRITE && row_item(wants, 1).sprite_id == ore);
    assert(row_item(wants, 2).kind == LAYOUT_ROW_ITEM_NONE);
    assert(row_item(wants, 2).offset.x == p8_fixed_add(row_item(wants, 1).offset.x, p8_fixed_from_int(7)));
    assert(row_item(receives, 1).kind == LAYOUT_ROW_ITEM_SPRITE && row_item(receives, 1).sprite_id == ore);
    assert(row_item(receives, 2).kind == LAYOUT_ROW_ITEM_NONE && row_item(receives, 3).kind == LAYOUT_ROW_ITEM_NONE);
    assert(row_item(receives, 3).offset.x == p8_fixed_add(row_item(receives, 2).offset.x, p8_fixed_from_int(8)));
    printf("  wants more: w=%d, receives: w=%d\n", p8_fixed_flr_int(wants->width), p8_fixed_flr_int(receives->width));
    assert(wants->width == p8_fixed_from_int(64) && receives->width == p8_fixed_from_int(64));
}

/* A planet whose production resource is luxury has no "wants more:" row,
 * though every want arrives. */
static void test_inspection_panel_w_maker_wants_no_more(void) {
    p8_fixed_t ore = p8_fixed_from_int(21);
    need_t *a;
    pointer_list_t *jy = malloc(sizeof *jy);
    production_t *kw = calloc(1, sizeof *kw);
    planet_t *planet;
    int r_w;

    start_game();
    a = need_new(game_zone(), (resource_t){.kind = RESOURCE_FIXED, .number = ore});
    assert(resource_code_lookup("w", &r_w));
    kw->has_resource = 1;
    kw->resource = p8_fixed_from_int(r_w);
    a->has_satisfied = 1;
    a->satisfied = p8_fixed_from_int(1);
    pointer_list_init(jy, &heap_allocator);
    pointer_list_add(jy, a);
    planet = planet_new(NULL);
    planet->needs = *jy;
    planet->has |= PLANET_HAS_NEEDS;
    planet->production = *kw;
    planet->has |= PLANET_HAS_PRODUCTION;
    planet->has_development_tier = 1;
    planet->development_tier = p8_fixed_from_int(1);
    entity_set_pos(&planet->header, vec_xy(64, 64));
    planet_inspection_panel(planet);

    assert(panel_row("wants more:") == NULL && panel_row("needs:") == NULL);
}

/* A laboratory (a structure) lists only its last want under "wants more:". */
static void test_inspection_panel_lab_wants_its_last(void) {
    need_t *a;
    need_t *b;
    pointer_list_t *jy = malloc(sizeof *jy);
    planet_t *lab;
    const layout_row_t *wants;

    start_game();
    a = need_new(game_zone(), (resource_t){.kind = RESOURCE_NIL});
    b = need_new(game_zone(), (resource_t){.kind = RESOURCE_NIL});
    a->has_satisfied = 1;
    a->satisfied = p8_fixed_from_int(1);
    b->has_satisfied = 1;
    b->satisfied = p8_fixed_from_int(1);
    pointer_list_init(jy, &heap_allocator);
    pointer_list_add(jy, a);
    pointer_list_add(jy, b);
    lab = structure_new(NULL);
    lab->needs = *jy;
    lab->has |= PLANET_HAS_NEEDS;
    lab->has_development_tier = 1;
    lab->development_tier = p8_fixed_from_int(1);
    entity_set_pos(&lab->header, vec_xy(64, 64));
    planet_inspection_panel(lab);

    wants = panel_row("wants more:");
    assert(wants != NULL && row_count(wants) == 3 && row_item(wants, 1).kind == LAYOUT_ROW_ITEM_NONE);
    assert(row_item(wants, 2).offset.x == p8_fixed_add(row_item(wants, 1).offset.x, p8_fixed_from_int(8)));
}

static void must_abort(void (*write)(void *), void *e) {
    pid_t child;
    int status;

    fflush(stdout);
    child = fork();
    assert(child >= 0);
    if (child == 0) {
        assert(freopen("/dev/null", "w", stderr) != NULL);
        write(e);
        _exit(0);
    }
    assert(waitpid(child, &status, 0) == child);
    assert(WIFSIGNALED(status) && WTERMSIG(status) == SIGABRT);
}

/* The argument is a row of the data tables with a key no member takes, or
 * more items than the keys hold. */
static void keys_of_row(void *e) {
    (void)layout_row_keys_of_row(e);
}

static void append_past_the_cap(void *e) {
    layout_row_keys_t *keys = e;

    layout_row_keys_append(keys, num(1));
}

static void draw_at_on_other(void *e) {
    layout_row_draw_at(&graphics, &memory, e, vec_xy(0, 0), 0);
}

/* A row is a struct whose class has draw_at, no other method and no state.
 * A row of the data tables gives its keys but for a draw_function, a
 * resource or more items than the keys hold, and layout_row_draw_at()
 * refuses anything that is not a row. A row laid out in place holds its
 * own placed items and texts. */
static void test_layout_row_is_a_struct(void) {
    static const data_item_t pair_items[] = {{.number = 24}, {.number = 5}};
    static const data_row_t pair = {.count = 2, .items = pair_items};
    static const data_item_t three[] = {{.text = "a"}, {.is_fixed = 1, .fixed = 0x8000}, {.row = &pair}};
    static const data_row_t with_fn = {.count = 1, .items = three, .draw_function = DRAW_FUNCTION_DRAW_ROW};
    static const data_row_t with_resource = {.count = 1, .items = three, .has_resource = 1, .resource = 0x70000};
    static const data_item_t many[LAYOUT_ROW_KEYS_CAP + 1] = {{.number = 1}};
    static const data_row_t too_long = {.count = LAYOUT_ROW_KEYS_CAP + 1, .items = many};
    static const data_row_t nested = {.count = 2, .items = three + 1, .has_color = 1, .color = 7};
    const entity_class_t *row_class = layout_row_class();
    layout_row_cell_t cells[] = {str("hi"), num(12)};
    layout_row_keys_t t = with_color(positionals(2, cells), 9);
    layout_row_keys_t full = {0};
    layout_row_t *lq;
    layout_row_t in_place;
    int i;

    lq = layout_row_new(&heap_allocator, &t);
    assert(lq != NULL && lq->header.class == row_class);
    assert(row_class->draw_at != NULL);
    assert(row_class->draw == NULL && row_class->draw_overlay == NULL && row_class->hit_test == NULL && row_class->new_year == NULL && row_class->refresh == NULL &&
           row_class->left_press == NULL && row_class->accept_drop == NULL && row_class->drag == NULL && row_class->drop == NULL &&
           row_class->attach_slipway == NULL && row_class->offer_trade == NULL && row_class->spawn_particle == NULL);
    for (i = 0; i < STATE_COUNT; i++) {
        assert(row_class->states[i] == NULL);
    }
    assert(layout_row_ready());
    assert(lq->has_color && lq->color == p8_fixed_from_int(9));
    assert(!lq->has_background);
    layout_row_init(&in_place, &heap_allocator, &t);
    assert(in_place.header.class == row_class && in_place.placed_items != lq->placed_items && in_place.placed_items[0].text != lq->placed_items[0].text);
    assert(strcmp(in_place.placed_items[0].text, "hi") == 0 && in_place.placed_items[1].sprite_id == p8_fixed_from_int(12));
    assert(in_place.has_color && in_place.color == p8_fixed_from_int(9) && in_place.placed_items_count == 2 && in_place.width == lq->width);

    t = layout_row_keys_of_row(&nested);
    assert(t.count == 2 && t.has_color && t.color == p8_fixed_from_int(7));
    assert(!t.has_background && !t.has_score && !t.has_pos);
    assert(t.items[0].kind == LAYOUT_ROW_CELL_FIXED && t.items[0].as.fixed == 0x8000);
    assert(t.items[1].kind == LAYOUT_ROW_CELL_TUPLE && t.items[1].as.tuple.count == 2 && t.items[1].as.tuple.scalars[0].value == p8_fixed_from_int(24));
    for (i = 0; i < LAYOUT_ROW_KEYS_CAP; i++) {
        layout_row_keys_append(&full, num(i));
    }
    assert(full.count == LAYOUT_ROW_KEYS_CAP && full.items[LAYOUT_ROW_KEYS_CAP - 1].as.fixed == p8_fixed_from_int(LAYOUT_ROW_KEYS_CAP - 1));

    start_game();
    must_abort(keys_of_row, (void *)&with_fn);
    must_abort(keys_of_row, (void *)&with_resource);
    must_abort(keys_of_row, (void *)&too_long);
    must_abort(append_past_the_cap, &full);
    must_abort(draw_at_on_other, main_ui());
}

/* A tuple cell copies its row's scalars: overwriting the row afterwards
 * changes neither the cell nor what a row laid out from it draws. A row of
 * a resource reads as {resource, 5}, a NULL row as a nothing cell. */
static void test_tuple_cell_copies_its_row(void) {
    data_item_t items[] = {{.number = 13}, {.number = 8}, {.number = 3}, {.is_false = 1}};
    data_row_t row = {.count = 4, .items = items};
    data_row_t resource_row = {.has_resource = 1, .resource = p8_fixed_from_int(21)};
    layout_row_cell_t cell = layout_row_cell_row(&row);
    layout_row_cell_t of_resource = layout_row_cell_row(&resource_row);
    layout_row_keys_t keys = positionals(1, &cell);
    uint32_t want = drawn_by_draw_at(layout_row_new(&heap_allocator, &keys), vec_xy(64, 60), 0);

    memset(items, 0xA5, sizeof items);
    memset(&row, 0xA5, sizeof row);
    memset(&resource_row, 0xA5, sizeof resource_row);
    assert(cell.kind == LAYOUT_ROW_CELL_TUPLE && cell.as.tuple.count == 4);
    assert(cell.as.tuple.scalars[0].kind == LAYOUT_ROW_SCALAR_FIXED && cell.as.tuple.scalars[0].value == p8_fixed_from_int(13));
    assert(cell.as.tuple.scalars[2].kind == LAYOUT_ROW_SCALAR_FIXED && cell.as.tuple.scalars[2].value == p8_fixed_from_int(3));
    assert(cell.as.tuple.scalars[3].kind == LAYOUT_ROW_SCALAR_NOTHING);
    assert(drawn_by_draw_at(layout_row_new(&heap_allocator, &keys), vec_xy(64, 60), 0) == want);
    assert(of_resource.kind == LAYOUT_ROW_CELL_TUPLE && of_resource.as.tuple.count == 2);
    assert(of_resource.as.tuple.scalars[0].value == p8_fixed_from_int(21) && of_resource.as.tuple.scalars[1].value == p8_fixed_from_int(5));
    assert(layout_row_cell_row(NULL).kind == LAYOUT_ROW_CELL_NOTHING);
}

/* A row copies its keys: overwriting them after the row is built changes
 * neither its position nor what it draws. */
static void test_row_keeps_nothing_of_its_keys(void) {
    layout_row_cell_t cells[] = {str("hi"), num(12)};
    layout_row_keys_t keys = with_pos(with_color(positionals(2, cells), 9), vec_xy(30, 40));
    layout_row_t *lq = layout_row_new(&heap_allocator, &keys);
    uint32_t want = drawn_by_draw_at(lq, vec_xy(64, 60), 0);

    memset(&keys, 0xA5, sizeof keys);
    assert(entity_has_pos(&lq->header) && entity_pos(&lq->header).x == p8_fixed_from_int(30) && entity_pos(&lq->header).y == p8_fixed_from_int(40));
    assert(row_count(lq) == 2 && drawn_by_draw_at(lq, vec_xy(64, 60), 0) == want);
}

/* A row's serial names its contents: every fill, by layout_row_new or in place, gets a new one. */
static void test_every_filled_row_gets_its_own_serial(void) {
    layout_row_cell_t cells[] = {str("hi"), num(12)};
    layout_row_keys_t keys = positionals(2, cells);
    layout_row_t *first = layout_row_new(&heap_allocator, &keys);
    layout_row_t *second = layout_row_new(&heap_allocator, &keys);
    layout_row_t in_place;
    uint32_t before;

    assert(first->serial != 0 && second->serial != 0 && first->serial != second->serial);
    layout_row_init(&in_place, &heap_allocator, &keys);
    before = in_place.serial;
    assert(before != 0 && before != first->serial && before != second->serial);
    layout_row_init(&in_place, &heap_allocator, NULL);
    assert(in_place.serial != 0 && in_place.serial != before && "a row filled again in place is a new row");
}

static void tuple_of_row(void *e) {
    (void)layout_row_cell_row(e);
}

/* A tuple holds numbers only: a row with a text, a nested row or more than
 * four items is refused. */
static void test_tuple_refuses_text_and_rows(void) {
    static const data_item_t text_items[] = {{.text = "tuple"}, {.number = 5}};
    static const data_row_t with_text = {.count = 2, .items = text_items};
    static const data_item_t nested_items[] = {{.number = 12}, {.row = &with_text}};
    static const data_row_t with_row = {.count = 2, .items = nested_items};
    static const data_item_t five_items[] = {{.number = 1}, {.number = 2}, {.number = 3}, {.number = 4}, {.number = 5}};
    static const data_row_t too_long = {.count = 5, .items = five_items};

    must_abort(tuple_of_row, (void *)&with_text);
    must_abort(tuple_of_row, (void *)&with_row);
    must_abort(tuple_of_row, (void *)&too_long);
}

/* Each scenario runs in a child forked right after cart_load(), as a fresh
 * process would start it: the random state is the same for all. */
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

static void print_line(void *context, const char *line) {
    (void)context;
    fputs(line, stdout);
}

static void test_planet_resource_row_largest_row_fits_a_slot(void) {
    const state_sink_t sink = {print_line, NULL};

    largest_slots_run();
    largest_slots_format(&sink);
    assert(ui_zone_max_bytes(SLOT_KIND_ROW) <= ui_zone_slot_size(SLOT_KIND_ROW));
}

static void background_stripes(p8_vram_t *mem) {
    for (int y = 0; y < P8_SCREEN_HEIGHT; y++) {
        for (int x = 0; x < P8_SCREEN_WIDTH; x++) {
            mem->screen[y][x] = (unsigned char)(1 + (x * 5 + y) % 15);
        }
    }
}

/* A row of text, a sprite, a sprite tuple and text draws through the text image what layout_row_draw_at
 * draws, at every alignment, fractional and negative positions and under a camera. */
static void test_the_text_image_sibling_draws_like_draw_at(void) {
    static const int tuple_values[] = {13, 8};
    static const p8_fixed_t xs[] = {P8_FIXED_LIT(0x00400000, 64), P8_FIXED_LIT(0xFFF60000, -10),
                                    P8_FIXED_LIT(0x00404CCD, 64.3), P8_FIXED_LIT(0xFFFF8000, -0.5),
                                    P8_FIXED_LIT(0x007E0000, 126)};
    static const p8_fixed_t ys[] = {P8_FIXED_LIT(0x003C0000, 60), P8_FIXED_LIT(0xFFFD0000, -3),
                                    P8_FIXED_LIT(0x003C6000, 60.375), P8_FIXED_LIT(0x007C0000, 124)};
    static const p8_fixed_t as[] = {0, P8_FIXED_LIT(0x00008000, 0.5), P8_FIXED_LIT(0x00010000, 1.0)};
    static const int cameras[][2] = {{0, 0}, {-13, 21}};
    static p8_vram_t direct;
    layout_row_cell_t cells[] = {str("cost"), num(12), layout_row_cell_nothing(), str("hi")};
    cells[2] = tuple_of(2, tuple_values);
    layout_row_keys_t keys = with_color(positionals(4, cells), 11);
    layout_row_t *row = layout_row_new(&heap_allocator, &keys);

    install_copy_counter();
    for (size_t c = 0; c < sizeof cameras / sizeof cameras[0]; c++) {
        for (size_t n = 0; n < sizeof as / sizeof as[0]; n++) {
            for (size_t i = 0; i < sizeof xs / sizeof xs[0]; i++) {
                for (size_t j = 0; j < sizeof ys / sizeof ys[0]; j++) {
                    vec_t p = vec_make(xs[i] + p8_fixed_from_int(cameras[c][0]), ys[j] + p8_fixed_from_int(cameras[c][1]));
                    p8_graphics_t direct_gfx;
                    text_image_frame_begin();
                    p8_gfx_init(&graphics);
                    p8_camera(&graphics, cameras[c][0], cameras[c][1]);
                    direct_gfx = graphics;
                    background_stripes(&memory);
                    direct = memory;
                    layout_row_draw_at(&direct_gfx, &direct, row, p, as[n]);
                    layout_row_draw_at_text_image(&graphics, &memory, row, p, as[n]);
                    assert(graphics.camera_x == cameras[c][0] && graphics.camera_y == cameras[c][1]);
                    assert(memcmp(direct.screen, memory.screen, sizeof direct.screen) == 0);
                    assert(memcmp(direct.draw_palette, memory.draw_palette, sizeof direct.draw_palette) == 0);
                }
            }
        }
    }
    p8_draw_backend_set(NULL);
    assert(g_copies == 2 * 2 * 3 * 5 * 4 && "each of the two text items copied, every draw");
    layout_row_draw_at_text_image(&graphics, &memory, NULL, vec_xy(0, 0), 0);
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
    in_fresh_child(test_constructions);
    in_fresh_child(test_planet_resource_row);
    in_fresh_child(test_planet_resource_row_largest_row_fits_a_slot);
    in_fresh_child(test_research_project_layout_row_keys);
    in_fresh_child(test_calls);
    in_fresh_child(test_the_text_image_sibling_draws_like_draw_at);
    in_fresh_child(test_planet_panel);
    in_fresh_child(test_inspection_panel_wants_more);
    in_fresh_child(test_inspection_panel_w_maker_wants_no_more);
    in_fresh_child(test_inspection_panel_lab_wants_its_last);
    in_fresh_child(test_layout_row_is_a_struct);
    in_fresh_child(test_row_of_eight_cells_in_bump_zone);
    in_fresh_child(test_tuple_cell_copies_its_row);
    in_fresh_child(test_row_keeps_nothing_of_its_keys);
    in_fresh_child(test_tuple_refuses_text_and_rows);
    in_fresh_child(test_placed_items_ignore_stale_zone_bytes);
    in_fresh_child(test_every_filled_row_gets_its_own_serial);

    printf("test_layout_row: all tests passed\n");
    return 0;
}
