-- Fill pattern images: the LÖVE side of src/p8/gfx_software.c's fillp_color.
local bit = require("bit")
local draw_cost = require("p8.draw_cost")
local screen = require("love.screen")
local vram = require("p8.vram")

local M = {}

-- One screen-wide span at any origin 0..3 fits.
M.PATTERN_IMAGE_WIDTH = vram.P8_SCREEN_WIDTH + 4
M.PATTERN_IMAGE_HEIGHT = vram.P8_SCREEN_HEIGHT + 4

local images = {}

local function build_image(pattern, col, transparent)
  local data = love.image.newImageData(M.PATTERN_IMAGE_WIDTH, M.PATTERN_IMAGE_HEIGHT)
  local clear_color = screen.SCREEN_COLORS[bit.band(col, 0x0F)]
  local set_color = screen.SCREEN_COLORS[bit.band(bit.rshift(col, 4), 0x0F)]
  for j = 0, M.PATTERN_IMAGE_HEIGHT - 1 do
    for i = 0, M.PATTERN_IMAGE_WIDTH - 1 do
      local bit_index = 15 - ((j % 4) * 4 + i % 4)
      if bit.band(bit.rshift(pattern, bit_index), 1) == 0 then
        data:setPixel(i, j, clear_color.red, clear_color.green, clear_color.blue)
      elseif transparent == 0 then
        data:setPixel(i, j, set_color.red, set_color.green, set_color.blue)
      end
    end
  end
  return love.graphics.newImage(data)
end

-- The image whose pixel (i, j) is the C's fillp_color at world (i, j) for the
-- current pattern of gfx, alpha 0 where it returns -1. Built on first use per
-- p8_draw_cost_pattern_key, kept for the run: never draw into it.
function M.pattern_image(gfx, col)
  local key = draw_cost.p8_draw_cost_pattern_key(gfx, col)
  local image = images[key]
  if image == nil then
    image = build_image(bit.band(gfx.fillp_pattern, 0xFFFF), bit.band(col, 0xFF), gfx.fillp_transparent)
    images[key] = image
  end
  return image
end

return M
