-- Every function returns a new { x, y } table and never changes the ones it receives.
local fixed = require("p8.fixed")

local M = {}

function M.make(x, y)
  local result = { x = x, y = y }
  return result
end

function M.add(a, b)
  return M.make(fixed.add(a.x, b.x), fixed.add(a.y, b.y))
end

function M.sub(a, b)
  return M.make(fixed.sub(a.x, b.x), fixed.sub(a.y, b.y))
end

function M.mul(v, m)
  return M.make(fixed.mul(v.x, m), fixed.mul(v.y, m))
end

function M.neg(v)
  return M.make(fixed.neg(v.x), fixed.neg(v.y))
end

function M.dot(a, b)
  return fixed.add(fixed.mul(a.x, b.x), fixed.mul(a.y, b.y))
end

function M.length_squared(v)
  return M.dot(v, v)
end

function M.perpendicular(v)
  return M.make(fixed.neg(v.y), v.x)
end

function M.div(v, d)
  return M.make(fixed.div(v.x, d), fixed.div(v.y, d))
end

function M.length(v)
  return fixed.sqrt(M.length_squared(v))
end

function M.normalize(v)
  return M.div(v, M.length(v))
end

function M.from_polar(fy, lx)
  local unit = M.make(fixed.cos(lx), fixed.sin(lx))
  return M.mul(unit, fy)
end

return M
