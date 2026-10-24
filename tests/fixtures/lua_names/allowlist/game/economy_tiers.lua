local M = {}

function M.compute(p)
    local planet = require("game.planet")
    planet.init_instance(p)
end

function M.tier(planet)
    return planet.needs, planet.needz
end

return M
