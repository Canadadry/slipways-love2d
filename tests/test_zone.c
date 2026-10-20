/* Linked under ASan. Each case runs in a child process: this binary reruns
 * itself with the case name, and the parent reads its exit status and its
 * stderr. */

#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/wait.h>

#include "engine/zone.h"

#define STDERR_LOG "tmp/test_zone_stderr.txt"

BUMP_ZONE_STORAGE(bump_cells, 64);
SLOT_ZONE_STORAGE(slot_cells, slot_taken, 16, 4);
FLIP_ZONE_STORAGE(flip_zone_cells, 64);

static bump_zone_t bump;
static slot_zone_t slots;
static flip_zone_t flip_zone;

static void init_zones(void) {
    bump_zone_init(&bump, "test-frame", bump_cells, 64);
    slot_zone_init(&slots, "test-slots", slot_cells, slot_taken, 16, 4);
    flip_zone_init(&flip_zone, "test-flip", flip_zone_cells, 64);
}

/* volatile: the read through a stale pointer must not be dropped by the
 * optimiser, it is what the case observes. */
static volatile unsigned char sink;

static int case_control(void) {
    unsigned char *a = bump_zone_alloc(&bump, 24);
    memset(a, 7, 24);
    sink = a[23];
    bump_zone_reset(&bump);
    unsigned char *b = bump_zone_alloc(&bump, 64);
    memset(b, 9, 64);
    sink = b[63];

    unsigned char *s = slot_zone_alloc(&slots);
    memset(s, 5, 16);
    sink = s[15];
    slot_zone_release(&slots, s);
    for (int i = 0; i < 4; i++) {
        unsigned char *t = slot_zone_alloc(&slots);
        memset(t, 3, 16);
        sink = t[0];
    }
    return 0;
}

static int case_bump_stale_after_reset(void) {
    unsigned char *a = bump_zone_alloc(&bump, 24);
    a[0] = 1;
    bump_zone_reset(&bump);
    for (int i = 0; i < 2; i++) {
        unsigned char *b = bump_zone_alloc(&bump, 24);
        memset(b, 2, 24);
    }
    sink = a[0];
    return 0;
}

static int case_slot_stale_after_release(void) {
    unsigned char *a = slot_zone_alloc(&slots);
    a[0] = 1;
    slot_zone_release(&slots, a);
    for (int i = 0; i < 3; i++) {
        unsigned char *b = slot_zone_alloc(&slots);
        memset(b, 2, 16);
    }
    sink = a[0];
    return 0;
}

static int case_bump_overflow_past_object(void) {
    unsigned char *a = bump_zone_alloc(&bump, 5);
    memset(a, 1, 5);
    sink = a[5];
    return 0;
}

static int case_bump_full(void) {
    bump_zone_alloc(&bump, 40);
    bump_zone_alloc(&bump, 40);
    return 0;
}

static int case_slot_taken_follows(void) {
    assert(slot_zone_taken(&slots) == 0);
    unsigned char *a = slot_zone_alloc(&slots);
    unsigned char *b = slot_zone_alloc(&slots);
    assert(slot_zone_taken(&slots) == 2);
    slot_zone_release(&slots, a);
    assert(slot_zone_taken(&slots) == 1 && "releasing a slot stops counting it");
    slot_zone_release(&slots, b);
    assert(slot_zone_taken(&slots) == 0);
    return 0;
}

static int case_slot_full(void) {
    for (int i = 0; i < 5; i++) slot_zone_alloc(&slots);
    return 0;
}

static int case_flip_zone_grab_arena_follows(void) {
    assert(flip_zone_used(&flip_zone) == 0);
    memset(flip_zone_alloc(&flip_zone, 5), 1, 5);
    assert(flip_zone_used(&flip_zone) == 8 && "rounded up to the cell");
    memset(flip_zone_alloc(&flip_zone, 16), 1, 16);
    assert(flip_zone_used(&flip_zone) == 24);
    assert(flip_zone_max(&flip_zone) == 24);
    return 0;
}

static void expect_filled(const unsigned char *p, size_t n, unsigned char v) {
    for (size_t i = 0; i < n; i++) assert(p[i] == v && "block overwritten");
}

static int case_flip_zone_old_generation_lives(void) {
    unsigned char *a = flip_zone_alloc(&flip_zone, 24);
    memset(a, 7, 24);
    flip_zone_flip(&flip_zone);
    for (int i = 0; i < 5; i++) memset(flip_zone_alloc(&flip_zone, 8), 9, 8);
    expect_filled(a, 24, 7);
    assert(flip_zone_used(&flip_zone) == 64 && "both generations count");
    return 0;
}

/* One generation per end: returns the first block of generation gen (0 at
 * the bottom, 1 at the top), after the next one has served three blocks. */
static unsigned char *flip_zone_then_serve(int gen) {
    unsigned char *kept = NULL;
    for (int g = 0; g <= gen + 1; g++) {
        if (g > 0) {
            flip_zone_flip(&flip_zone);
            flip_zone_release(&flip_zone);
        }
        unsigned char *a = flip_zone_alloc(&flip_zone, 8);
        a[0] = 1;
        if (g == gen) kept = a;
    }
    for (int i = 0; i < 2; i++) memset(flip_zone_alloc(&flip_zone, 8), 2, 8);
    return kept;
}

static int case_flip_zone_new_generation_after_release(void) {
    flip_zone_then_serve(1);
    return 0;
}

static int case_flip_zone_stale_bottom_after_release(void) {
    unsigned char *a = flip_zone_then_serve(0);
    sink = a[0];
    return 0;
}

static int case_flip_zone_stale_top_after_release(void) {
    unsigned char *a = flip_zone_then_serve(1);
    sink = a[0];
    return 0;
}

static int case_flip_zone_twice_without_release(void) {
    flip_zone_flip(&flip_zone);
    flip_zone_flip(&flip_zone);
    return 0;
}

static int case_flip_zone_ends_meet(void) {
    memset(flip_zone_alloc(&flip_zone, 40), 1, 40);
    flip_zone_flip(&flip_zone);
    memset(flip_zone_alloc(&flip_zone, 24), 2, 24);
    fprintf(stderr, "filled\n");
    flip_zone_alloc(&flip_zone, 8);
    return 0;
}

static int case_flip_zone_max_keeps_peak(void) {
    flip_zone_alloc(&flip_zone, 24);
    flip_zone_flip(&flip_zone);
    flip_zone_alloc(&flip_zone, 16);
    flip_zone_release(&flip_zone);
    assert(flip_zone_used(&flip_zone) == 16 && "the release stops counting the old generation");
    flip_zone_alloc(&flip_zone, 8);
    assert(flip_zone_used(&flip_zone) == 24);
    assert(flip_zone_max(&flip_zone) == 40 && "the maximum keeps the flip frame");
    return 0;
}

/* Two live generations, then the full reset. */
static unsigned char *reset_both_ends(unsigned char **top) {
    unsigned char *bottom = flip_zone_alloc(&flip_zone, 24);
    bottom[0] = 1;
    flip_zone_flip(&flip_zone);
    *top = flip_zone_alloc(&flip_zone, 16);
    (*top)[0] = 1;
    flip_zone_reset(&flip_zone);
    return bottom;
}

static int case_flip_zone_reset_empties(void) {
    unsigned char *top;
    reset_both_ends(&top);
    assert(flip_zone_used(&flip_zone) == 0);
    assert(flip_zone_max(&flip_zone) == 40 && "the reset leaves the session maximum alone");
    flip_zone_flip(&flip_zone);
    memset(flip_zone_alloc(&flip_zone, 64), 3, 64);
    return 0;
}

static int case_flip_zone_stale_bottom_after_reset(void) {
    unsigned char *top;
    unsigned char *bottom = reset_both_ends(&top);
    sink = bottom[0];
    return 0;
}

static int case_flip_zone_stale_top_after_reset(void) {
    unsigned char *top;
    reset_both_ends(&top);
    for (int i = 0; i < 3; i++) memset(flip_zone_alloc(&flip_zone, 8), 2, 8);
    sink = top[0];
    return 0;
}

static const struct {
    const char *name;
    int (*run)(void);
} CASES[] = {
    {"control", case_control},
    {"bump_stale_after_reset", case_bump_stale_after_reset},
    {"slot_stale_after_release", case_slot_stale_after_release},
    {"bump_overflow_past_object", case_bump_overflow_past_object},
    {"bump_full", case_bump_full},
    {"slot_taken_follows", case_slot_taken_follows},
    {"slot_full", case_slot_full},
    {"flip_zone_grab_arena_follows", case_flip_zone_grab_arena_follows},
    {"flip_zone_old_generation_lives", case_flip_zone_old_generation_lives},
    {"flip_zone_new_generation_after_release", case_flip_zone_new_generation_after_release},
    {"flip_zone_stale_bottom_after_release", case_flip_zone_stale_bottom_after_release},
    {"flip_zone_stale_top_after_release", case_flip_zone_stale_top_after_release},
    {"flip_zone_twice_without_release", case_flip_zone_twice_without_release},
    {"flip_zone_ends_meet", case_flip_zone_ends_meet},
    {"flip_zone_max_keeps_peak", case_flip_zone_max_keeps_peak},
    {"flip_zone_reset_empties", case_flip_zone_reset_empties},
    {"flip_zone_stale_bottom_after_reset", case_flip_zone_stale_bottom_after_reset},
    {"flip_zone_stale_top_after_reset", case_flip_zone_stale_top_after_reset},
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
        assert(0 && "the control case must not crash");
    }
    free(err);
}

static void expect_poison_crash(const char *name) {
    char *err;
    int status = run_child(name, &err);
    if (WEXITSTATUS(status) == 0 || strstr(err, "use-after-poison") == NULL) {
        fprintf(stderr, "%s: expected use-after-poison, exit %d\n%s", name, WEXITSTATUS(status), err);
        assert(0 && "an access outside the lifetime must crash under ASan");
    }
    free(err);
}

static void expect_assert(const char *name, const char *message) {
    char *err;
    int status = run_child(name, &err);
    if (WEXITSTATUS(status) == 0 || strstr(err, message) == NULL) {
        fprintf(stderr, "%s: expected an assert \"%s\", exit %d\n%s", name, message, WEXITSTATUS(status), err);
        assert(0 && "the case must fail on this assert");
    }
    free(err);
}

static void expect_fatal(const char *name, const char *zone, const char *limit) {
    char *err;
    int status = run_child(name, &err);
    if (WEXITSTATUS(status) != 3 || strstr(err, zone) == NULL || strstr(err, limit) == NULL) {
        fprintf(stderr, "%s: expected exit 3 naming %s and %s, exit %d\n%s",
                name, zone, limit, WEXITSTATUS(status), err);
        assert(0 && "a full zone must call p8_fatal");
    }
    free(err);
}

int main(int argc, char **argv) {
    init_zones();
    if (argc == 2) {
        for (size_t i = 0; i < CASE_COUNT; i++)
            if (strcmp(argv[1], CASES[i].name) == 0) return CASES[i].run();
        fprintf(stderr, "test_zone: unknown case %s\n", argv[1]);
        return 2;
    }
    self_path = argv[0];
    assert(ZONE_BYTES_BY_WIDTH(16u, 8u) == 16u && "the host has 8-byte pointers: the first column");
    expect_clean("control");
    expect_clean("slot_taken_follows");
    expect_clean("flip_zone_grab_arena_follows");
    expect_clean("flip_zone_old_generation_lives");
    expect_clean("flip_zone_new_generation_after_release");
    expect_clean("flip_zone_max_keeps_peak");
    expect_clean("flip_zone_reset_empties");
    expect_poison_crash("bump_stale_after_reset");
    expect_poison_crash("slot_stale_after_release");
    expect_poison_crash("bump_overflow_past_object");
    expect_poison_crash("flip_zone_stale_bottom_after_release");
    expect_poison_crash("flip_zone_stale_top_after_release");
    expect_poison_crash("flip_zone_stale_bottom_after_reset");
    expect_poison_crash("flip_zone_stale_top_after_reset");
    expect_assert("flip_zone_twice_without_release", "flush the exit queue between two game starts");
    expect_fatal("bump_full", "test-frame", " 64 ");
    expect_fatal("slot_full", "test-slots", " 4 ");
    expect_fatal("flip_zone_ends_meet", "filled\np8_fatal: zone test-flip full", " 64 ");
    remove(STDERR_LOG);
    printf("test_zone: all tests passed\n");
    return 0;
}
