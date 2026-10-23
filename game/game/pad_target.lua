local entity_class = require("engine.entity_class")
local pad_direction = require("game.pad_direction")
local planet = require("game.planet")
local fixed = require("p8.fixed")

local M = {}

M.PAD_TARGET_PERPENDICULAR_WEIGHT = 2

function M.pad_target_is_candidate(e)
  return (e ~= nil and not e.is_dead and planet.planet_is_family(e) and
    (e.state == entity_class.STATE_KNOWN or e.state == entity_class.STATE_SETTLED))
end

local function is_direction(direction)
  return direction == pad_direction.PAD_DIRECTION_UP or direction == pad_direction.PAD_DIRECTION_DOWN or
    direction == pad_direction.PAD_DIRECTION_LEFT or direction == pad_direction.PAD_DIRECTION_RIGHT
end

-- Returns a new { along, perpendicular } table on each call.
local function offset_in(direction, from, to)
  local dx = fixed.sub(to.x, from.x)
  local dy = fixed.sub(to.y, from.y)
  if direction == pad_direction.PAD_DIRECTION_RIGHT then
    return { along = dx, perpendicular = fixed.abs(dy) }
  elseif direction == pad_direction.PAD_DIRECTION_LEFT then
    return { along = -dx, perpendicular = fixed.abs(dy) }
  elseif direction == pad_direction.PAD_DIRECTION_DOWN then
    return { along = dy, perpendicular = fixed.abs(dx) }
  end
  return { along = -dy, perpendicular = fixed.abs(dx) }
end

function M.pad_target_pick(origin, direction, candidates)
  assert(is_direction(direction),
    "pad_target_pick: the direction is exactly one of PAD_DIRECTION_UP, DOWN, LEFT or RIGHT")
  assert(origin ~= nil and entity_class.has_map_pos(origin),
    "pad_target_pick: the origin is an entity with a map position")
  if candidates == nil then
    return nil
  end
  local from = entity_class.map_pos(origin)
  local best = nil
  local best_offset = { along = 0, perpendicular = 0 }
  local best_score = 0
  for i = 1, #candidates do
    local e = candidates[i]
    if e == origin then
      goto continue
    end
    assert(e ~= nil and entity_class.has_map_pos(e),
      "pad_target_pick: every candidate is an entity with a map position")
    do
      local offset = offset_in(direction, from, entity_class.map_pos(e))
      if offset.along <= 0 or offset.perpendicular > offset.along then
        goto continue
      end
      local score = fixed.add(offset.along, offset.perpendicular * M.PAD_TARGET_PERPENDICULAR_WEIGHT)
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
