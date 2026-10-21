-- The LÖVE draw backend: a p8_draw_backend_t (see p8/gfx.lua) drawing onto
-- the game canvas of love.screen. Like a C backend it reads gfx and mem and
-- never writes them; it applies the camera and clips to the screen itself.
local bit = require("bit")
local fatal = require("p8.fatal")
local vram = require("p8.vram")
local screen = require("love.screen")

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
  for i = 0, count - 1 do
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
  for i = 0, count - 1 do
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

-- In the current colour, the world rectangle with inclusive corners
-- (x0, y0) <= (x1, y1), under the camera and clipped to the screen.
local function fill_rectangle(gfx, x0, y0, x1, y1)
  local sx0 = max(x0 - gfx.camera_x, 0)
  local sx1 = min(x1 - gfx.camera_x, vram.P8_SCREEN_WIDTH - 1)
  local sy0 = max(y0 - gfx.camera_y, 0)
  local sy1 = min(y1 - gfx.camera_y, vram.P8_SCREEN_HEIGHT - 1)
  if sx0 > sx1 or sy0 > sy1 then
    return
  end
  love.graphics.rectangle("fill", sx0, sy0, sx1 - sx0 + 1, sy1 - sy0 + 1)
end

-- The C's fillp_color with no pattern: the low nibble, never a fatal.
local function love_rectfill(gfx, mem, x1, y1, x2, y2, col)
  -- TODO: fill patterns, with love.pattern.
  assert(gfx.fillp_pattern == 0, "love backend rectfill: fill patterns are not drawn yet")
  set_color(bit.band(col, 0x0F))
  fill_rectangle(gfx, min(x1, x2), min(y1, y2), max(x1, x2), max(y1, y2))
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
-- rectangle, in the C's fillp_color colour with no pattern.
local function love_circfill(gfx, mem, cx, cy, r, col)
  -- TODO: fill patterns, with love.pattern.
  assert(gfx.fillp_pattern == 0, "love backend circfill: fill patterns are not drawn yet")
  if r < 0 then
    return
  end
  set_color(bit.band(col, 0x0F))
  local x = r
  local y = 0
  local err = 0
  while x >= y do
    fill_rectangle(gfx, cx - x, cy + y, cx + x, cy + y)
    fill_rectangle(gfx, cx - x, cy - y, cx + x, cy - y)
    fill_rectangle(gfx, cx - y, cy + x, cx + y, cy + x)
    fill_rectangle(gfx, cx - y, cy - x, cx + y, cy - x)
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

local LOVE_BACKEND = {
  cls = love_cls, pset = love_pset, line = love_line, rect = love_rect, circ = love_circ, lines = love_lines, psets = love_psets, rectfill = love_rectfill, circfill = love_circfill,
}

-- Shared by every caller: never write it.
function M.backend_get()
  return LOVE_BACKEND
end

return M
