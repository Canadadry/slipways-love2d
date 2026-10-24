local M = {}

M.TARGET_NONE = 0
M.TARGET_ENTITY = 1
M.TARGET_TRACK_THROWAWAY = 2
M.TARGET_SLIPWAY_TOO_FAR = 3
M.TARGET_WORMHOLE_TOO_FAR = 4

-- A new table; cost stays nil when the targeting has none.
function M.zero()
  return {
    archetype = nil,
    target = { kind = M.TARGET_NONE, id = nil },
  }
end

return M
