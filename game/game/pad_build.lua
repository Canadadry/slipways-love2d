local entity_id = require("engine.entity_id")
local entity_registry = require("engine.entity_registry")
local pad_panel = require("game.pad_panel")
local structure_icon = require("game.structure_icon")

local M = {}

local INT_MAX = 2147483647

-- icon is an entity id, nil for none.
function M.pad_build_init(b)
  b.open = false
  b.icon = nil
end

function M.pad_build_open(b)
  b.open = true
end

function M.pad_build_close(b)
  b.open = false
end

function M.pad_build_is_open(b)
  return b.open
end

function M.pad_build_icon(b)
  local icon = entity_id.get(b.icon)
  return structure_icon.structure_icon_is_revealed(icon) and icon or nil
end

local function icon_beside(x, step)
  local nearest = nil
  local nearest_x = 0
  local bucket = entity_registry.entities_by_message(entity_registry.MESSAGE_HIT_TEST)
  for i = 1, #bucket do
    local e = bucket[i]
    if structure_icon.structure_icon_is_revealed(e) then
      local at_x = pad_panel.pad_panel_focus_point(e, 0, 0)
      if (step < 0 and at_x < x or step >= 0 and at_x > x) and
        (nearest == nil or (step < 0 and at_x > nearest_x or step >= 0 and at_x < nearest_x)) then
        nearest = e
        nearest_x = at_x
      end
    end
  end
  return nearest
end

function M.pad_build_focus(b)
  local icon = M.pad_build_icon(b)
  if icon == nil then
    icon = icon_beside(INT_MAX, -1)
    b.icon = entity_id.link(icon)
  end
  return icon
end

function M.pad_build_pick(b, step)
  local icon = M.pad_build_icon(b)
  if icon == nil then
    return
  end
  local x, y = pad_panel.pad_panel_focus_point(icon, 0, 0)
  local next_icon = icon_beside(x, step)
  if next_icon ~= nil then
    b.icon = entity_id.link(next_icon)
  end
end

return M
