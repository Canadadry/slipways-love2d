#include "game/save_state.h"
#include "game/save_storage.h"
#include "devtools/save_storage_memory.h"

#include "save_storage_contract.h"

#include <assert.h>
#include <stdint.h>
#include <stdio.h>

static uint8_t g_bytes[SAVE_STATE_MAX_BYTES];

static void test_storage_is_empty_at_process_start(void) {
    assert(save_storage_read(g_bytes, SAVE_STATE_MAX_BYTES) == 0);
}

static void test_write_count_rises_on_each_write_only(void) {
    int before = save_storage_memory_write_count();
    g_bytes[0] = 7;
    assert(save_storage_write(g_bytes, 1));
    assert(save_storage_memory_write_count() == before + 1);
    assert(save_storage_write(g_bytes, 1));
    assert(save_storage_memory_write_count() == before + 2);
    assert(save_storage_read(g_bytes, SAVE_STATE_MAX_BYTES) == 1);
    assert(save_storage_erase());
    assert(save_storage_memory_write_count() == before + 2);
}

int main(void) {
    test_storage_is_empty_at_process_start();
    save_storage_contract_run();
    test_write_count_rises_on_each_write_only();
    printf("test_save_storage_memory: ok\n");
    return 0;
}
