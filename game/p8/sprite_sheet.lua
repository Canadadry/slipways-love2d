local bit = require("bit")
local cint = require("p8.cint")
local p8_sprite_sheet_packed_bytes = require("p8.sprite_sheet_packed")
local vram = require("p8.vram")

local M = {}

M.P8_SPRITE_SHEET_PACKED_SIZE = cint.div(vram.P8_SPRITE_SHEET_WIDTH * vram.P8_SPRITE_SHEET_HEIGHT, 2)

assert(p8_sprite_sheet_packed_bytes[M.P8_SPRITE_SHEET_PACKED_SIZE - 1] ~= nil
  and p8_sprite_sheet_packed_bytes[M.P8_SPRITE_SHEET_PACKED_SIZE] == nil,
  "sprite_sheet_packed.lua does not hold P8_SPRITE_SHEET_PACKED_SIZE bytes: run make asset-export")

-- Writes only mem.sprite_sheet.
function M.p8_sprite_sheet_load(mem)
  for y = 0, vram.P8_SPRITE_SHEET_HEIGHT - 1 do
    for x = 0, vram.P8_SPRITE_SHEET_WIDTH - 1 do
      local byte = p8_sprite_sheet_packed_bytes[y * cint.div(vram.P8_SPRITE_SHEET_WIDTH, 2) + cint.div(x, 2)]
      mem.sprite_sheet[y][x] = (cint.mod(x, 2) == 0) and bit.band(byte, 0x0F) or bit.rshift(byte, 4)
    end
  end
end

-- The table shared by every caller, keyed by byte address from 0: never write it. Pixel
-- (x, y) is in byte y * 64 + x / 2: low nibble for an even x, high for odd.
function M.p8_sprite_sheet_packed()
  return p8_sprite_sheet_packed_bytes
end

return M
