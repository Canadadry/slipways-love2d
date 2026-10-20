#include "save_storage_contract.h"

#include "game/save_state.h"
#include "game/save_storage.h"

#include <assert.h>
#include <stdint.h>
#include <string.h>

static uint8_t g_written[SAVE_STATE_MAX_BYTES];
static uint8_t g_read[SAVE_STATE_MAX_BYTES];

static void assert_reads_back(int count) {
    memset(g_read, 0xA5, sizeof g_read);
    assert(save_storage_read(g_read, SAVE_STATE_MAX_BYTES) == count);
    assert(memcmp(g_read, g_written, (size_t)count) == 0);
}

static void test_empty_storage_reads_nothing(void) {
    assert(save_storage_erase() == 1);
    assert(save_storage_read(g_read, SAVE_STATE_MAX_BYTES) == 0);
}

static void test_every_byte_value_round_trips(void) {
    for (int i = 0; i < 256; i++) g_written[i] = (uint8_t)i;
    assert(save_storage_write(g_written, 256) == 1);
    assert_reads_back(256);
}

static void test_a_full_record_round_trips(void) {
    for (int i = 0; i < SAVE_STATE_MAX_BYTES; i++) g_written[i] = (uint8_t)(i * 7 + 3);
    assert(save_storage_write(g_written, SAVE_STATE_MAX_BYTES) == 1);
    assert_reads_back(SAVE_STATE_MAX_BYTES);
}

static void test_a_shorter_overwrite_leaves_no_stale_tail(void) {
    memset(g_written, 0x11, 600);
    assert(save_storage_write(g_written, 600) == 1);
    memset(g_written, 0x22, 40);
    assert(save_storage_write(g_written, 40) == 1);
    assert_reads_back(40);
}

static void test_erase_empties_the_storage(void) {
    g_written[0] = 0x42;
    assert(save_storage_write(g_written, 1) == 1);
    assert(save_storage_erase() == 1);
    assert(save_storage_read(g_read, SAVE_STATE_MAX_BYTES) == 0);
    assert(save_storage_erase() == 1);
}

void save_storage_contract_run(void) {
    test_empty_storage_reads_nothing();
    test_every_byte_value_round_trips();
    test_a_full_record_round_trips();
    test_a_shorter_overwrite_leaves_no_stale_tail();
    test_erase_empties_the_storage();
}
