local fixed = require("p8.fixed")
local M = {}

local ZERO = fixed.from_intx(0)
local ONE = fixed.ONE_
local STILL_ONE = fixed.ONE

function M.init_instance(p)
    local fixed_late = require("p8.fixed")
    p.x = fixed_late.ad(ZERO, ONE)
    p.y = require("p8.fixed"):from_int(STILL_ONE)
    p.z = require("p8.fixed").ONEE
    return M.helper(p)
end

return M
