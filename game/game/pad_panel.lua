local bit = require("bit")
local entity_class = require("engine.entity_class")
local entity_id = require("engine.entity_id")
local entity_registry = require("engine.entity_registry")
local vec = require("engine.vec")
local pad_direction = require("game.pad_direction")
local probe = require("game.probe")
local scroll = require("game.scroll")
local ui_module = require("game.ui")
local ui_button = require("game.ui_button")
local fixed = require("p8.fixed")
local input = require("p8.input")

local M = {}

M.PAD_PANEL_ANY_LAYER = -32768

local SCREEN_SPACE_LAYER = 10

function M.pad_panel_box_open()
  local ui = ui_module.main_ui()
  return ui ~= nil and #ui.ephemerals > 0
end

local function mouse_at(x, y)
  local camera = scroll.main_camera_typed()
  local mouse = { world = vec.make(0, 0), screen = vec.make(0, 0), has = 0 }
  mouse.screen = vec.make(fixed.from_int(x), fixed.from_int(y))
  mouse.world = camera ~= nil and vec.add(mouse.screen, entity_class.pos(camera)) or mouse.screen
  mouse.has = bit.bor(entity_class.MOUSE_POS_HAS_SCREEN, entity_class.MOUSE_POS_HAS_WORLD)
  return mouse
end

function M.pad_panel_entity_under(x, y, above, accept)
  local mouse = mouse_at(x, y)
  local bucket = entity_registry.entities_by_message(entity_registry.MESSAGE_HIT_TEST)
  for i = 1, #bucket do
    local e = bucket[i]
    if not e.is_dead and (above == M.PAD_PANEL_ANY_LAYER or fixed.flr_int(e.layer) > above) and
      accept(e) and e.class.hit_test ~= nil and e.class.hit_test(e, mouse) then
      return e
    end
  end
  return nil
end

local function is_modal_backdrop(e)
  return e.class == ui_button.modal_backdrop_class()
end

local function panel_open_now(ui, economy)
  if ui == nil then
    return false
  end
  assert(ui_button.modal_backdrop_class().hit_test == entity_registry.hit_test_always,
    "pad_panel_open: the backdrop is found at (0, 0) only because its hit test always holds")
  return economy == nil or #ui.ephemerals > 0 or
    M.pad_panel_entity_under(0, 0, M.PAD_PANEL_ANY_LAYER, is_modal_backdrop) ~= nil
end

local g_memo = {
  in_step = false,
  known = false,
  key = { ui = nil, economy = nil, ephemerals_count = 0, generation = 0 },
  open = false,
}

-- Inside the pair, pad_panel_open reuses its answer while the ui, the economy, the box's ephemerals count and
-- the registered set are unchanged; a backdrop's is_dead is not watched, nothing writes it inside a pad step.
function M.pad_panel_step_begin()
  assert(not g_memo.in_step, "pad_panel_step_begin: a pad step is already open")
  g_memo.in_step = true
  g_memo.known = false
end

function M.pad_panel_step_end()
  assert(g_memo.in_step, "pad_panel_step_end: no pad step is open")
  g_memo.in_step = false
end

function M.pad_panel_open()
  local ui = ui_module.main_ui()
  local economy = probe.main_economy_typed()
  if not g_memo.in_step then
    return panel_open_now(ui, economy)
  end
  local key = {
    ui = ui ~= nil and ui.id or nil,
    economy = economy ~= nil and economy.id or nil,
    ephemerals_count = ui ~= nil and #ui.ephemerals or 0,
    generation = entity_registry.generation(),
  }
  if not g_memo.known or g_memo.key.ui ~= key.ui or
    g_memo.key.economy ~= key.economy or
    g_memo.key.ephemerals_count ~= key.ephemerals_count or g_memo.key.generation ~= key.generation then
    g_memo.key = key
    g_memo.open = panel_open_now(ui, economy)
    g_memo.known = true
  end
  return g_memo.open
end

-- Mirrors ui_update: while a box is open, a left press reaches only its ephemerals.
local function candidates_all()
  local it = { from_ephemerals = M.pad_panel_box_open(), index = 1 }
  return it
end

function M.pad_panel_is_candidate(e)
  return e.class.left_press ~= nil and e.layer ~= nil
end

-- Advances it.index in place.
local function candidates_next(it)
  local ui = ui_module.main_ui()
  if ui == nil then
    return nil
  end
  while true do
    local e
    if it.from_ephemerals then
      if it.index > #ui.ephemerals then
        return nil
      end
      e = entity_id.resolve(ui.ephemerals[it.index])
      it.index = it.index + 1
    else
      local bucket = entity_registry.entities_by_message(entity_registry.MESSAGE_HIT_TEST)
      if it.index > #bucket then
        return nil
      end
      e = bucket[it.index]
      it.index = it.index + 1
    end
    if M.pad_panel_is_candidate(e) then
      return e
    end
  end
end

local function top_layer()
  local it = candidates_all()
  local top = -1
  local e = candidates_next(it)
  while e ~= nil do
    local layer = fixed.flr_int(e.layer)
    if layer > top then
      top = layer
    end
    e = candidates_next(it)
  end
  return top
end

local function is_item_on(e, layer)
  return M.pad_panel_is_candidate(e) and fixed.flr_int(e.layer) == layer and
    e.hit_box ~= nil
end

function M.pad_panel_is_item(e)
  if e == nil or not M.pad_panel_open() then
    return false
  end
  local layer = top_layer()
  local it = candidates_all()
  local candidate = candidates_next(it)
  while candidate ~= nil do
    if candidate == e then
      return is_item_on(e, layer)
    end
    candidate = candidates_next(it)
  end
  return false
end

function M.pad_panel_first_item()
  if not M.pad_panel_open() then
    return nil
  end
  local layer = top_layer()
  local it = candidates_all()
  local e = candidates_next(it)
  while e ~= nil do
    if is_item_on(e, layer) then
      return e
    end
    e = candidates_next(it)
  end
  return nil
end

-- Returns a new box on each call.
local function box_at(e, origin)
  if fixed.flr_int(e.layer) < SCREEN_SPACE_LAYER then
    local camera = scroll.main_camera_typed()
    if camera ~= nil then
      origin = vec.sub(origin, entity_class.pos(camera))
    end
  end
  local box = {
    left = fixed.add(origin.x, e.hit_box.left),
    top = fixed.add(origin.y, e.hit_box.top),
    right = fixed.add(origin.x, e.hit_box.right),
    bottom = fixed.add(origin.y, e.hit_box.bottom),
  }
  return box
end

local function screen_box(e)
  return box_at(e, entity_class.pos(e))
end

-- A box row still sliding in is judged where it comes to rest, or a Down on its first frames skips a row.
local function rest_box(e)
  local button = e.class == ui_button.button_class() and e or nil
  return box_at(e, (button ~= nil and button.slide_target ~= nil) and button.slide_target or
    entity_class.pos(e))
end

local function middle(low, high)
  return fixed.flr(fixed.div(fixed.add(low, high), fixed.from_int(2)))
end

local function box_middle(box, x, y)
  x = fixed.flr_int(middle(box.left, box.right))
  y = fixed.flr_int(middle(box.top, box.bottom))
  return x, y
end

function M.pad_panel_focus_point(e, x, y)
  x, y = box_middle(screen_box(e), x, y)
  return x, y
end

local function is_direction(direction)
  return direction == pad_direction.PAD_DIRECTION_UP or direction == pad_direction.PAD_DIRECTION_DOWN or
    direction == pad_direction.PAD_DIRECTION_LEFT or direction == pad_direction.PAD_DIRECTION_RIGHT
end

local function absolute(value)
  return value < 0 and -value or value
end

local function span_gap(low_a, high_a, low_b, high_b)
  if low_b > high_a then
    return fixed.flr_int(fixed.sub(low_b, high_a))
  end
  if low_a > high_b then
    return fixed.flr_int(fixed.sub(low_a, high_b))
  end
  return 0
end

local function neighbor_score(item, candidate, direction)
  local from = rest_box(item)
  local to = rest_box(candidate)
  local from_x, from_y = box_middle(from, 0, 0)
  local to_x, to_y = box_middle(to, 0, 0)
  if direction == pad_direction.PAD_DIRECTION_UP then
    if fixed.from_int(to_y) >= from.top then return -1 end
  elseif direction == pad_direction.PAD_DIRECTION_DOWN then
    if fixed.from_int(to_y) <= from.bottom then return -1 end
  elseif direction == pad_direction.PAD_DIRECTION_LEFT then
    if fixed.from_int(to_x) >= from.left then return -1 end
  else
    if fixed.from_int(to_x) <= from.right then return -1 end
  end
  if direction == pad_direction.PAD_DIRECTION_UP or direction == pad_direction.PAD_DIRECTION_DOWN then
    return absolute(to_y - from_y) + 2 * span_gap(from.left, from.right, to.left, to.right)
  end
  return absolute(to_x - from_x) + 2 * span_gap(from.top, from.bottom, to.top, to.bottom)
end

function M.pad_panel_neighbor(item, direction)
  assert(is_direction(direction),
    "pad_panel_neighbor: the direction is exactly one of PAD_DIRECTION_UP, DOWN, LEFT or RIGHT")
  local layer = top_layer()
  local it = candidates_all()
  local best = nil
  local best_score = -1
  local e = candidates_next(it)
  while e ~= nil do
    if e ~= item and is_item_on(e, layer) then
      local score = neighbor_score(item, e, direction)
      if score >= 0 and (best == nil or score < best_score) then
        best = e
        best_score = score
      end
    end
    e = candidates_next(it)
  end
  return best
end

function M.pad_panel_step(focus, choose_held, direction, x, y, mouse_buttons)
  local at = M.pad_panel_is_item(focus) and focus or M.pad_panel_first_item()
  if at ~= nil and direction ~= pad_direction.PAD_DIRECTION_NONE then
    local next_item = M.pad_panel_neighbor(at, direction)
    if next_item ~= nil then
      at = next_item
    end
  end
  if at ~= nil then
    x, y = M.pad_panel_focus_point(at, x, y)
  end
  -- With no focus the cursor still sits on the last point: on the score panel a press there closes it.
  mouse_buttons = (at ~= nil and choose_held) and input.MOUSE_LEFT or 0
  return at, x, y, mouse_buttons
end

return M
