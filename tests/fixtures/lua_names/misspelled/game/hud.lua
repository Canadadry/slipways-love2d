local screen = require("p8.vram")
local M = {}

function M.draw()
    local screen = require("p8.fixed")
    return screen.ONE
end

return M
