/* The defaults each class outside the planet kinds starts on, each against
 * its own data: a fresh struct of each module, then its ready cycle.
 * No shared reader, no table of modules. */

#include <assert.h>
#include <stdio.h>
#include <stdlib.h>

#include "p8/sprite_sheet.h"
#include "game/background.h"
#include "engine/bootstrap.h"
#include "game/cart.h"
#include "engine/dispatch_context.h"
#include "game/economy.h"
#include "p8/fixed.h"
#include "game/main_economy.h"
#include "p8/gfx.h"
#include "game/help.h"
#include "p8/input.h"
#include "game/layout.h"
#include "p8/vram.h"
#include "game/mouse_button.h"
#include "game/mouse.h"
#include "game/probe.h"
#include "game/research.h"
#include "game/structure_icon.h"
#include "game/score_panel.h"
#include "game/scroll.h"
#include "game/trade.h"
#include "engine/game_zone.h"
#include "game/ui.h"
#include "game/ui_button.h"
#include "game/wormhole.h"

static int same_vec(vec_t a, vec_t b) {
    return a.x == b.x && a.y == b.y;
}


static p8_vram_t memory;
static p8_graphics_t graphics;
static p8_input_t input;

static mouse_button_t *mouse_left_of(const mouse_t *nf) {
    assert(nf != NULL && mouse_left_button() != NULL);
    return (mouse_button_t *)mouse_left_button();
}

/* The second reset_world() (the one cart_start_game() runs) finds the
 * camera, the mouse and game/ui.c ready and leaves their state alone: a mark
 * written into the mouse's left button survives it; the camera and the ui
 * stay ready throughout. */
static void test_second_reset_world_keeps_specs(void) {
    mouse_button_t *mouse_left = mouse_left_of(main_mouse_typed());
    int hm = mouse_left->released;

    assert(camera_ready());
    mouse_left->released = 0x7777;
    assert(ui_ready());

    reset_world(NULL);

    assert(camera_ready());
    assert(mouse_left_of(main_mouse_typed()) == mouse_left && mouse_left_of(main_mouse_typed())->released == 0x7777);
    assert(ui_ready());
    mouse_left->released = hm;
}

/* A fresh background reads its archetype and starts on the entity's
 * defaults; ready until its shutdown. */
static void test_background_defaults_and_ready(void) {
    background_t *bg = background_new();

    assert(bg->archetype == &background_archetype && bg->archetype->tile_choices == &background_tile_choices_seq);
    assert(bg->header.state == bg->archetype->state && bg->header.state == ENTITY_STATE_NORMAL && bg->header.state_time == 0);
    assert(background_ready());
    background_shutdown();
    assert(!background_ready());
}

static void test_button_defaults_and_ready(void) {
    button_t *js = button_new(NULL);

    assert(js->archetype == &button_archetype && js->header.layer == js->archetype->layer);
    assert(js->header.state == js->archetype->state && js->header.state == ENTITY_STATE_NORMAL && js->header.state_time == 0);
    assert(button_ready());
    button_shutdown();
    assert(!button_ready());
}

static void test_mouse_pos_starts_at_zero(void) {
    const mouse_t *nf = mouse_new();

    assert(nf->archetype == &mouse_archetype);
    assert(entity_has_pos(&nf->header) && nf->header.pos.x == 0 && nf->header.pos.y == 0);
}

/* The position before shake and the position are copies of the archetype's. */
static void test_camera_defaults_and_ready(void) {
    camera_t *qb = camera_new();

    assert(qb->archetype == &camera_archetype);
    assert(same_vec(qb->header.map_pos, qb->archetype->pos_before_shake) && entity_has_map_pos(&qb->header) && same_vec(qb->header.pos, qb->archetype->pos) && entity_has_pos(&qb->header) && qb->header.shake == qb->archetype->shake);
    assert(qb->header.state == qb->archetype->state && qb->header.state == ENTITY_STATE_NORMAL && qb->header.state_time == 0);
    assert(camera_ready());
    camera_shutdown();
    assert(!camera_ready());
}

/* The wormhole's own state replaces the entity's; its too-far target holds
 * the status_text of the data. */
static void test_wormhole_defaults_and_ready(void) {
    wormhole_t *mz = wormhole_new();

    assert(mz->archetype == &wormhole_archetype && mz->targeting.archetype == &wormhole_archetype.targeting);
    assert(mz->header.state == mz->archetype->state && mz->header.state == ENTITY_STATE_PLACING && mz->header.state_time == 0);
    assert(mz->radius == mz->archetype->radius);
    assert(mz->targeting.cost == mz->archetype->targeting.cost && mz->targeting.cost == p8_fixed_from_int(3));
    assert(mz->targeting.has_cost == mz->archetype->targeting.has_cost);
    assert((mz->has & WORMHOLE_HAS_TOO_FAR_TARGET) && wormhole_too_far_target_wrapper()->status_text == wormhole_too_far_target_status_text);
    assert(wormhole_ready());
    wormhole_shutdown();
    assert(!wormhole_ready());
}

static void test_layout_ready(void) {
    assert(layout_row_ready() && tooltip_ready());
    layout_shutdown();
    assert(!layout_row_ready() && !tooltip_ready());
}

/* A fresh help screen: its archetype's defaults copied, the entity's under
 * them. */
static void test_help_screen_defaults_and_ready(void) {
    const probe_t *g_ = help_screen_new();

    assert(g_->archetype == &help_screen_archetype && g_->emitter.archetype == &help_screen_archetype.emitter &&
           g_->targeting.archetype == &help_screen_archetype.targeting);
    assert(entity_has_pos(&g_->header) && same_vec(g_->header.pos, g_->archetype->pos));
    assert(g_->emitter.trail_colors == g_->archetype->emitter.trail_colors);
    assert(g_->emitter.matrix_xx == g_->archetype->emitter.matrix_xx && g_->emitter.matrix_xy == g_->archetype->emitter.matrix_xy);
    assert(g_->emitter.matrix_yx == g_->archetype->emitter.matrix_yx && g_->emitter.matrix_yy == g_->archetype->emitter.matrix_yy);
    assert(g_->emitter.angle_slices == g_->archetype->emitter.angle_slices && g_->emitter.base_radius == g_->archetype->emitter.base_radius);
    assert(g_->emitter.speed_factor == g_->archetype->emitter.speed_factor);
    assert(g_->targeting.cost == g_->archetype->targeting.cost);
    assert(g_->targeting.has_cost == g_->archetype->targeting.has_cost);
    assert(g_->targeting.has_cost == 1);
    assert(g_->header.state == g_->archetype->state && g_->header.state == ENTITY_STATE_NORMAL && g_->header.state_time == 0);
    assert(help_screen_ready());
    help_shutdown();
    assert(!help_screen_ready());
}

/* A fresh probe: its archetype's defaults copied, the entity's under them;
 * no position yet. */
static void test_probe_defaults_and_ready(void) {
    probe_t *g_ = probe_new(NULL);

    assert(g_->archetype == &probe_archetype && g_->emitter.archetype == &probe_archetype.emitter &&
           g_->targeting.archetype == &probe_archetype.targeting);
    assert(g_->targeting.cost == g_->archetype->targeting.cost);
    assert(g_->targeting.has_cost == g_->archetype->targeting.has_cost && g_->targeting.has_cost == 1);
    assert(g_->emitter.speed_factor == g_->archetype->emitter.speed_factor);
    assert(g_->emitter.angle_slices == g_->archetype->emitter.angle_slices);
    assert(g_->emitter.trail_colors == g_->archetype->emitter.trail_colors);
    assert(g_->emitter.matrix_xx == g_->archetype->emitter.matrix_xx && g_->emitter.matrix_xy == g_->archetype->emitter.matrix_xy);
    assert(g_->emitter.matrix_yx == g_->archetype->emitter.matrix_yx && g_->emitter.matrix_yy == g_->archetype->emitter.matrix_yy);
    assert(g_->emitter.base_radius == g_->archetype->emitter.base_radius);
    assert(!entity_has_pos(&g_->header));
    assert(g_->header.state == g_->archetype->state && g_->header.state == ENTITY_STATE_NORMAL && g_->header.state_time == 0);
    assert(probe_ready());
    probe_shutdown();
    assert(!probe_ready());
}

static void test_research_project_defaults_and_ready(void) {
    research_project_t *hp = research_project_new(NULL);

    assert(hp->archetype == &research_project_archetype && hp->archetype->row_tail == &research_project_row_tail_row);
    assert(research_project_ready());
    research_shutdown();
    assert(!research_project_ready());
}

static void test_structure_icon_defaults_and_ready(void) {
    structure_icon_t *ie = structure_icon_new(NULL);

    assert(ie->archetype == &structure_icon_archetype && entity_has_pos(&ie->header) && same_vec(ie->header.pos, ie->archetype->pos));
    assert(ie->header.state == ie->archetype->state && ie->header.state == ENTITY_STATE_NORMAL && ie->header.state_time == 0);
    assert(structure_icon_ready());
    structure_icon_shutdown();
    assert(!structure_icon_ready());
}

/* score_panel_layout_block() holds the record of the data. */
static void test_score_panel_defaults_and_ready(void) {
    score_panel_t *pj = score_panel_new();
    const score_panel_layout_block_t *cr = score_panel_layout_block();

    assert(pj->archetype == &score_panel_archetype && pj->header.layer == pj->archetype->layer);
    assert(score_panel_last_box(pj) == NULL);
    assert((cr->has & SCORE_PANEL_LAYOUT_BLOCK_HAS_FIXED_POS) && cr->fixed_pos.x == 0 && cr->fixed_pos.y == p8_fixed_from_int(11));
    assert((cr->has & SCORE_PANEL_LAYOUT_BLOCK_HAS_ROW_STEP) && cr->row_step.x == 0 && cr->row_step.y == p8_fixed_from_int(11));
    assert(cr->back_color == p8_fixed_from_int(10));
    assert(pj->header.state == pj->archetype->state && pj->header.state == ENTITY_STATE_NORMAL && pj->header.state_time == 0);
    assert(score_panel_ready());
    score_panel_shutdown();
    assert(!score_panel_ready());
}

/* The economy and the trade route: no defaults, a root struct. */
static void test_economy_ready(void) {
    economy_t *hy = economy_new();

    assert(hy->header.class == economy_class());
    assert(economy_ready());
    economy_shutdown();
    assert(!economy_ready());
}

static void test_trade_ready(void) {
    assert(trade_ready());
    trade_shutdown();
    assert(!trade_ready());
}

int main(void) {
    p8_vram_init(&memory);
    p8_gfx_init(&graphics);
    p8_sprite_sheet_load(&memory);
    p8_input_init(&input);
    dispatch_context_set_gfx(&graphics);
    dispatch_context_set_mem(&memory);
    dispatch_context_set_input(&input);

    assert(!trade_ready());
    assert(!economy_ready());
    assert(!research_project_ready());
    assert(!score_panel_ready());
    assert(!structure_icon_ready());
    assert(!probe_ready());
    assert(!help_screen_ready());
    cart_load();
    reset_world(NULL);

    test_second_reset_world_keeps_specs();
    test_background_defaults_and_ready();
    test_button_defaults_and_ready();
    test_camera_defaults_and_ready();
    test_mouse_pos_starts_at_zero();
    test_wormhole_defaults_and_ready();
    test_layout_ready();
    test_help_screen_defaults_and_ready();
    test_probe_defaults_and_ready();
    test_research_project_defaults_and_ready();
    test_structure_icon_defaults_and_ready();
    test_score_panel_defaults_and_ready();
    test_economy_ready();
    test_trade_ready();

    printf("test_spec_defaults: all tests passed\n");
    return 0;
}
