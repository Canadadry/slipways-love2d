-- Copy of src/game/galaxy.h then src/game/galaxy.c.
--
-- Include cycle: game.planet and game.economy_score are required at call time, inside the
-- functions that need them, so that this module loads before they exist.
local entity_class = require("engine.entity_class")
local entity_geom = require("engine.entity_geom")
local entity_registry = require("engine.entity_registry")
local vec = require("engine.vec")
local gen = require("game.gen")
local fixed = require("p8.fixed")
local str = require("p8.str")

local M = {}

-- Planets lie in [margin, margin + span) on both axes, in map units; a planet's position is its map position times the scale.
M.GALAXY_MAP_MARGIN = fixed.P8_FIXED_LIT(0x00002000, 0.125)
M.GALAXY_MAP_SPAN = fixed.P8_FIXED_LIT(0x0002c000, 2.75)
M.GALAXY_MAP_SCALE = 128

function M.galaxy_populate(planet_count)
  local planet = require("game.planet")
  local fixed_one = fixed.p8_fixed_from_int(1)
  local threshold = fixed.P8_FIXED_LIT(0x00000ccc, 0.05)
  local planet_id

  planet_id = fixed_one
  while planet_id <= planet_count do
    local p_vec
    local id_num_str
    local keys = planet.planet_keys_zero()
    local made

    -- Not an inert loop: every accepted planet registers under the planet tag, so later candidates are rejected against it.
    repeat
      local rx = fixed.p8_fixed_add(fixed.p8_fixed_rnd(M.GALAXY_MAP_SPAN), M.GALAXY_MAP_MARGIN)
      local ry = fixed.p8_fixed_add(fixed.p8_fixed_rnd(M.GALAXY_MAP_SPAN), M.GALAXY_MAP_MARGIN)
      p_vec = vec.vec_make(rx, ry)
    until entity_geom.entity_geom_any_within(p_vec, entity_registry.entities_by_tag(entity_class.TAG_PLANET),
      threshold) == 0

    id_num_str = str.p8_tostr_fixed(planet_id)
    planet.planet_keys_set_id(keys, str.p8_concat2("p", id_num_str))
    keys.map_pos = p_vec
    keys.pos = vec.vec_mul(p_vec, fixed.p8_fixed_from_int(M.GALAXY_MAP_SCALE))

    made = planet.planet_new(keys)
    planet_id = fixed.p8_fixed_add(planet_id, fixed_one)
  end

  M.galaxy_install_rolls()
end

-- The type picker and vanish walk of a new game, for the current difficulty.
-- Draws nothing from the RNG: a resume builds them the same way.
function M.galaxy_install_rolls()
  local planet = require("game.planet")
  local difficulty = require("game.economy_score").economy_score_difficulty()
  local difficulty_type_bias = fixed.p8_fixed_from_int(0)
  local type_picker_random
  local type_picker_state
  local vanish_roll_state

  if difficulty ~= nil and difficulty.type_bias ~= nil then
    difficulty_type_bias = difficulty.type_bias
  end

  type_picker_random = gen.random_walk_new(fixed.P8_FIXED_LIT(0x00008000, 0.5), difficulty_type_bias)
  type_picker_state = gen.weighted_picker_new(fixed.p8_fixed_from_int(2), gen.random_walk_draw,
    type_picker_random)
  planet.planet_set_type_picker(type_picker_state)

  vanish_roll_state = gen.random_walk_new(fixed.P8_FIXED_LIT(0x0000cccc, 0.8),
    fixed.P8_FIXED_LIT(0x00008000, 0.5))
  planet.planet_set_vanish_roll(vanish_roll_state)
end

return M
