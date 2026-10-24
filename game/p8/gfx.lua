-- gfx is { camera_x, camera_y, fillp_pattern, fillp_transparent, pen_color }.
-- A point list is { { x, y, col }, ... } and a segment list
-- { { x1, y1, x2, y2 }, ... }, each passed with its count and read from 1 to
-- count. A draw backend is a table of 16 functions: cls, pset, line,
-- rect, rectfill, circ, circfill, spr, print, print_outlined, image_begin,
-- image_window, image_end, image_draw, lines, psets.
local bit = require("bit")
local cint = require("p8.cint")
local vram = require("p8.vram")

local M = {}

M.SPRITE_SIZE = 8

M.FONT_GLYPH_WIDTH = 3
M.FONT_GLYPH_HEIGHT = 5
M.FONT_CELL_WIDTH = 4

local active_backend = nil

-- Required at the call: p8.gfx_software requires p8.image, which requires p8.gfx.
local function backend()
  if active_backend ~= nil then
    return active_backend
  end
  return require("p8.gfx_software").backend()
end

-- nil installs the software backend. Returns the backend active before the
-- call, never nil.
function M.draw_backend_set(new_backend)
  local previous = backend()
  active_backend = new_backend
  return previous
end

function M.draw_backend_get()
  return backend()
end

function M.init(gfx)
  gfx.camera_x = 0
  gfx.camera_y = 0
  gfx.fillp_pattern = 0
  gfx.fillp_transparent = false
  gfx.pen_color = 6
end

function M.cls(mem)
  backend().cls(mem)
end

function M.pset(gfx, mem, x, y, col)
  backend().pset(gfx, mem, x, y, col)
end

function M.psets(gfx, mem, points, count)
  assert(count >= 0 and (count == 0 or points ~= nil), "gfx.psets: a negative count or a NULL list")
  backend().psets(gfx, mem, points, count)
end

function M.camera(gfx, x, y)
  gfx.camera_x = x
  gfx.camera_y = y
end

function M.camera_reset(gfx)
  M.camera(gfx, 0, 0)
end

function M.palt(mem, col, transparent)
  assert(type(transparent) == "boolean", "gfx.palt: transparent is a boolean, as in PICO-8; 0 would be truthy")
  if col < 0 or col > 15 then
    return
  end
  if transparent then
    mem.draw_palette[col] = bit.band(bit.bor(mem.draw_palette[col], vram.DRAW_PALETTE_TRANSPARENT_BIT), 0xFF)
  else
    mem.draw_palette[col] = bit.band(mem.draw_palette[col], bit.bnot(vram.DRAW_PALETTE_TRANSPARENT_BIT), 0xFF)
  end
end

function M.is_transparent(mem, col)
  if col < 0 or col > 15 then
    return false
  end
  return bit.band(mem.draw_palette[col], vram.DRAW_PALETTE_TRANSPARENT_BIT) ~= 0
end

function M.line(gfx, mem, x1, y1, x2, y2, col)
  backend().line(gfx, mem, x1, y1, x2, y2, col)
end

function M.lines(gfx, mem, segments, count, col)
  assert(count >= 0 and (count == 0 or segments ~= nil), "gfx.lines: a negative count or a NULL list")
  backend().lines(gfx, mem, segments, count, col)
end

function M.rect(gfx, mem, x1, y1, x2, y2, col)
  backend().rect(gfx, mem, x1, y1, x2, y2, col)
end

function M.rectfill(gfx, mem, x1, y1, x2, y2, col)
  backend().rectfill(gfx, mem, x1, y1, x2, y2, col)
end

function M.circ(gfx, mem, cx, cy, r, col)
  backend().circ(gfx, mem, cx, cy, r, col)
end

function M.circfill(gfx, mem, cx, cy, r, col)
  backend().circfill(gfx, mem, cx, cy, r, col)
end

function M.fillp(gfx, pattern)
  gfx.fillp_pattern = bit.band(pattern, 0xFFFF)
  gfx.fillp_transparent = false
end

function M.fillp_transparent(gfx, pattern)
  gfx.fillp_pattern = bit.band(pattern, 0xFFFF)
  gfx.fillp_transparent = true
end

function M.fillp_reset(gfx)
  M.fillp(gfx, 0)
end

function M.sget(mem, x, y)
  if x < 0 or x >= vram.SPRITE_SHEET_WIDTH or y < 0 or y >= vram.SPRITE_SHEET_HEIGHT then
    return 0
  end
  return mem.sprite_sheet[y][x]
end

function M.spr(gfx, mem, n, x, y, w, h)
  backend().spr(gfx, mem, n, x, y, w, h)
end

function M.spr_default(gfx, mem, n, x, y)
  M.spr(gfx, mem, n, x, y, 1, 1)
end

local function map_cell_in_range(x, y)
  return x >= 0 and x < vram.MAP_WIDTH and y >= 0 and y < vram.MAP_HEIGHT
end

function M.mset(mem, x, y, val)
  if not map_cell_in_range(x, y) then
    return
  end
  mem.map_cells[y][x] = bit.band(val, 0xFF)
  mem.map_version = vram.next_map_version()
end

local function map_cell_get(mem, x, y)
  if not map_cell_in_range(x, y) then
    return 0
  end
  return mem.map_cells[y][x]
end

local function floor_div_sprite(a)
  return a >= 0 and cint.div(a, M.SPRITE_SIZE) or -cint.div(-a + M.SPRITE_SIZE - 1, M.SPRITE_SIZE)
end

local function clamp_cells(i, count)
  return i < 0 and 0 or (i > count and count or i)
end

-- Returns first, end_: the half-open range of the cells 0..count-1 whose
-- 8-pixel span, drawn from origin, meets 0..extent-1 after camera.
local function visible_cells(origin, camera, extent, count)
  local first = clamp_cells(floor_div_sprite(camera - origin), count)
  local end_ = clamp_cells(floor_div_sprite(camera - origin + extent - 1) + 1, count)
  return first, end_
end

-- Issues no spr for a cell whose 8x8 box lies wholly off screen after the camera.
function M.map(gfx, mem, cel_x, cel_y, sx, sy, cel_w, cel_h)
  local c_first, c_end = visible_cells(sx, gfx.camera_x, vram.SCREEN_WIDTH, cel_w)
  local r_first, r_end = visible_cells(sy, gfx.camera_y, vram.SCREEN_HEIGHT, cel_h)
  for r = r_first, r_end - 1 do
    for c = c_first, c_end - 1 do
      local sprite_n = map_cell_get(mem, cel_x + c, cel_y + r)
      if sprite_n == 0 then
        goto continue
      end
      M.spr(gfx, mem, sprite_n, sx + c * M.SPRITE_SIZE, sy + r * M.SPRITE_SIZE, 1, 1)
      ::continue::
    end
  end
end

function M.print(gfx, mem, str, x, y, col)
  backend().print(gfx, mem, str, x, y, col)
end

-- The pixels of eight gfx.print calls in colour 0 at the neighbour offsets
-- (-1,-1), (0,-1), (1,-1), (-1,0), (1,0), (-1,1), (0,1), (1,1), then one
-- gfx.print in col, stored as given. The caller passes the low nibble.
function M.print_outlined(gfx, mem, str, x, y, col)
  backend().print_outlined(gfx, mem, str, x, y, col)
end

return M
