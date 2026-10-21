-- Lua-only checks of game/p8/ that need no C reference and no LÖVE.
-- Usage, from the repository root: luajit tests/lua/test_runtime.lua
package.path = "game/?.lua;" .. package.path

local cint = require("p8.cint")
local fatal = require("p8.fatal")
local sprite_sheet = require("p8.sprite_sheet")
local vram = require("p8.vram")

local function expect(actual, expected, what)
  if actual ~= expected then
    error(string.format("%s: expected %s, got %s", what, tostring(expected), tostring(actual)), 2)
  end
end

local function test_cint_div_truncates_toward_zero()
  local cases = {
    { 7, 2, 3 }, { -7, 2, -3 }, { 7, -2, -3 }, { -7, -2, 3 },
    { 0, 5, 0 }, { 6, 3, 2 }, { -6, 3, -2 }, { 1, 4, 0 }, { -1, 4, 0 },
    { 2147483647, 1, 2147483647 }, { -2147483648, 1, -2147483648 },
    { 2147483647, -2147483648, 0 }, { -2147483648, 2147483647, -1 },
    { 2147483646, 2147483647, 0 }, { -2147483647, 2147483647, -1 },
  }
  for _, case in ipairs(cases) do
    local a, b, q = case[1], case[2], case[3]
    expect(cint.div(a, b), q, string.format("cint.div(%d, %d)", a, b))
  end
end

local function test_cint_mod_takes_the_sign_of_the_dividend()
  local cases = {
    { 7, 2, 1 }, { -7, 2, -1 }, { 7, -2, 1 }, { -7, -2, -1 },
    { 0, 5, 0 }, { 6, 3, 0 }, { -6, 3, 0 }, { -1, 4, -1 }, { -5, 4, -1 },
    { 2147483647, 2, 1 }, { -2147483648, 3, -2 }, { -2147483648, 2147483647, -1 },
  }
  for _, case in ipairs(cases) do
    local a, b, r = case[1], case[2], case[3]
    expect(cint.mod(a, b), r, string.format("cint.mod(%d, %d)", a, b))
  end
end

-- Looped so the JIT-compiled path is checked too, not only the interpreter.
local function test_cint_results_print_as_c_ints()
  for _ = 1, 200 do
    expect(tostring(cint.div(-1, 4)), "0", "tostring(cint.div(-1, 4))")
    expect(tostring(cint.mod(-4, 2)), "0", "tostring(cint.mod(-4, 2))")
    expect(tostring(cint.div(0, -5)), "0", "tostring(cint.div(0, -5))")
    expect(tostring(cint.div(0, 5)), "0", "tostring(cint.div(0, 5))")
    expect(tostring(cint.mod(0, -5)), "0", "tostring(cint.mod(0, -5))")
    expect(tostring(cint.mod(4, -2)), "0", "tostring(cint.mod(4, -2))")
  end
end

local function test_cint_zero_divisor_raises()
  expect(pcall(cint.div, 1, 0), false, "cint.div(1, 0) succeeds")
  expect(pcall(cint.mod, 1, 0), false, "cint.mod(1, 0) succeeds")
end

local function test_vram_init_makes_color_0_see_through()
  local mem = {}
  vram.p8_vram_init(mem)
  expect(mem.draw_palette[0], 0x80, "draw_palette[0]")
  for c = 1, 15 do
    expect(mem.draw_palette[c], c, string.format("draw_palette[%d]", c))
  end
  expect(mem.screen[vram.P8_SCREEN_HEIGHT - 1][vram.P8_SCREEN_WIDTH - 1], 0, "last screen pixel")
  expect(mem.sprite_sheet[vram.P8_SPRITE_SHEET_HEIGHT - 1][0], 0, "sheet pixel")
  expect(mem.map_cells[vram.P8_MAP_HEIGHT - 1][vram.P8_MAP_WIDTH - 1], 0, "last map cell")
end

local function test_vram_init_gives_a_fresh_map_version()
  local first, second = {}, {}
  vram.p8_vram_init(first)
  vram.p8_vram_init(second)
  expect(second.map_version > first.map_version, true, "second map version above the first")
  expect(vram.p8_vram_next_map_version(), second.map_version + 1, "next map version")
end

local function test_fatal_raises_the_message_alone()
  local ok, message = pcall(fatal.p8_fatal, "image slot 3 never built")
  expect(ok, false, "p8_fatal returns")
  expect(message, "image slot 3 never built", "p8_fatal message")
end

-- Byte 8 of the packed sheet is 0x03 and byte 11 is 0x30, both on row 0.
local function test_sprite_sheet_load_splits_each_byte_low_nibble_first()
  local mem = {}
  vram.p8_vram_init(mem)
  sprite_sheet.p8_sprite_sheet_load(mem)
  local packed = sprite_sheet.p8_sprite_sheet_packed()
  expect(packed[8], 0x03, "packed byte 8")
  expect(mem.sprite_sheet[0][16], 3, "sheet (16, 0)")
  expect(mem.sprite_sheet[0][17], 0, "sheet (17, 0)")
  expect(mem.sprite_sheet[0][22], 0, "sheet (22, 0)")
  expect(mem.sprite_sheet[0][23], 3, "sheet (23, 0)")
  for y = 0, vram.P8_SPRITE_SHEET_HEIGHT - 1 do
    for x = 0, vram.P8_SPRITE_SHEET_WIDTH - 1, 2 do
      local byte = packed[y * 64 + x / 2]
      expect(mem.sprite_sheet[y][x] + mem.sprite_sheet[y][x + 1] * 16, byte, string.format("sheet row %d byte %d", y, x / 2))
    end
  end
  expect(packed[sprite_sheet.P8_SPRITE_SHEET_PACKED_SIZE - 1] ~= nil, true, "last packed byte")
  expect(packed[sprite_sheet.P8_SPRITE_SHEET_PACKED_SIZE], nil, "byte past the packed sheet")
end

test_cint_div_truncates_toward_zero()
test_cint_mod_takes_the_sign_of_the_dividend()
test_cint_results_print_as_c_ints()
test_cint_zero_divisor_raises()
test_vram_init_makes_color_0_see_through()
test_vram_init_gives_a_fresh_map_version()
test_fatal_raises_the_message_alone()
test_sprite_sheet_load_splits_each_byte_low_nibble_first()
print("test_runtime: all tests passed")
