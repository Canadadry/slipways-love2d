-- Copy of src/p8/fixed.h then src/p8/fixed.c, in the C order. A fixed is a
-- number holding the signed 32-bit raw value of the C p8_fixed_t.
local bit = require("bit")
local p8_quarter_wave = require("p8.quarter_wave")

local M = {}

-- bit.tobit wraps modulo 2^32 only for magnitudes below 2^51.
local tobit = bit.tobit
local band = bit.band
local bor = bit.bor
local lshift = bit.lshift
local arshift = bit.arshift
local rshift = bit.rshift
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

-- mid * mid rounds above 2^53, yet compares right: n stays below 2^48.
local function isqrt_u64(n)
  local lo = 0
  local hi = 0xFFFFFFFF
  while lo < hi do
    local mid = lo + floor((hi - lo + 1) / 2)
    if mid * mid <= n then
      lo = mid
    else
      hi = mid - 1
    end
  end
  return lo
end

function M.p8_fixed_sqrt(x)
  if x < 0 then
    return 0
  end
  return isqrt_u64(x * 0x10000)
end

function M.p8_fixed_pow075(x)
  local s = M.p8_fixed_sqrt(x)
  return M.p8_fixed_mul(s, M.p8_fixed_sqrt(s))
end

local P8_RND_LCG_MULTIPLIER = 1664525
local P8_RND_LCG_INCREMENT = 1013904223
local P8_RND_DEFAULT_SEED = 0x2545F491

-- A uint32_t: a number in 0 .. 2^32 - 1, never a signed bit result.
local rnd_state = P8_RND_DEFAULT_SEED

-- The C uint32_t *state is an argument plus a second return value: the new
-- state. The 64-bit magnitude * state (up to 2^63) does not fit a double:
-- the state is split into 16-bit halves.
function M.p8_fixed_rnd_from_state(state, x)
  if x == 0 then
    return 0, state
  end
  local magnitude = x < 0 and -x or x
  state = (state * P8_RND_LCG_MULTIPLIER + P8_RND_LCG_INCREMENT) % 0x100000000
  local state_high, state_low = floor(state / 0x10000), state % 0x10000
  local scaled = floor((magnitude * state_high + floor(magnitude * state_low / 0x10000)) / 0x10000)
  local result = tobit(scaled)
  return x < 0 and M.p8_fixed_neg(result) or result, state
end

function M.p8_fixed_rnd(x)
  local result
  result, rnd_state = M.p8_fixed_rnd_from_state(rnd_state, x)
  return result
end

function M.p8_fixed_rnd_state()
  return rnd_state
end

function M.p8_fixed_set_rnd_state(state)
  assert(state >= 0 and state < 0x100000000 and state == floor(state), "p8_fixed_set_rnd_state: state is a uint32_t")
  rnd_state = state
end

function M.p8_fixed_rnd_default()
  return M.p8_fixed_rnd(M.p8_fixed_from_int(1))
end

-- 0 - v, not -v: -0 would print as "-0".
local function quarter_wave_at(f)
  local quadrant = band(rshift(f, 14), 3)
  local r = band(f, 0x3FFF)
  local v = band(quadrant, 1) ~= 0 and p8_quarter_wave[0x4000 - r] or p8_quarter_wave[r]
  return band(quadrant, 2) ~= 0 and 0 - v or v
end

function M.p8_fixed_cos(x)
  return quarter_wave_at(band(x + 0x4000, 0xFFFF))
end

function M.p8_fixed_sin(x)
  return 0 - quarter_wave_at(band(x, 0xFFFF))
end

return M
