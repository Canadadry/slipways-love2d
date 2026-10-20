/* Linked under ASan. Each case runs in a child process: this binary reruns
 * itself with the case name, and the parent reads its exit status and its
 * stderr. */

#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/wait.h>

#include "p8/sprite_sheet.h"
#include "engine/bootstrap.h"
#include "game/cart.h"
#include "engine/dispatch_context.h"
#include "game/economy_score.h"
#include "game/main_economy.h"
#include "engine/entity.h"
#include "engine/entity_id.h"
#include "engine/entity_registry.h"
#include "engine/exit_queue.h"
#include "p8/gfx.h"
#include "p8/input.h"
#include "p8/vram.h"
#include "game/message_box.h"
#include "game/score_panel.h"
#include "game/ui.h"
#include "game/ui_button.h"

#define STDERR_LOG "tmp/test_exit_queue_stderr.txt"

/* volatile: the read through a stale pointer must not be dropped by the
 * optimiser, it is what the case observes. */
static volatile unsigned char sink;

static int released;

static void release_count(entity_t *self) {
    (void)self;
    released++;
}

static void release_frees(entity_t *self) {
    free(self);
}

static const entity_class_t counted_cls = {.release = release_count};
static const entity_class_t freed_cls = {.release = release_frees};
static const entity_class_t kept_cls = {0};

/* A message box: the shown message box and the score panel's last box only
 * ever point to boxes. It carries is_hovered, so that ui_update_hover can
 * write it through previous_hovered. */
static message_box_t *new_entity(const entity_class_t *cls) {
    message_box_t *mh = calloc(1, sizeof *mh);
    assert(mh != NULL);
    mh->header.class = cls;
    mh->header.has |= ENTITY_HAS_IS_HOVERED;
    return mh;
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
    /* The title's message box replaced the ui's ephemerals: the first frame
     * flushes the queue. */
    cart_flush_exit_queue();
}

static int case_entity_unregister_alive_until_flush(void) {
    load_cart_and_start();
    message_box_t *e = new_entity(&freed_cls);
    entity_register(&e->header);
    entity_unregister(&e->header);
    assert(exit_queue_count() == 1 && "entity_unregister queues a live entity that has something to release");
    sink = (unsigned char)e->header.is_dead;
    cart_update60();
    assert(exit_queue_count() == 0 && "cart_update60 flushes the queue");
    return 0;
}

static int case_entity_unregister_released_at_flush(void) {
    load_cart_and_start();
    message_box_t *e = new_entity(&freed_cls);
    entity_register(&e->header);
    entity_unregister(&e->header);
    cart_update60();
    sink = (unsigned char)e->header.is_dead;
    return 0;
}

/* ui_clear_ephemerals replaces the ui's ephemerals; the frame still reads
 * the old list. */
static entity_id_list_t replaced_ephemerals(void) {
    load_cart_and_start();
    const entity_id_list_t old = main_ui()->ephemerals;
    assert((main_ui()->has & UI_HAS_OWN_EPHEMERALS) && old.capacity > 0);
    ui_clear_ephemerals(main_ui());
    assert(exit_queue_count() == 1 && "ui_clear_ephemerals queues the list it replaces");
    return old;
}

static int case_ephemerals_alive_until_flush(void) {
    const entity_id_list_t old = replaced_ephemerals();
    sink = (unsigned char)old.items[0].index;
    cart_update60();
    assert(exit_queue_count() == 0);
    return 0;
}

static int case_ephemerals_released_at_flush(void) {
    const entity_id_list_t old = replaced_ephemerals();
    cart_update60();
    sink = (unsigned char)old.items[0].index;
    return 0;
}

static int case_entity_unregister_outside_live_entities_or_without_release(void) {
    load_cart_and_start();
    message_box_t *absent = new_entity(&counted_cls);
    message_box_t *kept = new_entity(&kept_cls);
    entity_unregister(&absent->header);
    assert(exit_queue_count() == 0 && "an entity outside the live list is not queued");
    entity_register(&kept->header);
    entity_unregister(&kept->header);
    assert(exit_queue_count() == 1 && "a class without release is queued too: the flush frees its id");
    entity_unregister(&kept->header);
    assert(exit_queue_count() == 1 && "unregistered twice, queued once");
    cart_update60();
    assert(exit_queue_count() == 0 && released == 0);
    return 0;
}

static int registered_with_release(void) {
    const entity_list_t *hv = live_entities();
    int n = 0;
    for (int i = 0; i < hv->count; i++) n += hv->items[i]->class->release != NULL;
    return n;
}

static int case_reset_world_enqueues_registered(void) {
    load_cart_and_start();
    message_box_t *counted = new_entity(&counted_cls);
    message_box_t *kept = new_entity(&kept_cls);
    entity_register(&counted->header);
    entity_register(&kept->header);
    int expected = registered_with_release();
    reset_world(NULL);
    assert(exit_queue_count() == expected && "reset_world queues every registered entity that has something to release");
    entity_unregister(&counted->header);
    assert(exit_queue_count() == expected && "entity_unregister after reset_world: the entity is no longer live, not queued twice");
    assert(released == 0 && "reset_world releases nothing before the end of the frame");
    cart_load();
    assert(released == 1 && exit_queue_count() == 0 && "cart_load flushes the queue");
    return 0;
}

/* The largest reset_world burst a worst-case/ game has reached (136 with a
 * release), with margin: keep it when those games are re-authored lower. */
#define FULL_GAME_LIVE_ENTITIES 150

static int case_reset_world_after_a_full_game(void) {
    load_cart_and_start();
    while (registered_with_release() < FULL_GAME_LIVE_ENTITIES) entity_register(&new_entity(&counted_cls)->header);
    reset_world(NULL);
    assert(exit_queue_count() == FULL_GAME_LIVE_ENTITIES);
    return 0;
}

static int case_shutdown_drains(void) {
    load_cart_and_start();
    message_box_t *e = new_entity(&counted_cls);
    message_box_t *alive = new_entity(&kept_cls);
    entity_register(&e->header);
    entity_unregister(&e->header);
    entity_construct(&alive->header);
    entity_id_t alive_id = entity_id_of(&alive->header);
    cart_shutdown();
    assert(released == 1 && exit_queue_count() == 0 &&
           "cart_shutdown flushes the queue: no entry survives until the next cart_load");
    assert(entity_get(alive_id) == NULL && entity_id_live() == 0 && "cart_shutdown leaves no table behind");
    free(alive);
    return 0;
}

static message_box_t *pointed_at_everywhere(void) {
    load_cart_and_start();
    cart_start_game(NULL);
    message_box_t *e = new_entity(&freed_cls);
    entity_construct(&e->header);
    main_ui()->hovered = entity_id_link(&e->header);
    main_ui()->previous_hovered = entity_id_link(&e->header);
    message_box_set_shown(e);
    score_panel_set_last_box(main_score_panel(), e);
    entity_unregister(&e->header);
    return e;
}

static int case_flush_forgets_references(void) {
    message_box_t *e = pointed_at_everywhere();
    assert(ui_hovered(main_ui()) == &e->header && ui_previous_hovered(main_ui()) == &e->header &&
           "an unregistered entity still resolves until the flush");
    cart_flush_exit_queue();
    assert(ui_hovered(main_ui()) == NULL && ui_previous_hovered(main_ui()) == NULL);
    assert(shown_message_box() == NULL && score_panel_last_box(main_score_panel()) == NULL);
    return 0;
}

static int case_update60_after_forgetting(void) {
    (void)pointed_at_everywhere();
    cart_update60();
    cart_draw();
    assert(shown_message_box() == NULL && score_panel_last_box(main_score_panel()) == NULL);
    return 0;
}

/* cart_start_game drops the title, and the difficulty button with it;
 * economy_score_difficulty still returns it for the whole game. */
static int case_difficulty_outlives_its_button(void) {
    load_cart_and_start();
    const entity_list_t *hd = entities_by_message(ENTITY_MESSAGE_HIT_TEST);
    button_t *nj = NULL;
    for (int i = 0; i < hd->count && nj == NULL; i++)
        if (hd->items[i]->class == button_class() && ((button_t *)hd->items[i])->has_type_bias)
            nj = (button_t *)hd->items[i];
    assert(nj != NULL);
    const char *px = nj->name;
    p8_fixed_t trade = nj->trade;
    p8_fixed_t en = nj->type_bias;
    cart_start_game(nj);
    cart_update60();
    cart_draw();
    const button_t *dk = economy_score_difficulty();
    assert(dk != NULL && dk->name == px && dk->has_trade && dk->trade == trade && dk->has_type_bias && dk->type_bias == en);
    return 0;
}

static int case_singletons_resolve_until_the_flush(void) {
    load_cart_and_start();
    cart_start_game(NULL);
    score_panel_t *score_panel = main_score_panel();
    ui_t *ui = main_ui();
    entity_t *mouse = main_mouse();
    assert(score_panel != NULL && ui != NULL && main_mouse_typed() != NULL);
    entity_unregister(&score_panel->header);
    entity_unregister(&ui->header);
    entity_unregister(mouse);
    assert(main_score_panel() == score_panel && main_ui() == ui && main_mouse() == mouse);
    cart_flush_exit_queue();
    assert(main_score_panel() == NULL && main_ui() == NULL && main_mouse() == NULL && main_mouse_typed() == NULL);
    return 0;
}

static int case_singletons_of_a_previous_world_never_resolve(void) {
    load_cart_and_start();
    cart_start_game(NULL);
    assert(main_economy_typed() != NULL && main_score_panel() != NULL);
    message_box_t *box = message_box_new(NULL);
    score_panel_set_last_box(main_score_panel(), box);
    score_panel_t *score_panel = main_score_panel();
    assert(shown_message_box() == box && score_panel_last_box(score_panel) == box);
    reset_world(NULL);
    assert(main_economy() == NULL && main_economy_typed() == NULL && main_economy_placeholder() == NULL);
    assert(main_score_panel() == NULL && shown_message_box() == NULL && score_panel_last_box(score_panel) == NULL);
    return 0;
}

static int case_main_economy_placeholder_survives_reset_world(void) {
    load_cart_and_start();
    assert(main_economy_placeholder() != NULL && main_economy() == &main_economy_placeholder()->header);
    reset_world(NULL);
    assert(main_economy_placeholder() != NULL && main_economy_typed() == NULL);
    return 0;
}

static int case_difficulty_is_copied_at_the_choice(void) {
    static button_t chosen;
    load_cart_and_start();
    chosen.name = "chosen";
    chosen.has_type_bias = 1;
    chosen.type_bias = p8_fixed_from_int(2);
    economy_score_set_difficulty(&chosen);
    memset(&chosen, 0, sizeof chosen);
    const button_t *dk = economy_score_difficulty();
    assert(dk != NULL && dk != &chosen && strcmp(dk->name, "chosen") == 0 && dk->has_type_bias &&
           dk->type_bias == p8_fixed_from_int(2) && !dk->has_trade);
    economy_score_set_difficulty(NULL);
    assert(economy_score_difficulty() == NULL);
    return 0;
}

static int case_full(void) {
    message_box_t *e = new_entity(&counted_cls);
    for (int i = 0; i <= EXIT_QUEUE_CAP; i++) exit_queue_put(&e->header);
    return 0;
}

static int case_max_survives_flush(void) {
    released = 0;
    message_box_t *e = new_entity(&counted_cls);
    assert(exit_queue_max() == 0);
    for (int i = 0; i < 3; i++) exit_queue_put(&e->header);
    exit_queue_flush();
    assert(exit_queue_count() == 0);
    assert(exit_queue_max() == 3 && "the flush empties the queue, not its peak");
    exit_queue_put(&e->header);
    exit_queue_flush();
    assert(exit_queue_max() == 3 && "a smaller batch leaves the peak alone");
    assert(released == 4);
    free(e);
    return 0;
}

static const struct {
    const char *name;
    int (*run)(void);
} CASES[] = {
    {"max_survives_flush", case_max_survives_flush},
    {"entity_unregister_alive_until_flush", case_entity_unregister_alive_until_flush},
    {"entity_unregister_released_at_flush", case_entity_unregister_released_at_flush},
    {"ephemerals_alive_until_flush", case_ephemerals_alive_until_flush},
    {"ephemerals_released_at_flush", case_ephemerals_released_at_flush},
    {"entity_unregister_outside_live_entities_or_without_release", case_entity_unregister_outside_live_entities_or_without_release},
    {"reset_world_enqueues_registered", case_reset_world_enqueues_registered},
    {"reset_world_after_a_full_game", case_reset_world_after_a_full_game},
    {"shutdown_drains", case_shutdown_drains},
    {"flush_forgets_references", case_flush_forgets_references},
    {"update60_after_forgetting", case_update60_after_forgetting},
    {"difficulty_outlives_its_button", case_difficulty_outlives_its_button},
    {"singletons_resolve_until_the_flush", case_singletons_resolve_until_the_flush},
    {"singletons_of_a_previous_world_never_resolve", case_singletons_of_a_previous_world_never_resolve},
    {"main_economy_placeholder_survives_reset_world", case_main_economy_placeholder_survives_reset_world},
    {"difficulty_is_copied_at_the_choice", case_difficulty_is_copied_at_the_choice},
    {"full", case_full},
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

static void expect_freed_crash(const char *name) {
    char *err;
    int status = run_child(name, &err);
    if (WEXITSTATUS(status) == 0 || strstr(err, "heap-use-after-free") == NULL) {
        fprintf(stderr, "%s: expected heap-use-after-free, exit %d\n%s", name, WEXITSTATUS(status), err);
        assert(0 && "an entity read after the queue flush must crash under ASan");
    }
    free(err);
}

static void expect_poison_crash(const char *name) {
    char *err;
    int status = run_child(name, &err);
    if (WEXITSTATUS(status) == 0 || strstr(err, "use-after-poison") == NULL) {
        fprintf(stderr, "%s: expected use-after-poison, exit %d\n%s", name, WEXITSTATUS(status), err);
        assert(0 && "a list read after the queue flush must crash under ASan");
    }
    free(err);
}

static void expect_fatal(const char *name, const char *message) {
    char *err;
    int status = run_child(name, &err);
    if (WEXITSTATUS(status) != 3 || strstr(err, message) == NULL) {
        fprintf(stderr, "%s: expected exit 3 and \"%s\", exit %d\n%s",
                name, message, WEXITSTATUS(status), err);
        assert(0 && "a full exit queue must call p8_fatal");
    }
    free(err);
}

int main(int argc, char **argv) {
    if (argc == 2) {
        for (size_t i = 0; i < CASE_COUNT; i++)
            if (strcmp(argv[1], CASES[i].name) == 0) return CASES[i].run();
        fprintf(stderr, "test_exit_queue: unknown case %s\n", argv[1]);
        return 2;
    }
    self_path = argv[0];
    expect_clean("entity_unregister_alive_until_flush");
    expect_freed_crash("entity_unregister_released_at_flush");
    expect_clean("ephemerals_alive_until_flush");
    expect_poison_crash("ephemerals_released_at_flush");
    expect_clean("entity_unregister_outside_live_entities_or_without_release");
    expect_clean("reset_world_enqueues_registered");
    expect_clean("reset_world_after_a_full_game");
    expect_clean("shutdown_drains");
    expect_clean("flush_forgets_references");
    expect_clean("update60_after_forgetting");
    expect_clean("difficulty_outlives_its_button");
    expect_clean("singletons_resolve_until_the_flush");
    expect_clean("singletons_of_a_previous_world_never_resolve");
    expect_clean("main_economy_placeholder_survives_reset_world");
    expect_clean("difficulty_is_copied_at_the_choice");
    expect_fatal("full", "exit queue full, limit 315");
    expect_clean("max_survives_flush");
    remove(STDERR_LOG);
    printf("test_exit_queue: all tests passed\n");
    return 0;
}
