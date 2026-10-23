-- Every function returns a new { x, y } table and never changes the ones it receives.
local fixed = require("p8.fixed")

local M = {}

function M.vec_make(x, y)
  local result = { x = x, y = y }
  return result
end

function M.vec_add(a, b)
  return M.vec_make(fixed.add(a.x, b.x), fixed.add(a.y, b.y))
end

function M.vec_sub(a, b)
  return M.vec_make(fixed.sub(a.x, b.x), fixed.sub(a.y, b.y))
end

function M.vec_mul(v, m)
  return M.vec_make(fixed.mul(v.x, m), fixed.mul(v.y, m))
end

function M.vec_neg(v)
  return M.vec_make(fixed.neg(v.x), fixed.neg(v.y))
end

function M.vec_dot(a, b)
  return fixed.add(fixed.mul(a.x, b.x), fixed.mul(a.y, b.y))
end

function M.vec_length_squared(v)
  return M.vec_dot(v, v)
end

function M.vec_perpendicular(v)
  return M.vec_make(fixed.neg(v.y), v.x)
end

function M.vec_div(v, d)
  return M.vec_make(fixed.div(v.x, d), fixed.div(v.y, d))
end

function M.vec_length(v)
  return fixed.sqrt(M.vec_length_squared(v))
end

function M.vec_normalize(v)
  return M.vec_div(v, M.vec_length(v))
end

function M.vec_from_polar(fy, lx)
  local unit = M.vec_make(fixed.cos(lx), fixed.sin(lx))
  return M.vec_mul(unit, fy)
end

return M
