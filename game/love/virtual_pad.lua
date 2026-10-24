local pad_layout = require("love.pad_layout")
local pad_touch = require("love.pad_touch")
local pad_visibility = require("love.pad_visibility")
local window_layout = require("love.window_layout")

local M = {}

local function check_state(virtual_pad, caller)
  if type(virtual_pad) ~= "table" or virtual_pad.touch_state == nil then
    assert(false, string.format("virtual_pad.%s: virtual_pad must be a table passed to init, got %s",
      caller, tostring(virtual_pad)))
  end
end

-- Readable: placed, this frame's pad_layout.place result (nil before the first
-- update), touch_state.stick, and virtual_mask, the touches' mask without the
-- gamepad's (0 before the first update). visibility is private: callers read shown.
function M.init(virtual_pad)
  virtual_pad.visibility = {}
  pad_visibility.init(virtual_pad.visibility)
  virtual_pad.touch_state = {}
  pad_touch.init(virtual_pad.touch_state)
  virtual_pad.placed = nil
  virtual_pad.virtual_mask = 0
end

function M.update(virtual_pad, gamepad_mask, touches, mouse, width, height)
  check_state(virtual_pad, "update")
  local placed = pad_layout.place(window_layout.compute(width, height).pad)
  local passed, passed_mouse = pad_visibility.filter(virtual_pad.visibility, gamepad_mask, touches, mouse)
  local virtual = pad_touch.update(virtual_pad.touch_state, passed, passed_mouse, placed)
  virtual_pad.placed = placed
  virtual_pad.virtual_mask = virtual
  return pad_touch.combine(gamepad_mask, virtual)
end

function M.shown(virtual_pad)
  check_state(virtual_pad, "shown")
  return pad_visibility.shown(virtual_pad.visibility)
end

return M
