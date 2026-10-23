-- Copy of src/game/loading_screen.h then src/game/loading_screen.c.
local bit = require("bit")
local gfx_module = require("p8.gfx")
local vram = require("p8.vram")

local M = {}

-- Not 1: every e2e hash depends on the two loading frames.
M.LOADING_SCREEN_FRAMES = 2

M.LOADING_TEXT = "loading"

local LOADING_TEXT_LENGTH = 8
local DRAW_PALETTE_LENGTH = 16

local TEXT_COLOR = 7
local MARGIN = 1
local TEXT_WIDTH = (LOADING_TEXT_LENGTH - 1) * gfx_module.P8_FONT_CELL_WIDTH -
  (gfx_module.P8_FONT_CELL_WIDTH - gfx_module.P8_FONT_GLYPH_WIDTH)
local TEXT_X = vram.P8_SCREEN_WIDTH - MARGIN - TEXT_WIDTH
local TEXT_Y = vram.P8_SCREEN_HEIGHT - MARGIN - gfx_module.P8_FONT_GLYPH_HEIGHT

-- Lua only: a by-value p8_graphics_t.
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
function M.loading_screen_draw(gfx, mem)
  assert(gfx ~= nil and mem ~= nil, "loading_screen_draw: print dereferences both")
  local found = graphics_copy(gfx)
  local palette = {}

  for i = 0, DRAW_PALETTE_LENGTH - 1 do
    palette[i] = mem.draw_palette[i]
  end
  for col = 0, DRAW_PALETTE_LENGTH - 1 do
    mem.draw_palette[col] = bit.band((col == 0) and bit.bor(col, vram.P8_DRAW_PALETTE_TRANSPARENT_BIT) or col, 0xFF)
  end
  gfx_module.p8_camera_reset(gfx)
  gfx_module.p8_fillp_reset(gfx)

  gfx_module.p8_cls(mem)
  gfx_module.p8_print(gfx, mem, M.LOADING_TEXT, TEXT_X, TEXT_Y, TEXT_COLOR)

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
