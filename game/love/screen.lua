-- The game canvas and the assets every backend module reads: the LÖVE side
-- of p8.vram's screen and p8.palette's colours.
local palette = require("p8.palette")
local vram = require("p8.vram")

local M = {}

M.SCALE = 4

-- Indexed 0..15 like palette.RGB: { red, green, blue } as byte / 255.
M.COLORS = {}
for c = 0, palette.COLORS - 1 do
  local rgb = palette.RGB[c]
  M.COLORS[c] = { red = rgb.red / 255, green = rgb.green / 255, blue = rgb.blue / 255 }
end

local canvas = nil
local sprites_data = nil
local font_data = nil

-- Must run first in love.load: the canvas has to be canvas 0, the one
-- tmp/fake_love captures. It starts colour 0, as vram.init's screen.
function M.init()
  love.graphics.setDefaultFilter("nearest", "nearest")
  canvas = love.graphics.newCanvas(vram.SCREEN_WIDTH, vram.SCREEN_HEIGHT)
  love.graphics.setCanvas(canvas)
  love.graphics.clear(0, 0, 0)
  love.graphics.setCanvas()
  sprites_data = love.image.newImageData("sprites.png")
  font_data = love.image.newImageData("font.png")
end

function M.frame_begin()
  assert(canvas ~= nil, "screen.frame_begin: screen.init has not run")
  love.graphics.setCanvas(canvas)
end

function M.frame_end()
  love.graphics.setCanvas()
end

-- design.md's present: call it once, last in love.draw, outside any push.
function M.present()
  assert(canvas ~= nil, "screen.present: screen.init has not run")
  love.graphics.setCanvas()
  love.graphics.setColor(1, 1, 1)
  love.graphics.draw(canvas, 0, 0, 0, M.SCALE, M.SCALE)
end

-- The ImageData of sprites.png, loaded once by screen.init: never write it.
function M.sprites_data()
  return sprites_data
end

-- The ImageData of font.png, loaded once by screen.init: never write it.
function M.font_data()
  return font_data
end

return M
