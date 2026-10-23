-- Copy of src/game/score_panel_display.h then src/game/score_panel_display.c.
--
-- Also defines score_panel_new_year_method, score_panel_update_method, score_panel_display_init and
-- score_panel_display_shutdown, declared in score_panel.h. message_box_keys_t is
-- { has_duration_scale, duration_scale, keep_open }.
--
-- Include cycle: game.score_panel requires this module at load (its class holds the two methods), so
-- game.score_panel is required here at call time.
local entity_registry = require("engine.entity_registry")
local vec = require("engine.vec")
local economy_score = require("game.economy_score")
local game_data = require("game.game_data")
local layout = require("game.layout")
local map_tables = require("game.map_tables")
local message_box = require("game.message_box")
local ui_button = require("game.ui_button")
local cint = require("p8.cint")
local fixed = require("p8.fixed")
local str = require("p8.str")
local vram = require("p8.vram")

local M = {}

local data_item = map_tables.map_tables_make_data_item
local data_row = map_tables.map_tables_make_data_row

-- data_item_t assigned into storage: every field of src written into dst.
local function data_item_assign(dst, src)
  dst.text = src.text
  dst.number = src.number
  dst.row = src.row
  dst.is_false = src.is_false
  dst.is_fixed = src.is_fixed
  dst.fixed = src.fixed
  dst.is_nil = src.is_nil
end

local function message_box_keys_zero()
  return { has_duration_scale = 0, duration_scale = 0, keep_open = nil }
end

local g_score_panel_new_year_keep_open = 0

local function score_panel_new_year_keep_open_method(self)
  g_score_panel_new_year_keep_open = g_score_panel_new_year_keep_open - 1
  return (g_score_panel_new_year_keep_open > 0) and 1 or 0
end

local function score_panel_year_banner_label_row()
  local year_banner = require("game.score_panel").score_panel_year_banner()

  if year_banner ~= nil then
    return year_banner.second
  end
  return nil
end

function M.score_panel_new_year(year)
  local score_panel = require("game.score_panel")
  local year_banner = score_panel.score_panel_year_banner()
  local countdown_rows = score_panel.score_panel_countdown_rows()
  local message = nil
  local year_str
  local countdown_index

  if year_banner == nil then
    return
  end

  countdown_index = 3426 - fixed.p8_fixed_flr_int(year)
  if countdown_index >= 1 and countdown_index <= countdown_rows.count then
    local item = countdown_rows.items[countdown_index - 1]

    assert(item.row ~= nil or item.is_false ~= 0, "score_panel_new_year: the countdown rows hold rows and false only")
    message = item.row
  end

  year_str = str.p8_tostr_fixed(year)
  assert(#"year " + #year_str <= score_panel.SCORE_PANEL_TEXT_CAP,
    "score_panel_new_year: a year is at most a 5-digit integer")
  year_banner.year = "year " .. year_str

  data_item_assign(year_banner.second_items[0], data_item({ text = year_banner.year }))

  if message ~= nil then
    data_item_assign(year_banner.items[2], data_item({ row = message }))
    year_banner.row.count = 3
    g_score_panel_new_year_keep_open = 90
    year_banner.keep_open = score_panel_new_year_keep_open_method
  else
    year_banner.row.count = 2
  end

  do
    local keys = message_box_keys_zero()
    local made

    keys.keep_open = year_banner.keep_open
    made = message_box.message_box_new(keys)
    for i = 0, year_banner.row.count - 1 do
      if year_banner.row.items[i].row == year_banner.second then
        -- The cart's year boxes share the banner: a box still drawn when the next year opens shows the new year.
        message_box.message_box_add_shared_row(made, score_panel_year_banner_label_row)
      else
        message_box.message_box_add_row(made, year_banner.row.items[i].row)
      end
    end
  end
end

local function score_panel_toggle_visibility()
  local ju = require("game.score_panel").main_score_panel()
  if ju ~= nil then
    ju.is_shown = (ju.has_is_shown ~= 0 and ju.is_shown ~= 0) and 0 or 1
    ju.has_is_shown = 1
  end
end

local function score_panel_toggle_on_click_method(self)
  score_panel_toggle_visibility()
  return 1
end

local function score_panel_toggle_button_on_click_method(self)
  score_panel_toggle_visibility()
end

local function score_panel_update_keep_open_method(self)
  local ju = require("game.score_panel").main_score_panel()
  return (ju ~= nil and ju.has_is_shown ~= 0 and ju.is_shown ~= 0) and 1 or 0
end

local function score_panel_final_score_text(s, i)
  if i == 1 then
    return str.p8_tostr_fixed(s.development)
  elseif i == 2 then
    return str.p8_tostr_fixed(s.population)
  elseif i == 3 then
    return str.p8_tostr_fixed(s.technology)
  elseif i == 4 then
    return economy_score.economy_score_percent_text(nil, s.happiness)
  elseif i == 5 then
    return str.p8_tostr_fixed(s.total)
  elseif i == 6 then
    return str.p8_tostr_fixed(s.tier)
  end
  return str.p8_concat2("", "")
end

local function score_panel_update_add_row(box, entry, context)
  local score_i
  local values = {}
  local has_value = {}
  local keys = layout.layout_row_keys_zero()

  for index = 0, 4 do
    values[index] = data_item({})
    has_value[index] = 0
  end

  if entry.has_score == 0 then
    message_box.message_box_add_row(box, entry.row)
    return
  end

  assert(entry.row.count <= 5,
    "score_panel_update_add_row: a scored row holds at most the five items its line lays out")
  for index = 0, entry.row.count - 1 do
    data_item_assign(values[index], entry.row.items[index])
    has_value[index] = 1
  end

  score_i = entry.score

  values[2] = data_item({ text = score_panel_final_score_text(context.final_score, score_i) })
  has_value[2] = 1

  if score_i == 0 then
    local fm = require("game.probe").main_economy_typed()
    local end_status_str = "current score"
    local dk = economy_score.economy_score_difficulty()
    local px = ""
    local piece1
    local piece2

    if fm ~= nil and fm.has_end_status ~= 0 and fm.end_status ~= nil then
      end_status_str = fm.end_status
    end
    if dk ~= nil and dk.name ~= nil then
      px = dk.name
    end
    piece1 = str.p8_concat2(end_status_str, " (")
    piece2 = str.p8_concat2(piece1, px)
    values[0] = data_item({ text = str.p8_concat2(piece2, ")") })
    has_value[0] = 1
  end

  if score_i == 6 then
    for index = 0, 4 do
      local icon = (index + 1 <= fixed.p8_fixed_flr_int(context.final_score.tier)) and 56 or 57
      values[index] = data_item({ number = icon })
      has_value[index] = 1
    end
  end

  for index = 0, 4 do
    if has_value[index] ~= 0 then
      layout.layout_row_keys_append(keys, layout.layout_row_cell_item(values[index]))
    end
  end
  keys.has_color = entry.row.has_color
  keys.color = fixed.p8_fixed_from_int(entry.row.color)
  keys.has_score = 1
  keys.score = fixed.p8_fixed_from_int(score_i)
  message_box.message_box_add_layout_row(box, keys,
    vec.vec_make(fixed.p8_fixed_from_int(entry.pos_x), fixed.p8_fixed_from_int(entry.pos_y)), entry.has_pos,
    fixed.P8_FIXED_LIT(0x00008000, 0.5))
end

-- The function-local statics of score_panel_update: one row, the same object at every call.
local back_items = { [0] = data_item({ number = 10 }), data_item({ number = 7 }) }
local back = data_row({ count = 2, items = back_items })

function M.score_panel_update(self)
  local score_panel = require("game.score_panel")
  local economy = require("game.probe").main_economy_typed()
  local has_end_status = (economy ~= nil and economy.has_end_status ~= 0 and economy.end_status ~= nil) and 1 or 0
  local has_curtain = (self ~= nil and self.has_curtain ~= 0) and 1 or 0

  if has_end_status ~= 0 and has_curtain == 0 then
    local restart_keys = ui_button.button_keys_zero({})
    local toggle_keys = ui_button.button_keys_zero({})
    local made1
    local made2
    local first_line_pos = vec.vec_make(fixed.p8_fixed_from_int(cint.div(vram.P8_SCREEN_WIDTH, 2) - 13),
      fixed.p8_fixed_from_int(cint.div(vram.P8_SCREEN_HEIGHT, 2) + 36))
    local second_line_pos = vec.vec_make(fixed.p8_fixed_from_int(cint.div(vram.P8_SCREEN_WIDTH, 2) - 25),
      fixed.p8_fixed_from_int(cint.div(vram.P8_SCREEN_HEIGHT, 2) + 36))

    ui_button.modal_backdrop_new(nil)

    layout.layout_row_keys_append(restart_keys.layout_row_keys, layout.layout_row_cell_string(" again? "))
    restart_keys.has_layer = 1
    restart_keys.layer = fixed.p8_fixed_from_int(15)
    restart_keys.has_pos = 1
    restart_keys.pos = first_line_pos
    restart_keys.has_on_click = 1
    restart_keys.on_click = score_panel.score_panel_init_field_value()
    made1 = ui_button.button_new(restart_keys)

    layout.layout_row_keys_append(toggle_keys.layout_row_keys, layout.layout_row_cell_row(back))
    toggle_keys.has_layer = 1
    toggle_keys.layer = fixed.p8_fixed_from_int(15)
    toggle_keys.has_pos = 1
    toggle_keys.pos = second_line_pos
    toggle_keys.has_on_click = 1
    toggle_keys.on_click = score_panel_toggle_button_on_click_method
    made2 = ui_button.button_new(toggle_keys)

    if self ~= nil then
      self.is_shown = 1
      self.has_is_shown = 1
      self.curtain = 0
      self.has_curtain = 1
      entity_registry.entity_registry_set_layer(self, fixed.p8_fixed_from_int(13))
    end
  end

  do
    local is_shown = (self ~= nil and self.has_is_shown ~= 0 and self.is_shown ~= 0) and 1 or 0
    local last_box_equals_shown_message_box = 0

    -- Without the NULL test, NULL == NULL would fire on the first eligible tick.
    if self ~= nil then
      local last_box = score_panel.score_panel_last_box(self)
      last_box_equals_shown_message_box = (last_box ~= nil and message_box.shown_message_box() == last_box) and 1 or 0
    end

    if is_shown ~= 0 and last_box_equals_shown_message_box == 0 then
      local ctx = { final_score = economy_score.economy_score_final() }
      local is_ready = score_panel.score_panel_ready()
      local keys = message_box_keys_zero()
      local made
      local i = 0

      keys.has_duration_scale = 1
      keys.duration_scale = fixed.p8_fixed_from_int(3)
      keys.keep_open = score_panel_update_keep_open_method
      made = message_box.message_box_new(keys)
      while is_ready ~= 0 and i < game_data.SCORE_ROWS_COUNT do
        score_panel_update_add_row(made, game_data.score_rows_table[i], ctx)
        i = i + 1
      end

      if self ~= nil then
        score_panel.score_panel_set_last_box(self, made)
      end
    end
  end
end

function M.score_panel_display_init()
  M.score_panel_display_shutdown()
  g_score_panel_new_year_keep_open = 0
end

function M.score_panel_display_shutdown()
end

function M.score_panel_toggle_on_click()
  return score_panel_toggle_on_click_method
end

function M.score_panel_new_year_method(self, year)
  M.score_panel_new_year(year)
end

function M.score_panel_update_method(self, t)
  M.score_panel_update(self)
end

return M
