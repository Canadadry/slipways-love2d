/* The research project, the 34 projects built by cart_load(), read on its
 * struct members and through its production, cost, tooltip and layout row,
 * then installed on fresh planets by planet_install_project() and
 * planet_apply_project(), in games built directly: no events file. */

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
#include "game/economy.h"
#include "engine/entity.h"
#include "engine/entity_registry.h"
#include "p8/fixed.h"
#include "game/main_economy.h"
#include "p8/gfx.h"
#include "p8/input.h"
#include "game/layout.h"
#include "p8/vram.h"
#include "game/planet.h"
#include "game/planet_display.h"
#include "game/planet_family.h"
#include "game/research.h"
#include "game/research_display.h"
#include "game/scroll.h"
#include "game/slipway.h"
#include "engine/entity_class.h"
#include "game/tech_gate.h"
#include "game/ui.h"
#include "game/ui_button.h"
#include "engine/vec.h"
#include "engine/zone.h"
#include "engine/frame_zone.h"
#include "engine/game_zone.h"

static mouse_pos_t mouse_world_only(vec_t world) {
    return (mouse_pos_t){.world = world, .has = MOUSE_POS_HAS_WORLD};
}

static const vec_t *pos_ref(const entity_t *e) {
    return entity_has_pos(e) ? &e->pos : NULL;
}

static const vec_t *map_pos_ref(const entity_t *e) {
    return entity_has_map_pos(e) ? &e->map_pos : NULL;
}


/* Project 12, a colony that needs two resources and exports one; project 3,
 * a project with an output and no inputs, which exports to any planet a
 * slipway joins it to. */
#define COLONY 12
#define EXPORTER 3

#define PROJECTS_COUNT 34
#define PROJECTS_NAMED 26
#define PROJECTS_HASH 0xA4D742D8u
#define DEFAULTS_HASH 0x4FE030B1u
#define FUNCTIONS_HASH 0x6154CA4Fu
#define FUNCTIONS_INVENTED_HASH 0xD71074AAu
#define INSTALL_HASH 0x45B1D201u
#define BUILT_WORLD_HASH 0x41A39EBCu
#define HARVESTED_WORLD_HASH 0x8209C037u

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

static int entity_list_rank_in(const entity_list_t *list, const void *e) {
    int i;
    for (i = 0; list != NULL && i < list->count; i++) {
        if (list->items[i] == e) {
            return i;
        }
    }
    return -1;
}

/* The projects by rank, in order. */
static int projects_count(void) {
    return research_projects_count();
}

static void *project(int k) {
    assert(research_projects_at(k) != NULL);
    return research_projects_at(k);
}

/* 1-based rank of the entity among the projects by rank, -1 when none. */
static int projects_rank(const void *e) {
    int k;
    for (k = 1; e != NULL && k <= projects_count(); k++) {
        if (project(k) == e) {
            return k;
        }
    }
    return -1;
}

/* The kind words the frozen hashes fold before a value, FUNCTION_KIND the one
 * a project without an icon row folds in place of its row (PROJECTS_HASH);
 * the enum entries are gone. Any other value moves them. */
enum { FIXED_KIND = 0, STRING_KIND = 1, BOOL_KIND = 2, REF_KIND = 3, FUNCTION_KIND = 4 };

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

/* 0xDEAD for NULL, else 0 (no pointer it sees carried a tag) and, above
 * `depth` 0, the rank among the live entities and among the projects. */
static uint32_t fold_ref(uint32_t h, const void *ref, int depth) {
    h = fnv(h, (uint32_t)REF_KIND);
    if (ref == NULL) {
        return fnv(h, 0xDEADu);
    }
    h = fnv(h, 0u);
    if (depth <= 0) {
        return h;
    }
    h = fnv(h, (uint32_t)entity_list_rank_in(live_entities(), ref));
    return fnv(h, (uint32_t)projects_rank(ref));
}

/* A need's resource with the bytes the frozen hashes fold for it: kind word
 * 0 then the number; kind word 3 then 0xDEAD when absent. */
static uint32_t fold_resource(uint32_t h, resource_t qe) {
    assert(qe.kind != RESOURCE_FALSE && "fold_resource: a need's resource is never false");
    return qe.kind == RESOURCE_FIXED ? fnv(fnv(h, 0u), qe.number) : fnv(fnv(h, 3u), 0xDEADu);
}

/* The frozen hashes were taken when rows were table views and name lists
 * tables: these are the bytes they folded for their tag and their two kinds
 * of key, frozen values of a tag and an enum since deleted. Changing one
 * reddens every constant that folds it. */
enum { VIEW_TAG = 0x9E5C0B7Au, VIEW_INDEX_KEY = 0, VIEW_NAME_KEY = 1, VIEW_LIST_TAG = 0x9E5C1157u };

/* `REF`, the count, then per name the index-key kind, its position and
 * the string: the format DEFAULTS_HASH froze. */
static uint32_t fold_names(uint32_t h, const str_seq_t *names) {
    int i;

    h = fnv(h, (uint32_t)REF_KIND);
    h = fnv(h, (uint32_t)names->count);
    for (i = 0; i < names->count; i++) {
        h = fnv(h, (uint32_t)VIEW_INDEX_KEY);
        h = fnv(h, (uint32_t)(i + 1));
        h = fold_string(h, names->items[i]);
    }
    return h;
}

/* Frozen copies of the position name lists and of the routed ones: the code
 * has no such lists, and DEFAULTS_HASH folds these. */
static const char *const frozen_positions_items[] = {"name", "planet_codes", "base_cost", "production_key", "inputs", "output", "upgrade_tech", "byproduct"};
static const str_seq_t FROZEN_POSITIONS = {8, frozen_positions_items};
static const char *const frozen_routed_positions_items[] = {"output", "byproduct"};
static const str_seq_t FROZEN_ROUTED_POSITIONS = {2, frozen_routed_positions_items};

static uint32_t fold_absent(uint32_t h) {
    return fnv(h, 0xFFFFFFFFu);
}

/* A member of one kind: the kind word is constant by the struct's shape,
 * kept so that PROJECTS_HASH and INSTALL_HASH still prove the typed reads.
 * A NULL string, or an unset flag, folds as absent. */
static uint32_t fold_string_member(uint32_t h, const char *s) {
    if (s == NULL) {
        return fold_absent(h);
    }
    h = fnv(h, (uint32_t)STRING_KIND);
    for (; *s != '\0'; s++) {
        h = fnv(h, (uint32_t)(unsigned char)*s);
    }
    return fnv(h, 0xFFu);
}

static uint32_t fold_fixed_member(uint32_t h, int has, p8_fixed_t v) {
    return has ? fnv(fnv(h, (uint32_t)FIXED_KIND), (uint32_t)v) : fold_absent(h);
}

static uint32_t fold_bool_member(uint32_t h, int has, int b) {
    return has ? fnv(fnv(h, (uint32_t)BOOL_KIND), (uint32_t)(b != 0)) : fold_absent(h);
}

/* An `upgrade_tech` of false is `has_upgrade_tech` with a NULL one. */
static uint32_t fold_upgrade_tech(uint32_t h, const research_project_t *b) {
    if (!b->has_upgrade_tech) {
        return fold_absent(h);
    }
    return b->upgrade_tech == NULL ? fold_bool_member(h, 1, 0) : fold_string_member(h, b->upgrade_tech);
}

static uint32_t fold_view_head(uint32_t h, int count) {
    h = fnv(h, (uint32_t)REF_KIND);
    h = fnv(h, (uint32_t)VIEW_TAG);
    return fnv(h, (uint32_t)count);
}

static uint32_t fold_view_index(uint32_t h, int i) {
    return fnv(fnv(h, (uint32_t)VIEW_INDEX_KEY), (uint32_t)i);
}

static uint32_t fold_view_name(uint32_t h, const char *name) {
    return fold_string(fnv(h, (uint32_t)VIEW_NAME_KEY), name);
}

static uint32_t fold_row(uint32_t h, const data_row_t *row, int depth);

static uint32_t fold_item(uint32_t h, const data_item_t *item, int depth) {
    if (item->row != NULL) {
        return fold_row(h, item->row, depth);
    }
    if (item->text != NULL) {
        return fold_string(h, item->text);
    }
    if (item->is_false) {
        return fold_bool(h, 0);
    }
    if (item->is_nil) {
        return fold_ref(h, NULL, depth);
    }
    return fold_fixed(h, item->is_fixed ? item->fixed : p8_fixed_from_int(item->number));
}

/* The names the frozen hashes folded, before draw functions were an enum. */
static const char *const draw_function_names[DRAW_FUNCTION_COUNT] = {
    [DRAW_FUNCTION_DRAW_BAR] = "draw_bar",
    [DRAW_FUNCTION_PRINT_OUTLINED] = "print_outlined",
    [DRAW_FUNCTION_DRAW_HELP_FRAME] = "draw_help_frame",
    [DRAW_FUNCTION_SPR] = "spr",
    [DRAW_FUNCTION_DRAW_ROW] = "draw_row",
};

/* A generated or built row, folded as the frozen hashes folded its view:
 * items, then `color`, then `draw_function`. */
static uint32_t fold_row(uint32_t h, const data_row_t *row, int depth) {
    int i;

    if (depth <= 0) {
        return fnv(fnv(h, (uint32_t)REF_KIND), (uint32_t)VIEW_TAG);
    }
    h = fold_view_head(h, row->count + (row->has_color != 0) + (row->draw_function != DRAW_FUNCTION_NONE));
    for (i = 0; i < row->count; i++) {
        h = fold_item(fold_view_index(h, i + 1), &row->items[i], depth - 1);
    }
    if (row->has_color) {
        h = fold_fixed(fold_view_name(h, "color"), p8_fixed_from_int(row->color));
    }
    if (row->draw_function != DRAW_FUNCTION_NONE) {
        h = fold_string(fold_view_name(h, "draw_function"), draw_function_names[row->draw_function]);
    }
    return h;
}

/* A cell folded as the frozen hashes folded the item it was: a string, a
 * number, or a tuple as a row of numbers (nothing as false). */
static uint32_t fold_cell(uint32_t h, layout_row_cell_t cell) {
    int i;

    switch (cell.kind) {
    case LAYOUT_ROW_CELL_STRING:
        return fold_string(h, cell.as.text);
    case LAYOUT_ROW_CELL_FIXED:
        return fold_fixed(h, cell.as.fixed);
    case LAYOUT_ROW_CELL_TUPLE:
        h = fold_view_head(h, cell.as.tuple.count);
        for (i = 0; i < cell.as.tuple.count; i++) {
            layout_row_scalar_t scalar = cell.as.tuple.scalars[i];

            h = fold_view_index(h, i + 1);
            h = scalar.kind == LAYOUT_ROW_SCALAR_FIXED ? fold_fixed(h, scalar.value) : fold_bool(h, 0);
        }
        return h;
    default:
        assert(0 && "fold_cell: the icon row holds a string, a number and a tuple");
        return h;
    }
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

/* A row by its placed items, then `color`, `score`, `pos` and `width`. */
static uint32_t fold_layout_row_instance(uint32_t h, const void *l) {
    const layout_row_t *lq = l;
    int i;

    if (l == NULL) {
        return fnv(h, 0xDEADu);
    }
    assert(lq->header.class == layout_row_class());
    assert(!lq->has_background);
    assert(lq->placed_items != NULL);
    h = fold_view_head(h, lq->placed_items_count + (lq->has_color != 0) +
                              (lq->has_score != 0) + (pos_ref(&lq->header) != NULL) + 1);
    for (i = 0; i < lq->placed_items_count; i++) {
        h = fold_placed_item(fold_view_index(h, i + 1), lq->placed_items[i]);
    }
    if (lq->has_color) {
        h = fold_fixed(fold_view_name(h, "color"), lq->color);
    }
    if (lq->has_score) {
        h = fold_fixed(fold_view_name(h, "score"), lq->score);
    }
    if (pos_ref(&lq->header) != NULL) {
        h = fold_ref(fold_view_name(h, "pos"), pos_ref(&lq->header), 3);
    }
    return fold_fixed(fold_view_name(h, "width"), lq->width);
}

/* The project's buy row laid out in the frame zone, NULL when it has none. */
static const void *project_row(const research_project_t *b) {
    layout_row_keys_t keys;

    return research_project_layout_row_keys(b, &keys) ? layout_row_new(frame_zone(), &keys) : NULL;
}

static void check(const char *label, uint32_t got, uint32_t want) {
    printf("[%s] 0x%08Xu\n", label, (unsigned)got);
    fflush(stdout);
    assert(got == want);
}

static void check_count(const char *label, int got, int want) {
    printf("[%s] %d\n", label, got);
    fflush(stdout);
    assert(got == want);
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

/* A game started as cart_start_game() starts it, with no difficulty picked:
 * a NULL difficulty reads as difficulty 0. */
static void start_game(void) {
    p8_input_init(&input);
    dispatch_context_set_input(&input);
    cart_init();
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
 * two joined by a slipway paid by its drop as a drag pays it (the drag reads
 * the hovered planet off the main UI), then left to settle. */
static void built_world(void) {
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
}

/* Several projects share a name: the one found by name is the last by rank. */
static void *last_with_name(const char *px) {
    void *found = NULL;
    int k;
    for (k = 1; k <= projects_count(); k++) {
        const research_project_t *b = project(k);
        if (b->name != NULL && strcmp(b->name, px) == 0) {
            found = project(k);
        }
    }
    return found;
}

/* Every project as its construction left it: its own fields, `inputs`,
 * `layout_row`, the class's row tail shared by all, and the project found by
 * name aliasing the last project of that name. */
static void test_projects(void) {
    uint32_t hash = 2166136261u;
    uint32_t hash_by_name = 2166136261u;
    void *row_tail = NULL;
    int named = 0;
    int index;

    check_count("projects_count", projects_count(), PROJECTS_COUNT);
    for (index = 1; index <= projects_count(); index++) {
        named += research_projects_find(((const research_project_t *)project(index))->name) == project(index);
    }
    check_count("projects_named", named, PROJECTS_NAMED);

    for (index = 1; index <= PROJECTS_COUNT; index++) {
        research_project_t *built = project(index);
        const pointer_list_t *i_list;
        void *project_by_name;
        int inner_index;

        hash = fnv(hash, (uint32_t)index);
        hash = fold_string_member(hash, built->name);
        hash = fold_string_member(hash, built->planet_codes);
        hash = fold_fixed_member(hash, built->has_base_cost, built->base_cost);
        hash = fold_string_member(hash, built->production_key);
        hash = fold_fixed_member(hash, built->has_output, built->output);
        hash = fold_upgrade_tech(hash, built);
        hash = fold_fixed_member(hash, built->has_byproduct, built->byproduct);
        hash = fold_string_member(hash, built->required_tech);
        hash = fold_fixed_member(hash, built->has_icon_sprite, built->icon_sprite);
        hash = fold_string_member(hash, built->class_name);
        hash = fold_string_member(hash, built->next_type);
        i_list = &built->inputs;
        hash = fnv(hash, (uint32_t)i_list->count);
        for (inner_index = 0; inner_index < i_list->count; inner_index++) {
            resource_t qe;
            qe = need_resource(i_list->items[inner_index]);
            hash = fold_resource(hash, qe);
        }
        if (built->has_icon_sprite) {
            layout_row_keys_t icon_keys = research_project_icon_row_keys(built);

            /* Constant by the row's shape, kept so that PROJECTS_HASH still
             * proves the typed reads. */
            hash = fnv(hash, (uint32_t)REF_KIND);
            assert(icon_keys.count == 3 && !icon_keys.has_color);
            for (inner_index = 0; inner_index < 3; inner_index++) {
                hash = fold_cell(hash, icon_keys.items[inner_index]);
            }
        } else {
            hash = fnv(hash, (uint32_t)FUNCTION_KIND);
        }

        if (index == 1) {
            row_tail = (void *)built->archetype->row_tail;
            assert(row_tail != NULL);
        }
        assert((void *)built->archetype->row_tail == row_tail);
        if (built->name != NULL) {
            project_by_name = research_projects_find(built->name);
            assert(project_by_name != NULL);
            assert(project_by_name == last_with_name(built->name));
            hash = fnv(hash, (uint32_t)projects_rank(project_by_name));
        }
    }
    assert(row_tail == &research_project_row_tail_row);
    hash_by_name = fold_names(hash_by_name, &FROZEN_POSITIONS);
    hash_by_name = fold_names(hash_by_name, &FROZEN_ROUTED_POSITIONS);
    hash_by_name = fold_row(hash_by_name, row_tail, 2);
    check("projects", hash, PROJECTS_HASH);
    check("defaults", hash_by_name, DEFAULTS_HASH);
}

/* Per project: its production as an entry of `production_tables_table`, its
 * cost, its tooltip and its layout row, the same each time it is built for a
 * project without an icon row. */
static uint32_t fold_functions(uint32_t h) {
    int k;

    for (k = 1; k <= PROJECTS_COUNT; k++) {
        research_project_t *b = project(k);
        const production_tables_entry_t *po = research_project_production(b);
        const data_row_t *jb = research_project_tooltip(b);
        uint32_t layout_row_h;

        frame_zone_reset();
        h = fnv(h, (uint32_t)k);
        h = fnv(h, (uint32_t)(po == NULL ? -2 : (int)(po - production_tables_table)));
        h = fnv(h, (uint32_t)research_project_cost(b));
        h = jb == NULL ? fnv(h, 0xDEADu) : fold_row(h, jb, 4);
        assert((jb == NULL) == (b->has_icon_sprite != 0));
        layout_row_h = fold_layout_row_instance(2166136261u, project_row(b));
        h = fnv(h, layout_row_h);
        if (!b->has_icon_sprite) {
            assert(fold_layout_row_instance(2166136261u, project_row(b)) == layout_row_h);
        }
    }
    return h;
}

/* That technology invented at no cost: an entry whose icon resource (0) is
 * no resource code. Never freed. */
static void invent(const char *px) {
    technologies_entry_t *t = malloc(sizeof *t);

    *t = (technologies_entry_t){px, 0, NULL, 0, 0, NULL};
    economy_invent(main_economy_typed(), t);
    assert(tech_gate_is_unlocked(px));
}

static void test_functions(void) {
    int k;

    assert(main_economy_typed() == NULL && !tech_gate_is_unlocked("nanomaterials"));
    start_game();
    assert(!tech_gate_is_unlocked("nanomaterials"));
    check("functions", fold_functions(2166136261u), FUNCTIONS_HASH);

    invent("nanomaterials");
    for (k = 1; k <= PROJECTS_COUNT; k++) {
        const research_project_t *b = project(k);
        if (b->has_upgrade_tech && b->upgrade_tech != NULL && !tech_gate_is_unlocked(b->upgrade_tech)) {
            invent(b->upgrade_tech);
        }
    }
    check("functions_invented", fold_functions(2166136261u), FUNCTIONS_INVENTED_HASH);

    assert(research_project_production(NULL) == NULL && research_project_cost(NULL) == 0);
    {
        layout_row_keys_t untouched = {.count = -1};

        assert(research_project_tooltip(NULL) == NULL && !research_project_layout_row_keys(NULL, &untouched));
        assert(untouched.count == -1);
    }
}

static vec_t vec_xy(int x, int y) {
    return vec_make(p8_fixed_from_int(x), p8_fixed_from_int(y));
}

/* A bare planet on its `planet_id`, `map_pos` and `pos`, the screen
 * position offset from the main camera's. */
static void *new_planet(const char *id, int x, int y) {
    planet_keys_t keys = {0};
    const vec_t *main_camera_pos;

    assert(main_camera_typed() != NULL);
    main_camera_pos = pos_ref(&main_camera_typed()->header);
    assert(main_camera_pos != NULL);
    if (id != NULL) {
        planet_keys_set_id(&keys, id);
    }
    keys.map_pos = vec_xy(x / 2, y / 2);
    keys.has_map_pos = 1;
    keys.pos = vec_add(*main_camera_pos, vec_xy(x + 16, y - 4));
    keys.has_pos = 1;
    return planet_new(&keys);
}

static int with_next_type(const research_project_t *b) {
    return b->next_type != NULL;
}

static int with_class_name(const research_project_t *b) {
    return b->class_name != NULL;
}

static int ordinary(const research_project_t *b) {
    return b->class_name == NULL && b->next_type == NULL && !b->has_icon_sprite;
}

/* `nth` counts from 0. */
static void *project_with(int (*match)(const research_project_t *), int nth) {
    int k;
    for (k = 1; k <= PROJECTS_COUNT; k++) {
        void *b = project(k);
        if (match(b) && nth-- == 0) {
            return b;
        }
    }
    assert(0 && "no such project");
    return NULL;
}

/* The production by content: a production_t has no tag to fold. */
static uint32_t fold_production(uint32_t h, const planet_t *p) {
    const production_t *k = planet_production_read(p);

    if (k == NULL) {
        return fnv(h, 0xFFFFFFFFu);
    }
    h = fnv(h, 1u);
    h = k->has_resource ? fnv(fnv(h, 1u), (uint32_t)k->resource) : fnv(h, 0u);
    return fnv(h, (uint32_t)k->amount);
}

/* A planet's `needs`, folded as the frozen hashes folded the list of tables
 * it was: each need, its resource (absent included), then `satisfied` when
 * written. */
static uint32_t fold_needs(uint32_t h, const planet_t *p) {
    const pointer_list_t *jy = &p->needs;
    int i;

    if (!(p->has & PLANET_HAS_NEEDS)) {
        return fnv(h, 0xFFFFFFFFu);
    }
    h = fnv(fnv(h, (uint32_t)REF_KIND), (uint32_t)VIEW_LIST_TAG);
    h = fnv(h, (uint32_t)jy->count);
    for (i = 0; i < jy->count; i++) {
        const need_t *want = jy->items[i];

        h = fold_view_head(h, 1 + (want->has_satisfied != 0));
        h = fold_resource(fold_view_name(h, "resource"), need_resource(want));
        if (want->has_satisfied) {
            h = fold_fixed(fold_view_name(h, "satisfied"), want->satisfied);
        }
    }
    return h;
}

static uint32_t fold_planet(uint32_t hash, void *entity) {
    const planet_t *p = as_planet(entity);

    hash = fnv(hash, (uint32_t)projects_rank(research_project_get(p->project)));
    hash = fnv(hash, (uint32_t)(research_project_get(p->project) != NULL));
    hash = fold_string_member(hash, p->description);
    hash = fold_needs(hash, p);
    hash = fold_production(hash, p);
    hash = fold_string_member(hash, entity_state_name(p->header.state));
    hash = fold_bool_member(hash, (p->header.fold_anchor != 0), p->header.fold_anchor);
    hash = fold_bool_member(hash, (p->header.is_dead != 0), p->header.is_dead);
    hash = fold_fixed_member(hash, 1, p->header.shake);
    hash = p->planet_type != NULL ? fnv(hash, (uint32_t)(p->planet_type - planet_types_table)) : fnv(hash, 0xFFFFFFFFu);
    return fnv(hash, (uint32_t)entity_list_rank_in(live_entities(), p));
}

/* A project makes a need per character of its input codes, one of a
 * character that is no resource code needing no resource;
 * planet_install_project() copies each need, never shares it. */
static void test_wants(void) {
    project_records_entry_t ri = {0};
    research_project_keys_t keys = {0};
    const pointer_list_t *jy;
    research_project_t *b;
    void *a;
    int p;
    int k;

    start_game();
    ri.npos = 5;
    ri.name = "x";
    ri.planet_codes = "y";
    ri.base_cost = 1;
    ri.production_key = "z";
    ri.inputs = "pz";
    keys.record = &ri;
    b = research_project_new(&keys);
    assert(b->inputs.count == 2 && resource_code_lookup("p", &p));
    assert(((const need_t *)b->inputs.items[0])->row.has_resource);
    assert(((const need_t *)b->inputs.items[0])->row.resource == p8_fixed_from_int(p));
    assert(!((const need_t *)b->inputs.items[1])->row.has_resource);

    a = new_planet("p950", 12, 16);
    planet_install_project(a, b);
    assert(as_planet(a)->has & PLANET_HAS_NEEDS);
    jy = &as_planet(a)->needs;
    assert(jy->count == 2);
    for (k = 0; k < 2; k++) {
        assert(jy->items[k] != b->inputs.items[k]);
        assert(memcmp(jy->items[k], b->inputs.items[k], sizeof(need_t)) == 0);
    }
}

static void test_install(void) {
    uint32_t hash = 2166136261u;
    void *ordinary_project;
    void *first;
    void *next_type_project;
    int nth;

    start_game();
    ordinary_project = project_with(ordinary, 0);

    first = new_planet("p901", 10, 12);
    planet_install_project(first, ordinary_project);
    assert((void *)research_project_get(as_planet(first)->project) == ordinary_project && as_planet(first)->description == NULL);
    hash = fold_planet(hash, first);

    first = new_planet("p902", 30, 14);
    planet_apply_project(first, ordinary_project);
    assert((void *)research_project_get(as_planet(first)->project) == ordinary_project);
    hash = fold_planet(hash, first);

    next_type_project = project_with(with_next_type, 0);
    first = new_planet("p903", 50, 16);
    planet_apply_project(first, next_type_project);
    assert(as_planet(first)->planet_type != NULL);
    hash = fold_planet(hash, first);

    for (nth = 0; nth < 2; nth++) {
        int before;
        research_project_t *cw = project_with(with_class_name, nth);
        first = new_planet(nth == 0 ? "p904" : "p905", 70 + 20 * nth, 18);
        before = live_entities()->count;
        planet_apply_project(first, cw);
        hash = fold_string_member(hash, cw->class_name);
        hash = fnv(hash, (uint32_t)(live_entities()->count - before));
        hash = fold_planet(hash, first);
        hash = fold_planet(hash, live_entities()->items[live_entities()->count - 1]);
    }

    first = new_planet("p906", 110, 20);
    planet_apply_project(first, NULL);
    assert(research_project_get(as_planet(first)->project) == NULL);
    hash = fold_planet(hash, first);
    check("install", hash, INSTALL_HASH);
}

/* Each planet: the rank of its project among the projects; then the
 * screen, once a frame is played and drawn. */
static uint32_t fold_world(void) {
    uint32_t h = 2166136261u;
    const entity_list_t *jo;
    int i;

    cart_update60();
    cart_draw();
    jo = entities_by_tag(TAG_PLANET);
    for (i = 0; jo != NULL && i < jo->count; i++) {
        h = fnv(h, (uint32_t)projects_rank(research_project_get(as_planet(jo->items[i])->project)));
    }
    return fnv(h, screen_hash());
}

static void test_built_world(void) {
    built_world();
    check("built_world", fold_world(), BUILT_WORLD_HASH);
}

/* A discovered planet harvested (class name "burst"): planet_apply_project()
 * swaps it for a burst. */
static void test_harvested_world(void) {
    const entity_t *last;
    void *p;
    int i;

    start_game();
    p = entities_by_tag(TAG_PLANET)->items[0];
    planet_roll_type(p);
    planet_apply_project(p, project_with(with_class_name, 1));
    last = live_entities()->items[live_entities()->count - 1];
    assert(last != NULL && last->class == burst_class());
    for (i = 0; i < 60; i++) {
        update_entities();
    }
    check("harvested_world", fold_world(), HARVESTED_WORLD_HASH);
}

static size_t block_bytes(size_t size) {
    return ZONE_CELLS(size) * sizeof(zone_cell_t);
}

/* Each project's `inputs` items unless empty, and its needs: the projects
 * themselves, list headers included, live in the project table. */
static size_t projects_bytes(void) {
    size_t bytes = 0;
    int k;

    for (k = 1; k <= projects_count(); k++) {
        const research_project_t *b = project(k);
        const int n = b->inputs.count;

        bytes += (size_t)n * block_bytes(sizeof(need_t));
        if (n > 0) {
            bytes += block_bytes((size_t)n * sizeof(void *));
        }
    }
    return bytes;
}

/* The projects' needs are all the load sub-zone holds after cart_load(), each
 * `inputs` list reserved to its exact length. */
static void test_projects_in_load_zone(void) {
    int k;

    for (k = 1; k <= projects_count(); k++) {
        const research_project_t *b = project(k);
        assert(b->inputs.capacity == b->inputs.count && b->inputs.zone == game_zone_load_zone());
    }
    printf("  load after cart_load: %lu\n", (unsigned long)load_zone_used());
    assert(load_zone_used() == projects_bytes());
}

/* A need is taken from the zone its caller passes, and gives back the resource it was made with. */
static void test_needs_new_in_the_zone_passed(void) {
    const resource_t qe = {.kind = RESOURCE_FIXED, .number = p8_fixed_from_int(3)};
    const resource_t nil = {.kind = RESOURCE_NIL};
    const size_t load_zone = load_zone_used();
    const size_t game_zone_used_before = game_zone_used();
    need_t *a = need_new(game_zone_load_zone(), qe);
    need_t *b = need_new(game_zone(), nil);

    assert(load_zone_used() == load_zone + block_bytes(sizeof *a));
    assert(game_zone_used() == game_zone_used_before + block_bytes(sizeof *b));
    assert(a->row.has_resource && a->row.resource == qe.number && !a->has_satisfied);
    assert(!b->row.has_resource && !b->has_satisfied);
    assert(resource_equal(need_resource(a), qe) && resource_equal(need_resource(b), nil));
}

/* The projects rebuilt from `project_records_table` forget the ones held
 * and are the same; shutdown leaves no class and no project, the row tail
 * still the generated row; init again, then a load, gives them once more,
 * and a second load takes the same bytes. The load sub-zone holds one set
 * of projects: it is reset before the rebuild, as cart_load() does. */
static void test_rebuild_and_shutdown(void) {
    size_t k;

    game_zone_reset();
    research_build_projects();
    test_projects();

    research_shutdown();
    assert(research_projects_count() == 0 && research_projects_at(1) == NULL && research_projects_find("colony") == NULL);
    assert(!research_project_ready());
    assert(research_project_row_tail() == &research_project_row_tail_row);
    research_shutdown();

    research_init();
    assert(research_project_ready() && research_project_row_tail() == &research_project_row_tail_row);
    cart_load();
    test_projects();
    k = load_zone_used();
    cart_load();
    assert(load_zone_used() == k);
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

/* As must_abort, and the abort is the assert whose text holds `needle`:
 * past it, the construction reads out of bounds and may abort elsewhere. */
static void must_abort_saying(void (*write)(void *), void *e, const char *needle) {
    char said[512];
    ssize_t n;
    size_t got = 0;
    int fds[2];
    pid_t child;
    int status;

    fflush(stdout);
    assert(pipe(fds) == 0);
    child = fork();
    assert(child >= 0);
    if (child == 0) {
        close(fds[0]);
        assert(dup2(fds[1], 2) == 2);
        write(e);
        _exit(0);
    }
    close(fds[1]);
    while (got < sizeof said - 1 && (n = read(fds[0], said + got, sizeof said - 1 - got)) > 0) {
        got += (size_t)n;
    }
    said[got] = '\0';
    close(fds[0]);
    assert(waitpid(child, &status, 0) == child);
    assert(WIFSIGNALED(status) && WTERMSIG(status) == SIGABRT);
    assert(strstr(said, needle) != NULL);
}

static void build_with_header(void *e) {
    project_records_entry_t header = {0};
    research_project_keys_t keys = {0};

    (void)e;
    header.header = "x";
    keys.record = &header;
    (void)research_project_new(&keys);
}

static void build_with_output_missing_from_resource_codes(void *e) {
    project_records_entry_t ri = {0};
    research_project_keys_t keys = {0};

    (void)e;
    ri.npos = 6;
    ri.name = "x";
    ri.planet_codes = "y";
    ri.base_cost = 1;
    ri.production_key = "z";
    ri.inputs = "";
    ri.output = "not a resource code";
    keys.record = &ri;
    (void)research_project_new(&keys);
}

static void build_with_icon_sprite_and_no_name(void *e) {
    project_records_entry_t ri = {0};
    research_project_keys_t keys = {0};

    (void)e;
    ri.has_icon_sprite = 1;
    ri.icon_sprite = 3;
    keys.record = &ri;
    (void)research_project_new(&keys);
}

static void build_with_nine_positions(void *e) {
    project_records_entry_t ri = {0};
    research_project_keys_t keys = {0};

    (void)e;
    ri.npos = 9;
    ri.name = "x";
    keys.record = &ri;
    (void)research_project_new(&keys);
}

static void build_with_no_name_string(void *e) {
    project_records_entry_t ri = {0};
    research_project_keys_t keys = {0};

    (void)e;
    ri.npos = 1;
    keys.record = &ri;
    (void)research_project_new(&keys);
}

static void build_with_no_output_number(void *e) {
    project_records_entry_t ri = {0};
    research_project_keys_t keys = {0};

    (void)e;
    ri.npos = 6;
    ri.name = "x";
    ri.planet_codes = "y";
    ri.base_cost = 1;
    ri.production_key = "z";
    ri.inputs = "";
    keys.record = &ri;
    (void)research_project_new(&keys);
}

static void cost_on_other(void *e) {
    (void)research_project_cost(e);
}

static void production_on_other(void *e) {
    (void)research_project_production(e);
}

static void tooltip_on_other(void *e) {
    (void)research_project_tooltip(e);
}

static void layout_row_on_other(void *e) {
    layout_row_keys_t keys;

    (void)research_project_layout_row_keys(e, &keys);
}

static void install_project_on_other(void *e) {
    planet_install_project((planet_t *)entities_by_tag(TAG_PLANET)->items[0], e);
}

static void apply_project_on_other(void *e) {
    planet_apply_project((planet_t *)entities_by_tag(TAG_PLANET)->items[0], e);
}

/* A project is a struct whose descriptor has no method and no writer, its
 * row tail the class's; a header entry, a routed position missing from the
 * resource codes, an `icon_sprite` without a name, more than eight
 * positions, or a position of the wrong type stops its construction, and its
 * functions refuse anything that is not a project. With no key, only the
 * row tail, empty `inputs` and no `layout_row`. */
static void test_research_project_is_a_struct(void) {
    const entity_class_t *cls = research_project_class();
    research_project_t *b = project(1);
    research_project_t *bare;
    int j;

    assert(b != NULL && b->header.class == cls);
    assert(cls->draw == NULL && cls->draw_overlay == NULL && cls->hit_test == NULL && cls->new_year == NULL && cls->refresh == NULL &&
           cls->has_refresh == NULL && cls->left_press == NULL && cls->accept_drop == NULL);
    assert(cls->drag == NULL && cls->drop == NULL && cls->attach_slipway == NULL && cls->offer_trade == NULL && cls->draw_at == NULL &&
           cls->spawn_particle == NULL);
    for (j = 0; j < STATE_COUNT; j++) {
        assert(cls->states[j] == NULL);
    }
    assert(research_project_ready());
    assert(b->archetype->row_tail != NULL && b->archetype->row_tail == research_project_row_tail());

    bare = research_project_new(NULL);
    assert(bare != NULL && bare->header.class == cls);
    assert(bare->archetype == &research_project_archetype && bare->archetype == b->archetype);
    assert(bare->inputs.count == 0 && bare->inputs.zone == game_zone_load_zone());
    assert(bare->name == NULL && bare->planet_codes == NULL && !bare->has_base_cost && bare->production_key == NULL && !bare->has_output);
    assert(!bare->has_upgrade_tech && !bare->has_byproduct && bare->required_tech == NULL && !bare->has_icon_sprite && bare->class_name == NULL);
    assert(bare->next_type == NULL);

    for (j = 1; j <= PROJECTS_COUNT; j++) {
        research_project_t *with_icon = project(j);
        layout_row_keys_t keys;
        const layout_row_cell_t *cells;

        if (!with_icon->has_icon_sprite) {
            continue;
        }
        keys = research_project_icon_row_keys(with_icon);
        cells = keys.items;
        assert(keys.count == 3 && !keys.has_color && !keys.has_background && !keys.has_score && !keys.has_pos);
        assert(cells[0].kind == LAYOUT_ROW_CELL_STRING && strcmp(cells[0].as.text, with_icon->name) == 0);
        assert(cells[1].kind == LAYOUT_ROW_CELL_FIXED && cells[1].as.fixed == p8_fixed_from_int(32));
        assert(cells[2].kind == LAYOUT_ROW_CELL_TUPLE && cells[2].as.tuple.count == 2);
        assert(cells[2].as.tuple.scalars[0].kind == LAYOUT_ROW_SCALAR_FIXED && cells[2].as.tuple.scalars[0].value == with_icon->icon_sprite);
        assert(cells[2].as.tuple.scalars[1].kind == LAYOUT_ROW_SCALAR_FIXED && cells[2].as.tuple.scalars[1].value == p8_fixed_from_int(8));
    }

    start_game();
    must_abort(build_with_header, NULL);
    must_abort(build_with_output_missing_from_resource_codes, NULL);
    must_abort(build_with_icon_sprite_and_no_name, NULL);
    must_abort_saying(build_with_nine_positions, NULL, "research_project_positions: a record has eight positions");
    must_abort(build_with_no_name_string, NULL);
    must_abort(build_with_no_output_number, NULL);
    must_abort(cost_on_other, main_ui());
    must_abort(production_on_other, main_ui());
    must_abort(tooltip_on_other, main_ui());
    must_abort(layout_row_on_other, main_ui());
    must_abort(install_project_on_other, main_ui());
    must_abort(apply_project_on_other, main_ui());
}

/* The project found by name is the last one registered under it, even one
 * research_build_projects() did not make; the one by rank keeps its own. */
static void test_projects_name_is_the_last_registered(void) {
    research_project_t *first = project(1);
    research_project_keys_t keys = {0};
    research_project_t *again;
    int i;

    for (i = 0; keys.record == NULL; i++) {
        assert(i < PROJECT_RECORDS_COUNT);
        if (project_records_table[i].header == NULL && strcmp(project_records_table[i].name, first->name) == 0) {
            keys.record = &project_records_table[i];
        }
    }
    again = research_project_new(&keys);
    assert(again != first && research_projects_find(first->name) == again);
    assert(research_projects_at(1) == first && research_projects_count() == PROJECTS_COUNT);
}

static void test_project_id_resolves_until_reset_or_rebuild(void) {
    research_project_t *b = project(COLONY);
    const char *name = b->name;
    research_project_id_t id = research_project_id_of(b);

    assert(research_project_get(id) == b);
    assert(research_project_get(RESEARCH_PROJECT_ID_NONE) == NULL);
    assert(research_project_get(research_project_id_of(NULL)) == NULL);

    reset_world(NULL);
    research_expire_project_ids();
    assert(research_project_get(id) == NULL);
    assert(research_projects_at(COLONY) == b);
    assert(research_project_get(research_project_id_of(b)) == b);

    id = research_project_id_of(research_projects_find(name));
    assert(research_project_get(id) != NULL);
    game_zone_reset();
    research_build_projects();
    assert(research_project_get(id) == NULL);
    assert(research_project_get(research_project_id_of(research_projects_find(name))) == research_projects_find(name));
    assert(research_projects_find(name) != NULL && research_projects_count() == PROJECTS_COUNT);
}

static void test_project_links_resolve_until_reset(void) {
    button_keys_t keys = {0};
    button_t *row;
    void *a;

    start_game();
    a = new_planet("p960", 14, 18);
    planet_install_project(a, project(COLONY));
    keys.project = research_project_id_of(project(EXPORTER));
    row = button_new(&keys);
    assert(research_project_get(as_planet(a)->project) == project(COLONY));
    assert(research_project_get(row->project) == project(EXPORTER));

    cart_update60();
    cart_start_game(NULL);
    assert(research_project_get(as_planet(a)->project) == NULL);
    assert(research_project_get(row->project) == NULL);
}

static void fill_project_table(void) {
    int i;

    for (i = 0; i <= PROJECT_RECORDS_COUNT; i++) {
        (void)research_project_new(NULL);
    }
}

static void test_project_table_full_is_fatal(void) {
    char said[512];
    ssize_t n;
    size_t got = 0;
    int fds[2];
    pid_t child;
    int status;

    fflush(stdout);
    assert(pipe(fds) == 0);
    child = fork();
    assert(child >= 0);
    if (child == 0) {
        close(fds[0]);
        assert(dup2(fds[1], 2) == 2);
        fill_project_table();
        _exit(0);
    }
    close(fds[1]);
    while (got < sizeof said - 1 && (n = read(fds[0], said + got, sizeof said - 1 - got)) > 0) {
        got += (size_t)n;
    }
    said[got] = '\0';
    close(fds[0]);
    assert(waitpid(child, &status, 0) == child);
    assert(WIFEXITED(status) && WEXITSTATUS(status) == 3);
    assert(strstr(said, "research project table full") != NULL);
}

/* Each scenario runs in a child forked right after cart_load(), as a fresh
 * process would start it: the rnd state is the same for all. */
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

int main(void) {
    p8_vram_init(&memory);
    p8_gfx_init(&graphics);
    p8_sprite_sheet_load(&memory);
    p8_input_init(&input);
    dispatch_context_set_gfx(&graphics);
    dispatch_context_set_mem(&memory);
    dispatch_context_set_input(&input);

    cart_load();
    in_fresh_child(test_projects);
    in_fresh_child(test_functions);
    in_fresh_child(test_install);
    in_fresh_child(test_wants);
    in_fresh_child(test_built_world);
    in_fresh_child(test_harvested_world);
    in_fresh_child(test_research_project_is_a_struct);
    in_fresh_child(test_projects_name_is_the_last_registered);
    in_fresh_child(test_rebuild_and_shutdown);
    in_fresh_child(test_projects_in_load_zone);
    in_fresh_child(test_needs_new_in_the_zone_passed);
    in_fresh_child(test_project_id_resolves_until_reset_or_rebuild);
    in_fresh_child(test_project_links_resolve_until_reset);
    in_fresh_child(test_project_table_full_is_fatal);

    printf("test_research_project: all tests passed\n");
    return 0;
}
