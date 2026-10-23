-- The LÖVE draw backend: a p8_draw_backend_t (see p8/gfx.lua) drawing onto
-- the game canvas of love.screen. Like a C backend it reads gfx and mem and
-- never writes them; it applies the camera and clips to the screen itself.
local bit = require("bit")
local cint = require("p8.cint")
local fatal = require("p8.fatal")
local gfx_module = require("p8.gfx")
local gfx_software = require("p8.gfx_software")
local vram = require("p8.vram")
local pattern = require("love.pattern")
local screen = require("love.screen")
local sheet = require("love.sheet")
local slot = require("love.slot")
local text = require("love.text")

local M = {}

-- The C stores any byte and only devtools/dump.c's assert catches one above
-- 15; a LÖVE colour has to be one of the 16.
local function check_color(call, col)
  if col < 0 or col > 15 or col ~= math.floor(col) then
    fatal.p8_fatal(string.format("love backend %s: colour %s is not 0..15", call, tostring(col)))
  end
end

local function set_color(col)
  local color = screen.SCREEN_COLORS[col]
  love.graphics.setColor(color.red, color.green, color.blue)
end

local function on_screen(sx, sy)
  return sx >= 0 and sx < vram.P8_SCREEN_WIDTH and sy >= 0 and sy < vram.P8_SCREEN_HEIGHT
end

local function love_cls(mem)
  love.graphics.clear(0, 0, 0)
end

local function love_pset(gfx, mem, x, y, col)
  check_color("pset", col)
  local sx = x - gfx.camera_x
  local sy = y - gfx.camera_y
  if not on_screen(sx, sy) then
    return
  end
  set_color(col)
  love.graphics.points(sx + 0.5, sy + 0.5)
end

local function flush_points(coordinates, col)
  if #coordinates == 0 then
    return
  end
  set_color(col)
  love.graphics.points(coordinates)
end

-- One points call per run of on-screen points sharing a colour, in list order.
local function love_psets(gfx, mem, points, count)
  local coordinates = {}
  local run_col = nil
  for i = 1, count do
    local point = points[i]
    check_color("psets", point.col)
    local sx = point.x - gfx.camera_x
    local sy = point.y - gfx.camera_y
    if on_screen(sx, sy) then
      if point.col ~= run_col then
        flush_points(coordinates, run_col)
        coordinates = {}
        run_col = point.col
      end
      coordinates[#coordinates + 1] = sx + 0.5
      coordinates[#coordinates + 1] = sy + 0.5
    end
  end
  flush_points(coordinates, run_col)
end

local function append_point(gfx, coordinates, x, y)
  local sx = x - gfx.camera_x
  local sy = y - gfx.camera_y
  if on_screen(sx, sy) then
    coordinates[#coordinates + 1] = sx + 0.5
    coordinates[#coordinates + 1] = sy + 0.5
  end
end

-- src/p8/gfx_software.c's software_line loop, copied: appends the on-screen
-- pixels of one segment to coordinates as point centres.
local function line_points(gfx, coordinates, x1, y1, x2, y2)
  local dx = math.abs(x2 - x1)
  local sx = x1 < x2 and 1 or -1
  local dy = -math.abs(y2 - y1)
  local sy = y1 < y2 and 1 or -1
  local err = dx + dy
  local x = x1
  local y = y1
  while true do
    append_point(gfx, coordinates, x, y)
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

local function love_line(gfx, mem, x1, y1, x2, y2, col)
  check_color("line", col)
  local coordinates = {}
  line_points(gfx, coordinates, x1, y1, x2, y2)
  flush_points(coordinates, col)
end

local function love_lines(gfx, mem, segments, count, col)
  check_color("lines", col)
  for i = 1, count do
    local segment = segments[i]
    local coordinates = {}
    line_points(gfx, coordinates, segment.x1, segment.y1, segment.x2, segment.y2)
    flush_points(coordinates, col)
  end
end

-- src/p8/gfx_software.c's software_circ loop and circ_plot8, copied: one
-- points call for the whole circle.
local function love_circ(gfx, mem, cx, cy, r, col)
  check_color("circ", col)
  if r < 0 then
    return
  end
  local coordinates = {}
  local x = r
  local y = 0
  local err = 0
  while x >= y do
    append_point(gfx, coordinates, cx + x, cy + y)
    append_point(gfx, coordinates, cx - x, cy + y)
    append_point(gfx, coordinates, cx + x, cy - y)
    append_point(gfx, coordinates, cx - x, cy - y)
    append_point(gfx, coordinates, cx + y, cy + x)
    append_point(gfx, coordinates, cx - y, cy + x)
    append_point(gfx, coordinates, cx + y, cy - x)
    append_point(gfx, coordinates, cx - y, cy - x)
    y = y + 1
    if err <= 0 then
      err = err + 2 * y + 1
    end
    if err > 0 then
      x = x - 1
      err = err - (2 * x + 1)
    end
  end
  flush_points(coordinates, col)
end

local function min(a, b)
  return a < b and a or b
end

local function max(a, b)
  return a < b and b or a
end

-- Clips the world rectangle with inclusive corners (x0, y0) <= (x1, y1) to
-- the screen under the camera; returns its screen corners, or nil when
-- nothing is left.
local function clip_rectangle(gfx, x0, y0, x1, y1)
  local sx0 = max(x0 - gfx.camera_x, 0)
  local sx1 = min(x1 - gfx.camera_x, vram.P8_SCREEN_WIDTH - 1)
  local sy0 = max(y0 - gfx.camera_y, 0)
  local sy1 = min(y1 - gfx.camera_y, vram.P8_SCREEN_HEIGHT - 1)
  if sx0 > sx1 or sy0 > sy1 then
    return nil
  end
  return sx0, sy0, sx1, sy1
end

-- In the current colour, the world rectangle with inclusive corners
-- (x0, y0) <= (x1, y1), under the camera and clipped to the screen.
local function fill_rectangle(gfx, x0, y0, x1, y1)
  local sx0, sy0, sx1, sy1 = clip_rectangle(gfx, x0, y0, x1, y1)
  if sx0 == nil then
    return
  end
  love.graphics.rectangle("fill", sx0, sy0, sx1 - sx0 + 1, sy1 - sy0 + 1)
end

-- Selects how fill_span draws, as the C's fillp_color: returns the pattern
-- image (colour set to white), or nil after setting the colour col & 0x0F.
local function begin_fill(gfx, col)
  if gfx.fillp_pattern == 0 then
    set_color(bit.band(col, 0x0F))
    return nil
  end
  love.graphics.setColor(1, 1, 1)
  return pattern.pattern_image(gfx, col)
end

-- The world rectangle (x0, y0) <= (x1, y1) as begin_fill chose: one integer
-- rectangle, or one quad of the pattern image cut at the world phase of its
-- first pixel, since the C aligns the pattern before the camera.
local function fill_span(gfx, image, x0, y0, x1, y1)
  if image == nil then
    fill_rectangle(gfx, x0, y0, x1, y1)
    return
  end
  local sx0, sy0, sx1, sy1 = clip_rectangle(gfx, x0, y0, x1, y1)
  if sx0 == nil then
    return
  end
  local quad = love.graphics.newQuad((sx0 + gfx.camera_x) % 4, (sy0 + gfx.camera_y) % 4, sx1 - sx0 + 1, sy1 - sy0 + 1,
    pattern.PATTERN_IMAGE_WIDTH, pattern.PATTERN_IMAGE_HEIGHT)
  love.graphics.draw(image, quad, sx0, sy0)
end

local function love_rectfill(gfx, mem, x1, y1, x2, y2, col)
  local image = begin_fill(gfx, col)
  fill_span(gfx, image, min(x1, x2), min(y1, y2), max(x1, x2), max(y1, y2))
end

local function love_rect(gfx, mem, x1, y1, x2, y2, col)
  check_color("rect", col)
  local xmin = min(x1, x2)
  local xmax = max(x1, x2)
  local ymin = min(y1, y2)
  local ymax = max(y1, y2)
  set_color(col)
  fill_rectangle(gfx, xmin, ymin, xmax, ymin)
  fill_rectangle(gfx, xmin, ymax, xmax, ymax)
  fill_rectangle(gfx, xmin, ymin, xmin, ymax)
  fill_rectangle(gfx, xmax, ymin, xmax, ymax)
end

-- src/p8/gfx_software.c's software_circfill loop, copied: each span is one
-- fill_span.
local function love_circfill(gfx, mem, cx, cy, r, col)
  if r < 0 then
    return
  end
  local image = begin_fill(gfx, col)
  local x = r
  local y = 0
  local err = 0
  while x >= y do
    fill_span(gfx, image, cx - x, cy + y, cx + x, cy + y)
    fill_span(gfx, image, cx - x, cy - y, cx + x, cy - y)
    fill_span(gfx, image, cx - y, cy + x, cx + y, cy + x)
    fill_span(gfx, image, cx - y, cy - x, cx + y, cy - x)
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

-- src/p8/gfx_software.c's software_spr: the part of the box on the sheet is
-- one quad of the recoloured sheet; the part off it reads colour 0 (sheet_get),
-- so it is up to four rectangles of draw palette entry 0, or nothing.
local function love_spr(gfx, mem, n, x, y, w, h)
  if w <= 0 or h <= 0 then
    return
  end
  local sprites_per_row = cint.div(vram.P8_SPRITE_SHEET_WIDTH, gfx_module.P8_SPRITE_SIZE)
  local sheet_x = cint.mod(n, sprites_per_row) * gfx_module.P8_SPRITE_SIZE
  local sheet_y = cint.div(n, sprites_per_row) * gfx_module.P8_SPRITE_SIZE
  local box_x1 = x + w * gfx_module.P8_SPRITE_SIZE - 1
  local box_y1 = y + h * gfx_module.P8_SPRITE_SIZE - 1
  local wx0 = x + max(sheet_x, 0) - sheet_x
  local wy0 = y + max(sheet_y, 0) - sheet_y
  local wx1 = min(box_x1, x + vram.P8_SPRITE_SHEET_WIDTH - 1 - sheet_x)
  local wy1 = min(box_y1, y + vram.P8_SPRITE_SHEET_HEIGHT - 1 - sheet_y)
  local on_sheet = wx0 <= wx1 and wy0 <= wy1
  if on_sheet then
    local sx0, sy0, sx1, sy1 = clip_rectangle(gfx, wx0, wy0, wx1, wy1)
    if sx0 ~= nil then
      local qx = sheet_x + sx0 + gfx.camera_x - x
      local qy = sheet_y + sy0 + gfx.camera_y - y
      local qw = sx1 - sx0 + 1
      local qh = sy1 - sy0 + 1
      local image = sheet.sheet_image(mem, qx, qy, qx + qw - 1, qy + qh - 1)
      local quad = love.graphics.newQuad(qx, qy, qw, qh, vram.P8_SPRITE_SHEET_WIDTH, vram.P8_SPRITE_SHEET_HEIGHT)
      love.graphics.setColor(1, 1, 1)
      love.graphics.draw(image, quad, sx0, sy0)
    end
  end
  local entry = mem.draw_palette[0]
  if bit.band(entry, vram.P8_DRAW_PALETTE_TRANSPARENT_BIT) ~= 0 then
    return
  end
  set_color(bit.band(entry, 0x0F))
  if not on_sheet then
    fill_rectangle(gfx, x, y, box_x1, box_y1)
    return
  end
  fill_rectangle(gfx, x, y, box_x1, wy0 - 1)
  fill_rectangle(gfx, x, wy1 + 1, box_x1, box_y1)
  fill_rectangle(gfx, x, wy0, wx0 - 1, wy1)
  fill_rectangle(gfx, wx1 + 1, wy0, box_x1, wy1)
end

local function love_print(gfx, mem, str, x, y, col)
  check_color("print", col)
  set_color(col)
  text.text_draw(str, x - gfx.camera_x, y - gfx.camera_y)
end

-- src/p8/gfx_software.c's software_print_outlined order, copied.
local function love_print_outlined(gfx, mem, str, x, y, col)
  for dy = -1, 1 do
    for dx = -1, 1 do
      if dx ~= 0 or dy ~= 0 then
        love_print(gfx, mem, str, x + dx, y + dy, 0)
      end
    end
  end
  love_print(gfx, mem, str, x, y, col)
end

local SOFTWARE_BACKEND = gfx_software.p8_gfx_software_backend()
local building = false

local function love_image_begin(slot_index, w, h)
  building = true
  SOFTWARE_BACKEND.image_begin(slot_index, w, h)
end

local function love_image_window(slot_index, x, y)
  SOFTWARE_BACKEND.image_window(slot_index, x, y)
end

local function love_image_end(slot_index)
  SOFTWARE_BACKEND.image_end(slot_index)
  building = false
  slot.slot_build(slot_index)
end

local function love_image_draw(gfx, mem, slot_index, x, y)
  slot.slot_draw(slot_index, x - gfx.camera_x, y - gfx.camera_y)
end

-- Between image_begin and image_end the software backend draws into the slot
-- store, which love_image_end turns into the slot's image.
local function building_forwards(name, draw)
  local software_draw = SOFTWARE_BACKEND[name]
  return function(...)
    if building then
      return software_draw(...)
    end
    return draw(...)
  end
end

local LOVE_BACKEND = {
  cls = love_cls, pset = building_forwards("pset", love_pset), line = building_forwards("line", love_line),
  rect = building_forwards("rect", love_rect), rectfill = building_forwards("rectfill", love_rectfill),
  circ = building_forwards("circ", love_circ), circfill = building_forwards("circfill", love_circfill),
  spr = building_forwards("spr", love_spr), print = building_forwards("print", love_print),
  print_outlined = building_forwards("print_outlined", love_print_outlined),
  image_begin = love_image_begin, image_window = love_image_window, image_end = love_image_end,
  image_draw = love_image_draw,
  lines = building_forwards("lines", love_lines), psets = building_forwards("psets", love_psets),
}

-- Shared by every caller: never write it.
function M.backend_get()
  return LOVE_BACKEND
end

return M
