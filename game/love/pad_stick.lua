local M = {}

-- p8.input's PAD_BUTTON_1 to PAD_BUTTON_4 (dpup, dpdown, dpleft, dpright): pad_touch ORs the mask into pad_buttons.
M.UP = 1
M.DOWN = 2
M.LEFT = 4
M.RIGHT = 8

local function horizontal(dx)
  return dx > 0 and M.RIGHT or M.LEFT
end

local function vertical(dy)
  return dy < 0 and M.UP or M.DOWN
end

local function direction(dx, dy)
  local ax_squared = dx * dx
  local ay_squared = dy * dy
  if ax_squared >= 3 * ay_squared then
    return horizontal(dx)
  end
  if ay_squared >= 3 * ax_squared then
    return vertical(dy)
  end
  return horizontal(dx) + vertical(dy)
end

local function check_number(value, caller, argument)
  assert(type(value) == "number",
    string.format("pad_stick.%s: %s must be a number, got %s", caller, argument, tostring(value)))
end

function M.start(x, y, base_size)
  check_number(x, "start", "x")
  check_number(y, "start", "y")
  assert(type(base_size) == "number" and base_size > 0,
    string.format("pad_stick.start: base_size must be a number > 0, got %s", tostring(base_size)))
  return { base_x = x, base_y = y, radius = base_size / 2, knob_x = x, knob_y = y, mask = 0 }
end

function M.follow(stick, x, y)
  assert(type(stick) == "table",
    string.format("pad_stick.follow: stick must be a table, got %s", tostring(stick)))
  check_number(x, "follow", "x")
  check_number(y, "follow", "y")
  local dx = x - stick.base_x
  local dy = y - stick.base_y
  local distance_squared = dx * dx + dy * dy
  local radius = stick.radius
  local dead_radius = radius * 0.25
  if distance_squared < dead_radius * dead_radius then
    stick.mask = 0
  else
    stick.mask = direction(dx, dy)
  end
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
