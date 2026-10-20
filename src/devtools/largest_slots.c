#include "devtools/largest_slots.h"

#include <assert.h>
#include <stdio.h>

#include "engine/entity.h"
#include "engine/entity_class.h"
#include "engine/entity_id_list.h"
#include "engine/entity_registry.h"
#include "engine/frame_zone.h"
#include "engine/pointer_list.h"
#include "game/cart.h"
#include "game/help.h"
#include "game/layout.h"
#include "game/main_economy.h"
#include "game/map_tables.h"
#include "game/menu.h"
#include "game/research.h"
#include "game/research_display.h"
#include "game/ui.h"
#include "game/ui_button.h"
#include "game/ui_zone.h"
#include "p8/fixed.h"
#include "p8/str.h"

static void one_frame(void) {
    cart_update60();
    cart_draw();
}

/* A cart_init with no frame since the last: the previous help gives back
 * its help screen list, the only one, before the new help takes it. */
static void leave_help_screen(void) {
    const entity_list_t *live = live_entities();
    for (int i = 0; i < live->count; i++) {
        if (live->items[i]->class == help_screen_class()) {
            entity_unregister(live->items[i]);
            cart_flush_exit_queue();
            return;
        }
    }
}

static void start_game(void) {
    leave_help_screen();
    cart_init();
    cart_flush_exit_queue();
    cart_start_game(NULL);
    cart_flush_exit_queue();
}

static const button_t *last_ephemeral_button(void) {
    const entity_id_list_t *ephemerals = &main_ui()->ephemerals;
    const entity_t *last = entity_id_list_at(ephemerals, ephemerals->count - 1);

    assert(last->class == button_class());
    return (const button_t *)last;
}

/* Every tech's row of the tech tree screen (its tooltip bubble), the
 * "invented" row once every tech is, and every project's row as the
 * projects panel builds it (its layout row, its cost, its tooltip bubble). */
static void run_largest_buttons(void) {
    economy_t *economy;
    int i;

    start_game();
    economy = main_economy_typed();
    economy->research_points = p8_fixed_from_int(9999);
    /* Each screen closes, its buttons gone at the next frame, before the
     * next opens: the game never opens one over the other. */
    for (i = 0; i < TECHNOLOGIES_COUNT; i++) {
        economy->era = p8_fixed_from_int(technologies_table[i].era);
        menu_show_tech_tree_screen();
        ui_clear_ephemerals(main_ui());
        one_frame();
    }
    for (i = 0; i < TECHNOLOGIES_COUNT; i++) {
        pointer_list_add(&economy->invented, (char *)technologies_table[i].name);
    }
    menu_show_tech_tree_screen();
    assert(last_ephemeral_button()->tooltip->rows_count == TECHNOLOGIES_COUNT);
    one_frame();

    for (i = 1; i <= research_projects_count(); i++) {
        research_project_t *project = research_projects_at(i);
        button_keys_t keys = {.has_side_tooltip = 1, .has_side_tooltip_row = 1, .has_tooltip = 1};

        layout_row_keys_append(&keys.side_tooltip_row, layout_row_cell_string(p8_concat2(frame_zone(), "$", p8_tostr_fixed(frame_zone(), research_project_cost(project)))));
        if (project->has_icon_sprite) {
            keys.layout_row_keys = research_project_icon_row_keys(project);
        } else {
            (void)research_project_layout_row_keys(project, &keys.layout_row_keys);
        }
        button_keys_set_tooltip_row(&keys, research_project_tooltip(project));
        entity_unregister(&button_new(&keys)->header);
        one_frame();
    }
}

/* The largest row planet_refresh() can build: a planet producing luxury,
 * so two outputs, with the most wants a project has and an amount of 5, the
 * most drawn one by one. */
static void run_largest_row(void) {
    const pointer_list_t *inputs = NULL;
    const allocator_t *slot;
    layout_row_keys_t keys;
    int luxury;
    int k;

    for (k = 1; research_projects_at(k) != NULL; k++) {
        const research_project_t *project = research_projects_at(k);
        if (inputs == NULL || project->inputs.count > inputs->count) {
            inputs = &project->inputs;
        }
    }
    assert(inputs != NULL && inputs->count == 2 && "the most wants a project has; a longer input list needs a bigger row slot");
    assert(resource_code_lookup("w", &luxury));
    slot = ui_zone_slot(SLOT_KIND_ROW);
    assert(research_display_planet_resource_row_keys(inputs, 1, p8_fixed_from_int(luxury), p8_fixed_from_int(5), 0, 0, &keys));
    assert(layout_row_new(slot, &keys) != NULL);
    arena_release(slot);
}

void largest_slots_run(void) {
    run_largest_buttons();
    run_largest_row();
}

static void format_slot(const state_sink_t *sink, const char *name, slot_kind_t slot_kind) {
    char line[64];
    int written = snprintf(line, sizeof line, "bytes_%s %lu %lu\n", name, (unsigned long)ui_zone_max_bytes(slot_kind), (unsigned long)ui_zone_slot_size(slot_kind));

    assert(written > 0 && (size_t)written < sizeof line);
    sink->write_line(sink->context, line);
}

void largest_slots_format(const state_sink_t *sink) {
    assert(sink != NULL && sink->write_line != NULL);
    format_slot(sink, "button", SLOT_KIND_BUTTON);
    format_slot(sink, "row", SLOT_KIND_ROW);
}
