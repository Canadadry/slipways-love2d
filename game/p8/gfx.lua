-- Copy of src/p8/gfx.h, src/p8/draw_backend.h then src/p8/gfx.c.
-- A p8_graphics_t is { camera_x, camera_y, fillp_pattern, fillp_transparent,
-- pen_color }. A p8_point_t list is { { x, y, col }, ... } and a
-- p8_segment_t list { { x1, y1, x2, y2 }, ... }, each passed with its
-- count and read from 1 to count. A p8_draw_backend_t is a table of the 16 C fields: cls, pset, line,
-- rect, rectfill, circ, circfill, spr, print, print_outlined, image_begin,
-- image_window, image_end, image_draw, lines, psets.
local bit = require("bit")
local cint = require("p8.cint")
local vram = require("p8.vram")

local M = {}

M.P8_SPRITE_SIZE = 8

M.P8_FONT_GLYPH_WIDTH = 3
M.P8_FONT_GLYPH_HEIGHT = 5
M.P8_FONT_CELL_WIDTH = 4

local active_backend = nil

-- Required at the call: p8.gfx_software requires p8.image, which requires p8.gfx.
local function backend()
  if active_backend ~= nil then
    return active_backend
  end
  return require("p8.gfx_software").p8_gfx_software_backend()
end

-- nil installs the software backend. Returns the backend active before the
-- call, never nil.
function M.p8_draw_backend_set(new_backend)
  local previous = backend()
  active_backend = new_backend
  return previous
end

function M.p8_draw_backend_get()
  return backend()
end

function M.p8_gfx_init(gfx)
  gfx.camera_x = 0
  gfx.camera_y = 0
  gfx.fillp_pattern = 0
  gfx.fillp_transparent = false
  gfx.pen_color = 6
end

function M.p8_cls(mem)
  backend().cls(mem)
end

function M.p8_pset(gfx, mem, x, y, col)
  backend().pset(gfx, mem, x, y, col)
end

function M.p8_psets(gfx, mem, points, count)
  assert(count >= 0 and (count == 0 or points ~= nil), "p8_psets: a negative count or a NULL list")
  backend().psets(gfx, mem, points, count)
end

function M.p8_camera(gfx, x, y)
  gfx.camera_x = x
  gfx.camera_y = y
end

function M.p8_camera_reset(gfx)
  M.p8_camera(gfx, 0, 0)
end

function M.p8_palt(mem, col, transparent)
  assert(type(transparent) == "boolean", "p8_palt: transparent is a boolean, as in PICO-8; 0 would be truthy")
  if col < 0 or col > 15 then
    return
  end
  if transparent then
    mem.draw_palette[col] = bit.band(bit.bor(mem.draw_palette[col], vram.P8_DRAW_PALETTE_TRANSPARENT_BIT), 0xFF)
  else
    mem.draw_palette[col] = bit.band(mem.draw_palette[col], bit.bnot(vram.P8_DRAW_PALETTE_TRANSPARENT_BIT), 0xFF)
  end
end

function M.p8_gfx_is_transparent(mem, col)
  if col < 0 or col > 15 then
    return false
  end
  return bit.band(mem.draw_palette[col], vram.P8_DRAW_PALETTE_TRANSPARENT_BIT) ~= 0
end

function M.p8_line(gfx, mem, x1, y1, x2, y2, col)
  backend().line(gfx, mem, x1, y1, x2, y2, col)
end

function M.p8_lines(gfx, mem, segments, count, col)
  assert(count >= 0 and (count == 0 or segments ~= nil), "p8_lines: a negative count or a NULL list")
  backend().lines(gfx, mem, segments, count, col)
end

function M.p8_rect(gfx, mem, x1, y1, x2, y2, col)
  backend().rect(gfx, mem, x1, y1, x2, y2, col)
end

function M.p8_rectfill(gfx, mem, x1, y1, x2, y2, col)
  backend().rectfill(gfx, mem, x1, y1, x2, y2, col)
end

function M.p8_circ(gfx, mem, cx, cy, r, col)
  backend().circ(gfx, mem, cx, cy, r, col)
end

function M.p8_circfill(gfx, mem, cx, cy, r, col)
  backend().circfill(gfx, mem, cx, cy, r, col)
end

function M.p8_fillp(gfx, pattern)
  gfx.fillp_pattern = bit.band(pattern, 0xFFFF)
  gfx.fillp_transparent = false
end

function M.p8_fillp_transparent(gfx, pattern)
  gfx.fillp_pattern = bit.band(pattern, 0xFFFF)
  gfx.fillp_transparent = true
end

function M.p8_fillp_reset(gfx)
  M.p8_fillp(gfx, 0)
end

function M.p8_sget(mem, x, y)
  if x < 0 or x >= vram.P8_SPRITE_SHEET_WIDTH or y < 0 or y >= vram.P8_SPRITE_SHEET_HEIGHT then
    return 0
  end
  return mem.sprite_sheet[y][x]
end

function M.p8_spr(gfx, mem, n, x, y, w, h)
  backend().spr(gfx, mem, n, x, y, w, h)
end

function M.p8_spr_default(gfx, mem, n, x, y)
  M.p8_spr(gfx, mem, n, x, y, 1, 1)
end

local function map_cell_in_range(x, y)
  return x >= 0 and x < vram.P8_MAP_WIDTH and y >= 0 and y < vram.P8_MAP_HEIGHT
end

function M.p8_mset(mem, x, y, val)
  if not map_cell_in_range(x, y) then
    return
  end
  mem.map_cells[y][x] = bit.band(val, 0xFF)
  mem.map_version = vram.p8_vram_next_map_version()
end

local function map_cell_get(mem, x, y)
  if not map_cell_in_range(x, y) then
    return 0
  end
  return mem.map_cells[y][x]
end

local function floor_div_sprite(a)
  return a >= 0 and cint.div(a, M.P8_SPRITE_SIZE) or -cint.div(-a + M.P8_SPRITE_SIZE - 1, M.P8_SPRITE_SIZE)
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
function M.p8_map(gfx, mem, cel_x, cel_y, sx, sy, cel_w, cel_h)
  local c_first, c_end = visible_cells(sx, gfx.camera_x, vram.P8_SCREEN_WIDTH, cel_w)
  local r_first, r_end = visible_cells(sy, gfx.camera_y, vram.P8_SCREEN_HEIGHT, cel_h)
  for r = r_first, r_end - 1 do
    for c = c_first, c_end - 1 do
      local sprite_n = map_cell_get(mem, cel_x + c, cel_y + r)
      if sprite_n == 0 then
        goto continue
      end
      M.p8_spr(gfx, mem, sprite_n, sx + c * M.P8_SPRITE_SIZE, sy + r * M.P8_SPRITE_SIZE, 1, 1)
      ::continue::
    end
  end
end

function M.p8_print(gfx, mem, str, x, y, col)
  backend().print(gfx, mem, str, x, y, col)
end

-- The pixels of eight p8_print calls in colour 0 at the neighbour offsets
-- (-1,-1), (0,-1), (1,-1), (-1,0), (1,0), (-1,1), (0,1), (1,1), then one
-- p8_print in col, stored as given. The caller passes the low nibble.
function M.p8_print_outlined(gfx, mem, str, x, y, col)
  backend().print_outlined(gfx, mem, str, x, y, col)
end

return M
