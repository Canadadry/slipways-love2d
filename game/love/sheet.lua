-- Recoloured copies of sprites.png: the LÖVE side of p8.gfx_software's
-- software_spr.
local bit = require("bit")
local cint = require("p8.cint")
local gfx_module = require("p8.gfx")
local palette = require("p8.palette")
local vram = require("p8.vram")
local screen = require("love.screen")

local M = {}

local CELLS_PER_ROW = cint.div(vram.SPRITE_SHEET_WIDTH, gfx_module.SPRITE_SIZE)

local index_of_rgb = {}
for c = 0, palette.COLORS - 1 do
  local rgb = palette.RGB[c]
  index_of_rgb[rgb.red * 65536 + rgb.green * 256 + rgb.blue] = c
end

-- cell_indices[cell][dy][dx]: palette index of sprites.png's pixel dx, dy of
-- the cell, read on the cell's first fill.
local cell_indices = {}
-- Per draw palette key: { data = ImageData, filled = { [cell] = true },
-- image = Image of data, or nil when cells were filled after it }.
local sheets = {}

local function read_cell(cell)
  local data = screen.sprites_data()
  local x0 = cint.mod(cell, CELLS_PER_ROW) * gfx_module.SPRITE_SIZE
  local y0 = cint.div(cell, CELLS_PER_ROW) * gfx_module.SPRITE_SIZE
  local indices = {}
  for dy = 0, gfx_module.SPRITE_SIZE - 1 do
    local row = {}
    for dx = 0, gfx_module.SPRITE_SIZE - 1 do
      local red, green, blue, alpha = data:getPixel(x0 + dx, y0 + dy)
      local key = math.floor(red * 255 + 0.5) * 65536 + math.floor(green * 255 + 0.5) * 256 + math.floor(blue * 255 + 0.5)
      local c = index_of_rgb[key]
      if c == nil or alpha ~= 1 then
        error(string.format("sprites.png: pixel (%d, %d) is not a palette colour", x0 + dx, y0 + dy), 0)
      end
      row[dx] = c
    end
    indices[dy] = row
  end
  return indices
end

-- A fresh ImageData is alpha 0 everywhere, so see-through pixels are skipped.
local function fill_cell(sheet, draw_palette, cell)
  local indices = cell_indices[cell]
  if indices == nil then
    indices = read_cell(cell)
    cell_indices[cell] = indices
  end
  local x0 = cint.mod(cell, CELLS_PER_ROW) * gfx_module.SPRITE_SIZE
  local y0 = cint.div(cell, CELLS_PER_ROW) * gfx_module.SPRITE_SIZE
  for dy = 0, gfx_module.SPRITE_SIZE - 1 do
    local row = indices[dy]
    for dx = 0, gfx_module.SPRITE_SIZE - 1 do
      local entry = draw_palette[row[dx]]
      if bit.band(entry, vram.DRAW_PALETTE_TRANSPARENT_BIT) == 0 then
        local color = screen.COLORS[bit.band(entry, 0x0F)]
        sheet.data:setPixel(x0 + dx, y0 + dy, color.red, color.green, color.blue)
      end
    end
  end
  sheet.filled[cell] = true
  sheet.image = nil
end

-- The sprite sheet as mem.draw_palette maps it (entry c of the draw palette
-- for index c, alpha 0 where its see-through bit is set), exact at least over
-- the sheet pixels x0..x1, y0..y1, which must lie on the sheet. Each 8x8 cell
-- is filled the first time a draw reaches it under that draw palette, then
-- kept for the run: never draw into the image, and never keep it across calls.
function M.image(mem, x0, y0, x1, y1)
  assert(x0 >= 0 and y0 >= 0 and x1 < vram.SPRITE_SHEET_WIDTH and y1 < vram.SPRITE_SHEET_HEIGHT
    and x0 <= x1 and y0 <= y1, "sheet.image: the region must lie on the sprite sheet")
  local bytes = {}
  for c = 0, palette.COLORS - 1 do
    bytes[c + 1] = mem.draw_palette[c]
  end
  local key = string.char(unpack(bytes))
  local sheet = sheets[key]
  if sheet == nil then
    sheet = {
      data = love.image.newImageData(vram.SPRITE_SHEET_WIDTH, vram.SPRITE_SHEET_HEIGHT), filled = {}, image = nil,
    }
    sheets[key] = sheet
  end
  for cy = cint.div(y0, gfx_module.SPRITE_SIZE), cint.div(y1, gfx_module.SPRITE_SIZE) do
    for cx = cint.div(x0, gfx_module.SPRITE_SIZE), cint.div(x1, gfx_module.SPRITE_SIZE) do
      local cell = cy * CELLS_PER_ROW + cx
      if not sheet.filled[cell] then
        fill_cell(sheet, mem.draw_palette, cell)
      end
    end
  end
  if sheet.image == nil then
    sheet.image = love.graphics.newImage(sheet.data)
  end
  return sheet.image
end

return M
