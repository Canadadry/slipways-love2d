#include <assert.h>
#include <stdio.h>

#include "engine/allocator.h"
#include "engine/slot_tally.h"
#include "engine/zone.h"

enum { KIND_BARE, KIND_ARENA, KIND_COUNT };

#define BARE_SIZE 32u
#define BARE_COUNT 4u
#define ARENA_SIZE 512u
#define ARENA_COUNT 3u

SLOT_ZONE_STORAGE(bare_cells, bare_taken, BARE_SIZE, BARE_COUNT);
SLOT_ZONE_STORAGE(arena_cells, arena_taken, ARENA_SIZE, ARENA_COUNT);

static const slot_kind_zone_t KINDS[KIND_COUNT] = {
    [KIND_BARE] = {"test bare", NULL, bare_cells, bare_taken, BARE_SIZE, BARE_COUNT},
    [KIND_ARENA] = {"test arena", "slot arena", arena_cells, arena_taken, ARENA_SIZE, ARENA_COUNT},
};

static slot_zone_t zones[KIND_COUNT];
static allocator_t handles[KIND_COUNT];
static slot_tally_t tally;

static void setup(void) {
    allocator_init_slot_kinds(KINDS, KIND_COUNT, zones, handles);
    slot_tally_init(&tally, KINDS, zones, KIND_COUNT);
}

static unsigned char *take_bare(void) {
    unsigned char *p = allocator_alloc(&handles[KIND_BARE], BARE_SIZE);
    slot_tally_take(&tally, KIND_BARE);
    return p;
}

static void give_back_bare(unsigned char *p) {
    slot_tally_give_back(&tally, KIND_BARE, p);
    allocator_release(&handles[KIND_BARE], p);
}

static const allocator_t *take_arena(void) {
    const allocator_t *arena = arena_take(&handles[KIND_ARENA], KINDS[KIND_ARENA].slot_name, ARENA_SIZE);
    slot_tally_take(&tally, KIND_ARENA);
    return arena;
}

static void give_back_arena(const allocator_t *arena) {
    slot_tally_give_back(&tally, KIND_ARENA, (const unsigned char *)arena);
    arena_release(arena);
}

static void test_max_live_survives_give_backs(void) {
    setup();
    unsigned char *a = take_bare();
    unsigned char *b = take_bare();
    unsigned char *c = take_bare();
    assert(slot_tally_live(&tally, KIND_BARE) == 3);
    give_back_bare(b);
    give_back_bare(a);
    assert(slot_tally_live(&tally, KIND_BARE) == 1);
    assert(slot_tally_max_live(&tally, KIND_BARE) == 3 && "the peak of live slots outlives their give back");
    unsigned char *d = take_bare();
    assert(slot_tally_max_live(&tally, KIND_BARE) == 3);
    assert(slot_tally_live(&tally, KIND_ARENA) == 0 && slot_tally_max_live(&tally, KIND_ARENA) == 0);
    give_back_bare(c);
    give_back_bare(d);
    assert(slot_tally_max_bytes(&tally, KIND_BARE) == 0 && "a bare slot is no arena: no bytes measured");
}

static void test_max_bytes_read_at_give_back(void) {
    const allocator_t *big = take_arena();
    allocator_alloc(big, 200);
    size_t big_used = arena_used(big);
    give_back_arena(big);
    const allocator_t *small = take_arena();
    allocator_alloc(small, 8);
    assert(slot_tally_max_bytes(&tally, KIND_ARENA) == big_used && "the largest arena, header included, read when given back");
    give_back_arena(small);
}

static void test_max_bytes_read_on_live_arena(void) {
    const allocator_t *a = take_arena();
    allocator_alloc(a, 400);
    size_t used = arena_used(a);
    assert(slot_tally_max_bytes(&tally, KIND_ARENA) == used && "an arena still live is measured when read");
    give_back_arena(a);
}

static void test_zone_bytes_and_peak(void) {
    setup();
    assert(slot_tally_zone_bytes(&tally) == 0);
    unsigned char *a = take_bare();
    unsigned char *b = take_bare();
    const allocator_t *c = take_arena();
    size_t peak = SLOT_ZONE_BYTES(BARE_SIZE, 2) + SLOT_ZONE_BYTES(ARENA_SIZE, 1);
    assert(slot_tally_zone_bytes(&tally) == peak && "each live slot counts its reserved size");
    give_back_bare(a);
    give_back_arena(c);
    assert(slot_tally_zone_bytes(&tally) == SLOT_ZONE_BYTES(BARE_SIZE, 1));
    assert(slot_tally_zone_max_bytes(&tally) == peak && "the zone peak outlives the give backs");
    give_back_bare(b);
    assert(slot_tally_zone_bytes(&tally) == 0);
}

static void test_zone_reserved_is_every_slot(void) {
    setup();
    assert(slot_tally_zone_reserved(&tally) == SLOT_ZONE_BYTES(BARE_SIZE, BARE_COUNT) + SLOT_ZONE_BYTES(ARENA_SIZE, ARENA_COUNT));
}

int main(void) {
    test_max_live_survives_give_backs();
    test_max_bytes_read_at_give_back();
    test_max_bytes_read_on_live_arena();
    test_zone_bytes_and_peak();
    test_zone_reserved_is_every_slot();
    printf("test_slot_tally: all tests passed\n");
    return 0;
}
