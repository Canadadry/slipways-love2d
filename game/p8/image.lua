-- Copy of src/p8/image.h then src/p8/image.c. A p8_image_size_t is
-- { width, height }. The width-multiple-of-16 rule is not ported.
local fatal = require("p8.fatal")
local gfx_module = require("p8.gfx")
local vram = require("p8.vram")

local M = {}

-- One SLOT(width, height) call per slot, in slot order. Each side at most 256.
function M.P8_IMAGE_SLOTS(SLOT)
  SLOT(256, 256)
  M.P8_IMAGE_SMALL_SLOTS_8(SLOT)
  M.P8_IMAGE_SMALL_SLOTS_8(SLOT)
  M.P8_IMAGE_SMALL_SLOTS_8(SLOT)
  M.P8_IMAGE_SMALL_SLOTS_8(SLOT)
  M.P8_IMAGE_WIDE_SLOTS_8(SLOT)
  M.P8_IMAGE_WIDE_SLOTS_8(SLOT)
  M.P8_IMAGE_WIDE_SLOTS_8(SLOT)
  M.P8_IMAGE_WIDE_SLOTS_8(SLOT)
  for _ = 1, 5 do
    SLOT(16, 16)
  end
  for _ = 1, 8 do
    SLOT(16, 11)
  end
  for _ = 1, 10 do
    SLOT(112, 11)
  end
end

function M.P8_IMAGE_SMALL_SLOTS_8(SLOT)
  for _ = 1, 8 do
    SLOT(48, 8)
  end
end

function M.P8_IMAGE_WIDE_SLOTS_8(SLOT)
  for _ = 1, 8 do
    SLOT(128, 8)
  end
end

M.P8_IMAGE_SLOT_COUNT = 0
M.P8_IMAGE_PIXELS = 0
M.P8_IMAGE_SLOTS(function(width, height)
  M.P8_IMAGE_SLOT_COUNT = M.P8_IMAGE_SLOT_COUNT + 1
  M.P8_IMAGE_PIXELS = M.P8_IMAGE_PIXELS + width * height
end)

M.P8_IMAGE_WINDOW_WIDTH = vram.P8_SCREEN_WIDTH
M.P8_IMAGE_WINDOW_HEIGHT = vram.P8_SCREEN_HEIGHT

local SLOT_SIZES = {}
local slot_sizes_defined = 0
M.P8_IMAGE_SLOTS(function(width, height)
  SLOT_SIZES[slot_sizes_defined] = { width = width, height = height }
  slot_sizes_defined = slot_sizes_defined + 1
end)
M.SLOT_SIZES = SLOT_SIZES

local built = {}
for slot = 0, M.P8_IMAGE_SLOT_COUNT - 1 do
  built[slot] = false
end
local building_slot = -1
local guarded = nil

local function slot_fatal(slot, what)
  fatal.p8_fatal(string.format("image slot %d: %s", slot, what))
end

local function check_slot(slot)
  if slot < 0 or slot >= M.P8_IMAGE_SLOT_COUNT then slot_fatal(slot, "no such slot") end
end

-- Returns a new p8_image_size_t.
function M.p8_image_size(slot)
  check_slot(slot)
  return { width = SLOT_SIZES[slot].width, height = SLOT_SIZES[slot].height }
end

local function guard_cls(mem)
  slot_fatal(building_slot, "cls during a build")
end

local function guard_pset(gfx, mem, x, y, col)
  guarded.pset(gfx, mem, x, y, col)
end

local function guard_line(gfx, mem, x1, y1, x2, y2, col)
  guarded.line(gfx, mem, x1, y1, x2, y2, col)
end

local function guard_rect(gfx, mem, x1, y1, x2, y2, col)
  guarded.rect(gfx, mem, x1, y1, x2, y2, col)
end

local function guard_rectfill(gfx, mem, x1, y1, x2, y2, col)
  guarded.rectfill(gfx, mem, x1, y1, x2, y2, col)
end

local function guard_circ(gfx, mem, cx, cy, r, col)
  guarded.circ(gfx, mem, cx, cy, r, col)
end

local function guard_circfill(gfx, mem, cx, cy, r, col)
  guarded.circfill(gfx, mem, cx, cy, r, col)
end

local function guard_spr(gfx, mem, n, x, y, w, h)
  guarded.spr(gfx, mem, n, x, y, w, h)
end

local function guard_print(gfx, mem, str, x, y, col)
  guarded.print(gfx, mem, str, x, y, col)
end

local function guard_print_outlined(gfx, mem, str, x, y, col)
  guarded.print_outlined(gfx, mem, str, x, y, col)
end

local function guard_image_begin(slot, w, h)
  slot_fatal(slot, "build inside a build")
end

local function guard_image_window(slot, x, y)
  slot_fatal(slot, "window outside its build")
end

local function guard_image_end(slot)
  slot_fatal(slot, "end outside its build")
end

local function guard_image_draw(gfx, mem, slot, x, y)
  slot_fatal(slot, "draw during a build")
end

local function guard_lines(gfx, mem, segments, count, col)
  guarded.lines(gfx, mem, segments, count, col)
end

local function guard_psets(gfx, mem, points, count)
  guarded.psets(gfx, mem, points, count)
end

-- Stands in front of the backend during a build, which p8_cls cannot otherwise see.
local GUARD = {
  cls = guard_cls, pset = guard_pset, line = guard_line, rect = guard_rect, rectfill = guard_rectfill,
  circ = guard_circ, circfill = guard_circfill, spr = guard_spr, print = guard_print,
  print_outlined = guard_print_outlined,
  image_begin = guard_image_begin, image_window = guard_image_window, image_end = guard_image_end,
  image_draw = guard_image_draw,
  lines = guard_lines, psets = guard_psets,
}
M.GUARD = GUARD

-- Clears the slot, then calls draw(gfx, mem, context) once per window of at
-- most P8_IMAGE_WINDOW_WIDTH x P8_IMAGE_WINDOW_HEIGHT covering w x h,
-- row-major, with gfx's camera at the window's origin: image pixel (x, y) is
-- world (x, y). Restores the camera. draw must be deterministic and touch no
-- game state nor visual_random. A p8_fatal on a size over capacity, a cls, a
-- nested build or a p8_image_draw during the build.
function M.p8_image_build(gfx, mem, slot, w, h, draw, context)
  check_slot(slot)
  if building_slot >= 0 then slot_fatal(slot, "build inside a build") end
  local capacity = { width = SLOT_SIZES[slot].width, height = SLOT_SIZES[slot].height }
  if w < 0 or h < 0 or w > capacity.width or h > capacity.height then
    local what = string.format("%dx%d over its %dx%d", w, h, capacity.width, capacity.height)
    slot_fatal(slot, what)
  end
  local camera_x = gfx.camera_x
  local camera_y = gfx.camera_y
  guarded = gfx_module.p8_draw_backend_set(GUARD)
  building_slot = slot
  guarded.image_begin(slot, w, h)
  for y = 0, h - 1, M.P8_IMAGE_WINDOW_HEIGHT do
    for x = 0, w - 1, M.P8_IMAGE_WINDOW_WIDTH do
      guarded.image_window(slot, x, y)
      gfx_module.p8_camera(gfx, x, y)
      draw(gfx, mem, context)
    end
  end
  guarded.image_end(slot)
  building_slot = -1
  gfx_module.p8_draw_backend_set(guarded)
  built[slot] = true
  gfx_module.p8_camera(gfx, camera_x, camera_y)
end

-- The slot's last build, top-left at world (x, y) under the camera, clipped
-- to the screen; pixels the build never wrote are transparent. A p8_fatal on
-- a slot never built.
function M.p8_image_draw(gfx, mem, slot, x, y)
  check_slot(slot)
  if building_slot >= 0 then slot_fatal(slot, "draw during a build") end
  if not built[slot] then slot_fatal(slot, "drawn before any build") end
  gfx_module.p8_draw_backend_get().image_draw(gfx, mem, slot, x, y)
end

return M
