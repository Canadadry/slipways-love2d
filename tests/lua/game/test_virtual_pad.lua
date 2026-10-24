local check = require("check")
local pad_layout = require("love.pad_layout")
local virtual_pad = require("love.virtual_pad")
local window_layout = require("love.window_layout")

local expect = check.expect

local MOUSE_UP = { x = 0, y = 0, down = false }

local function new_pad()
  local pad = {}
  virtual_pad.init(pad)
  return pad
end

-- Not all 4096 masks: each update costs microseconds in its callees' asserts.
local function sampled_masks()
  local masks = { 0, 4095 }
  for bit_index = 0, 11 do
    masks[#masks + 1] = 2 ^ bit_index
  end
  for mask = 0, 4095, 41 do
    masks[#masks + 1] = mask
  end
  return masks
end

local function test_with_no_touch_the_gamepad_mask_passes_unchanged_and_the_pad_stays_hidden()
  local masks = sampled_masks()
  for _, size in ipairs({ { 640, 480 }, { 390, 844 } }) do
    local pad = new_pad()
    for _, mask in ipairs(masks) do
      expect(virtual_pad.update(pad, mask, {}, MOUSE_UP, size[1], size[2]), mask, "mask " .. mask)
    end
    expect(virtual_pad.shown(pad), false, "shown at " .. size[1] .. "x" .. size[2])
  end
end

local function place_for_window(width, height)
  return pad_layout.place(window_layout.compute(width, height).pad)
end

local PHONE = place_for_window(390, 844)
local DESKTOP = place_for_window(640, 480)

local function centre(placed, name)
  local button = placed.buttons[name]
  return button.x + button.width / 2, button.y + button.height / 2
end

local function touch_on(id, placed, name)
  local x, y = centre(placed, name)
  return { id = id, x = x, y = y }
end

local function on_phone(pad, gamepad_mask, touches)
  return virtual_pad.update(pad, gamepad_mask, touches, MOUSE_UP, 390, 844)
end

local function revealed_on_phone()
  local pad = new_pad()
  on_phone(pad, 0, { touch_on("reveal", PHONE, "a") })
  on_phone(pad, 0, {})
  return pad
end

local function test_the_revealing_touch_presses_nothing_and_the_next_one_presses()
  local pad = new_pad()
  expect(on_phone(pad, 0, { touch_on(1, PHONE, "a") }), 0, "revealing touch")
  expect(virtual_pad.shown(pad), true, "shown after the revealing touch")
  expect(on_phone(pad, 0, { touch_on(1, PHONE, "a") }), 0, "revealing touch held")
  expect(on_phone(pad, 0, { touch_on(2, PHONE, "a") }), 16, "next touch on a")
end

local function test_a_gamepad_button_held_since_before_the_reveal_adds_up_with_the_pad()
  local pad = new_pad()
  expect(on_phone(pad, 32, {}), 32, "gamepad b")
  expect(on_phone(pad, 32, { touch_on(1, PHONE, "a") }), 32, "revealing touch")
  expect(virtual_pad.shown(pad), true, "shown")
  expect(on_phone(pad, 32, { touch_on(2, PHONE, "a") }), 48, "touch on a and gamepad b")
end

local function test_the_stick_steers_through_the_wiring()
  local pad = revealed_on_phone()
  local zone = PHONE.stick_zone
  local x, y = zone.x + zone.width / 2, zone.y + zone.height / 4
  expect(on_phone(pad, 0, { { id = 1, x = x, y = y } }), 0, "stick claimed")
  expect(on_phone(pad, 0, { { id = 1, x = x + 60, y = y } }), 8, "stick right")
  expect(pad.touch_state.stick ~= nil, true, "stick readable")
end

local function test_a_gamepad_press_hides_the_pad_and_releases_the_touch()
  local pad = revealed_on_phone()
  local on_a = touch_on(1, PHONE, "a")
  expect(on_phone(pad, 0, { on_a }), 16, "finger on a")
  expect(on_phone(pad, 1, { on_a }), 1, "gamepad left")
  expect(virtual_pad.shown(pad), false, "hidden by the gamepad")
  expect(on_phone(pad, 0, { on_a }), 0, "finger still on a")
end

local function click(placed, name, down)
  local x, y = centre(placed, name)
  return { x = x, y = y, down = down }
end

local function test_a_left_click_counts_as_a_touch()
  local hover = new_pad()
  expect(virtual_pad.update(hover, 0, {}, click(DESKTOP, "a", false), 640, 480), 0, "hover")
  expect(virtual_pad.shown(hover), false, "hover reveals nothing")
  local pad = new_pad()
  expect(virtual_pad.update(pad, 0, {}, click(DESKTOP, "a", true), 640, 480), 0, "revealing click")
  expect(virtual_pad.shown(pad), true, "shown after the click")
  expect(virtual_pad.update(pad, 0, {}, click(DESKTOP, "a", false), 640, 480), 0, "button up")
  expect(virtual_pad.update(pad, 0, {}, click(DESKTOP, "a", true), 640, 480), 16, "click on a")
end

local function test_a_touch_on_the_game_half_in_tall_mode_presses_nothing()
  local pad = revealed_on_phone()
  expect(on_phone(pad, 0, { { id = 1, x = 195, y = 200 } }), 0, "game half")
end

local function test_the_layout_follows_the_window_size_frame_by_frame()
  local pad = revealed_on_phone()
  expect(on_phone(pad, 0, { touch_on(1, PHONE, "a") }), 16, "a at 390x844")
  expect(virtual_pad.update(pad, 0, { touch_on(2, DESKTOP, "a") }, MOUSE_UP, 640, 480), 16, "a at 640x480")
  for _, field in ipairs({ "x", "y", "width", "height" }) do
    expect(pad.placed.buttons.a[field], DESKTOP.buttons.a[field], "placed a." .. field)
  end
end

local function test_the_inputs_are_not_modified()
  local pad = new_pad()
  local on_a = touch_on(1, PHONE, "a")
  local touches = { on_a }
  local mouse = { x = 10, y = 20, down = true }
  virtual_pad.update(pad, 0, touches, mouse, 390, 844)
  expect(#touches, 1, "touch count")
  expect(touches[1], on_a, "touch record")
  expect(on_a.id .. "," .. on_a.x .. "," .. on_a.y, "1," .. table.concat({ centre(PHONE, "a") }, ","), "touch fields")
  expect(mouse.x .. "," .. mouse.y .. "," .. tostring(mouse.down), "10,20,true", "mouse fields")
end

local function expect_refusal(message, what, call, ...)
  local ok, err = pcall(call, ...)
  expect(ok, false, what .. " refused")
  expect(tostring(err):find(message, 1, true) ~= nil, true, what .. " message: " .. tostring(err))
end

local function test_a_malformed_state_or_size_is_refused()
  expect_refusal("virtual_pad.update: virtual_pad must be a table passed to init, got nil",
    "update without state", virtual_pad.update, nil, 0, {}, MOUSE_UP, 390, 844)
  expect_refusal("virtual_pad.update: virtual_pad must be a table passed to init, got table",
    "update never init-ed", virtual_pad.update, {}, 0, {}, MOUSE_UP, 390, 844)
  expect_refusal("virtual_pad.shown: virtual_pad must be a table passed to init, got table",
    "shown never init-ed", virtual_pad.shown, {})
  expect_refusal("window_layout.compute: width must be a number > 0, got 0",
    "zero width", virtual_pad.update, new_pad(), 0, {}, MOUSE_UP, 0, 844)
end

return check.run({
  { "with no touch the gamepad mask passes unchanged and the pad stays hidden",
    test_with_no_touch_the_gamepad_mask_passes_unchanged_and_the_pad_stays_hidden },
  { "the revealing touch presses nothing and the next one presses",
    test_the_revealing_touch_presses_nothing_and_the_next_one_presses },
  { "a gamepad button held since before the reveal adds up with the pad",
    test_a_gamepad_button_held_since_before_the_reveal_adds_up_with_the_pad },
  { "the stick steers through the wiring", test_the_stick_steers_through_the_wiring },
  { "a gamepad press hides the pad and releases the touch", test_a_gamepad_press_hides_the_pad_and_releases_the_touch },
  { "a left click counts as a touch", test_a_left_click_counts_as_a_touch },
  { "a touch on the game half in tall mode presses nothing", test_a_touch_on_the_game_half_in_tall_mode_presses_nothing },
  { "the layout follows the window size frame by frame", test_the_layout_follows_the_window_size_frame_by_frame },
  { "the inputs are not modified", test_the_inputs_are_not_modified },
  { "a malformed state or size is refused", test_a_malformed_state_or_size_is_refused },
})
