-- Text from font.png: the LÖVE side of src/p8/gfx_software.c's
-- software_print, cell for cell as src/devtools/asset_image.c lays it out.
local cint = require("p8.cint")
local gfx_module = require("p8.gfx")
local palette = require("p8.palette")
local vram = require("p8.vram")
local screen = require("love.screen")

local M = {}

local FIRST_CODE = 32
local LAST_CODE = 127
local COLUMNS = 16
local FONT_WIDTH = COLUMNS * gfx_module.P8_FONT_GLYPH_WIDTH
local FONT_HEIGHT = cint.div(LAST_CODE - FIRST_CODE + 1, COLUMNS) * gfx_module.P8_FONT_GLYPH_HEIGHT
local LIT = palette.P8_PALETTE_RGB[7]

local font_image = nil

-- A fresh ImageData is alpha 0 everywhere: only the lit pixels are set.
local function build_font_image()
  local data = screen.screen_font_data()
  local white = love.image.newImageData(FONT_WIDTH, FONT_HEIGHT)
  for y = 0, FONT_HEIGHT - 1 do
    for x = 0, FONT_WIDTH - 1 do
      local red, green, blue = data:getPixel(x, y)
      if math.floor(red * 255 + 0.5) == LIT.red and math.floor(green * 255 + 0.5) == LIT.green
        and math.floor(blue * 255 + 0.5) == LIT.blue then
        white:setPixel(x, y, 1, 1, 1, 1)
      end
    end
  end
  return love.graphics.newImage(white)
end

-- Draws str's glyphs in the current colour from x, y in screen coordinates:
-- byte b is cell b when 32 <= b <= 126, else cell 127, the placeholder (the
-- C's signed char sends bytes above 127 to it too).
function M.text_draw(str, x, y)
  if font_image == nil then
    font_image = build_font_image()
  end
  local width = gfx_module.P8_FONT_GLYPH_WIDTH
  local height = gfx_module.P8_FONT_GLYPH_HEIGHT
  if y + height <= 0 or y >= vram.P8_SCREEN_HEIGHT then
    return
  end
  local top = y < 0 and -y or 0
  local bottom = y + height > vram.P8_SCREEN_HEIGHT and vram.P8_SCREEN_HEIGHT - y or height
  for i = 1, #str do
    local sx = x + (i - 1) * gfx_module.P8_FONT_CELL_WIDTH
    if sx + width > 0 and sx < vram.P8_SCREEN_WIDTH then
      local code = string.byte(str, i)
      if code < FIRST_CODE or code > LAST_CODE - 1 then
        code = LAST_CODE
      end
      local cell = code - FIRST_CODE
      local left = sx < 0 and -sx or 0
      local right = sx + width > vram.P8_SCREEN_WIDTH and vram.P8_SCREEN_WIDTH - sx or width
      local quad = love.graphics.newQuad(cint.mod(cell, COLUMNS) * width + left,
        cint.div(cell, COLUMNS) * height + top, right - left, bottom - top, FONT_WIDTH, FONT_HEIGHT)
      love.graphics.draw(font_image, quad, sx + left, y + top)
    end
  end
end

return M
