/* The help screen, built by its only road (cart_init() on the title screen,
 * help_screen_new()), its defaults read on its archetype, and driven
 * through the real on_click, draw_layers(), update_entities() and
 * economy_recompute(). Its methods are compared to the public functions. No
 * events file. */

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
#include "engine/bootstrap.h"
#include "game/cart.h"
#include "engine/dispatch_context.h"
#include "engine/entity.h"
#include "engine/entity_registry.h"
#include "engine/entity_geom.h"
#include "p8/fixed.h"
#include "engine/geom.h"
#include "p8/gfx.h"
#include "game/economy.h"
#include "game/help.h"
#include "game/main_economy.h"
#include "game/save_storage.h"
#include "p8/input.h"
#include "p8/vram.h"
#include "game/particle.h"
#include "game/ui_zone.h"
#include "game/planet.h"
#include "game/probe.h"
#include "engine/entity_class.h"
#include "engine/entity_id.h"
#include "game/ui.h"
#include "game/ui_button.h"
#include "game/visual_random.h"
#include "engine/vec.h"

/* The emitter's particle list, NULL without EMITTER_HAS_PARTICLES. */
static pointer_list_t *particles_of(const emitter_t *emitter) {
    return (emitter->has & EMITTER_HAS_PARTICLES) ? (pointer_list_t *)&emitter->particles : NULL;
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



#define BUILT_HASH 0xB7038702u
#define LIFE_HASH 0xA3D935BFu
#define HOVER_HASH 0x56A42FA7u

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

/* 0 absent or false, 2 true. */
static uint32_t is_hovered_state(const probe_t *fv) {
    return fv->header.is_hovered ? 2 : 0;
}

/* The words the frozen hashes fold before a value: 0xF1 a number, 0xB0 a
 * boolean (then 0 or 1), 0x57 a string; any other moves every *_HASH. */
static uint32_t fold_fixed(uint32_t h, p8_fixed_t x) {
    return fnv(fnv(h, 0xF1u), (uint32_t)x);
}

static uint32_t fold_bool(uint32_t h, int b) {
    return fnv(fnv(h, 0xB0u), b ? 1u : 0u);
}

static uint32_t fold_string(uint32_t h, const char *s) {
    return fnv_str(fnv(h, 0x57u), s);
}

/* A class's tags, in enum order: each position, then its name, then
 * the count. */
static uint32_t fold_tags(uint32_t h, unsigned tags) {
    static const char *const names[TAG_COUNT] = {
        [TAG_STRUCTURE] = "structure", [TAG_ASCENSION] = "ascension", [TAG_SLIPGATE] = "slipgate",
        [TAG_PROBE] = "probe", [TAG_MESSAGE_BOX] = "message_box", [TAG_SLIPWAY] = "slipway",
        [TAG_WORMHOLE] = "wormhole", [TAG_PLANET] = "planet", [TAG_NODE] = "node",
    };
    uint32_t k = 0;
    int t;

    for (t = 0; t < TAG_COUNT; t++) {
        if (tags & TAG(t)) {
            k++;
            h = fnv(h, k);
            h = fold_string(h, names[t]);
        }
    }
    return fnv(h, k);
}

/* An integer sequence: each position, then its value, then the count. */
static uint32_t fold_seq(uint32_t h, const int_seq_t *s) {
    int i;

    for (i = 0; i < s->count; i++) {
        h = fnv(h, (uint32_t)(i + 1));
        h = fold_fixed(h, p8_fixed_from_int(s->items[i]));
    }
    return fnv(h, (uint32_t)s->count);
}

/* The names the frozen hashes folded, before draw functions were an enum. */
static const char *const draw_function_names[DRAW_FUNCTION_COUNT] = {
    [DRAW_FUNCTION_DRAW_BAR] = "draw_bar",
    [DRAW_FUNCTION_PRINT_OUTLINED] = "print_outlined",
    [DRAW_FUNCTION_DRAW_HELP_FRAME] = "draw_help_frame",
    [DRAW_FUNCTION_SPR] = "spr",
    [DRAW_FUNCTION_DRAW_ROW] = "draw_row",
};

/* A generated row: items (a nested row folded the same way), then its
 * color, then its draw function, then the entry count. */
static uint32_t fold_row(uint32_t h, const data_row_t *row) {
    int i;

    for (i = 0; i < row->count; i++) {
        const data_item_t *item = &row->items[i];

        h = fnv(h, (uint32_t)(i + 1));
        if (item->row != NULL) {
            h = fold_row(fnv(h, 0x7Au), item->row);
        } else if (item->text != NULL) {
            h = fold_string(h, item->text);
        } else if (item->is_false) {
            h = fold_bool(h, 0);
        } else {
            h = fold_fixed(h, item->is_fixed ? item->fixed : p8_fixed_from_int(item->number));
        }
    }
    if (row->has_color) {
        h = fold_fixed(fnv_str(h, "color"), p8_fixed_from_int(row->color));
    }
    if (row->draw_function != DRAW_FUNCTION_NONE) {
        h = fold_string(fnv_str(h, "draw_function"), draw_function_names[row->draw_function]);
    }
    return fnv(h, (uint32_t)(row->count + (row->has_color != 0) + (row->draw_function != DRAW_FUNCTION_NONE)));
}

static uint32_t fold_ref(uint32_t h, const void *ref) {
    if (ref == NULL) {
        return fnv(h, 0xDEADu);
    }
    if (entity_list_listed(live_entities(), ref)) {
        return fnv(fnv(h, 0x4Eu), (uint32_t)entity_list_rank_in(live_entities(), ref));
    }
    if (ref == (const void *)&help_screen_trail_colors_seq) {
        return fold_seq(fnv(h, 0x7Au), ref);
    }
    if (ref == (const void *)&help_screen_rows_row) {
        return fold_row(fnv(h, 0x7Au), ref);
    }
    return fnv(fnv(h, 0xEEEEu), 0u);
}

/* A member the reader always answers: its presence bit and kind byte are
 * constant by the struct's shape, kept so that BUILT_HASH and LIFE_HASH
 * still prove the typed reads. */
static uint32_t fold_fixed_member(uint32_t h, p8_fixed_t x) {
    return fnv(fnv(fnv(h, 1u), 0xF1u), (uint32_t)x);
}

static uint32_t fold_bool_member(uint32_t h, int b) {
    return fnv(fnv(fnv(h, 1u), 0xB0u), (uint32_t)b);
}

static uint32_t fold_string_member(uint32_t h, const char *s) {
    return s == NULL ? fnv(h, 0u) : fnv_str(fnv(fnv(h, 1u), 0x57u), s);
}

static uint32_t fold_ref_member(uint32_t h, const void *ref) {
    return ref == NULL ? fnv(h, 0u) : fold_ref(fnv(h, 1u), ref);
}

static uint32_t fold_vec(uint32_t h, const vec_t *p) {
    if (p == NULL) {
        return fnv(h, 0xDEADu);
    }
    return fnv(fnv(h, (uint32_t)p->x), (uint32_t)p->y);
}

static uint32_t fold_box(uint32_t h, const box_t *b) {
    assert(b != NULL);
    h = fnv(fnv(h, (uint32_t)b->left), (uint32_t)b->top);
    return fnv(fnv(h, (uint32_t)b->right), (uint32_t)b->bottom);
}

/* The item count of a list member, -1 when NULL. */
static int size_of(const pointer_list_t *list) {
    return list == NULL ? -1 : list->count;
}

static void check(const char *label, uint32_t got, uint32_t want) {
    printf("[%s] 0x%08Xu\n", label, (unsigned)got);
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

static vec_t point(int x, int y) {
    return vec_make(p8_fixed_from_int(x), p8_fixed_from_int(y));
}

static mouse_pos_t mouse_pos(vec_t world, vec_t screen) {
    return (mouse_pos_t){.world = world, .screen = screen, .has = MOUSE_POS_HAS_WORLD | MOUSE_POS_HAS_SCREEN};
}

static const entity_class_t *class_of(const void *e) {
    return ((const entity_t *)e)->class;
}

static void start_title(void) {
    p8_input_init(&input);
    dispatch_context_set_input(&input);
    cart_init();
}

/* The help, whatever its form: the probe's tags and layer 10 (a probe has
 * 2). */
static int is_help(entity_t *e) {
    const probe_t *g_ =
        e != NULL && (e->class->lineage & LINEAGE(LINEAGE_PROBE)) ? (const probe_t *)e : NULL;
    return g_ != NULL && g_->header.class->tags == TAG(TAG_PROBE) && g_->archetype->layer == p8_fixed_from_int(10);
}

static probe_t *the_help(void) {
    const entity_list_t *hv = live_entities();
    entity_t *found = NULL;
    int i;

    for (i = 0; i < hv->count; i++) {
        if (is_help(hv->items[i])) {
            assert(found == NULL);
            found = hv->items[i];
        }
    }
    assert(found != NULL && found->class == help_screen_class());
    return (probe_t *)found;
}

static int in_layer(int layer, const void *e) {
    return entity_id_list_has(ui_hovered_layer(main_ui(), layer), entity_id_of(e));
}

static const button_t *as_button(const void *e) {
    assert(e != NULL &&
           ((const entity_t *)e)->class == button_class());
    return e;
}

/* The title's help as its construction, emitter_init_in() and
 * entity_register() left it: its 22 defaults and the 6 of the probe, their
 * tables' contents and identity, its state and state time, its empty
 * particles; no name only read or written elsewhere; its methods; the 4
 * buttons message_box_open_box() built before it, each carrying the
 * on_click stamped from its difficulty_options_table row; where
 * entity_register() filed it; main_ui()'s ephemerals emptied. */
static void test_built(void) {
    uint32_t hash = 2166136261u;
    const entity_list_t *live;
    probe_t *help_screen;
    const probe_t *probe;
    const emitter_t *emitter;
    const targeting_t *targeting;
    int rank;
    size_t index;
    int key_index;

    start_title();
    help_screen = the_help();
    probe = help_screen;
    emitter = &probe->emitter;
    targeting = &probe->targeting;
    live = live_entities();

    assert(probe->archetype->layer == p8_fixed_from_int(10));
    assert(emitter->archetype->lifetime == p8_fixed_from_int(17));
    assert(probe->archetype->back_color == p8_fixed_from_int(11));
    assert(probe->header.class->tags == TAG(TAG_PROBE));
    assert(pos_ref(&probe->header) != NULL && emitter->trail_colors != NULL);
    assert(probe->archetype->rows != NULL && targeting->archetype->confirmed_state != ENTITY_STATE_NONE);
    hash = fold_fixed_member(hash, probe->archetype->layer);
    hash = fold_ref_member(hash, pos_ref(&probe->header));
    hash = fold_ref_member(hash, &probe->archetype->fixed_pos);
    hash = fold_fixed_member(hash, probe->archetype->back_color);
    hash = fold_ref_member(hash, &probe->archetype->row_step);
    hash = fold_fixed_member(hash, probe->archetype->clear_ephemerals_after);
    hash = fold_fixed_member(hash, emitter->archetype->spawn_rate);
    hash = fold_ref_member(hash, emitter->trail_colors);
    hash = fold_fixed_member(hash, emitter->matrix_xx);
    hash = fold_fixed_member(hash, emitter->matrix_xy);
    hash = fold_fixed_member(hash, emitter->matrix_yx);
    hash = fold_fixed_member(hash, emitter->matrix_yy);
    hash = fold_fixed_member(hash, emitter->angle_slices);
    hash = fold_fixed_member(hash, emitter->base_radius);
    hash = fold_fixed_member(hash, emitter->speed_factor);
    hash = fold_fixed_member(hash, emitter->archetype->quarter_turn);
    hash = fold_fixed_member(hash, emitter->archetype->lifetime);
    hash = fold_ref_member(hash, &probe->archetype->hit_box);
    hash = fold_ref_member(hash, probe->archetype->rows);
    hash = fold_tags(fnv(fnv(hash, 1u), 0x7Au), probe->header.class->tags);
    hash = fold_fixed_member(hash, probe->archetype->fold_anchor);
    hash = fold_string_member(hash, entity_state_name(targeting->archetype->confirmed_state));
    hash = fold_fixed_member(hash, targeting->cost);
    hash = fold_bool_member(hash, targeting->archetype->clock_ticks != 0);
    hash = fold_fixed_member(hash, targeting->archetype->attach_distance);
    hash = fold_vec(hash, pos_ref(&probe->header));
    hash = fold_vec(hash, &probe->archetype->fixed_pos);
    hash = fold_vec(hash, &probe->archetype->row_step);
    hash = fold_box(hash, &probe->archetype->hit_box);
    assert(probe->archetype == &help_screen_archetype && entity_has_pos(&probe->header) && probe->header.pos.x == help_screen_archetype.pos.x &&
           probe->header.pos.y == help_screen_archetype.pos.y);
    assert(emitter->trail_colors == &help_screen_trail_colors_seq);
    assert(emitter->trail_colors != &probe_trail_colors_seq);
    assert(probe->archetype->rows == &help_screen_rows_row);
    hash = fold_string_member(hash, entity_state_name(probe->header.state));
    hash = fold_fixed_member(hash, probe->header.state_time);
    hash = fnv(hash, (uint32_t)size_of(particles_of(emitter)));
    assert(size_of(particles_of(emitter)) == 0);
    assert(!help_screen->header.is_hovered && map_pos_ref(&probe->header) == NULL && targeting_target(targeting) == NULL && !probe->header.is_dead);
    assert(class_of(help_screen)->new_year == NULL && class_of(help_screen)->refresh == NULL && class_of(help_screen)->states[ENTITY_STATE_NORMAL] == NULL &&
           class_of(help_screen)->left_press == NULL);

    assert(class_of(help_screen)->draw == help_screen_draw_method);
    assert(class_of(help_screen)->draw_overlay == targeting_tooltip_method);
    assert(class_of(help_screen)->spawn_particle == probe_spawn_particle);
    assert(class_of(help_screen)->drag == targeting_track_method);
    assert(class_of(help_screen)->drop == targeting_confirm_method);
    assert(class_of(help_screen)->hit_test != NULL);

    rank = entity_list_rank_in(live, help_screen);
    hash = fnv(hash, (uint32_t)rank);
    hash = fnv(hash, (uint32_t)live->count);
    assert(rank >= 4);
    for (key_index = 0; key_index < 4; key_index++) {
        const button_t *b = as_button(live->items[rank - 4 + key_index]);
        vec_t qf;
        assert(b->header.layer == p8_fixed_from_int(11));
        assert(slide_target_ref(b) != NULL);
        qf = *slide_target_ref(b);
        assert(qf.x == p8_fixed_from_int(P8_SCREEN_WIDTH / 2 - 30) && qf.y == p8_fixed_from_int(P8_SCREEN_HEIGHT / 2 - 8 + 11 * key_index));
        hash = fold_vec(hash, slide_target_ref(b));
        hash = fold_vec(hash, pos_ref(&b->header));
        hash = fold_fixed_member(hash, b->header.layer);
    }
    for (index = 0; index < 5; index++) {
        static const entity_message_t messages[] = {ENTITY_MESSAGE_DRAW, ENTITY_MESSAGE_DRAW_OVERLAY, ENTITY_MESSAGE_HIT_TEST, ENTITY_MESSAGE_NEW_YEAR, ENTITY_MESSAGE_REFRESH};
        hash = fnv(hash, (uint32_t)entity_list_rank_in(entities_by_message(messages[index]), help_screen));
    }
    assert(entity_list_listed(entities_by_message(ENTITY_MESSAGE_DRAW), help_screen) && entity_list_listed(entities_by_message(ENTITY_MESSAGE_DRAW_OVERLAY), help_screen) && entity_list_listed(entities_by_message(ENTITY_MESSAGE_HIT_TEST), help_screen));
    assert(!entity_list_listed(entities_by_message(ENTITY_MESSAGE_NEW_YEAR), help_screen) && !entity_list_listed(entities_by_message(ENTITY_MESSAGE_REFRESH), help_screen));
    hash = fnv(hash, (uint32_t)entity_list_rank_in(entities_by_tag(TAG_PROBE), help_screen));
    assert(entity_list_listed(entities_by_tag(TAG_PROBE), help_screen));

    for (key_index = 0; key_index < 4; key_index++) {
        const button_t *row = (const button_t *)live->items[rank - 4 + key_index];
        assert(row->has_on_click);
        assert(row->on_click == help_start_game_field_value());
    }
    assert(main_ui()->ephemerals.count == 0);
    check("built", hash, BUILT_HASH);
}

/* draw_layers(ENTITY_MESSAGE_DRAW) x3 with the screen and the particles;
 * draw_layers(ENTITY_MESSAGE_DRAW_OVERLAY); update_entities() x3; hit_test in and out of
 * the hit box, the screen cursor kept since the layer is 10 or more. */
static void test_life(void) {
    uint32_t hash = 2166136261u;
    probe_t *fv;
    int index;

    start_title();
    fv = the_help();
    for (index = 0; index < 3; index++) {
        draw_layers(ENTITY_MESSAGE_DRAW);
        hash = fnv(hash, screen_hash());
        hash = fnv(hash, (uint32_t)size_of(particles_of(&fv->emitter)));
    }
    draw_layers(ENTITY_MESSAGE_DRAW_OVERLAY);
    hash = fnv(hash, screen_hash());
    for (index = 0; index < 3; index++) {
        update_entities();
        hash = fold_fixed_member(hash, fv->header.state_time);
        hash = fold_string_member(hash, entity_state_name(fv->header.state));
        assert(entity_list_listed(live_entities(), fv));
    }
    {
        const int link_x = P8_SCREEN_WIDTH / 2;
        const int link_y = P8_SCREEN_HEIGHT / 2 + 48;
        const int logo_y = P8_SCREEN_HEIGHT / 2 - 54;
        const int box_left = P8_SCREEN_WIDTH / 2 - 40;
        const int box_top = P8_SCREEN_HEIGHT / 2 + 42;

        assert(class_of(fv)->hit_test(&fv->header, mouse_pos(point(link_x, logo_y), point(link_x, link_y))) == 1);
        assert(class_of(fv)->hit_test(&fv->header, mouse_pos(point(link_x, link_y), point(link_x, logo_y))) == 0);
        assert(class_of(fv)->hit_test(&fv->header, mouse_pos(point(box_left, box_top), point(box_left, box_top))) == 1);
        assert(class_of(fv)->hit_test(&fv->header, mouse_pos(point(box_left - 1, box_top), point(box_left - 1, box_top))) == 0);
    }
    hash = fnv(hash, (uint32_t)size_of(particles_of(&fv->emitter)));
    check("life", hash, LIFE_HASH);
}

/* The logo, 128 pixels wide, is centered: drawn alone, its rows hold
 * nothing left of W/2 - 64 or right of W/2 + 63, and reach both columns. */
static void test_logo_is_centered(void) {
    probe_t *fv;
    int left = P8_SCREEN_WIDTH;
    int right = -1;
    int x;
    int y;

    start_title();
    fv = the_help();
    p8_cls(&memory);
    help_screen_draw(&graphics, &memory, fv, vec_make(0, 0));
    for (y = 0; y < P8_SCREEN_HEIGHT / 2 - 20; y++) {
        for (x = 0; x < P8_SCREEN_WIDTH; x++) {
            if (memory.screen[y][x] != 0) {
                left = x < left ? x : left;
                right = x > right ? x : right;
            }
        }
    }
    assert(left == P8_SCREEN_WIDTH / 2 - 64 && right == P8_SCREEN_WIDTH / 2 + 63);
}

static p8_input_frame_t g_mouse;

static void mouse_at(int x, int y) {
    memset(&g_mouse, 0, sizeof g_mouse);
    g_mouse.mouse_x = x;
    g_mouse.mouse_y = y;
    input.frames = &g_mouse;
    input.frame_count = 1;
    input.current_frame = 0;
    dispatch_context_set_input(&input);
    update_entities();
}

/* The mouse over "need help?": main_ui()'s hovered, is_hovered, the help
 * in layer 10 of hovered_by_layer; then out, is_hovered false. */
static void test_hover(void) {
    uint32_t h = 2166136261u;
    probe_t *fv;

    start_title();
    fv = the_help();
    mouse_at(P8_SCREEN_WIDTH / 2, P8_SCREEN_HEIGHT / 2 + 48);
    ui_update(&memory, main_ui());
    assert((void *)ui_hovered(main_ui()) == fv);
    assert(is_hovered_state(fv) == 2);
    assert(in_layer(10, fv));
    h = fnv(h, is_hovered_state(fv));

    mouse_at(P8_SCREEN_WIDTH / 2, P8_SCREEN_HEIGHT / 2 - 54);
    ui_update(&memory, main_ui());
    assert((void *)ui_hovered(main_ui()) != fv);
    assert(is_hovered_state(fv) == 0);
    assert(!in_layer(10, fv));
    h = fnv(h, is_hovered_state(fv));
    check("hover", h, HOVER_HASH);
}

/* Gives back each particle of the list and leaves it empty. */
static void empty_particles(pointer_list_t *qy) {
    while (qy->count > 0) {
        void *particle = qy->items[0];
        pointer_list_del(qy, particle);
        allocator_release(particle_zone(PARTICLE_SLOT_PARTICLE), particle);
    }
}

/* A particle of the help is drawn in the help's own trail colors: at life
 * 6.5, less the frame's 0.14, index 6 is colour 9, where the probe's five
 * have none. */
static void test_particle_takes_the_help_palette(void) {
    particle_t *particle = particle_take();
    pointer_list_t *qy;
    vec_t ra;
    probe_t *fv;
    int at_x;
    int at_y;

    start_title();
    fv = the_help();
    assert(pos_ref(&fv->header) != NULL);
    ra = *pos_ref(&fv->header);
    particle->pos = vec_make(0, 0);
    particle->velocity = vec_make(0, 0);
    particle->has_acceleration = 0;
    particle->life = P8_FIXED_LIT(0x00068000, 6.5);
    qy = particles_of(&fv->emitter);
    empty_particles(qy);
    pointer_list_add(qy, particle);
    p8_camera_reset(&graphics);
    at_x = p8_fixed_flr_int(ra.x);
    at_y = p8_fixed_flr_int(ra.y);
    memory.screen[at_y][at_x] = 3;
    emitter_update(&graphics, &memory, &fv->header);
    assert(memory.screen[at_y][at_x] == help_screen_trail_colors_seq.items[5] && help_screen_trail_colors_seq.items[5] == 9);
}

/* cart_start_game(), as a difficulty button starts it, takes the help out of
 * the live entities, the probe tag and the draw and hit_test message lists. */
static void test_start_game_retires_the_help(void) {
    void *fv;

    start_title();
    fv = the_help();
    cart_start_game(NULL);
    assert(!entity_list_listed(live_entities(), fv));
    assert(!entity_list_listed(entities_by_tag(TAG_PROBE), fv) && !entity_list_listed(entities_by_message(ENTITY_MESSAGE_DRAW), fv) && !entity_list_listed(entities_by_message(ENTITY_MESSAGE_HIT_TEST), fv));
}

/* The title's help is a struct whose descriptor shares the probe's 4
 * inherited entries and holds its own draw and hit_test. Ready from its
 * init to its shutdown. */
static void test_help_screen_is_a_struct(void) {
    const entity_class_t *cls = help_screen_class();
    const entity_class_t *g_ = probe_class();
    void *fv;
    int j;

    start_title();
    fv = the_help();
    assert(fv != NULL && ((const entity_t *)fv)->class == cls);
    assert(cls->drag != NULL && cls->drag == g_->drag && cls->drop != NULL && cls->drop == g_->drop && cls->spawn_particle != NULL &&
           cls->spawn_particle == g_->spawn_particle);
    assert(cls->attach_slipway == NULL && cls->offer_trade == NULL && cls->draw_at == NULL);
    assert(cls->draw_overlay != NULL && cls->draw_overlay == g_->draw_overlay);
    assert(cls->draw == help_screen_draw_method && cls->hit_test != NULL);
    assert(cls->new_year == NULL && cls->refresh == NULL && cls->has_refresh == NULL && cls->left_press == NULL &&
           cls->accept_drop == NULL);
    for (j = 0; j < STATE_COUNT; j++) {
        assert(cls->states[j] == NULL);
    }
    assert(help_screen_ready());

    help_shutdown();
    assert(!help_screen_ready());
    help_init();
    assert(help_screen_ready());
}

static void call_emitter_init(void *e) {
    emitter_init(e);
}

static void call_emitter_spawn(void *e) {
    emitter_spawn(e, p8_fixed_from_int(1));
}

static void call_emitter_update(void *e) {
    emitter_update(&graphics, &memory, e);
}

static void call_probe_spawn_particle(void *e) {
    (void)probe_spawn_particle(e);
}

static void call_targeting_track(void *e) {
    targeting_track(e, mouse_pos(point(1, 1), point(2, 2)));
}

static void call_targeting_tooltip(void *e) {
    targeting_tooltip(&graphics, &memory, e);
}

static void call_targeting_confirm(void *e) {
    (void)targeting_confirm(e);
}

static void call_contains_cursor(void *e) {
    (void)entity_geom_contains_cursor(e, mouse_pos(point(1, 1), point(2, 2)));
}

static void call_bob(void *e) {
    planet_bob(e, p8_fixed_from_int(1));
}

/* On a NULL instance: targeting_confirm() and entity_geom_contains_cursor()
 * return 0; probe_spawn_particle() draws three random numbers, divides by
 * zero angle slices and returns a new particle; the six others do nothing. */
static void test_probe_closed(void) {
    static void (*const fns[])(void *) = {call_emitter_init, call_emitter_spawn, call_emitter_update, call_probe_spawn_particle, call_targeting_track,
                                          call_targeting_tooltip, call_targeting_confirm, call_contains_cursor, call_bob};
    size_t i;

    start_title();
    assert(probe_ready());
    for (i = 0; i < sizeof fns / sizeof fns[0]; i++) {
        fns[i](NULL);
    }
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

/* What an emitter holds in the particle zone, by sort. */
typedef struct {
    int particle;
    int particles;
    int particles_help_screen;
    int rows;
} held_t;

static held_t held_now(void) {
    held_t h;
    h.particle = particle_zone_live(PARTICLE_SLOT_PARTICLE);
    h.particles = particle_zone_live(PARTICLE_SLOT_LIST);
    h.particles_help_screen = particle_zone_live(PARTICLE_SLOT_HELP_LIST);
    h.rows = ui_zone_live(SLOT_KIND_ROW);
    return h;
}

/* The help that cart_start_game() takes out gives back, at the exit queue's
 * flush, its particles and its particle list, the one slot of its sort. */
static void test_start_game_gives_back_the_help_particles(void) {
    probe_t *fv;
    held_t title;
    held_t after;

    start_title();
    fv = the_help();
    emitter_spawn(&fv->header, p8_fixed_from_int(5));
    title = held_now();
    assert(title.particles_help_screen == 1 && particles_of(&fv->emitter)->count >= 5);
    assert(title.particle == particles_of(&fv->emitter)->count);
    cart_start_game(NULL);
    cart_flush_exit_queue();
    after = held_now();
    assert(after.particles_help_screen == 0 && after.particle == 0);
    assert(particles_of(&fv->emitter) == NULL);
}

/* A drawn title frame takes exactly one game-stream draw, whatever its
 * particles do, so a new game's galaxy depends on the title's frame count
 * only; the particles draw from the visual stream. */
static void test_a_drawn_title_frame_takes_one_game_draw(void) {
    start_title();
    for (int i = 0; i < 40; i++) {
        uint32_t want = p8_fixed_rnd_state();
        uint32_t visual = visual_random_state();
        (void)p8_fixed_rnd_from_state(&want, P8_FIXED_INT(1));
        cart_update60();
        cart_draw();
        assert(p8_fixed_rnd_state() == want && "a drawn title frame takes one game draw");
        assert(visual_random_state() != visual && "the title's particles draw from the visual stream");
    }
}

enum { TITLE_SETTLE_FRAMES = 30, RESUME_TOP = 41, RESUME_BOTTOM = 51, FORGIVING_TOP = 52 };

typedef unsigned char screen_t[P8_SCREEN_HEIGHT][P8_SCREEN_WIDTH];

static const button_t *title_button(const char *name) {
    const entity_list_t *bucket = entities_by_message(ENTITY_MESSAGE_HIT_TEST);
    for (int i = 0; i < bucket->count; i++) {
        const button_t *b = (const button_t *)bucket->items[i];
        if (b->header.class == button_class() && b->name != NULL && strcmp(b->name, name) == 0) return b;
    }
    return NULL;
}

/* In a child: a game played and saved, the save kept or erased, then the
 * title settled from the same random state; its screen comes back through a pipe. */
static void title_after_a_saved_game(int keep_save, screen_t out) {
    int fds[2];
    pid_t child;
    int status;

    assert(pipe(fds) == 0);
    fflush(stdout);
    child = fork();
    assert(child >= 0);
    if (child == 0) {
        uint8_t bytes[SAVE_STATE_MAX_BYTES];

        close(fds[0]);
        start_title();
        cart_start_game((button_t *)title_button("forgiving"));
        cart_update60();
        economy_new_year(main_economy_typed());
        cart_update60();
        assert(save_storage_read(bytes, (int)sizeof bytes) > 0 && "the end turn wrote a save");
        if (!keep_save) assert(save_storage_erase());
        p8_fixed_set_rnd_state(0x7171E5u);
        cart_init();
        for (int i = 0; i < TITLE_SETTLE_FRAMES; i++) {
            cart_update60();
            cart_draw();
        }
        assert((title_button("resume") != NULL) == keep_save);
        assert(write(fds[1], memory.screen, sizeof memory.screen) == (ssize_t)sizeof memory.screen);
        _exit(0);
    }
    close(fds[1]);
    {
        size_t got = 0;
        while (got < sizeof(screen_t)) {
            ssize_t n = read(fds[0], (unsigned char *)out + got, sizeof(screen_t) - got);
            assert(n > 0);
            got += (size_t)n;
        }
    }
    close(fds[0]);
    assert(waitpid(child, &status, 0) == child);
    assert(WIFEXITED(status) && WEXITSTATUS(status) == 0);
}

static void test_a_save_adds_the_resume_row_and_moves_nothing_else(void) {
    static screen_t empty;
    static screen_t saved;

    title_after_a_saved_game(0, empty);
    title_after_a_saved_game(1, saved);
    for (int y = 0; y < P8_SCREEN_HEIGHT; y++) {
        if (y < RESUME_TOP || y > RESUME_BOTTOM) {
            assert(memcmp(empty[y], saved[y], P8_SCREEN_WIDTH) == 0 && "the resume row moves no other row");
        } else {
            for (int x = 0; x < P8_SCREEN_WIDTH; x++) assert(empty[y][x] == 0 && "the band under the logo is free");
        }
    }
    assert(memcmp(saved[RESUME_TOP], saved[FORGIVING_TOP], P8_SCREEN_WIDTH) == 0 && "the same top border as forgiving");
    assert(memcmp(saved[RESUME_TOP + 1], saved[FORGIVING_TOP + 1], P8_SCREEN_WIDTH) == 0 && "the same fill as forgiving");
    assert(memcmp(saved[RESUME_BOTTOM - 1], saved[FORGIVING_TOP + 1], P8_SCREEN_WIDTH) == 0 && "the same fill as forgiving");
    assert(memcmp(saved[RESUME_BOTTOM], saved[FORGIVING_TOP + RESUME_BOTTOM - RESUME_TOP], P8_SCREEN_WIDTH) == 0 &&
           "the same bottom border as forgiving");
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
    in_fresh_child(test_a_drawn_title_frame_takes_one_game_draw);
    in_fresh_child(test_built);
    in_fresh_child(test_life);
    in_fresh_child(test_hover);
    in_fresh_child(test_logo_is_centered);
    in_fresh_child(test_particle_takes_the_help_palette);
    in_fresh_child(test_start_game_retires_the_help);
    in_fresh_child(test_help_screen_is_a_struct);
    in_fresh_child(test_start_game_gives_back_the_help_particles);
    in_fresh_child(test_probe_closed);
    test_a_save_adds_the_resume_row_and_moves_nothing_else();

    printf("test_help_screen: all tests passed\n");
    return 0;
}
