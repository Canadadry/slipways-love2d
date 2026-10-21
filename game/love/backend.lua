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

local function min(a, b)
  return a < b and a or b
end

local function max(a, b)
  return a < b and b or a
end

-- The C's fillp_color with no pattern: the low nibble, never a fatal.
local function love_rectfill(gfx, mem, x1, y1, x2, y2, col)
  -- TODO: fill patterns, with love.pattern.
  assert(gfx.fillp_pattern == 0, "love backend rectfill: fill patterns are not drawn yet")
  local sx0 = max(min(x1, x2) - gfx.camera_x, 0)
  local sx1 = min(max(x1, x2) - gfx.camera_x, vram.P8_SCREEN_WIDTH - 1)
  local sy0 = max(min(y1, y2) - gfx.camera_y, 0)
  local sy1 = min(max(y1, y2) - gfx.camera_y, vram.P8_SCREEN_HEIGHT - 1)
  if sx0 > sx1 or sy0 > sy1 then
    return
  end
  set_color(bit.band(col, 0x0F))
  love.graphics.rectangle("fill", sx0, sy0, sx1 - sx0 + 1, sy1 - sy0 + 1)
end

local LOVE_BACKEND = {
  cls = love_cls, pset = love_pset, psets = love_psets, rectfill = love_rectfill,
}

-- Shared by every caller: never write it.
function M.backend_get()
  return LOVE_BACKEND
end

return M
