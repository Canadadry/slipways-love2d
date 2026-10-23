-- Lua-only checks of game/p8/ that need no C reference and no LÖVE.
-- Usage, from the repository root: luajit tests/lua/test_runtime.lua
package.path = "game/?.lua;" .. package.path

local cint = require("p8.cint")
local draw_meter = require("p8.draw_meter")
local fatal = require("p8.fatal")
local image = require("p8.image")
local gfx = require("p8.gfx")
local gfx_software = require("p8.gfx_software")
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

local function new_runtime()
  local graphics, mem = {}, {}
  gfx.p8_gfx_init(graphics)
  vram.p8_vram_init(mem)
  return graphics, mem
end

local function count_pixels(mem, col)
  local count = 0
  for y = 0, vram.P8_SCREEN_HEIGHT - 1 do
    for x = 0, vram.P8_SCREEN_WIDTH - 1 do
      if mem.screen[y][x] == col then
        count = count + 1
      end
    end
  end
  return count
end

local function test_software_pset_draws_under_the_camera_and_clips()
  local graphics, mem = new_runtime()
  gfx.p8_camera(graphics, 10, -5)
  gfx.p8_pset(graphics, mem, 12, 0, 9)
  gfx.p8_pset(graphics, mem, 9, 0, 8)
  gfx.p8_pset(graphics, mem, 170, 0, 8)
  expect(mem.screen[5][2], 9, "pset at world (12, 0) under camera (10, -5)")
  expect(count_pixels(mem, 8), 0, "off-screen psets")
  gfx.p8_camera_reset(graphics)
  gfx.p8_cls(mem)
  expect(count_pixels(mem, 0), vram.P8_SCREEN_WIDTH * vram.P8_SCREEN_HEIGHT, "pixels after cls")
end

local function test_backend_set_returns_the_previous_one_and_nil_restores_software()
  local software = gfx.p8_draw_backend_get()
  local recorder = {}
  expect(gfx.p8_draw_backend_set(recorder), software, "backend before the first install")
  expect(gfx.p8_draw_backend_get(), recorder, "installed backend")
  expect(gfx.p8_draw_backend_set(nil), recorder, "backend replaced by nil")
  expect(gfx.p8_draw_backend_get(), software, "backend after nil")
end

local function test_palt_and_fillp_keep_the_c_ranges()
  local graphics, mem = new_runtime()
  gfx.p8_palt(mem, 16, true)
  gfx.p8_palt(mem, -1, true)
  expect(gfx.p8_gfx_is_transparent(mem, 16), false, "is_transparent(16)")
  expect(gfx.p8_gfx_is_transparent(mem, -1), false, "is_transparent(-1)")
  gfx.p8_palt(mem, 3, true)
  expect(mem.draw_palette[3], 0x83, "draw_palette[3] after palt(3, true)")
  expect(gfx.p8_gfx_is_transparent(mem, 3), true, "is_transparent(3)")
  gfx.p8_palt(mem, 0, false)
  expect(mem.draw_palette[0], 0, "draw_palette[0] after palt(0, false)")
  gfx.p8_fillp(graphics, 0x12345)
  expect(graphics.fillp_pattern, 0x2345, "fillp pattern")
  expect(graphics.fillp_transparent, false, "fillp transparent flag")
  gfx.p8_fillp_transparent(graphics, 0x1FFFF)
  expect(graphics.fillp_pattern, 0xFFFF, "fillp_transparent pattern")
  expect(graphics.fillp_transparent, true, "fillp_transparent flag")
  gfx.p8_fillp_reset(graphics)
  expect(graphics.fillp_pattern, 0, "pattern after reset")
  expect(graphics.fillp_transparent, false, "flag after reset")
end

-- Returns the backend and the list of { n, x, y } of every spr it received.
local function spr_recorder()
  local calls = {}
  local recorder = {
    spr = function(_, _, n, x, y, w, h)
      expect(w * 10 + h, 11, "map spr size")
      calls[#calls + 1] = { n = n, x = x, y = y }
    end,
  }
  return recorder, calls
end

local function test_map_draws_only_the_visible_non_empty_cells()
  local graphics, mem = new_runtime()
  local version = mem.map_version
  for x = 0, vram.P8_MAP_WIDTH - 1 do
    gfx.p8_mset(mem, x, 0, 1 + x % 2)
  end
  gfx.p8_mset(mem, 32, 0, 5)
  gfx.p8_mset(mem, 0, -1, 5)
  gfx.p8_mset(mem, 4, 0, 0x104)
  expect(mem.map_cells[0][4], 4, "cell (4, 0) keeps the low byte")
  expect(mem.map_version > version, true, "mset gives a new map version")
  gfx.p8_mset(mem, 6, 0, 0)

  local recorder, calls = spr_recorder()
  gfx.p8_draw_backend_set(recorder)
  gfx.p8_map(graphics, mem, 0, 0, -16, 0, 32, 1)
  expect(#calls, 19, "cells drawn from sx -16")
  expect(calls[1].x, 0, "first cell x")
  expect(calls[1].n, 1, "first cell sprite")
  expect(calls[#calls].x, 152, "last cell x")

  recorder, calls = spr_recorder()
  gfx.p8_draw_backend_set(recorder)
  gfx.p8_camera(graphics, -3, 0)
  gfx.p8_map(graphics, mem, 0, 0, 0, 0, 32, 2)
  expect(#calls, 19, "cells drawn under camera -3")
  expect(calls[#calls].x, 152, "last cell x under camera -3")

  recorder, calls = spr_recorder()
  gfx.p8_draw_backend_set(recorder)
  gfx.p8_camera(graphics, 0, 0)
  gfx.p8_map(graphics, mem, 0, 0, 160 + 3, 0, 32, 1)
  expect(#calls, 0, "cells drawn from sx 163, wholly off screen")
  gfx.p8_draw_backend_set(nil)
end

local function test_lines_rects_and_circles_light_the_c_pixels()
  local graphics, mem = new_runtime()
  gfx.p8_line(graphics, mem, 2, 2, 5, 5, 8)
  expect(count_pixels(mem, 8), 4, "diagonal line pixels")
  expect(mem.screen[5][5], 8, "line end")
  gfx.p8_line(graphics, mem, 0, 10, 6, 12, 9)
  expect(count_pixels(mem, 9), 7, "shallow line pixels")
  expect(mem.screen[11][3], 9, "shallow line middle")
  gfx.p8_lines(graphics, mem, { [0] = { x1 = 20, y1 = 0, x2 = 20, y2 = 2 }, { x1 = 22, y1 = 0, x2 = 23, y2 = 0 } }, 2, 10)
  expect(count_pixels(mem, 10), 5, "lines pixels")
  gfx.p8_psets(graphics, mem, { [0] = { x = 30, y = 0, col = 11 }, { x = 30, y = 0, col = 12 } }, 2)
  expect(mem.screen[0][30], 12, "the last pset of a list wins")

  graphics, mem = new_runtime()
  gfx.p8_rect(graphics, mem, 5, 4, 2, 2, 8)
  expect(count_pixels(mem, 8), 10, "rect outline pixels")
  expect(mem.screen[3][3], 0, "rect inside")
  gfx.p8_circ(graphics, mem, 10, 30, 3, 9)
  expect(count_pixels(mem, 9), 12, "circ r 3 pixels")
  expect(mem.screen[31][12], 9, "circ (2, 1) offset")
  gfx.p8_circ(graphics, mem, 40, 30, -1, 9)
  gfx.p8_circfill(graphics, mem, 40, 30, -1, 9)
  expect(count_pixels(mem, 9), 12, "negative radii draw nothing")
  gfx.p8_circfill(graphics, mem, 10, 60, 3, 0x0A)
  expect(count_pixels(mem, 10), 25, "circfill r 3 pixels")
  gfx.p8_circ(graphics, mem, 60, 60, 0, 11)
  expect(count_pixels(mem, 11), 1, "circ r 0 pixels")
end

local function test_fill_patterns_align_on_world_coordinates()
  local graphics, mem = new_runtime()
  gfx.p8_fillp(graphics, 0x5A5A)
  gfx.p8_camera(graphics, 1, 0)
  gfx.p8_rectfill(graphics, mem, 4, 0, 1, 1, 0x98)
  expect(mem.screen[0][0], 9, "world (1, 0): set bit, high nibble")
  expect(mem.screen[0][1], 8, "world (2, 0): clear bit, low nibble")
  expect(mem.screen[1][0], 8, "world (1, 1): clear bit")
  expect(mem.screen[1][3], 9, "world (4, 1): set bit")
  gfx.p8_camera_reset(graphics)

  gfx.p8_fillp_reset(graphics)
  gfx.p8_rectfill(graphics, mem, 0, 10, 3, 10, 3)
  gfx.p8_fillp_transparent(graphics, 0x5A5A)
  gfx.p8_rectfill(graphics, mem, 0, 10, 3, 10, 0xF7)
  expect(mem.screen[10][0], 7, "transparent pattern, clear bit")
  expect(mem.screen[10][1], 3, "transparent pattern, set bit shows the background")
  gfx.p8_circfill(graphics, mem, 50, 50, 0, 0x07)
  expect(mem.screen[50][50], 7, "circfill (50, 50): clear bit 5")
end

local function test_spr_draws_through_the_draw_palette_and_off_sheet_as_color_0()
  local graphics, mem = new_runtime()
  sprite_sheet.p8_sprite_sheet_load(mem)
  expect(gfx.p8_sget(mem, 16, 0), 3, "sget (16, 0)")
  expect(gfx.p8_sget(mem, 128, 0), 0, "sget past the sheet")
  gfx.p8_rectfill(graphics, mem, 0, 0, 159, 119, 5)
  mem.draw_palette[3] = 12
  gfx.p8_spr_default(graphics, mem, 2, 0, 0)
  expect(mem.screen[0][0], 12, "sprite 2 top-left, colour 3 drawn as 12")
  expect(mem.screen[0][1], 5, "see-through colour 0 keeps the background")
  gfx.p8_palt(mem, 0, false)
  mem.draw_palette[0] = 0x0E
  gfx.p8_spr(graphics, mem, -1, 20, 20, 1, 1)
  expect(count_pixels(mem, 14), 64, "sprite -1 is wholly off the sheet: colour 0 through the palette")
  gfx.p8_spr(graphics, mem, 2, 40, 40, 0, 1)
  expect(count_pixels(mem, 14), 64, "a zero-width sprite draws nothing")
end

local function glyph_bits(mem, x, y)
  local bits = 0
  for row = 0, 4 do
    for column = 0, 3 do
      bits = bits * 2 + (mem.screen[y + row][x + column] ~= 0 and 1 or 0)
    end
  end
  return bits
end

local function test_print_draws_the_c_font_rows()
  local graphics, mem = new_runtime()
  gfx.p8_print(graphics, mem, "aAvU~ ", 10, 20, 7)
  expect(glyph_bits(mem, 10, 20), 0x4AEAA, "glyph a")
  expect(glyph_bits(mem, 14, 20), 0x4AEAA, "glyph A")
  expect(glyph_bits(mem, 18, 20), glyph_bits(mem, 22, 20), "v drawn as U")
  expect(glyph_bits(mem, 26, 20), 0xEEEEE, "placeholder for ~")
  expect(glyph_bits(mem, 30, 20), 0, "space")
  expect(gfx_software.p8_font_has_glyph(string.byte("~")), false, "has_glyph(~)")
  expect(gfx_software.p8_font_has_glyph(string.byte("a")), true, "has_glyph(a)")
  expect(gfx_software.p8_font_has_glyph(string.byte(" ")), true, "has_glyph(space)")
  expect(gfx_software.p8_font_has_glyph(200), false, "has_glyph(200)")
  expect(gfx_software.p8_font_glyph_rows(string.byte("?"))[4], 2, "last row of ?")

  graphics, mem = new_runtime()
  gfx.p8_camera(graphics, -1, -1)
  gfx.p8_rectfill(graphics, mem, -1, -1, 10, 10, 1)
  gfx.p8_print_outlined(graphics, mem, "I", 0, 0, 7)
  expect(mem.screen[1][1], 7, "outlined I top-left")
  expect(mem.screen[0][0], 0, "outline above-left")
  expect(mem.screen[2][4], 0, "outline right of the bar")
  expect(mem.screen[2][1], 0, "outline left of the bar")
  expect(mem.screen[0][5], 1, "background past the outline")
end

local function counts_text(counts)
  return string.format("%d %d %d %d %d", counts.bytes, counts.build_bytes, counts.primitives, counts.cluts, counts.patterns)
end

-- Expected counts worked out by hand from game/p8/draw_cost.lua.
local function test_draw_meter_counts_the_c_costs_per_frame_and_peak()
  local graphics, mem = new_runtime()
  sprite_sheet.p8_sprite_sheet_load(mem)
  local software = gfx.p8_draw_backend_get()
  expect(draw_meter.p8_draw_meter_install(), software, "wrapped backend")
  expect(counts_text(draw_meter.p8_draw_meter_last_frame()), "0 0 0 0 0", "last frame before any end")

  gfx.p8_cls(mem)
  gfx.p8_pset(graphics, mem, 3, 3, 7)
  draw_meter.p8_draw_meter_frame_end()
  expect(counts_text(draw_meter.p8_draw_meter_last_frame()), "156 0 7 0 0", "frame 1")
  expect(mem.screen[3][3], 7, "the meter draws through the wrapped backend")

  gfx.p8_spr(graphics, mem, 2, 0, 0, 1, 1)
  gfx.p8_fillp(graphics, 0x5A5A)
  gfx.p8_rectfill(graphics, mem, 0, 0, 3, 3, 8)
  gfx.p8_fillp_reset(graphics)
  gfx.p8_spr(graphics, mem, 2, 8, 0, 1, 1)
  gfx.p8_circ(graphics, mem, 10, 10, 3, 9)
  gfx.p8_circfill(graphics, mem, 10, 10, 3, 9)
  gfx.p8_print(graphics, mem, "A B", 0, 50, 7)
  gfx.p8_pset(graphics, mem, -1, -1, 7)
  image.p8_image_build(graphics, mem, 2, 48, 8, function(g, m)
    gfx.p8_pset(g, m, 0, 0, 7)
  end)
  image.p8_image_draw(graphics, mem, 2, 0, 100)
  draw_meter.p8_draw_meter_frame_end()
  expect(counts_text(draw_meter.p8_draw_meter_last_frame()), "624 92 42 1 1", "frame 2")

  gfx.p8_print_outlined(graphics, mem, "  ", 0, 0, 7)
  gfx.p8_lines(graphics, mem, { [0] = { x1 = 0, y1 = 0, x2 = 5, y2 = 5 }, { x1 = -9, y1 = 0, x2 = -5, y2 = 3 } }, 2, 7)
  gfx.p8_psets(graphics, mem, { [0] = { x = 0, y = 0, col = 7 }, { x = 1, y = 0, col = 7 } }, 2)
  gfx.p8_rect(graphics, mem, 0, 0, 3, 3, 7)
  draw_meter.p8_draw_meter_frame_end()
  expect(counts_text(draw_meter.p8_draw_meter_last_frame()), "232 0 12 0 0", "frame 3")
  expect(counts_text(draw_meter.p8_draw_meter_peak()), "624 92 42 1 1", "peak")

  expect(gfx.p8_draw_backend_set(software) ~= software, true, "meter removed")
end

local function expect_fatal(message, f, ...)
  local ok, raised = pcall(f, ...)
  expect(ok, false, message .. " succeeds")
  expect(raised, message, "fatal message")
end

local function test_image_build_runs_one_draw_per_window_and_restores_the_camera()
  local graphics, mem = new_runtime()
  expect(image.P8_IMAGE_SLOT_COUNT, 88, "slot count")
  expect(image.P8_IMAGE_PIXELS, 125600, "slot store pixels")
  local size = image.p8_image_size(0)
  expect(size.width * 1000 + size.height, 256256, "slot 0 size")
  size = image.p8_image_size(33)
  expect(size.width * 1000 + size.height, 128008, "slot 33 size")
  size = image.p8_image_size(87)
  expect(size.width * 1000 + size.height, 112011, "slot 87 size")
  expect_fatal("image slot 88: no such slot", image.p8_image_size, 88)

  gfx.p8_camera(graphics, 7, 9)
  local windows = {}
  image.p8_image_build(graphics, mem, 0, 256, 256, function(g, m, context)
    expect(m, mem, "draw receives mem")
    windows[#windows + 1] = string.format("%s%d,%d", context, g.camera_x, g.camera_y)
  end, "w")
  expect(table.concat(windows, " "), "w0,0 w160,0 w0,120 w160,120 w0,240 w160,240", "windows")
  expect(graphics.camera_x * 100 + graphics.camera_y, 709, "camera restored")
  expect_fatal("image slot 1: 49x8 over its 48x8", image.p8_image_build, graphics, mem, 1, 49, 8, function() end)
  expect_fatal("image slot 5: drawn before any build", image.p8_image_draw, graphics, mem, 5, 0, 0)
end

local function test_image_draw_copies_what_the_build_wrote_under_the_camera()
  local graphics, mem = new_runtime()
  image.p8_image_build(graphics, mem, 1, 48, 8, function(g, m)
    gfx.p8_rectfill(g, m, 0, 0, 2, 0, 7)
    gfx.p8_pset(g, m, 47, 7, 9)
    gfx.p8_pset(g, m, 48, 0, 9)
  end)
  gfx.p8_rectfill(graphics, mem, 0, 0, 159, 119, 1)
  gfx.p8_camera(graphics, 10, -5)
  image.p8_image_draw(graphics, mem, 1, 20, 0)
  expect(mem.screen[5][10], 7, "image (0, 0)")
  expect(mem.screen[5][12], 7, "image (2, 0)")
  expect(mem.screen[5][13], 1, "unwritten image pixel")
  expect(mem.screen[12][57], 9, "image (47, 7)")
  expect(count_pixels(mem, 9), 1, "pixel past the image width")
  image.p8_image_draw(graphics, mem, 1, 155, 110)
  expect(mem.screen[115][145], 7, "image clipped at the screen corner")
end

-- Last: the fatal leaves the build open, as the C would have exited.
local function test_a_cls_during_a_build_is_fatal()
  local graphics, mem = new_runtime()
  expect_fatal("image slot 2: cls during a build", image.p8_image_build, graphics, mem, 2, 48, 8, function(_, m)
    gfx.p8_cls(m)
  end)
  expect_fatal("image slot 1: draw during a build", image.p8_image_draw, graphics, mem, 1, 0, 0)
  expect_fatal("image slot 3: build inside a build", image.p8_image_build, graphics, mem, 3, 48, 8, function() end)
end

test_cint_div_truncates_toward_zero()
test_cint_mod_takes_the_sign_of_the_dividend()
test_cint_results_print_as_c_ints()
test_cint_zero_divisor_raises()
test_vram_init_makes_color_0_see_through()
test_vram_init_gives_a_fresh_map_version()
test_fatal_raises_the_message_alone()
test_sprite_sheet_load_splits_each_byte_low_nibble_first()
test_software_pset_draws_under_the_camera_and_clips()
test_backend_set_returns_the_previous_one_and_nil_restores_software()
test_palt_and_fillp_keep_the_c_ranges()
test_map_draws_only_the_visible_non_empty_cells()
test_lines_rects_and_circles_light_the_c_pixels()
test_fill_patterns_align_on_world_coordinates()
test_spr_draws_through_the_draw_palette_and_off_sheet_as_color_0()
test_print_draws_the_c_font_rows()
test_image_build_runs_one_draw_per_window_and_restores_the_camera()
test_image_draw_copies_what_the_build_wrote_under_the_camera()
test_draw_meter_counts_the_c_costs_per_frame_and_peak()
test_a_cls_during_a_build_is_fatal()
print("test_runtime: all tests passed")
