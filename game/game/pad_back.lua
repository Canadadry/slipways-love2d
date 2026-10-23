local entity_id = require("engine.entity_id")
local pad_target = require("game.pad_target")

local M = {}

-- last_acted is an entity id, nil for none.
function M.pad_back_init(back)
  back.last_acted = nil
end

function M.pad_back_record(back, e)
  if e ~= nil then
    back.last_acted = entity_id.entity_id_link(e)
  end
end

function M.pad_back_target(back, hovered)
  local e = entity_id.entity_get(back.last_acted)
  if pad_target.pad_target_is_candidate(e) and e ~= hovered then
    return e
  end
  return nil
end

return M
