local check = require("check")
local pad_layout = require("love.pad_layout")
local pad_stick = require("love.pad_stick")
local window_layout = require("love.window_layout")

local expect = check.expect

local function expect_stick(stick, base_x, base_y, radius, knob_x, knob_y, mask, what)
  expect(stick.base_x, base_x, what .. " base_x")
  expect(stick.base_y, base_y, what .. " base_y")
  expect(stick.radius, radius, what .. " radius")
  expect(stick.knob_x, knob_x, what .. " knob_x")
  expect(stick.knob_y, knob_y, what .. " knob_y")
  expect(stick.mask, mask, what .. " mask")
end

local function test_the_masks_are_the_bits_of_dpup_dpdown_dpleft_and_dpright()
  expect(pad_stick.UP, 1, "UP")
  expect(pad_stick.DOWN, 2, "DOWN")
  expect(pad_stick.LEFT, 4, "LEFT")
  expect(pad_stick.RIGHT, 8, "RIGHT")
end

local function expect_mask(base_size, base_x, base_y, x, y, mask, what)
  local stick = pad_stick.start(base_x, base_y, base_size)
  pad_stick.follow(stick, x, y)
  expect(stick.mask, mask, what .. " mask")
end

local function test_the_axes_and_the_middle_of_a_diagonal_give_their_directions_with_up_toward_the_top()
  local cases = {
    { 20, 0, 8, "right" }, { -20, 0, 4, "left" }, { 0, -20, 1, "up" }, { 0, 20, 2, "down" },
    { 20, 20, 10, "down right" }, { -20, -20, 5, "up left" },
  }
  for _, case in ipairs(cases) do
    expect_mask(117, 40, 600, 40 + case[1], 600 + case[2], case[3], case[4])
  end
end

local function test_a_touch_exactly_a_quarter_of_the_radius_away_on_an_axis_points_and_the_next_double_inside_does_not()
  expect_mask(117, 40, 600, 54.625, 600, 8, "right on the dead-zone edge")
  expect((54.625 - 2 ^ -47) - 40, 14.625 - 2 ^ -47, "offset just inside on the right")
  expect_mask(117, 40, 600, 54.625 - 2 ^ -47, 600, 0, "right just inside the dead zone")
  expect_mask(117, 40, 600, 40, 585.375, 1, "up on the dead-zone edge")
  expect((585.375 + 2 ^ -43) - 600, -14.625 + 2 ^ -43, "offset just inside upward")
  expect_mask(117, 40, 600, 40, 585.375 + 2 ^ -43, 0, "up just inside the dead zone")
end

local function test_a_touch_exactly_a_quarter_of_the_radius_away_on_a_diagonal_points_and_the_next_double_inside_does_not()
  expect(9 * 9 + 12 * 12, 15 * 15, "offset (9, -12) at the dead radius")
  expect_mask(120, 40, 600, 49, 588, 9, "up right on the dead-zone edge")
  expect((588 + 2 ^ -43) - 600, -12 + 2 ^ -43, "offset just inside on the diagonal")
  expect_mask(120, 40, 600, 49, 588 + 2 ^ -43, 0, "up right just inside the dead zone")
end

local function test_the_nearest_integer_points_on_both_sides_of_each_sector_edge_in_all_four_quadrants()
  local cases = {
    { 26, -15, 8, "29.98 deg up right, horizontal side of the 30 deg edge" },
    { 26, 15, 8, "29.98 deg down right, horizontal side of the 30 deg edge" },
    { -26, -15, 4, "29.98 deg up left, horizontal side of the 30 deg edge" },
    { -26, 15, 4, "29.98 deg down left, horizontal side of the 30 deg edge" },
    { 19, -11, 9, "30.07 deg up right, diagonal side of the 30 deg edge" },
    { 19, 11, 10, "30.07 deg down right, diagonal side of the 30 deg edge" },
    { -19, -11, 5, "30.07 deg up left, diagonal side of the 30 deg edge" },
    { -19, 11, 6, "30.07 deg down left, diagonal side of the 30 deg edge" },
    { 15, -26, 1, "60.02 deg up right, vertical side of the 60 deg edge" },
    { 15, 26, 2, "60.02 deg down right, vertical side of the 60 deg edge" },
    { -15, -26, 1, "60.02 deg up left, vertical side of the 60 deg edge" },
    { -15, 26, 2, "60.02 deg down left, vertical side of the 60 deg edge" },
    { 11, -19, 9, "59.93 deg up right, diagonal side of the 60 deg edge" },
    { 11, 19, 10, "59.93 deg down right, diagonal side of the 60 deg edge" },
    { -11, -19, 5, "59.93 deg up left, diagonal side of the 60 deg edge" },
    { -11, 19, 6, "59.93 deg down left, diagonal side of the 60 deg edge" },
  }
  for _, case in ipairs(cases) do
    expect_mask(117, 40, 600, 40 + case[1], 600 + case[2], case[3], case[4])
  end
end

local function test_an_exact_tie_on_a_sector_edge_goes_to_the_straight_sector()
  local t = 20.784609690826528
  expect(t * t, 3 * 12 * 12, "t squared ties three times 12 squared")
  expect_mask(117, 0, 0, t, 12, 8, "tie on the 30 deg edge down right")
  expect_mask(117, 0, 0, -t, -12, 4, "tie on the 30 deg edge up left")
  expect_mask(117, 0, 0, 12, -t, 1, "tie on the 60 deg edge up right")
  expect_mask(117, 0, 0, -12, t, 2, "tie on the 60 deg edge down left")
end

local function test_a_new_stick_is_centred_on_the_touch_with_its_knob_at_the_centre_and_no_direction()
  expect_stick(pad_stick.start(40, 600, 117), 40, 600, 58.5, 40, 600, 0, "start")
end

local function test_following_the_start_point_leaves_the_fresh_stick_unchanged()
  local stick = pad_stick.start(40, 600, 117)
  pad_stick.follow(stick, 40, 600)
  expect_stick(stick, 40, 600, 58.5, 40, 600, 0, "follow at the start point")
end

local function test_a_touch_inside_the_base_puts_the_knob_on_the_touch()
  local stick = pad_stick.start(40, 600, 120)
  pad_stick.follow(stick, 50, 610)
  expect_stick(stick, 40, 600, 60, 50, 610, 0, "follow inside the base")
end

local function expect_knob(stick, knob_x, knob_y, what)
  expect(stick.knob_x, knob_x, what .. " knob_x")
  expect(stick.knob_y, knob_y, what .. " knob_y")
end

local function test_a_touch_exactly_on_the_base_edge_puts_the_knob_on_the_touch()
  local stick = pad_stick.start(40, 600, 120)
  expect(36 * 36 + 48 * 48, 60 * 60, "offset (36, 48) at the radius")
  pad_stick.follow(stick, 76, 648)
  expect_knob(stick, 76, 648, "follow on the base edge")
end

local function test_a_touch_beyond_the_base_caps_the_knob_on_the_edge_in_the_touch_direction()
  local stick = pad_stick.start(40, 600, 120)
  expect(72 * 72 + 96 * 96, 120 * 120, "offset (72, -96) at twice the radius")
  pad_stick.follow(stick, 112, 504)
  expect_knob(stick, 76, 552, "follow at twice the radius on a diagonal")
  expect(stick.mask, 9, "follow at twice the radius on a diagonal mask")
  pad_stick.follow(stick, 76, 552)
  expect(stick.mask, 9, "follow at the capped knob mask")
  pad_stick.follow(stick, 40, 800)
  expect_knob(stick, 40, 660, "follow far below")
  expect(stick.mask, 2, "follow far below mask")
end

local function test_a_touch_back_inside_the_base_after_a_capped_one_puts_the_knob_on_the_touch()
  local stick = pad_stick.start(40, 600, 120)
  pad_stick.follow(stick, 112, 504)
  pad_stick.follow(stick, 42, 601)
  expect_stick(stick, 40, 600, 60, 42, 601, 0, "follow back inside")
end

local function test_the_placed_base_size_of_a_portrait_iphone_gives_a_radius_of_58_5_points()
  local placed = pad_layout.place(window_layout.compute(390, 844).pad)
  expect(pad_stick.start(40, 600, placed.stick_base_size).radius, 58.5, "radius")
end

local function expect_refused(call, argument, ...)
  local ok, err = pcall(call, ...)
  expect(ok, false, argument .. ": call succeeded")
  expect(string.find(tostring(err), argument, 1, true) ~= nil, true, "error names " .. argument .. ": " .. tostring(err))
end

local function test_a_base_size_that_is_not_a_positive_number_or_a_missing_coordinate_is_refused_by_start()
  expect_refused(pad_stick.start, "pad_stick.start: base_size", 40, 600, 0)
  expect_refused(pad_stick.start, "pad_stick.start: base_size", 40, 600, "48")
  expect_refused(pad_stick.start, "pad_stick.start: y", 40, nil, 117)
  expect_refused(pad_stick.start, "pad_stick.start: x", "40", 600, 117)
end

local function test_a_coordinate_that_is_not_a_number_or_a_missing_stick_is_refused_by_follow()
  local stick = pad_stick.start(40, 600, 117)
  expect_refused(pad_stick.follow, "pad_stick.follow: x", stick, "50", 600)
  expect_refused(pad_stick.follow, "pad_stick.follow: y", stick, 50, nil)
  expect_refused(pad_stick.follow, "pad_stick.follow: stick", nil, 50, 600)
end

return check.run({
  { "the nearest integer points on both sides of each sector edge in all four quadrants", test_the_nearest_integer_points_on_both_sides_of_each_sector_edge_in_all_four_quadrants },
  { "an exact tie on a sector edge goes to the straight sector", test_an_exact_tie_on_a_sector_edge_goes_to_the_straight_sector },
  { "a touch exactly a quarter of the radius away on an axis points and the next double inside does not", test_a_touch_exactly_a_quarter_of_the_radius_away_on_an_axis_points_and_the_next_double_inside_does_not },
  { "a touch exactly a quarter of the radius away on a diagonal points and the next double inside does not", test_a_touch_exactly_a_quarter_of_the_radius_away_on_a_diagonal_points_and_the_next_double_inside_does_not },
  { "the masks are the bits of dpup dpdown dpleft and dpright", test_the_masks_are_the_bits_of_dpup_dpdown_dpleft_and_dpright },
  { "the axes and the middle of a diagonal give their directions with up toward the top", test_the_axes_and_the_middle_of_a_diagonal_give_their_directions_with_up_toward_the_top },
  { "a new stick is centred on the touch with its knob at the centre and no direction", test_a_new_stick_is_centred_on_the_touch_with_its_knob_at_the_centre_and_no_direction },
  { "following the start point leaves the fresh stick unchanged", test_following_the_start_point_leaves_the_fresh_stick_unchanged },
  { "a touch inside the base puts the knob on the touch", test_a_touch_inside_the_base_puts_the_knob_on_the_touch },
  { "a touch exactly on the base edge puts the knob on the touch", test_a_touch_exactly_on_the_base_edge_puts_the_knob_on_the_touch },
  { "a touch beyond the base caps the knob on the edge in the touch direction", test_a_touch_beyond_the_base_caps_the_knob_on_the_edge_in_the_touch_direction },
  { "a touch back inside the base after a capped one puts the knob on the touch", test_a_touch_back_inside_the_base_after_a_capped_one_puts_the_knob_on_the_touch },
  { "the placed base size of a portrait iphone gives a radius of 58.5 points", test_the_placed_base_size_of_a_portrait_iphone_gives_a_radius_of_58_5_points },
  { "a base size that is not a positive number or a missing coordinate is refused by start", test_a_base_size_that_is_not_a_positive_number_or_a_missing_coordinate_is_refused_by_start },
  { "a coordinate that is not a number or a missing stick is refused by follow", test_a_coordinate_that_is_not_a_number_or_a_missing_stick_is_refused_by_follow },
})
