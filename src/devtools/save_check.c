#define _POSIX_C_SOURCE 200809L

#include "devtools/save_check.h"

#include "devtools/state.h"
#include "engine/allocator.h"
#include "engine/dispatch_context.h"
#include "engine/entity.h"
#include "engine/entity_id.h"
#include "engine/entity_registry.h"
#include "engine/slot_group.h"
#include "game/cart.h"
#include "game/menu.h"
#include "game/message_box.h"
#include "game/pad_panel.h"
#include "game/particle.h"
#include "game/particle_zone.h"
#include "game/planet_family.h"
#include "game/scroll.h"
#include "game/slipway.h"
#include "game/ui.h"
#include "game/ui_zone.h"
#include "game/visual_random.h"

#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

static save_state_t g_state;
static char g_raw_dump[2 * SAVE_CHECK_DUMP_CAP];

static void assert_save_frame_conditions(void) {
    const entity_list_t *live = live_entities();
    assert(entity_registry_count_tagged(TAG_WORMHOLE) == 0 && "save frame: a wormhole is live");
    for (int i = 0; i < live->count; i++) {
        assert(((entity_t *)live->items[i])->state != ENTITY_STATE_PLACING && "save frame: an entity is being placed");
    }
}

int save_check_screen_comparable(void) {
    return entity_registry_count_tagged(TAG_MESSAGE_BOX) == 0 && shown_message_box() == NULL && !pad_panel_box_open() &&
           !pad_panel_open() && !pause_menu_shown();
}

static int is_session_line(const char *line) {
    return strncmp(line, "zone ", 5) == 0 || strncmp(line, "cap ", 4) == 0;
}

static void write_dump(char *out) {
    FILE *stream = fmemopen(g_raw_dump, sizeof g_raw_dump, "w");
    size_t written;
    size_t kept = 0;
    assert(stream != NULL);
    state_write(stream);
    written = (size_t)ftell(stream);
    fclose(stream);
    assert(written < sizeof g_raw_dump - 1 && "save_check: the state dump outgrew its buffer, raise SAVE_CHECK_DUMP_CAP");
    g_raw_dump[written] = '\0';
    for (const char *line = g_raw_dump; *line != '\0';) {
        const char *end = strchr(line, '\n');
        size_t length = end != NULL ? (size_t)(end - line) + 1 : strlen(line);
        if (!is_session_line(line)) {
            assert(kept + length < SAVE_CHECK_DUMP_CAP && "save_check: the filtered dump outgrew SAVE_CHECK_DUMP_CAP");
            memcpy(out + kept, line, length);
            kept += length;
        }
        line += length;
    }
    out[kept] = '\0';
}

void save_check_capture(save_check_snapshot_t *out) {
    assert(out != NULL);
    assert_save_frame_conditions();
    save_state_capture(&g_state);
    out->byte_count = save_state_encode(&g_state, out->bytes);
    out->visual_random_state = visual_random_state();
    write_dump(out->dump);
    out->has_screen = 0;
}

static void release_particles_in_place(emitter_t *emitter) {
    if (!(emitter->has & EMITTER_HAS_PARTICLES)) {
        return;
    }
    for (int i = 0; i < emitter->particles.count; i++) {
        allocator_release(particle_zone(PARTICLE_SLOT_PARTICLE), emitter->particles.items[i]);
    }
    emitter->particles.count = 0;
}

enum { DROPPED_CAP = 64 };

/* No accessor names the toast class: a toast is told by the slot zone it lives in. */
static int is_toast(const entity_t *e) {
    const slot_group_handle_t *handle = (const slot_group_handle_t *)ui_zone(SLOT_KIND_TOAST);
    const slot_zone_t *zone = &handle->group->zones[handle->kind];
    uintptr_t start = (uintptr_t)zone->storage;
    uintptr_t at = (uintptr_t)e;
    return at >= start && at < start + zone->slot_size * zone->count;
}

static int is_unsaved_entity(const entity_t *e) {
    return e->class == burst_class() || e->state == ENTITY_STATE_PLACING || is_toast(e) ||
           (entity_has(e, ENTITY_HAS_IS_DEAD) && e->is_dead);
}

static void unregister_unsaved_entities(void) {
    entity_t *dropped[DROPPED_CAP];
    int dropped_count = 0;
    const entity_list_t *live = live_entities();
    for (int i = 0; i < live->count; i++) {
        entity_t *e = live->items[i];
        if (is_unsaved_entity(e)) {
            assert(dropped_count < DROPPED_CAP && "save_check: more unsaved entities than DROPPED_CAP");
            dropped[dropped_count++] = e;
        }
    }
    for (int i = 0; i < dropped_count; i++) {
        entity_unregister(dropped[i]);
    }
    cart_flush_exit_queue();
}

static void reset_slipways(void) {
    const entity_list_t *slipways = entities_by_tag(TAG_SLIPWAY);
    for (int i = 0; i < entity_registry_count_tagged(TAG_SLIPWAY); i++) {
        slipway_t *slipway = (slipway_t *)slipways->items[i];
        for (int j = 0; j < slipway->transit_dots.count; j++) {
            allocator_release(particle_zone(PARTICLE_SLOT_TRANSIT_DOT), slipway->transit_dots.items[j]);
        }
        slipway->transit_dots.count = 0;
        slipway->served_planets.count = 0;
        slipway->fade_in_step = slipway->archetype->fade_in_step;
        slipway->targeted_by = ENTITY_ID_NONE;
    }
}

void save_check_drop_unsaved_state(void) {
    const entity_list_t *live;
    entity_t *camera = main_camera();
    unregister_unsaved_entities();
    live = live_entities();
    for (int i = 0; i < live->count; i++) {
        entity_t *e = live->items[i];
        emitter_t *emitter = emitter_of(e);
        if (emitter != NULL) {
            release_particles_in_place(emitter);
        }
        if (entity_has(e, ENTITY_HAS_SHAKE)) {
            e->shake = p8_fixed_from_int(0);
        }
    }
    if (camera != NULL) {
        camera->shake = p8_fixed_from_int(0);
        entity_set_pos(camera, entity_map_pos(camera));
    }
    reset_slipways();
    if (main_ui() != NULL) {
        ui_set_pad_crosshair(main_ui(), 0);
        ui_set_pad_selection(main_ui(), NULL, 0);
        ui_set_pad_tab(main_ui(), NULL);
    }
}

void save_check_hide_particles(int hidden) {
    particle_hide_pixels(hidden);
}

void save_check_capture_screen(save_check_snapshot_t *out) {
    assert(out != NULL);
    assert(save_check_screen_comparable() && "compared screen: a message box, panel, menu or the pause menu is open");
    assert(particle_pixels_hidden() && "save_check: the frame was drawn with particles shown; call save_check_hide_particles(1) before it");
    assert(particle_zone_live(PARTICLE_SLOT_BURST) == 0 &&
           "save_check: a burst is live and the save never holds one, so the compared frames differ");
    memcpy(out->screen, dispatch_context_mem()->screen, sizeof out->screen);
    out->has_screen = 1;
}

void save_check_resume(const save_check_snapshot_t *saved) {
    int valid;
    assert(saved != NULL);
    valid = save_state_decode(saved->bytes, saved->byte_count, &g_state);
    assert(valid && "save_check_resume: the saved bytes do not decode");
    (void)valid;
    cart_resume_game(&g_state);
    visual_random_set_state(saved->visual_random_state);
}

static int line_length(const char *line) {
    const char *end = strchr(line, '\n');
    return end != NULL ? (int)(end - line) : (int)strlen(line);
}

static int compare_bytes(const save_check_snapshot_t *a, const save_check_snapshot_t *b, char *report, size_t size) {
    int shorter = a->byte_count < b->byte_count ? a->byte_count : b->byte_count;
    for (int i = 0; i < shorter; i++) {
        if (a->bytes[i] != b->bytes[i]) {
            snprintf(report, size, "byte %d: 0x%02x != 0x%02x", i, a->bytes[i], b->bytes[i]);
            return 1;
        }
    }
    if (a->byte_count != b->byte_count) {
        snprintf(report, size, "byte count: %d != %d", a->byte_count, b->byte_count);
        return 1;
    }
    return 0;
}

static int compare_dumps(const save_check_snapshot_t *a, const save_check_snapshot_t *b, char *report, size_t size) {
    const char *left = a->dump;
    const char *right = b->dump;
    int line = 1;
    while (*left != '\0' || *right != '\0') {
        int left_length = line_length(left);
        int right_length = line_length(right);
        if (left_length != right_length || memcmp(left, right, (size_t)left_length) != 0) {
            snprintf(report, size, "dump line %d: \"%.*s\" != \"%.*s\"", line, left_length, left, right_length, right);
            return 1;
        }
        left += left_length + (left[left_length] == '\n');
        right += right_length + (right[right_length] == '\n');
        line++;
    }
    return 0;
}

static int compare_screens(const save_check_snapshot_t *a, const save_check_snapshot_t *b, char *report, size_t size) {
    if (!a->has_screen || !b->has_screen) {
        return 0;
    }
    for (int y = 0; y < P8_SCREEN_HEIGHT; y++) {
        for (int x = 0; x < P8_SCREEN_WIDTH; x++) {
            if (a->screen[y][x] != b->screen[y][x]) {
                snprintf(report, size, "pixel %d %d: color %d != %d", x, y, a->screen[y][x], b->screen[y][x]);
                return 1;
            }
        }
    }
    return 0;
}

typedef int (*compare_part_fn_t)(const save_check_snapshot_t *a, const save_check_snapshot_t *b, char *report, size_t size);

int save_check_compare(const save_check_snapshot_t *a, const save_check_snapshot_t *b, char *report, size_t size) {
    static const compare_part_fn_t parts[] = {compare_bytes, compare_dumps, compare_screens};
    int differs = 0;
    assert(a != NULL && b != NULL && report != NULL && size > 0);
    report[0] = '\0';
    for (size_t i = 0; i < sizeof parts / sizeof parts[0]; i++) {
        size_t used = strlen(report);
        size_t separator = differs ? 2 : 0;
        if (used + separator >= size) {
            break;
        }
        if (parts[i](a, b, report + used + separator, size - used - separator)) {
            if (differs) {
                memcpy(report + used, "; ", 2);
            }
            differs = 1;
        }
    }
    return differs;
}
