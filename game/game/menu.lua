-- Copy of src/game/menu.h then src/game/menu.c.
--
-- pause_menu_open_box_t is the entity header plus { has_, fixed_pos, row_step, back_color, has_back_color }: its
-- own `has` (PAUSE_MENU_OPEN_BOX_HAS_* bits) is `has_`, since the header's `has` holds the ENTITY_HAS_* bits.
local bit = require("bit")
local entity_class = require("engine.entity_class")
local entity_id = require("engine.entity_id")
local entity_registry = require("engine.entity_registry")
local vec = require("engine.vec")
local economy_module = require("game.economy")
local game_data = require("game.game_data")
local layout = require("game.layout")
local map_tables = require("game.map_tables")
local message_box = require("game.message_box")
local probe = require("game.probe")
local score_panel = require("game.score_panel")
local score_panel_display = require("game.score_panel_display")
local tech_gate = require("game.tech_gate")
local ui_module = require("game.ui")
local ui_button = require("game.ui_button")
local fixed = require("p8.fixed")
local str = require("p8.str")

local M = {}

local function vec_copy(v)
  return vec.vec_make(v.x, v.y)
end

-- Lua only: modal_backdrop_keys_t keys = {0}, a new table.
local function modal_backdrop_keys_zero()
  return { on_click = nil }
end

local g_menu_up = 0
local g_galaxy_menu_pos = {}
for i = 0, game_data.GALAXY_MENU_COUNT - 1 do
  g_galaxy_menu_pos[i] = vec.vec_make(0, 0)
end

local galaxy_menu_button4_on_click_method

local PAUSE_MENU_OPEN_BOX_HAS_FIXED_POS = bit.lshift(1, 0)
local PAUSE_MENU_OPEN_BOX_HAS_ROW_STEP = bit.lshift(1, 1)

M.pause_menu_open_box_class_def = {
  states = {},
  tags = 0,
  lineage = 0,
  skip_draw_off_screen = 0,
}

M.g_pause_menu_open_box = {}
entity_class.entity_init_header(M.g_pause_menu_open_box, M.pause_menu_open_box_class_def)
M.g_pause_menu_open_box.has_ = 0
M.g_pause_menu_open_box.fixed_pos = vec.vec_make(0, 0)
M.g_pause_menu_open_box.row_step = vec.vec_make(0, 0)
M.g_pause_menu_open_box.back_color = 0
M.g_pause_menu_open_box.has_back_color = 0

local function tech_tree_screen_row_on_click_method(self)
  if self == nil then
    return
  end
  if self.technology ~= nil then
    economy_module.economy_invent(probe.main_economy_typed(), self.technology)
  end
end

local pair_items = { [0] = map_tables.map_tables_make_data_item({ number = 24 }),
  map_tables.map_tables_make_data_item({ number = 5 }) }
local pair = map_tables.map_tables_make_data_row({ count = 2, items = pair_items })

local function tech_tree_screen_step(t, b)
  local research_cost_result
  local research_cost_str

  layout.layout_row_keys_append(b.layout_row_keys, layout.layout_row_cell_fixed(fixed.p8_fixed_from_int(t.icon)))
  layout.layout_row_keys_append(b.layout_row_keys, layout.layout_row_cell_string(t.name))
  b.layout_row_keys.has_color = 1
  b.layout_row_keys.color = fixed.p8_fixed_from_int(6)

  research_cost_result = tech_gate.tech_gate_research_cost(t)
  research_cost_str = str.p8_tostr_fixed(research_cost_result)

  layout.layout_row_keys_append(b.side_tooltip_row, layout.layout_row_cell_row(pair))
  layout.layout_row_keys_append(b.side_tooltip_row, layout.layout_row_cell_string(research_cost_str))
  b.has_side_tooltip = 1
  b.has_side_tooltip_row = 1

  b.has_tooltip = 1
  ui_button.button_keys_set_tooltip_row(b, t.tooltip_rows)

  -- An unaffordable tech gets on_click present but nil, not absent.
  b.has_on_click = 1
  if tech_gate.tech_gate_affordable_flags(t) ~= 0 then
    b.technology = t
    b.on_click = tech_tree_screen_row_on_click_method
  end
end

local function invented_lines(list)
  local n = (list ~= nil) and #list or 0
  local row = {}
  local items = {}

  row.count = 0
  row.items = nil
  row.has_color = 0
  row.color = 0
  row.draw_function = map_tables.DRAW_FUNCTION_NONE
  row.has_resource = 0
  row.resource = 0
  for i = 0, n - 1 do
    items[i] = map_tables.map_tables_make_data_item({})
  end
  for i = 0, n - 1 do
    items[i].text = list[i + 1]
  end
  row.count = n
  row.items = items
  return row
end

function M.menu_show_tech_tree_screen()
  local economy = probe.main_economy_typed()
  local current_era = fixed.p8_fixed_from_int(0)
  local invented_count = 0
  local next_threshold = fixed.p8_fixed_from_int(0)
  local tier_thresholds
  local available_technologies_list
  local buttons
  local row_count = 0
  local layout_block
  local frame = message_box.open_box_frame_zero()
  local found

  if economy ~= nil then
    current_era = economy.era
  end
  if economy ~= nil then
    invented_count = #economy.invented
  end

  found, tier_thresholds = game_data.int_seq_at(game_data.tier_thresholds_table, game_data.TIER_THRESHOLDS_COUNT,
    fixed.p8_fixed_flr_int(current_era) - 1, tier_thresholds)
  if found ~= 0 then
    next_threshold = fixed.p8_fixed_from_int(tier_thresholds)
  end

  -- 1 lists every tech of the era, affordable or not; the galaxy row's refresh must pass 0.
  available_technologies_list = tech_gate.tech_gate_available_technologies(1)
  buttons = {}
  for i = 0, available_technologies_list.count do
    buttons[i] = ui_button.button_keys_zero({})
  end
  for index = 0, available_technologies_list.count - 1 do
    tech_tree_screen_step(available_technologies_list.items[index], buttons[row_count])
    row_count = row_count + 1
  end

  if invented_count > 0 then
    local extra = buttons[row_count]
    row_count = row_count + 1
    local invented_text = str.p8_tostr_fixed(fixed.p8_fixed_from_int(invented_count))
    local label = str.p8_concat2(invented_text, " invented")
    layout.layout_row_keys_append(extra.layout_row_keys, layout.layout_row_cell_fixed(fixed.p8_fixed_from_int(30)))
    layout.layout_row_keys_append(extra.layout_row_keys, layout.layout_row_cell_string(label))
    extra.layout_row_keys.has_color = 1
    extra.layout_row_keys.color = fixed.p8_fixed_from_int(3)
    extra.has_tooltip = 1
    ui_button.button_keys_set_tooltip_row(extra, invented_lines(economy.invented))
  end

  layout_block = score_panel.score_panel_layout_block()
  if layout_block ~= nil then
    local tier_thresholds_era = fixed.p8_fixed_from_int(0)
    local era_str
    local diff1_str
    local diff2_str
    local tmp1
    local tmp2
    local tmp3
    local ep

    found, tier_thresholds = game_data.int_seq_at(game_data.tier_thresholds_table, game_data.TIER_THRESHOLDS_COUNT,
      fixed.p8_fixed_flr_int(current_era), tier_thresholds)
    if found ~= 0 then
      tier_thresholds_era = fixed.p8_fixed_from_int(tier_thresholds)
    end

    era_str = str.p8_tostr_fixed(current_era)
    diff1_str = str.p8_tostr_fixed(
      fixed.p8_fixed_sub(fixed.p8_fixed_from_int(invented_count), next_threshold))
    diff2_str = str.p8_tostr_fixed(fixed.p8_fixed_sub(tier_thresholds_era, next_threshold))

    tmp1 = str.p8_concat2("level ", era_str)
    tmp2 = str.p8_concat2(tmp1, " (")
    tmp3 = str.p8_concat2(tmp2, diff1_str)
    tmp1 = str.p8_concat2(tmp3, "/")
    tmp2 = str.p8_concat2(tmp1, diff2_str)
    ep = str.p8_concat2(tmp2, ")")

    message_box.open_box_frame_set_description(frame, ep)
    if bit.band(layout_block.has_, score_panel.SCORE_PANEL_LAYOUT_BLOCK_HAS_FIXED_POS) ~= 0 then
      frame.has_fixed_pos = 1
      frame.fixed_pos = vec_copy(layout_block.fixed_pos)
    end
    frame.has_back_color = 1
    frame.back_color = layout_block.back_color
    if bit.band(layout_block.has_, score_panel.SCORE_PANEL_LAYOUT_BLOCK_HAS_ROW_STEP) ~= 0 then
      frame.has_row_step = 1
      frame.row_step = vec_copy(layout_block.row_step)
    end
    message_box.message_box_open_box(frame, buttons, row_count)
  end
end

local function galaxy_menu_button1_on_click_method(self)
  local fm = probe.main_economy_typed()
  if fm ~= nil then
    fm.year_ticks = fixed.p8_fixed_from_int(1)
  end
  economy_module.economy_new_year(fm)
end

local function galaxy_menu_button2_on_click_method(self)
  M.menu_show_tech_tree_screen()
end

local function galaxy_menu_button2_refresh_method(self)
  local available_technologies_list
  local available_technologies_count
  local keys = layout.layout_row_keys_zero()

  if self == nil then
    return
  end

  -- Passing 1 here would light the icon whenever any tech of the era is left.
  available_technologies_list = tech_gate.tech_gate_available_technologies(0)
  available_technologies_count = available_technologies_list.count

  layout.layout_row_keys_append(keys,
    layout.layout_row_cell_fixed(fixed.p8_fixed_from_int((available_technologies_count > 0) and 24 or 37)))
  self.layout_row = layout.layout_row_new(keys)
end

local function galaxy_menu_button3_on_click_method(self)
  local ju = score_panel.main_score_panel()
  local keys = modal_backdrop_keys_zero()
  local built
  if ju ~= nil then
    ju.is_shown = 1
    ju.has_is_shown = 1
  end
  keys.on_click = score_panel_display.score_panel_toggle_on_click()
  built = ui_button.modal_backdrop_new(keys)
end

-- The three rows must stay distinct non-nil functions: ui_button draws a button without on_click in color 5
-- instead of 12/13, and pause_menu_shown knows the menu by row 2's identity.
local function pause_menu_row2_on_click_method(self)
end

local function pause_menu_row3_on_click_method(self)
end

local function pause_menu_row4_on_click_method(self)
end

-- Entry 3 is set once galaxy_menu_button4_on_click_method is defined: here it is still nil.
M.galaxy_menu_on_click_table = {
  [0] = galaxy_menu_button1_on_click_method,
  galaxy_menu_button2_on_click_method,
  galaxy_menu_button3_on_click_method,
}
M.galaxy_menu_refresh_table = { [1] = galaxy_menu_button2_refresh_method }

galaxy_menu_button4_on_click_method = function(self)
  local my = {
    [0] = score_panel.score_panel_init_field_value(),
    pause_menu_row2_on_click_method,
    pause_menu_row3_on_click_method,
    pause_menu_row4_on_click_method,
  }
  local bs = {}
  local frame = message_box.open_box_frame_zero()

  if g_menu_up == 0 then
    return
  end
  for i = 0, game_data.PAUSE_MENU_COUNT - 1 do
    local e = game_data.pause_menu_table[i]

    bs[i] = ui_button.button_keys_zero({})
    bs[i].layout_row_keys = layout.layout_row_keys_of_row(e.layout_row)
    bs[i].has_on_click = 1
    bs[i].on_click = my[i]
  end
  frame.has_fixed_pos = (bit.band(M.g_pause_menu_open_box.has_, PAUSE_MENU_OPEN_BOX_HAS_FIXED_POS) ~= 0) and 1 or 0
  frame.fixed_pos = vec_copy(M.g_pause_menu_open_box.fixed_pos)
  frame.has_back_color = M.g_pause_menu_open_box.has_back_color
  frame.back_color = M.g_pause_menu_open_box.back_color
  frame.has_row_step = (bit.band(M.g_pause_menu_open_box.has_, PAUSE_MENU_OPEN_BOX_HAS_ROW_STEP) ~= 0) and 1 or 0
  frame.row_step = vec_copy(M.g_pause_menu_open_box.row_step)
  message_box.message_box_open_box(frame, bs, game_data.PAUSE_MENU_COUNT)
end
M.galaxy_menu_on_click_table[3] = galaxy_menu_button4_on_click_method

function M.galaxy_menu_button(i)
  local e
  local keys = ui_button.button_keys_zero({})

  assert(g_menu_up ~= 0 and i >= 1 and i <= game_data.GALAXY_MENU_COUNT,
    "galaxy_menu_button: the galaxy menu has GALAXY_MENU_COUNT rows, after menu_init()")
  e = game_data.galaxy_menu_table[i - 1]
  keys.layout_row_keys = layout.layout_row_keys_of_row(e.layout_row)
  keys.has_pos = 1
  keys.pos = vec_copy(g_galaxy_menu_pos[i - 1])
  keys.has_on_click = 1
  keys.on_click = M.galaxy_menu_on_click_table[i - 1]
  keys.refresh = M.galaxy_menu_refresh_table[i - 1]
  return ui_button.button_new(keys)
end

function M.galaxy_menu_on_click(i)
  assert(i >= 1 and i <= game_data.GALAXY_MENU_COUNT)
  return M.galaxy_menu_on_click_table[i - 1]
end

-- The live HUD button whose on_click is galaxy_menu_on_click(i); nil when there is none.
function M.galaxy_menu_find_button(i)
  local on_click = M.galaxy_menu_on_click(i)
  local bucket = entity_registry.entities_by_message(entity_registry.ENTITY_MESSAGE_HIT_TEST)
  for k = 1, #bucket do
    local e = bucket[k]
    if e.is_dead == 0 and e.class == ui_button.button_class() and e.on_click == on_click then
      return e
    end
  end
  return nil
end

function M.pause_menu_open_box()
  if g_menu_up ~= 0 then
    return M.g_pause_menu_open_box
  end
  return nil
end

function M.pause_menu_open_box_class()
  return M.pause_menu_open_box_class_def
end

-- 1 while the ui's open box is the pause menu.
function M.pause_menu_shown()
  local ui = ui_module.main_ui()
  if ui == nil then
    return 0
  end
  local ephemerals = ui.ephemerals
  for i = 1, #ephemerals do
    local e = entity_id.entity_id_resolve(ephemerals[i])
    if e.class == ui_button.button_class() and e.on_click == pause_menu_row2_on_click_method then
      return 1
    end
  end
  return 0
end

function M.menu_init()
  M.menu_shutdown()

  for i = 0, game_data.GALAXY_MENU_COUNT - 1 do
    g_galaxy_menu_pos[i] = vec.vec_make(fixed.p8_fixed_from_int(game_data.galaxy_menu_table[i].pos_x),
      fixed.p8_fixed_from_int(game_data.galaxy_menu_table[i].pos_y))
  end
  M.g_pause_menu_open_box.fixed_pos = vec.vec_make(fixed.p8_fixed_from_int(game_data.pause_menu_tail.fixed_pos_x),
    fixed.p8_fixed_from_int(game_data.pause_menu_tail.fixed_pos_y))
  M.g_pause_menu_open_box.row_step = vec.vec_make(fixed.p8_fixed_from_int(game_data.pause_menu_tail.row_step_x),
    fixed.p8_fixed_from_int(game_data.pause_menu_tail.row_step_y))
  M.g_pause_menu_open_box.has_ = bit.bor(M.g_pause_menu_open_box.has_,
    bit.bor(PAUSE_MENU_OPEN_BOX_HAS_FIXED_POS, PAUSE_MENU_OPEN_BOX_HAS_ROW_STEP))
  M.g_pause_menu_open_box.back_color = fixed.p8_fixed_from_int(game_data.pause_menu_tail.back_color)
  M.g_pause_menu_open_box.has_back_color = 1
  g_menu_up = 1
end

function M.menu_shutdown()
  g_menu_up = 0
  M.g_pause_menu_open_box.has_ = bit.band(M.g_pause_menu_open_box.has_,
    bit.band(bit.bnot(bit.bor(PAUSE_MENU_OPEN_BOX_HAS_FIXED_POS, PAUSE_MENU_OPEN_BOX_HAS_ROW_STEP)), 0xFFFF))
  M.g_pause_menu_open_box.back_color = 0
  M.g_pause_menu_open_box.has_back_color = 0
end

return M
