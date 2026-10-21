-- Copy of src/p8/draw_cost.h then src/p8/draw_cost.c. A p8_draw_cost_t is
-- { bytes, primitives }, returned as a new table; a p8_draw_cost_window_t is
-- { patterned, pattern_key }, passed by reference and updated in place.
local bit = require("bit")
local gfx_module = require("p8.gfx")
local vram = require("p8.vram")

local M = {}

-- PSn00bSDK packet sizes on MIPS, tag included.
M.P8_GPU_FILL_BYTES = 16
M.P8_GPU_TILE_1_BYTES = 12
M.P8_GPU_TILE_BYTES = 16
M.P8_GPU_LINE_F2_BYTES = 16
M.P8_GPU_SPRT_BYTES = 20
M.P8_GPU_DR_TPAGE_BYTES = 8
M.P8_GPU_DR_TWIN_BYTES = 8
M.P8_GPU_DR_ENV_BYTES = 36
M.P8_GPU_POLY_FT4_BYTES = 40
M.P8_GPU_DR_AREA_BYTES = 12
M.P8_GPU_DR_OFFSET_BYTES = 8
M.P8_GPU_DR_STP_BYTES = 8
M.P8_GPU_COMMAND_BYTES = 8

-- An upper bound of what the GPU backend may emit for each call, 0 when the
-- call's screen bounding box misses 0..159 x 0..119. Coordinates are the
-- API's, before gfx's camera.

local FREE = { bytes = 0, primitives = 0 }
M.FREE = FREE

local function cost_of(bytes_each, primitives)
  local cost = { bytes = bytes_each * primitives, primitives = primitives }
  return cost
end

local function on_screen(gfx, xmin, ymin, xmax, ymax)
  xmin = xmin - gfx.camera_x
  xmax = xmax - gfx.camera_x
  ymin = ymin - gfx.camera_y
  ymax = ymax - gfx.camera_y
  return (xmax >= 0 and xmin < vram.P8_SCREEN_WIDTH and ymax >= 0 and ymin < vram.P8_SCREEN_HEIGHT) and 1 or 0
end

local function min(a, b)
  return a < b and a or b
end

local function max(a, b)
  return a < b and b or a
end

local function box_on_screen(gfx, x1, y1, x2, y2)
  return on_screen(gfx, min(x1, x2), min(y1, y2), max(x1, x2), max(y1, y2))
end

local function cost_add(a, b)
  return { bytes = a.bytes + b.bytes, primitives = a.primitives + b.primitives }
end

local function window_pair()
  local pair = { bytes = M.P8_GPU_DR_TPAGE_BYTES + M.P8_GPU_DR_TWIN_BYTES, primitives = 2 }
  return pair
end

local function window_switch(window, patterned, pattern_key)
  if window.patterned == patterned and window.pattern_key == pattern_key then
    return { bytes = FREE.bytes, primitives = FREE.primitives }
  end
  window.patterned = patterned
  window.pattern_key = pattern_key
  return window_pair()
end

local function unpatterned_window(window)
  return window_switch(window, 0, 0)
end

-- Resets window to the frame's initial state and returns the fixed per-frame
-- overhead (draw environments, texture state, x2 upscale quad).
function M.p8_draw_cost_frame_begin(window)
  window.patterned = 0
  window.pattern_key = 0
  local overhead = cost_add(cost_of(M.P8_GPU_DR_ENV_BYTES, 2), window_pair())
  return cost_add(overhead, cost_of(M.P8_GPU_POLY_FT4_BYTES, 1))
end

-- What tells two pattern slots apart: the pattern, the colour byte and the transparent flag.
function M.p8_draw_cost_pattern_key(gfx, col)
  return bit.bor(bit.band(gfx.fillp_pattern, 0xFFFF), bit.lshift(bit.band(col, 0xFF), 16),
    bit.lshift(gfx.fillp_transparent ~= 0 and 1 or 0, 24))
end

local function fill_spans(window, gfx, col, spans)
  if bit.band(gfx.fillp_pattern, 0xFFFF) == 0 then
    return cost_of(M.P8_GPU_TILE_BYTES, spans)
  end
  local pair = window_switch(window, 1, M.p8_draw_cost_pattern_key(gfx, col))
  return cost_add(pair, cost_of(M.P8_GPU_SPRT_BYTES, spans))
end

function M.p8_draw_cost_cls()
  return cost_of(M.P8_GPU_FILL_BYTES, 1)
end

function M.p8_draw_cost_pset(gfx, x, y)
  if on_screen(gfx, x, y, x, y) ~= 0 then
    return cost_of(M.P8_GPU_TILE_1_BYTES, 1)
  end
  return { bytes = FREE.bytes, primitives = FREE.primitives }
end

function M.p8_draw_cost_line(gfx, x1, y1, x2, y2)
  if box_on_screen(gfx, x1, y1, x2, y2) ~= 0 then
    return cost_of(M.P8_GPU_LINE_F2_BYTES, 1)
  end
  return { bytes = FREE.bytes, primitives = FREE.primitives }
end

function M.p8_draw_cost_rect(gfx, x1, y1, x2, y2)
  if box_on_screen(gfx, x1, y1, x2, y2) ~= 0 then
    return cost_of(M.P8_GPU_TILE_BYTES, 4)
  end
  return { bytes = FREE.bytes, primitives = FREE.primitives }
end

function M.p8_draw_cost_rectfill(window, gfx, x1, y1, x2, y2, col)
  if box_on_screen(gfx, x1, y1, x2, y2) ~= 0 then
    return fill_spans(window, gfx, col, 1)
  end
  return { bytes = FREE.bytes, primitives = FREE.primitives }
end

-- Must count the iterations of gfx_software.c's midpoint loop, or circles go undercounted.
local function circle_iterations(r)
  local x = r
  local y = 0
  local err = 0
  local iterations = 0
  while x >= y do
    iterations = iterations + 1
    y = y + 1
    if err <= 0 then
      err = err + 2 * y + 1
    end
    if err > 0 then
      x = x - 1
      err = err - (2 * x + 1)
    end
  end
  return iterations
end

local function circle_on_screen(gfx, cx, cy, r)
  return (r >= 0 and on_screen(gfx, cx - r, cy - r, cx + r, cy + r) ~= 0) and 1 or 0
end

function M.p8_draw_cost_circ(gfx, cx, cy, r)
  if circle_on_screen(gfx, cx, cy, r) ~= 0 then
    return cost_of(M.P8_GPU_TILE_1_BYTES, 8 * circle_iterations(r))
  end
  return { bytes = FREE.bytes, primitives = FREE.primitives }
end

function M.p8_draw_cost_circfill(window, gfx, cx, cy, r, col)
  if circle_on_screen(gfx, cx, cy, r) ~= 0 then
    return fill_spans(window, gfx, col, 4 * circle_iterations(r))
  end
  return { bytes = FREE.bytes, primitives = FREE.primitives }
end

function M.p8_draw_cost_spr(window, gfx, n, x, y, w, h)
  if w <= 0 or h <= 0
    or on_screen(gfx, x, y, x + w * gfx_module.P8_SPRITE_SIZE - 1, y + h * gfx_module.P8_SPRITE_SIZE - 1) == 0 then
    return { bytes = FREE.bytes, primitives = FREE.primitives }
  end
  return cost_add(unpatterned_window(window), cost_of(M.P8_GPU_SPRT_BYTES, 1))
end

-- Only the space glyph has no lit row. margin widens the box on every side.
local function text_cost(window, gfx, str, x, y, margin)
  local length = 0
  local glyphs = 0
  for p = 1, #str do
    length = length + 1
    glyphs = glyphs + (string.byte(str, p) ~= string.byte(" ") and 1 or 0)
  end
  local right = x + (length - 1) * gfx_module.P8_FONT_CELL_WIDTH + gfx_module.P8_FONT_GLYPH_WIDTH - 1
  if glyphs == 0
    or on_screen(gfx, x - margin, y - margin, right + margin, y + gfx_module.P8_FONT_GLYPH_HEIGHT - 1 + margin) == 0 then
    return { bytes = FREE.bytes, primitives = FREE.primitives }
  end
  return cost_add(unpatterned_window(window), cost_of(M.P8_GPU_SPRT_BYTES, glyphs))
end

function M.p8_draw_cost_print(window, gfx, str, x, y)
  return text_cost(window, gfx, str, x, y, 0)
end

-- One outlined sprite per non-space glyph; the box reaches one pixel further than print's on each side.
function M.p8_draw_cost_print_outlined(window, gfx, str, x, y)
  return text_cost(window, gfx, str, x, y, 1)
end

-- A build's own packets, all in the build buffer: the clear and the mask bit
-- on, each window's draw area and offset, then the frame's area and offset,
-- the mask bit off and the texture cache flush.
function M.p8_draw_cost_image_begin()
  local cost = { bytes = M.P8_GPU_FILL_BYTES + M.P8_GPU_DR_STP_BYTES, primitives = 2 }
  return cost
end

function M.p8_draw_cost_image_window()
  local cost = { bytes = M.P8_GPU_DR_AREA_BYTES + M.P8_GPU_DR_OFFSET_BYTES, primitives = 2 }
  return cost
end

function M.p8_draw_cost_image_end()
  local cost = {
    bytes = M.P8_GPU_DR_AREA_BYTES + M.P8_GPU_DR_OFFSET_BYTES + M.P8_GPU_DR_STP_BYTES + M.P8_GPU_COMMAND_BYTES,
    primitives = 4,
  }
  return cost
end

-- The copy of a w x h image at world (x, y); it leaves no texture window.
function M.p8_draw_cost_image_draw(window, gfx, x, y, w, h)
  if w <= 0 or h <= 0 or on_screen(gfx, x, y, x + w - 1, y + h - 1) == 0 then
    return { bytes = FREE.bytes, primitives = FREE.primitives }
  end
  window.patterned = 0
  window.pattern_key = 0
  local cost = { bytes = M.P8_GPU_DR_TWIN_BYTES + 2 * M.P8_GPU_DR_TPAGE_BYTES + M.P8_GPU_SPRT_BYTES, primitives = 4 }
  return cost
end

return M
