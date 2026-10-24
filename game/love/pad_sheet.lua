-- The sprite rectangles of pad.png. tools/pad_art.c holds its own copy: change both, then run make pad-art.
local M = {}

M.WIDTH = 240
M.HEIGHT = 72

M.STICK_BASE = { x = 0, y = 0, width = 48, height = 48 }
M.STICK_KNOB = { x = 0, y = 48, width = 24, height = 24 }

local BUTTON_NAMES = { "a", "b", "x", "y", "leftshoulder", "rightshoulder", "start", "back" }
local BUTTON_SIZE = 24
local BUTTONS_X = 48

M.BUTTONS = {}
for k, name in ipairs(BUTTON_NAMES) do
  local x = BUTTONS_X + BUTTON_SIZE * (k - 1)
  M.BUTTONS[name] = {
    released = { x = x, y = 0, width = BUTTON_SIZE, height = BUTTON_SIZE },
    pressed = { x = x, y = BUTTON_SIZE, width = BUTTON_SIZE, height = BUTTON_SIZE },
  }
end

local function new_quad(rectangle)
  return love.graphics.newQuad(rectangle.x, rectangle.y, rectangle.width, rectangle.height, M.WIDTH, M.HEIGHT)
end

-- Reads pad.png: call once, in love.load. Returns { image, stick_base,
-- stick_knob, buttons = { [name] = { released, pressed } } }: image is the
-- Image, every other leaf a Quad.
function M.load()
  local sheet = {
    image = love.graphics.newImage(love.image.newImageData("pad.png")),
    stick_base = new_quad(M.STICK_BASE),
    stick_knob = new_quad(M.STICK_KNOB),
    buttons = {},
  }
  for name, button in pairs(M.BUTTONS) do
    sheet.buttons[name] = { released = new_quad(button.released), pressed = new_quad(button.pressed) }
  end
  return sheet
end

return M
