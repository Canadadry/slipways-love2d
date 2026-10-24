-- A filter in front of pad_touch.update: a touch it holds back is absent from
-- what it returns, so pad_touch sees it lifted. No module state: the caller
-- holds visibility.
local bit = require("bit")

local M = {}

local function refuse_unless(condition, caller, field, expected, value)
  assert(condition, string.format("pad_visibility.%s: %s must be %s, got %s", caller, field, expected, tostring(value)))
end

local function check_visibility(visibility, caller)
  refuse_unless(type(visibility) == "table" and type(visibility.held_back) == "table",
    caller, "visibility", "a table passed to init", visibility)
end

-- The fields are private: callers read shown.
function M.init(visibility)
  refuse_unless(type(visibility) == "table", "init", "visibility", "a table", visibility)
  visibility.visible = false
  visibility.held_back = {}
  visibility.mouse_held_back = false
  visibility.previous_gamepad_mask = 0
end

function M.shown(visibility)
  check_visibility(visibility, "shown")
  return visibility.visible
end

local function forget_lifted(visibility, touches, mouse)
  local present = {}
  for _, record in ipairs(touches) do
    present[record.id] = true
  end
  for id in pairs(visibility.held_back) do
    if not present[id] then
      visibility.held_back[id] = nil
    end
  end
  if mouse == nil or not mouse.down then
    visibility.mouse_held_back = false
  end
end

-- Returns whether a touch or the mouse was not held back yet.
local function hold_back_all(visibility, touches, mouse)
  local any = false
  for _, record in ipairs(touches) do
    if not visibility.held_back[record.id] then
      visibility.held_back[record.id] = true
      any = true
    end
  end
  if mouse ~= nil and mouse.down and not visibility.mouse_held_back then
    visibility.mouse_held_back = true
    any = true
  end
  return any
end

local ALL_BUTTONS = 4095

local function check_input(gamepad_mask, touches, mouse)
  refuse_unless(type(gamepad_mask) == "number" and gamepad_mask >= 0 and gamepad_mask <= ALL_BUTTONS
    and gamepad_mask == math.floor(gamepad_mask), "filter", "gamepad_mask", "an integer 0-4095", gamepad_mask)
  refuse_unless(type(touches) == "table", "filter", "touches", "a table", touches)
  local seen = {}
  for index, record in ipairs(touches) do
    local field = string.format("touches[%d]", index)
    refuse_unless(type(record) == "table", "filter", field, "a table", record)
    refuse_unless(record.id ~= nil, "filter", field .. ".id", "non-nil", record.id)
    refuse_unless(not seen[record.id], "filter", field .. ".id", "unique", record.id)
    seen[record.id] = true
  end
  if mouse ~= nil then
    refuse_unless(type(mouse) == "table", "filter", "mouse", "nil or a table", mouse)
    refuse_unless(type(mouse.down) == "boolean", "filter", "mouse.down", "a boolean", mouse.down)
  end
end

-- Returns the touches and the mouse pad_touch may see; a hidden pad passes
-- nothing, not even a mouse with its button up.
function M.filter(visibility, gamepad_mask, touches, mouse)
  check_visibility(visibility, "filter")
  check_input(gamepad_mask, touches, mouse)
  forget_lifted(visibility, touches, mouse)
  if bit.band(gamepad_mask, bit.bnot(visibility.previous_gamepad_mask)) ~= 0 then
    visibility.visible = false
    hold_back_all(visibility, touches, mouse)
  elseif not visibility.visible and hold_back_all(visibility, touches, mouse) then
    visibility.visible = true
  end
  visibility.previous_gamepad_mask = gamepad_mask
  local passed = {}
  for _, record in ipairs(touches) do
    if not visibility.held_back[record.id] then
      passed[#passed + 1] = record
    end
  end
  if not visibility.visible or visibility.mouse_held_back then
    return passed, nil
  end
  return passed, mouse
end

return M
