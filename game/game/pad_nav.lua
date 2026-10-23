local bit = require("bit")
local entity_class = require("engine.entity_class")
local entity_id = require("engine.entity_id")
local entity_registry = require("engine.entity_registry")
local geom = require("engine.geom")
local vec = require("engine.vec")
local background = require("game.background")
local bindings_module = require("game.bindings")
local cheat = require("game.cheat")
local menu = require("game.menu")
local message_box = require("game.message_box")
local pad_back = require("game.pad_back")
local pad_build = require("game.pad_build")
local pad_cycle = require("game.pad_cycle")
local pad_direction = require("game.pad_direction")
local pad_jump = require("game.pad_jump")
local pad_panel = require("game.pad_panel")
local pad_select = require("game.pad_select")
local pad_slipway = require("game.pad_slipway")
local pad_target = require("game.pad_target")
local planet_module = require("game.planet")
local probe = require("game.probe")
local scroll = require("game.scroll")
local slipway_module = require("game.slipway")
local ui_module = require("game.ui")
local cint = require("p8.cint")
local fixed = require("p8.fixed")
local input = require("p8.input")
local vram = require("p8.vram")

local M = {}

M.PAD_NAV_MAP_JUMP = 0
M.PAD_NAV_MAP_AIM = 1
M.PAD_NAV_MAP_SLIPWAY = 2

M.PAD_NAV_PLACEMENT_NONE = 0
M.PAD_NAV_PLACEMENT_PRESSED = 1
M.PAD_NAV_PLACEMENT_CARRYING = 2
M.PAD_NAV_PLACEMENT_SUSPENDED = 3

function M.pad_nav_panel_open()
  return pad_panel.pad_panel_open()
end

local function on_the_map(nav)
  assert(not (pad_build.pad_build_is_open(nav.build) and
      pad_cycle.pad_cycle_panel(nav.cycle) ~= pad_cycle.PAD_CYCLE_CLOSED),
    "pad_nav: the build bar and a cycle panel are never open together")
  return not pad_build.pad_build_is_open(nav.build) and
    pad_cycle.pad_cycle_panel(nav.cycle) == pad_cycle.PAD_CYCLE_CLOSED
end

local function cycle_open(nav)
  return pad_cycle.pad_cycle_panel(nav.cycle) ~= pad_cycle.PAD_CYCLE_CLOSED
end

function M.pad_nav_last_acted(nav)
  if probe.main_economy_typed() ~= nil then
    return pad_back.pad_back_target(nav.back, nil)
  end
  return nil
end

function M.pad_nav_select_held(nav)
  return nav.select.held_frames
end

function M.pad_nav_year_chain_open(nav)
  return nav.select.chain_frames > 0
end

local function game_unseen(nav)
  local economy = probe.main_economy_typed()
  return economy ~= nil and entity_id.entity_get(nav.game) ~= economy
end

local function map_mode_now(nav)
  if not game_unseen(nav) then
    return nav.map_mode
  end
  return pad_jump.pad_jump_any_candidate(entity_registry.entities_by_tag(entity_class.TAG_NODE)) and
    M.PAD_NAV_MAP_JUMP or M.PAD_NAV_MAP_AIM
end

function M.pad_nav_aim_mode(nav)
  return probe.main_economy_typed() ~= nil and on_the_map(nav) and not M.pad_nav_panel_open() and
    map_mode_now(nav) == M.PAD_NAV_MAP_AIM
end

local function live(e)
  if e ~= nil and not e.is_dead then
    return e
  end
  return nil
end

function M.pad_nav_jump_mode(nav)
  return probe.main_economy_typed() ~= nil and on_the_map(nav) and not M.pad_nav_panel_open() and
    map_mode_now(nav) == M.PAD_NAV_MAP_JUMP
end

function M.pad_nav_hovered(nav)
  if nav.map_mode == M.PAD_NAV_MAP_JUMP and not game_unseen(nav) then
    return pad_jump.pad_jump_hovered(nav.jump)
  end
  return nil
end

function M.pad_nav_slipway_start(nav)
  if map_mode_now(nav) == M.PAD_NAV_MAP_SLIPWAY then
    return live(entity_id.entity_get(nav.slipway.start))
  end
  return nil
end

function M.pad_nav_slipway_target(nav)
  local target = nil
  if map_mode_now(nav) == M.PAD_NAV_MAP_SLIPWAY then
    target = entity_id.entity_get(nav.slipway.target)
  end
  if pad_target.pad_target_is_candidate(target) then
    return target
  end
  return nil
end

function M.pad_nav_build_bar_open(nav)
  return probe.main_economy_typed() ~= nil and pad_build.pad_build_is_open(nav.build)
end

function M.pad_nav_tech_open(nav)
  return probe.main_economy_typed() ~= nil and
    pad_cycle.pad_cycle_panel(nav.cycle) == pad_cycle.PAD_CYCLE_TECH
end

function M.pad_nav_score_open(nav)
  return probe.main_economy_typed() ~= nil and
    pad_cycle.pad_cycle_panel(nav.cycle) == pad_cycle.PAD_CYCLE_SCORE
end

function M.pad_nav_build_icon(nav)
  if M.pad_nav_build_bar_open(nav) then
    return pad_build.pad_build_icon(nav.build)
  end
  return nil
end

local function placing_structure(e)
  e = live(e)
  if e ~= nil and planet_module.planet_is_family(e) and e.state == entity_class.ENTITY_STATE_PLACING then
    return e
  end
  return nil
end

local function held_structure()
  if ui_module.main_ui() ~= nil then
    return placing_structure(ui_module.ui_left_held(ui_module.main_ui()))
  end
  return nil
end

function M.pad_nav_placing(nav)
  if nav.placement == M.PAD_NAV_PLACEMENT_CARRYING then
    return held_structure()
  end
  return nil
end

function M.pad_nav_is_item(e)
  return pad_panel.pad_panel_is_item(e)
end

function M.pad_nav_first_item()
  return pad_panel.pad_panel_first_item()
end

function M.pad_nav_neighbor(item, direction)
  return pad_panel.pad_panel_neighbor(item, direction)
end

local HELD_FRAMES_CAP = 41

local function reset_aim_mode(nav)
  for i = 1, pad_jump.PAD_JUMP_DIRECTIONS do
    nav.held_frames[i] = 0
  end
  nav.probe_or_fold_engaged = false
end

local function pan_speed(held_frames)
  if held_frames == 0 then
    return 0
  end
  if held_frames <= 10 then
    return 1
  end
  return (held_frames <= 40) and 2 or 4
end

local pan_action = {
  bindings_module.BINDINGS_ACTION_PAN_UP, bindings_module.BINDINGS_ACTION_PAN_DOWN,
  bindings_module.BINDINGS_ACTION_PAN_LEFT, bindings_module.BINDINGS_ACTION_PAN_RIGHT,
}

local function aim_pan(nav, mode, pad_buttons)
  assert(bindings_module.bindings_mode_has_action(mode, bindings_module.BINDINGS_ACTION_PAN_UP),
    "aim_pan: a mode with pan_* actions (aim or placement)")
  local speed = {}
  for i = 1, pad_jump.PAD_JUMP_DIRECTIONS do
    if bit.band(pad_buttons, bindings_module.bindings_mask(nav.bindings, mode, pan_action[i])) == 0 then
      nav.held_frames[i] = 0
    elseif nav.held_frames[i] < HELD_FRAMES_CAP then
      nav.held_frames[i] = nav.held_frames[i] + 1
    end
    speed[i] = pan_speed(nav.held_frames[i])
  end
  assert(pad_jump.pad_jump_direction(1) == pad_direction.PAD_DIRECTION_UP and
    pad_jump.pad_jump_direction(2) == pad_direction.PAD_DIRECTION_DOWN and
    pad_jump.pad_jump_direction(3) == pad_direction.PAD_DIRECTION_LEFT and
    pad_jump.pad_jump_direction(4) == pad_direction.PAD_DIRECTION_RIGHT,
    "aim_pan: speed[] and pan_action[] are indexed Up, Down, Left, Right")
  local camera = scroll.main_camera_typed()
  if camera ~= nil then
    scroll.camera_pan(camera, speed[4] - speed[3], speed[2] - speed[1])
  end
end

local function reset_map_mode(nav)
  nav.map_mode = M.PAD_NAV_MAP_JUMP
  pad_jump.pad_jump_hover(nav.jump, nil)
  pad_jump.pad_jump_repeat_clear(nav.jump)
  nav.marker_frames = 0
  nav.placement = M.PAD_NAV_PLACEMENT_NONE
  nav.suspended_carry = nil
  pad_slipway.pad_slipway_init(nav.slipway)
  pad_select.pad_select_init(nav.select)
end

local function close_panels(nav)
  pad_build.pad_build_close(nav.build)
  pad_cycle.pad_cycle_set(nav.cycle, pad_cycle.PAD_CYCLE_CLOSED)
end

local function copy_bindings(nav, bindings)
  if nav.bindings == nil then
    nav.bindings = { masks = {} }
  end
  local masks = nav.bindings.masks
  for mode = 1, bindings_module.BINDINGS_MODE_COUNT do
    if masks[mode] == nil then
      masks[mode] = {}
    end
    for action = 1, bindings_module.BINDINGS_ACTION_COUNT do
      masks[mode][action] = bindings.masks[mode][action]
    end
  end
end

local SUB_TABLES = { "build", "cycle", "jump", "slipway", "back", "select", "held_frames" }

local function create_sub_tables(nav)
  for i = 1, #SUB_TABLES do
    if nav[SUB_TABLES[i]] == nil then
      nav[SUB_TABLES[i]] = {}
    end
  end
end

-- focus, suspended_carry and game are entity ids, nil for none; held_frames is indexed by
-- pad_jump_direction's index. The sub-tables are created only when absent and bindings.masks
-- is filled in place, so a re-init keeps their identity.
function M.pad_nav_init(nav, bindings)
  assert(bindings ~= nil, "pad_nav_init: the caller hands its compiled table, never NULL")
  create_sub_tables(nav)
  copy_bindings(nav, bindings)
  nav.focus = nil
  pad_build.pad_build_init(nav.build)
  pad_cycle.pad_cycle_init(nav.cycle)
  nav.game = nil
  pad_jump.pad_jump_init(nav.jump)
  pad_back.pad_back_init(nav.back)
  reset_map_mode(nav)
  nav.release_pending = false
  nav.pause_due = false
  nav.pause_choose_row_armed = false
  nav.menu_choose_row_armed = false
  nav.previous_pad = 0
  nav.chord_held = false
  nav.left_was_down = false
  nav.cursor_x = 0
  nav.cursor_y = 0
  reset_aim_mode(nav)
end

function M.pad_nav_bindings(nav)
  return nav.bindings
end

local function action_pressed(nav, pressed, mode, action)
  return bit.band(pressed, bindings_module.bindings_mask(nav.bindings, mode, action)) ~= 0
end

local function action_held(nav, pad_buttons, mode, action)
  return bit.band(pad_buttons, bindings_module.bindings_mask(nav.bindings, mode, action)) ~= 0
end

function M.pad_nav_focus(nav)
  local focus = entity_id.entity_get(nav.focus)
  if M.pad_nav_is_item(focus) then
    return focus
  end
  return nil
end

local function planet_menu_open(nav)
  return on_the_map(nav) and pad_panel.pad_panel_box_open() and not menu.pause_menu_shown()
end

local function armed_panel_open(nav)
  return (on_the_map(nav) or cycle_open(nav)) and M.pad_nav_panel_open() and
    not menu.pause_menu_shown()
end

local function panel_mode(nav)
  if menu.pause_menu_shown() then
    return bindings_module.BINDINGS_MODE_PAUSE
  end
  if probe.main_economy_typed() == nil then
    return bindings_module.BINDINGS_MODE_TITLE
  end
  return cycle_open(nav) and bindings_module.BINDINGS_MODE_PANELS or bindings_module.BINDINGS_MODE_MENU
end

local move_action = {
  bindings_module.BINDINGS_ACTION_MOVE_UP, bindings_module.BINDINGS_ACTION_MOVE_DOWN,
  bindings_module.BINDINGS_ACTION_MOVE_LEFT, bindings_module.BINDINGS_ACTION_MOVE_RIGHT,
}

local function first_pressed_move(nav, pressed, mode)
  assert(bindings_module.bindings_mode_has_action(mode, bindings_module.BINDINGS_ACTION_MOVE_UP),
    "first_pressed_move: a mode with move_* actions (title, menu, pause or panels)")
  for i = 1, pad_jump.PAD_JUMP_DIRECTIONS do
    if action_pressed(nav, pressed, mode, move_action[i]) then
      return pad_jump.pad_jump_direction(i)
    end
  end
  return pad_direction.PAD_DIRECTION_NONE
end

local function panel_step(nav, pad_buttons, pressed, played)
  local mode = panel_mode(nav)
  local direction = first_pressed_move(nav, pressed, mode)
  local choose_held = action_held(nav, pad_buttons, mode, bindings_module.BINDINGS_ACTION_CHOOSE_ROW)
  if menu.pause_menu_shown() then
    nav.pause_choose_row_armed = nav.pause_choose_row_armed or
      action_pressed(nav, pressed, bindings_module.BINDINGS_MODE_PAUSE, bindings_module.BINDINGS_ACTION_CHOOSE_ROW)
    choose_held = choose_held and nav.pause_choose_row_armed
  elseif armed_panel_open(nav) then
    nav.menu_choose_row_armed = nav.menu_choose_row_armed or
      action_pressed(nav, pressed, mode, bindings_module.BINDINGS_ACTION_CHOOSE_ROW)
    choose_held = choose_held and nav.menu_choose_row_armed
  end
  local at
  at, nav.cursor_x, nav.cursor_y, played.mouse_buttons = pad_panel.pad_panel_step(entity_id.entity_get(nav.focus),
    choose_held, direction, nav.cursor_x, nav.cursor_y, played.mouse_buttons)
  nav.focus = entity_id.entity_id_link(at)
end

-- Keep in step with pad_panel's box_at for layers under 10: world point minus the camera, floored.
local function screen_point(e, x, y)
  local at = entity_class.entity_pos(e)
  local camera = scroll.main_camera_typed()
  if camera ~= nil then
    at = vec.vec_sub(at, entity_class.entity_pos(camera))
  end
  x = fixed.p8_fixed_flr_int(at.x)
  y = fixed.p8_fixed_flr_int(at.y)
  return x, y
end

local function entity_under(x, y, accept)
  return pad_panel.pad_panel_entity_under(x, y, pad_panel.PAD_PANEL_ANY_LAYER, accept)
end

local function presses_off_background(e)
  return e.class.left_press ~= nil and e.class ~= background.background_class()
end

local function click(nav, played)
  played.mouse_buttons = input.P8_MOUSE_LEFT
  nav.release_pending = true
end

local function enter_aim_mode(nav)
  nav.map_mode = M.PAD_NAV_MAP_AIM
  nav.cursor_x = cint.div(vram.P8_SCREEN_WIDTH, 2)
  nav.cursor_y = cint.div(vram.P8_SCREEN_HEIGHT, 2)
  pad_jump.pad_jump_repeat_clear(nav.jump)
end

local function pressed_before(planet, x, y)
  return pad_panel.pad_panel_entity_under(x, y, fixed.p8_fixed_flr_int(planet.layer),
    pad_panel.pad_panel_is_candidate) ~= nil
end

local function held_slipway()
  local held = nil
  if ui_module.main_ui() ~= nil then
    held = live(ui_module.ui_left_held(ui_module.main_ui()))
  end
  if held ~= nil and held.class == slipway_module.slipway_class() then
    return held
  end
  return nil
end

local function drop_held_slipway()
  local held = held_slipway()
  if held ~= nil then
    held.class.drop(held, nil)
    ui_module.ui_set_left_held(ui_module.main_ui(), nil)
  end
end

local function centre_world_point(x, y)
  local camera = scroll.main_camera_typed()
  local at
  if camera ~= nil then
    at = scroll.camera_pos_before_shake(camera)
  else
    at = vec.vec_make(0, 0)
  end
  x = fixed.p8_fixed_flr_int(at.x) + cint.div(vram.P8_SCREEN_WIDTH, 2)
  y = fixed.p8_fixed_flr_int(at.y) + cint.div(vram.P8_SCREEN_HEIGHT, 2)
  return x, y
end

local edge_scroll

local function live_candidates()
  return pad_jump.pad_jump_candidates(entity_registry.entities_by_tag(entity_class.TAG_NODE))
end

local function back_to_jump_mode(nav, e)
  nav.map_mode = M.PAD_NAV_MAP_JUMP
  if pad_target.pad_target_is_candidate(e) then
    pad_jump.pad_jump_hover(nav.jump, e)
  else
    pad_jump.pad_jump_hover(nav.jump, nil)
  end
  pad_slipway.pad_slipway_init(nav.slipway)
end

-- Seeded, not pad_jump_hover: jump mode must never reseed on the first colony afterwards.
local function leave_aim_mode(nav)
  if not pad_jump.pad_jump_any_candidate(entity_registry.entities_by_tag(entity_class.TAG_NODE)) then
    return
  end
  local candidates = live_candidates()
  local x, y = centre_world_point(0, 0)
  local nearest = pad_jump.pad_jump_nearest(x, y, candidates)
  nav.map_mode = M.PAD_NAV_MAP_JUMP
  pad_jump.pad_jump_hover_seeded(nav.jump, nearest)
  pad_jump.pad_jump_repeat_clear(nav.jump)
end

local function cancel_slipway(nav)
  if nav.map_mode ~= M.PAD_NAV_MAP_SLIPWAY then
    return false
  end
  local held = held_slipway() ~= nil
  drop_held_slipway()
  back_to_jump_mode(nav, M.pad_nav_slipway_start(nav))
  return held
end

local function select_tap(nav, current, shown)
  local candidates = live_candidates()
  local next = pad_select.pad_select_next(candidates, current)
  local jumps = next ~= nil and next ~= current
  if jumps then
    back_to_jump_mode(nav, next)
    pad_jump.pad_jump_hover_seeded(nav.jump, next)
    pad_jump.pad_jump_repeat_clear(nav.jump)
    edge_scroll(next)
    shown = next
    nav.cursor_x, nav.cursor_y = screen_point(next, nav.cursor_x, nav.cursor_y)
  end
  return jumps, shown
end

-- On a click's release frame probe_or_fold starts nothing: finish_step forces its press up.
local function aim_mode_step(nav, pad_buttons, pressed, back_returns, release, still, select_tapped, played, shown)
  nav.focus = nil
  nav.cursor_x = cint.div(vram.P8_SCREEN_WIDTH, 2)
  nav.cursor_y = cint.div(vram.P8_SCREEN_HEIGHT, 2)
  if not still then
    aim_pan(nav, bindings_module.BINDINGS_MODE_AIM, pad_buttons)
  end
  if select_tapped and not nav.probe_or_fold_engaged then
    local jumps
    jumps, shown = select_tap(nav, nil, shown)
    if jumps then
      return shown
    end
  end
  if action_pressed(nav, pressed, bindings_module.BINDINGS_MODE_AIM, bindings_module.BINDINGS_ACTION_LEAVE_AIM) and
    not nav.probe_or_fold_engaged then
    leave_aim_mode(nav)
    return shown
  end
  if not action_held(nav, pad_buttons, bindings_module.BINDINGS_MODE_AIM,
      bindings_module.BINDINGS_ACTION_PROBE_OR_FOLD) then
    nav.probe_or_fold_engaged = false
  elseif action_pressed(nav, pressed, bindings_module.BINDINGS_MODE_AIM,
      bindings_module.BINDINGS_ACTION_PROBE_OR_FOLD) and not release then
    nav.probe_or_fold_engaged = entity_under(nav.cursor_x, nav.cursor_y, presses_off_background) == nil
  end
  if nav.probe_or_fold_engaged then
    played.mouse_buttons = input.P8_MOUSE_LEFT
  elseif back_returns then
    leave_aim_mode(nav)
  end
  return shown
end

local function reset_on_new_game(nav)
  if not game_unseen(nav) then
    return
  end
  local start = map_mode_now(nav)
  pad_jump.pad_jump_init(nav.jump)
  pad_back.pad_back_init(nav.back)
  nav.game = entity_id.entity_id_link(probe.main_economy_typed())
  reset_map_mode(nav)
  if start == M.PAD_NAV_MAP_AIM then
    enter_aim_mode(nav)
  end
end

-- Never pause, which only pause_step closes: cleared on a release frame, the resumed carry would press the map.
local function back_closes_the_box()
  if pad_panel.pad_panel_box_open() and not menu.pause_menu_shown() then
    ui_module.ui_clear_ephemerals(ui_module.main_ui())
  end
end

local function show_marker(nav, shown)
  local ui = ui_module.main_ui()
  if shown == nil then
    nav.marker_frames = 0
  end
  if ui ~= nil then
    ui_module.ui_set_pad_selection(ui, shown, nav.marker_frames)
  end
  if shown ~= nil then
    nav.marker_frames = cint.mod(nav.marker_frames + 1, ui_module.UI_PAD_BLINK_PERIOD)
  end
end

local function own_panel_up(nav)
  if pad_cycle.pad_cycle_panel(nav.cycle) == pad_cycle.PAD_CYCLE_TECH then
    return pad_panel.pad_panel_box_open() and not menu.pause_menu_shown()
  end
  if pad_cycle.pad_cycle_panel(nav.cycle) == pad_cycle.PAD_CYCLE_SCORE then
    return M.pad_nav_panel_open() and not pad_panel.pad_panel_box_open() and
      pad_panel.pad_panel_first_item() == nil
  end
  return false
end

local function foreign_panel_open(nav)
  return M.pad_nav_panel_open() and not own_panel_up(nav)
end

-- A slipway released on this frame is judged on the next: the shoulders and pause wait for it.
local function map_action_due(nav)
  return nav.map_mode == M.PAD_NAV_MAP_SLIPWAY and nav.slipway.phase == pad_slipway.PAD_SLIPWAY_RELEASED
end

local function shoulders_can_switch(nav, release)
  if (foreign_panel_open(nav) and not planet_menu_open(nav)) or release or
    pad_cycle.pad_cycle_landing_due(nav.cycle) then
    return false
  end
  return not on_the_map(nav) or not map_action_due(nav)
end

local CANCELLED_NOTHING = 0
local CANCELLED_STRUCTURE = 1
local CANCELLED_SLIPWAY = 2

-- The caller owes a cancelled structure cancel_point, button up; a cancelled slipway is already dropped.
local function unwind_sub_modes(nav)
  if not on_the_map(nav) then
    return CANCELLED_NOTHING
  end
  if planet_menu_open(nav) then
    ui_module.ui_clear_ephemerals(ui_module.main_ui())
  end
  if M.pad_nav_placing(nav) ~= nil then
    return CANCELLED_STRUCTURE
  end
  return cancel_slipway(nav) and CANCELLED_SLIPWAY or CANCELLED_NOTHING
end

local function leave_panel(nav, played)
  if not own_panel_up(nav) then
    return
  end
  if pad_cycle.pad_cycle_panel(nav.cycle) == pad_cycle.PAD_CYCLE_TECH then
    ui_module.ui_clear_ephemerals(ui_module.main_ui())
  else
    click(nav, played)
  end
end

local function map_binding_mode(nav)
  if M.pad_nav_placing(nav) ~= nil then
    return bindings_module.BINDINGS_MODE_PLACEMENT
  end
  local mode = map_mode_now(nav)
  if mode == M.PAD_NAV_MAP_AIM then
    return bindings_module.BINDINGS_MODE_AIM
  elseif mode == M.PAD_NAV_MAP_SLIPWAY then
    return bindings_module.BINDINGS_MODE_SLIPWAY
  end
  return bindings_module.BINDINGS_MODE_JUMP
end

-- The mode the dispatch would run from the nav state at the call: decide it before anything is unwound.
local function nav_mode(nav)
  if on_the_map(nav) and M.pad_nav_placing(nav) ~= nil then
    return bindings_module.BINDINGS_MODE_PLACEMENT
  end
  if M.pad_nav_panel_open() then
    return panel_mode(nav)
  end
  if cycle_open(nav) then
    return bindings_module.BINDINGS_MODE_PANELS
  end
  if pad_build.pad_build_is_open(nav.build) then
    return bindings_module.BINDINGS_MODE_BUILD_BAR
  end
  return map_binding_mode(nav)
end

local function back_mode(nav)
  if pad_panel.pad_panel_box_open() and not menu.pause_menu_shown() then
    return cycle_open(nav) and bindings_module.BINDINGS_MODE_PANELS or bindings_module.BINDINGS_MODE_MENU
  end
  return map_binding_mode(nav)
end

local function back_action(mode)
  if mode == bindings_module.BINDINGS_MODE_JUMP or mode == bindings_module.BINDINGS_MODE_AIM then
    return bindings_module.BINDINGS_ACTION_BACK
  elseif mode == bindings_module.BINDINGS_MODE_SLIPWAY then
    return bindings_module.BINDINGS_ACTION_CANCEL_SLIPWAY
  elseif mode == bindings_module.BINDINGS_MODE_PLACEMENT then
    return bindings_module.BINDINGS_ACTION_CANCEL_PLACEMENT
  elseif mode == bindings_module.BINDINGS_MODE_MENU then
    return bindings_module.BINDINGS_ACTION_CLOSE_MENU
  elseif mode == bindings_module.BINDINGS_MODE_PANELS then
    return bindings_module.BINDINGS_ACTION_CLOSE_PANEL
  end
  assert(false, "back_action: back_mode returns only a map, menu or panels mode")
  return bindings_module.BINDINGS_ACTION_BACK
end

local function tech_shoulder_action(mode)
  return (mode == bindings_module.BINDINGS_MODE_PANELS) and bindings_module.BINDINGS_ACTION_NEXT_PANEL or
    bindings_module.BINDINGS_ACTION_OPEN_TECH_PANEL
end

local function one_shoulder(nav, pressed, mode)
  return action_pressed(nav, pressed, mode, tech_shoulder_action(mode)) ~=
    action_pressed(nav, pressed, mode, bindings_module.BINDINGS_ACTION_OPEN_BUILD_BAR)
end

local function shoulder_switch(nav, pressed, mode, played)
  leave_panel(nav, played)
  if action_pressed(nav, pressed, mode, tech_shoulder_action(mode)) then
    pad_build.pad_build_close(nav.build)
    pad_cycle.pad_cycle_set(nav.cycle, pad_cycle.pad_cycle_next(pad_cycle.pad_cycle_panel(nav.cycle)))
  elseif pad_build.pad_build_is_open(nav.build) then
    pad_build.pad_build_close(nav.build)
  else
    pad_cycle.pad_cycle_set(nav.cycle, pad_cycle.PAD_CYCLE_CLOSED)
    pad_build.pad_build_open(nav.build)
  end
end

-- Off screen, so a press finds no left_press under the cursor.
local PARKED_X = -16
local PARKED_Y = -16

local YEAR_SKIP_BUTTON = 1
local PAUSE_BUTTON = 4

local function cycle_button(nav)
  local index = pad_cycle.pad_cycle_hud_button(pad_cycle.pad_cycle_panel(nav.cycle))
  if index ~= 0 then
    return menu.galaxy_menu_find_button(index)
  end
  return nil
end

local function build_step(nav, pressed, release, played)
  local icon = pad_build.pad_build_focus(nav.build)
  if icon ~= nil and action_pressed(nav, pressed, bindings_module.BINDINGS_MODE_BUILD_BAR,
      bindings_module.BINDINGS_ACTION_TAKE_STRUCTURE) and not release then
    played.mouse_buttons = input.P8_MOUSE_LEFT
    nav.placement = M.PAD_NAV_PLACEMENT_PRESSED
  end
  -- The bar has no up or down: it borrows the panels' pair, which ranks before left and right.
  if action_pressed(nav, pressed, bindings_module.BINDINGS_MODE_PANELS, bindings_module.BINDINGS_ACTION_MOVE_UP) or
    action_pressed(nav, pressed, bindings_module.BINDINGS_MODE_PANELS, bindings_module.BINDINGS_ACTION_MOVE_DOWN) then
    return
  end
  if action_pressed(nav, pressed, bindings_module.BINDINGS_MODE_BUILD_BAR,
      bindings_module.BINDINGS_ACTION_PICK_LEFT) then
    pad_build.pad_build_pick(nav.build, -1)
  elseif action_pressed(nav, pressed, bindings_module.BINDINGS_MODE_BUILD_BAR,
      bindings_module.BINDINGS_ACTION_PICK_RIGHT) then
    pad_build.pad_build_pick(nav.build, 1)
  end
end

local function place_cursor(nav)
  local button = cycle_button(nav)
  local icon = nil
  if pad_build.pad_build_is_open(nav.build) then
    icon = pad_build.pad_build_focus(nav.build)
  end
  if on_the_map(nav) then
    local hovered = M.pad_nav_hovered(nav)
    if hovered ~= nil then
      nav.cursor_x, nav.cursor_y = screen_point(hovered, nav.cursor_x, nav.cursor_y)
    else
      nav.cursor_x = cint.div(vram.P8_SCREEN_WIDTH, 2)
      nav.cursor_y = cint.div(vram.P8_SCREEN_HEIGHT, 2)
    end
  elseif button ~= nil then
    nav.cursor_x, nav.cursor_y = pad_panel.pad_panel_focus_point(button, nav.cursor_x, nav.cursor_y)
  elseif icon ~= nil then
    nav.cursor_x, nav.cursor_y = pad_panel.pad_panel_focus_point(icon, nav.cursor_x, nav.cursor_y)
  else
    nav.cursor_x = PARKED_X
    nav.cursor_y = PARKED_Y
  end
end

local function bar_or_cycle_step(nav, pad_buttons, pressed, release, played)
  if M.pad_nav_panel_open() then
    panel_step(nav, pad_buttons, pressed, played)
    return
  end
  nav.focus = nil
  if pad_build.pad_build_is_open(nav.build) then
    build_step(nav, pressed, release, played)
  end
  place_cursor(nav)
end

local function begin_placement(nav)
  if nav.placement == M.PAD_NAV_PLACEMENT_CARRYING and M.pad_nav_placing(nav) == nil then
    nav.placement = M.PAD_NAV_PLACEMENT_NONE
  end
  if nav.placement ~= M.PAD_NAV_PLACEMENT_PRESSED then
    return false
  end
  if held_structure() == nil then
    nav.placement = M.PAD_NAV_PLACEMENT_NONE
    return false
  end
  close_panels(nav)
  nav.placement = M.PAD_NAV_PLACEMENT_CARRYING
  if nav.map_mode == M.PAD_NAV_MAP_JUMP then
    enter_aim_mode(nav)
  end
  return true
end

local function cancel_point(nav)
  local icon = pad_build.pad_build_icon(nav.build)
  if icon ~= nil then
    nav.cursor_x, nav.cursor_y = pad_panel.pad_panel_focus_point(icon, nav.cursor_x, nav.cursor_y)
  else
    nav.cursor_x = PARKED_X
    nav.cursor_y = PARKED_Y
  end
  local at = vec.vec_make(fixed.p8_fixed_from_int(nav.cursor_x), fixed.p8_fixed_from_int(nav.cursor_y))
  assert(ui_module.ui_panel_box() == nil or not geom.box_contains(ui_module.ui_panel_box(), at),
    "cancel_point: inside the ui's panel box the drop would be judged on the map, not cancelled")
end

local function placement_step(nav, pad_buttons, pressed, played)
  nav.focus = nil
  nav.probe_or_fold_engaged = false
  if action_pressed(nav, pressed, bindings_module.BINDINGS_MODE_PLACEMENT,
      bindings_module.BINDINGS_ACTION_CANCEL_PLACEMENT) or M.pad_nav_panel_open() then
    cancel_point(nav)
    return
  end
  nav.cursor_x = cint.div(vram.P8_SCREEN_WIDTH, 2)
  nav.cursor_y = cint.div(vram.P8_SCREEN_HEIGHT, 2)
  aim_pan(nav, bindings_module.BINDINGS_MODE_PLACEMENT, pad_buttons)
  -- place_structure drops by letting the held left button go for one frame: the ui's drop runs the cart's rules.
  if action_pressed(nav, pressed, bindings_module.BINDINGS_MODE_PLACEMENT,
      bindings_module.BINDINGS_ACTION_PLACE_STRUCTURE) then
    return
  end
  played.mouse_buttons = input.P8_MOUSE_LEFT
end

local function seed_hovered(nav)
  if pad_jump.pad_jump_hovered(nav.jump) ~= nil then
    return
  end
  local candidates = live_candidates()
  local x, y = centre_world_point(0, 0)
  pad_jump.pad_jump_reseed(nav.jump, candidates, x, y)
end

local function rest_screen_point(camera, hovered, x, y)
  local from = scroll.camera_pos_before_shake(camera)
  x, y = pad_jump.pad_jump_world_point(hovered, x, y)
  x = x - fixed.p8_fixed_flr_int(from.x)
  y = y - fixed.p8_fixed_flr_int(from.y)
  return x, y
end

edge_scroll = function(hovered)
  local camera = scroll.main_camera_typed()
  if camera == nil then
    return
  end
  local x, y = rest_screen_point(camera, hovered, 0, 0)
  local dx, dy = pad_jump.pad_jump_scroll(x, y, 0, 0)
  if dx ~= 0 or dy ~= 0 then
    scroll.camera_pan(camera, dx, dy)
  end
end

local function pressable(e)
  local camera = scroll.main_camera_typed()
  local x, y = 0, 0
  if camera == nil then
    return false
  end
  x, y = rest_screen_point(camera, e, x, y)
  if not pad_jump.pad_jump_inside(x, y) then
    return false
  end
  x, y = screen_point(e, x, y)
  return not pressed_before(e, x, y)
end

local function click_hovered(nav, hovered, played)
  if not pressable(hovered) then
    return false
  end
  click(nav, played)
  nav.menu_choose_row_armed = false
  return true
end

local function press_slipway_start(nav, start, played)
  nav.cursor_x, nav.cursor_y = screen_point(start, nav.cursor_x, nav.cursor_y)
  played.mouse_buttons = input.P8_MOUSE_LEFT
end

local function act_on_hovered_starts_slipway(nav, hovered, played)
  if not pad_slipway.pad_slipway_can_start(hovered) or not pressable(hovered) or nav.left_was_down then
    return false
  end
  press_slipway_start(nav, hovered, played)
  nav.map_mode = M.PAD_NAV_MAP_SLIPWAY
  pad_slipway.pad_slipway_init(nav.slipway)
  nav.slipway.start = entity_id.entity_id_link(hovered)
  nav.slipway.phase = pad_slipway.PAD_SLIPWAY_PRESSED
  return true
end

local function jump_from(nav, hovered, direction)
  local candidates = live_candidates()
  local target = pad_target.pad_target_pick(hovered, direction, candidates)
  if target ~= nil then
    pad_jump.pad_jump_hover(nav.jump, target)
  end
end

local function jump_step(nav, pad_buttons, pressed, release, back_returns, select_tapped, played, shown)
  reset_aim_mode(nav)
  nav.focus = nil
  seed_hovered(nav)
  if action_pressed(nav, pressed, bindings_module.BINDINGS_MODE_JUMP, bindings_module.BINDINGS_ACTION_ENTER_AIM) then
    enter_aim_mode(nav)
    return shown
  end
  -- Before the click check: the hold-repeat count keeps running through a click's frames.
  local direction =
    pad_jump.pad_jump_repeat_step(nav.jump, nav.bindings, bindings_module.BINDINGS_MODE_JUMP, pad_buttons, pressed)
  local hovered = pad_jump.pad_jump_hovered(nav.jump)
  local clicking = release or nav.release_pending
  local back = nil
  if back_returns and not clicking then
    back = pad_back.pad_back_target(nav.back, hovered)
  end
  if back ~= nil then
    pad_jump.pad_jump_hover(nav.jump, back)
    edge_scroll(back)
    shown = back
    nav.cursor_x, nav.cursor_y = screen_point(back, nav.cursor_x, nav.cursor_y)
    return shown
  end
  if hovered == nil then
    nav.cursor_x = cint.div(vram.P8_SCREEN_WIDTH, 2)
    nav.cursor_y = cint.div(vram.P8_SCREEN_HEIGHT, 2)
    return shown
  end
  if not clicking and action_pressed(nav, pressed, bindings_module.BINDINGS_MODE_JUMP,
      bindings_module.BINDINGS_ACTION_OPEN_MENU) then
    clicking = click_hovered(nav, hovered, played)
  end
  if not clicking and action_pressed(nav, pressed, bindings_module.BINDINGS_MODE_JUMP,
      bindings_module.BINDINGS_ACTION_ACT_ON_HOVERED) and hovered.state == entity_class.ENTITY_STATE_KNOWN then
    clicking = click_hovered(nav, hovered, played)
    if clicking then
      pad_back.pad_back_record(nav.back, hovered)
    end
  end
  if not clicking and action_pressed(nav, pressed, bindings_module.BINDINGS_MODE_JUMP,
      bindings_module.BINDINGS_ACTION_ACT_ON_HOVERED) and act_on_hovered_starts_slipway(nav, hovered, played) then
    pad_back.pad_back_record(nav.back, hovered)
    return shown
  end
  if not clicking and select_tapped then
    local jumps
    jumps, shown = select_tap(nav, hovered, shown)
    if jumps then
      return shown
    end
  end
  if direction ~= pad_direction.PAD_DIRECTION_NONE and not clicking then
    jump_from(nav, hovered, direction)
    hovered = pad_jump.pad_jump_hovered(nav.jump)
  end
  if not clicking then
    edge_scroll(hovered)
    shown = hovered
  end
  nav.cursor_x, nav.cursor_y = screen_point(hovered, nav.cursor_x, nav.cursor_y)
  return shown
end

-- Not pressable: after the outcome the start's screen point may lie off screen.
local function press_start_again(nav, start, played)
  if not pad_slipway.pad_slipway_can_start(start) then
    return false
  end
  local x, y = screen_point(start, 0, 0)
  if pressed_before(start, x, y) then
    return false
  end
  press_slipway_start(nav, start, played)
  return true
end

local function slipway_step(nav, pad_buttons, pressed, back_returns, release, played, shown)
  reset_aim_mode(nav)
  nav.focus = nil
  local start = M.pad_nav_slipway_start(nav)
  if start == nil or not planet_module.planet_is_family(start) then
    drop_held_slipway()
    back_to_jump_mode(nav, M.pad_nav_slipway_target(nav))
    return shown
  end
  if nav.slipway.phase == pad_slipway.PAD_SLIPWAY_RELEASED then
    local outcome =
      pad_slipway.pad_slipway_outcome(start, entity_id.entity_get(nav.slipway.target), nav.slipway.linked_before)
    if outcome == pad_slipway.PAD_SLIPWAY_BUILT then
      pad_back.pad_back_record(nav.back, entity_id.entity_get(nav.slipway.target))
    end
    pad_slipway.pad_slipway_rebase(nav.slipway, outcome)
    start = live(entity_id.entity_get(nav.slipway.start))
    if not press_start_again(nav, start, played) then
      back_to_jump_mode(nav, start)
    end
    return shown
  end
  if nav.slipway.phase == pad_slipway.PAD_SLIPWAY_PRESSED then
    if held_slipway() == nil then
      back_to_jump_mode(nav, start)
      return shown
    end
    nav.slipway.phase = pad_slipway.PAD_SLIPWAY_HELD
  end
  if back_returns then
    drop_held_slipway()
    back_to_jump_mode(nav, start)
    nav.cursor_x, nav.cursor_y = screen_point(start, nav.cursor_x, nav.cursor_y)
    return shown
  end
  local direction =
    pad_jump.pad_jump_repeat_step(nav.jump, nav.bindings, bindings_module.BINDINGS_MODE_SLIPWAY, pad_buttons, pressed)
  local target = M.pad_nav_slipway_target(nav)
  nav.slipway.target = entity_id.entity_id_link(target)
  local build_on_target = action_pressed(nav, pressed, bindings_module.BINDINGS_MODE_SLIPWAY,
    bindings_module.BINDINGS_ACTION_BUILD_ON_TARGET)
  if build_on_target and not release and target ~= nil and pressable(target) then
    pad_slipway.pad_slipway_release(nav.slipway, pad_slipway.pad_slipway_between(start, target) ~= nil)
    nav.cursor_x, nav.cursor_y = screen_point(target, nav.cursor_x, nav.cursor_y)
    return shown
  end
  if direction ~= pad_direction.PAD_DIRECTION_NONE and not build_on_target then
    local candidates = live_candidates()
    local targets = pad_slipway.pad_slipway_candidates(candidates, start)
    local origin = start
    if target ~= nil then
      origin = target
    end
    local picked = pad_target.pad_target_pick(origin, direction, targets)
    if picked ~= nil then
      target = picked
      nav.slipway.target = entity_id.entity_id_link(target)
    end
  end
  local aimed = start
  if target ~= nil then
    aimed = target
  end
  edge_scroll(aimed)
  nav.cursor_x, nav.cursor_y = screen_point(aimed, nav.cursor_x, nav.cursor_y)
  played.mouse_buttons = input.P8_MOUSE_LEFT
  shown = aimed
  return shown
end

local function map_step(nav, pad_buttons, pressed, release, back_returns, select, year_release, played, shown,
                        repeat_kept)
  if M.pad_nav_placing(nav) ~= nil then
    placement_step(nav, pad_buttons, pressed, played)
  elseif M.pad_nav_panel_open() then
    reset_aim_mode(nav)
    if action_pressed(nav, pressed, bindings_module.BINDINGS_MODE_MENU, bindings_module.BINDINGS_ACTION_OPEN_MENU) and
      not release and not nav.release_pending and planet_menu_open(nav) then
      ui_module.ui_clear_ephemerals(ui_module.main_ui())
    else
      panel_step(nav, pad_buttons, pressed, played)
    end
  elseif M.pad_nav_aim_mode(nav) then
    shown = aim_mode_step(nav, pad_buttons, pressed, back_returns, release, year_release,
      select == pad_select.PAD_SELECT_TAP, played, shown)
  elseif nav.map_mode == M.PAD_NAV_MAP_SLIPWAY then
    shown = slipway_step(nav, pad_buttons, pressed, back_returns, release, played, shown)
    repeat_kept = true
  else
    assert(M.pad_nav_jump_mode(nav),
      "map_step: after placement, panels, aim and slipway mode, only jump mode is left")
    shown = jump_step(nav, pad_buttons, pressed, release, back_returns, select == pad_select.PAD_SELECT_TAP,
      played, shown)
    repeat_kept = true
  end
  return shown, repeat_kept
end

local function switch_panels(nav, pressed, release, foreign_was_open, cancelled, played)
  cancelled = CANCELLED_NOTHING
  if probe.main_economy_typed() == nil then
    close_panels(nav)
    return false, cancelled
  end
  local mode = nav_mode(nav)
  if one_shoulder(nav, pressed, mode) and shoulders_can_switch(nav, release) then
    cancelled = unwind_sub_modes(nav)
    shoulder_switch(nav, pressed, mode, played)
    return true, cancelled
  end
  -- The bar's state, not nav_mode: a box shown over the bar would read menu's buttons.
  local bar = pad_build.pad_build_is_open(nav.build)
  local leaving
  if bar then
    leaving = action_pressed(nav, pressed, bindings_module.BINDINGS_MODE_BUILD_BAR,
      bindings_module.BINDINGS_ACTION_CLOSE_BAR)
  else
    leaving = action_pressed(nav, pressed, bindings_module.BINDINGS_MODE_PANELS,
      bindings_module.BINDINGS_ACTION_CLOSE_PANEL)
  end
  if not on_the_map(nav) and leaving and not foreign_was_open and not release then
    leave_panel(nav, played)
    close_panels(nav)
    return true, cancelled
  end
  return false, cancelled
end

local function pause_button_free(nav, release)
  if probe.main_economy_typed() == nil or M.pad_nav_panel_open() or release or map_action_due(nav) or
    nav.probe_or_fold_engaged or cycle_open(nav) then
    return nil
  end
  return menu.galaxy_menu_find_button(PAUSE_BUTTON)
end

-- Held by the ui, the structure would be dragged onto a pressed row and dropped there.
local function suspend_placement(nav, carried)
  nav.suspended_carry = entity_id.entity_id_link(carried)
  nav.placement = M.PAD_NAV_PLACEMENT_SUSPENDED
  ui_module.ui_set_left_held(ui_module.main_ui(), nil)
end

-- Every close of pause leaves the left button down on its frame, so the resumed carry makes no press edge.
local function resume_placement(nav)
  if nav.placement ~= M.PAD_NAV_PLACEMENT_SUSPENDED then
    return false
  end
  local suspended = placing_structure(entity_id.entity_get(nav.suspended_carry))
  if suspended == nil or probe.main_economy_typed() == nil then
    nav.placement = M.PAD_NAV_PLACEMENT_NONE
    nav.suspended_carry = nil
    return false
  end
  if menu.pause_menu_shown() then
    return false
  end
  ui_module.ui_set_left_held(ui_module.main_ui(), suspended)
  nav.placement = M.PAD_NAV_PLACEMENT_CARRYING
  nav.suspended_carry = nil
  return true
end

local function pause_step(nav, pressed, release, played)
  if menu.pause_menu_shown() then
    if not action_pressed(nav, pressed, bindings_module.BINDINGS_MODE_PAUSE, bindings_module.BINDINGS_ACTION_CLOSE_PAUSE) or
      release then
      return false
    end
    nav.cursor_x = cint.div(vram.P8_SCREEN_WIDTH, 2)
    nav.cursor_y = cint.div(vram.P8_SCREEN_HEIGHT, 2)
    click(nav, played)
    return true
  end
  local due = nav.pause_due
  nav.pause_due = false
  local opening = due or
    action_pressed(nav, pressed, nav_mode(nav), bindings_module.BINDINGS_ACTION_PAUSE)
  local button = nil
  if opening then
    button = pause_button_free(nav, release)
  end
  if button == nil then
    return false
  end
  nav.cursor_x, nav.cursor_y = pad_panel.pad_panel_focus_point(button, nav.cursor_x, nav.cursor_y)
  local carried = M.pad_nav_placing(nav)
  if carried ~= nil then
    suspend_placement(nav, carried)
  else
    cancel_slipway(nav)
  end
  -- The opening click needs the left button up on the frame before.
  if nav.left_was_down and not due then
    nav.pause_due = true
  else
    click(nav, played)
  end
  return true
end

local function lit_button(nav)
  if probe.main_economy_typed() ~= nil then
    return cycle_button(nav)
  end
  return nil
end

local function finish_step(nav, release, shown, played)
  if release then
    played.mouse_buttons = 0
  end
  nav.left_was_down = bit.band(played.mouse_buttons, input.P8_MOUSE_LEFT) ~= 0
  played.mouse_x = nav.cursor_x
  played.mouse_y = nav.cursor_y
  show_marker(nav, shown)
  if ui_module.main_ui() ~= nil then
    ui_module.ui_set_pad_tab(ui_module.main_ui(), lit_button(nav))
    ui_module.ui_set_pad_crosshair(ui_module.main_ui(),
      M.pad_nav_aim_mode(nav) and M.pad_nav_placing(nav) == nil)
  end
end

local function select_allowed(nav, panels_changing, release)
  return probe.main_economy_typed() ~= nil and on_the_map(nav) and not panels_changing and
    not menu.pause_menu_shown() and not M.pad_nav_panel_open() and
    (nav.map_mode == M.PAD_NAV_MAP_JUMP or nav.map_mode == M.PAD_NAV_MAP_AIM) and
    nav.placement == M.PAD_NAV_PLACEMENT_NONE and not nav.probe_or_fold_engaged and not release and
    not nav.release_pending
end

-- Read in every mode, so the latch sees a Select held from elsewhere. Jump's and aim's buttons both count in either.
local function select_held(nav, pad_buttons)
  return action_held(nav, pad_buttons, bindings_module.BINDINGS_MODE_JUMP,
      bindings_module.BINDINGS_ACTION_SELECT_TAP_OR_YEAR) or
    action_held(nav, pad_buttons, bindings_module.BINDINGS_MODE_AIM,
      bindings_module.BINDINGS_ACTION_SELECT_TAP_OR_YEAR)
end

local function select_ends_year(nav, played)
  local button = menu.galaxy_menu_find_button(YEAR_SKIP_BUTTON)
  if button == nil or nav.left_was_down then
    return false
  end
  nav.cursor_x, nav.cursor_y = pad_panel.pad_panel_focus_point(button, nav.cursor_x, nav.cursor_y)
  click(nav, played)
  pad_select.pad_select_year_ended(nav.select)
  return true
end

local function without_chord(nav, raw_buttons)
  local face = cheat.cheat_chord_buttons()
  if bit.band(raw_buttons, bit.bnot(nav.previous_pad), face) == face then
    nav.chord_held = true
  elseif bit.band(raw_buttons, face) == 0 then
    nav.chord_held = false
  end
  if nav.chord_held then
    return bit.band(raw_buttons, bit.bnot(face))
  end
  return raw_buttons
end

local function run_step(nav, raw_buttons, played)
  local pad_buttons = without_chord(nav, raw_buttons)
  local pressed = bit.band(pad_buttons, bit.bnot(nav.previous_pad))
  local release = nav.release_pending
  nav.previous_pad = raw_buttons
  nav.release_pending = false
  played.mouse_buttons = 0
  local shown = nil
  if not menu.pause_menu_shown() then
    nav.pause_choose_row_armed = false
  end
  if not armed_panel_open(nav) then
    nav.menu_choose_row_armed = false
  end
  reset_on_new_game(nav)
  local panels_changed = begin_placement(nav)
  if resume_placement(nav) then
    release = false
  end
  local select_down = select_held(nav, pad_buttons)
  if not panels_changed and pause_step(nav, pressed, release, played) then
    pad_select.pad_select_step(nav.select, select_down, false)
    pad_jump.pad_jump_repeat_clear(nav.jump)
    reset_aim_mode(nav)
    nav.focus = nil
    finish_step(nav, release, nil, played)
    return
  end
  local foreign_was_open = foreign_panel_open(nav)
  local back_in = back_mode(nav)
  local back = bindings_module.bindings_mask(nav.bindings, back_in, back_action(back_in))
  local back_returns = bit.band(pressed, back) ~= 0 and not pad_panel.pad_panel_box_open()
  if bit.band(pressed, back) ~= 0 then
    back_closes_the_box()
    if back_returns and on_the_map(nav) and message_box.shown_message_box() ~= nil then
      message_box.message_box_close_shown()
      back_returns = false
      pressed = bit.band(pressed, bit.bnot(back))
    end
  end
  if M.pad_nav_panel_open() then
    cancel_slipway(nav)
  end
  local cancelled = CANCELLED_NOTHING
  local repeat_kept = false
  if not panels_changed then
    panels_changed, cancelled = switch_panels(nav, pressed, release, foreign_was_open, cancelled, played)
  end
  -- A leaving click and a landing click never share a frame: the landing waits for the release.
  local clicks_due = release or nav.release_pending or cancelled ~= CANCELLED_NOTHING
  local landed = pad_cycle.pad_cycle_landing_due(nav.cycle) and not clicks_due
  if landed then
    pad_cycle.pad_cycle_landed(nav.cycle)
    if cycle_button(nav) ~= nil then
      click(nav, played)
    end
  elseif not panels_changed and cycle_open(nav) and not pad_cycle.pad_cycle_landing_due(nav.cycle) and
    not clicks_due and not own_panel_up(nav) then
    close_panels(nav)
    panels_changed = true
  end
  local year_release = pad_select.pad_select_year_just_ended(nav.select)
  local select = pad_select.pad_select_step(nav.select, select_down,
    select_allowed(nav, panels_changed or landed, release))
  -- The frame that opens or closes the bar or a cycle panel runs no other step, so one press never acts twice.
  if panels_changed or landed then
    reset_aim_mode(nav)
    nav.focus = nil
    place_cursor(nav)
    if cancelled == CANCELLED_STRUCTURE then
      cancel_point(nav)
    elseif M.pad_nav_placing(nav) ~= nil then
      played.mouse_buttons = input.P8_MOUSE_LEFT
    end
  elseif select == pad_select.PAD_SELECT_YEAR and select_ends_year(nav, played) then
    reset_aim_mode(nav)
    nav.focus = nil
  elseif on_the_map(nav) then
    shown, repeat_kept = map_step(nav, pad_buttons, pressed, release, back_returns, select, year_release, played, shown,
      repeat_kept)
  else
    reset_aim_mode(nav)
    bar_or_cycle_step(nav, pad_buttons, pressed, release, played)
  end
  if not repeat_kept then
    pad_jump.pad_jump_repeat_clear(nav.jump)
  end
  finish_step(nav, release, shown, played)
end

function M.pad_nav_step(nav, raw_buttons, played)
  pad_panel.pad_panel_step_begin()
  run_step(nav, raw_buttons, played)
  pad_panel.pad_panel_step_end()
end

return M
