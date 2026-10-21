-- Copy of src/p8/vram.h then src/p8/vram.c. A p8_vram_t is a table with the
-- C fields: draw_palette[16], sprite_sheet[y][x], map_cells[y][x],
-- screen[y][x] (every array indexed from 0) and map_version.
local bit = require("bit")

local M = {}

M.P8_DRAW_PALETTE_TRANSPARENT_BIT = 0x80

M.P8_SCREEN_WIDTH = 160
M.P8_SCREEN_HEIGHT = 120

M.P8_SPRITE_SHEET_WIDTH = 128
M.P8_SPRITE_SHEET_HEIGHT = 128

M.P8_MAP_WIDTH = 32
M.P8_MAP_HEIGHT = 32

local last_map_version = 0

function M.p8_vram_next_map_version()
  last_map_version = last_map_version + 1
  return last_map_version
end

local function zero_grid(height, width)
  local rows = {}
  for y = 0, height - 1 do
    local row = {}
    for x = 0, width - 1 do
      row[x] = 0
    end
    rows[y] = row
  end
  return rows
end

function M.p8_vram_init(mem)
  mem.draw_palette = {}
  mem.sprite_sheet = zero_grid(M.P8_SPRITE_SHEET_HEIGHT, M.P8_SPRITE_SHEET_WIDTH)
  mem.map_cells = zero_grid(M.P8_MAP_HEIGHT, M.P8_MAP_WIDTH)
  mem.screen = zero_grid(M.P8_SCREEN_HEIGHT, M.P8_SCREEN_WIDTH)
  mem.map_version = 0

  for c = 0, 15 do
    local entry = c
    if c == 0 then
      entry = bit.bor(entry, M.P8_DRAW_PALETTE_TRANSPARENT_BIT)
    end
    mem.draw_palette[c] = entry
  end
  mem.map_version = M.p8_vram_next_map_version()
end

return M
