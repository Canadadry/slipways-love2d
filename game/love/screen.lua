local palette = require("p8.palette")
local vram = require("p8.vram")
local window_layout = require("love.window_layout")

local M = {}

M.COLORS = {}
for c = 0, palette.COLORS - 1 do
  local rgb = palette.RGB[c]
  M.COLORS[c] = { red = rgb.red / 255, green = rgb.green / 255, blue = rgb.blue / 255 }
end

local canvas = nil
local sprites_data = nil
local font_data = nil

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

function M.get_dimensions()
  return love.graphics.getDimensions()
end

-- Outside any push: a transform would move and scale the game in the window.
function M.present()
  assert(canvas ~= nil, "screen.present: screen.init has not run")
  love.graphics.setCanvas()
  love.graphics.setColor(1, 1, 1)
  local game = window_layout.compute(M.get_dimensions()).game
  love.graphics.draw(canvas, game.x, game.y, 0, game.scale, game.scale)
end

function M.sprites_data()
  return sprites_data
end

function M.font_data()
  return font_data
end

return M
