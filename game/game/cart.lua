-- Help and the score panel receive cart.request_start_game and cart.init_method at cart.load time,
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

M.NO_DEMO_FRAME = -1

local g_title_built = false
local g_save_cut = false
local g_demo_frame = M.NO_DEMO_FRAME
-- 0 when not loading, else the loading frame being played, from 1 (the press) to loading_screen.FRAMES.
local g_loading_frame = 0
local g_requested_option = nil

local function quit_game()
  entity_registry.abandon_live_entities()
  M.flush_exit_queue()
  entity_registry.forget_registries()
end

-- Do not list the inits data_init.init() and score_panel.init() run themselves: they would run twice.
function M.load()
  local memory

  quit_game()
  g_title_built = false
  g_save_cut = false
  g_demo_frame = M.NO_DEMO_FRAME
  g_loading_frame = 0
  entity.init()
  assert(entity.ready(), "cart.load: entity.init() must run before every class that extends entity_t")
  scroll.camera_init()
  mouse.init()
  ui.init()

  layout.init()
  trade.init()

  ui_button.button_init()
  ui_button.modal_backdrop_init()
  background.init()
  message_box.init()
  message_box.toast_init()
  probe.init()
  economy_module.init()

  memory = dispatch_context.mem()
  if memory ~= nil then
    data_init.init()
  end

  planet.init()
  slipway.init()
  planet_family.init()
  assert(planet_family.structure_ready() and planet_family.burst_ready() and planet.ready(),
    "cart.load: structure and burst extend planet: planet.init() must run first")
  planet_family_trade.init()
  assert(planet_family_trade.slipgate_ready() and planet_family_trade.synthesizer_ready()
      and planet_family.structure_ready() and planet_family.ascension_ready(),
    "cart.load: slipgate extends structure and synthesizer extends ascension: planet_family.init() must run first")
  wormhole.init()

  help.init()
  assert(help.screen_ready() and probe.ready(),
    "cart.load: the help screen extends probe: probe.init() must run first")
  score_panel.init()
  structure_icon.init()
  menu.init()

  help.set_start_game(M.request_start_game)
  score_panel.set_init(M.init_method)
end

function M.flush_exit_queue()
  exit_queue.flush()
end

function M.shutdown()
  quit_game()
  menu.shutdown()
  structure_icon.shutdown()
  score_panel.shutdown()
  help.shutdown()

  wormhole.shutdown()
  planet_family_trade.shutdown()
  planet_family.shutdown()
  slipway.shutdown()
  planet.shutdown()

  data_init.shutdown()

  economy_module.shutdown()
  probe.shutdown()
  message_box.toast_shutdown()
  message_box.shutdown()
  background.shutdown()
  ui_button.modal_backdrop_shutdown()
  ui_button.button_shutdown()

  trade.shutdown()
  layout.shutdown()

  ui.shutdown()
  mouse.shutdown()
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
  M.resume_game(g_save_state)
end

local function title_resume_handler()
  local count = save_storage.save_storage_read(g_save_bytes)

  if count > 0 and save_state.save_state_decode(g_save_bytes, count, g_save_state) then
    return resume_saved_game
  else
    return nil
  end
end

function M.init()
  assert(structure_icon.ready(), "cart.init: cart.load() must run first")
  assert(help.start_game_field_value() == M.request_start_game
      and score_panel.init_field_value() == M.init_method,
    "cart.init: help.init() and score_panel.init() reset these bridges, so cart.load() must "
      .. "install them afterwards: every button copies the value when it is built")

  g_loading_frame = 0
  bootstrap.reset_world()
  research.expire_project_ids()

  if structure_icon.rest_cursor() ~= nil then
    structure_icon.set_cursor(structure_icon.rest_cursor())
  end
  message_box.set_shown(nil)

  if probe.main_economy_typed() ~= nil then probe.set_main_economy(nil) end
  score_panel.set_main(nil)

  help.set_resume_game(title_resume_handler())
  help.screen_new()
  g_title_built = true
end

function M.title_shown()
  return g_title_built and g_loading_frame == 0 and message_box.shown_message_box() == nil
    and entity_registry.count_tagged(entity_class.TAG_MESSAGE_BOX) == 0
end

-- Between frames only, never from a handler inside ui.update or update_entities: it releases
-- every live entity while they are still being walked.
function M.return_to_title()
  entity_registry.abandon_live_entities()
  M.flush_exit_queue()
  -- After the flush: reset_world's own abandon would walk the released entries.
  entity_registry.forget_registries()
  g_save_cut = false
  g_demo_frame = M.NO_DEMO_FRAME
  M.init()
end

function M.cut_save()
  g_save_cut = true
end

function M.set_demo_frame(frame)
  g_demo_frame = frame
end

function M.init_method(self)
  M.init()
end

local function start_world(difficulty)
  assert(economy_module.ready() and planet_family.structure_ready(),
    "game start: cart.load() must run first")

  economy_score.set_difficulty(difficulty)
  g_title_built = false

  bootstrap.reset_world()
  research.expire_project_ids()
  background.new()
end

local function start_interface()
  local kind_table = {}

  probe.set_main_economy(economy_module.new())
  score_panel.set_main(score_panel.new())

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
    structure_icon.new(keys)
  end
end

local function recompute_economy()
  assert(probe.main_economy_typed() ~= nil,
    "game start: the main economy must be this game's economy before economy.recompute()")
  economy_module.recompute(probe.main_economy_typed())
end

function M.start_game(chosen_option)
  start_world(chosen_option)
  galaxy.populate(fixed.from_int(90))
  start_interface()
  recompute_economy()
end

-- The title's difficulty buttons: copies the option's value, then the press frame and the next
-- draw the loading screen and the frame after generates the galaxy. nil starts with no difficulty.
function M.request_start_game(chosen_option)
  g_requested_option = nil
  if chosen_option ~= nil then
    g_requested_option = ui_button.button_zero()
    g_requested_option.name = chosen_option.name
    g_requested_option.trade = chosen_option.trade
    g_requested_option.type_bias = chosen_option.type_bias
  end
  g_loading_frame = 1
end

function M.loading()
  return g_loading_frame ~= 0
end

function M.resume_game(state)
  local difficulty = {}

  save_load.save_load_difficulty(state, difficulty)
  save_load.save_load_background_seed(state)
  start_world(difficulty)
  assert(background.live().seed == state.background_seed,
    "cart.resume_game: the RNG moved between the reseed and the background")
  save_load.save_load_world(state)
  start_interface()
  save_load.save_load_globals(state)
  recompute_economy()
end

local function save_at_frame_end()
  local economy = probe.main_economy_typed()

  if economy == nil or not economy.save_pending or economy_score.difficulty() == nil then return end
  if economy.end_status ~= nil then
    save_storage.save_storage_erase()
    economy.save_pending = false
  elseif wormhole.live_count() == 0 then
    save_state.save_state_capture(g_save_state)
    save_storage.save_storage_write(g_save_bytes, save_state.save_state_encode(g_save_state, g_save_bytes))
    economy.save_pending = false
  end
end

function M.update60()
  if g_loading_frame > 0 and g_loading_frame < loading_screen.FRAMES then
    g_loading_frame = g_loading_frame + 1
    return
  end
  if g_loading_frame == loading_screen.FRAMES then
    M.flush_exit_queue()
    M.start_game(g_requested_option)
    g_loading_frame = 0
    if not g_save_cut then save_at_frame_end() end
    return
  end
  M.flush_exit_queue()
  bootstrap.update_entities()
  ui.update(dispatch_context.mem(), ui.main_ui())
  if not g_save_cut then save_at_frame_end() end
end

function M.draw()
  local mem = dispatch_context.mem()

  if g_loading_frame ~= 0 then
    loading_screen.draw(dispatch_context.gfx(), mem)
    return
  end
  text_image.frame_begin()
  button_image.frame_begin()
  gfx.cls(mem)
  hud.select_palette_block_default(mem)
  entity_registry.draw_layers(entity_registry.MESSAGE_DRAW)
  entity_registry.draw_layers(entity_registry.MESSAGE_DRAW_OVERLAY)
  demo_overlay.draw(dispatch_context.gfx(), mem, g_demo_frame)
end

return M
