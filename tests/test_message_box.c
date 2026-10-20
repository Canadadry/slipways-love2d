/* The message box, built the two ways the game builds it
 * (score_panel_new_year() from the rewritten year banner,
 * score_panel_update() from the score rows) and lived through the real
 * update_entities() and draw_layers(ENTITY_MESSAGE_DRAW_OVERLAY). */

#define _POSIX_C_SOURCE 200809L

#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <signal.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>

#include "p8/sprite_sheet.h"
#include "engine/bootstrap.h"
#include "game/cart.h"
#include "engine/entity_class.h"
#include "engine/dispatch_context.h"
#include "engine/entity_registry.h"
#include "p8/fixed.h"
#include "p8/gfx.h"
#include "p8/input.h"
#include "p8/vram.h"
#include "game/message_box.h"
#include "engine/entity_id.h"
#include "game/economy_score.h"
#include "game/main_economy.h"
#include "game/score_panel.h"
#include "game/score_panel_display.h"
#include "engine/allocator.h"
#include "engine/frame_zone.h"
#include "game/ui_zone.h"
#include "game/layout.h"
#include "game/draw_functions.h"
#include "game/hud.h"
#include "game/text_image.h"
#include "p8/draw_backend.h"

#define YEAR_MESSAGE_TICKS 192
#define YEAR_MESSAGE_SEQUENCE_HASH 0xE6D93DEAu
#define YEAR_SILENT_TICKS 104
#define YEAR_SILENT_SEQUENCE_HASH 0x7D2BC640u
#define TWO_YEARS_TICKS 213
#define TWO_YEARS_SEQUENCE_HASH 0x5A23C244u
#define SCORES_TICKS 113
#define SCORES_SEQUENCE_HASH 0xAEC98B0Eu

#define MAX_BOXES 4

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

static uint32_t screen_hash_of_blank(void) {
    p8_cls(&memory);
    return screen_hash();
}

static uint32_t draw_hash(void) {
    p8_gfx_init(&graphics);
    p8_cls(&memory);
    draw_layers(ENTITY_MESSAGE_DRAW_OVERLAY);
    return screen_hash();
}

static int entity_list_listed(const entity_list_t *list, const void *e) {
    int i;
    for (i = 0; list != NULL && i < list->count; i++) {
        if (list->items[i] == e) {
            return 1;
        }
    }
    return 0;
}

static int entity_list_count_of(const entity_list_t *list) {
    return list != NULL ? list->count : 0;
}

/* The kind words the frozen hashes fold before a value: any other value
 * moves them. */
enum { FIXED_KIND = 0, BOOL_KIND = 2 };

/* The kind words are constant by the struct's shape, kept so that the
 * sequence hashes still prove the typed reads. */
static uint32_t fold_box(uint32_t h, const message_box_t *e) {
    h = fnv(fnv(h, (uint32_t)FIXED_KIND), (uint32_t)e->header.state_time);
    if (e->header.is_dead) {
        h = fnv(fnv(h, (uint32_t)BOOL_KIND), (uint32_t)e->header.is_dead);
    } else {
        h = fnv(h, 0xDEADu);
    }
    h = fnv(h, (uint32_t)(shown_message_box() == e));
    h = fnv(h, (uint32_t)entity_list_listed(live_entities(), e));
    h = fnv(h, (uint32_t)entity_list_listed(entities_by_message(ENTITY_MESSAGE_DRAW_OVERLAY), e));
    return fnv(h, (uint32_t)entity_list_listed(entities_by_tag(TAG_MESSAGE_BOX), e));
}

typedef struct {
    const message_box_t *boxes[MAX_BOXES];
    int count;
    uint32_t hash;
    int ticks;
    int drew;
} run_t;

static void run_start(run_t *run) {
    run->count = 0;
    run->hash = 2166136261u;
    run->ticks = 0;
    run->drew = 0;
}

static void track(run_t *run, const message_box_t *box) {
    assert(box != NULL && run->count < MAX_BOXES);
    run->boxes[run->count++] = box;
}

static int any_listed(const run_t *run) {
    int i;
    for (i = 0; i < run->count; i++) {
        if (entity_list_listed(live_entities(), run->boxes[i])) {
            return 1;
        }
    }
    return 0;
}

/* One game tick: frame_zone_reset() and update_entities() as cart_update60()
 * does them, then draw_layers(ENTITY_MESSAGE_DRAW_OVERLAY) on a fresh
 * framebuffer, folded with every tracked box's state. */
static void tick(run_t *run) {
    uint32_t drawn;
    int i;

    frame_zone_reset();
    update_entities();
    drawn = draw_hash();
    p8_cls(&memory);
    run->drew |= drawn != screen_hash();
    run->hash = fnv(run->hash, drawn);
    for (i = 0; i < run->count; i++) {
        run->hash = fold_box(run->hash, run->boxes[i]);
    }
    run->ticks++;
    assert(run->ticks < 5000);
}

static void run_out(run_t *run) {
    while (any_listed(run)) {
        tick(run);
    }
}

static void run_check(const run_t *run, int want_ticks, uint32_t want_sequence, const char *label) {
    int i;

    for (i = 0; i < run->count; i++) {
        assert(!entity_list_listed(entities_by_message(ENTITY_MESSAGE_DRAW_OVERLAY), run->boxes[i]));
        assert(!entity_list_listed(entities_by_tag(TAG_MESSAGE_BOX), run->boxes[i]));
        assert(shown_message_box() != run->boxes[i]);
    }
    assert(run->drew);
    printf("[%s] ticks=%d sequence=0x%08Xu\n", label, run->ticks, (unsigned)run->hash);
    fflush(stdout);
    assert(run->ticks == want_ticks);
    assert(run->hash == want_sequence);
}

/* score_panel_new_year() discards the box it builds; message_box_construct()
 * has just made it the shown message box. */
static const message_box_t *year_box(int yr) {
    const message_box_t *box;
    int before = entity_list_count_of(entities_by_tag(TAG_MESSAGE_BOX));

    score_panel_new_year(p8_fixed_from_int(yr));
    box = shown_message_box();
    assert(box != NULL);
    assert(entity_list_count_of(entities_by_tag(TAG_MESSAGE_BOX)) == before + 1);
    assert(entity_list_listed(entities_by_tag(TAG_MESSAGE_BOX), box) && entity_list_listed(live_entities(), box) && entity_list_listed(entities_by_message(ENTITY_MESSAGE_DRAW_OVERLAY), box));
    return box;
}

/* 3425 is the first countdown row, "last year!": keep_open counts down 90
 * calls. */
static void test_year_with_a_message(void) {
    run_t run;

    reset_world(NULL);
    cart_flush_exit_queue();
    run_start(&run);
    track(&run, year_box(3425));
    run.hash = fnv(run.hash, draw_hash());
    run_out(&run);
    run_check(&run, YEAR_MESSAGE_TICKS, YEAR_MESSAGE_SEQUENCE_HASH, "year with a message");
}

/* 3400 has no message: the banner's third article is dropped and the
 * previous year's keep_open, its countdown already spent, is copied again. */
static void test_year_without_a_message(void) {
    run_t run;

    reset_world(NULL);
    cart_flush_exit_queue();
    run_start(&run);
    track(&run, year_box(3400));
    run.hash = fnv(run.hash, draw_hash());
    run_out(&run);
    run_check(&run, YEAR_SILENT_TICKS, YEAR_SILENT_SEQUENCE_HASH, "year without a message");
}

/* The second score_panel_new_year() marks the first box dead through the
 * shown message box, and rewrites the banner's year text, a row the first
 * box still shares, before it is drawn one last time. */
static void test_two_years_in_a_row(void) {
    run_t run;
    const message_box_t *first;
    const message_box_t *second;
    int i;

    reset_world(NULL);
    cart_flush_exit_queue();
    run_start(&run);
    first = year_box(3424);
    track(&run, first);
    for (i = 0; i < 20; i++) {
        tick(&run);
    }
    assert(!first->header.is_dead);
    second = year_box(3421);
    track(&run, second);
    assert(first->header.is_dead);
    run.hash = fnv(run.hash, draw_hash());
    tick(&run);
    assert(!entity_list_listed(live_entities(), first));
    run_out(&run);
    run_check(&run, TWO_YEARS_TICKS, TWO_YEARS_SEQUENCE_HASH, "two years in a row");
}

static void set_is_shown(score_panel_t *ju, int on) {
    ju->is_shown = on;
    ju->has_is_shown = 1;
}

/* A game started by cart_start_game() with no difficulty picked (a NULL
 * difficulty reads as difficulty 0), then reset_world() again: the live
 * entities hold only the main camera, mouse and user interface. The game's
 * economy and score panel take a slot of the new table, unregistered, to
 * stay the main ones. */
static score_panel_t *game_main_score_panel(void) {
    economy_t *economy;
    score_panel_t *ju;

    cart_start_game(NULL);
    economy = main_economy_typed();
    ju = main_score_panel();
    reset_world(NULL);
    cart_flush_exit_queue();
    entity_id_take(&economy->header);
    entity_id_take(&ju->header);
    probe_set_main_economy(&economy->header);
    score_panel_set_main(ju);
    assert(main_score_panel() == ju && main_economy_typed() == economy);
    return ju;
}

/* score_panel_update() on the game's one panel, its curtain already set: the
 * box of duration scale 3 stays open while is_shown, and is not rebuilt
 * while it is the panel's last box. */
static void test_scores_box_follows_is_shown(void) {
    run_t run;
    score_panel_t *ju = game_main_score_panel();
    const message_box_t *box;
    int built;
    int i;

    ju->curtain = p8_fixed_from_int(0);
    ju->has_curtain = 1;
    set_is_shown(ju, 1);
    run_start(&run);

    score_panel_update(ju);
    box = shown_message_box();
    assert(box != NULL);
    assert(score_panel_last_box(ju) == box);
    built = entity_list_count_of(entities_by_tag(TAG_MESSAGE_BOX));
    track(&run, box);
    run.hash = fnv(run.hash, draw_hash());

    for (i = 0; i < 80; i++) {
        score_panel_update(ju);
        assert(entity_list_count_of(entities_by_tag(TAG_MESSAGE_BOX)) == built);
        tick(&run);
        assert(entity_list_listed(live_entities(), box));
    }
    set_is_shown(ju, 0);
    while (any_listed(&run)) {
        score_panel_update(ju);
        assert(entity_list_count_of(entities_by_tag(TAG_MESSAGE_BOX)) == built);
        tick(&run);
    }
    run_check(&run, SCORES_TICKS, SCORES_SEQUENCE_HASH, "scores box follows is_shown");
}

/* The box's layer and duration scale are the class's generated defaults. */
static void test_defaults_match_the_parsed_spec(void) {
    const message_box_t *box;

    reset_world(NULL);
    cart_flush_exit_queue();
    box = year_box(3400);
    assert(box->archetype == &message_box_archetype);
    assert(box->archetype->layer == p8_fixed_from_int(14));
    assert(box->duration_scale == box->archetype->duration_scale);
    assert(box->duration_scale == p8_fixed_from_int(5));
    assert(box->header.class->tags == TAG(TAG_MESSAGE_BOX));
    assert(box->header.state == ENTITY_STATE_NORMAL);
    assert(message_box_ready());
    message_box_shutdown();
    assert(!message_box_ready());
    message_box_init();
}

static int g_keep_open_calls = 0;

static int counting_keep_open(message_box_t *self) {
    (void)self;
    g_keep_open_calls++;
    return 1;
}

/* Built by the score panel or directly, a box is a struct that lands in the
 * live entities, the draw_overlay message list and the message_box tag only;
 * its keep_open is a field the overlay draw calls, and it has no update
 * state for update_entities() to call. */
static void test_message_box_is_a_struct_registered_under_draw_overlay(void) {
    const entity_message_t others[] = {ENTITY_MESSAGE_DRAW, ENTITY_MESSAGE_HIT_TEST, ENTITY_MESSAGE_NEW_YEAR, ENTITY_MESSAGE_REFRESH};
    message_box_keys_t keys = {0};
    const message_box_t *from_score_panel;
    void *generic;
    int out;
    int i;

    reset_world(NULL);
    cart_flush_exit_queue();
    from_score_panel = year_box(3400);
    keys.keep_open = counting_keep_open;
    generic = message_box_new(&keys);

    assert(from_score_panel != NULL);
    assert(generic != NULL);
    assert(shown_message_box() == generic);
    assert(entity_list_listed(live_entities(), generic) && entity_list_listed(entities_by_message(ENTITY_MESSAGE_DRAW_OVERLAY), generic) && entity_list_listed(entities_by_tag(TAG_MESSAGE_BOX), generic));
    for (i = 0; i < 4; i++) {
        assert(!entity_list_listed(entities_by_message(others[i]), from_score_panel) && !entity_list_listed(entities_by_message(others[i]), generic));
    }
    assert(((message_box_t *)generic)->keep_open != NULL);
    out = ((message_box_t *)generic)->keep_open(generic);
    assert(g_keep_open_calls == 1 && out);
    assert(((const entity_t *)generic)->class->states[ENTITY_STATE_NORMAL] == NULL);
}

/* A NULL keep_open key leaves the box without keep_open, as a box built
 * with no key. */
static void test_message_box_nil_keep_open_key_reads_absent(void) {
    message_box_keys_t keys = {0};
    message_box_t *box;

    reset_world(NULL);
    cart_flush_exit_queue();
    keys.keep_open = NULL;
    box = message_box_new(&keys);
    assert(box->keep_open == NULL);
}


/* Closed the way its own timeout closes it; with no box shown it does nothing. */
static void test_close_shown_kills_the_shown_box_only(void) {
    message_box_keys_t keys = {0};
    message_box_t *first, *second;

    reset_world(NULL);
    cart_flush_exit_queue();
    message_box_close_shown();
    assert(shown_message_box() == NULL);
    first = message_box_new(&keys);
    second = message_box_new(&keys);
    assert(first->header.is_dead && shown_message_box() == second);
    message_box_close_shown();
    assert(second->header.is_dead && shown_message_box() == NULL);
    message_box_close_shown();
    assert(shown_message_box() == NULL);
}

/* A box takes a slot; once update_entities() takes it out at the end of its
 * run, emptying the exit queue gives the slot back. */
static void test_box_gives_its_slot_back(void) {
    run_t run;
    int base;

    reset_world(NULL);
    cart_flush_exit_queue();
    base = ui_zone_live(SLOT_KIND_MESSAGE_BOX);
    run_start(&run);
    track(&run, year_box(3400));
    assert(ui_zone_live(SLOT_KIND_MESSAGE_BOX) == base + 1);
    run_out(&run);
    assert(ui_zone_live(SLOT_KIND_MESSAGE_BOX) == base + 1 && "out of the live entities, still read this frame");
    cart_flush_exit_queue();
    assert(ui_zone_live(SLOT_KIND_MESSAGE_BOX) == base);
}

/* Fills the frame zone with garbage after a reset. */
static void scribble_frame_zone(void) {
    unsigned char *reuse;

    frame_zone_reset();
    reuse = allocator_alloc(frame_zone(), 4096);
    memset(reuse, 'X', 4096);
}

/* score_panel_update() makes its texts in the frame zone: the box
 * lays them out in its own slot and draws them on later frames, after the
 * zone is reset and reused. */
static void test_scores_box_keeps_its_articles(void) {
    score_panel_t *ju = game_main_score_panel();
    uint32_t want;

    ju->curtain = p8_fixed_from_int(0);
    ju->has_curtain = 1;
    set_is_shown(ju, 1);
    score_panel_update(ju);
    assert(shown_message_box() != NULL && score_panel_last_box(ju) == shown_message_box());
    want = draw_hash();
    scribble_frame_zone();
    assert(draw_hash() == want);
}

/* score_panel_new_year() makes its year label in the frame zone: the year
 * banner, which every year box shares, keeps its own copy. */
static void test_year_box_keeps_its_label(void) {
    uint32_t want;

    reset_world(NULL);
    cart_flush_exit_queue();
    (void)year_box(3424);
    want = draw_hash();
    scribble_frame_zone();
    assert(draw_hash() == want);
}

/* The largest box the game builds: score_panel_update()'s, with the longest
 * label ("current score" and the longest difficulty) and the longest
 * happiness text, a negative fraction. Its slot holds it without stopping. */
static void test_largest_scores_box_fits_its_slot(void) {
    static button_t challenging;
    score_panel_t *ju = game_main_score_panel();
    const button_t *dk = economy_score_difficulty();
    button_t saved = dk != NULL ? *dk : (button_t){0};

    challenging.name = "challenging";
    economy_score_set_difficulty(&challenging);
    main_economy_typed()->end_status = NULL;
    main_economy_typed()->has_end_status = 0;
    main_economy_typed()->happiness = P8_FIXED_LIT(0x80000007, -32767.9999);
    main_economy_typed()->has_happiness = 1;
    ju->curtain = p8_fixed_from_int(0);
    ju->has_curtain = 1;
    set_is_shown(ju, 1);
    score_panel_update(ju);
    assert(shown_message_box() != NULL && score_panel_last_box(ju) == shown_message_box());
    economy_score_set_difficulty(dk != NULL ? &saved : NULL);
}

/* A box copies the rows added to it: rewriting the caller's row and
 * text after the row is added changes nothing the box draws. */
static void test_box_copies_its_rows(void) {
    char text[] = "2 years remain";
    data_item_t items[] = {{.text = text}, {.number = P8_SCREEN_WIDTH / 2}, {.number = 40}, {.number = 9}, {.is_fixed = 1, .fixed = P8_FIXED_LIT(0x00008000, 0.5)}};
    data_row_t row = {.count = 5, .items = items, .draw_function = DRAW_FUNCTION_PRINT_OUTLINED};
    message_box_t *box;
    uint32_t want;

    reset_world(NULL);
    cart_flush_exit_queue();
    box = message_box_new(NULL);
    message_box_add_row(box, &row);
    box->header.state_time = p8_fixed_mul(box->duration_scale, p8_fixed_from_int(10));
    want = draw_hash();
    assert(want != screen_hash_of_blank());
    strcpy(text, "XXXXXXXXXXXXXX");
    memset(items + 1, 0, 4 * sizeof items[0]);
    assert(draw_hash() == want);
}

/* Runs `write` in a child process, which must stop on an assert. */
static void must_abort(void (*write)(void)) {
    int status;
    pid_t child;

    fflush(stdout);
    child = fork();
    assert(child >= 0);
    if (child == 0) {
        assert(freopen("/dev/null", "w", stderr) != NULL);
        write();
        _exit(0);
    }
    assert(waitpid(child, &status, 0) == child);
    assert(WIFSIGNALED(status) && WTERMSIG(status) == SIGABRT);
}

static void add_too_many_rows(void) {
    static const data_item_t bar_items[] = {{.number = 1}, {.number = 2}, {.number = 3}};
    static const data_row_t bar = {.count = 3, .items = bar_items, .draw_function = DRAW_FUNCTION_DRAW_BAR};
    message_box_t *box = message_box_new(NULL);
    int i;

    for (i = 0; i <= MESSAGE_BOX_ITEMS_CAP; i++) {
        message_box_add_row(box, &bar);
    }
}

static void add_six_arguments(void) {
    static const data_item_t six[] = {{.number = 1}, {.number = 2}, {.number = 3}, {.number = 4}, {.number = 5}, {.number = 6}};
    static const data_row_t row = {.count = 6, .items = six, .draw_function = DRAW_FUNCTION_SPR};
    message_box_add_row(message_box_new(NULL), &row);
}

static void add_long_text(void) {
    static const data_item_t text[] = {{.text = "seventeen letters"}, {.number = 2}};
    static const data_row_t row = {.count = 2, .items = text, .draw_function = DRAW_FUNCTION_PRINT_OUTLINED};
    message_box_add_row(message_box_new(NULL), &row);
}

static void add_text_second(void) {
    static const data_item_t text[] = {{.number = 2}, {.text = "late"}};
    static const data_row_t row = {.count = 2, .items = text, .draw_function = DRAW_FUNCTION_PRINT_OUTLINED};
    message_box_add_row(message_box_new(NULL), &row);
}

static void add_layout_row_as_row(void) {
    static const data_item_t three[] = {{.is_nil = 1}, {.is_nil = 1}, {.number = 0}};
    static const data_row_t row = {.count = 3, .items = three, .draw_function = DRAW_FUNCTION_DRAW_ROW};
    message_box_add_row(message_box_new(NULL), &row);
}

/* A box holds MESSAGE_BOX_ITEMS_CAP articles of five numbers at most, a
 * text of MESSAGE_BOX_TEXT_CAP letters in the first one only, and takes a
 * layout row only through message_box_add_layout_row(). */
static void test_article_limits_stop(void) {
    reset_world(NULL);
    cart_flush_exit_queue();
    must_abort(add_too_many_rows);
    must_abort(add_six_arguments);
    must_abort(add_long_text);
    must_abort(add_text_second);
    must_abort(add_layout_row_as_row);
}

/* A layout-row article draws what layout_row_draw_at() draws at its
 * position and alignment once the box has settled (no camera offset);
 * its texts are the box's own, not its keys'. */
static void test_layout_row_article_draws_its_row(void) {
    char text[] = "hi";
    layout_row_keys_t keys = {0};
    layout_row_t *reference;
    message_box_t *box;
    uint32_t want;

    reset_world(NULL);
    cart_flush_exit_queue();
    layout_row_keys_append(&keys, layout_row_cell_string(text));
    layout_row_keys_append(&keys, layout_row_cell_fixed(p8_fixed_from_int(12)));
    keys.has_color = 1;
    keys.color = p8_fixed_from_int(11);
    reference = layout_row_new(frame_zone(), &keys);
    p8_gfx_init(&graphics);
    p8_cls(&memory);
    layout_row_draw_at(&graphics, &memory, reference, vec_make(p8_fixed_from_int(64), p8_fixed_from_int(30)),
                       P8_FIXED_LIT(0x00008000, 0.5));
    want = screen_hash();

    box = message_box_new(NULL);
    message_box_add_layout_row(box, &keys, vec_make(p8_fixed_from_int(64), p8_fixed_from_int(30)), 1, P8_FIXED_LIT(0x00008000, 0.5));
    assert(box->articles_count == 1 && box->articles[0].draw_function == DRAW_FUNCTION_DRAW_ROW);
    text[0] = 'X';
    scribble_frame_zone();
    box->header.state_time = p8_fixed_mul(box->duration_scale, p8_fixed_from_int(10));
    p8_gfx_init(&graphics);
    p8_cls(&memory);
    message_box_draw_overlay(&graphics, &memory, box);
    assert(screen_hash() == want);
}

/* The box's articles drawn the way they were before the text image path: glyph by glyph. */
static void draw_articles_glyph_by_glyph(p8_graphics_t *gfx, p8_vram_t *mem, const message_box_t *self) {
    p8_fixed_t t = p8_fixed_sub(p8_fixed_div(self->header.state_time, self->duration_scale), p8_fixed_from_int(10));
    p8_fixed_t nc = p8_fixed_div(p8_fixed_neg(p8_fixed_mul(p8_fixed_mul(t, t), t)), p8_fixed_from_int(9));
    int i;

    for (i = 0; i < self->articles_count; i++) {
        const message_box_article_t *article = &self->articles[i];

        p8_camera(gfx, p8_fixed_flr_int(nc), 0);
        nc = p8_fixed_neg(nc);
        if (article->kind == MESSAGE_BOX_ARTICLE_SHARED_ROW) {
            draw_functions_draw_row(gfx, mem, article->as.row_of());
        } else if (article->kind == MESSAGE_BOX_ARTICLE_LAYOUT_ROW) {
            layout_row_draw_at(gfx, mem, &article->as.layout.row, article->as.layout.pos, article->as.layout.align);
        } else {
            data_item_t items[MESSAGE_BOX_ARGUMENTS_CAP];
            data_row_t row = {.count = article->as.scalars.count, .items = items, .draw_function = article->draw_function};
            int k;

            for (k = 0; k < row.count; k++) {
                const message_box_argument_t *argument = &article->as.scalars.arguments[k];

                items[k] = argument->kind == MESSAGE_BOX_ARGUMENT_TEXT    ? (data_item_t){.text = article->as.scalars.text}
                           : argument->kind == MESSAGE_BOX_ARGUMENT_FALSE ? (data_item_t){.is_false = 1}
                           : argument->kind == MESSAGE_BOX_ARGUMENT_NIL   ? (data_item_t){.is_nil = 1}
                                                                          : (data_item_t){.is_fixed = 1, .fixed = argument->number};
            }
            draw_functions_draw_row(gfx, mem, &row);
        }
    }
    p8_camera_reset(gfx);
}

static int g_copies;
static const p8_draw_backend_t *g_software;

static void count_image_draw(const p8_graphics_t *gfx, p8_vram_t *mem, int slot, int x, int y) {
    g_copies++;
    g_software->image_draw(gfx, mem, slot, x, y);
}

/* keep_open is left out: it would pin state_time and hide the slide-out shake. */
static void assert_box_draws_like_glyph_by_glyph(message_box_t *box) {
    static const int tenths[] = {0, 40, 95, 100, 103, 104};
    static p8_vram_t direct;
    message_box_keep_open_fn_t keep_open = box->keep_open;
    p8_fixed_t state_time = box->header.state_time;
    size_t k;

    box->keep_open = NULL;
    for (k = 0; k < sizeof tenths / sizeof tenths[0]; k++) {
        p8_graphics_t direct_gfx;
        int copies = g_copies;

        box->header.state_time = p8_fixed_div(p8_fixed_mul(box->duration_scale, p8_fixed_from_int(tenths[k])), p8_fixed_from_int(10));
        text_image_frame_begin();
        p8_gfx_init(&graphics);
        direct_gfx = graphics;
        p8_cls(&memory);
        hud_select_palette_block_default(&memory);
        direct = memory;
        draw_articles_glyph_by_glyph(&direct_gfx, &direct, box);
        message_box_draw_overlay(&graphics, &memory, box);
        assert(memcmp(direct.screen, memory.screen, sizeof direct.screen) == 0);
        assert(memcmp(direct.draw_palette, memory.draw_palette, sizeof direct.draw_palette) == 0);
        assert(graphics.camera_x == 0 && graphics.camera_y == 0);
        assert(g_copies > copies && "the box's text is copied from text slots");
        assert(!box->header.is_dead);
    }
    box->keep_open = keep_open;
    box->header.state_time = state_time;
}

static int box_has(const message_box_t *box, message_box_article_kind_t kind) {
    int i;

    for (i = 0; i < box->articles_count; i++) {
        if (box->articles[i].kind == kind) {
            return 1;
        }
    }
    return 0;
}

static void test_box_text_draws_through_text_slots_like_glyph_by_glyph(void) {
    static p8_draw_backend_t counting;
    score_panel_t *ju;
    message_box_t *year;
    const p8_draw_backend_t *previous;

    g_software = p8_gfx_software_backend();
    counting = *g_software;
    counting.image_draw = count_image_draw;
    previous = p8_draw_backend_set(&counting);

    reset_world(NULL);
    cart_flush_exit_queue();
    year = (message_box_t *)year_box(3425);
    assert(box_has(year, MESSAGE_BOX_ARTICLE_SHARED_ROW) && box_has(year, MESSAGE_BOX_ARTICLE_SCALARS));
    assert_box_draws_like_glyph_by_glyph(year);

    ju = game_main_score_panel();
    ju->curtain = p8_fixed_from_int(0);
    ju->has_curtain = 1;
    set_is_shown(ju, 1);
    score_panel_update(ju);
    assert(shown_message_box() != NULL && box_has(shown_message_box(), MESSAGE_BOX_ARTICLE_LAYOUT_ROW));
    assert_box_draws_like_glyph_by_glyph(shown_message_box());

    p8_draw_backend_set(previous);
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
    cart_init();

    test_year_with_a_message();
    test_year_without_a_message();
    test_two_years_in_a_row();
    test_scores_box_follows_is_shown();
    test_defaults_match_the_parsed_spec();
    test_message_box_is_a_struct_registered_under_draw_overlay();
    test_message_box_nil_keep_open_key_reads_absent();
    test_close_shown_kills_the_shown_box_only();
    test_scores_box_keeps_its_articles();
    test_box_gives_its_slot_back();
    test_year_box_keeps_its_label();
    test_largest_scores_box_fits_its_slot();
    test_box_copies_its_rows();
    test_article_limits_stop();
    test_layout_row_article_draws_its_row();
    test_box_text_draws_through_text_slots_like_glyph_by_glyph();

    printf("test_message_box: all tests passed\n");
    return 0;
}
