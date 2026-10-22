-- Compares game/p8/fixed.lua with the C results written by tools/fixed_reference.c.
-- Usage, from the repository root: luajit tests/lua/test_fixed.lua <reference file | ->
package.path = "game/?.lua;" .. package.path

local bit = require("bit")
local fixed = require("p8.fixed")

local MAX_REPORTED = 5

-- Per C name: the type of each input then of each output. "f" is a fixed or
-- an int32_t (signed), "u" a uint32_t (non-negative).
local UNARY = { inputs = { "f" }, outputs = { "f" } }
local BINARY = { inputs = { "f", "f" }, outputs = { "f" } }
local CONSTANT = { inputs = {}, outputs = { "f" } }
local SIGNATURES = {
  P8_FIXED_SHIFT = CONSTANT,
  P8_FIXED_DIV0_POS = CONSTANT,
  P8_FIXED_DIV0_NEG = CONSTANT,
  P8_FIXED_LIT = { inputs = { "u", "u" }, outputs = { "f" } },
  p8_fixed_from_int = UNARY,
  P8_FIXED_INT = UNARY,
  p8_fixed_add = BINARY,
  p8_fixed_sub = BINARY,
  p8_fixed_mul = BINARY,
  p8_fixed_div = BINARY,
  p8_fixed_neg = UNARY,
  p8_fixed_flr = UNARY,
  p8_fixed_flr_int = UNARY,
  p8_fixed_abs = UNARY,
  p8_fixed_sgn = UNARY,
  p8_fixed_mid = { inputs = { "f", "f", "f" }, outputs = { "f" } },
  p8_fixed_band = BINARY,
  p8_fixed_bor = BINARY,
  p8_fixed_mod = BINARY,
  p8_fixed_shl = BINARY,
  p8_fixed_sqrt = UNARY,
  p8_fixed_pow075 = UNARY,
  p8_fixed_cos = UNARY,
  p8_fixed_sin = UNARY,
  p8_fixed_rnd = { inputs = { "f" }, outputs = { "f", "u" } },
  p8_fixed_set_rnd_state = { inputs = { "u" }, outputs = { "u" } },
  p8_fixed_rnd_default = { inputs = {}, outputs = { "f", "u" } },
  p8_fixed_rnd_from_state = { inputs = { "u", "f" }, outputs = { "f", "u" } },
  p8_fixed_rnd_state = { inputs = {}, outputs = { "u" } },
}

-- Sections whose C call also reads the global random state: each record
-- holds the call's results then the state it leaves.
local ADAPTERS = {
  p8_fixed_rnd = function(m)
    return m.p8_fixed_rnd and m.p8_fixed_rnd_state and function(x)
      return m.p8_fixed_rnd(x), m.p8_fixed_rnd_state()
    end
  end,
  p8_fixed_rnd_default = function(m)
    return m.p8_fixed_rnd_default and m.p8_fixed_rnd_state and function()
      return m.p8_fixed_rnd_default(), m.p8_fixed_rnd_state()
    end
  end,
  p8_fixed_set_rnd_state = function(m)
    return m.p8_fixed_set_rnd_state and m.p8_fixed_rnd_state and function(state)
      m.p8_fixed_set_rnd_state(state)
      return m.p8_fixed_rnd_state()
    end
  end,
}

local function fail(message)
  io.stderr:write("test_fixed: ", message, "\n")
  os.exit(1)
end

local path = arg[1]
if not path then
  fail("usage: luajit tests/lua/test_fixed.lua <reference file | ->")
end
local file = path == "-" and io.stdin or io.open(path, "rb")
if not file then
  fail("cannot open " .. path)
end
local data = file:read("*a")
if path ~= "-" then
  file:close()
end

local byte = string.byte
local position = 1

local function read_u32()
  if position + 3 > #data then
    fail(string.format("truncated reference at byte %d", position - 1))
  end
  local b1, b2, b3, b4 = byte(data, position, position + 3)
  position = position + 4
  return b1 + b2 * 0x100 + b3 * 0x10000 + b4 * 0x1000000
end

local function decode(raw, kind)
  if kind == "f" and raw >= 0x80000000 then
    return raw - 0x100000000
  end
  return raw
end

local function show(value)
  if type(value) == "number" and value == math.floor(value) and value >= -0x80000000 and value < 0x100000000 then
    return "0x" .. bit.tohex(value)
  end
  return tostring(value)
end

if data:sub(1, 8) ~= "P8FIXREF" then
  fail("malformed reference: bad magic")
end
position = 9
local version = read_u32()
if version ~= 1 then
  fail("malformed reference: version " .. version)
end

local mismatches = {}
-- Per C name, summed over its sections: a name may have several.
local differing, checked = {}, {}
local seen = {}
local records_read = 0
local failed = false

while true do
  local name_length = read_u32()
  if name_length == 0 then
    break
  end
  if position + name_length - 1 > #data then
    fail("truncated reference in a section name")
  end
  local name = data:sub(position, position + name_length - 1)
  position = position + name_length
  local signature = SIGNATURES[name]
  if not signature then
    fail("unknown section " .. name)
  end
  local input_count, output_count, record_count = read_u32(), read_u32(), read_u32()
  if input_count ~= #signature.inputs or output_count ~= #signature.outputs then
    fail(string.format("section %s: %d inputs, %d outputs, expected %d and %d", name, input_count, output_count,
      #signature.inputs, #signature.outputs))
  end
  if output_count > 3 then
    fail(string.format("section %s: %d outputs, the reader keeps 3 return values", name, output_count))
  end
  seen[name] = true
  local fn = ADAPTERS[name] and ADAPTERS[name](fixed) or fixed[name]
  if type(fn) == "number" then
    local value = fn
    fn = function()
      return value
    end
  end
  local count = differing[name] or 0
  local inputs, actual, expected = {}, {}, {}
  for _ = 1, record_count do
    for i = 1, input_count do
      inputs[i] = decode(read_u32(), signature.inputs[i])
    end
    if fn then
      actual[1], actual[2], actual[3] = fn(unpack(inputs, 1, input_count))
    end
    local bad = not fn
    for i = 1, output_count do
      expected[i] = decode(read_u32(), signature.outputs[i])
      if actual[i] ~= expected[i] then
        bad = true
      end
    end
    if bad then
      count = count + 1
      if count <= MAX_REPORTED then
        local shown_inputs, shown_expected, shown_actual = {}, {}, {}
        for i = 1, input_count do
          shown_inputs[i] = show(inputs[i])
        end
        for i = 1, output_count do
          shown_expected[i] = show(expected[i])
          shown_actual[i] = fn and show(actual[i]) or "missing function"
        end
        io.stderr:write(string.format("%s(%s): expected %s, got %s\n", name, table.concat(shown_inputs, ", "),
          table.concat(shown_expected, ", "), table.concat(shown_actual, ", ")))
      end
    end
  end
  records_read = records_read + record_count
  differing[name] = count
  checked[name] = (checked[name] or 0) + record_count
end
for name, count in pairs(differing) do
  if count > 0 then
    mismatches[#mismatches + 1] = string.format("%s: %d of %d records differ", name, count, checked[name])
    failed = true
  end
end

local total = read_u32()
if total ~= records_read then
  fail(string.format("malformed reference: trailer says %d records, read %d", total, records_read))
end
if position <= #data then
  fail("malformed reference: bytes after the trailer")
end
for name in pairs(SIGNATURES) do
  if not seen[name] then
    mismatches[#mismatches + 1] = name .. ": section missing from the reference"
    failed = true
  end
end
if failed then
  table.sort(mismatches)
  for _, line in ipairs(mismatches) do
    io.stderr:write("test_fixed: ", line, "\n")
  end
  os.exit(1)
end
print(string.format("test_fixed: %d records equal to the C", records_read))
