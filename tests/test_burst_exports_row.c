/* The "exports:" row of planet_inspection_panel counts the routes held in
 * the outgoing routes list a burst receives at construction, read on the
 * member itself. */

#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "engine/bootstrap.h"
#include "engine/entity_id.h"
#include "game/data_init.h"
#include "engine/entity_class.h"
#include "engine/dispatch_context.h"
#include "p8/fixed.h"
#include "p8/gfx.h"
#include "game/layout.h"
#include "p8/vram.h"
#include "game/planet.h"
#include "game/planet_display.h"
#include "game/planet_family.h"
#include "game/trade.h"
#include "engine/game_zone.h"
#include "game/ui.h"
#include "game/ui_button.h"
#include "engine/vec.h"

/* The buttons planet_inspection_panel just built. */
static const entity_id_list_t *main_ui_ephemerals(void) {
    const ui_t *ok = main_ui();
    assert(ok != NULL);
    return &ok->ephemerals;
}

static const layout_row_t *button_layout_row(const void *button) {
    const layout_row_t *lq;

    assert(button != NULL);
    assert(((const entity_t *)button)->class == button_class());
    lq = ((const button_t *)button)->layout_row;
    assert(lq != NULL);
    return lq;
}

static const char *layout_row_index_string(const layout_row_t *lq, int index) {
    const layout_row_item_t *v;
    if (index < 1 || index > lq->placed_items_count) {
        return NULL;
    }
    v = &lq->placed_items[index - 1];
    return (v->kind == LAYOUT_ROW_ITEM_TEXT) ? v->text : NULL;
}

static int layout_row_index_is_string(const layout_row_t *lq, int index, const char *expected) {
    const char *s = layout_row_index_string(lq, index);
    return s != NULL && strcmp(s, expected) == 0;
}

/* What planet_inspection_panel reaches when called on a burst, built
 * without a boot: the data tables (data_init), the planet, burst, layout
 * row and button classes, and the main_ui singleton the panel fills. */
static void setup_panel_state(p8_vram_t *mem, p8_graphics_t *gfx) {
    p8_vram_init(mem);
    p8_gfx_init(gfx);
    dispatch_context_set_gfx(gfx);
    dispatch_context_set_mem(mem);

    data_init();
    reset_world(NULL);
    layout_init();
    button_init();
    planet_init();
    planet_family_init();
}

/* Read on the instance's member, not the class defaults, so the test
 * says nothing about where the value comes from. */
static int outgoing_routes_present(const planet_t *planet) {
    return (planet->has & PLANET_HAS_OUTGOING_ROUTES) != 0;
}

static void test_burst_export_line_counts_the_outgoing_routes(void) {
    p8_vram_t memory;
    p8_graphics_t graphics;
    planet_t *planet;
    static production_t production;
    trade_route_list_t *outgoing;
    const trade_route_t route_a = trade_new(NULL);
    const trade_route_t route_b = trade_new(NULL);
    const entity_id_list_t *ephemerals;
    const layout_row_t *exports_layout_row = NULL;
    int index;

    setup_panel_state(&memory, &graphics);

    planet = burst_new(NULL);
    assert(planet != NULL);

    /* 99 is deliberately not one of planet_made_resources(), so the panel
     * takes the else branch and builds the "exports:" row, not the "makes:" one. */
    production.has_resource = 1;
    production.resource = p8_fixed_from_int(99);
    production.amount = p8_fixed_from_int(2);
    planet->production = production;
    planet->has |= PLANET_HAS_PRODUCTION;

    entity_set_pos(&planet->header, vec_make(p8_fixed_from_int(64), p8_fixed_from_int(64)));

    /* A fresh burst exports nothing, against a capacity of the production amount. */
    planet_inspection_panel(planet);
    ephemerals = main_ui_ephemerals();
    for (index = 0; index < ephemerals->count; index++) {
        const layout_row_t *lq = button_layout_row(entity_id_list_at(ephemerals, index));
        if (layout_row_index_is_string(lq, 1, "exports:")) {
            exports_layout_row = lq;
        }
    }
    assert(exports_layout_row != NULL);
    assert(layout_row_index_is_string(exports_layout_row, 2, "0/2"));

    /* Two routes in the outgoing routes list; a burst's own list has no
     * zone, since nothing in the game adds to it. */
    assert(outgoing_routes_present(planet) && trade_route_list_count(&planet->outgoing_routes) == 0);
    outgoing = &planet->outgoing_routes;
    trade_route_list_init(outgoing, game_zone());
    trade_route_list_add(outgoing, &route_a);
    trade_route_list_add(outgoing, &route_b);

    exports_layout_row = NULL;
    planet_inspection_panel(planet);
    ephemerals = main_ui_ephemerals();
    for (index = 0; index < ephemerals->count; index++) {
        const layout_row_t *lq = button_layout_row(entity_id_list_at(ephemerals, index));
        if (layout_row_index_is_string(lq, 1, "exports:")) {
            exports_layout_row = lq;
        }
    }
    assert(exports_layout_row != NULL);

    /* Two routes out of a capacity of two -- not "0/2", and not a number
     * assembled out of a neighbouring key. */
    assert(layout_row_index_is_string(exports_layout_row, 2, "2/2"));
}

int main(void) {
    test_burst_export_line_counts_the_outgoing_routes();

    printf("test_burst_exports_row: all tests passed\n");
    return 0;
}
