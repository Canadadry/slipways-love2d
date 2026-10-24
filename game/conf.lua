-- The fake passes empty tables and reads nothing back: these fields matter to real LÖVE only.
local vram = require("p8.vram")

local START_SCALE = 4

function love.conf(t)
  t.version = "11.5"
  t.identity = "slipways"
  t.window.title = "Slipways"
  t.window.width = vram.SCREEN_WIDTH * START_SCALE
  t.window.height = vram.SCREEN_HEIGHT * START_SCALE
  t.window.resizable = true
  t.window.highdpi = true
  t.window.vsync = 1
  t.modules.audio = false
  t.modules.sound = false
end
