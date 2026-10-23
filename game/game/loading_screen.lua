local bit = require("bit")
local gfx_module = require("p8.gfx")
local vram = require("p8.vram")

local M = {}

-- Not 1: every e2e hash depends on the two loading frames.
M.FRAMES = 2

M.TEXT = "loading"

local LOADING_TEXT_LENGTH = 8
local DRAW_PALETTE_LENGTH = 16

local TEXT_COLOR = 7
local MARGIN = 1
local TEXT_WIDTH = (LOADING_TEXT_LENGTH - 1) * gfx_module.FONT_CELL_WIDTH -
  (gfx_module.FONT_CELL_WIDTH - gfx_module.FONT_GLYPH_WIDTH)
local TEXT_X = vram.SCREEN_WIDTH - MARGIN - TEXT_WIDTH
local TEXT_Y = vram.SCREEN_HEIGHT - MARGIN - gfx_module.FONT_GLYPH_HEIGHT

local function graphics_copy(gfx)
  return {
    camera_x = gfx.camera_x,
    camera_y = gfx.camera_y,
    fillp_pattern = gfx.fillp_pattern,
    fillp_transparent = gfx.fillp_transparent,
    pen_color = gfx.pen_color,
  }
end

-- Draws with the camera at 0,0, no fill pattern and the default palette; restores what it found.
function M.draw(gfx, mem)
  assert(gfx ~= nil and mem ~= nil, "loading_screen.draw: print dereferences both")
  local found = graphics_copy(gfx)
  local palette = {}

  for i = 0, DRAW_PALETTE_LENGTH - 1 do
    palette[i] = mem.draw_palette[i]
  end
  for col = 0, DRAW_PALETTE_LENGTH - 1 do
    mem.draw_palette[col] = bit.band((col == 0) and bit.bor(col, vram.DRAW_PALETTE_TRANSPARENT_BIT) or col, 0xFF)
  end
  gfx_module.camera_reset(gfx)
  gfx_module.fillp_reset(gfx)

  gfx_module.cls(mem)
  gfx_module.print(gfx, mem, M.TEXT, TEXT_X, TEXT_Y, TEXT_COLOR)

  gfx.camera_x = found.camera_x
  gfx.camera_y = found.camera_y
  gfx.fillp_pattern = found.fillp_pattern
  gfx.fillp_transparent = found.fillp_transparent
  gfx.pen_color = found.pen_color
  for i = 0, DRAW_PALETTE_LENGTH - 1 do
    mem.draw_palette[i] = palette[i]
  end
end

return M
