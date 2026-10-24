local entity_class = require("engine.entity_class")
local entity_id = require("engine.entity_id")
local planet = require("game.planet")
local planet_family_trade = require("game.planet_family_trade")
local slipway_module = require("game.slipway")
local bit = require("bit")

local M = {}

M.NONE = 0
M.PRESSED = 1
M.HELD = 2
M.RELEASED = 3

M.REFUSED = 0
M.BUILT = 1

-- start and target are entity ids, nil for none.
function M.init(slipway)
  slipway.start = nil
  slipway.target = nil
  slipway.phase = M.NONE
  slipway.linked_before = false
end

function M.can_start(e)
  if e == nil or e.is_dead or not planet.is_family(e) or e.state ~= entity_class.STATE_SETTLED then
    return false
  end
  return bit.band(e.class.tags, entity_class.TAG(entity_class.TAG_SLIPGATE)) == 0 or
    planet_family_trade.slipgate_takes_slipway(e)
end

-- An empty array for a nil list.
function M.candidates(candidates, start)
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

function M.between(start, target)
  assert(start ~= nil and planet.is_family(start), "pad_slipway.between: only a planet holds a slipways list")
  local slipways = start.slipways
  for i = 1, #slipways do
    local slipway = entity_id.resolve(slipways[i])
    assert(not slipway.is_dead,
      "pad_slipway.between: a slipway dies only when its drop is refused, before it joins a planet's list")
    if slipway_module.other_end(slipway, start) == target then
      return slipway
    end
  end
  return nil
end

function M.outcome(start, target, linked_before)
  if linked_before or target == nil then
    return M.REFUSED
  end
  return M.between(start, target) ~= nil and M.BUILT or M.REFUSED
end

function M.release(slipway, linked_before)
  assert(slipway.phase == M.HELD and slipway.target ~= nil,
    "pad_slipway.release: only a held slipway aimed at a target is released")
  slipway.linked_before = linked_before
  slipway.phase = M.RELEASED
end

function M.rebase(slipway, outcome)
  assert(slipway.phase == M.RELEASED, "pad_slipway.rebase: only the frame after a release rebases")
  if outcome == M.BUILT then
    slipway.start = slipway.target
  end
  slipway.target = nil
  slipway.phase = M.PRESSED
end

return M
