-- The game canvas and the assets every backend module reads. Not a C copy:
-- the LÖVE side of src/p8/vram.c's screen and src/p8/palette.c's colours.
local palette = require("p8.palette")
local vram = require("p8.vram")

local M = {}

-- Indexed 0..15 like P8_PALETTE_RGB: { red, green, blue } as byte / 255.
M.SCREEN_COLORS = {}
for c = 0, palette.P8_PALETTE_COLORS - 1 do
  local rgb = palette.P8_PALETTE_RGB[c]
  M.SCREEN_COLORS[c] = { red = rgb.red / 255, green = rgb.green / 255, blue = rgb.blue / 255 }
end

local canvas = nil
local sprites_data = nil
local font_data = nil

-- Must run first in love.load: the canvas has to be canvas 0, the one
-- tmp/fake_love captures. It starts colour 0, as p8_vram_init's screen.
function M.screen_init()
  love.graphics.setDefaultFilter("nearest", "nearest")
  canvas = love.graphics.newCanvas(vram.P8_SCREEN_WIDTH, vram.P8_SCREEN_HEIGHT)
  love.graphics.setCanvas(canvas)
  love.graphics.clear(0, 0, 0)
  love.graphics.setCanvas()
  sprites_data = love.image.newImageData("sprites.png")
  font_data = love.image.newImageData("font.png")
end

function M.screen_frame_begin()
  assert(canvas ~= nil, "screen_frame_begin: screen_init has not run")
  love.graphics.setCanvas(canvas)
end

function M.screen_frame_end()
  love.graphics.setCanvas()
end

-- The ImageData of sprites.png, loaded once by screen_init: never write it.
function M.screen_sprites_data()
  return sprites_data
end

-- The ImageData of font.png, loaded once by screen_init: never write it.
function M.screen_font_data()
  return font_data
end

return M
