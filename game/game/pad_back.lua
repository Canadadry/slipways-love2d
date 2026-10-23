local entity_id = require("engine.entity_id")
local pad_target = require("game.pad_target")

local M = {}

-- last_acted is an entity id, nil for none.
function M.init(back)
  back.last_acted = nil
end

function M.record(back, e)
  if e ~= nil then
    back.last_acted = entity_id.link(e)
  end
end

function M.target(back, hovered)
  local e = entity_id.get(back.last_acted)
  if pad_target.is_candidate(e) and e ~= hovered then
    return e
  end
  return nil
end

return M
