local M = {}

M.TARGETING_TARGET_NONE = 0
M.TARGETING_TARGET_ENTITY = 1
M.TARGETING_TARGET_TRACK_THROWAWAY = 2
M.TARGETING_TARGET_SLIPWAY_TOO_FAR = 3
M.TARGETING_TARGET_WORMHOLE_TOO_FAR = 4

-- A new table; cost stays nil when the targeting has none.
function M.targeting_zero()
  return {
    archetype = nil,
    target = { kind = M.TARGETING_TARGET_NONE, id = nil },
  }
end

return M
