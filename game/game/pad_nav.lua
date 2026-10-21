-- Copy of src/game/pad_nav.h then src/game/pad_nav.c.
--
-- Shapes, with the C field names:
--   pad_nav_t: { focus, build, cycle, placement, suspended_carry, pause_due, pause_choose_row_armed,
--     menu_choose_row_armed, map_mode, marker_frames, release_pending, previous_pad, chord_held, left_was_down,
--     cursor_x, cursor_y, held_frames, probe_or_fold_engaged, game, jump, slipway, back, select, bindings };
--     focus, suspended_carry and game entity id tables, held_frames 0-indexed. pad_nav_init creates the sub-tables
--     only when absent and fills bindings.masks in place, so a re-init keeps their identity as the C struct does.
-- TODO: the rest of pad_nav.c, from map_binding_mode to pad_nav_step (step 3.13, chunk 4).
local bit = require("bit")
local entity_class = require("engine.entity_class")
local entity_id = require("engine.entity_id")
local entity_id_type = require("engine.entity_id_type")
local entity_registry = require("engine.entity_registry")
local frame_zone = require("engine.frame_zone")
local vec = require("engine.vec")
local background = require("game.background")
local bindings_module = require("game.bindings")
local menu = require("game.menu")
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

local function entity_id_none()
  local none = entity_id_type.ENTITY_ID_NONE
  return { index = none.index, generation = none.generation }
end

function M.pad_nav_panel_open()
  return pad_panel.pad_panel_open()
end

local function on_the_map(nav)
  assert(not (pad_build.pad_build_is_open(nav.build) ~= 0 and
      pad_cycle.pad_cycle_panel(nav.cycle) ~= pad_cycle.PAD_CYCLE_CLOSED),
    "pad_nav: the build bar and a cycle panel are never open together")
  return (pad_build.pad_build_is_open(nav.build) == 0 and
    pad_cycle.pad_cycle_panel(nav.cycle) == pad_cycle.PAD_CYCLE_CLOSED) and 1 or 0
end

local function cycle_open(nav)
  return (pad_cycle.pad_cycle_panel(nav.cycle) ~= pad_cycle.PAD_CYCLE_CLOSED) and 1 or 0
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
  return (nav.select.chain_frames > 0) and 1 or 0
end

local function game_unseen(nav)
  local economy = probe.main_economy_typed()
  return (economy ~= nil and entity_id.entity_get(nav.game) ~= economy) and 1 or 0
end

local function map_mode_now(nav)
  if game_unseen(nav) == 0 then
    return nav.map_mode
  end
  return (pad_jump.pad_jump_any_candidate(entity_registry.entities_by_tag(entity_class.TAG_NODE)) ~= 0) and
    M.PAD_NAV_MAP_JUMP or M.PAD_NAV_MAP_AIM
end

function M.pad_nav_aim_mode(nav)
  return (probe.main_economy_typed() ~= nil and on_the_map(nav) ~= 0 and M.pad_nav_panel_open() == 0 and
    map_mode_now(nav) == M.PAD_NAV_MAP_AIM) and 1 or 0
end

local function live(e)
  if e ~= nil and e.is_dead == 0 then
    return e
  end
  return nil
end

function M.pad_nav_jump_mode(nav)
  return (probe.main_economy_typed() ~= nil and on_the_map(nav) ~= 0 and M.pad_nav_panel_open() == 0 and
    map_mode_now(nav) == M.PAD_NAV_MAP_JUMP) and 1 or 0
end

function M.pad_nav_hovered(nav)
  if nav.map_mode == M.PAD_NAV_MAP_JUMP and game_unseen(nav) == 0 then
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
  if pad_target.pad_target_is_candidate(target) ~= 0 then
    return target
  end
  return nil
end

function M.pad_nav_build_bar_open(nav)
  return (probe.main_economy_typed() ~= nil and pad_build.pad_build_is_open(nav.build) ~= 0) and 1 or 0
end

function M.pad_nav_tech_open(nav)
  return (probe.main_economy_typed() ~= nil and
    pad_cycle.pad_cycle_panel(nav.cycle) == pad_cycle.PAD_CYCLE_TECH) and 1 or 0
end

function M.pad_nav_score_open(nav)
  return (probe.main_economy_typed() ~= nil and
    pad_cycle.pad_cycle_panel(nav.cycle) == pad_cycle.PAD_CYCLE_SCORE) and 1 or 0
end

function M.pad_nav_build_icon(nav)
  if M.pad_nav_build_bar_open(nav) ~= 0 then
    return pad_build.pad_build_icon(nav.build)
  end
  return nil
end

local function placing_structure(e)
  e = live(e)
  if e ~= nil and planet_module.planet_is_family(e) ~= 0 and e.state == entity_class.ENTITY_STATE_PLACING then
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
  for i = 0, pad_jump.PAD_JUMP_DIRECTIONS - 1 do
    nav.held_frames[i] = 0
  end
  nav.probe_or_fold_engaged = 0
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
  [0] = bindings_module.BINDINGS_ACTION_PAN_UP, bindings_module.BINDINGS_ACTION_PAN_DOWN,
  bindings_module.BINDINGS_ACTION_PAN_LEFT, bindings_module.BINDINGS_ACTION_PAN_RIGHT,
}

local function aim_pan(nav, mode, pad_buttons)
  assert(bindings_module.bindings_mode_has_action(mode, bindings_module.BINDINGS_ACTION_PAN_UP) ~= 0,
    "aim_pan: a mode with pan_* actions (aim or placement)")
  local speed = {}
  for i = 0, pad_jump.PAD_JUMP_DIRECTIONS - 1 do
    if bit.band(pad_buttons, bindings_module.bindings_mask(nav.bindings, mode, pan_action[i])) == 0 then
      nav.held_frames[i] = 0
    elseif nav.held_frames[i] < HELD_FRAMES_CAP then
      nav.held_frames[i] = nav.held_frames[i] + 1
    end
    speed[i] = pan_speed(nav.held_frames[i])
  end
  assert(pad_jump.pad_jump_direction(0) == pad_direction.PAD_DIRECTION_UP and
    pad_jump.pad_jump_direction(1) == pad_direction.PAD_DIRECTION_DOWN and
    pad_jump.pad_jump_direction(2) == pad_direction.PAD_DIRECTION_LEFT and
    pad_jump.pad_jump_direction(3) == pad_direction.PAD_DIRECTION_RIGHT,
    "aim_pan: speed[] and pan_action[] are indexed Up, Down, Left, Right")
  local camera = scroll.main_camera_typed()
  if camera ~= nil then
    scroll.camera_pan(camera, speed[3] - speed[2], speed[1] - speed[0])
  end
end

local function reset_map_mode(nav)
  nav.map_mode = M.PAD_NAV_MAP_JUMP
  pad_jump.pad_jump_hover(nav.jump, nil)
  pad_jump.pad_jump_repeat_clear(nav.jump)
  nav.marker_frames = 0
  nav.placement = M.PAD_NAV_PLACEMENT_NONE
  nav.suspended_carry = entity_id_none()
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
  for mode = 0, bindings_module.BINDINGS_MODE_COUNT - 1 do
    if masks[mode] == nil then
      masks[mode] = {}
    end
    for action = 0, bindings_module.BINDINGS_ACTION_COUNT - 1 do
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

function M.pad_nav_init(nav, bindings)
  assert(bindings ~= nil, "pad_nav_init: the caller hands its compiled table, never NULL")
  create_sub_tables(nav)
  copy_bindings(nav, bindings)
  nav.focus = entity_id_none()
  pad_build.pad_build_init(nav.build)
  pad_cycle.pad_cycle_init(nav.cycle)
  nav.game = entity_id_none()
  pad_jump.pad_jump_init(nav.jump)
  pad_back.pad_back_init(nav.back)
  reset_map_mode(nav)
  nav.release_pending = 0
  nav.pause_due = 0
  nav.pause_choose_row_armed = 0
  nav.menu_choose_row_armed = 0
  nav.previous_pad = 0
  nav.chord_held = 0
  nav.left_was_down = 0
  nav.cursor_x = 0
  nav.cursor_y = 0
  reset_aim_mode(nav)
end

function M.pad_nav_bindings(nav)
  return nav.bindings
end

local function action_pressed(nav, pressed, mode, action)
  return (bit.band(pressed, bindings_module.bindings_mask(nav.bindings, mode, action)) ~= 0) and 1 or 0
end

local function action_held(nav, pad_buttons, mode, action)
  return (bit.band(pad_buttons, bindings_module.bindings_mask(nav.bindings, mode, action)) ~= 0) and 1 or 0
end

function M.pad_nav_focus(nav)
  local focus = entity_id.entity_get(nav.focus)
  if M.pad_nav_is_item(focus) ~= 0 then
    return focus
  end
  return nil
end

local function planet_menu_open(nav)
  return (on_the_map(nav) ~= 0 and pad_panel.pad_panel_box_open() ~= 0 and menu.pause_menu_shown() == 0) and 1 or 0
end

local function armed_panel_open(nav)
  return ((on_the_map(nav) ~= 0 or cycle_open(nav) ~= 0) and M.pad_nav_panel_open() ~= 0 and
    menu.pause_menu_shown() == 0) and 1 or 0
end

local function panel_mode(nav)
  if menu.pause_menu_shown() ~= 0 then
    return bindings_module.BINDINGS_MODE_PAUSE
  end
  if probe.main_economy_typed() == nil then
    return bindings_module.BINDINGS_MODE_TITLE
  end
  return (cycle_open(nav) ~= 0) and bindings_module.BINDINGS_MODE_PANELS or bindings_module.BINDINGS_MODE_MENU
end

local move_action = {
  [0] = bindings_module.BINDINGS_ACTION_MOVE_UP, bindings_module.BINDINGS_ACTION_MOVE_DOWN,
  bindings_module.BINDINGS_ACTION_MOVE_LEFT, bindings_module.BINDINGS_ACTION_MOVE_RIGHT,
}

local function first_pressed_move(nav, pressed, mode)
  assert(bindings_module.bindings_mode_has_action(mode, bindings_module.BINDINGS_ACTION_MOVE_UP) ~= 0,
    "first_pressed_move: a mode with move_* actions (title, menu, pause or panels)")
  for i = 0, pad_jump.PAD_JUMP_DIRECTIONS - 1 do
    if action_pressed(nav, pressed, mode, move_action[i]) ~= 0 then
      return pad_jump.pad_jump_direction(i)
    end
  end
  return pad_direction.PAD_DIRECTION_NONE
end

local function panel_step(nav, pad_buttons, pressed, played)
  local mode = panel_mode(nav)
  local direction = first_pressed_move(nav, pressed, mode)
  local choose_held = action_held(nav, pad_buttons, mode, bindings_module.BINDINGS_ACTION_CHOOSE_ROW)
  if menu.pause_menu_shown() ~= 0 then
    nav.pause_choose_row_armed = bit.bor(nav.pause_choose_row_armed,
      action_pressed(nav, pressed, bindings_module.BINDINGS_MODE_PAUSE, bindings_module.BINDINGS_ACTION_CHOOSE_ROW))
    choose_held = bit.band(choose_held, nav.pause_choose_row_armed)
  elseif armed_panel_open(nav) ~= 0 then
    nav.menu_choose_row_armed = bit.bor(nav.menu_choose_row_armed,
      action_pressed(nav, pressed, mode, bindings_module.BINDINGS_ACTION_CHOOSE_ROW))
    choose_held = bit.band(choose_held, nav.menu_choose_row_armed)
  end
  local at
  at, nav.cursor_x, nav.cursor_y, played.mouse_buttons = pad_panel.pad_panel_step(entity_id.entity_get(nav.focus),
    choose_held, direction, nav.cursor_x, nav.cursor_y, played.mouse_buttons)
  nav.focus = entity_id.entity_id_link(at)
end

-- Keep in step with pad_panel.c's box_at for layers under 10: world point minus the camera, floored.
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
  return (e.class.left_press ~= nil and e.class ~= background.background_class()) and 1 or 0
end

local function click(nav, played)
  played.mouse_buttons = input.P8_MOUSE_LEFT
  nav.release_pending = 1
end

local function enter_aim_mode(nav)
  nav.map_mode = M.PAD_NAV_MAP_AIM
  nav.cursor_x = cint.div(vram.P8_SCREEN_WIDTH, 2)
  nav.cursor_y = cint.div(vram.P8_SCREEN_HEIGHT, 2)
  pad_jump.pad_jump_repeat_clear(nav.jump)
end

local function pressed_before(planet, x, y)
  return (pad_panel.pad_panel_entity_under(x, y, fixed.p8_fixed_flr_int(planet.layer),
    pad_panel.pad_panel_is_candidate) ~= nil) and 1 or 0
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
  return pad_jump.pad_jump_candidates(frame_zone.frame_zone(), entity_registry.entities_by_tag(entity_class.TAG_NODE))
end

local function back_to_jump_mode(nav, e)
  nav.map_mode = M.PAD_NAV_MAP_JUMP
  if pad_target.pad_target_is_candidate(e) ~= 0 then
    pad_jump.pad_jump_hover(nav.jump, e)
  else
    pad_jump.pad_jump_hover(nav.jump, nil)
  end
  pad_slipway.pad_slipway_init(nav.slipway)
end

-- Seeded, not pad_jump_hover: jump mode must never reseed on the first colony afterwards.
local function leave_aim_mode(nav)
  if pad_jump.pad_jump_any_candidate(entity_registry.entities_by_tag(entity_class.TAG_NODE)) == 0 then
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
    return 0
  end
  local held = (held_slipway() ~= nil) and 1 or 0
  drop_held_slipway()
  back_to_jump_mode(nav, M.pad_nav_slipway_start(nav))
  return held
end

local function select_tap(nav, current, shown)
  local candidates = live_candidates()
  local next = pad_select.pad_select_next(candidates, current)
  local jumps = (next ~= nil and next ~= current) and 1 or 0
  if jumps ~= 0 then
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
  nav.focus = entity_id_none()
  nav.cursor_x = cint.div(vram.P8_SCREEN_WIDTH, 2)
  nav.cursor_y = cint.div(vram.P8_SCREEN_HEIGHT, 2)
  if still == 0 then
    aim_pan(nav, bindings_module.BINDINGS_MODE_AIM, pad_buttons)
  end
  if select_tapped ~= 0 and nav.probe_or_fold_engaged == 0 then
    local jumps
    jumps, shown = select_tap(nav, nil, shown)
    if jumps ~= 0 then
      return shown
    end
  end
  if action_pressed(nav, pressed, bindings_module.BINDINGS_MODE_AIM, bindings_module.BINDINGS_ACTION_LEAVE_AIM) ~= 0 and
    nav.probe_or_fold_engaged == 0 then
    leave_aim_mode(nav)
    return shown
  end
  if action_held(nav, pad_buttons, bindings_module.BINDINGS_MODE_AIM,
      bindings_module.BINDINGS_ACTION_PROBE_OR_FOLD) == 0 then
    nav.probe_or_fold_engaged = 0
  elseif action_pressed(nav, pressed, bindings_module.BINDINGS_MODE_AIM,
      bindings_module.BINDINGS_ACTION_PROBE_OR_FOLD) ~= 0 and release == 0 then
    nav.probe_or_fold_engaged = (entity_under(nav.cursor_x, nav.cursor_y, presses_off_background) == nil) and 1 or 0
  end
  if nav.probe_or_fold_engaged ~= 0 then
    played.mouse_buttons = input.P8_MOUSE_LEFT
  elseif back_returns ~= 0 then
    leave_aim_mode(nav)
  end
  return shown
end

local function reset_on_new_game(nav)
  if game_unseen(nav) == 0 then
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
  if pad_panel.pad_panel_box_open() ~= 0 and menu.pause_menu_shown() == 0 then
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
    return (pad_panel.pad_panel_box_open() ~= 0 and menu.pause_menu_shown() == 0) and 1 or 0
  end
  if pad_cycle.pad_cycle_panel(nav.cycle) == pad_cycle.PAD_CYCLE_SCORE then
    return (M.pad_nav_panel_open() ~= 0 and pad_panel.pad_panel_box_open() == 0 and
      pad_panel.pad_panel_first_item() == nil) and 1 or 0
  end
  return 0
end

local function foreign_panel_open(nav)
  return (M.pad_nav_panel_open() ~= 0 and own_panel_up(nav) == 0) and 1 or 0
end

-- A slipway released on this frame is judged on the next: the shoulders and pause wait for it.
local function map_action_due(nav)
  return (nav.map_mode == M.PAD_NAV_MAP_SLIPWAY and nav.slipway.phase == pad_slipway.PAD_SLIPWAY_RELEASED) and 1 or 0
end

local function shoulders_can_switch(nav, release)
  if (foreign_panel_open(nav) ~= 0 and planet_menu_open(nav) == 0) or release ~= 0 or
    pad_cycle.pad_cycle_landing_due(nav.cycle) ~= 0 then
    return 0
  end
  return (on_the_map(nav) == 0 or map_action_due(nav) == 0) and 1 or 0
end

local CANCELLED_NOTHING = 0
local CANCELLED_STRUCTURE = 1
local CANCELLED_SLIPWAY = 2

-- The caller owes a cancelled structure cancel_point, button up; a cancelled slipway is already dropped.
local function unwind_sub_modes(nav)
  if on_the_map(nav) == 0 then
    return CANCELLED_NOTHING
  end
  if planet_menu_open(nav) ~= 0 then
    ui_module.ui_clear_ephemerals(ui_module.main_ui())
  end
  if M.pad_nav_placing(nav) ~= nil then
    return CANCELLED_STRUCTURE
  end
  return (cancel_slipway(nav) ~= 0) and CANCELLED_SLIPWAY or CANCELLED_NOTHING
end

local function leave_panel(nav, played)
  if own_panel_up(nav) == 0 then
    return
  end
  if pad_cycle.pad_cycle_panel(nav.cycle) == pad_cycle.PAD_CYCLE_TECH then
    ui_module.ui_clear_ephemerals(ui_module.main_ui())
  else
    click(nav, played)
  end
end

return M
