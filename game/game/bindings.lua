-- A bindings table is { masks }, masks[mode][action] by bindings.MODE_* and
-- bindings.ACTION_*, every entry present, zeros included; a parse error is
-- { line, text }. A range [start, end_) of a parsed text excludes end_; a
-- position past the text reads 0.
-- The parameter error of fail, parse_line and bindings.parse shadows Lua's
-- error there: raise through fatal.raise, never error().
local bit_module = require("bit")
local input = require("p8.input")

local M = {}

M.MODE_TITLE = 1
M.MODE_JUMP = 2
M.MODE_AIM = 3
M.MODE_SLIPWAY = 4
M.MODE_MENU = 5
M.MODE_PAUSE = 6
M.MODE_BUILD_BAR = 7
M.MODE_PLACEMENT = 8
M.MODE_PANELS = 9
M.MODE_COUNT = 9

M.ACTION_MOVE_UP = 1
M.ACTION_MOVE_DOWN = 2
M.ACTION_MOVE_LEFT = 3
M.ACTION_MOVE_RIGHT = 4
M.ACTION_CHOOSE_ROW = 5
M.ACTION_JUMP_UP = 6
M.ACTION_JUMP_DOWN = 7
M.ACTION_JUMP_LEFT = 8
M.ACTION_JUMP_RIGHT = 9
M.ACTION_ACT_ON_HOVERED = 10
M.ACTION_BACK = 11
M.ACTION_ENTER_AIM = 12
M.ACTION_OPEN_MENU = 13
M.ACTION_OPEN_BUILD_BAR = 14
M.ACTION_OPEN_TECH_PANEL = 15
M.ACTION_PAUSE = 16
M.ACTION_SELECT_TAP_OR_YEAR = 17
M.ACTION_PAN_UP = 18
M.ACTION_PAN_DOWN = 19
M.ACTION_PAN_LEFT = 20
M.ACTION_PAN_RIGHT = 21
M.ACTION_PROBE_OR_FOLD = 22
M.ACTION_LEAVE_AIM = 23
M.ACTION_BUILD_ON_TARGET = 24
M.ACTION_CANCEL_SLIPWAY = 25
M.ACTION_CLOSE_MENU = 26
M.ACTION_CLOSE_PAUSE = 27
M.ACTION_PICK_LEFT = 28
M.ACTION_PICK_RIGHT = 29
M.ACTION_TAKE_STRUCTURE = 30
M.ACTION_CLOSE_BAR = 31
M.ACTION_PLACE_STRUCTURE = 32
M.ACTION_CANCEL_PLACEMENT = 33
M.ACTION_CLOSE_PANEL = 34
M.ACTION_NEXT_PANEL = 35
M.ACTION_COUNT = 35

M.mode_names = {
  "title", "jump", "aim", "slipway", "menu", "pause", "build_bar", "placement", "panels",
}

M.action_names = {
  "move_up", "move_down", "move_left", "move_right", "choose_row",
  "jump_up", "jump_down", "jump_left", "jump_right", "act_on_hovered",
  "back", "enter_aim", "open_menu", "open_build_bar", "open_tech_panel",
  "pause", "select_tap_or_year", "pan_up", "pan_down", "pan_left",
  "pan_right", "probe_or_fold", "leave_aim", "build_on_target", "cancel_slipway",
  "close_menu", "close_pause", "pick_left", "pick_right", "take_structure",
  "close_bar", "place_structure", "cancel_placement", "close_panel", "next_panel",
}

local MENU_ROWS = {
  M.ACTION_MOVE_UP, M.ACTION_MOVE_DOWN, M.ACTION_MOVE_LEFT,
  M.ACTION_MOVE_RIGHT, M.ACTION_CHOOSE_ROW,
}
local JUMPS = {
  M.ACTION_JUMP_UP, M.ACTION_JUMP_DOWN, M.ACTION_JUMP_LEFT,
  M.ACTION_JUMP_RIGHT,
}
local PANS = {
  M.ACTION_PAN_UP, M.ACTION_PAN_DOWN, M.ACTION_PAN_LEFT,
  M.ACTION_PAN_RIGHT,
}
local MAP_SHORTCUTS = {
  M.ACTION_OPEN_BUILD_BAR, M.ACTION_OPEN_TECH_PANEL, M.ACTION_PAUSE,
}

-- One row of mode_actions: true for each action of the groups and the listed
-- actions, false for every other action.
local function mode_actions_row(groups, ...)
  local row = {}
  for action = 1, M.ACTION_COUNT do
    row[action] = false
  end
  for _, group in ipairs(groups) do
    for _, action in ipairs(group) do
      row[action] = true
    end
  end
  for _, action in ipairs({ ... }) do
    row[action] = true
  end
  return row
end

M.mode_actions = {
  [M.MODE_TITLE] = mode_actions_row({ MENU_ROWS }),
  [M.MODE_JUMP] = mode_actions_row({ JUMPS, MAP_SHORTCUTS }, M.ACTION_ACT_ON_HOVERED,
    M.ACTION_BACK, M.ACTION_ENTER_AIM, M.ACTION_OPEN_MENU,
    M.ACTION_SELECT_TAP_OR_YEAR),
  [M.MODE_AIM] = mode_actions_row({ PANS, MAP_SHORTCUTS }, M.ACTION_PROBE_OR_FOLD,
    M.ACTION_BACK, M.ACTION_LEAVE_AIM, M.ACTION_SELECT_TAP_OR_YEAR),
  [M.MODE_SLIPWAY] = mode_actions_row({ JUMPS, MAP_SHORTCUTS }, M.ACTION_BUILD_ON_TARGET,
    M.ACTION_CANCEL_SLIPWAY),
  [M.MODE_MENU] = mode_actions_row({ MENU_ROWS }, M.ACTION_CLOSE_MENU,
    M.ACTION_OPEN_MENU, M.ACTION_OPEN_BUILD_BAR, M.ACTION_OPEN_TECH_PANEL),
  [M.MODE_PAUSE] = mode_actions_row({ MENU_ROWS }, M.ACTION_CLOSE_PAUSE),
  [M.MODE_BUILD_BAR] = mode_actions_row({ MAP_SHORTCUTS }, M.ACTION_PICK_LEFT,
    M.ACTION_PICK_RIGHT, M.ACTION_TAKE_STRUCTURE, M.ACTION_CLOSE_BAR),
  [M.MODE_PLACEMENT] = mode_actions_row({ PANS, MAP_SHORTCUTS }, M.ACTION_PLACE_STRUCTURE,
    M.ACTION_CANCEL_PLACEMENT),
  [M.MODE_PANELS] = mode_actions_row({ MENU_ROWS }, M.ACTION_CLOSE_PANEL,
    M.ACTION_OPEN_BUILD_BAR, M.ACTION_NEXT_PANEL),
}

local function byte_at(s, position)
  return string.byte(s, position) or 0
end

-- The position of byte c in [start, start + length), or nil.
local function memchr(s, start, c, length)
  local found = string.find(s, c, start, true)
  if found ~= nil and found < start + length then
    return found
  end
  return nil
end

local function new_masks()
  local masks = {}
  for mode = 1, M.MODE_COUNT do
    masks[mode] = {}
    for action = 1, M.ACTION_COUNT do
      masks[mode][action] = 0
    end
  end
  return masks
end

-- Returns the number, then the position where its digits stop: 0 and
-- position 1 when text does not start with 1 to 12.
function M.read_button_number(text)
  assert(text ~= nil, "bindings.read_button_number needs a text")
  local end_ = 1
  if byte_at(text, 1) < 49 or byte_at(text, 1) > 57 then
    return 0, end_
  end
  local cursor = 1
  local number = 0
  while byte_at(text, cursor) >= 48 and byte_at(text, cursor) <= 57 do
    if number <= input.PAD_BUTTON_COUNT then
      number = number * 10 + (byte_at(text, cursor) - 48)
    end
    cursor = cursor + 1
  end
  if number > input.PAD_BUTTON_COUNT then
    return 0, end_
  end
  end_ = cursor
  return number, end_
end

function M.mode_has_action(mode, action)
  assert(mode >= 1 and mode <= M.MODE_COUNT, "bindings.mode_has_action: mode out of range")
  assert(action >= 1 and action <= M.ACTION_COUNT, "bindings.mode_has_action: action out of range")
  return M.mode_actions[mode][action]
end

function M.mask(bindings, mode, action)
  assert(bindings ~= nil, "bindings.mask needs a table")
  if not M.mode_has_action(mode, action) then
    return 0
  end
  return bindings.masks[mode][action]
end

function M.mode_name(mode)
  assert(mode >= 1 and mode <= M.MODE_COUNT, "bindings.mode_name: mode out of range")
  return M.mode_names[mode]
end

function M.action_name(action)
  assert(action >= 1 and action <= M.ACTION_COUNT, "bindings.action_name: action out of range")
  return M.action_names[action]
end

local function fail(error, file_name, line_number, format, ...)
  error.line = line_number
  error.text = string.format("%s:%d: ", file_name, line_number) .. string.format(format, ...)
  return false
end

local function skip_blanks(s, start, end_)
  while start < end_ and (byte_at(s, start) == 32 or byte_at(s, start) == 9) do
    start = start + 1
  end
  return start
end

local function trim_end(s, start, end_)
  while end_ > start and (byte_at(s, end_ - 1) == 32 or byte_at(s, end_ - 1) == 9) do
    end_ = end_ - 1
  end
  return end_
end

local function copy_token(s, start, end_)
  return s:sub(start, end_ - 1)
end

-- Returns nil for an unknown name.
local function find_mode(name)
  for mode = 1, M.MODE_COUNT do
    if M.mode_names[mode] == name then
      return mode
    end
  end
  return nil
end

-- Returns nil for an unknown name.
local function find_action(name)
  for action = 1, M.ACTION_COUNT do
    if M.action_names[action] == name then
      return action
    end
  end
  return nil
end

local function mode_mask(bindings, mode)
  local mask = 0
  for action = 1, M.ACTION_COUNT do
    mask = bit_module.bor(mask, bindings.masks[mode][action])
  end
  return mask
end

local function parse_line(file_name, line_number, text, start, end_, out, given_on_line, error)
  local equal = memchr(text, start, "=", end_ - start)
  if equal == nil then
    return fail(error, file_name, line_number, "no '=' in \"%s\"", copy_token(text, start, end_))
  end
  local left_end = trim_end(text, start, equal)
  local dot = memchr(text, start, ".", left_end - start)
  if dot == nil then
    return fail(error, file_name, line_number, "expected mode.action, got \"%s\"", copy_token(text, start, left_end))
  end
  local token = copy_token(text, start, trim_end(text, start, dot))
  local mode = find_mode(token)
  if mode == nil then
    return fail(error, file_name, line_number, "unknown mode \"%s\"", token)
  end
  local action_start = skip_blanks(text, dot + 1, left_end)
  token = copy_token(text, action_start, left_end)
  local action = find_action(token)
  if action == nil or not M.mode_actions[mode][action] then
    return fail(error, file_name, line_number, "unknown action \"%s\" in mode %s", token, M.mode_names[mode])
  end
  local first_line = given_on_line[mode][action]
  if first_line ~= 0 then
    return fail(error, file_name, line_number, "%s.%s already given on line %d", M.mode_names[mode],
      M.action_names[action], first_line)
  end
  given_on_line[mode][action] = line_number
  local bound_in_mode = mode_mask(out, mode)
  local line_mask = 0
  local item = equal + 1
  while true do
    local comma = memchr(text, item, ",", end_ - item)
    local item_end = (comma ~= nil) and comma or end_
    local item_start = skip_blanks(text, item, item_end)
    token = copy_token(text, item_start, trim_end(text, item_start, item_end))
    if byte_at(token, 1) == 0 then
      return fail(error, file_name, line_number, "empty button in the line of %s.%s", M.mode_names[mode],
        M.action_names[action])
    end
    local number, digits_end = M.read_button_number(token)
    if number == 0 or digits_end <= #token then
      return fail(error, file_name, line_number, "bad button \"%s\": expected a number from 1 to %d", token,
        input.PAD_BUTTON_COUNT)
    end
    local bit = bit_module.lshift(1, number - 1)
    if bit_module.band(line_mask, bit) ~= 0 then
      return fail(error, file_name, line_number, "button %d listed twice for %s.%s", number, M.mode_names[mode],
        M.action_names[action])
    end
    if bit_module.band(bound_in_mode, bit) ~= 0 then
      return fail(error, file_name, line_number, "button %d already bound in mode %s", number, M.mode_names[mode])
    end
    line_mask = bit_module.bor(line_mask, bit)
    if comma == nil then
      break
    end
    item = comma + 1
  end
  out.masks[mode][action] = line_mask
  return true
end

-- A NUL byte ends the text.
function M.parse(file_name, text, out, error)
  assert(file_name ~= nil and text ~= nil and out ~= nil and error ~= nil, "bindings.parse: NULL argument")
  out.masks = new_masks()
  error.line = 0
  error.text = ""
  local given_on_line = new_masks()
  local line_number = 0
  local text_end = string.find(text, "\0", 1, true) or #text + 1
  local line = 1
  while line < text_end do
    line_number = line_number + 1
    local newline = string.find(text, "\n", line, true)
    if newline ~= nil and newline >= text_end then
      newline = nil
    end
    local end_ = (newline ~= nil) and newline or text_end
    local next_ = (newline ~= nil) and newline + 1 or end_
    if end_ > line and byte_at(text, end_ - 1) == 13 then
      end_ = end_ - 1
    end
    local length = (end_ > line) and end_ - line or 0
    local hash = memchr(text, line, "#", length)
    local content_end = (hash ~= nil) and hash or end_
    local content_start = skip_blanks(text, line, content_end)
    content_end = trim_end(text, content_start, content_end)
    if content_start < content_end
      and not parse_line(file_name, line_number, text, content_start, content_end, out, given_on_line, error) then
      return false
    end
    line = next_
  end
  return true
end

return M
