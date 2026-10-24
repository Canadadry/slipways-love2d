-- LÖVE window units, y growing downward. No module state: the caller holds
-- touch_state.
local bit = require("bit")
local input = require("p8.input")
local pad_stick = require("love.pad_stick")

local M = {}

M.BUTTON_MASKS = {
  a = input.PAD_BUTTON_5,
  b = input.PAD_BUTTON_6,
  x = input.PAD_BUTTON_7,
  y = input.PAD_BUTTON_8,
  leftshoulder = input.PAD_BUTTON_9,
  rightshoulder = input.PAD_BUTTON_10,
  start = input.PAD_BUTTON_11,
  back = input.PAD_BUTTON_12,
}

-- touch_state.stick is the pad_stick table of the stick touch, nil without
-- one; stick_id its touch's id; held is private.
function M.init(touch_state)
  assert(type(touch_state) == "table",
    string.format("pad_touch.init: touch_state must be a table, got %s", tostring(touch_state)))
  touch_state.stick = nil
  touch_state.stick_id = nil
  touch_state.held = {}
end

local function inside(rectangle, x, y)
  return rectangle.x <= x and x < rectangle.x + rectangle.width
    and rectangle.y <= y and y < rectangle.y + rectangle.height
end

local function buttons_under(buttons, x, y)
  local mask = 0
  for name, rectangle in pairs(buttons) do
    if inside(rectangle, x, y) then
      mask = bit.bor(mask, M.BUTTON_MASKS[name])
    end
  end
  return mask
end

-- A table of its own, so no touch id can equal it.
local MOUSE_ID = {}

local function refuse_unless(condition, field, expected, value)
  assert(condition, string.format("pad_touch.update: %s must be %s, got %s", field, expected, tostring(value)))
end

local function check_touch(record, index, seen)
  local field = string.format("touches[%d]", index)
  refuse_unless(type(record) == "table", field, "a table", record)
  refuse_unless(record.id ~= nil, field .. ".id", "non-nil", record.id)
  refuse_unless(not seen[record.id], field .. ".id", "unique", record.id)
  refuse_unless(type(record.x) == "number", field .. ".x", "a number", record.x)
  refuse_unless(type(record.y) == "number", field .. ".y", "a number", record.y)
  seen[record.id] = true
end

local function check_mouse(mouse)
  if mouse == nil then
    return
  end
  refuse_unless(type(mouse) == "table", "mouse", "nil or a table", mouse)
  refuse_unless(type(mouse.x) == "number", "mouse.x", "a number", mouse.x)
  refuse_unless(type(mouse.y) == "number", "mouse.y", "a number", mouse.y)
  refuse_unless(type(mouse.down) == "boolean", "mouse.down", "a boolean", mouse.down)
end

local function check_placed(placed)
  refuse_unless(type(placed) == "table", "placed", "a table", placed)
  refuse_unless(type(placed.stick_zone) == "table", "placed.stick_zone", "a table", placed.stick_zone)
  refuse_unless(type(placed.buttons) == "table", "placed.buttons", "a table", placed.buttons)
  refuse_unless(type(placed.stick_base_size) == "number", "placed.stick_base_size", "a number", placed.stick_base_size)
end

-- touches then the mouse while its button is down; ids checked unique.
local function frame_touches(touches, mouse)
  refuse_unless(type(touches) == "table", "touches", "a table", touches)
  check_mouse(mouse)
  local frame = {}
  local seen = {}
  for index, record in ipairs(touches) do
    check_touch(record, index, seen)
    frame[index] = record
  end
  if mouse ~= nil and mouse.down then
    frame[#frame + 1] = { id = MOUSE_ID, x = mouse.x, y = mouse.y }
  end
  return frame
end

local function follow_or_lift(touch_state, frame)
  for _, record in ipairs(frame) do
    if record.id == touch_state.stick_id then
      pad_stick.follow(touch_state.stick, record.x, record.y)
      return
    end
  end
  touch_state.stick = nil
  touch_state.stick_id = nil
end

-- Only a touch first seen this frame claims: one that landed on a button, or
-- while another finger held the stick, never becomes the stick later.
local function claim(touch_state, frame, placed)
  for _, record in ipairs(frame) do
    if not touch_state.held[record.id] and inside(placed.stick_zone, record.x, record.y)
      and buttons_under(placed.buttons, record.x, record.y) == 0 then
      touch_state.stick = pad_stick.start(record.x, record.y, placed.stick_base_size)
      touch_state.stick_id = record.id
      pad_stick.follow(touch_state.stick, record.x, record.y)
      return
    end
  end
end

-- Returns the virtual mask; the stick's touch never presses a button.
function M.update(touch_state, touches, mouse, placed)
  refuse_unless(type(touch_state) == "table" and type(touch_state.held) == "table",
    "touch_state", "a table passed to init", touch_state)
  check_placed(placed)
  local frame = frame_touches(touches, mouse)
  follow_or_lift(touch_state, frame)
  if touch_state.stick == nil then
    claim(touch_state, frame, placed)
  end
  local mask = touch_state.stick ~= nil and touch_state.stick.mask or 0
  local held = {}
  for _, record in ipairs(frame) do
    if record.id ~= touch_state.stick_id then
      mask = bit.bor(mask, buttons_under(placed.buttons, record.x, record.y))
    end
    held[record.id] = true
  end
  touch_state.held = held
  return mask
end

local ALL_BUTTONS = 4095

local function check_mask(value, argument)
  assert(type(value) == "number" and value >= 0 and value <= ALL_BUTTONS and value == math.floor(value),
    string.format("pad_touch.combine: %s must be an integer 0-4095, got %s", argument, tostring(value)))
end

function M.combine(gamepad_mask, virtual_mask)
  check_mask(gamepad_mask, "gamepad_mask")
  check_mask(virtual_mask, "virtual_mask")
  return bit.bor(gamepad_mask, virtual_mask)
end

return M
