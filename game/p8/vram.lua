local bit = require("bit")

local M = {}

M.DRAW_PALETTE_TRANSPARENT_BIT = 0x80

M.SCREEN_WIDTH = 160
M.SCREEN_HEIGHT = 120

M.SPRITE_SHEET_WIDTH = 128
M.SPRITE_SHEET_HEIGHT = 128

M.MAP_WIDTH = 32
M.MAP_HEIGHT = 32

local last_map_version = 0

function M.next_map_version()
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

function M.init(mem)
  mem.draw_palette = {}
  mem.sprite_sheet = zero_grid(M.SPRITE_SHEET_HEIGHT, M.SPRITE_SHEET_WIDTH)
  mem.map_cells = zero_grid(M.MAP_HEIGHT, M.MAP_WIDTH)
  mem.screen = zero_grid(M.SCREEN_HEIGHT, M.SCREEN_WIDTH)
  mem.map_version = 0

  for c = 0, 15 do
    local entry = c
    if c == 0 then
      entry = bit.bor(entry, M.DRAW_PALETTE_TRANSPARENT_BIT)
    end
    mem.draw_palette[c] = entry
  end
  mem.map_version = M.next_map_version()
end

return M
