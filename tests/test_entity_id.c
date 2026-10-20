#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/wait.h>

#include "engine/bootstrap.h"
#include "engine/entity.h"
#include "engine/entity_id.h"
#include "engine/entity_registry.h"
#include "engine/exit_queue.h"
#include "engine/game_zone.h"
#include "engine/zone_sizes.h"

#define STDERR_LOG "tmp/test_entity_id_stderr.txt"

static entity_t g_crowd[ENTITY_TABLE_CAP + 1];

static void no_link_resolves_to_null(void) {
    entity_id_t zeroed;
    memset(&zeroed, 0, sizeof zeroed);
    assert(entity_id_is_none(zeroed));
    assert(entity_id_equal(zeroed, ENTITY_ID_NONE));
    assert(entity_get(ENTITY_ID_NONE) == NULL);
}

static void table_takes_its_budgeted_block_from_the_game_zone(void) {
    game_zone_reset();
    size_t before = game_zone_used();
    entity_id_table_init(game_zone());
    assert(game_zone_used() - before == ENTITY_TABLE_BYTES);
}

static void new_world(void) {
    game_zone_reset();
    entity_id_table_init(game_zone());
}

static void taken_id_resolves_to_its_entity(void) {
    entity_t first = {0}, second = {0};
    new_world();
    entity_id_t first_id = entity_id_take(&first);
    entity_id_t second_id = entity_id_take(&second);
    assert(!entity_id_is_none(first_id));
    assert(!entity_id_equal(first_id, second_id));
    assert(entity_get(first_id) == &first);
    assert(entity_get(second_id) == &second);
    assert(entity_id_equal(entity_id_of(&first), first_id));
    assert(entity_id_equal(entity_id_of(&second), second_id));
}

static void released_slot_never_resolves_again(void) {
    entity_t dead = {0}, reborn = {0};
    new_world();
    entity_id_t dead_id = entity_id_take(&dead);
    entity_id_release(&dead);
    assert(entity_get(dead_id) == NULL);
    assert(entity_id_is_none(entity_id_of(&dead)));
    entity_id_t reborn_id = entity_id_take(&reborn);
    assert(reborn_id.index == dead_id.index && "a freed slot is reused");
    assert(!entity_id_equal(reborn_id, dead_id));
    assert(entity_get(dead_id) == NULL);
    assert(entity_get(reborn_id) == &reborn);
}

static void releasing_an_entity_without_a_slot_frees_nothing(void) {
    entity_t held = {0}, never_taken = {0};
    new_world();
    entity_id_t held_id = entity_id_take(&held);
    assert(entity_id_is_none(entity_id_of(&never_taken)));
    entity_id_release(&never_taken);
    assert(entity_get(held_id) == &held);
}

static void a_copy_of_a_live_entity_holds_no_id(void) {
    entity_t original = {0};
    new_world();
    entity_id_t id = entity_id_take(&original);
    entity_t copy = original;
    assert(entity_id_is_none(entity_id_of(&copy)));
    entity_id_release(&copy);
    assert(entity_get(id) == &original && "releasing a copy frees nothing");
}

static void generation_wraparound_skips_zero(void) {
    entity_t e = {0};
    int wrapped = 0;
    new_world();
    entity_id_t previous = entity_id_take(&e);
    for (long i = 0; i <= UINT16_MAX; i++) {
        entity_id_release(&e);
        entity_id_t id = entity_id_take(&e);
        assert(id.index == previous.index);
        assert(id.generation != 0 && "a live slot never carries generation 0");
        if (previous.generation == UINT16_MAX) {
            assert(id.generation == 1);
            wrapped = 1;
        }
        previous = id;
    }
    assert(wrapped);
}

static void ids_of_a_previous_world_never_resolve(void) {
    entity_t old = {0}, cycled = {0}, fresh = {0};
    new_world();
    entity_id_t old_id = entity_id_take(&old);
    entity_id_release(&old);
    entity_id_t cycled_id = entity_id_take(&cycled);
    new_world();
    assert(entity_get(old_id) == NULL);
    assert(entity_get(cycled_id) == NULL);
    entity_id_t fresh_id = entity_id_take(&fresh);
    assert(fresh_id.index == cycled_id.index && "the new world reuses the index");
    assert(entity_get(cycled_id) == NULL);
    assert(entity_get(fresh_id) == &fresh);
}

static void live_count_follows_slots_and_peak_survives_reset(void) {
    entity_t a = {0}, b = {0}, c = {0};
    new_world();
    assert(entity_id_live() == 0);
    int peak_before = entity_id_max();
    entity_id_take(&a);
    entity_id_take(&b);
    entity_id_take(&c);
    assert(entity_id_live() == 3);
    entity_id_release(&b);
    assert(entity_id_live() == 2);
    int peak = entity_id_max();
    assert(peak >= 3 && peak >= peak_before);
    new_world();
    assert(entity_id_live() == 0);
    assert(entity_id_max() == peak);
}

static void a_forgotten_table_resolves_nothing(void) {
    entity_t e = {0};
    new_world();
    entity_id_t id = entity_id_take(&e);
    entity_id_table_forget();
    assert(entity_get(id) == NULL);
    assert(entity_id_is_none(entity_id_of(&e)));
    assert(entity_id_live() == 0);
    entity_id_release(&e);
}

static void a_full_table_reuses_a_freed_slot(void) {
    new_world();
    for (int i = 0; i < ENTITY_TABLE_CAP; i++) entity_id_take(&g_crowd[i]);
    entity_id_release(&g_crowd[7]);
    entity_id_t late = entity_id_take(&g_crowd[ENTITY_TABLE_CAP]);
    assert(entity_get(late) == &g_crowd[ENTITY_TABLE_CAP]);
}

static const entity_class_t kept_cls = {0};

static void registry_world(void) {
    entity_registry_shutdown();
    game_zone_reset();
    entity_registry_init(NULL);
}

/* Static, never on the stack: a stack address can repeat one an earlier
 * case left in its table. */
static entity_t g_lifecycle[4];

static entity_t *lifecycle_entity(int i, const entity_class_t *cls) {
    entity_t none = {0};
    g_lifecycle[i] = none;
    g_lifecycle[i].class = cls;
    return &g_lifecycle[i];
}

static void constructed_entity_resolves_by_its_id(void) {
    registry_world();
    entity_t *e = lifecycle_entity(0, &kept_cls);
    entity_construct(e);
    entity_id_t id = entity_id_of(e);
    assert(!entity_id_is_none(id) && "entity_construct takes a slot");
    assert(entity_get(id) == e);
}

static entity_id_t g_releasing_id;
static int g_released;

static void release_after_slot(entity_t *self) {
    (void)self;
    assert(entity_get(g_releasing_id) == NULL && "the slot is freed before release returns the memory");
    g_released++;
}

static const entity_class_t released_cls = {.release = release_after_slot};

static void flush_frees_the_slot_then_releases(void) {
    registry_world();
    entity_t *kept = lifecycle_entity(0, &kept_cls);
    entity_t *released = lifecycle_entity(1, &released_cls);
    entity_construct(kept);
    entity_construct(released);
    g_releasing_id = entity_id_of(released);
    entity_unregister(kept);
    entity_unregister(released);
    g_released = 0;
    exit_queue_flush();
    assert(g_released == 1);
}

static void reset_world_drops_every_old_id(void) {
    registry_world();
    entity_t *old = lifecycle_entity(0, &kept_cls);
    entity_t *abandoned = lifecycle_entity(1, &released_cls);
    entity_t *fresh = lifecycle_entity(2, &kept_cls);
    entity_construct(old);
    entity_construct(abandoned);
    entity_id_t old_id = entity_id_of(old);
    g_releasing_id = entity_id_of(abandoned);
    reset_world(NULL);
    assert(entity_get(old_id) == NULL && entity_get(g_releasing_id) == NULL);
    entity_construct(fresh);
    entity_id_t fresh_id = entity_id_of(fresh);
    exit_queue_flush();
    assert(entity_get(fresh_id) == fresh && "an abandoned entity frees no slot of the new world");
    assert(entity_get(old_id) == NULL);
}

static void registered_without_construct_holds_no_id(void) {
    registry_world();
    entity_t *held = lifecycle_entity(0, &kept_cls);
    entity_t *bare = lifecycle_entity(1, &kept_cls);
    entity_construct(held);
    entity_id_t held_id = entity_id_of(held);
    entity_register(bare);
    assert(entity_id_is_none(entity_id_of(bare)));
    entity_unregister(bare);
    exit_queue_flush();
    assert(entity_get(held_id) == held);
    assert(entity_id_live() == 1);
}

static void dead_entity_resolves_until_the_flush(void) {
    registry_world();
    entity_t *e = lifecycle_entity(0, &kept_cls);
    entity_construct(e);
    entity_id_t id = entity_id_of(e);
    entity_unregister(e);
    assert(entity_get(id) == e && "the frame of the death still reads the entity");
    exit_queue_flush();
    assert(entity_get(id) == NULL && "a class without release frees its slot at the flush");
}

static void a_link_is_the_id_of_a_constructed_entity_or_none(void) {
    entity_t linked = {0};
    new_world();
    entity_id_t id = entity_id_take(&linked);
    assert(entity_id_equal(entity_id_link(&linked), id));
    assert(entity_id_is_none(entity_id_link(NULL)));
}

/* Runs in a child process: p8_fatal exits. */
static int take_one_slot_too_many(void) {
    new_world();
    for (int i = 0; i <= ENTITY_TABLE_CAP; i++) entity_id_take(&g_crowd[i]);
    return 0;
}

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

static void full_table_is_fatal(const char *self_path) {
    char cmd[512];
    snprintf(cmd, sizeof cmd, "%s full > /dev/null 2> " STDERR_LOG, self_path);
    int status = system(cmd);
    assert(status != -1 && WIFEXITED(status));
    char *err = read_whole_file(STDERR_LOG);
    if (WEXITSTATUS(status) != 3 || strstr(err, "entity table full, limit 512") == NULL) {
        fprintf(stderr, "full: expected exit 3 and the limit, exit %d\n%s", WEXITSTATUS(status), err);
        assert(0 && "taking a slot in a full table must call p8_fatal");
    }
    free(err);
    remove(STDERR_LOG);
}

int main(int argc, char **argv) {
    if (argc == 2 && strcmp(argv[1], "full") == 0) return take_one_slot_too_many();
    no_link_resolves_to_null();
    table_takes_its_budgeted_block_from_the_game_zone();
    taken_id_resolves_to_its_entity();
    released_slot_never_resolves_again();
    releasing_an_entity_without_a_slot_frees_nothing();
    a_copy_of_a_live_entity_holds_no_id();
    a_link_is_the_id_of_a_constructed_entity_or_none();
    generation_wraparound_skips_zero();
    ids_of_a_previous_world_never_resolve();
    live_count_follows_slots_and_peak_survives_reset();
    a_forgotten_table_resolves_nothing();
    a_full_table_reuses_a_freed_slot();
    constructed_entity_resolves_by_its_id();
    dead_entity_resolves_until_the_flush();
    flush_frees_the_slot_then_releases();
    reset_world_drops_every_old_id();
    registered_without_construct_holds_no_id();
    full_table_is_fatal(argv[0]);
    printf("test_entity_id: all tests passed\n");
    return 0;
}
