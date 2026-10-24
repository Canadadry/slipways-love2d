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
  pad_stick.follow(stick, 40, 800)
  expect_knob(stick, 40, 660, "follow far below")
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
