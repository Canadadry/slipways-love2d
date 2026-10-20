#include "game/cart.h"

#include <assert.h>
#include <stddef.h>

#include "game/background.h"
#include "engine/bootstrap.h"
#include "game/main_economy.h"
#include "engine/entity_class.h"
#include "game/data_init.h"
#include "game/demo_overlay.h"
#include "engine/dispatch_context.h"
#include "game/economy.h"
#include "game/economy_score.h"
#include "engine/entity_registry.h"
#include "engine/exit_queue.h"
#include "p8/fixed.h"
#include "game/galaxy.h"
#include "p8/gfx.h"
#include "game/help.h"
#include "game/hud.h"
#include "engine/entity.h"
#include "game/layout.h"
#include "game/loading_screen.h"
#include "p8/vram.h"
#include "game/menu.h"
#include "game/mouse.h"
#include "game/message_box.h"
#include "game/grab_arena.h"
#include "game/planet.h"
#include "game/planet_family.h"
#include "game/planet_family_trade.h"
#include "game/probe.h"
#include "game/research.h"
#include "game/save_load.h"
#include "game/save_state.h"
#include "game/save_storage.h"
#include "game/structure_icon.h"
#include "game/score_panel.h"
#include "game/scroll.h"
#include "game/slipway.h"
#include "game/trade.h"
#include "game/text_image.h"
#include "game/button_image.h"
#include "game/ui.h"
#include "game/ui_button.h"
#include "game/wormhole.h"
#include "engine/frame_zone.h"
#include "engine/game_zone.h"

static int g_title_built;
static int g_save_cut;
static int g_demo_frame = CART_NO_DEMO_FRAME;
/* 0 when not loading, else the loading frame being played, from 1 (the press) to LOADING_SCREEN_FRAMES. */
static int g_loading_frame;
static button_t g_requested_option;
static int g_has_requested_option;

static void quit_game_zone(void) {
    entity_registry_abandon_live_entities();
    cart_flush_exit_queue();
    game_zone_reset();
    entity_registry_forget_registries();
    grab_arena_forget_free();
}

/* Do not list the inits data_init() and score_panel_init() run themselves: they would run twice. */
void cart_load(void) {
    p8_vram_t *memory;

    quit_game_zone();
    g_title_built = 0;
    g_save_cut = 0;
    g_demo_frame = CART_NO_DEMO_FRAME;
    g_loading_frame = 0;
    entity_init();
    assert(entity_ready() && "cart_load: entity_init() must run before every class that extends entity_t");
    camera_init();
    mouse_init();
    ui_init();

    layout_init();
    trade_init();

    button_init();
    modal_backdrop_init();
    background_init();
    message_box_init();
    toast_init();
    probe_init();
    economy_init();

    memory = dispatch_context_mem();
    if (memory != NULL) {
        data_init();
    }

    planet_init();
    slipway_init();
    planet_family_init();
    assert(structure_ready() && burst_ready() && planet_ready() &&
           "cart_load: structure and burst extend planet: planet_init() must run first");
    planet_family_trade_init();
    assert(slipgate_ready() && synthesizer_ready() && structure_ready() && ascension_ready() &&
           "cart_load: slipgate extends structure and synthesizer extends ascension: planet_family_init() must run first");
    wormhole_init();

    help_init();
    assert(help_screen_ready() && probe_ready() && "cart_load: the help screen extends probe: probe_init() must run first");
    score_panel_init();
    structure_icon_init();
    menu_init();

    help_set_start_game(cart_request_start_game);
    score_panel_set_init(cart_init_method);
}

static void forget_game_zone(void) {
    grab_arena_forget_free();
}

void cart_flush_exit_queue(void) {
    exit_queue_flush();
    if (game_zone_pending()) {
        forget_game_zone();
        game_zone_release();
    }
}

void cart_shutdown(void) {
    quit_game_zone();
    menu_shutdown();
    structure_icon_shutdown();
    score_panel_shutdown();
    help_shutdown();

    wormhole_shutdown();
    planet_family_trade_shutdown();
    planet_family_shutdown();
    slipway_shutdown();
    planet_shutdown();

    data_init_shutdown();

    economy_shutdown();
    probe_shutdown();
    toast_shutdown();
    message_box_shutdown();
    background_shutdown();
    modal_backdrop_shutdown();
    button_shutdown();

    trade_shutdown();
    layout_shutdown();

    ui_shutdown();
    mouse_shutdown();
    camera_shutdown();
    entity_shutdown();
}

static save_state_t g_save_state;
static uint8_t g_save_bytes[SAVE_STATE_MAX_BYTES];

static void resume_saved_game(button_t *self) {
    (void)self;
    cart_resume_game(&g_save_state);
}

static button_fn_t title_resume_handler(void) {
    int count = save_storage_read(g_save_bytes, (int)sizeof g_save_bytes);

    return (count > 0 && save_state_decode(g_save_bytes, count, &g_save_state)) ? resume_saved_game : NULL;
}

void cart_init(void) {
    assert(structure_icon_ready() && "cart_init: cart_load() must run first");
    assert(help_start_game_field_value() == cart_request_start_game &&
           score_panel_init_field_value() == cart_init_method &&
           "cart_init: help_init() and score_panel_init() reset these bridges, so cart_load() must "
           "install them afterwards: every button copies the value when it is built");

    g_loading_frame = 0;
    if (!game_zone_pending() && game_zone_used() > 0) game_zone_flip();
    reset_world(NULL);
    research_expire_project_ids();

    if (structure_icon_rest_cursor() != NULL) {
        structure_icon_set_cursor(*structure_icon_rest_cursor());
    }
    message_box_set_shown(NULL);

    if (main_economy_typed() != NULL) probe_set_main_economy(NULL);
    score_panel_set_main(NULL);

    help_set_resume_game(title_resume_handler());
    (void)help_screen_new();
    g_title_built = 1;
}

int cart_title_shown(void) {
    return g_title_built && g_loading_frame == 0 && shown_message_box() == NULL && entity_registry_count_tagged(TAG_MESSAGE_BOX) == 0;
}

/* Between frames only, never from a handler inside ui_update or update_entities: it releases
   every live entity while they are still being walked. */
void cart_return_to_title(void) {
    entity_registry_abandon_live_entities();
    cart_flush_exit_queue();
    /* After the flush: reset_world's own abandon would walk the released entries. */
    entity_registry_forget_registries();
    g_save_cut = 0;
    g_demo_frame = CART_NO_DEMO_FRAME;
    cart_init();
}

void cart_cut_save(void) {
    g_save_cut = 1;
}

void cart_set_demo_frame(int frame) {
    g_demo_frame = frame;
}

void cart_init_method(button_t *self) {
    (void)self;
    cart_init();
}

const entity_registry_reserve_t cart_game_registry_reserve = {
    329,
    {
        [ENTITY_MESSAGE_DRAW] = 322,
        [ENTITY_MESSAGE_DRAW_OVERLAY] = 265,
        [ENTITY_MESSAGE_HIT_TEST] = 125,
        [ENTITY_MESSAGE_NEW_YEAR] = 111,
        [ENTITY_MESSAGE_REFRESH] = 120,
    },
    {
        [TAG_STRUCTURE] = 8,
        [TAG_ASCENSION] = 3,
        [TAG_SLIPGATE] = 14,
        [TAG_PROBE] = 2,
        [TAG_MESSAGE_BOX] = 4,
        [TAG_SLIPWAY] = 216,
        [TAG_WORMHOLE] = 3,
        [TAG_PLANET] = 110,
        [TAG_NODE] = 110,
    },
};

static void start_world(const button_t *difficulty) {
    assert(economy_ready() && structure_ready() && "game start: cart_load() must run first");

    economy_score_set_difficulty(difficulty);
    g_title_built = 0;
    game_zone_flip();

    reset_world(&cart_game_registry_reserve);
    research_expire_project_ids();
    (void)background_new();
}

static void start_interface(void) {
    const entity_kind_t *kind_table[7];
    int index;

    probe_set_main_economy((entity_t *)economy_new());
    score_panel_set_main(score_panel_new());

    if (pause_menu_open_box() != NULL) {
        int j;

        for (j = 1; j <= GALAXY_MENU_COUNT; j++) {
            (void)galaxy_menu_button(j);
        }
    }

    kind_table[0] = structure_kind();
    kind_table[1] = processor_kind();
    kind_table[2] = slipgate_kind();
    kind_table[3] = infraspace_gate_kind();
    kind_table[4] = trading_hub_kind();
    kind_table[5] = synthesizer_kind();
    kind_table[6] = ascension_kind();

    for (index = 0; index < 7; index++) {
        structure_icon_keys_t keys = {0};

        keys.kind = kind_table[index];
        keys.tooltip = tooltip_new(game_zone(), &icon_tooltips_table[index]);
        (void)structure_icon_new(&keys);
    }
}

static void recompute_economy(void) {
    assert(main_economy_typed() != NULL && "game start: the main economy must be this game's economy before economy_recompute()");
    economy_recompute(main_economy_typed());
}

void cart_start_game(button_t *chosen_option) {
    start_world(chosen_option);
    galaxy_populate(p8_fixed_from_int(90));
    start_interface();
    recompute_economy();
}

void cart_request_start_game(button_t *chosen_option) {
    g_has_requested_option = chosen_option != NULL;
    g_requested_option = (button_t){0};
    if (chosen_option != NULL) {
        g_requested_option.name = chosen_option->name;
        g_requested_option.has_trade = chosen_option->has_trade;
        g_requested_option.trade = chosen_option->trade;
        g_requested_option.has_type_bias = chosen_option->has_type_bias;
        g_requested_option.type_bias = chosen_option->type_bias;
    }
    g_loading_frame = 1;
}

int cart_loading(void) {
    return g_loading_frame != 0;
}

void cart_resume_game(const save_state_t *state) {
    button_t difficulty;

    save_load_difficulty(state, &difficulty);
    save_load_background_seed(state);
    start_world(&difficulty);
    assert(background_live()->seed == state->background_seed && "cart_resume_game: the RNG moved between the reseed and the background");
    save_load_world(state);
    start_interface();
    save_load_globals(state);
    recompute_economy();
}

static void save_at_frame_end(void) {
    economy_t *economy = main_economy_typed();

    if (economy == NULL || !economy->save_pending || economy_score_difficulty() == NULL) return;
    if (economy->has_end_status && economy->end_status != NULL) {
        (void)save_storage_erase();
        economy->save_pending = 0;
    } else if (wormhole_live_count() == 0) {
        save_state_capture(&g_save_state);
        (void)save_storage_write(g_save_bytes, save_state_encode(&g_save_state, g_save_bytes));
        economy->save_pending = 0;
    }
}

void cart_update60(void) {
    if (g_loading_frame > 0 && g_loading_frame < LOADING_SCREEN_FRAMES) {
        g_loading_frame++;
        return;
    }
    if (g_loading_frame == LOADING_SCREEN_FRAMES) {
        frame_zone_reset();
        cart_flush_exit_queue();
        cart_start_game(g_has_requested_option ? &g_requested_option : NULL);
        g_loading_frame = 0;
        if (!g_save_cut) save_at_frame_end();
        return;
    }
    frame_zone_reset();
    cart_flush_exit_queue();
    update_entities();
    ui_update(dispatch_context_mem(), main_ui());
    if (!g_save_cut) save_at_frame_end();
}

void cart_draw(void) {
    p8_vram_t *mem = dispatch_context_mem();

    if (g_loading_frame != 0) {
        loading_screen_draw(dispatch_context_gfx(), mem);
        return;
    }
    text_image_frame_begin();
    button_image_frame_begin();
    p8_cls(mem);
    hud_select_palette_block_default(mem);
    draw_layers(ENTITY_MESSAGE_DRAW);
    draw_layers(ENTITY_MESSAGE_DRAW_OVERLAY);
    demo_overlay_draw(dispatch_context_gfx(), mem, g_demo_frame);
}
