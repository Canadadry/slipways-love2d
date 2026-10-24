local check = require("check")
local pad_layout = require("love.pad_layout")
local window_layout = require("love.window_layout")

local expect = check.expect

local BUTTON_NAMES = { "a", "b", "x", "y", "leftshoulder", "rightshoulder", "start", "back" }

local function intersects(first, second)
  return first.x < second.x + second.width and second.x < first.x + first.width
    and first.y < second.y + second.height and second.y < first.y + first.height
end

local function centre_x(rectangle)
  return rectangle.x + rectangle.width / 2
end

local function centre_y(rectangle)
  return rectangle.y + rectangle.height / 2
end

local function expect_inside_the_pad(rectangle, what)
  expect(rectangle.x >= 0, true, what .. " x >= 0")
  expect(rectangle.y >= 0, true, what .. " y >= 0")
  expect(rectangle.x + rectangle.width <= window_layout.PAD_WIDTH, true, what .. " right <= PAD_WIDTH")
  expect(rectangle.y + rectangle.height <= window_layout.PAD_HEIGHT, true, what .. " bottom <= PAD_HEIGHT")
end

local function expect_rectangle(rectangle, x, y, width, height, what)
  expect(rectangle.x, x, what .. " x")
  expect(rectangle.y, y, what .. " y")
  expect(rectangle.width, width, what .. " width")
  expect(rectangle.height, height, what .. " height")
end

local function place_for_window(width, height)
  return pad_layout.place(window_layout.compute(width, height).pad)
end

local function expect_every_button_placed_by_the_formula(placed, pad, what)
  for _, name in ipairs(BUTTON_NAMES) do
    local button = pad_layout.BUTTONS[name]
    expect_rectangle(placed.buttons[name], pad.x + button.x * pad.scale, pad.y + button.y * pad.scale,
      button.width * pad.scale, button.height * pad.scale, what .. " " .. name)
  end
end

local function test_the_buttons_are_exactly_the_eight_gamepad_names_each_a_rectangle_with_positive_size()
  local count = 0
  for _ in pairs(pad_layout.BUTTONS) do
    count = count + 1
  end
  expect(count, #BUTTON_NAMES, "button count")
  for _, name in ipairs(BUTTON_NAMES) do
    local button = pad_layout.BUTTONS[name]
    expect(type(button), "table", name)
    expect(type(button.x), "number", name .. " x")
    expect(type(button.y), "number", name .. " y")
    expect(button.width > 0, true, name .. " width > 0")
    expect(button.height > 0, true, name .. " height > 0")
  end
end

local function test_every_button_and_the_stick_zone_lie_inside_the_pad()
  for _, name in ipairs(BUTTON_NAMES) do
    expect_inside_the_pad(pad_layout.BUTTONS[name], name)
  end
  expect_inside_the_pad(pad_layout.STICK_ZONE, "stick zone")
end

local function test_no_two_buttons_overlap()
  for i = 1, #BUTTON_NAMES do
    for j = i + 1, #BUTTON_NAMES do
      local first, second = BUTTON_NAMES[i], BUTTON_NAMES[j]
      expect(intersects(pad_layout.BUTTONS[first], pad_layout.BUTTONS[second]), false, first .. " overlaps " .. second)
    end
  end
end

local function test_the_stick_zone_is_the_left_half_and_the_face_buttons_lie_in_the_right_half()
  local zone = pad_layout.STICK_ZONE
  expect(zone.x, 0, "stick zone x")
  expect(zone.y, 0, "stick zone y")
  expect(zone.width, window_layout.PAD_WIDTH / 2, "stick zone width")
  expect(zone.height, window_layout.PAD_HEIGHT, "stick zone height")
  for _, name in ipairs({ "a", "b", "x", "y" }) do
    expect(pad_layout.BUTTONS[name].x >= window_layout.PAD_WIDTH / 2, true, name .. " in the right half")
  end
end

local function test_a_b_x_y_form_an_xbox_diamond()
  local buttons = pad_layout.BUTTONS
  local a, b, x, y = buttons.a, buttons.b, buttons.x, buttons.y
  expect(centre_x(y), centre_x(a), "y and a centre x")
  expect(centre_y(x), centre_y(b), "x and b centre y")
  expect(centre_x(x) < centre_x(a) and centre_x(a) < centre_x(b), true, "centre x between x and b")
  expect(centre_y(y) < centre_y(x) and centre_y(x) < centre_y(a), true, "centre y between y and a")
  for _, other in ipairs({ b, x, y }) do
    expect(centre_y(a) > centre_y(other), true, "a is the lowest")
  end
  for _, other in ipairs({ a, b, x }) do
    expect(centre_y(y) < centre_y(other), true, "y is the highest")
  end
  for _, other in ipairs({ a, b, y }) do
    expect(centre_x(x) < centre_x(other), true, "x is the leftmost")
  end
  for _, other in ipairs({ a, x, y }) do
    expect(centre_x(b) > centre_x(other), true, "b is the rightmost")
  end
end

local function test_the_shoulders_sit_in_the_top_corners()
  local buttons = pad_layout.BUTTONS
  local left, right = buttons.leftshoulder, buttons.rightshoulder
  for _, name in ipairs(BUTTON_NAMES) do
    local button = buttons[name]
    expect(left.x <= button.x, true, "leftshoulder left of " .. name)
    expect(right.x + right.width >= button.x + button.width, true, "rightshoulder right of " .. name)
  end
  expect(left.y < buttons.y.y, true, "leftshoulder above y")
  expect(right.y < buttons.y.y, true, "rightshoulder above y")
end

local function test_back_and_start_share_the_bottom_row_centred_on_the_pad()
  local buttons = pad_layout.BUTTONS
  local back, start = buttons.back, buttons.start
  expect((centre_x(back) + centre_x(start)) / 2, window_layout.PAD_WIDTH / 2, "centre of back and start")
  expect(back.y, start.y, "back and start y")
  expect(back.height, start.height, "back and start height")
  expect(back.x < start.x, true, "back left of start")
  for _, name in ipairs(BUTTON_NAMES) do
    local button = buttons[name]
    expect(back.y + back.height >= button.y + button.height, true, "back bottom below " .. name)
  end
end

local function test_only_the_left_shoulder_and_back_lie_in_the_stick_zone()
  local inside = {}
  for _, name in ipairs(BUTTON_NAMES) do
    if intersects(pad_layout.BUTTONS[name], pad_layout.STICK_ZONE) then
      inside[#inside + 1] = name
    end
  end
  expect(table.concat(inside, " "), "leftshoulder back", "buttons in the stick zone")
end

local function test_a_portrait_iphone_places_the_pad_elements_in_the_bottom_half_at_scale_2_4375()
  local placed = place_for_window(390, 844)
  expect_rectangle(placed.buttons.a, 263.25, 691.5, 58.5, 58.5, "a")
  expect_rectangle(placed.stick_zone, 0, 486.75, 195, 292.5, "stick zone")
  expect(placed.stick_base_size, 117, "stick base size")
  expect(placed.stick_knob_size, 58.5, "stick knob size")
  expect_every_button_placed_by_the_formula(placed, { x = 0, y = 486.75, scale = 2.4375 }, "390x844")
end

local function test_every_button_is_at_least_44_points_on_a_portrait_iphone()
  local placed = place_for_window(390, 844)
  for _, name in ipairs(BUTTON_NAMES) do
    expect(placed.buttons[name].width >= 44, true, name .. " width >= 44")
    expect(placed.buttons[name].height >= 44, true, name .. " height >= 44")
  end
end

local function test_the_default_desktop_window_places_the_pad_over_the_game_at_scale_4()
  local placed = place_for_window(640, 480)
  expect_rectangle(placed.buttons.a, 432, 336, 96, 96, "a")
  expect_rectangle(placed.stick_zone, 0, 0, 320, 480, "stick zone")
  expect(placed.stick_base_size, 192, "stick base size")
  expect(placed.stick_knob_size, 96, "stick knob size")
  expect_every_button_placed_by_the_formula(placed, { x = 0, y = 0, scale = 4 }, "640x480")
end

local function test_a_landscape_iphone_places_the_pad_between_the_bars_at_scale_3_25()
  local placed = place_for_window(844, 390)
  expect_rectangle(placed.buttons.a, 513, 273, 78, 78, "a")
  expect_rectangle(placed.stick_zone, 162, 0, 260, 390, "stick zone")
  expect(placed.stick_base_size, 156, "stick base size")
  expect(placed.stick_knob_size, 78, "stick knob size")
  expect_every_button_placed_by_the_formula(placed, { x = 162, y = 0, scale = 3.25 }, "844x390")
end

local function test_an_ipad_places_the_pad_between_the_bars_at_scale_5_125()
  local placed = place_for_window(820, 1180)
  expect_rectangle(placed.buttons.a, 553.5, 713, 123, 123, "a")
  expect_rectangle(placed.stick_zone, 0, 282.5, 410, 615, "stick zone")
  expect(placed.stick_base_size, 246, "stick base size")
  expect(placed.stick_knob_size, 123, "stick knob size")
  expect_every_button_placed_by_the_formula(placed, { x = 0, y = 282.5, scale = 5.125 }, "820x1180")
end

local function test_a_small_tall_window_places_the_pad_in_the_bottom_half_at_scale_1_25()
  local placed = place_for_window(200, 420)
  expect_rectangle(placed.buttons.a, 135, 345, 30, 30, "a")
  expect_rectangle(placed.stick_zone, 0, 240, 100, 150, "stick zone")
  expect(placed.stick_base_size, 60, "stick base size")
  expect(placed.stick_knob_size, 30, "stick knob size")
  expect_every_button_placed_by_the_formula(placed, { x = 0, y = 240, scale = 1.25 }, "200x420")
end

local function test_a_call_returns_fresh_tables_and_leaves_the_table_and_earlier_results_unchanged()
  local first = place_for_window(390, 844)
  local second = place_for_window(640, 480)
  expect(rawequal(first, second), false, "both calls return the same table")
  expect(rawequal(first.buttons, second.buttons), false, "both calls return the same buttons")
  expect_rectangle(first.buttons.a, 263.25, 691.5, 58.5, 58.5, "first a after the second call")
  expect_rectangle(first.stick_zone, 0, 486.75, 195, 292.5, "first stick zone after the second call")
  second.buttons.a.x = -1
  second.stick_zone.width = -1
  expect_rectangle(pad_layout.BUTTONS.a, 108, 84, 24, 24, "table a after a change of a result")
  expect_rectangle(pad_layout.STICK_ZONE, 0, 0, 80, 120, "table stick zone after a change of a result")
  local third = place_for_window(640, 480)
  expect_rectangle(third.buttons.a, 432, 336, 96, 96, "third a")
  expect_rectangle(third.stick_zone, 0, 0, 320, 480, "third stick zone")
end

local function expect_refused(pad, field)
  local ok, err = pcall(pad_layout.place, pad)
  local what = "place with a bad " .. field
  expect(ok, false, what .. " succeeded")
  local message = tostring(err)
  expect(string.find(message, "pad_layout.place:", 1, true) ~= nil, true, what .. " message: " .. message)
  expect(string.find(message, "pad." .. field, 1, true) ~= nil, true, what .. " error names pad." .. field .. ": " .. message)
end

local function test_a_pad_without_a_positive_scale_or_a_numeric_position_is_refused_naming_the_field()
  expect_refused({ x = 0, y = 0, scale = 0 }, "scale")
  expect_refused({ y = 0, scale = 4 }, "x")
  expect_refused({ x = 0, y = "0", scale = 4 }, "y")
end

return check.run({
  { "the buttons are exactly the eight gamepad names each a rectangle with positive size", test_the_buttons_are_exactly_the_eight_gamepad_names_each_a_rectangle_with_positive_size },
  { "every button and the stick zone lie inside the pad", test_every_button_and_the_stick_zone_lie_inside_the_pad },
  { "no two buttons overlap", test_no_two_buttons_overlap },
  { "the stick zone is the left half and the face buttons lie in the right half", test_the_stick_zone_is_the_left_half_and_the_face_buttons_lie_in_the_right_half },
  { "a b x y form an xbox diamond", test_a_b_x_y_form_an_xbox_diamond },
  { "the shoulders sit in the top corners", test_the_shoulders_sit_in_the_top_corners },
  { "back and start share the bottom row centred on the pad", test_back_and_start_share_the_bottom_row_centred_on_the_pad },
  { "only the left shoulder and back lie in the stick zone", test_only_the_left_shoulder_and_back_lie_in_the_stick_zone },
  { "a portrait iphone places the pad elements in the bottom half at scale 2.4375", test_a_portrait_iphone_places_the_pad_elements_in_the_bottom_half_at_scale_2_4375 },
  { "every button is at least 44 points on a portrait iphone", test_every_button_is_at_least_44_points_on_a_portrait_iphone },
  { "the default desktop window places the pad over the game at scale 4", test_the_default_desktop_window_places_the_pad_over_the_game_at_scale_4 },
  { "a landscape iphone places the pad between the bars at scale 3.25", test_a_landscape_iphone_places_the_pad_between_the_bars_at_scale_3_25 },
  { "an ipad places the pad between the bars at scale 5.125", test_an_ipad_places_the_pad_between_the_bars_at_scale_5_125 },
  { "a small tall window places the pad in the bottom half at scale 1.25", test_a_small_tall_window_places_the_pad_in_the_bottom_half_at_scale_1_25 },
  { "a call returns fresh tables and leaves the table and earlier results unchanged", test_a_call_returns_fresh_tables_and_leaves_the_table_and_earlier_results_unchanged },
  { "a pad without a positive scale or a numeric position is refused naming the field", test_a_pad_without_a_positive_scale_or_a_numeric_position_is_refused_naming_the_field },
})
