-- The positions and order must stay those of tools/pad_art.c's expected
-- frame, or the exact comparison fails for the wrong reason.
local pad_sheet = require("love.pad_sheet")

local graphics = love.graphics
local BUTTON_NAMES = { "a", "b", "x", "y", "leftshoulder", "rightshoulder", "start", "back" }
local sheet

function love.load()
  local canvas = graphics.newCanvas(160, 120)
  graphics.setCanvas(canvas)
  graphics.clear(0, 0, 0)
  graphics.setCanvas()
  sheet = pad_sheet.load()
end

function love.draw()
  graphics.setCanvas()
  graphics.setColor(1, 1, 1, 1)
  graphics.draw(sheet.image, sheet.stick_base, 4, 4)
  local quads = { sheet.stick_knob }
  for _, name in ipairs(BUTTON_NAMES) do
    quads[#quads + 1] = sheet.buttons[name].released
    quads[#quads + 1] = sheet.buttons[name].pressed
  end
  for k, quad in ipairs(quads) do
    local slot = k - 1
    graphics.draw(sheet.image, quad, 56 + 26 * (slot % 7), 4 + 26 * math.floor(slot / 7))
  end
end

local function rectangle_line(name, rectangle)
  return string.format("%s %d %d %d %d\n", name, rectangle.x, rectangle.y, rectangle.width, rectangle.height)
end

function debug_state()
  local lines = { string.format("%d %d\n", pad_sheet.WIDTH, pad_sheet.HEIGHT),
    rectangle_line("stick_base", pad_sheet.STICK_BASE), rectangle_line("stick_knob", pad_sheet.STICK_KNOB) }
  for _, name in ipairs(BUTTON_NAMES) do
    lines[#lines + 1] = rectangle_line(name .. " released", pad_sheet.BUTTONS[name].released)
    lines[#lines + 1] = rectangle_line(name .. " pressed", pad_sheet.BUTTONS[name].pressed)
  end
  return table.concat(lines)
end
