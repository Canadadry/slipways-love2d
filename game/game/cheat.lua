-- Copy of src/game/cheat.h then src/game/cheat.c.
--
-- Shapes, with the C field names:
--   cheat_t: { previous_first, previous_second, chords }.
local probe = require("game.probe")
local fixed = require("p8.fixed")
local input = require("p8.input")
local bit = require("bit")

local M = {}

M.CHEAT_CHORDS = 4

local CHORD_FIRST = input.P8_PAD_BUTTON_5
local CHORD_SECOND = input.P8_PAD_BUTTON_6

function M.cheat_init(cheat)
  cheat.previous_first = 0
  cheat.previous_second = 0
  cheat.chords = 0
end

local function apply()
  local fm = probe.main_economy()
  local hy
  if fm == nil then
    return
  end
  if fm.class == probe.main_economy_placeholder_class() then
    local provisional = fm
    provisional.research_points = fixed.p8_fixed_from_int(99)
    provisional.has_research_points = 1
    provisional.credits = fixed.p8_fixed_from_int(200)
    provisional.has_credits = 1
    return
  end
  hy = probe.main_economy_typed()
  hy.research_points = fixed.p8_fixed_from_int(99)
  hy.credits = fixed.p8_fixed_from_int(200)
end

function M.cheat_chord_buttons()
  return bit.bor(CHORD_FIRST, CHORD_SECOND)
end

function M.cheat_step(cheat, pad_buttons)
  local first = (bit.band(pad_buttons, CHORD_FIRST) ~= 0) and 1 or 0
  local second = (bit.band(pad_buttons, CHORD_SECOND) ~= 0) and 1 or 0
  local first_edge = (first ~= 0 and cheat.previous_first == 0) and 1 or 0
  local second_edge = (second ~= 0 and cheat.previous_second == 0) and 1 or 0

  cheat.previous_first = first
  cheat.previous_second = second

  if first_edge == 0 and second_edge == 0 then
    return 0
  end
  if not (first_edge ~= 0 and second_edge ~= 0) then
    cheat.chords = 0
    return 0
  end
  cheat.chords = cheat.chords + 1
  if cheat.chords < M.CHEAT_CHORDS then
    return 0
  end
  cheat.chords = 0
  apply()
  return 1
end

return M
