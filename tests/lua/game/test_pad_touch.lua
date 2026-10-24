local check = require("check")
local pad_layout = require("love.pad_layout")
local pad_touch = require("love.pad_touch")
local window_layout = require("love.window_layout")

local expect = check.expect

local function test_button_masks_are_bits_5_to_12_keyed_by_the_eight_layout_buttons()
  local expected = { a = 16, b = 32, x = 64, y = 128, leftshoulder = 256, rightshoulder = 512, start = 1024, back = 2048 }
  local count = 0
  for name, mask in pairs(pad_touch.BUTTON_MASKS) do
    count = count + 1
    expect(mask, expected[name], name)
    expect(pad_layout.BUTTONS[name] ~= nil, true, name .. " is a layout button")
  end
  expect(count, 8, "button count")
end

local function place_for_window(width, height)
  return pad_layout.place(window_layout.compute(width, height).pad)
end

local PHONE = place_for_window(390, 844)

local function new_state()
  local touch_state = {}
  pad_touch.init(touch_state)
  return touch_state
end

local function test_no_touch_and_no_mouse_press_nothing()
  expect(pad_touch.update(new_state(), {}, nil, PHONE), 0, "mask")
end

local function centre(placed, name)
  local button = placed.buttons[name]
  return button.x + button.width / 2, button.y + button.height / 2
end

local function touch(id, x, y)
  return { id = id, x = x, y = y }
end

local function touch_on(id, placed, name)
  local x, y = centre(placed, name)
  return touch(id, x, y)
end

local function test_a_finger_sliding_from_a_to_b_releases_a_and_presses_b()
  local touch_state = new_state()
  expect(pad_touch.update(touch_state, { touch_on(1, PHONE, "a") }, nil, PHONE), 16, "on a")
  expect(pad_touch.update(touch_state, { touch(1, 331.75, 700) }, nil, PHONE), 0, "between a and b")
  expect(pad_touch.update(touch_state, { touch_on(1, PHONE, "b") }, nil, PHONE), 32, "on b")
  expect(pad_touch.update(touch_state, {}, nil, PHONE), 0, "lifted")
end

local function test_a_button_rectangle_is_half_open()
  expect(PHONE.buttons.a.x + PHONE.buttons.a.width, 321.75, "a right edge")
  expect(PHONE.buttons.a.x, 263.25, "a left edge")
  expect(PHONE.buttons.a.y, 691.5, "a top edge")
  expect(pad_touch.update(new_state(), { touch(1, 321.75, 720.75) }, nil, PHONE), 0, "on a's right edge")
  expect(pad_touch.update(new_state(), { touch(1, 292.5, 750) }, nil, PHONE), 0, "on a's bottom edge")
  expect(pad_touch.update(new_state(), { touch(1, 263.25, 691.5) }, nil, PHONE), 16, "on a's top left corner")
end

local function test_two_fingers_on_a_and_b_press_the_cheat_chord_and_lift_one_by_one()
  local touch_state = new_state()
  local on_a, on_b = touch_on(1, PHONE, "a"), touch_on(2, PHONE, "b")
  expect(pad_touch.update(touch_state, { on_a, on_b }, nil, PHONE), 48, "a and b")
  expect(pad_touch.update(touch_state, { on_b }, nil, PHONE), 32, "a lifted")
  expect(pad_touch.update(touch_state, {}, nil, PHONE), 0, "b lifted")
end

local function test_three_fingers_press_three_buttons()
  local touches = { touch_on(1, PHONE, "x"), touch_on(2, PHONE, "y"), touch_on(3, PHONE, "start") }
  expect(pad_touch.update(new_state(), touches, nil, PHONE), 64 + 128 + 1024, "x, y and start")
end

local function test_a_finger_landing_outside_the_pad_and_sliding_onto_a_presses_a()
  local touch_state = new_state()
  expect(pad_touch.update(touch_state, { touch(1, 300, 400) }, nil, PHONE), 0, "in the game half")
  expect(pad_touch.update(touch_state, { touch_on(1, PHONE, "a") }, nil, PHONE), 16, "slid onto a")
end

local function mouse_on(placed, name, down)
  local x, y = centre(placed, name)
  return { x = x, y = y, down = down }
end

local function test_the_mouse_presses_a_button_only_while_its_button_is_down()
  local touch_state = new_state()
  expect(pad_touch.update(touch_state, {}, mouse_on(PHONE, "a", true), PHONE), 16, "down on a")
  expect(pad_touch.update(touch_state, {}, mouse_on(PHONE, "a", false), PHONE), 0, "up on a")
end

local function test_the_mouse_and_a_finger_add_up()
  local touch_state = new_state()
  expect(pad_touch.update(touch_state, { touch_on(1, PHONE, "b") }, mouse_on(PHONE, "a", true), PHONE), 48, "mouse on a, finger on b")
end

local FREE_X, FREE_Y = 100, 650

local function expect_stick_base(touch_state, x, y, what)
  expect(touch_state.stick ~= nil, true, what .. " has a stick")
  expect(touch_state.stick.base_x, x, what .. " base x")
  expect(touch_state.stick.base_y, y, what .. " base y")
end

local function test_a_finger_landing_on_the_free_stick_zone_claims_the_stick_and_steers()
  local touch_state = new_state()
  expect(pad_touch.update(touch_state, { touch(1, FREE_X, FREE_Y) }, nil, PHONE), 0, "landed")
  expect_stick_base(touch_state, FREE_X, FREE_Y, "landed")
  expect(pad_touch.update(touch_state, { touch(1, FREE_X + 60, FREE_Y) }, nil, PHONE), 8, "60 right")
end

local function test_the_stick_touch_keeps_steering_out_of_its_zone_and_over_a_and_lifting_drops_it()
  local touch_state = new_state()
  pad_touch.update(touch_state, { touch(1, FREE_X, FREE_Y) }, nil, PHONE)
  expect(PHONE.stick_zone.x + PHONE.stick_zone.width, 195, "stick zone right edge")
  expect(pad_touch.update(touch_state, { touch(1, 380, FREE_Y) }, nil, PHONE), 8, "out of the zone")
  expect(pad_touch.update(touch_state, { touch_on(1, PHONE, "a") }, nil, PHONE), 8, "over a")
  expect_stick_base(touch_state, FREE_X, FREE_Y, "over a")
  expect(pad_touch.update(touch_state, {}, nil, PHONE), 0, "lifted")
  expect(touch_state.stick, nil, "stick after lifting")
end

local function test_one_thumb_steers_while_the_other_presses_a()
  local touch_state = new_state()
  pad_touch.update(touch_state, { touch(1, FREE_X, FREE_Y) }, nil, PHONE)
  expect(pad_touch.update(touch_state, { touch(1, FREE_X + 60, FREE_Y), touch_on(2, PHONE, "a") }, nil, PHONE), 8 + 16, "right and a")
end

local function test_a_finger_landing_on_l_or_back_presses_that_button_and_never_claims_the_stick()
  local on_l = new_state()
  expect(pad_touch.update(on_l, { touch_on(1, PHONE, "leftshoulder") }, nil, PHONE), 256, "on l")
  expect(on_l.stick, nil, "stick after landing on l")
  local on_back = new_state()
  expect(pad_touch.update(on_back, { touch_on(1, PHONE, "back") }, nil, PHONE), 2048, "on back")
  expect(on_back.stick, nil, "stick after landing on back")
end

local function test_a_finger_slid_off_back_never_steers()
  local touch_state = new_state()
  pad_touch.update(touch_state, { touch_on(1, PHONE, "back") }, nil, PHONE)
  expect(pad_touch.update(touch_state, { touch(1, FREE_X, FREE_Y) }, nil, PHONE), 0, "slid to the free zone")
  expect(touch_state.stick, nil, "stick after sliding off back")
  expect(pad_touch.update(touch_state, { touch(1, FREE_X + 60, FREE_Y) }, nil, PHONE), 0, "slid 60 right")
  pad_touch.update(touch_state, { touch(1, FREE_X + 60, FREE_Y), touch(2, FREE_X, FREE_Y) }, nil, PHONE)
  expect_stick_base(touch_state, FREE_X, FREE_Y, "a new finger")
end

local function test_a_second_finger_in_the_zone_while_the_stick_is_held_never_becomes_the_stick()
  local touch_state = new_state()
  pad_touch.update(touch_state, { touch(1, FREE_X, FREE_Y) }, nil, PHONE)
  expect(pad_touch.update(touch_state, { touch(1, FREE_X, FREE_Y), touch(2, 60, 600) }, nil, PHONE), 0, "second finger in the zone")
  expect_stick_base(touch_state, FREE_X, FREE_Y, "second finger in the zone")
  expect(pad_touch.update(touch_state, { touch(1, FREE_X, FREE_Y), touch_on(2, PHONE, "back") }, nil, PHONE), 2048, "second finger on back")
  expect(pad_touch.update(touch_state, { touch(2, 60, 600) }, nil, PHONE), 0, "stick lifted, second finger back in the zone")
  expect(touch_state.stick, nil, "stick after the first finger lifts")
  expect(pad_touch.update(touch_state, { touch_on(2, PHONE, "back") }, nil, PHONE), 2048, "second finger on back again")
  expect(touch_state.stick, nil, "stick with the second finger on back")
end

local function test_lifting_and_touching_again_re_centres_the_stick()
  local touch_state = new_state()
  pad_touch.update(touch_state, { touch(1, FREE_X, FREE_Y) }, nil, PHONE)
  pad_touch.update(touch_state, {}, nil, PHONE)
  pad_touch.update(touch_state, { touch(2, 50, 700) }, nil, PHONE)
  expect_stick_base(touch_state, 50, 700, "new touch")
end

local function test_a_stick_lifted_and_a_finger_landing_on_the_same_frame_hand_the_stick_over()
  local touch_state = new_state()
  pad_touch.update(touch_state, { touch(1, FREE_X, FREE_Y) }, nil, PHONE)
  pad_touch.update(touch_state, { touch(2, 50, 700) }, nil, PHONE)
  expect(touch_state.stick_id, 2, "stick owner")
  expect_stick_base(touch_state, 50, 700, "the landing finger")
end

local function test_the_mouse_held_in_the_zone_drives_the_stick_until_released()
  local touch_state = new_state()
  expect(pad_touch.update(touch_state, {}, { x = FREE_X, y = FREE_Y, down = true }, PHONE), 0, "down in the zone")
  expect_stick_base(touch_state, FREE_X, FREE_Y, "down in the zone")
  expect(pad_touch.update(touch_state, {}, { x = FREE_X + 60, y = FREE_Y, down = true }, PHONE), 8, "60 right")
  expect(pad_touch.update(touch_state, {}, { x = FREE_X + 60, y = FREE_Y, down = false }, PHONE), 0, "released")
  expect(touch_state.stick, nil, "stick after release")
end

local function test_a_resize_keeps_the_stick_where_it_started_and_moves_the_buttons()
  local desktop = place_for_window(640, 480)
  local touch_state = new_state()
  pad_touch.update(touch_state, { touch(1, FREE_X, FREE_Y) }, nil, PHONE)
  local a_x, a_y = centre(desktop, "a")
  expect(a_x, 480, "desktop a x")
  expect(a_y, 384, "desktop a y")
  local touches = { touch(1, FREE_X + 60, FREE_Y), touch_on(2, desktop, "a") }
  expect(pad_touch.update(touch_state, touches, nil, desktop), 8 + 16, "right and desktop a")
  expect_stick_base(touch_state, FREE_X, FREE_Y, "after the resize")
end

local function test_init_drops_the_stick_and_forgets_every_touch()
  local touch_state = new_state()
  pad_touch.update(touch_state, { touch(1, FREE_X, FREE_Y) }, nil, PHONE)
  local touches = { touch(1, 50, 700), touch_on(2, PHONE, "a") }
  pad_touch.update(touch_state, touches, nil, PHONE)
  pad_touch.init(touch_state)
  expect(touch_state.stick, nil, "stick after init")
  expect(pad_touch.update(touch_state, touches, nil, PHONE), 16, "the same touches after init")
  expect_stick_base(touch_state, 50, 700, "the stick finger, new again")
end

local function test_of_two_fingers_landing_in_the_zone_together_the_first_listed_claims_the_stick()
  local touch_state = new_state()
  pad_touch.update(touch_state, { touch(1, FREE_X, FREE_Y), touch(2, 50, 700) }, nil, PHONE)
  expect(touch_state.stick_id, 1, "stick owner")
  expect_stick_base(touch_state, FREE_X, FREE_Y, "the first finger")
end

local function expect_refused(call, argument, what)
  local ok, err = pcall(call)
  expect(ok, false, what .. " succeeded")
  expect(string.find(tostring(err), argument, 1, true) ~= nil, true, what .. " error names " .. argument .. ": " .. tostring(err))
end

local function test_combine_ors_the_gamepad_mask_with_the_virtual_mask()
  expect(pad_touch.combine(0, 0), 0, "0 | 0")
  expect(pad_touch.combine(16, 16), 16, "16 | 16")
  expect(pad_touch.combine(1, 2), 3, "1 | 2")
  expect(pad_touch.combine(4095, 0), 4095, "4095 | 0")
end

local function test_combine_refuses_a_mask_outside_0_to_4095_naming_the_argument()
  for _, bad in ipairs({ -1, 4096, 1.5, "16" }) do
    expect_refused(function() return pad_touch.combine(bad, 0) end, "gamepad_mask", "combine(" .. tostring(bad) .. ", 0)")
    expect_refused(function() return pad_touch.combine(0, bad) end, "virtual_mask", "combine(0, " .. tostring(bad) .. ")")
  end
end

local function expect_update_refused(touch_state, touches, mouse, placed, field, what)
  expect_refused(function() return pad_touch.update(touch_state, touches, mouse, placed) end, field, what)
end

local function test_update_refuses_malformed_input_naming_the_field()
  expect_update_refused(new_state(), "touches", nil, PHONE, "touches", "a string as touches")
  expect_update_refused(new_state(), { { x = 1, y = 2 } }, nil, PHONE, "touches[1].id", "a touch without id")
  expect_update_refused(new_state(), { touch(1, 1, 2), { id = 2, x = "1", y = 2 } }, nil, PHONE, "touches[2].x", "a string x")
  expect_update_refused(new_state(), { { id = 1, x = 1 } }, nil, PHONE, "touches[1].y", "a touch without y")
  expect_update_refused(new_state(), { touch(7, 1, 2), touch(7, 3, 4) }, nil, PHONE, "touches[2].id", "two touches with id 7")
  expect_update_refused(new_state(), {}, { x = 1, y = 2, down = 1 }, PHONE, "mouse.down", "a number as mouse.down")
  expect_update_refused(new_state(), {}, { x = "1", y = 2, down = false }, PHONE, "mouse.x", "a string as mouse.x")
  expect_update_refused(new_state(), {}, true, PHONE, "mouse", "a boolean as mouse")
  expect_update_refused(new_state(), {}, nil, {}, "placed.stick_zone", "an empty placed")
  expect_update_refused(new_state(), {}, nil, { stick_zone = PHONE.stick_zone, stick_base_size = 117 }, "placed.buttons", "a placed without buttons")
  expect_update_refused(new_state(), {}, nil, { stick_zone = PHONE.stick_zone, buttons = PHONE.buttons }, "placed.stick_base_size", "a placed without stick_base_size")
  expect_update_refused({}, {}, nil, PHONE, "touch_state", "a state never initialised")
  expect_update_refused(nil, {}, nil, PHONE, "touch_state", "no state")
end

return check.run({
  { "button masks are bits 5 to 12 keyed by the eight layout buttons", test_button_masks_are_bits_5_to_12_keyed_by_the_eight_layout_buttons },
  { "no touch and no mouse press nothing", test_no_touch_and_no_mouse_press_nothing },
  { "a finger sliding from a to b releases a and presses b", test_a_finger_sliding_from_a_to_b_releases_a_and_presses_b },
  { "a button rectangle is half open", test_a_button_rectangle_is_half_open },
  { "two fingers on a and b press the cheat chord and lift one by one", test_two_fingers_on_a_and_b_press_the_cheat_chord_and_lift_one_by_one },
  { "three fingers press three buttons", test_three_fingers_press_three_buttons },
  { "a finger landing outside the pad and sliding onto a presses a", test_a_finger_landing_outside_the_pad_and_sliding_onto_a_presses_a },
  { "the mouse presses a button only while its button is down", test_the_mouse_presses_a_button_only_while_its_button_is_down },
  { "the mouse and a finger add up", test_the_mouse_and_a_finger_add_up },
  { "update refuses malformed input naming the field", test_update_refuses_malformed_input_naming_the_field },
  { "a finger landing on the free stick zone claims the stick and steers", test_a_finger_landing_on_the_free_stick_zone_claims_the_stick_and_steers },
  { "the stick touch keeps steering out of its zone and over a and lifting drops it", test_the_stick_touch_keeps_steering_out_of_its_zone_and_over_a_and_lifting_drops_it },
  { "one thumb steers while the other presses a", test_one_thumb_steers_while_the_other_presses_a },
  { "a finger landing on l or back presses that button and never claims the stick", test_a_finger_landing_on_l_or_back_presses_that_button_and_never_claims_the_stick },
  { "a finger slid off back never steers", test_a_finger_slid_off_back_never_steers },
  { "a second finger in the zone while the stick is held never becomes the stick", test_a_second_finger_in_the_zone_while_the_stick_is_held_never_becomes_the_stick },
  { "lifting and touching again re centres the stick", test_lifting_and_touching_again_re_centres_the_stick },
  { "a stick lifted and a finger landing on the same frame hand the stick over", test_a_stick_lifted_and_a_finger_landing_on_the_same_frame_hand_the_stick_over },
  { "the mouse held in the zone drives the stick until released", test_the_mouse_held_in_the_zone_drives_the_stick_until_released },
  { "a resize keeps the stick where it started and moves the buttons", test_a_resize_keeps_the_stick_where_it_started_and_moves_the_buttons },
  { "init drops the stick and forgets every touch", test_init_drops_the_stick_and_forgets_every_touch },
  { "of two fingers landing in the zone together the first listed claims the stick", test_of_two_fingers_landing_in_the_zone_together_the_first_listed_claims_the_stick },
  { "combine ors the gamepad mask with the virtual mask", test_combine_ors_the_gamepad_mask_with_the_virtual_mask },
  { "combine refuses a mask outside 0 to 4095 naming the argument", test_combine_refuses_a_mask_outside_0_to_4095_naming_the_argument },
})
