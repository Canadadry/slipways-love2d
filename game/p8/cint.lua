-- Integer division and remainder truncating toward zero, on Lua numbers
-- holding 32-bit integers. Lua's `%` and math.floor round toward minus infinity.
local M = {}

local floor = math.floor
local ceil = math.ceil

-- Exact for 32-bit operands: the double quotient's rounding error stays
-- below the 1/|b| gap to the next integer.
function M.div(a, b)
  if b == 0 then
    error("cint.div: division by zero", 2)
  end
  local quotient = a / b
  -- + 0 turns -0 (from 0 / -5, or ceil(-0.25)) into 0, which tostring
  -- would print as "-0".
  if quotient >= 0 then
    return floor(quotient) + 0
  end
  return ceil(quotient) + 0
end

function M.mod(a, b)
  if b == 0 then
    error("cint.mod: division by zero", 2)
  end
  return a - M.div(a, b) * b
end

return M
