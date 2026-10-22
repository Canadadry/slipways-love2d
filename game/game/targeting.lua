-- Copy of src/game/targeting.h, which has no .c: its enum as fields, plus
-- the Lua-only targeting_zero. Its functions are defined in src/game/probe.c
-- and ported with it.
--
-- Shapes, with the C field names:
--   targeting_target_t: { kind, id }.
--   targeting_t: { archetype, target, cost, has_cost }.
--   targeting_track_target_t: the entity header plus targeted_by, one table.
local entity_id_type = require("engine.entity_id_type")

local M = {}

M.TARGETING_TARGET_NONE = 0
M.TARGETING_TARGET_ENTITY = 1
M.TARGETING_TARGET_TRACK_THROWAWAY = 2
M.TARGETING_TARGET_SLIPWAY_TOO_FAR = 3
M.TARGETING_TARGET_WORMHOLE_TOO_FAR = 4

-- Lua only: targeting_t at its C zero, the inline sub-struct later code writes field by field.
function M.targeting_zero()
  local none = entity_id_type.ENTITY_ID_NONE
  return {
    archetype = nil,
    target = { kind = M.TARGETING_TARGET_NONE, id = { index = none.index, generation = none.generation } },
    cost = 0,
    has_cost = 0,
  }
end

return M
