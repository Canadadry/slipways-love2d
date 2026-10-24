local M = {}

local function rectangle(x, y, width, height)
  return { x = x, y = y, width = width, height = height }
end

M.STICK_ZONE = rectangle(0, 0, 80, 120)
M.STICK_BASE_SIZE = 48
M.STICK_KNOB_SIZE = 24

M.BUTTONS = {
  leftshoulder = rectangle(4, 4, 24, 24),
  rightshoulder = rectangle(132, 4, 24, 24),
  y = rectangle(108, 36, 24, 24),
  x = rectangle(84, 60, 24, 24),
  b = rectangle(132, 60, 24, 24),
  a = rectangle(108, 84, 24, 24),
  back = rectangle(54, 94, 24, 24),
  start = rectangle(82, 94, 24, 24),
}

local function scale_rectangle(reference, pad)
  return rectangle(pad.x + reference.x * pad.scale, pad.y + reference.y * pad.scale,
    reference.width * pad.scale, reference.height * pad.scale)
end

local function check_number(value, field)
  assert(type(value) == "number",
    string.format("pad_layout.place: pad.%s must be a number, got %s", field, tostring(value)))
end

function M.place(pad)
  check_number(pad.x, "x")
  check_number(pad.y, "y")
  assert(type(pad.scale) == "number" and pad.scale > 0,
    string.format("pad_layout.place: pad.scale must be a number > 0, got %s", tostring(pad.scale)))
  local buttons = {}
  for name, button in pairs(M.BUTTONS) do
    buttons[name] = scale_rectangle(button, pad)
  end
  return {
    stick_zone = scale_rectangle(M.STICK_ZONE, pad),
    stick_base_size = M.STICK_BASE_SIZE * pad.scale,
    stick_knob_size = M.STICK_KNOB_SIZE * pad.scale,
    buttons = buttons,
  }
end

return M
