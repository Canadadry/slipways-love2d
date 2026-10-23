-- Copy of src/game/pad_select.h then src/game/pad_select.c.
--
-- Shapes, with the C field names:
--   pad_select_t: { held_frames, fired, chain_frames }.
local entity_class = require("engine.entity_class")
local pad_target = require("game.pad_target")
local planet_module = require("game.planet")
local cint = require("p8.cint")
local bit = require("bit")

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
  s.fired = 0
  s.chain_frames = 0
end

function M.pad_select_step(s, down, allowed)
  local chain_open = s.chain_frames > 0
  if chain_open then
    s.chain_frames = s.chain_frames - 1
  end
  if allowed == 0 then
    s.held_frames = 0
    s.fired = down
    return M.PAD_SELECT_NONE
  end
  if down == 0 then
    local tap = s.held_frames > 0 and s.fired == 0 and s.held_frames <= M.PAD_SELECT_TAP_FRAMES
    s.held_frames = 0
    s.fired = 0
    return tap and M.PAD_SELECT_TAP or M.PAD_SELECT_NONE
  end
  if s.fired ~= 0 and s.held_frames == 0 then
    return M.PAD_SELECT_NONE
  end
  if s.held_frames < INT_MAX then
    s.held_frames = s.held_frames + 1
  end
  if s.fired == 0 and (s.held_frames == M.PAD_SELECT_HOLD_FRAMES or (s.held_frames == 1 and chain_open)) then
    s.fired = 1
    return M.PAD_SELECT_YEAR
  end
  return M.PAD_SELECT_NONE
end

function M.pad_select_year_ended(s)
  s.chain_frames = M.PAD_SELECT_CHAIN_FRAMES
end

function M.pad_select_year_just_ended(s)
  return (s.chain_frames == M.PAD_SELECT_CHAIN_FRAMES) and 1 or 0
end

local function need_is_unmet(need)
  return (need == nil or need.satisfied == nil or need.satisfied == 0) and 1 or 0
end

function M.pad_select_needs_attention(e)
  if pad_target.pad_target_is_candidate(e) == 0 or e.state ~= entity_class.ENTITY_STATE_SETTLED then
    return 0
  end
  local planet = e
  if bit.band(planet.has_, planet_module.PLANET_HAS_NEEDS) == 0 then
    return 0
  end
  for i = 1, #planet.needs do
    if need_is_unmet(planet.needs[i]) ~= 0 then
      return 1
    end
  end
  return 0
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
    if M.pad_select_needs_attention(e) ~= 0 then
      return e
    end
  end
  return nil
end

return M
