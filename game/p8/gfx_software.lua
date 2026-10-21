-- Copy of src/p8/gfx_software.c. A C pointer into image_store is its index
-- in image_store here: slot_pixels returns one, target holds one (nil while
-- drawing to the screen).
local bit = require("bit")
local cint = require("p8.cint")
local gfx_module = require("p8.gfx")
local image = require("p8.image")
local vram = require("p8.vram")

local M = {}

-- Holds only while callers draw colours 0..15: pset, line, rect, circ and print store col unmasked, and only dump.c asserts it.
local IMAGE_EMPTY = 0xFF

local SLOT_PIXELS = {}
local slot_pixels_defined = 0
image.P8_IMAGE_SLOTS(function(width, height)
  SLOT_PIXELS[slot_pixels_defined] = width * height
  slot_pixels_defined = slot_pixels_defined + 1
end)

local image_store = {}
for i = 0, image.P8_IMAGE_PIXELS - 1 do
  image_store[i] = 0
end
local image_built = {}
for slot = 0, image.P8_IMAGE_SLOT_COUNT - 1 do
  image_built[slot] = { width = 0, height = 0 }
end

local target = nil
local target_size = { width = 0, height = 0 }
local window_x = 0
local window_y = 0

local function software_cls(mem)
  for y = 0, vram.P8_SCREEN_HEIGHT - 1 do
    local row = mem.screen[y]
    for x = 0, vram.P8_SCREEN_WIDTH - 1 do
      row[x] = 0
    end
  end
end

local function plot(gfx, mem, x, y, col)
  local sx = x - gfx.camera_x
  local sy = y - gfx.camera_y
  if sx < 0 or sx >= vram.P8_SCREEN_WIDTH or sy < 0 or sy >= vram.P8_SCREEN_HEIGHT then
    return
  end
  if target == nil then
    mem.screen[sy][sx] = bit.band(col, 0xFF)
    return
  end
  local ix = window_x + sx
  local iy = window_y + sy
  if ix < target_size.width and iy < target_size.height then
    image_store[target + iy * target_size.width + ix] = bit.band(col, 0xFF)
  end
end

local function software_line(gfx, mem, x1, y1, x2, y2, col)
  local dx = math.abs(x2 - x1)
  local sx = x1 < x2 and 1 or -1
  local dy = -math.abs(y2 - y1)
  local sy = y1 < y2 and 1 or -1
  local err = dx + dy
  local x = x1
  local y = y1

  while true do
    plot(gfx, mem, x, y, col)
    if x == x2 and y == y2 then
      break
    end
    local e2 = 2 * err
    if e2 >= dy then
      err = err + dy
      x = x + sx
    end
    if e2 <= dx then
      err = err + dx
      y = y + sy
    end
  end
end

local function fillp_bit_set(gfx, x, y)
  local row = cint.mod(cint.mod(y, 4) + 4, 4)
  local col = cint.mod(cint.mod(x, 4) + 4, 4)
  local bit_index = 15 - (row * 4 + col)
  return bit.band(bit.rshift(gfx.fillp_pattern, bit_index), 1)
end

-- Masked here, not in p8_pset, so devtools/dump.c's assert still catches a primitive storing an index above 0x0f.
local function fillp_color(gfx, x, y, col)
  if fillp_bit_set(gfx, x, y) == 0 then
    return bit.band(col, 0x0F)
  end
  if gfx.fillp_transparent ~= 0 then
    return -1
  end
  return bit.band(bit.arshift(col, 4), 0x0F)
end

local function min(a, b)
  return a < b and a or b
end

local function max(a, b)
  return a < b and b or a
end

local function software_rect(gfx, mem, x1, y1, x2, y2, col)
  local xmin = min(x1, x2)
  local xmax = max(x1, x2)
  local ymin = min(y1, y2)
  local ymax = max(y1, y2)

  for x = xmin, xmax do
    plot(gfx, mem, x, ymin, col)
    plot(gfx, mem, x, ymax, col)
  end
  for y = ymin, ymax do
    plot(gfx, mem, xmin, y, col)
    plot(gfx, mem, xmax, y, col)
  end
end

local function circ_plot8(gfx, mem, cx, cy, dx, dy, col)
  plot(gfx, mem, cx + dx, cy + dy, col)
  plot(gfx, mem, cx - dx, cy + dy, col)
  plot(gfx, mem, cx + dx, cy - dy, col)
  plot(gfx, mem, cx - dx, cy - dy, col)
  plot(gfx, mem, cx + dy, cy + dx, col)
  plot(gfx, mem, cx - dy, cy + dx, col)
  plot(gfx, mem, cx + dy, cy - dx, col)
  plot(gfx, mem, cx - dy, cy - dx, col)
end

local function software_circ(gfx, mem, cx, cy, r, col)
  if r < 0 then
    return
  end
  local x = r
  local y = 0
  local err = 0

  while x >= y do
    circ_plot8(gfx, mem, cx, cy, x, y, col)
    y = y + 1
    if err <= 0 then
      err = err + 2 * y + 1
    end
    if err > 0 then
      x = x - 1
      err = err - (2 * x + 1)
    end
  end
end

local function circfill_span(gfx, mem, x1, x2, y, col)
  local xmin = min(x1, x2)
  local xmax = max(x1, x2)

  for x = xmin, xmax do
    local pattern_col = fillp_color(gfx, x, y, col)
    if pattern_col >= 0 then
      plot(gfx, mem, x, y, pattern_col)
    end
  end
end

local function software_circfill(gfx, mem, cx, cy, r, col)
  if r < 0 then
    return
  end
  local x = r
  local y = 0
  local err = 0

  while x >= y do
    circfill_span(gfx, mem, cx - x, cx + x, cy + y, col)
    circfill_span(gfx, mem, cx - x, cx + x, cy - y, col)
    circfill_span(gfx, mem, cx - y, cx + y, cy + x, col)
    circfill_span(gfx, mem, cx - y, cx + y, cy - x, col)
    y = y + 1
    if err <= 0 then
      err = err + 2 * y + 1
    end
    if err > 0 then
      x = x - 1
      err = err - (2 * x + 1)
    end
  end
end

local function sheet_get(mem, x, y)
  if x < 0 or x >= vram.P8_SPRITE_SHEET_WIDTH or y < 0 or y >= vram.P8_SPRITE_SHEET_HEIGHT then
    return 0
  end
  return mem.sprite_sheet[y][x]
end

local function software_spr(gfx, mem, n, x, y, w, h)
  local sprites_per_row = cint.div(vram.P8_SPRITE_SHEET_WIDTH, gfx_module.P8_SPRITE_SIZE)
  local sheet_x = cint.mod(n, sprites_per_row) * gfx_module.P8_SPRITE_SIZE
  local sheet_y = cint.div(n, sprites_per_row) * gfx_module.P8_SPRITE_SIZE
  local pixel_w = w * gfx_module.P8_SPRITE_SIZE
  local pixel_h = h * gfx_module.P8_SPRITE_SIZE

  for dy = 0, pixel_h - 1 do
    for dx = 0, pixel_w - 1 do
      local col = sheet_get(mem, sheet_x + dx, sheet_y + dy)
      if bit.band(mem.draw_palette[col], vram.P8_DRAW_PALETTE_TRANSPARENT_BIT) ~= 0 then
        goto continue
      end
      plot(gfx, mem, x + dx, y + dy, bit.band(mem.draw_palette[col], 0x0f))
      ::continue::
    end
  end
end

local FONT_ROWS_PLACEHOLDER = { [0] = 7, 7, 7, 7, 7 }
local FONT_ROWS_SPACE = { [0] = 0, 0, 0, 0, 0 }

local FONT_ROWS_0 = { [0] = 7, 5, 5, 5, 7 }
local FONT_ROWS_1 = { [0] = 2, 6, 2, 2, 7 }
local FONT_ROWS_2 = { [0] = 7, 1, 7, 4, 7 }
local FONT_ROWS_3 = { [0] = 7, 1, 7, 1, 7 }
local FONT_ROWS_4 = { [0] = 5, 5, 7, 1, 1 }
local FONT_ROWS_5 = { [0] = 7, 4, 7, 1, 7 }
local FONT_ROWS_6 = { [0] = 7, 4, 7, 5, 7 }
local FONT_ROWS_7 = { [0] = 7, 1, 1, 1, 1 }
local FONT_ROWS_8 = { [0] = 7, 5, 7, 5, 7 }
local FONT_ROWS_9 = { [0] = 7, 5, 7, 1, 7 }

local FONT_ROWS_A = { [0] = 2, 5, 7, 5, 5 }
local FONT_ROWS_B = { [0] = 4, 4, 6, 5, 6 }
local FONT_ROWS_C = { [0] = 3, 4, 4, 4, 3 }
local FONT_ROWS_D = { [0] = 1, 1, 3, 5, 3 }
local FONT_ROWS_E = { [0] = 7, 4, 6, 4, 7 }
local FONT_ROWS_F = { [0] = 7, 4, 6, 4, 4 }
local FONT_ROWS_G = { [0] = 3, 4, 5, 5, 3 }
local FONT_ROWS_H = { [0] = 5, 5, 7, 5, 5 }
local FONT_ROWS_I = { [0] = 7, 2, 2, 2, 7 }
local FONT_ROWS_J = { [0] = 1, 1, 1, 5, 2 }
local FONT_ROWS_K = { [0] = 5, 5, 6, 5, 5 }
local FONT_ROWS_L = { [0] = 4, 4, 4, 4, 7 }
local FONT_ROWS_M = { [0] = 5, 7, 7, 5, 5 }
local FONT_ROWS_N = { [0] = 5, 6, 5, 5, 5 }
local FONT_ROWS_O = { [0] = 2, 5, 5, 5, 2 }
local FONT_ROWS_P = { [0] = 6, 5, 6, 4, 4 }
local FONT_ROWS_Q = { [0] = 2, 5, 5, 3, 1 }
local FONT_ROWS_R = { [0] = 6, 5, 6, 5, 5 }
local FONT_ROWS_S = { [0] = 3, 4, 2, 1, 6 }
local FONT_ROWS_T = { [0] = 7, 2, 2, 2, 2 }
local FONT_ROWS_U = { [0] = 5, 5, 5, 5, 2 }
local FONT_ROWS_W = { [0] = 5, 5, 5, 7, 5 }
local FONT_ROWS_X = { [0] = 5, 5, 2, 5, 5 }
local FONT_ROWS_Y = { [0] = 5, 5, 2, 2, 2 }
local FONT_ROWS_Z = { [0] = 7, 1, 2, 4, 7 }

local FONT_ROWS_BANG = { [0] = 2, 2, 2, 0, 2 }
local FONT_ROWS_DOLLAR = { [0] = 2, 7, 2, 7, 2 }
local FONT_ROWS_PERCENT = { [0] = 5, 1, 2, 4, 5 }
local FONT_ROWS_APOSTROPHE = { [0] = 2, 2, 0, 0, 0 }
local FONT_ROWS_LPAREN = { [0] = 1, 2, 2, 2, 1 }
local FONT_ROWS_RPAREN = { [0] = 4, 2, 2, 2, 4 }
local FONT_ROWS_PLUS = { [0] = 0, 2, 7, 2, 0 }
local FONT_ROWS_COMMA = { [0] = 0, 0, 0, 2, 4 }
local FONT_ROWS_MINUS = { [0] = 0, 0, 7, 0, 0 }
local FONT_ROWS_PERIOD = { [0] = 0, 0, 0, 0, 2 }
local FONT_ROWS_SLASH = { [0] = 1, 1, 2, 4, 4 }
local FONT_ROWS_COLON = { [0] = 0, 2, 0, 2, 0 }
local FONT_ROWS_EQUALS = { [0] = 0, 7, 0, 7, 0 }
local FONT_ROWS_QUESTION = { [0] = 6, 1, 2, 0, 2 }

-- The cases of p8_font_glyph_rows's switch, in its order: the characters of
-- each case label, then the rows it returns.
local FONT_CASES = {
  { " ", FONT_ROWS_SPACE },
  { "0", FONT_ROWS_0 }, { "1", FONT_ROWS_1 }, { "2", FONT_ROWS_2 }, { "3", FONT_ROWS_3 }, { "4", FONT_ROWS_4 },
  { "5", FONT_ROWS_5 }, { "6", FONT_ROWS_6 }, { "7", FONT_ROWS_7 }, { "8", FONT_ROWS_8 }, { "9", FONT_ROWS_9 },
  { "Aa", FONT_ROWS_A }, { "Bb", FONT_ROWS_B }, { "Cc", FONT_ROWS_C }, { "Dd", FONT_ROWS_D }, { "Ee", FONT_ROWS_E },
  { "Ff", FONT_ROWS_F }, { "Gg", FONT_ROWS_G }, { "Hh", FONT_ROWS_H }, { "Ii", FONT_ROWS_I }, { "Jj", FONT_ROWS_J },
  { "Kk", FONT_ROWS_K }, { "Ll", FONT_ROWS_L }, { "Mm", FONT_ROWS_M }, { "Nn", FONT_ROWS_N }, { "Oo", FONT_ROWS_O },
  { "Pp", FONT_ROWS_P }, { "Qq", FONT_ROWS_Q }, { "Rr", FONT_ROWS_R }, { "Ss", FONT_ROWS_S }, { "Tt", FONT_ROWS_T },
  { "Uu", FONT_ROWS_U }, { "Vv", FONT_ROWS_U }, { "Ww", FONT_ROWS_W }, { "Xx", FONT_ROWS_X }, { "Yy", FONT_ROWS_Y },
  { "Zz", FONT_ROWS_Z },
  { "!", FONT_ROWS_BANG }, { "$", FONT_ROWS_DOLLAR }, { "%", FONT_ROWS_PERCENT }, { "'", FONT_ROWS_APOSTROPHE },
  { "(", FONT_ROWS_LPAREN }, { ")", FONT_ROWS_RPAREN }, { "+", FONT_ROWS_PLUS }, { ",", FONT_ROWS_COMMA },
  { "-", FONT_ROWS_MINUS }, { ".", FONT_ROWS_PERIOD }, { "/", FONT_ROWS_SLASH }, { ":", FONT_ROWS_COLON },
  { "=", FONT_ROWS_EQUALS }, { "?", FONT_ROWS_QUESTION },
}

local FONT_ROWS_BY_CHARACTER = {}
for _, case in ipairs(FONT_CASES) do
  for i = 1, #case[1] do
    FONT_ROWS_BY_CHARACTER[string.byte(case[1], i)] = case[2]
  end
end

-- character is a byte value. The P8_FONT_GLYPH_HEIGHT row bytes print draws
-- for it, indexed from 0, bit 2 the leftmost column: the placeholder's for a
-- character with no glyph. Shared by every caller: never write it.
function M.p8_font_glyph_rows(character)
  return FONT_ROWS_BY_CHARACTER[character] or FONT_ROWS_PLACEHOLDER
end

function M.p8_font_has_glyph(character)
  return M.p8_font_glyph_rows(character) ~= FONT_ROWS_PLACEHOLDER and 1 or 0
end

local function software_print(gfx, mem, str, x, y, col)
  local cx = x
  for p = 1, #str do
    local rows = M.p8_font_glyph_rows(string.byte(str, p))
    for row = 0, gfx_module.P8_FONT_GLYPH_HEIGHT - 1 do
      for gx = 0, gfx_module.P8_FONT_GLYPH_WIDTH - 1 do
        local bit_index = gfx_module.P8_FONT_GLYPH_WIDTH - 1 - gx
        if bit.band(bit.rshift(rows[row], bit_index), 1) ~= 0 then
          plot(gfx, mem, cx + gx, y + row, col)
        end
      end
    end
    cx = cx + gfx_module.P8_FONT_CELL_WIDTH
  end
end

local function software_print_outlined(gfx, mem, str, x, y, col)
  for dy = -1, 1 do
    for dx = -1, 1 do
      if dx ~= 0 or dy ~= 0 then
        software_print(gfx, mem, str, x + dx, y + dy, 0)
      end
    end
  end
  software_print(gfx, mem, str, x, y, col)
end

local function software_rectfill(gfx, mem, x1, y1, x2, y2, col)
  local xmin = min(x1, x2)
  local xmax = max(x1, x2)
  local ymin = min(y1, y2)
  local ymax = max(y1, y2)

  for y = ymin, ymax do
    for x = xmin, xmax do
      local pattern_col = fillp_color(gfx, x, y, col)
      if pattern_col >= 0 then
        plot(gfx, mem, x, y, pattern_col)
      end
    end
  end
end

local function software_pset(gfx, mem, x, y, col)
  plot(gfx, mem, x, y, col)
end

local function slot_pixels(slot)
  local pixels = 0
  for before = 0, slot - 1 do
    pixels = pixels + SLOT_PIXELS[before]
  end
  return pixels
end

local function software_image_begin(slot, w, h)
  target = slot_pixels(slot)
  target_size.width = w
  target_size.height = h
  image_built[slot] = { width = target_size.width, height = target_size.height }
  for i = target, target + w * h - 1 do
    image_store[i] = IMAGE_EMPTY
  end
end

local function software_image_window(slot, x, y)
  window_x = x
  window_y = y
end

local function software_image_end(slot)
  target = nil
end

local function software_image_draw(gfx, mem, slot, x, y)
  local pixels = slot_pixels(slot)
  local size = image_built[slot]
  local left = x - gfx.camera_x
  local top = y - gfx.camera_y
  local ix_first = max(0, -left)
  local ix_end = min(size.width, vram.P8_SCREEN_WIDTH - left)
  local iy_first = max(0, -top)
  local iy_end = min(size.height, vram.P8_SCREEN_HEIGHT - top)
  for iy = iy_first, iy_end - 1 do
    local row = pixels + iy * size.width
    local screen_row = mem.screen[top + iy]
    for ix = ix_first, ix_end - 1 do
      if image_store[row + ix] ~= IMAGE_EMPTY then
        screen_row[left + ix] = image_store[row + ix]
      end
    end
  end
end

local function software_lines(gfx, mem, segments, count, col)
  for i = 0, count - 1 do
    software_line(gfx, mem, segments[i].x1, segments[i].y1, segments[i].x2, segments[i].y2, col)
  end
end

local function software_psets(gfx, mem, points, count)
  for i = 0, count - 1 do
    software_pset(gfx, mem, points[i].x, points[i].y, points[i].col)
  end
end

local SOFTWARE_BACKEND = {
  cls = software_cls, pset = software_pset, line = software_line, rect = software_rect, rectfill = software_rectfill,
  circ = software_circ, circfill = software_circfill, spr = software_spr, print = software_print,
  print_outlined = software_print_outlined,
  image_begin = software_image_begin, image_window = software_image_window, image_end = software_image_end,
  image_draw = software_image_draw,
  lines = software_lines, psets = software_psets,
}

-- Rasterises into mem.screen. Shared by every caller: never write it.
function M.p8_gfx_software_backend()
  return SOFTWARE_BACKEND
end

return M
