-- LÖVE screen units, y growing downward. The stick table is the only state.
local M = {}

local function check_number(value, caller, argument)
  assert(type(value) == "number",
    string.format("pad_stick.%s: %s must be a number, got %s", caller, argument, tostring(value)))
end

-- base_size: the placed stick_base_size of pad_layout.place, a diameter.
function M.start(x, y, base_size)
  check_number(x, "start", "x")
  check_number(y, "start", "y")
  assert(type(base_size) == "number" and base_size > 0,
    string.format("pad_stick.start: base_size must be a number > 0, got %s", tostring(base_size)))
  return { base_x = x, base_y = y, radius = base_size / 2, knob_x = x, knob_y = y, mask = 0 }
end

-- The touch may be anywhere on the screen; the knob's centre stops on the
-- base's edge.
function M.follow(stick, x, y)
  assert(type(stick) == "table",
    string.format("pad_stick.follow: stick must be a table, got %s", tostring(stick)))
  check_number(x, "follow", "x")
  check_number(y, "follow", "y")
  local dx = x - stick.base_x
  local dy = y - stick.base_y
  local distance_squared = dx * dx + dy * dy
  local radius = stick.radius
  if distance_squared <= radius * radius then
    stick.knob_x = x
    stick.knob_y = y
  else
    local shrink = radius / math.sqrt(distance_squared)
    stick.knob_x = stick.base_x + dx * shrink
    stick.knob_y = stick.base_y + dy * shrink
  end
end

return M
