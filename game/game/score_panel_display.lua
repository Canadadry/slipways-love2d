-- Also defines score_panel_display.new_year_method, score_panel_display.update_method, score_panel_display.init and
-- score_panel_display.shutdown for game.score_panel.
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

local data_item = map_tables.make_data_item
local data_row = map_tables.make_data_row

-- Every field of the data item src written into dst.
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
  return { duration_scale = nil, keep_open = nil }
end

local g_score_panel_new_year_keep_open = 0

local function score_panel_new_year_keep_open_method(self)
  g_score_panel_new_year_keep_open = g_score_panel_new_year_keep_open - 1
  return g_score_panel_new_year_keep_open > 0
end

local function score_panel_year_banner_label_row()
  local year_banner = require("game.score_panel").year_banner()

  if year_banner ~= nil then
    return year_banner.second
  end
  return nil
end

function M.new_year(year)
  local score_panel = require("game.score_panel")
  local year_banner = score_panel.year_banner()
  local countdown_rows = score_panel.countdown_rows()
  local message = nil
  local year_str
  local countdown_index

  if year_banner == nil then
    return
  end

  countdown_index = 3426 - fixed.flr_int(year)
  if countdown_index >= 1 and countdown_index <= countdown_rows.count then
    local item = countdown_rows.items[countdown_index]

    assert(item.row ~= nil or item.is_false, "score_panel_display.new_year: the countdown rows hold rows and false only")
    message = item.row
  end

  year_str = str.tostr_fixed(year)
  year_banner.year = "year " .. year_str

  data_item_assign(year_banner.second_items[1], data_item({ text = year_banner.year }))

  if message ~= nil then
    -- Past the copied banner row's two items: created here, never written into a missing slot.
    year_banner.items[3] = data_item({ row = message })
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
    made = message_box.new(keys)
    for i = 1, year_banner.row.count do
      if year_banner.row.items[i].row == year_banner.second then
        -- The cart's year boxes share the banner: a box still drawn when the next year opens shows the new year.
        message_box.add_shared_row(made, score_panel_year_banner_label_row)
      else
        message_box.add_row(made, year_banner.row.items[i].row)
      end
    end
  end
end

local function score_panel_toggle_visibility()
  local ju = require("game.score_panel").main_score_panel()
  if ju ~= nil then
    ju.is_shown = not ju.is_shown
  end
end

local function score_panel_toggle_on_click_method(self)
  score_panel_toggle_visibility()
  return true
end

local function score_panel_toggle_button_on_click_method(self)
  score_panel_toggle_visibility()
end

local function score_panel_update_keep_open_method(self)
  local ju = require("game.score_panel").main_score_panel()
  return ju ~= nil and (ju.is_shown or false)
end

local function score_panel_final_score_text(s, i)
  if i == 1 then
    return str.tostr_fixed(s.development)
  elseif i == 2 then
    return str.tostr_fixed(s.population)
  elseif i == 3 then
    return str.tostr_fixed(s.technology)
  elseif i == 4 then
    return economy_score.percent_text(s.happiness)
  elseif i == 5 then
    return str.tostr_fixed(s.total)
  elseif i == 6 then
    return str.tostr_fixed(s.tier)
  end
  return str.concat2("", "")
end

local function score_panel_update_add_row(box, entry, context)
  local score_i
  local values = {}
  local keys = layout.row_keys_zero()

  if entry.score == nil then
    message_box.add_row(box, entry.row)
    return
  end

  for index = 1, entry.row.count do
    values[index] = data_item({})
    data_item_assign(values[index], entry.row.items[index])
  end

  score_i = entry.score

  values[3] = data_item({ text = score_panel_final_score_text(context.final_score, score_i) })

  if score_i == 0 then
    local fm = require("game.probe").main_economy_typed()
    local end_status_str = "current score"
    local dk = economy_score.difficulty()
    local px = ""
    local piece1
    local piece2

    if fm ~= nil and fm.end_status ~= nil then
      end_status_str = fm.end_status
    end
    if dk ~= nil and dk.name ~= nil then
      px = dk.name
    end
    piece1 = str.concat2(end_status_str, " (")
    piece2 = str.concat2(piece1, px)
    values[1] = data_item({ text = str.concat2(piece2, ")") })
  end

  if score_i == 6 then
    for index = 1, 5 do
      local icon = (index <= fixed.flr_int(context.final_score.tier)) and 56 or 57
      values[index] = data_item({ number = icon })
    end
  end

  for index = 1, 5 do
    if values[index] ~= nil then
      layout.row_keys_append(keys, layout.row_cell_item(values[index]))
    end
  end
  if entry.row.color ~= nil then
    keys.color = fixed.from_int(entry.row.color)
  end
  keys.score = fixed.from_int(score_i)
  message_box.add_layout_row(box, keys,
    entry.pos_x and vec.make(fixed.from_int(entry.pos_x), fixed.from_int(entry.pos_y)),
    fixed.LIT(0x00008000, 0.5))
end

-- The function-local statics of score_panel_display.update: one row, the same object at every call.
local back_items = { data_item({ number = 10 }), data_item({ number = 7 }) }
local back = data_row({ count = 2, items = back_items })

function M.update(self)
  local score_panel = require("game.score_panel")
  local economy = require("game.probe").main_economy_typed()

  if economy ~= nil and economy.end_status ~= nil and (self == nil or self.curtain == nil) then
    local restart_keys = ui_button.button_keys_zero({})
    local toggle_keys = ui_button.button_keys_zero({})
    local made1
    local made2
    local first_line_pos = vec.make(fixed.from_int(cint.div(vram.SCREEN_WIDTH, 2) - 13),
      fixed.from_int(cint.div(vram.SCREEN_HEIGHT, 2) + 36))
    local second_line_pos = vec.make(fixed.from_int(cint.div(vram.SCREEN_WIDTH, 2) - 25),
      fixed.from_int(cint.div(vram.SCREEN_HEIGHT, 2) + 36))

    ui_button.modal_backdrop_new(nil)

    layout.row_keys_append(restart_keys.layout_row_keys, layout.row_cell_string(" again? "))
    restart_keys.layer = fixed.from_int(15)
    restart_keys.pos = first_line_pos
    restart_keys.on_click = score_panel.init_field_value()
    made1 = ui_button.button_new(restart_keys)

    layout.row_keys_append(toggle_keys.layout_row_keys, layout.row_cell_row(back))
    toggle_keys.layer = fixed.from_int(15)
    toggle_keys.pos = second_line_pos
    toggle_keys.on_click = score_panel_toggle_button_on_click_method
    made2 = ui_button.button_new(toggle_keys)

    if self ~= nil then
      self.is_shown = true
      self.curtain = 0
      entity_registry.set_layer(self, fixed.from_int(13))
    end
  end

  do
    local is_shown = self ~= nil and (self.is_shown or false)
    local last_box_equals_shown_message_box = false

    -- Without the nil test, nil == nil would fire on the first eligible tick.
    if self ~= nil then
      local last_box = score_panel.last_box(self)
      last_box_equals_shown_message_box = last_box ~= nil and message_box.shown_message_box() == last_box
    end

    if is_shown and not last_box_equals_shown_message_box then
      local ctx = { final_score = economy_score.final() }
      local is_ready = score_panel.ready()
      local keys = message_box_keys_zero()
      local made
      local i = 1

      keys.duration_scale = fixed.from_int(3)
      keys.keep_open = score_panel_update_keep_open_method
      made = message_box.new(keys)
      while is_ready and i <= game_data.SCORE_ROWS_COUNT do
        score_panel_update_add_row(made, game_data.score_rows_table[i], ctx)
        i = i + 1
      end

      if self ~= nil then
        score_panel.set_last_box(self, made)
      end
    end
  end
end

function M.init()
  M.shutdown()
  g_score_panel_new_year_keep_open = 0
end

function M.shutdown()
end

function M.toggle_on_click()
  return score_panel_toggle_on_click_method
end

function M.new_year_method(self, year)
  M.new_year(year)
end

function M.update_method(self, t)
  M.update(self)
end

return M
