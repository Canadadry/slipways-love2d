/* Linked under ASan. Each case runs in a child process: this binary reruns
 * itself with the case name, and the parent reads its exit status and its
 * stderr. */

#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/wait.h>

#include "p8/sprite_sheet.h"
#include "engine/entity_id.h"
#include "game/cart.h"
#include "engine/collect.h"
#include "engine/dispatch_context.h"
#include "engine/entity_registry.h"
#include "engine/entity_list.h"
#include "game/main_economy.h"
#include "p8/gfx.h"
#include "p8/input.h"
#include "game/layout.h"
#include "p8/vram.h"
#include "game/menu.h"
#include "game/message_box.h"
#include "game/planet.h"
#include "game/planet_display.h"
#include "game/score_panel.h"
#include "p8/str.h"
#include "game/ui.h"
#include "game/ui_button.h"
#include "engine/allocator.h"
#include "engine/frame_zone.h"
#include "engine/zone_sizes.h"
#include "heap_allocator.h"

#define STDERR_LOG "tmp/test_frame_zone_stderr.txt"

/* volatile: the read through a stale pointer must not be dropped by the
 * optimiser, it is what the case observes. */
static volatile unsigned char sink;

static unsigned char *counting_block(void) {
    unsigned char *block = allocator_alloc(frame_zone(), 64);
    for (int i = 0; i < 64; i++) block[i] = (unsigned char)i;
    return block;
}

static void allocate_again(void) {
    for (int i = 0; i < 4; i++) memset(allocator_alloc(frame_zone(), 64), 2, 64);
}

static int case_lazy_init(void) {
    unsigned char *block = counting_block();
    assert(block[63] == 63);
    return 0;
}

static int case_reset_before_first_use(void) {
    frame_zone_reset();
    unsigned char *block = counting_block();
    assert(block[63] == 63);
    return 0;
}

static int case_alive_within_frame(void) {
    unsigned char *block = counting_block();
    allocate_again();
    for (int i = 0; i < 64; i++) assert(block[i] == i && "a frame block lives until the reset");
    return 0;
}

static int case_dead_next_frame(void) {
    unsigned char *block = counting_block();
    frame_zone_reset();
    allocate_again();
    sink = block[63];
    return 0;
}

static p8_vram_t memory;
static p8_graphics_t graphics;
static p8_input_t input;

static void load_cart_and_start(void) {
    p8_vram_init(&memory);
    p8_gfx_init(&graphics);
    p8_sprite_sheet_load(&memory);
    p8_input_init(&input);
    dispatch_context_set_gfx(&graphics);
    dispatch_context_set_mem(&memory);
    dispatch_context_set_input(&input);
    cart_load();
    cart_init();
}

static int case_update60_alive_until_next(void) {
    load_cart_and_start();
    cart_update60();
    unsigned char *block = counting_block();
    cart_draw();
    sink = block[63];
    return 0;
}

static int case_update60_resets(void) {
    load_cart_and_start();
    cart_update60();
    unsigned char *block = counting_block();
    cart_draw();
    cart_update60();
    allocate_again();
    sink = block[63];
    return 0;
}

static void start_game(void) {
    load_cart_and_start();
    cart_start_game(NULL);
}

static size_t read_placed_texts(const layout_row_t *lq) {
    size_t sum = 0;
    for (int i = 0; i < lq->placed_items_count; i++) {
        if (lq->placed_items[i].kind == LAYOUT_ROW_ITEM_TEXT) sum += strlen(lq->placed_items[i].text);
    }
    return sum;
}

/* Resets the frame zone once the panel is built, then rereads and draws
 * each button, tooltip included: returns the number of tooltips drawn. */
static int reset_then_draw_panel(void) {
    const entity_id_list_t *le = &main_ui()->ephemerals;
    int bubbles = 0;

    frame_zone_reset();
    allocate_again();
    for (int i = 0; i < le->count; i++) {
        button_t *js = (button_t *)entity_id_list_at(le, i);
        vec_t p = entity_pos(&js->header);

        sink = (unsigned char)read_placed_texts(js->layout_row);
        if (js->side_tooltip != NULL) sink = (unsigned char)read_placed_texts(js->side_tooltip);
        if (js->tooltip != NULL) {
            for (int k = 0; k < js->tooltip->rows_count; k++) sink = (unsigned char)read_placed_texts(js->tooltip->rows[k]);
            bubbles++;
        }
        js->header.is_hovered = 1;
        button_draw(&graphics, &memory, js, p);
        button_draw_overlay(&graphics, &memory, js);
    }
    return bubbles;
}

static int case_inspection_panel_rows_survive_reset(void) {
    static production_t kw = {.has_resource = 1};
    planet_t *planet;

    start_game();
    planet = (planet_t *)entities_by_tag(TAG_PLANET)->items[0];
    kw.resource = p8_fixed_from_int(99);
    kw.amount = p8_fixed_from_int(1);
    planet->production = kw;
    planet->has |= PLANET_HAS_PRODUCTION;
    planet_inspection_panel(planet);
    assert(main_ui()->ephemerals.count >= 2 && "planet_inspection_panel: the icon and the exports row");
    reset_then_draw_panel();
    return 0;
}

static uint32_t row_screen_hash(const layout_row_t *row) {
    uint32_t h = 2166136261u;

    p8_cls(&memory);
    layout_row_draw_at_default(&graphics, &memory, row, vec_make(p8_fixed_from_int(8), p8_fixed_from_int(60)));
    for (int y = 0; y < P8_SCREEN_HEIGHT; y++) {
        for (int x = 0; x < P8_SCREEN_WIDTH; x++) h = (h ^ memory.screen[y][x]) * 16777619u;
    }
    return h;
}

/* An amount above 5 puts a frame-zone text in the row's cells. */
static int case_planet_row_survives_reset(void) {
    static production_t kw = {.has_resource = 1};
    planet_t *planet;
    uint32_t before;

    start_game();
    planet = (planet_t *)entities_by_tag(TAG_PLANET)->items[0];
    kw.resource = p8_fixed_from_int(21);
    kw.amount = p8_fixed_from_int(6);
    planet->production = kw;
    planet->has |= PLANET_HAS_PRODUCTION;
    planet_refresh(planet);
    assert(planet->layout_row != NULL && planet->layout_row->placed_items_count > 0);
    before = row_screen_hash(planet->layout_row);
    frame_zone_reset();
    allocate_again();
    sink = (unsigned char)read_placed_texts(planet->layout_row);
    assert(row_screen_hash(planet->layout_row) == before && "planet_refresh: the stored row draws the same after the frame");
    return 0;
}

static int case_invented_lines_survive_reset(void) {
    start_game();
    pointer_list_add(&main_economy_typed()->invented, "slipgates");
    menu_show_tech_tree_screen();
    assert(reset_then_draw_panel() >= 1 && "menu_show_tech_tree_screen: the invented-count row carries its tooltip");
    return 0;
}

static int case_research_project_tooltip_survives_reset(void) {
    planet_t *planet;

    start_game();
    planet = (planet_t *)entities_by_tag(TAG_PLANET)->items[0];
    planet_roll_type(planet);
    (void)planet_left_press(planet);
    assert(reset_then_draw_panel() >= 1 && "planet_projects_panel: a building carries the research_project tooltip");
    return 0;
}

static int happiness_cells(const message_box_t *mh) {
    int found = 0;
    for (int k = 0; k < mh->articles_count; k++) {
        if (mh->articles[k].draw_function != DRAW_FUNCTION_DRAW_ROW) continue;
        const layout_row_t *lq = &mh->articles[k].as.layout.row;
        for (int i = 0; i < lq->placed_items_count; i++)
            found += lq->placed_items[i].kind == LAYOUT_ROW_ITEM_TEXT && strcmp(lq->placed_items[i].text, "1%") == 0;
    }
    return found;
}

static int case_final_score_happiness_survives_reset(void) {
    economy_t *fm;
    score_panel_t *ju;

    start_game();
    fm = main_economy_typed();
    fm->happiness = p8_fixed_from_int(1);
    fm->has_happiness = 1;
    fm->end_status = "final score";
    fm->has_end_status = 1;
    ju = main_score_panel();
    ju->header.class->states[ENTITY_STATE_NORMAL](&ju->header, 0);
    assert(shown_message_box() != NULL && "score_panel_update_method: a finished game opens the score message box");
    frame_zone_reset();
    allocate_again();
    assert(happiness_cells(shown_message_box()) == 1 && "score_panel_update_method: the happiness cell keeps its copy of the final score");
    return 0;
}

#define LIST_LENGTH 12

static entity_t cells[LIST_LENGTH];

static int is_even_cell(entity_t *e, void *ctx) {
    (void)ctx;
    return (e - cells) % 2 == 0;
}

static entity_list_t even_cells(void) {
    entity_list_t all;
    entity_list_init(&all, frame_zone());
    for (int i = 0; i < LIST_LENGTH; i++) entity_list_add(&all, &cells[i]);
    return entity_list_filter(frame_zone(), &all, is_even_cell, NULL);
}

static int case_list_alive_within_frame(void) {
    entity_list_t even = even_cells();
    assert(even.count == LIST_LENGTH / 2);
    for (int i = 0; i < LIST_LENGTH / 2; i++) assert(even.items[i] == &cells[2 * i]);
    return 0;
}

static int case_list_dead_next_frame(void) {
    entity_list_t even = even_cells();
    frame_zone_reset();
    allocate_again();
    sink = (unsigned char)(even.items[0] == &cells[0]);
    return 0;
}

static char *frame_text(const char *s) {
    return p8_concat2(frame_zone(), s, "");
}

static layout_row_t *row_of_frame_text(const allocator_t *zone, const char *s) {
    layout_row_keys_t keys = {0};

    layout_row_keys_append(&keys, layout_row_cell_string(frame_text(s)));
    return layout_row_new(zone, &keys);
}

/* The texts of a cell come from the frame zone: after the reset, the panel
 * still draws its copy, and a tuple cell made from a frame-zone row still
 * lays out. */
static int case_panel_texts_survive_reset(void) {
    data_item_t line = {.text = frame_text("bubble")};
    data_row_t bubble_row = {.count = 1, .items = &line};

    load_cart_and_start();
    layout_row_keys_t two_keys = {0};
    layout_row_keys_append(&two_keys, layout_row_cell_string(frame_text("row")));
    layout_row_keys_append(&two_keys, layout_row_cell_string(frame_text("second")));
    layout_row_t *row = layout_row_new(&heap_allocator, &two_keys);
    tooltip_t *bubble = tooltip_new(&heap_allocator, &bubble_row);
    data_item_t *tuple_items = (data_item_t *)allocator_alloc(frame_zone(), 2 * sizeof *tuple_items);
    data_row_t *tuple = (data_row_t *)allocator_alloc(frame_zone(), sizeof *tuple);
    tuple_items[0] = (data_item_t){.number = 12};
    tuple_items[1] = (data_item_t){.number = 5};
    *tuple = (data_row_t){.count = 2, .items = tuple_items};
    layout_row_keys_t tuple_keys = {0};
    layout_row_keys_append(&tuple_keys, layout_row_cell_row(tuple));
    layout_row_t *of_tuple;
    frame_zone_reset();
    allocate_again();
    of_tuple = layout_row_new(&heap_allocator, &tuple_keys);
    sink = (unsigned char)(read_placed_texts(row) + read_placed_texts(bubble->rows[0]));
    layout_row_draw_at_default(&graphics, &memory, row, vec_make(0, 0));
    tooltip_draw_at(&graphics, &memory, bubble, 0, 0);
    layout_row_draw_at_default(&graphics, &memory, of_tuple, vec_make(0, 0));
    return 0;
}

static int case_panel_struct_in_zone(void) {
    load_cart_and_start();
    layout_row_t *row = row_of_frame_text(frame_zone(), "row");
    frame_zone_reset();
    allocate_again();
    sink = (unsigned char)row->placed_items_count;
    return 0;
}

static int case_bubble_struct_in_zone(void) {
    load_cart_and_start();
    tooltip_t *bubble = tooltip_new(frame_zone(), NULL);
    frame_zone_reset();
    allocate_again();
    sink = (unsigned char)bubble->rows_count;
    return 0;
}

static int case_full(void) {
    allocator_alloc(frame_zone(), FRAME_ZONE_BYTES / 2);
    allocator_alloc(frame_zone(), FRAME_ZONE_BYTES / 2 + 1);
    return 0;
}

static int case_alive_before_mark(void) {
    unsigned char *block = counting_block();
    frame_zone_mark_t mark = frame_zone_mark();
    allocate_again();
    frame_zone_rewind(mark);
    allocate_again();
    for (int i = 0; i < 64; i++) assert(block[i] == i && "a block taken before the mark outlives the rewind");
    return 0;
}

static int case_dead_after_rewind(void) {
    frame_zone_mark_t mark = frame_zone_mark();
    unsigned char *block = counting_block();
    frame_zone_rewind(mark);
    sink = block[63];
    return 0;
}

static int case_rewind_gives_room_back(void) {
    frame_zone_mark_t mark = frame_zone_mark();
    for (int i = 0; i < 8; i++) {
        allocator_alloc(frame_zone(), FRAME_ZONE_BYTES / 2);
        frame_zone_rewind(mark);
    }
    return 0;
}

static int case_rewind_across_reset(void) {
    allocate_again();
    frame_zone_mark_t mark = frame_zone_mark();
    frame_zone_reset();
    allocate_again();
    frame_zone_rewind(mark);
    return 0;
}

static int case_max_survives_rewind_and_reset(void) {
    frame_zone_reset();
    frame_zone_mark_t mark = frame_zone_mark();
    allocator_alloc(frame_zone(), 4096);
    size_t peak = frame_zone_used();
    assert(peak >= 4096);
    frame_zone_rewind(mark);
    assert(frame_zone_used() == mark.used && "the current follows the rewind");
    assert(frame_zone_max() == peak && "the peak keeps what was taken before the rewind");

    allocator_alloc(frame_zone(), 1024);
    assert(frame_zone_max() == peak && "a lower use leaves the peak alone");
    allocator_alloc(frame_zone(), 8192);
    size_t higher = frame_zone_used();
    frame_zone_reset();
    assert(frame_zone_used() == 0);
    assert(frame_zone_max() == higher && "the peak keeps what was taken before the reset");
    return 0;
}

static const struct {
    const char *name;
    int (*run)(void);
} CASES[] = {
    {"max_survives_rewind_and_reset", case_max_survives_rewind_and_reset},
    {"lazy_init", case_lazy_init},
    {"reset_before_first_use", case_reset_before_first_use},
    {"alive_within_frame", case_alive_within_frame},
    {"dead_next_frame", case_dead_next_frame},
    {"update60_alive_until_next", case_update60_alive_until_next},
    {"update60_resets", case_update60_resets},
    {"list_alive_within_frame", case_list_alive_within_frame},
    {"list_dead_next_frame", case_list_dead_next_frame},
    {"full", case_full},
    {"alive_before_mark", case_alive_before_mark},
    {"dead_after_rewind", case_dead_after_rewind},
    {"rewind_gives_room_back", case_rewind_gives_room_back},
    {"rewind_across_reset", case_rewind_across_reset},
    {"inspection_panel_rows_survive_reset", case_inspection_panel_rows_survive_reset},
    {"invented_lines_survive_reset", case_invented_lines_survive_reset},
    {"planet_row_survives_reset", case_planet_row_survives_reset},
    {"research_project_tooltip_survives_reset", case_research_project_tooltip_survives_reset},
    {"final_score_happiness_survives_reset", case_final_score_happiness_survives_reset},
    {"panel_texts_survive_reset", case_panel_texts_survive_reset},
    {"panel_struct_in_zone", case_panel_struct_in_zone},
    {"bubble_struct_in_zone", case_bubble_struct_in_zone},
};

#define CASE_COUNT (sizeof CASES / sizeof CASES[0])

/* Allocated NUL-terminated buffer, freed by the caller. */
static char *read_whole_file(const char *path) {
    FILE *file = fopen(path, "rb");
    assert(file != NULL);
    assert(fseek(file, 0, SEEK_END) == 0);
    long size = ftell(file);
    assert(size >= 0);
    rewind(file);
    char *buf = malloc((size_t)size + 1);
    assert(buf != NULL);
    assert(fread(buf, 1, (size_t)size, file) == (size_t)size);
    buf[size] = '\0';
    fclose(file);
    return buf;
}

static const char *self_path;

/* Returns the raw status of system(); *err receives the child's stderr,
 * freed by the caller. */
static int run_child(const char *name, char **err) {
    char cmd[512];
    remove(STDERR_LOG);
    snprintf(cmd, sizeof cmd, "%s %s > /dev/null 2> " STDERR_LOG, self_path, name);
    int status = system(cmd);
    assert(status != -1 && WIFEXITED(status));
    *err = read_whole_file(STDERR_LOG);
    return status;
}

static void expect_clean(const char *name) {
    char *err;
    int status = run_child(name, &err);
    if (WEXITSTATUS(status) != 0 || err[0] != '\0') {
        fprintf(stderr, "%s: expected clean, exit %d\n%s", name, WEXITSTATUS(status), err);
        assert(0 && "the case must neither crash nor write to stderr");
    }
    free(err);
}

static void expect_poison_crash(const char *name) {
    char *err;
    int status = run_child(name, &err);
    if (WEXITSTATUS(status) == 0 || strstr(err, "use-after-poison") == NULL) {
        fprintf(stderr, "%s: expected use-after-poison, exit %d\n%s", name, WEXITSTATUS(status), err);
        assert(0 && "a frame block read on the next frame must crash under ASan");
    }
    free(err);
}

static void expect_fatal(const char *name, const char *message) {
    char *err;
    int status = run_child(name, &err);
    if (WEXITSTATUS(status) != 3 || strstr(err, message) == NULL) {
        fprintf(stderr, "%s: expected exit 3 and \"%s\", exit %d\n%s",
                name, message, WEXITSTATUS(status), err);
        assert(0 && "a full frame zone must call p8_fatal");
    }
    free(err);
}

static void expect_assert(const char *name, const char *message) {
    char *err;
    int status = run_child(name, &err);
    if (WEXITSTATUS(status) == 0 || strstr(err, message) == NULL) {
        fprintf(stderr, "%s: expected an assert on \"%s\", exit %d\n%s", name, message, WEXITSTATUS(status), err);
        assert(0 && "the case must stop on its assert");
    }
    free(err);
}

int main(int argc, char **argv) {
    if (argc == 2) {
        for (size_t i = 0; i < CASE_COUNT; i++)
            if (strcmp(argv[1], CASES[i].name) == 0) return CASES[i].run();
        fprintf(stderr, "test_frame_zone: unknown case %s\n", argv[1]);
        return 2;
    }
    self_path = argv[0];
    expect_clean("lazy_init");
    expect_clean("reset_before_first_use");
    expect_clean("alive_within_frame");
    expect_poison_crash("dead_next_frame");
    expect_clean("update60_alive_until_next");
    expect_poison_crash("update60_resets");
    expect_clean("list_alive_within_frame");
    expect_poison_crash("list_dead_next_frame");
    expect_fatal("full", "zone frame full, limit 20256 bytes");
    expect_clean("alive_before_mark");
    expect_poison_crash("dead_after_rewind");
    expect_clean("rewind_gives_room_back");
    expect_assert("rewind_across_reset", "a mark from before a reset");
    expect_clean("max_survives_rewind_and_reset");
    expect_clean("inspection_panel_rows_survive_reset");
    expect_clean("invented_lines_survive_reset");
    expect_clean("planet_row_survives_reset");
    expect_clean("research_project_tooltip_survives_reset");
    expect_clean("final_score_happiness_survives_reset");
    expect_clean("panel_texts_survive_reset");
    expect_poison_crash("panel_struct_in_zone");
    expect_poison_crash("bubble_struct_in_zone");
    remove(STDERR_LOG);
    printf("test_frame_zone: all tests passed\n");
    return 0;
}
