#include "game/save_storage.h"
#include "devtools/save_storage_memory.h"

#include "game/save_state.h"

#include <assert.h>
#include <string.h>

static uint8_t g_record[SAVE_STATE_MAX_BYTES];
static int g_count;
static int g_write_count;

int save_storage_write(const uint8_t *bytes, int count) {
    assert(count >= 1 && count <= SAVE_STATE_MAX_BYTES && "to clear the save, call save_storage_erase");
    memcpy(g_record, bytes, (size_t)count);
    g_count = count;
    g_write_count++;
    return 1;
}

int save_storage_read(uint8_t *bytes, int cap) {
    assert(cap >= SAVE_STATE_MAX_BYTES && "a smaller buffer would lose the end of a record");
    memcpy(bytes, g_record, (size_t)g_count);
    return g_count;
}

int save_storage_erase(void) {
    g_count = 0;
    return 1;
}

int save_storage_memory_write_count(void) {
    return g_write_count;
}
