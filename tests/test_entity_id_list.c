#include "engine/entity_id_list.h"

#include <assert.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/wait.h>
#include <unistd.h>

#include "engine/entity_geom.h"
#include "engine/entity_id.h"
#include "heap_allocator.h"

static entity_t g_entities[6];

static void new_table(void) {
    entity_t none = {0};
    entity_id_table_init(&heap_allocator);
    for (int i = 0; i < 6; i++) {
        g_entities[i] = none;
        entity_id_take(&g_entities[i]);
    }
}

static entity_id_t id_of(int i) {
    return entity_id_of(&g_entities[i]);
}

static void iteration_resolves_every_id_in_order(void) {
    entity_id_list_t list;
    entity_id_list_iter_t it;
    entity_t *e;
    int seen = 0;
    new_table();
    entity_id_list_init(&list, &heap_allocator);
    for (int i = 0; i < 5; i++) entity_id_list_add(&list, id_of(i));
    assert(entity_id_list_count(&list) == 5);
    it = entity_id_list_all(&list);
    while (entity_id_list_iter_next(&it, &e)) {
        assert(e == &g_entities[seen]);
        seen++;
    }
    assert(seen == 5);
    entity_id_list_free(&list);
    assert(entity_id_list_count(&list) == 0);
}

static void delete_removes_the_first_equal_id_and_has_sees_it(void) {
    entity_id_list_t list;
    new_table();
    entity_id_list_init(&list, &heap_allocator);
    entity_id_list_add(&list, id_of(0));
    entity_id_list_add(&list, id_of(1));
    entity_id_list_add(&list, id_of(2));
    entity_id_list_add(&list, id_of(1));
    assert(entity_id_list_has(&list, id_of(1)));
    assert(!entity_id_list_has(&list, id_of(3)));
    assert(!entity_id_list_has(NULL, id_of(1)) && "a NULL list has nothing");
    assert(entity_id_list_del(&list, id_of(1)) == 1);
    assert(entity_id_list_count(&list) == 3);
    assert(entity_id_list_has(&list, id_of(1)) && "only the first equal id goes");
    assert(entity_id_list_del(&list, id_of(1)) == 1);
    assert(!entity_id_list_has(&list, id_of(1)));
    assert(entity_id_list_del(&list, id_of(3)) == 0);
    assert(entity_id_list_count(&list) == 2);
    entity_id_list_free(&list);
}

static void deleting_the_current_id_while_walking_visits_the_rest(void) {
    entity_id_list_t list;
    entity_id_list_iter_t it;
    entity_t *e;
    int seen = 0;
    new_table();
    entity_id_list_init(&list, &heap_allocator);
    for (int i = 0; i < 4; i++) entity_id_list_add(&list, id_of(i));
    it = entity_id_list_all(&list);
    while (entity_id_list_iter_next(&it, &e)) {
        assert(e == &g_entities[seen]);
        entity_id_list_del(&list, entity_id_of(e));
        seen++;
    }
    assert(seen == 4 && entity_id_list_count(&list) == 0);
    entity_id_list_free(&list);
}

static void list_of(entity_id_list_t *list, int first, int count) {
    entity_id_list_init(list, &heap_allocator);
    for (int i = first; i < first + count; i++) entity_id_list_add(list, id_of(i));
}

static void assert_walks(const entity_id_list_t *list, const int *expected, int count) {
    entity_id_list_iter_t it = entity_id_list_all(list);
    entity_t *e;
    int seen = 0;
    while (entity_id_list_iter_next(&it, &e)) {
        assert(seen < count && e == &g_entities[expected[seen]]);
        seen++;
    }
    assert(seen == count);
}

static void concat_copies_both_lists_in_order(void) {
    entity_id_list_t a, b, joined;
    const int expected[] = {0, 1, 3, 4, 5};
    new_table();
    list_of(&a, 0, 2);
    list_of(&b, 3, 3);
    joined = entity_id_list_concat(&heap_allocator, &a, &b);
    assert_walks(&joined, expected, 5);
    entity_id_list_free(&a);
    assert_walks(&joined, expected, 5);
    entity_id_list_free(&b);
    entity_id_list_free(&joined);
}

static int is_even_entity(entity_t *e, void *ctx) {
    (void)ctx;
    return (e - g_entities) % 2 == 0;
}

static void filter_keeps_the_resolved_entities_the_predicate_accepts(void) {
    entity_id_list_t all, even, none;
    const int expected[] = {0, 2, 4};
    new_table();
    list_of(&all, 0, 6);
    even = entity_id_list_filter(&heap_allocator, &all, is_even_entity, NULL);
    assert_walks(&even, expected, 3);
    none = entity_id_list_filter(&heap_allocator, NULL, is_even_entity, NULL);
    assert(entity_id_list_count(&none) == 0 && "filtering a NULL list gives an empty list");
    entity_id_list_free(&all);
    entity_id_list_free(&even);
    entity_id_list_free(&none);
}

static void clear_empties_and_a_reserved_list_grows_past_its_reserve(void) {
    entity_id_list_t list;
    const int expected[] = {0, 1, 2, 3, 4};
    new_table();
    entity_id_list_init_reserve(&list, &heap_allocator, 2);
    for (int i = 0; i < 5; i++) entity_id_list_add(&list, id_of(i));
    assert_walks(&list, expected, 5);
    entity_id_list_clear(&list);
    assert(entity_id_list_count(&list) == 0);
    entity_id_list_add(&list, id_of(0));
    assert_walks(&list, expected, 1);
    entity_id_list_free(&list);
}

static void place(int i, int x, int y) {
    entity_set_map_pos(&g_entities[i], vec_make(p8_fixed_from_int(x), p8_fixed_from_int(y)));
}

static void nearest_in_ids_skips_the_same_spot_and_ignores_unlisted_entities(void) {
    entity_id_list_t anchors;
    entity_t *nearest;
    vec_t origin = vec_make(p8_fixed_from_int(0), p8_fixed_from_int(0));
    new_table();
    place(0, 0, 0);
    place(1, 5, 0);
    place(2, 0, 3);
    place(3, 1, 1);
    entity_id_list_init(&anchors, &heap_allocator);
    entity_id_list_add(&anchors, id_of(0));
    entity_id_list_add(&anchors, id_of(1));
    entity_id_list_add(&anchors, id_of(2));
    p8_fixed_t distance = entity_geom_nearest_in_ids(origin, &anchors, &nearest);
    assert(nearest == &g_entities[2] && "the anchor on the spot itself is skipped, unlisted 3 too");
    assert(distance == p8_fixed_from_int(9));
    distance = entity_geom_nearest_in_ids(origin, NULL, &nearest);
    assert(nearest == NULL && distance == p8_fixed_from_int(32767));
    entity_id_list_free(&anchors);
}

static void at_resolves_the_id_at_an_index(void) {
    entity_id_list_t list;
    new_table();
    entity_id_list_init(&list, &heap_allocator);
    for (int i = 0; i < 3; i++) entity_id_list_add(&list, id_of(i));
    assert(entity_id_list_at(&list, 0) == &g_entities[0]);
    assert(entity_id_list_at(&list, 2) == &g_entities[2]);
    entity_id_list_free(&list);
}

#define STDERR_LOG "tmp/test_entity_id_list_stderr.txt"

static void walk_a_list_holding_a_dead_id(void) {
    entity_id_list_t list;
    entity_id_list_iter_t it;
    entity_t *e;
    new_table();
    entity_id_list_init(&list, &heap_allocator);
    entity_id_list_add(&list, id_of(0));
    entity_id_list_add(&list, id_of(1));
    entity_id_release(&g_entities[1]);
    it = entity_id_list_all(&list);
    while (entity_id_list_iter_next(&it, &e)) {
    }
}

static void read_a_dead_id_at_an_index(void) {
    entity_id_list_t list;
    new_table();
    entity_id_list_init(&list, &heap_allocator);
    entity_id_list_add(&list, id_of(0));
    entity_id_release(&g_entities[0]);
    (void)entity_id_list_at(&list, 0);
}

static void expect_dead_id_assert(void (*body)(void)) {
    fflush(stdout);
    pid_t child = fork();
    assert(child >= 0);
    if (child == 0) {
        if (freopen(STDERR_LOG, "w", stderr) == NULL) _exit(2);
        body();
        _exit(0);
    }
    int status;
    assert(waitpid(child, &status, 0) == child);
    assert(WIFSIGNALED(status) && WTERMSIG(status) == SIGABRT && "reading a dead id from a list must assert");
    FILE *log = fopen(STDERR_LOG, "r");
    char line[512] = {0};
    assert(log != NULL);
    assert(fread(line, 1, sizeof line - 1, log) > 0);
    fclose(log);
    assert(strstr(line, "remove it where the entity dies") != NULL);
    remove(STDERR_LOG);
}

int main(void) {
    iteration_resolves_every_id_in_order();
    delete_removes_the_first_equal_id_and_has_sees_it();
    deleting_the_current_id_while_walking_visits_the_rest();
    concat_copies_both_lists_in_order();
    filter_keeps_the_resolved_entities_the_predicate_accepts();
    clear_empties_and_a_reserved_list_grows_past_its_reserve();
    nearest_in_ids_skips_the_same_spot_and_ignores_unlisted_entities();
    at_resolves_the_id_at_an_index();
    expect_dead_id_assert(walk_a_list_holding_a_dead_id);
    expect_dead_id_assert(read_a_dead_id_at_an_index);
    printf("test_entity_id_list: all tests passed\n");
    return 0;
}
