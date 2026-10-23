-- Copy of src/p8/str.h then src/p8/str.c.

local M = {}

function M.p8_sub(s, i, j)
  local len = #s

  if i < 1 then
    i = 1
  end
  if j > len then
    j = len
  end

  if j < i then
    return ""
  end

  return string.sub(s, i, j)
end

function M.p8_tostr_fixed(x)
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
  return buf
end

function M.p8_concat2(a, b)
  return a .. b
end

return M
