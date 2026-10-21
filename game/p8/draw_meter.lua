-- Copy of src/p8/draw_meter.h then src/p8/draw_meter.c. A
-- p8_draw_meter_counts_t is { bytes, build_bytes, primitives, cluts,
-- patterns }; build_bytes is what image builds emit, in their own buffer,
-- which bytes and primitives leave out.
local bit = require("bit")
local draw_cost = require("p8.draw_cost")
local gfx_module = require("p8.gfx")
local image = require("p8.image")

local M = {}

-- src/p8/gpu_sizes.h's two table sizes; gpu_sizes.h itself is not ported.
local GPU_CLUT_SLOTS = 6
local GPU_PATTERN_SLOTS = 4

local wrapped = nil
local window = { patterned = 0, pattern_key = 0 }
local frame = { bytes = 0, build_bytes = 0, primitives = 0, cluts = 0, patterns = 0 }
local last_frame = { bytes = 0, build_bytes = 0, primitives = 0, cluts = 0, patterns = 0 }
local peak = { bytes = 0, build_bytes = 0, primitives = 0, cluts = 0, patterns = 0 }
local building = 0
local built = {}
for slot = 0, image.P8_IMAGE_SLOT_COUNT - 1 do
  built[slot] = { width = 0, height = 0 }
end

local CLUT_TABLE = 2 * GPU_CLUT_SLOTS
local PATTERN_TABLE = 2 * GPU_PATTERN_SLOTS

local cluts = {}
for i = 0, CLUT_TABLE - 1 do
  cluts[i] = {}
  for c = 0, 15 do
    cluts[i][c] = 0
  end
end
local pattern_keys = {}
for i = 0, PATTERN_TABLE - 1 do
  pattern_keys[i] = 0
end

local function count_clut(mem)
  for i = 0, frame.cluts - 1 do
    local same = true
    for c = 0, 15 do
      if cluts[i][c] ~= mem.draw_palette[c] then
        same = false
        break
      end
    end
    if same then
      return
    end
  end
  if frame.cluts < CLUT_TABLE then
    for c = 0, 15 do
      cluts[frame.cluts][c] = mem.draw_palette[c]
    end
    frame.cluts = frame.cluts + 1
  end
end

local function count_pattern(gfx, col)
  if bit.band(gfx.fillp_pattern, 0xFFFF) == 0 then
    return
  end
  local key = draw_cost.p8_draw_cost_pattern_key(gfx, col)
  for i = 0, frame.patterns - 1 do
    if pattern_keys[i] == key then
      return
    end
  end
  if frame.patterns < PATTERN_TABLE then
    pattern_keys[frame.patterns] = key
    frame.patterns = frame.patterns + 1
  end
end

local function add(cost)
  if building ~= 0 then
    frame.build_bytes = frame.build_bytes + cost.bytes
    return
  end
  frame.bytes = frame.bytes + cost.bytes
  frame.primitives = frame.primitives + cost.primitives
end

local function frame_begin()
  frame.bytes = 0
  frame.build_bytes = 0
  frame.primitives = 0
  frame.cluts = 0
  frame.patterns = 0
  add(draw_cost.p8_draw_cost_frame_begin(window))
end

local function meter_cls(mem)
  add(draw_cost.p8_draw_cost_cls())
  wrapped.cls(mem)
end

local function meter_pset(gfx, mem, x, y, col)
  add(draw_cost.p8_draw_cost_pset(gfx, x, y))
  wrapped.pset(gfx, mem, x, y, col)
end

local function meter_line(gfx, mem, x1, y1, x2, y2, col)
  add(draw_cost.p8_draw_cost_line(gfx, x1, y1, x2, y2))
  wrapped.line(gfx, mem, x1, y1, x2, y2, col)
end

local function meter_rect(gfx, mem, x1, y1, x2, y2, col)
  add(draw_cost.p8_draw_cost_rect(gfx, x1, y1, x2, y2))
  wrapped.rect(gfx, mem, x1, y1, x2, y2, col)
end

local function meter_rectfill(gfx, mem, x1, y1, x2, y2, col)
  add(draw_cost.p8_draw_cost_rectfill(window, gfx, x1, y1, x2, y2, col))
  count_pattern(gfx, col)
  wrapped.rectfill(gfx, mem, x1, y1, x2, y2, col)
end

local function meter_circ(gfx, mem, cx, cy, r, col)
  add(draw_cost.p8_draw_cost_circ(gfx, cx, cy, r))
  wrapped.circ(gfx, mem, cx, cy, r, col)
end

local function meter_circfill(gfx, mem, cx, cy, r, col)
  add(draw_cost.p8_draw_cost_circfill(window, gfx, cx, cy, r, col))
  count_pattern(gfx, col)
  wrapped.circfill(gfx, mem, cx, cy, r, col)
end

local function meter_spr(gfx, mem, n, x, y, w, h)
  add(draw_cost.p8_draw_cost_spr(window, gfx, n, x, y, w, h))
  count_clut(mem)
  wrapped.spr(gfx, mem, n, x, y, w, h)
end

local function meter_print(gfx, mem, str, x, y, col)
  add(draw_cost.p8_draw_cost_print(window, gfx, str, x, y))
  wrapped.print(gfx, mem, str, x, y, col)
end

local function meter_print_outlined(gfx, mem, str, x, y, col)
  add(draw_cost.p8_draw_cost_print_outlined(window, gfx, str, x, y))
  wrapped.print_outlined(gfx, mem, str, x, y, col)
end

local function meter_image_begin(slot, w, h)
  building = 1
  built[slot].width = w
  built[slot].height = h
  add(draw_cost.p8_draw_cost_image_begin())
  wrapped.image_begin(slot, w, h)
end

local function meter_image_window(slot, x, y)
  add(draw_cost.p8_draw_cost_image_window())
  wrapped.image_window(slot, x, y)
end

local function meter_image_end(slot)
  add(draw_cost.p8_draw_cost_image_end())
  building = 0
  wrapped.image_end(slot)
end

local function meter_image_draw(gfx, mem, slot, x, y)
  add(draw_cost.p8_draw_cost_image_draw(window, gfx, x, y, built[slot].width, built[slot].height))
  wrapped.image_draw(gfx, mem, slot, x, y)
end

local function meter_lines(gfx, mem, segments, count, col)
  for i = 0, count - 1 do
    add(draw_cost.p8_draw_cost_line(gfx, segments[i].x1, segments[i].y1, segments[i].x2, segments[i].y2))
  end
  wrapped.lines(gfx, mem, segments, count, col)
end

local function meter_psets(gfx, mem, points, count)
  for i = 0, count - 1 do
    add(draw_cost.p8_draw_cost_pset(gfx, points[i].x, points[i].y))
  end
  wrapped.psets(gfx, mem, points, count)
end

local METER = {
  cls = meter_cls, pset = meter_pset, line = meter_line, rect = meter_rect, rectfill = meter_rectfill,
  circ = meter_circ, circfill = meter_circfill, spr = meter_spr, print = meter_print,
  print_outlined = meter_print_outlined,
  image_begin = meter_image_begin, image_window = meter_image_window, image_end = meter_image_end,
  image_draw = meter_image_draw,
  lines = meter_lines, psets = meter_psets,
}

-- Installs the meter around the backend active at the call and zeroes every
-- count. Returns the wrapped backend: reinstalling it with
-- p8_draw_backend_set removes the meter.
function M.p8_draw_meter_install()
  local active = gfx_module.p8_draw_backend_get()
  assert(active ~= METER, "the meter would wrap itself")
  wrapped = active
  last_frame.bytes = 0
  last_frame.build_bytes = 0
  last_frame.primitives = 0
  last_frame.cluts = 0
  last_frame.patterns = 0
  peak.bytes = 0
  peak.build_bytes = 0
  peak.primitives = 0
  peak.cluts = 0
  peak.patterns = 0
  building = 0
  frame_begin()
  gfx_module.p8_draw_backend_set(METER)
  return wrapped
end

local function larger(a, b)
  return a < b and b or a
end

function M.p8_draw_meter_frame_end()
  last_frame = {
    bytes = frame.bytes, build_bytes = frame.build_bytes, primitives = frame.primitives,
    cluts = frame.cluts, patterns = frame.patterns,
  }
  peak.bytes = frame.bytes > peak.bytes and frame.bytes or peak.bytes
  peak.build_bytes = frame.build_bytes > peak.build_bytes and frame.build_bytes or peak.build_bytes
  peak.primitives = larger(peak.primitives, frame.primitives)
  peak.cluts = larger(peak.cluts, frame.cluts)
  peak.patterns = larger(peak.patterns, frame.patterns)
  frame_begin()
end

-- Zero until the first p8_draw_meter_frame_end. Returns a new table.
function M.p8_draw_meter_last_frame()
  return {
    bytes = last_frame.bytes, build_bytes = last_frame.build_bytes, primitives = last_frame.primitives,
    cluts = last_frame.cluts, patterns = last_frame.patterns,
  }
end

-- Each field's own peak over the frames ended since the install. Returns a new table.
function M.p8_draw_meter_peak()
  return {
    bytes = peak.bytes, build_bytes = peak.build_bytes, primitives = peak.primitives,
    cluts = peak.cluts, patterns = peak.patterns,
  }
end

return M
