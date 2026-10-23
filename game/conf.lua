-- The fake passes empty tables and reads nothing back: these fields matter to real LÖVE only.
local screen = require("love.screen")
local vram = require("p8.vram")

function love.conf(t)
  t.version = "11.5"
  t.identity = "slipways"
  t.window.title = "Slipways"
  t.window.width = vram.SCREEN_WIDTH * screen.SCALE
  t.window.height = vram.SCREEN_HEIGHT * screen.SCALE
  t.window.resizable = false
  t.window.vsync = 1
  t.modules.audio = false
  t.modules.sound = false
end
