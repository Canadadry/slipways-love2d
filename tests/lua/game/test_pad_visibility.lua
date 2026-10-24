local check = require("check")
local pad_visibility = require("love.pad_visibility")

local expect = check.expect

local function new_visibility()
  local visibility = {}
  pad_visibility.init(visibility)
  return visibility
end

local function ids(touches)
  local list = {}
  for index, record in ipairs(touches) do
    list[index] = tostring(record.id)
  end
  return table.concat(list, ",")
end

local function touch(id)
  return { id = id, x = 0, y = 0 }
end

local function expect_frame(visibility, gamepad_mask, touches, mouse, shown, passed_ids, mouse_passed, what)
  local passed, passed_mouse = pad_visibility.filter(visibility, gamepad_mask, touches, mouse)
  expect(pad_visibility.shown(visibility), shown, what .. " shown")
  expect(ids(passed), passed_ids, what .. " passed touches")
  expect(passed_mouse ~= nil, mouse_passed, what .. " mouse passed")
end

local function test_the_pad_starts_hidden_and_an_empty_frame_keeps_it_hidden()
  local visibility = new_visibility()
  expect(pad_visibility.shown(visibility), false, "after init")
  expect_frame(visibility, 0, {}, nil, false, "", false, "empty frame")
end

local function test_the_revealing_touch_shows_the_pad_and_passes_nowhere_until_it_lifts()
  local visibility = new_visibility()
  expect_frame(visibility, 0, { touch(1) }, nil, true, "", false, "landed")
  expect_frame(visibility, 0, { { id = 1, x = 40, y = 700 } }, nil, true, "", false, "moved")
  expect_frame(visibility, 0, { { id = 1, x = 300, y = 720 } }, nil, true, "", false, "moved again")
  expect_frame(visibility, 0, {}, nil, true, "", false, "lifted")
  expect_frame(visibility, 0, { touch(1) }, nil, true, "1", false, "id 1 reused")
end

local function test_a_second_finger_landing_while_the_revealing_one_is_held_passes()
  local visibility = new_visibility()
  expect_frame(visibility, 0, { touch(1) }, nil, true, "", false, "1 reveals")
  expect_frame(visibility, 0, { touch(1), touch(2) }, nil, true, "2", false, "2 lands")
  expect_frame(visibility, 0, { touch(1), touch(2) }, nil, true, "2", false, "both held")
  expect_frame(visibility, 0, { touch(2), touch(1) }, nil, true, "2", false, "list order swapped")
end

local function test_two_fingers_landing_together_on_a_hidden_pad_both_pass_nowhere_until_each_lifts()
  local visibility = new_visibility()
  expect_frame(visibility, 0, { touch(1), touch(2) }, nil, true, "", false, "both land")
  expect_frame(visibility, 0, { touch(1), touch(2) }, nil, true, "", false, "both held")
  expect_frame(visibility, 0, { touch(2) }, nil, true, "", false, "1 lifted")
  expect_frame(visibility, 0, { touch(1), touch(2) }, nil, true, "1", false, "1 lands again")
  expect_frame(visibility, 0, { touch(1) }, nil, true, "1", false, "2 lifted")
  expect_frame(visibility, 0, { touch(1), touch(2) }, nil, true, "1,2", false, "2 lands again")
end

local function mouse(down)
  return { x = 10, y = 20, down = down }
end

local function test_a_mouse_moving_with_its_button_up_shows_nothing()
  local visibility = new_visibility()
  expect_frame(visibility, 0, {}, mouse(false), false, "", false, "button up")
  expect_frame(visibility, 0, {}, { x = 300, y = 720, down = false }, false, "", false, "moved, button up")
end

local function test_the_revealing_click_shows_the_pad_and_passes_nowhere_until_its_button_goes_up()
  local visibility = new_visibility()
  expect_frame(visibility, 0, {}, mouse(true), true, "", false, "clicked")
  expect_frame(visibility, 0, {}, { x = 300, y = 720, down = true }, true, "", false, "dragged")
  expect_frame(visibility, 0, {}, mouse(false), true, "", true, "released")
  expect_frame(visibility, 0, {}, mouse(true), true, "", true, "clicked again")
end

local function test_a_nil_mouse_lifts_a_held_back_click()
  local visibility = new_visibility()
  expect_frame(visibility, 0, {}, mouse(true), true, "", false, "clicked")
  expect_frame(visibility, 0, {}, nil, true, "", false, "mouse gone")
  expect_frame(visibility, 0, {}, mouse(true), true, "", true, "clicked again")
end

local function test_the_pad_stays_shown_through_100_empty_frames()
  local visibility = new_visibility()
  pad_visibility.filter(visibility, 0, { touch(1) }, nil)
  for frame = 1, 100 do
    expect_frame(visibility, 0, {}, nil, true, "", false, "empty frame " .. frame)
  end
end

local function shown_with_touches_3_4_and_the_mouse_passing()
  local visibility = new_visibility()
  pad_visibility.filter(visibility, 0, { touch(1) }, nil)
  pad_visibility.filter(visibility, 0, {}, nil)
  expect_frame(visibility, 0, { touch(3), touch(4) }, mouse(true), true, "3,4", true, "3, 4 and the mouse pass")
  return visibility
end

local function test_a_gamepad_press_hides_the_pad_and_holds_back_every_touch_until_it_lifts()
  local visibility = shown_with_touches_3_4_and_the_mouse_passing()
  expect_frame(visibility, 16, { touch(3), touch(4) }, mouse(true), false, "", false, "a pressed")
  expect_frame(visibility, 16, { touch(3), touch(4) }, mouse(true), false, "", false, "a held")
  expect_frame(visibility, 0, { touch(3), touch(4) }, mouse(true), false, "", false, "a released")
  expect_frame(visibility, 0, { touch(3), touch(4), touch(5) }, mouse(true), true, "", false, "5 reveals")
  expect_frame(visibility, 0, { touch(3), touch(4), touch(5), touch(6) }, mouse(true), true, "6", false, "6 lands")
  expect_frame(visibility, 0, { touch(6) }, mouse(false), true, "6", true, "the others lifted")
end

local function test_a_button_held_since_before_the_reveal_keeps_the_pad_shown_and_a_new_bit_hides_it()
  local visibility = new_visibility()
  expect_frame(visibility, 16, {}, nil, false, "", false, "a pressed while hidden")
  expect_frame(visibility, 16, { touch(1) }, nil, true, "", false, "1 reveals, a held")
  expect_frame(visibility, 16, { touch(2) }, nil, true, "2", false, "a still held")
  expect_frame(visibility, 16 + 32, { touch(2) }, nil, false, "", false, "b pressed")
end

local function test_each_of_the_12_gamepad_bits_alone_hides_a_shown_pad()
  for bit_index = 0, 11 do
    local visibility = new_visibility()
    pad_visibility.filter(visibility, 0, { touch(1) }, nil)
    expect_frame(visibility, 2 ^ bit_index, {}, nil, false, "", false, "bit " .. bit_index)
  end
end

local function test_a_press_on_the_frame_a_touch_lands_wins_and_holds_the_touch_back()
  local visibility = new_visibility()
  expect_frame(visibility, 1, { touch(1) }, nil, false, "", false, "press and touch")
  expect_frame(visibility, 0, { touch(1) }, nil, false, "", false, "touch still held")
  expect_frame(visibility, 0, { touch(1), touch(2) }, nil, true, "", false, "2 reveals")
end

local function test_a_press_on_the_frame_a_click_lands_wins_and_holds_the_click_back()
  local visibility = new_visibility()
  expect_frame(visibility, 1, {}, mouse(true), false, "", false, "press and click")
  expect_frame(visibility, 0, {}, mouse(true), false, "", false, "click still held")
  expect_frame(visibility, 0, { touch(2) }, mouse(true), true, "", false, "2 reveals")
end

local function test_a_press_while_hidden_changes_nothing()
  local visibility = new_visibility()
  expect_frame(visibility, 2048, {}, nil, false, "", false, "back pressed")
  expect_frame(visibility, 0, { touch(1) }, nil, true, "", false, "1 reveals")
end

local function test_filter_returns_a_new_array_of_the_callers_records_and_leaves_its_inputs_unchanged()
  local visibility = new_visibility()
  local first, second = touch(1), touch(2)
  local the_mouse = mouse(true)
  pad_visibility.filter(visibility, 0, { first }, nil)
  local touches = { first, second }
  local passed, passed_mouse = pad_visibility.filter(visibility, 0, touches, the_mouse)
  expect(passed ~= touches, true, "a new array")
  expect(#passed, 1, "passed count")
  expect(passed[1], second, "the caller's record")
  expect(passed_mouse, the_mouse, "the caller's mouse")
  expect(#touches, 2, "input count")
  expect(touches[1], first, "input record 1")
  expect(touches[2], second, "input record 2")
  expect(ids(touches), "1,2", "input ids")
  expect(first.x, 0, "record 1 x")
  expect(the_mouse.down, true, "mouse down")
end

local function expect_refused(call, field, what)
  local ok, err = pcall(call)
  expect(ok, false, what .. " succeeded")
  expect(string.find(tostring(err), field, 1, true) ~= nil, true, what .. " error names " .. field .. ": " .. tostring(err))
end

local function expect_filter_refused(visibility, gamepad_mask, touches, the_mouse, field, what)
  expect_refused(function() return pad_visibility.filter(visibility, gamepad_mask, touches, the_mouse) end,
    "pad_visibility.filter: " .. field, what)
end

local function test_filter_refuses_malformed_input_naming_the_field()
  for _, bad in ipairs({ -1, 4096, 1.5, "16" }) do
    expect_filter_refused(new_visibility(), bad, {}, nil, "gamepad_mask", "mask " .. tostring(bad))
  end
  expect_filter_refused(new_visibility(), 0, nil, nil, "touches", "nil touches")
  expect_filter_refused(new_visibility(), 0, { touch(1), 5 }, nil, "touches[2]", "a number as a record")
  expect_filter_refused(new_visibility(), 0, { { x = 1, y = 2 } }, nil, "touches[1].id", "a touch without id")
  expect_filter_refused(new_visibility(), 0, { touch(7), touch(7) }, nil, "touches[2].id", "two touches with id 7")
  expect_filter_refused(new_visibility(), 0, {}, { x = 1, y = 2, down = 1 }, "mouse.down", "a number as mouse.down")
  expect_filter_refused(new_visibility(), 0, {}, true, "mouse", "a boolean as mouse")
  expect_filter_refused({}, 0, {}, nil, "visibility", "a state never initialised")
  expect_filter_refused(nil, 0, {}, nil, "visibility", "no state")
end

local function test_shown_refuses_a_state_never_initialised()
  expect_refused(function() return pad_visibility.shown({}) end, "pad_visibility.shown: visibility", "a state never initialised")
  expect_refused(function() return pad_visibility.shown(nil) end, "pad_visibility.shown: visibility", "no state")
end

local function test_init_refuses_a_non_table()
  expect_refused(function() pad_visibility.init(nil) end, "pad_visibility.init: visibility", "no state")
end

return check.run({
  { "the pad starts hidden and an empty frame keeps it hidden", test_the_pad_starts_hidden_and_an_empty_frame_keeps_it_hidden },
  { "the revealing touch shows the pad and passes nowhere until it lifts", test_the_revealing_touch_shows_the_pad_and_passes_nowhere_until_it_lifts },
  { "a second finger landing while the revealing one is held passes", test_a_second_finger_landing_while_the_revealing_one_is_held_passes },
  { "two fingers landing together on a hidden pad both pass nowhere until each lifts", test_two_fingers_landing_together_on_a_hidden_pad_both_pass_nowhere_until_each_lifts },
  { "a mouse moving with its button up shows nothing", test_a_mouse_moving_with_its_button_up_shows_nothing },
  { "the revealing click shows the pad and passes nowhere until its button goes up", test_the_revealing_click_shows_the_pad_and_passes_nowhere_until_its_button_goes_up },
  { "a nil mouse lifts a held back click", test_a_nil_mouse_lifts_a_held_back_click },
  { "the pad stays shown through 100 empty frames", test_the_pad_stays_shown_through_100_empty_frames },
  { "a gamepad press hides the pad and holds back every touch until it lifts", test_a_gamepad_press_hides_the_pad_and_holds_back_every_touch_until_it_lifts },
  { "a button held since before the reveal keeps the pad shown and a new bit hides it", test_a_button_held_since_before_the_reveal_keeps_the_pad_shown_and_a_new_bit_hides_it },
  { "each of the 12 gamepad bits alone hides a shown pad", test_each_of_the_12_gamepad_bits_alone_hides_a_shown_pad },
  { "a press on the frame a touch lands wins and holds the touch back", test_a_press_on_the_frame_a_touch_lands_wins_and_holds_the_touch_back },
  { "a press on the frame a click lands wins and holds the click back", test_a_press_on_the_frame_a_click_lands_wins_and_holds_the_click_back },
  { "a press while hidden changes nothing", test_a_press_while_hidden_changes_nothing },
  { "filter returns a new array of the callers records and leaves its inputs unchanged", test_filter_returns_a_new_array_of_the_callers_records_and_leaves_its_inputs_unchanged },
  { "filter refuses malformed input naming the field", test_filter_refuses_malformed_input_naming_the_field },
  { "shown refuses a state never initialised", test_shown_refuses_a_state_never_initialised },
  { "init refuses a non table", test_init_refuses_a_non_table },
})
