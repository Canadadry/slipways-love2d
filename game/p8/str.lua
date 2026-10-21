-- Copy of src/p8/str.h then src/p8/str.c.
-- Each function performs the C allocation in zone for the C byte count
-- (length plus the terminator) and returns a Lua string; the block is dropped.
local allocator = require("engine.allocator")

local M = {}

function M.p8_sub(zone, s, i, j)
  local len = #s

  if i < 1 then
    i = 1
  end
  if j > len then
    j = len
  end

  if j < i then
    allocator.allocator_alloc(zone, 1)
    return ""
  end

  local n = j - i + 1
  allocator.allocator_alloc(zone, n + 1)
  return string.sub(s, i, j)
end

function M.p8_tostr_fixed(zone, x)
  local negative = (x < 0) and 1 or 0
  local mag = (negative ~= 0) and -x or x
  local ip = math.floor(mag / 0x10000)
  local p = (mag % 0x10000) * 625
  local q = math.floor(p / 4096)
  local r = p % 4096
  if r > 2048 or (r == 2048 and q % 2 == 1) then
    q = q + 1
  end
  if q == 10000 then
    ip = ip + 1
    q = 0
  end
  local buf = string.format("%s%d.%04d", (negative ~= 0) and "-" or "", ip, q)
  local dot = string.find(buf, ".", 1, true)
  if dot ~= nil then
    local last = #buf
    while last > dot and string.byte(buf, last) == string.byte("0") do
      last = last - 1
    end
    if last == dot then
      last = last - 1
    end
    buf = string.sub(buf, 1, last)
  end
  local n = #buf
  allocator.allocator_alloc(zone, n + 1)
  return buf
end

function M.p8_concat2(zone, a, b)
  local la = #a
  local lb = #b
  allocator.allocator_alloc(zone, la + lb + 1)
  return a .. b
end

return M
