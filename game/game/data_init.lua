local research = require("game.research")
local tech_gate = require("game.tech_gate")

local M = {}

function M.init()
  -- research.init() forgets the projects, so it runs before the build.
  research.init()
  research.build_projects()

  tech_gate.reset_affordable_flags()
end

function M.shutdown()
  research.shutdown()
end

return M
