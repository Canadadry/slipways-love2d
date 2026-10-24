local game_data = require("game.game_data")
local image = require("p8.image")

local M = {}

-- Every p8 image slot the game uses, 0-based handles; row cache slot k (from 1) is
-- image_slot.ROW_FIRST + k - 1, text and button cache slots follow the same rule, ring slot
-- image_slot.PLANET_RING_FIRST + k - 1 holds ring colour k.
M.BACKGROUND = 0
M.ROW_FIRST = 1
M.ROW_COUNT = 32
M.TEXT_FIRST = M.ROW_FIRST + M.ROW_COUNT
M.TEXT_COUNT = 32
M.PLANET_DISC = M.TEXT_FIRST + M.TEXT_COUNT
M.PLANET_RING_FIRST = M.PLANET_DISC + 1
M.BUTTON_ICON_FIRST = M.PLANET_RING_FIRST + game_data.PLANET_RING_COLORS_COUNT
M.BUTTON_ICON_COUNT = 8
M.BUTTON_WIDE_FIRST = M.BUTTON_ICON_FIRST + M.BUTTON_ICON_COUNT
M.BUTTON_WIDE_COUNT = 10
M.COUNT = M.BUTTON_WIDE_FIRST + M.BUTTON_WIDE_COUNT

assert(M.COUNT == image.SLOT_COUNT, "image_slot_count_matches_the_runtime")

return M
