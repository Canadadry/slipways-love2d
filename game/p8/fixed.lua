-- Copy of src/p8/fixed.h then src/p8/fixed.c, in the C order. A fixed is a
-- number holding the signed 32-bit raw value of the C p8_fixed_t.
local bit = require("bit")

local M = {}

-- bit.tobit wraps modulo 2^32 only for magnitudes below 2^51.
local tobit = bit.tobit
local band = bit.band
local bor = bit.bor
local lshift = bit.lshift
local arshift = bit.arshift
local floor = math.floor

M.P8_FIXED_SHIFT = 16

M.P8_FIXED_DIV0_POS = 0x7FFFFFFF
M.P8_FIXED_DIV0_NEG = -0x7FFFFFFF

function M.P8_FIXED_LIT(raw, decimal)
  return tobit(raw)
end

function M.p8_fixed_from_int(i)
  return lshift(i, M.P8_FIXED_SHIFT)
end

function M.P8_FIXED_INT(i)
  return lshift(i, M.P8_FIXED_SHIFT)
end

function M.p8_fixed_add(a, b)
  return tobit(a + b)
end

function M.p8_fixed_sub(a, b)
  return tobit(a - b)
end

-- The 64-bit product (up to 2^62) does not fit a double: split into halves.
function M.p8_fixed_mul(a, b)
  local a_high, a_low = arshift(a, 16), band(a, 0xFFFF)
  local b_high, b_low = arshift(b, 16), band(b, 0xFFFF)
  local rescaled = a_high * b_high * 0x10000 + a_high * b_low + a_low * b_high + floor(a_low * b_low / 0x10000)
  return tobit(rescaled)
end

function M.p8_fixed_neg(x)
  return tobit(-x)
end

function M.p8_fixed_flr(x)
  return band(x, 0xFFFF0000)
end

function M.p8_fixed_flr_int(x)
  return M.p8_fixed_flr(x) / 0x10000
end

function M.p8_fixed_abs(x)
  return x < 0 and M.p8_fixed_neg(x) or x
end

function M.p8_fixed_sgn(x)
  return x < 0 and M.p8_fixed_from_int(-1) or M.p8_fixed_from_int(1)
end

function M.p8_fixed_band(a, b)
  return band(a, b)
end

function M.p8_fixed_bor(a, b)
  return bor(a, b)
end

-- Truncating the double quotient is exact: with |numerator| < 2^48 its
-- rounding error stays below the 1/|b| gap to the next integer.
function M.p8_fixed_div(a, b)
  if b == 0 then
    return a < 0 and M.P8_FIXED_DIV0_NEG or M.P8_FIXED_DIV0_POS
  end
  local quotient = a * 0x10000 / b
  return tobit(quotient < 0 and -floor(-quotient) or floor(quotient))
end

function M.p8_fixed_mid(a, b, c)
  if (a <= b and b <= c) or (c <= b and b <= a) then
    return b
  end
  if (b <= a and a <= c) or (c <= a and a <= b) then
    return a
  end
  return c
end

function M.p8_fixed_mod(a, b)
  return M.p8_fixed_sub(a, M.p8_fixed_mul(M.p8_fixed_flr(M.p8_fixed_div(a, b)), b))
end

function M.p8_fixed_shl(x, n)
  if n < 0 or n >= 32 then
    return 0
  end
  return lshift(x, n)
end

return M
