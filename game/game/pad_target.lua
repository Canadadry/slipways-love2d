-- Copy of src/game/pad_target.h then src/game/pad_target.c.
--
-- Shapes, with the C field names:
--   offset_t: { along, perpendicular }, by value: a new table each time.
local entity_class = require("engine.entity_class")
local pad_direction = require("game.pad_direction")
local planet = require("game.planet")
local fixed = require("p8.fixed")

local M = {}

M.PAD_TARGET_PERPENDICULAR_WEIGHT = 2

function M.pad_target_is_candidate(e)
  return (e ~= nil and not e.is_dead and planet.planet_is_family(e) and
    (e.state == entity_class.ENTITY_STATE_KNOWN or e.state == entity_class.ENTITY_STATE_SETTLED)) and 1 or 0
end

local function is_direction(direction)
  return (direction == pad_direction.PAD_DIRECTION_UP or direction == pad_direction.PAD_DIRECTION_DOWN or
    direction == pad_direction.PAD_DIRECTION_LEFT or direction == pad_direction.PAD_DIRECTION_RIGHT) and 1 or 0
end

local function offset_in(direction, from, to)
  local dx = fixed.p8_fixed_sub(to.x, from.x)
  local dy = fixed.p8_fixed_sub(to.y, from.y)
  if direction == pad_direction.PAD_DIRECTION_RIGHT then
    return { along = dx, perpendicular = fixed.p8_fixed_abs(dy) }
  elseif direction == pad_direction.PAD_DIRECTION_LEFT then
    return { along = -dx, perpendicular = fixed.p8_fixed_abs(dy) }
  elseif direction == pad_direction.PAD_DIRECTION_DOWN then
    return { along = dy, perpendicular = fixed.p8_fixed_abs(dx) }
  end
  return { along = -dy, perpendicular = fixed.p8_fixed_abs(dx) }
end

function M.pad_target_pick(origin, direction, candidates)
  assert(is_direction(direction) ~= 0,
    "pad_target_pick: the direction is exactly one of PAD_DIRECTION_UP, DOWN, LEFT or RIGHT")
  assert(origin ~= nil and entity_class.entity_has_map_pos(origin),
    "pad_target_pick: the origin is an entity with a map position")
  if candidates == nil then
    return nil
  end
  local from = entity_class.entity_map_pos(origin)
  local best = nil
  local best_offset = { along = 0, perpendicular = 0 }
  local best_score = 0
  for i = 1, #candidates do
    local e = candidates[i]
    if e == origin then
      goto continue
    end
    assert(e ~= nil and entity_class.entity_has_map_pos(e),
      "pad_target_pick: every candidate is an entity with a map position")
    do
      local offset = offset_in(direction, from, entity_class.entity_map_pos(e))
      if offset.along <= 0 or offset.perpendicular > offset.along then
        goto continue
      end
      local score = fixed.p8_fixed_add(offset.along, offset.perpendicular * M.PAD_TARGET_PERPENDICULAR_WEIGHT)
      if best == nil or score < best_score or
          (score == best_score and offset.perpendicular < best_offset.perpendicular) then
        best = e
        best_offset = offset
        best_score = score
      end
    end
    ::continue::
  end
  return best
end

return M
