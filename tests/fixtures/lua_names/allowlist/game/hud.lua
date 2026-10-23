local M = {}

function M.draw(planet)
    local planet_module = require("game.planet")
    local planet = require("game.planet")
    planet_module.init_instance(planet)
    return planet.needs
end

return M
