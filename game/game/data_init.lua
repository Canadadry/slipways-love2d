local research = require("game.research")
local tech_gate = require("game.tech_gate")

local M = {}

function M.init()
  -- research_init() forgets the projects, so it runs before the build.
  research.research_init()
  research.research_build_projects()

  tech_gate.tech_gate_reset_affordable_flags()
end

function M.shutdown()
  research.research_shutdown()
end

return M
