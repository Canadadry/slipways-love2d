-- Reference format (frozen from the C, deleted in plan 4): one "<name> <input>... = <output>..."
-- line of hex words per record, then "end <record count>". Records run in file
-- order: the random sections share one state.
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

local WORD = " (%x%x%x%x%x%x%x%x)"
local PATTERNS = {}
for name, signature in pairs(SIGNATURES) do
  PATTERNS[name] = "^" .. name .. WORD:rep(#signature.inputs) .. " =" .. WORD:rep(#signature.outputs) .. "$"
end

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
local file = path == "-" and io.stdin or io.open(path, "r")
if not file then
  fail("cannot open " .. path)
end

local function decode(word, kind)
  local raw = tonumber(word, 16)
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

local mismatches = {}
local differing, checked = {}, {}
local functions = {}
local records_read = 0
local trailer
local line_number = 0
local failed = false

local function malformed(message)
  fail(string.format("malformed reference line %d: %s", line_number, message))
end

local function function_for(name)
  local fn = ADAPTERS[name] and ADAPTERS[name](fixed) or fixed[name]
  if type(fn) == "number" then
    local value = fn
    fn = function()
      return value
    end
  end
  return fn or false
end

local inputs, actual, expected, words = {}, {}, {}, {}
for line in file:lines() do
  line_number = line_number + 1
  if trailer then
    malformed("after the trailer")
  end
  if line:sub(1, 1) ~= "#" then
    local name = line:match("^[%w_]+")
    if name == "end" then
      trailer = tonumber(line:match("^end (%d+)$"))
      if not trailer then
        malformed("trailer is not \"end <record count>\"")
      end
    else
      local signature = SIGNATURES[name]
      if not signature then
        malformed("unknown name " .. tostring(name))
      end
      local input_count, output_count = #signature.inputs, #signature.outputs
      words[1], words[2], words[3], words[4], words[5] = line:match(PATTERNS[name])
      if not words[1] then
        malformed(string.format("%s takes %d inputs and %d outputs, each an 8-digit hex word", name, input_count,
          output_count))
      end
      local fn = functions[name]
      if fn == nil then
        fn = function_for(name)
        functions[name] = fn
      end
      for i = 1, input_count do
        inputs[i] = decode(words[i], signature.inputs[i])
      end
      if fn then
        actual[1], actual[2], actual[3] = fn(unpack(inputs, 1, input_count))
      end
      local bad = not fn
      for i = 1, output_count do
        expected[i] = decode(words[input_count + i], signature.outputs[i])
        if actual[i] ~= expected[i] then
          bad = true
        end
      end
      local count = differing[name] or 0
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
      differing[name] = count
      checked[name] = (checked[name] or 0) + 1
      records_read = records_read + 1
    end
  end
end
if path ~= "-" then
  file:close()
end

if not trailer then
  fail("malformed reference: no trailer line")
end
if trailer ~= records_read then
  fail(string.format("malformed reference: trailer says %d records, read %d", trailer, records_read))
end
for name, count in pairs(differing) do
  if count > 0 then
    mismatches[#mismatches + 1] = string.format("%s: %d of %d records differ", name, count, checked[name])
    failed = true
  end
end
for name in pairs(SIGNATURES) do
  if not checked[name] then
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
print(string.format("test_fixed: %d records equal to the reference", records_read))
