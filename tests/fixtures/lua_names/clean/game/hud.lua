local planet = require("game.planet")
local M = {}

function M.draw(p)
    planet.init_instance(p)
    return planet.g_count
end

return M
