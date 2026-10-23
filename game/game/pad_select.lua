local entity_class = require("engine.entity_class")
local pad_target = require("game.pad_target")
local cint = require("p8.cint")

local M = {}

M.HOLD_FRAMES = 30
M.TAP_FRAMES = 10
M.CHAIN_FRAMES = 15

M.NONE = 0
M.TAP = 1
M.YEAR = 2

local INT_MAX = 2147483647

function M.init(s)
  s.held_frames = 0
  s.fired = false
  s.chain_frames = 0
end

function M.step(s, down, allowed)
  local chain_open = s.chain_frames > 0
  if chain_open then
    s.chain_frames = s.chain_frames - 1
  end
  if not allowed then
    s.held_frames = 0
    s.fired = down
    return M.NONE
  end
  if not down then
    local tap = s.held_frames > 0 and not s.fired and s.held_frames <= M.TAP_FRAMES
    s.held_frames = 0
    s.fired = false
    return tap and M.TAP or M.NONE
  end
  if s.fired and s.held_frames == 0 then
    return M.NONE
  end
  if s.held_frames < INT_MAX then
    s.held_frames = s.held_frames + 1
  end
  if not s.fired and (s.held_frames == M.HOLD_FRAMES or (s.held_frames == 1 and chain_open)) then
    s.fired = true
    return M.YEAR
  end
  return M.NONE
end

function M.year_ended(s)
  s.chain_frames = M.CHAIN_FRAMES
end

function M.year_just_ended(s)
  return s.chain_frames == M.CHAIN_FRAMES
end

local function need_is_unmet(need)
  return need == nil or need.satisfied == nil or need.satisfied == 0
end

function M.needs_attention(e)
  if not pad_target.is_candidate(e) or e.state ~= entity_class.STATE_SETTLED then
    return false
  end
  local planet = e
  if planet.needs == nil then
    return false
  end
  for i = 1, #planet.needs do
    if need_is_unmet(planet.needs[i]) then
      return true
    end
  end
  return false
end

function M.next(candidates, current)
  assert(candidates ~= nil, "pad_select.next: the candidate list exists, even empty")
  local start = -1
  for i = 1, #candidates do
    if current == nil then
      break
    end
    if candidates[i] == current then
      start = i - 1
      break
    end
  end
  for step = 1, #candidates do
    local e = candidates[cint.mod(start + step, #candidates) + 1]
    if M.needs_attention(e) then
      return e
    end
  end
  return nil
end

return M
