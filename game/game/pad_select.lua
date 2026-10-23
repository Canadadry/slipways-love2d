local entity_class = require("engine.entity_class")
local pad_target = require("game.pad_target")
local cint = require("p8.cint")

local M = {}

M.PAD_SELECT_HOLD_FRAMES = 30
M.PAD_SELECT_TAP_FRAMES = 10
M.PAD_SELECT_CHAIN_FRAMES = 15

M.PAD_SELECT_NONE = 0
M.PAD_SELECT_TAP = 1
M.PAD_SELECT_YEAR = 2

local INT_MAX = 2147483647

function M.pad_select_init(s)
  s.held_frames = 0
  s.fired = false
  s.chain_frames = 0
end

function M.pad_select_step(s, down, allowed)
  local chain_open = s.chain_frames > 0
  if chain_open then
    s.chain_frames = s.chain_frames - 1
  end
  if not allowed then
    s.held_frames = 0
    s.fired = down
    return M.PAD_SELECT_NONE
  end
  if not down then
    local tap = s.held_frames > 0 and not s.fired and s.held_frames <= M.PAD_SELECT_TAP_FRAMES
    s.held_frames = 0
    s.fired = false
    return tap and M.PAD_SELECT_TAP or M.PAD_SELECT_NONE
  end
  if s.fired and s.held_frames == 0 then
    return M.PAD_SELECT_NONE
  end
  if s.held_frames < INT_MAX then
    s.held_frames = s.held_frames + 1
  end
  if not s.fired and (s.held_frames == M.PAD_SELECT_HOLD_FRAMES or (s.held_frames == 1 and chain_open)) then
    s.fired = true
    return M.PAD_SELECT_YEAR
  end
  return M.PAD_SELECT_NONE
end

function M.pad_select_year_ended(s)
  s.chain_frames = M.PAD_SELECT_CHAIN_FRAMES
end

function M.pad_select_year_just_ended(s)
  return s.chain_frames == M.PAD_SELECT_CHAIN_FRAMES
end

local function need_is_unmet(need)
  return need == nil or need.satisfied == nil or need.satisfied == 0
end

function M.pad_select_needs_attention(e)
  if not pad_target.pad_target_is_candidate(e) or e.state ~= entity_class.STATE_SETTLED then
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

function M.pad_select_next(candidates, current)
  assert(candidates ~= nil, "pad_select_next: the candidate list exists, even empty")
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
    if M.pad_select_needs_attention(e) then
      return e
    end
  end
  return nil
end

return M
