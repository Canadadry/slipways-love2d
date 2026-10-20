#include "game/pad_back.h"

#include "engine/entity_id.h"
#include "game/planet.h"
#include "heap_allocator.h"

#include <assert.h>
#include <stdio.h>

static entity_t *node(entity_t *storage, state_t state) {
    entity_init_header(storage, planet_class());
    storage->state = state;
    entity_id_take(storage);
    return storage;
}

static void test_init_records_nothing(void) {
    entity_t hovered;
    node(&hovered, ENTITY_STATE_SETTLED);
    pad_back_t back;
    pad_back_init(&back);
    assert(pad_back_target(&back, &hovered) == NULL);
    assert(pad_back_target(&back, NULL) == NULL);
}

static void test_the_record_is_the_target_unless_it_is_hovered(void) {
    entity_t colony, other;
    node(&colony, ENTITY_STATE_SETTLED);
    node(&other, ENTITY_STATE_KNOWN);
    pad_back_t back;
    pad_back_init(&back);
    pad_back_record(&back, &colony);
    assert(pad_back_target(&back, &other) == &colony);
    assert(pad_back_target(&back, NULL) == &colony);
    assert(pad_back_target(&back, &colony) == NULL);
}

static void test_recording_null_keeps_the_previous_record(void) {
    entity_t colony, other;
    node(&colony, ENTITY_STATE_SETTLED);
    node(&other, ENTITY_STATE_SETTLED);
    pad_back_t back;
    pad_back_init(&back);
    pad_back_record(&back, &colony);
    pad_back_record(&back, NULL);
    assert(pad_back_target(&back, &other) == &colony);
}

static void test_a_record_no_longer_a_candidate_gives_null(void) {
    entity_t dead, unknown, vanishing, released, other;
    node(&dead, ENTITY_STATE_SETTLED);
    node(&unknown, ENTITY_STATE_KNOWN);
    node(&vanishing, ENTITY_STATE_KNOWN);
    node(&released, ENTITY_STATE_KNOWN);
    node(&other, ENTITY_STATE_SETTLED);
    pad_back_t back;
    pad_back_init(&back);
    pad_back_record(&back, &dead);
    dead.is_dead = 1;
    assert(pad_back_target(&back, &other) == NULL);
    pad_back_record(&back, &unknown);
    unknown.state = ENTITY_STATE_UNKNOWN;
    assert(pad_back_target(&back, &other) == NULL);
    pad_back_record(&back, &vanishing);
    vanishing.state = ENTITY_STATE_VANISHING;
    assert(pad_back_target(&back, &other) == NULL);
    pad_back_record(&back, &released);
    entity_id_release(&released);
    assert(pad_back_target(&back, &other) == NULL);
}

static void test_a_later_record_replaces_an_earlier_one(void) {
    entity_t first, second, other;
    node(&first, ENTITY_STATE_SETTLED);
    node(&second, ENTITY_STATE_KNOWN);
    node(&other, ENTITY_STATE_SETTLED);
    pad_back_t back;
    pad_back_init(&back);
    pad_back_record(&back, &first);
    pad_back_record(&back, &second);
    assert(pad_back_target(&back, &other) == &second);
    assert(pad_back_target(&back, &first) == &second);
}

int main(void) {
    entity_id_table_init(&heap_allocator);
    test_init_records_nothing();
    test_the_record_is_the_target_unless_it_is_hovered();
    test_recording_null_keeps_the_previous_record();
    test_a_record_no_longer_a_candidate_gives_null();
    test_a_later_record_replaces_an_earlier_one();
    printf("test_pad_back: all tests passed\n");
    return 0;
}
