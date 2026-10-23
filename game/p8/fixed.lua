-- A fixed is a number holding a signed 32-bit 16.16 raw value.
local bit = require("bit")
local quarter_wave = require("p8.quarter_wave")

local M = {}

-- bit.tobit wraps modulo 2^32 only for magnitudes below 2^51.
local tobit = bit.tobit
local band = bit.band
local bor = bit.bor
local lshift = bit.lshift
local arshift = bit.arshift
local rshift = bit.rshift
local floor = math.floor

M.SHIFT = 16

M.DIV0_POS = 0x7FFFFFFF
M.DIV0_NEG = -0x7FFFFFFF

function M.LIT(raw, decimal)
  return tobit(raw)
end

function M.from_int(i)
  return lshift(i, M.SHIFT)
end

function M.INT(i)
  return lshift(i, M.SHIFT)
end

function M.add(a, b)
  return tobit(a + b)
end

function M.sub(a, b)
  return tobit(a - b)
end

-- The 64-bit product (up to 2^62) does not fit a double: split into halves.
function M.mul(a, b)
  local a_high, a_low = arshift(a, 16), band(a, 0xFFFF)
  local b_high, b_low = arshift(b, 16), band(b, 0xFFFF)
  local rescaled = a_high * b_high * 0x10000 + a_high * b_low + a_low * b_high + floor(a_low * b_low / 0x10000)
  return tobit(rescaled)
end

function M.neg(x)
  return tobit(-x)
end

function M.flr(x)
  return band(x, 0xFFFF0000)
end

function M.flr_int(x)
  return M.flr(x) / 0x10000
end

function M.abs(x)
  return x < 0 and M.neg(x) or x
end

function M.sgn(x)
  return x < 0 and M.from_int(-1) or M.from_int(1)
end

function M.band(a, b)
  return band(a, b)
end

function M.bor(a, b)
  return bor(a, b)
end

-- Truncating the double quotient is exact: with |numerator| < 2^48 its
-- rounding error stays below the 1/|b| gap to the next integer.
function M.div(a, b)
  if b == 0 then
    return a < 0 and M.DIV0_NEG or M.DIV0_POS
  end
  local quotient = a * 0x10000 / b
  return tobit(quotient < 0 and -floor(-quotient) or floor(quotient))
end

function M.mid(a, b, c)
  if (a <= b and b <= c) or (c <= b and b <= a) then
    return b
  end
  if (b <= a and a <= c) or (c <= a and a <= b) then
    return a
  end
  return c
end

function M.mod(a, b)
  return M.sub(a, M.mul(M.flr(M.div(a, b)), b))
end

function M.shl(x, n)
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

function M.sqrt(x)
  if x < 0 then
    return 0
  end
  return isqrt_u64(x * 0x10000)
end

function M.pow075(x)
  local s = M.sqrt(x)
  return M.mul(s, M.sqrt(s))
end

local RND_LCG_MULTIPLIER = 1664525
local RND_LCG_INCREMENT = 1013904223
local RND_DEFAULT_SEED = 0x2545F491

-- A number in 0 .. 2^32 - 1, never a signed bit result.
local rnd_state = RND_DEFAULT_SEED

-- Returns the new state as a second value. The 64-bit magnitude * state
-- (up to 2^63) does not fit a double: the state is split into 16-bit halves.
function M.rnd_from_state(state, x)
  if x == 0 then
    return 0, state
  end
  local magnitude = x < 0 and -x or x
  state = (state * RND_LCG_MULTIPLIER + RND_LCG_INCREMENT) % 0x100000000
  local state_high, state_low = floor(state / 0x10000), state % 0x10000
  local scaled = floor((magnitude * state_high + floor(magnitude * state_low / 0x10000)) / 0x10000)
  local result = tobit(scaled)
  return x < 0 and M.neg(result) or result, state
end

function M.rnd(x)
  local result
  result, rnd_state = M.rnd_from_state(rnd_state, x)
  return result
end

function M.rnd_state()
  return rnd_state
end

function M.set_rnd_state(state)
  assert(state >= 0 and state < 0x100000000 and state == floor(state), "fixed.set_rnd_state: state is a uint32_t")
  rnd_state = state
end

function M.rnd_default()
  return M.rnd(M.from_int(1))
end

-- 0 - v, not -v: -0 would print as "-0".
local function quarter_wave_at(f)
  local quadrant = band(rshift(f, 14), 3)
  local r = band(f, 0x3FFF)
  local v = band(quadrant, 1) ~= 0 and quarter_wave[0x4000 - r] or quarter_wave[r]
  return band(quadrant, 2) ~= 0 and 0 - v or v
end

function M.cos(x)
  return quarter_wave_at(band(x + 0x4000, 0xFFFF))
end

function M.sin(x)
  return 0 - quarter_wave_at(band(x, 0xFFFF))
end

return M
