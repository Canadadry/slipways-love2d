-- The visual stream: random draws for on-screen-only effects. No game
-- decision may read it, and the save does not hold it. It is seeded once at
-- boot and never reseeded.
local fixed = require("p8.fixed")

local M = {}

local VISUAL_RANDOM_BOOT_SEED = 0x9E3779B9

-- A number in 0 .. 2^32 - 1, never a signed bit result.
local visual_state = VISUAL_RANDOM_BOOT_SEED

function M.visual_random_draw(x)
  local result
  result, visual_state = fixed.p8_fixed_rnd_from_state(visual_state, x)
  return result
end

function M.visual_random_draw_one()
  local result
  result, visual_state = fixed.p8_fixed_rnd_from_state(visual_state, fixed.P8_FIXED_INT(1))
  return result
end

function M.visual_random_state()
  return visual_state
end

function M.visual_random_set_state(state)
  visual_state = state
end

return M
