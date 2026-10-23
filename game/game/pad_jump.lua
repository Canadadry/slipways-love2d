local entity_class = require("engine.entity_class")
local entity_id = require("engine.entity_id")
local bindings_module = require("game.bindings")
local galaxy = require("game.galaxy")
local pad_direction = require("game.pad_direction")
local pad_target = require("game.pad_target")
local planet = require("game.planet")
local fixed = require("p8.fixed")
local vram = require("p8.vram")
local bit = require("bit")

local M = {}

M.PAD_JUMP_REPEAT_DELAY = 18
M.PAD_JUMP_REPEAT_PERIOD = 8
M.PAD_JUMP_SCROLL_MARGIN = 20
M.PAD_JUMP_SCROLL_MARGIN_BOTTOM = 31
M.PAD_JUMP_SCROLL_SPEED = 8

M.PAD_JUMP_DIRECTIONS = 4

-- hovered is an entity id, nil for none.
function M.pad_jump_init(jump)
  jump.hovered = nil
  jump.seeded = false
  jump.repeat_direction = pad_direction.PAD_DIRECTION_NONE
  jump.repeat_frames = 0
end

function M.pad_jump_hover(jump, e)
  jump.hovered = entity_id.link(e)
end

function M.pad_jump_hovered(jump)
  local hovered = entity_id.get(jump.hovered)
  return pad_target.pad_target_is_candidate(hovered) and hovered or nil
end

function M.pad_jump_hover_seeded(jump, e)
  M.pad_jump_hover(jump, e)
  jump.seeded = jump.seeded or e ~= nil
end

function M.pad_jump_reseed(jump, candidates, centre_x, centre_y)
  local picked
  if jump.seeded then
    picked = M.pad_jump_nearest(centre_x, centre_y, candidates)
  else
    picked = M.pad_jump_seed(candidates, centre_x, centre_y)
  end
  M.pad_jump_hover_seeded(jump, picked)
  return picked
end

M.directions_by_priority = {
  pad_direction.PAD_DIRECTION_UP,
  pad_direction.PAD_DIRECTION_DOWN,
  pad_direction.PAD_DIRECTION_LEFT,
  pad_direction.PAD_DIRECTION_RIGHT,
}
local directions_by_priority = M.directions_by_priority

function M.pad_jump_direction(index)
  assert(index >= 1 and index <= M.PAD_JUMP_DIRECTIONS, "pad_jump_direction: index 1 to PAD_JUMP_DIRECTIONS")
  return directions_by_priority[index]
end

local function direction_mask(bindings, mode, direction)
  if direction == pad_direction.PAD_DIRECTION_UP then
    return bindings_module.bindings_mask(bindings, mode, bindings_module.BINDINGS_ACTION_JUMP_UP)
  elseif direction == pad_direction.PAD_DIRECTION_DOWN then
    return bindings_module.bindings_mask(bindings, mode, bindings_module.BINDINGS_ACTION_JUMP_DOWN)
  elseif direction == pad_direction.PAD_DIRECTION_LEFT then
    return bindings_module.bindings_mask(bindings, mode, bindings_module.BINDINGS_ACTION_JUMP_LEFT)
  elseif direction == pad_direction.PAD_DIRECTION_RIGHT then
    return bindings_module.bindings_mask(bindings, mode, bindings_module.BINDINGS_ACTION_JUMP_RIGHT)
  end
  return 0
end

local function first_pressed_direction(bindings, mode, pressed)
  for i = 1, M.PAD_JUMP_DIRECTIONS do
    if bit.band(pressed, direction_mask(bindings, mode, directions_by_priority[i])) ~= 0 then
      return directions_by_priority[i]
    end
  end
  return pad_direction.PAD_DIRECTION_NONE
end

function M.pad_jump_repeat_step(jump, bindings, mode, pad_buttons, pressed)
  assert(bindings ~= nil and
    bindings_module.bindings_mode_has_action(mode, bindings_module.BINDINGS_ACTION_JUMP_UP),
    "pad_jump_repeat_step: a mode with jump_* actions (jump or slipway)")
  local direction = first_pressed_direction(bindings, mode, pressed)
  if direction ~= pad_direction.PAD_DIRECTION_NONE then
    jump.repeat_direction = direction
    jump.repeat_frames = 0
    return jump.repeat_direction
  end
  if bit.band(pad_buttons, direction_mask(bindings, mode, jump.repeat_direction)) == 0 then
    M.pad_jump_repeat_clear(jump)
    return pad_direction.PAD_DIRECTION_NONE
  end
  jump.repeat_frames = jump.repeat_frames + 1
  if jump.repeat_frames == M.PAD_JUMP_REPEAT_DELAY + M.PAD_JUMP_REPEAT_PERIOD then
    jump.repeat_frames = M.PAD_JUMP_REPEAT_DELAY
  end
  return jump.repeat_frames == M.PAD_JUMP_REPEAT_DELAY and jump.repeat_direction or pad_direction.PAD_DIRECTION_NONE
end

function M.pad_jump_repeat_clear(jump)
  jump.repeat_direction = pad_direction.PAD_DIRECTION_NONE
  jump.repeat_frames = 0
end

local inner_left = M.PAD_JUMP_SCROLL_MARGIN
local inner_right = vram.SCREEN_WIDTH - 1 - M.PAD_JUMP_SCROLL_MARGIN
local inner_top = M.PAD_JUMP_SCROLL_MARGIN
-- PAD_JUMP_SCROLL_MARGIN_BOTTOM is the margin plus the bottom HUD row, which starts at vram.SCREEN_HEIGHT - 11.
local inner_bottom = vram.SCREEN_HEIGHT - 1 - M.PAD_JUMP_SCROLL_MARGIN_BOTTOM

function M.pad_jump_inside(x, y)
  return x >= inner_left and x <= inner_right and y >= inner_top and y <= inner_bottom
end

local function shift_into(value, low, high)
  local shift = value < low and value - low or value > high and value - high or 0
  if shift > M.PAD_JUMP_SCROLL_SPEED then
    return M.PAD_JUMP_SCROLL_SPEED
  end
  return shift < -M.PAD_JUMP_SCROLL_SPEED and -M.PAD_JUMP_SCROLL_SPEED or shift
end

function M.pad_jump_scroll(x, y, dx, dy)
  dx = shift_into(x, inner_left, inner_right)
  dy = shift_into(y, inner_top, inner_bottom)
  return dx, dy
end

local function world_pixel(map_units)
  return fixed.flr_int(fixed.mul(map_units, fixed.from_int(galaxy.MAP_SCALE)))
end

function M.pad_jump_world_point(e, x, y)
  assert(e ~= nil and entity_class.has_map_pos(e), "pad_jump_world_point: the entity has a map position")
  local map_pos = entity_class.map_pos(e)
  x = world_pixel(map_pos.x)
  y = world_pixel(map_pos.y)
  return x, y
end

function M.pad_jump_nearest(x, y, candidates)
  if candidates == nil then
    return nil
  end
  local best = nil
  local best_distance = 0
  for i = 1, #candidates do
    local e = candidates[i]
    local ex, ey = M.pad_jump_world_point(e, 0, 0)
    local distance = (ex - x) * (ex - x) + (ey - y) * (ey - y)
    if best == nil or distance < best_distance then
      best = e
      best_distance = distance
    end
  end
  return best
end

local function is_colony(e)
  return e.class == planet.planet_class() and e.state == entity_class.STATE_SETTLED
end

function M.pad_jump_seed(candidates, centre_x, centre_y)
  if candidates == nil then
    return nil
  end
  for i = 1, #candidates do
    if is_colony(candidates[i]) then
      return candidates[i]
    end
  end
  return M.pad_jump_nearest(centre_x, centre_y, candidates)
end

-- An empty array for a nil list.
function M.pad_jump_candidates(nodes)
  local candidates = {}
  if nodes == nil then
    return candidates
  end
  for i = 1, #nodes do
    if pad_target.pad_target_is_candidate(nodes[i]) then
      candidates[#candidates + 1] = nodes[i]
    end
  end
  return candidates
end

function M.pad_jump_any_candidate(nodes)
  if nodes == nil then
    return false
  end
  for i = 1, #nodes do
    if pad_target.pad_target_is_candidate(nodes[i]) then
      return true
    end
  end
  return false
end

return M
