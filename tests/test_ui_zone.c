/* Linked under ASan. Each case runs in a child process: this binary runs
 * itself again with the case name, and the parent reads its status and its
 * stderr. */

#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/wait.h>

#include "engine/allocator.h"
#include "engine/entity_id_list.h"
#include "game/ui_zone.h"

#define STDERR_LOG "tmp/test_ui_zone_stderr.txt"

/* volatile: the read through a stale pointer must not be removed by the
 * optimizer, it is what the case observes. */
static volatile unsigned char sink;

static int case_live_slots_follow_takes(void) {
    for (int s = 0; s < SLOT_KIND_COUNT; s++) assert(ui_zone_live((slot_kind_t)s) == 0);

    const allocator_t *a = ui_zone_slot(SLOT_KIND_BUTTON);
    const allocator_t *b = ui_zone_slot(SLOT_KIND_BUTTON);
    const allocator_t *c = ui_zone_slot(SLOT_KIND_TOAST);
    assert(ui_zone_live(SLOT_KIND_BUTTON) == 2);
    assert(ui_zone_live(SLOT_KIND_TOAST) == 1);
    assert(ui_zone_live(SLOT_KIND_MESSAGE_BOX) == 0);

    arena_release(a);
    assert(ui_zone_live(SLOT_KIND_BUTTON) == 1 && "releasing a slot uncounts its kind");
    arena_release(b);
    arena_release(c);
    for (int s = 0; s < SLOT_KIND_COUNT; s++) assert(ui_zone_live((slot_kind_t)s) == 0);
    return 0;
}

static int case_maxima_keep_the_peak(void) {
    assert(ui_zone_max_live(SLOT_KIND_MESSAGE_BOX) == 0 && ui_zone_max_bytes(SLOT_KIND_MESSAGE_BOX) == 0);
    assert(ui_zone_used() == 0 && ui_zone_max_used() == 0);

    const allocator_t *a = ui_zone_slot(SLOT_KIND_MESSAGE_BOX);
    const allocator_t *b = ui_zone_slot(SLOT_KIND_MESSAGE_BOX);
    allocator_alloc(a, 1000);
    size_t released_used = arena_used(a);
    size_t both = ui_zone_used();
    assert(both > 0);
    arena_release(a);
    assert(ui_zone_live(SLOT_KIND_MESSAGE_BOX) == 1);
    assert(ui_zone_max_live(SLOT_KIND_MESSAGE_BOX) == 2 && "the peak of live slots outlives their release");
    assert(ui_zone_max_bytes(SLOT_KIND_MESSAGE_BOX) == released_used && "the largest slot, header included, read at its release");
    assert(ui_zone_used() < both && ui_zone_max_used() == both && "the zone peak outlives the release");

    allocator_alloc(b, 2000);
    assert(ui_zone_max_bytes(SLOT_KIND_MESSAGE_BOX) == arena_used(b) && "a slot still live is measured when read");
    arena_release(b);
    assert(ui_zone_used() == 0);
    assert(ui_zone_max_live(SLOT_KIND_BUTTON) == 0 && "each kind keeps its own peak");
    return 0;
}

static int case_slot_alive_until_released(void) {
    const allocator_t *slot = ui_zone_slot(SLOT_KIND_ROW);
    unsigned char *text = allocator_alloc(slot, 64);
    memset(text, 5, 64);
    for (int i = 0; i < 64; i++) assert(text[i] == 5);
    arena_release(slot);
    return 0;
}

static int case_slot_dead_after_release(void) {
    const allocator_t *slot = ui_zone_slot(SLOT_KIND_ROW);
    unsigned char *text = allocator_alloc(slot, 64);
    memset(text, 5, 64);
    arena_release(slot);
    sink = text[63];
    return 0;
}

static int case_button_slot_full(void) {
    allocator_alloc(ui_zone_slot(SLOT_KIND_BUTTON), ui_zone_slot_size(SLOT_KIND_BUTTON));
    return 0;
}

static int case_modal_backdrop_zone_full(void) {
    for (int i = 0; i <= ui_zone_count(SLOT_KIND_MODAL_BACKDROP); i++) ui_zone_slot(SLOT_KIND_MODAL_BACKDROP);
    return 0;
}

static int case_row_slot_full(void) {
    allocator_alloc(ui_zone_slot(SLOT_KIND_ROW), ui_zone_slot_size(SLOT_KIND_ROW));
    return 0;
}

static const struct {
    const char *name;
    int (*run)(void);
} CASES[] = {
    {"live_slots_follow_takes", case_live_slots_follow_takes},
    {"slot_alive_until_released", case_slot_alive_until_released},
    {"maxima_keep_the_peak", case_maxima_keep_the_peak},
    {"slot_dead_after_release", case_slot_dead_after_release},
    {"button_slot_full", case_button_slot_full},
    {"row_slot_full", case_row_slot_full},
    {"modal_backdrop_zone_full", case_modal_backdrop_zone_full},
};

#define CASE_COUNT (sizeof CASES / sizeof CASES[0])

/* An allocated buffer, ending with a nul, which the caller frees. */
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
 * which the caller frees. */
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
        assert(0 && "a slot given back to its zone then read must crash under ASan");
    }
    free(err);
}

static void expect_fatal(const char *name, const char *message) {
    char *err;
    int status = run_child(name, &err);
    if (WEXITSTATUS(status) != 3 || strstr(err, message) == NULL) {
        fprintf(stderr, "%s: expected exit 3 and \"%s\", exit %d\n%s",
                name, message, WEXITSTATUS(status), err);
        assert(0 && "a full zone or a too small slot must call p8_fatal");
    }
    free(err);
}

int main(int argc, char **argv) {
    if (argc == 2) {
        for (size_t i = 0; i < CASE_COUNT; i++)
            if (strcmp(argv[1], CASES[i].name) == 0) return CASES[i].run();
        fprintf(stderr, "test_ui_zone: unknown case %s\n", argv[1]);
        return 2;
    }
    self_path = argv[0];
    assert(ui_zone_slot_size(SLOT_KIND_EPHEMERALS) ==
               ARENA_HEAD_BYTES + ZONE_BYTES(sizeof(entity_id_list_t)) +
                   ZONE_BYTES(UI_EPHEMERALS_RESERVE * sizeof(entity_id_t)) &&
           "an ephemerals slot is its arena head, list head and full reserve");
    expect_clean("live_slots_follow_takes");
    expect_clean("slot_alive_until_released");
    expect_clean("maxima_keep_the_peak");
    expect_poison_crash("slot_dead_after_release");
    expect_fatal("button_slot_full", "zone slot button full, limit ");
    expect_fatal("row_slot_full", "zone slot row full, limit ");
    expect_fatal("modal_backdrop_zone_full", "zone ui modal_backdrop full, limit 2 slots");
    remove(STDERR_LOG);
    printf("test_ui_zone: all tests passed\n");
    return 0;
}
