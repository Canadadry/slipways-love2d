local bit = require("bit")
local cint = require("p8.cint")
local packed_bytes = require("p8.sprite_sheet_packed")
local vram = require("p8.vram")

local M = {}

M.PACKED_SIZE = cint.div(vram.SPRITE_SHEET_WIDTH * vram.SPRITE_SHEET_HEIGHT, 2)

assert(packed_bytes[M.PACKED_SIZE - 1] ~= nil
  and packed_bytes[M.PACKED_SIZE] == nil,
  "sprite_sheet_packed.lua does not hold sprite_sheet.PACKED_SIZE bytes: run make asset-export")

-- Writes only mem.sprite_sheet.
function M.load(mem)
  for y = 0, vram.SPRITE_SHEET_HEIGHT - 1 do
    for x = 0, vram.SPRITE_SHEET_WIDTH - 1 do
      local byte = packed_bytes[y * cint.div(vram.SPRITE_SHEET_WIDTH, 2) + cint.div(x, 2)]
      mem.sprite_sheet[y][x] = (cint.mod(x, 2) == 0) and bit.band(byte, 0x0F) or bit.rshift(byte, 4)
    end
  end
end

-- The table shared by every caller, keyed by byte address from 0: never write it. Pixel
-- (x, y) is in byte y * 64 + x / 2: low nibble for an even x, high for odd.
function M.packed()
  return packed_bytes
end

return M
