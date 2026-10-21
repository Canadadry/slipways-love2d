-- Copy of src/engine/geom.h then src/engine/geom.c.
-- A box_t is { left, top, right, bottom }; box_contains receives it by value
-- and leaves it unchanged.
local fixed = require("p8.fixed")
local vec = require("engine.vec")
local allocator = require("engine.allocator")

local M = {}

M.SIZEOF = {
  box_t = 16,
}
local SIZEOF = M.SIZEOF

M.SIDE_ON_LINE = 0
M.SIDE_POSITIVE = 1
M.SIDE_NEGATIVE = -1

-- mid(left, right, x) == x, written out so the hover loop's inline test makes no call.
function M.box_contains(box, p)
  return (((box.left <= p.x and p.x <= box.right) or (box.right <= p.x and p.x <= box.left)) and
          ((box.top <= p.y and p.y <= box.bottom) or (box.bottom <= p.y and p.y <= box.top))) and 1 or 0
end

function M.geom_side_of_line(a, b, c)
  local kl = fixed.p8_fixed_sub(
    fixed.p8_fixed_mul(fixed.p8_fixed_sub(b.y, a.y), fixed.p8_fixed_sub(c.x, b.x)),
    fixed.p8_fixed_mul(fixed.p8_fixed_sub(b.x, a.x), fixed.p8_fixed_sub(c.y, b.y)))
  if kl == 0 then
    return M.SIDE_ON_LINE
  end
  return (fixed.p8_fixed_sgn(kl) > 0) and M.SIDE_POSITIVE or M.SIDE_NEGATIVE
end

function M.geom_segments_cross(p1, p2, q1, q2)
  local q1_side = M.geom_side_of_line(p1, p2, q1)
  local q2_side = M.geom_side_of_line(p1, p2, q2)
  local p1_side = M.geom_side_of_line(q1, q2, p1)
  local p2_side = M.geom_side_of_line(q1, q2, p2)
  return (q1_side ~= M.SIDE_ON_LINE and q2_side ~= M.SIDE_ON_LINE and
          p1_side ~= M.SIDE_ON_LINE and p2_side ~= M.SIDE_ON_LINE and
          q1_side ~= q2_side and p1_side ~= p2_side) and 1 or 0
end

local function geom_inside_32(v)
  return (v > -0x200000 and v < 0x200000) and 1 or 0
end

local function geom_beyond_span(a, b, c)
  return (c < ((a < b) and a or b) - 0x2000 or c > ((a < b) and b or a) + 0x2000) and 1 or 0
end

function M.geom_near_segment(a, b, c, fh)
  -- Trap: a match lies within 0.0628 of the box only for fh <= 1/256 and coordinates inside +-32, where no
  -- product wraps; the 1/8 margin rests on that proof, so widening either needs it redone.
  if fh <= 0x100 and geom_inside_32(a.x) ~= 0 and geom_inside_32(a.y) ~= 0 and geom_inside_32(b.x) ~= 0 and
      geom_inside_32(b.y) ~= 0 and geom_inside_32(c.x) ~= 0 and geom_inside_32(c.y) ~= 0 and
      (geom_beyond_span(a.x, b.x, c.x) ~= 0 or geom_beyond_span(a.y, b.y, c.y) ~= 0) then
    return 0
  end
  local d = vec.vec_sub(b, a)
  local l = fixed.p8_fixed_div(vec.vec_dot(d, vec.vec_sub(c, a)), vec.vec_length_squared(d))
  local lo = fixed.P8_FIXED_LIT(0x0000028f, 0.01)
  local hi = fixed.P8_FIXED_LIT(0x0000fd70, 0.99)
  local projected = vec.vec_sub(vec.vec_add(a, vec.vec_mul(d, l)), c)
  local distance_squared = vec.vec_length_squared(projected)
  return (l > lo and l < hi and distance_squared < fh) and 1 or 0
end

function M.box_make(zone, xl, yt, xr, yb)
  local box = allocator.allocator_alloc(zone, SIZEOF.box_t)
  box.left = xl
  box.top = yt
  box.right = xr
  box.bottom = yb
  return box
end

return M
