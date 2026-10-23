-- Copy of src/game/pad_slipway.h then src/game/pad_slipway.c.
--
-- Shapes, with the C field names:
--   pad_slipway_t: { start, target, phase, linked_before }, start and target entity ids (nil for none).
local entity_class = require("engine.entity_class")
local entity_id = require("engine.entity_id")
local planet = require("game.planet")
local planet_family_trade = require("game.planet_family_trade")
local slipway_module = require("game.slipway")
local bit = require("bit")

local M = {}

M.PAD_SLIPWAY_NONE = 0
M.PAD_SLIPWAY_PRESSED = 1
M.PAD_SLIPWAY_HELD = 2
M.PAD_SLIPWAY_RELEASED = 3

M.PAD_SLIPWAY_REFUSED = 0
M.PAD_SLIPWAY_BUILT = 1

function M.pad_slipway_init(slipway)
  slipway.start = nil
  slipway.target = nil
  slipway.phase = M.PAD_SLIPWAY_NONE
  slipway.linked_before = 0
end

function M.pad_slipway_can_start(e)
  if e == nil or e.is_dead ~= 0 or planet.planet_is_family(e) == 0 or e.state ~= entity_class.ENTITY_STATE_SETTLED then
    return 0
  end
  return (bit.band(e.class.tags, entity_class.TAG(entity_class.TAG_SLIPGATE)) == 0 or
    planet_family_trade.slipgate_takes_slipway(e) ~= 0) and 1 or 0
end

-- An empty array for a nil list.
function M.pad_slipway_candidates(candidates, start)
  local targets = {}
  if candidates == nil then
    return targets
  end
  for i = 1, #candidates do
    if candidates[i] ~= start then
      targets[#targets + 1] = candidates[i]
    end
  end
  return targets
end

function M.pad_slipway_between(start, target)
  assert(start ~= nil and planet.planet_is_family(start) ~= 0, "pad_slipway_between: only a planet holds a slipways list")
  local slipways = start.slipways
  for i = 1, #slipways do
    local slipway = entity_id.entity_id_resolve(slipways[i])
    assert(slipway.is_dead == 0,
      "pad_slipway_between: a slipway dies only when its drop is refused, before it joins a planet's list")
    if slipway_module.slipway_other_end(slipway, start) == target then
      return slipway
    end
  end
  return nil
end

function M.pad_slipway_outcome(start, target, linked_before)
  if linked_before ~= 0 or target == nil then
    return M.PAD_SLIPWAY_REFUSED
  end
  return M.pad_slipway_between(start, target) ~= nil and M.PAD_SLIPWAY_BUILT or M.PAD_SLIPWAY_REFUSED
end

function M.pad_slipway_release(slipway, linked_before)
  assert(slipway.phase == M.PAD_SLIPWAY_HELD and slipway.target ~= nil,
    "pad_slipway_release: only a held slipway aimed at a target is released")
  slipway.linked_before = linked_before
  slipway.phase = M.PAD_SLIPWAY_RELEASED
end

function M.pad_slipway_rebase(slipway, outcome)
  assert(slipway.phase == M.PAD_SLIPWAY_RELEASED, "pad_slipway_rebase: only the frame after a release rebases")
  if outcome == M.PAD_SLIPWAY_BUILT then
    slipway.start = slipway.target
  end
  slipway.target = nil
  slipway.phase = M.PAD_SLIPWAY_PRESSED
end

return M
