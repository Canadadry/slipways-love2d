local probe = require("game.probe")
local fixed = require("p8.fixed")
local input = require("p8.input")
local bit = require("bit")

local M = {}

M.CHORDS = 4

local CHORD_FIRST = input.PAD_BUTTON_5
local CHORD_SECOND = input.PAD_BUTTON_6

function M.init(cheat)
  cheat.previous_first = false
  cheat.previous_second = false
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
    provisional.research_points = fixed.from_int(99)
    provisional.credits = fixed.from_int(200)
    return
  end
  hy = probe.main_economy_typed()
  hy.research_points = fixed.from_int(99)
  hy.credits = fixed.from_int(200)
end

function M.chord_buttons()
  return bit.bor(CHORD_FIRST, CHORD_SECOND)
end

function M.step(cheat, pad_buttons)
  local first = bit.band(pad_buttons, CHORD_FIRST) ~= 0
  local second = bit.band(pad_buttons, CHORD_SECOND) ~= 0
  local first_edge = first and not cheat.previous_first
  local second_edge = second and not cheat.previous_second

  cheat.previous_first = first
  cheat.previous_second = second

  if not first_edge and not second_edge then
    return false
  end
  if not (first_edge and second_edge) then
    cheat.chords = 0
    return false
  end
  cheat.chords = cheat.chords + 1
  if cheat.chords < M.CHORDS then
    return false
  end
  cheat.chords = 0
  apply()
  return true
end

return M
