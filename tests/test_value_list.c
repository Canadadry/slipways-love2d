#include "engine/value_list.h"

#include <assert.h>
#include <stdio.h>

#include "heap_allocator.h"

typedef struct {
    int id;
    char name[13];
} record_t;

static record_t record(int id) {
    record_t r = {.id = id, .name = "record"};
    return r;
}

static const record_t *record_at(const value_list_t *list, int index) {
    return (const record_t *)value_list_at(list, index);
}

static void add_copies_each_value_in_order(void) {
    value_list_t list;
    value_list_init(&list, &heap_allocator, sizeof(record_t));
    for (int i = 0; i < 9; i++) {
        record_t r = record(i);
        value_list_add(&list, &r);
        r.id = -1;
    }
    assert(value_list_count(&list) == 9);
    for (int i = 0; i < 9; i++) assert(record_at(&list, i)->id == i && "the list holds a copy");
    value_list_free(&list);
    assert(value_list_count(&list) == 0);
}

static value_list_t list_of(int count) {
    value_list_t list;
    value_list_init(&list, &heap_allocator, sizeof(record_t));
    for (int i = 0; i < count; i++) {
        record_t r = record(i);
        value_list_add(&list, &r);
    }
    return list;
}

static void copy_owns_its_values(void) {
    value_list_t original = list_of(5);
    value_list_t copy = value_list_copy(&heap_allocator, &original);
    ((record_t *)value_list_at(&original, 2))->id = 99;
    value_list_free(&original);
    assert(value_list_count(&copy) == 5);
    for (int i = 0; i < 5; i++) assert(record_at(&copy, i)->id == i);
    record_t extra = record(5);
    value_list_add(&copy, &extra);
    assert(record_at(&copy, 5)->id == 5);
    value_list_free(&copy);
}

static void clear_empties_and_the_list_takes_values_again(void) {
    value_list_t list = list_of(3);
    value_list_clear(&list);
    assert(value_list_count(&list) == 0);
    record_t again = record(7);
    value_list_add(&list, &again);
    assert(value_list_count(&list) == 1 && record_at(&list, 0)->id == 7);
    value_list_free(&list);
}

static void copy_of_an_empty_list_is_empty(void) {
    value_list_t empty = list_of(0);
    value_list_t copy = value_list_copy(&heap_allocator, &empty);
    assert(value_list_count(&copy) == 0);
    record_t r = record(1);
    value_list_add(&copy, &r);
    assert(record_at(&copy, 0)->id == 1 && "the copy keeps the element size");
    value_list_free(&copy);
}

int main(void) {
    add_copies_each_value_in_order();
    copy_owns_its_values();
    clear_empties_and_the_list_takes_values_again();
    copy_of_an_empty_list_is_empty();
    printf("test_value_list: all tests passed\n");
    return 0;
}
