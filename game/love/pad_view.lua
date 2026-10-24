local bit = require("bit")
local pad_sheet = require("love.pad_sheet")
local pad_touch = require("love.pad_touch")
local virtual_pad_module = require("love.virtual_pad")

local M = {}

-- The sheet's order, never pairs: the later draw lies over the earlier one.
local BUTTON_NAMES = { "a", "b", "x", "y", "leftshoulder", "rightshoulder", "start", "back" }

-- Call once, in love.load after screen.init: the sheet is built under its nearest filter.
function M.init(view)
  view.sheet = pad_sheet.load()
end

local function draw_stretched(image, quad, sprite, x, y, width, height)
  love.graphics.draw(image, quad, x, y, 0, width / sprite.width, height / sprite.height)
end

local function draw_centred(image, quad, sprite, centre_x, centre_y, side)
  draw_stretched(image, quad, sprite, centre_x - side / 2, centre_y - side / 2, side, side)
end

-- Draws onto the screen, after screen.present: never onto canvas 0.
function M.draw(view, virtual_pad)
  assert(type(view) == "table" and view.sheet ~= nil, "pad_view.draw: view must be a table passed to init")
  assert(type(virtual_pad) == "table", "pad_view.draw: virtual_pad must be a table passed to virtual_pad.init")
  local placed = virtual_pad.placed
  if placed == nil or not virtual_pad_module.shown(virtual_pad) then
    return
  end
  local sheet = view.sheet
  love.graphics.setCanvas()
  love.graphics.setColor(1, 1, 1)
  for _, name in ipairs(BUTTON_NAMES) do
    local state = bit.band(virtual_pad.virtual_mask, pad_touch.BUTTON_MASKS[name]) ~= 0 and "pressed" or "released"
    local rectangle = placed.buttons[name]
    draw_stretched(sheet.image, sheet.buttons[name][state], pad_sheet.BUTTONS[name][state], rectangle.x, rectangle.y,
      rectangle.width, rectangle.height)
  end
  local stick = virtual_pad.touch_state.stick
  if stick ~= nil then
    draw_centred(sheet.image, sheet.stick_base, pad_sheet.STICK_BASE, stick.base_x, stick.base_y,
      placed.stick_base_size)
    draw_centred(sheet.image, sheet.stick_knob, pad_sheet.STICK_KNOB, stick.knob_x, stick.knob_y,
      placed.stick_knob_size)
  end
end

return M
