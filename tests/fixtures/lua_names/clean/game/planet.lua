local fixed = require("p8.fixed") -- fixed.not_a_read in a comment
local M = {}

local ZERO = fixed.from_int(0)
local label = "fixed.not_a_read either"

function M.init_instance(p)
    local fixed_again = require("p8.fixed")
    p.x = fixed_again.add(ZERO, fixed:from_int(1))
    p.y = require("p8.fixed").ONE
    M.g_count = (M.g_count or 0) + 1
    return label .. p.planet_type
end

return M
