-- Help and the score panel receive cart_request_start_game and cart_init_method at cart_load time,
-- never by require: no game module requires the cart, so it requires everything at load.
local bootstrap = require("engine.bootstrap")
local dispatch_context = require("engine.dispatch_context")
local entity = require("engine.entity")
local entity_class = require("engine.entity_class")
local entity_registry = require("engine.entity_registry")
local exit_queue = require("engine.exit_queue")
local background = require("game.background")
local button_image = require("game.button_image")
local data_init = require("game.data_init")
local demo_overlay = require("game.demo_overlay")
local economy_module = require("game.economy")
local economy_score = require("game.economy_score")
local galaxy = require("game.galaxy")
local game_data = require("game.game_data")
local help = require("game.help")
local hud = require("game.hud")
local layout = require("game.layout")
local loading_screen = require("game.loading_screen")
local map_tables = require("game.map_tables")
local menu = require("game.menu")
local message_box = require("game.message_box")
local mouse = require("game.mouse")
local planet = require("game.planet")
local planet_family = require("game.planet_family")
local planet_family_trade = require("game.planet_family_trade")
local probe = require("game.probe")
local research = require("game.research")
local save_load = require("game.save_load")
local save_state = require("game.save_state")
local save_storage = require("game.save_storage")
local score_panel = require("game.score_panel")
local scroll = require("game.scroll")
local slipway = require("game.slipway")
local structure_icon = require("game.structure_icon")
local text_image = require("game.text_image")
local trade = require("game.trade")
local ui = require("game.ui")
local ui_button = require("game.ui_button")
local wormhole = require("game.wormhole")
local fixed = require("p8.fixed")
local gfx = require("p8.gfx")

local M = {}

M.CART_NO_DEMO_FRAME = -1

local g_title_built = false
local g_save_cut = false
local g_demo_frame = M.CART_NO_DEMO_FRAME
-- 0 when not loading, else the loading frame being played, from 1 (the press) to LOADING_SCREEN_FRAMES.
local g_loading_frame = 0
local g_requested_option = nil

local function quit_game()
  entity_registry.abandon_live_entities()
  M.cart_flush_exit_queue()
  entity_registry.forget_registries()
end

-- Do not list the inits data_init.init() and score_panel_init() run themselves: they would run twice.
function M.cart_load()
  local memory

  quit_game()
  g_title_built = false
  g_save_cut = false
  g_demo_frame = M.CART_NO_DEMO_FRAME
  g_loading_frame = 0
  entity.init()
  assert(entity.ready(), "cart_load: entity.init() must run before every class that extends entity_t")
  scroll.camera_init()
  mouse.mouse_init()
  ui.ui_init()

  layout.init()
  trade.trade_init()

  ui_button.button_init()
  ui_button.modal_backdrop_init()
  background.init()
  message_box.message_box_init()
  message_box.toast_init()
  probe.probe_init()
  economy_module.economy_init()

  memory = dispatch_context.mem()
  if memory ~= nil then
    data_init.init()
  end

  planet.init()
  slipway.slipway_init()
  planet_family.init()
  assert(planet_family.structure_ready() and planet_family.burst_ready() and planet.ready(),
    "cart_load: structure and burst extend planet: planet.init() must run first")
  planet_family_trade.init()
  assert(planet_family_trade.slipgate_ready() and planet_family_trade.synthesizer_ready()
      and planet_family.structure_ready() and planet_family.ascension_ready(),
    "cart_load: slipgate extends structure and synthesizer extends ascension: planet_family.init() must run first")
  wormhole.wormhole_init()

  help.help_init()
  assert(help.help_screen_ready() and probe.probe_ready(),
    "cart_load: the help screen extends probe: probe_init() must run first")
  score_panel.score_panel_init()
  structure_icon.structure_icon_init()
  menu.menu_init()

  help.help_set_start_game(M.cart_request_start_game)
  score_panel.score_panel_set_init(M.cart_init_method)
end

function M.cart_flush_exit_queue()
  exit_queue.flush()
end

function M.cart_shutdown()
  quit_game()
  menu.menu_shutdown()
  structure_icon.structure_icon_shutdown()
  score_panel.score_panel_shutdown()
  help.help_shutdown()

  wormhole.wormhole_shutdown()
  planet_family_trade.shutdown()
  planet_family.shutdown()
  slipway.slipway_shutdown()
  planet.shutdown()

  data_init.shutdown()

  economy_module.economy_shutdown()
  probe.probe_shutdown()
  message_box.toast_shutdown()
  message_box.message_box_shutdown()
  background.shutdown()
  ui_button.modal_backdrop_shutdown()
  ui_button.button_shutdown()

  trade.trade_shutdown()
  layout.shutdown()

  ui.ui_shutdown()
  mouse.mouse_shutdown()
  scroll.camera_shutdown()
  entity.shutdown()
end

local g_save_state = {}
save_state.save_state_zero(g_save_state)
local g_save_bytes = {}
for i = 1, save_state.SAVE_STATE_MAX_BYTES do
  g_save_bytes[i] = 0
end

local function resume_saved_game(self)
  M.cart_resume_game(g_save_state)
end

local function title_resume_handler()
  local count = save_storage.save_storage_read(g_save_bytes)

  if count > 0 and save_state.save_state_decode(g_save_bytes, count, g_save_state) then
    return resume_saved_game
  else
    return nil
  end
end

function M.cart_init()
  assert(structure_icon.structure_icon_ready(), "cart_init: cart_load() must run first")
  assert(help.help_start_game_field_value() == M.cart_request_start_game
      and score_panel.score_panel_init_field_value() == M.cart_init_method,
    "cart_init: help_init() and score_panel_init() reset these bridges, so cart_load() must "
      .. "install them afterwards: every button copies the value when it is built")

  g_loading_frame = 0
  bootstrap.reset_world()
  research.research_expire_project_ids()

  if structure_icon.structure_icon_rest_cursor() ~= nil then
    structure_icon.structure_icon_set_cursor(structure_icon.structure_icon_rest_cursor())
  end
  message_box.message_box_set_shown(nil)

  if probe.main_economy_typed() ~= nil then probe.probe_set_main_economy(nil) end
  score_panel.score_panel_set_main(nil)

  help.help_set_resume_game(title_resume_handler())
  help.help_screen_new()
  g_title_built = true
end

function M.cart_title_shown()
  return g_title_built and g_loading_frame == 0 and message_box.shown_message_box() == nil
    and entity_registry.count_tagged(entity_class.TAG_MESSAGE_BOX) == 0
end

-- Between frames only, never from a handler inside ui_update or update_entities: it releases
-- every live entity while they are still being walked.
function M.cart_return_to_title()
  entity_registry.abandon_live_entities()
  M.cart_flush_exit_queue()
  -- After the flush: reset_world's own abandon would walk the released entries.
  entity_registry.forget_registries()
  g_save_cut = false
  g_demo_frame = M.CART_NO_DEMO_FRAME
  M.cart_init()
end

function M.cart_cut_save()
  g_save_cut = true
end

function M.cart_set_demo_frame(frame)
  g_demo_frame = frame
end

function M.cart_init_method(self)
  M.cart_init()
end

local function start_world(difficulty)
  assert(economy_module.economy_ready() and planet_family.structure_ready(),
    "game start: cart_load() must run first")

  economy_score.economy_score_set_difficulty(difficulty)
  g_title_built = false

  bootstrap.reset_world()
  research.research_expire_project_ids()
  background.new()
end

local function start_interface()
  local kind_table = {}

  probe.probe_set_main_economy(economy_module.economy_new())
  score_panel.score_panel_set_main(score_panel.score_panel_new())

  if menu.pause_menu_open_box() ~= nil then
    for j = 1, game_data.GALAXY_MENU_COUNT do
      menu.galaxy_menu_button(j)
    end
  end

  kind_table[1] = planet_family.structure_kind()
  kind_table[2] = planet_family.processor_kind()
  kind_table[3] = planet_family_trade.slipgate_kind()
  kind_table[4] = planet_family_trade.infraspace_gate_kind()
  kind_table[5] = planet_family.trading_hub_kind()
  kind_table[6] = planet_family_trade.synthesizer_kind()
  kind_table[7] = planet_family.ascension_kind()

  for index = 1, map_tables.ICON_TOOLTIPS_COUNT do
    local keys = { kind = nil, tooltip = nil }

    keys.kind = kind_table[index]
    keys.tooltip = layout.tooltip_new(map_tables.icon_tooltips_table[index])
    structure_icon.structure_icon_new(keys)
  end
end

local function recompute_economy()
  assert(probe.main_economy_typed() ~= nil,
    "game start: the main economy must be this game's economy before economy_recompute()")
  economy_module.economy_recompute(probe.main_economy_typed())
end

function M.cart_start_game(chosen_option)
  start_world(chosen_option)
  galaxy.populate(fixed.from_int(90))
  start_interface()
  recompute_economy()
end

-- The title's difficulty buttons: copies the option's value, then the press frame and the next
-- draw the loading screen and the frame after generates the galaxy. nil starts with no difficulty.
function M.cart_request_start_game(chosen_option)
  g_requested_option = nil
  if chosen_option ~= nil then
    g_requested_option = ui_button.button_zero()
    g_requested_option.name = chosen_option.name
    g_requested_option.trade = chosen_option.trade
    g_requested_option.type_bias = chosen_option.type_bias
  end
  g_loading_frame = 1
end

function M.cart_loading()
  return g_loading_frame ~= 0
end

function M.cart_resume_game(state)
  local difficulty = {}

  save_load.save_load_difficulty(state, difficulty)
  save_load.save_load_background_seed(state)
  start_world(difficulty)
  assert(background.live().seed == state.background_seed,
    "cart_resume_game: the RNG moved between the reseed and the background")
  save_load.save_load_world(state)
  start_interface()
  save_load.save_load_globals(state)
  recompute_economy()
end

local function save_at_frame_end()
  local economy = probe.main_economy_typed()

  if economy == nil or not economy.save_pending or economy_score.economy_score_difficulty() == nil then return end
  if economy.end_status ~= nil then
    save_storage.save_storage_erase()
    economy.save_pending = false
  elseif wormhole.wormhole_live_count() == 0 then
    save_state.save_state_capture(g_save_state)
    save_storage.save_storage_write(g_save_bytes, save_state.save_state_encode(g_save_state, g_save_bytes))
    economy.save_pending = false
  end
end

function M.cart_update60()
  if g_loading_frame > 0 and g_loading_frame < loading_screen.LOADING_SCREEN_FRAMES then
    g_loading_frame = g_loading_frame + 1
    return
  end
  if g_loading_frame == loading_screen.LOADING_SCREEN_FRAMES then
    M.cart_flush_exit_queue()
    M.cart_start_game(g_requested_option)
    g_loading_frame = 0
    if not g_save_cut then save_at_frame_end() end
    return
  end
  M.cart_flush_exit_queue()
  bootstrap.update_entities()
  ui.ui_update(dispatch_context.mem(), ui.main_ui())
  if not g_save_cut then save_at_frame_end() end
end

function M.cart_draw()
  local mem = dispatch_context.mem()

  if g_loading_frame ~= 0 then
    loading_screen.loading_screen_draw(dispatch_context.gfx(), mem)
    return
  end
  text_image.frame_begin()
  button_image.frame_begin()
  gfx.cls(mem)
  hud.hud_select_palette_block_default(mem)
  entity_registry.draw_layers(entity_registry.MESSAGE_DRAW)
  entity_registry.draw_layers(entity_registry.MESSAGE_DRAW_OVERLAY)
  demo_overlay.demo_overlay_draw(dispatch_context.gfx(), mem, g_demo_frame)
end

return M
