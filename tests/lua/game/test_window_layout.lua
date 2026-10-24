local check = require("check")
local window_layout = require("love.window_layout")

local expect = check.expect

local function expect_rectangle(rectangle, x, y, width, height, scale, what)
  expect(rectangle.x, x, what .. " x")
  expect(rectangle.y, y, what .. " y")
  expect(rectangle.width, width, what .. " width")
  expect(rectangle.height, height, what .. " height")
  expect(rectangle.scale, scale, what .. " scale")
end

local function test_a_portrait_iphone_puts_the_game_in_the_top_half_and_the_pad_in_the_bottom_half()
  local layout = window_layout.compute(390, 844)
  expect(layout.mode, window_layout.MODE_TALL, "mode")
  expect_rectangle(layout.game, 0, 64.75, 390, 292.5, 2.4375, "game")
  expect_rectangle(layout.pad, 0, 486.75, 390, 292.5, 2.4375, "pad")
  expect(layout.pad.y + layout.pad.height, 779.25, "pad bottom")
end

local function test_the_default_desktop_window_is_filled_by_the_game_at_scale_4_with_the_pad_over_it()
  local layout = window_layout.compute(640, 480)
  expect(layout.mode, window_layout.MODE_OVERLAY, "mode")
  expect_rectangle(layout.game, 0, 0, 640, 480, 4, "game")
  expect_rectangle(layout.pad, 0, 0, 640, 480, 4, "pad")
end

local function test_a_landscape_iphone_is_height_limited_and_centred_with_bars_left_and_right()
  local layout = window_layout.compute(844, 390)
  expect(layout.mode, window_layout.MODE_OVERLAY, "mode")
  expect_rectangle(layout.game, 162, 0, 520, 390, 3.25, "game")
  expect_rectangle(layout.pad, 162, 0, 520, 390, 3.25, "pad")
end

local function test_an_ipad_is_overlay_width_limited_and_centred_with_bars_above_and_below()
  local layout = window_layout.compute(820, 1180)
  expect(layout.mode, window_layout.MODE_OVERLAY, "mode")
  expect_rectangle(layout.game, 0, 282.5, 820, 615, 5.125, "game")
  expect_rectangle(layout.pad, 0, 282.5, 820, 615, 5.125, "pad")
end

local function test_a_window_exactly_twice_as_tall_as_wide_is_overlay()
  local layout = window_layout.compute(390, 780)
  expect(layout.mode, window_layout.MODE_OVERLAY, "mode")
  expect_rectangle(layout.game, 0, 243.75, 390, 292.5, 2.4375, "game")
  expect_rectangle(layout.pad, 0, 243.75, 390, 292.5, 2.4375, "pad")
end

local function test_one_unit_taller_than_twice_the_width_is_tall_with_unrounded_halves()
  local layout = window_layout.compute(390, 781)
  expect(layout.mode, window_layout.MODE_TALL, "mode")
  expect_rectangle(layout.game, 0, 49, 390, 292.5, 2.4375, "game")
  expect_rectangle(layout.pad, 0, 439.5, 390, 292.5, 2.4375, "pad")
end

local function test_the_overlay_pad_is_a_table_of_its_own()
  local layout = window_layout.compute(640, 480)
  expect(rawequal(layout.pad, layout.game), false, "pad and game are the same table")
  layout.pad.x = 1
  expect(layout.game.x, 0, "game x after a change of the pad")
end

local function test_a_call_does_not_depend_on_the_one_before()
  local first = window_layout.compute(390, 844)
  local second = window_layout.compute(640, 480)
  expect(second.mode, window_layout.MODE_OVERLAY, "second mode")
  expect_rectangle(second.game, 0, 0, 640, 480, 4, "second game")
  expect(rawequal(first.game, second.game), false, "both calls return the same game table")
  expect_rectangle(first.game, 0, 64.75, 390, 292.5, 2.4375, "first game after the second call")
end

local function expect_refused(width, height, argument)
  local ok, err = pcall(window_layout.compute, width, height)
  local what = string.format("compute(%q, %q)", tostring(width), tostring(height))
  expect(ok, false, what .. " succeeded")
  expect(string.find(tostring(err), argument, 1, true) ~= nil, true, what .. " error names " .. argument .. ": " .. tostring(err))
end

local function test_a_size_that_is_not_a_positive_number_is_refused_naming_the_argument()
  expect_refused(0, 480, "width")
  expect_refused(640, -1, "height")
  expect_refused("640", 480, "width")
end

return check.run({
  { "a portrait iphone puts the game in the top half and the pad in the bottom half", test_a_portrait_iphone_puts_the_game_in_the_top_half_and_the_pad_in_the_bottom_half },
  { "the default desktop window is filled by the game at scale 4 with the pad over it", test_the_default_desktop_window_is_filled_by_the_game_at_scale_4_with_the_pad_over_it },
  { "a landscape iphone is height limited and centred with bars left and right", test_a_landscape_iphone_is_height_limited_and_centred_with_bars_left_and_right },
  { "an ipad is overlay width limited and centred with bars above and below", test_an_ipad_is_overlay_width_limited_and_centred_with_bars_above_and_below },
  { "a window exactly twice as tall as wide is overlay", test_a_window_exactly_twice_as_tall_as_wide_is_overlay },
  { "one unit taller than twice the width is tall with unrounded halves", test_one_unit_taller_than_twice_the_width_is_tall_with_unrounded_halves },
  { "the overlay pad is a table of its own", test_the_overlay_pad_is_a_table_of_its_own },
  { "a call does not depend on the one before", test_a_call_does_not_depend_on_the_one_before },
  { "a size that is not a positive number is refused naming the argument", test_a_size_that_is_not_a_positive_number_is_refused_naming_the_argument },
})
