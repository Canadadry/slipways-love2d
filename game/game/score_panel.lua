-- score_panel_new_year_method, score_panel_update_method, score_panel_display_init and
-- score_panel_display_shutdown are defined in game.score_panel_display, required at load: the class holds
-- the two methods by identity.
local dispatch_context = require("engine.dispatch_context")
local entity = require("engine.entity")
local entity_class = require("engine.entity_class")
local entity_id = require("engine.entity_id")
local entity_registry = require("engine.entity_registry")
local vec = require("engine.vec")
local draw_functions = require("game.draw_functions")
local economy_formulas = require("game.economy_formulas")
local game_data = require("game.game_data")
local hud = require("game.hud")
local map_tables = require("game.map_tables")
local score_panel_display = require("game.score_panel_display")
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

local g_score_panel_up = false
local g_main_score_panel = nil

local function fixed_min(a, b)
  return (a < b) and a or b
end

local g_score_panel_readouts = {
  row = data_row({}),
  items = {},
  subs = {},
  sub_items = {},
  texts = {},
}
for i = 1, 7 do
  g_score_panel_readouts.subs[i] = data_row({})
  -- [1] is written by score_panel_refresh whatever the copied row's count: it must hold a table.
  g_score_panel_readouts.sub_items[i] = { data_item({}) }
  g_score_panel_readouts.texts[i] = ""
end

local g_score_panel_year_banner = {
  row = data_row({}),
  items = {},
  second = data_row({}),
  -- [1] is written by score_panel_new_year whatever the copied row's count: it must hold a table.
  second_items = { data_item({}) },
  year = "",
  keep_open = nil,
}

local function score_panel_row_copy(dst, items, src)
  for i = 1, src.count do
    items[i] = data_item({})
    data_item_assign(items[i], src.items[i])
  end
  dst.count = src.count
  dst.items = src.items
  dst.color = src.color
  dst.draw_function = src.draw_function
  dst.resource = src.resource
  dst.items = items
end

M.score_panel_layout_block_class_def = {
  states = {},
  tags = 0,
  lineage = 0,
  skip_draw_off_screen = false,
}

M.g_score_panel_layout_block = {}
entity_class.init_header(M.g_score_panel_layout_block, M.score_panel_layout_block_class_def)
M.g_score_panel_layout_block.fixed_pos = vec.make(0x0, 0xb0000)
M.g_score_panel_layout_block.row_step = vec.make(0x0, 0xb0000)
M.g_score_panel_layout_block.back_color = 0xa0000

function M.score_panel_readouts()
  if g_score_panel_up then
    return g_score_panel_readouts
  end
  return nil
end

function M.score_panel_year_banner()
  if g_score_panel_up then
    return g_score_panel_year_banner
  end
  return nil
end

function M.score_panel_layout_block()
  if g_score_panel_up then
    return M.g_score_panel_layout_block
  end
  return nil
end

function M.score_panel_layout_block_class()
  return M.score_panel_layout_block_class_def
end

function M.score_panel_frame_rows()
  return game_data.score_panel_archetype.frame_rows
end

function M.score_panel_countdown_rows()
  return game_data.score_panel_archetype.countdown_rows
end

local function score_panel_init_placeholder(self)
end

local g_init_method = score_panel_init_placeholder

function M.score_panel_init_field_value()
  return g_init_method
end

function M.score_panel_set_init(fn)
  if fn ~= nil then
    g_init_method = fn
  else
    g_init_method = score_panel_init_placeholder
  end
end

function M.score_panel_refresh()
  local probe = require("game.probe")
  local economy = probe.main_economy_typed()
  local economy_placeholder = probe.main_economy_placeholder()
  local main_economy_income = (economy ~= nil) and economy.income or fixed.from_int(0)
  local main_economy_credits = fixed.from_int(0)
  local main_economy_research_points = data_item({ is_nil = true })
  local main_economy_happiness = (economy ~= nil and economy.happiness ~= nil) and economy.happiness
    or fixed.from_int(0)
  local main_economy_year_ticks = (economy ~= nil) and economy.year_ticks or fixed.from_int(0)
  local main_economy_year
  local main_economy_production = nil
  local readouts_state
  local readouts = {}
  local text
  local credits_str
  local happiness_str
  local production_text
  local income_color = 0

  if economy ~= nil then
    main_economy_year = data_item({ is_fixed = true, fixed = economy.year })
  else
    main_economy_year = data_item({ is_nil = true })
  end
  if economy ~= nil then
    main_economy_production = economy.production
  end

  if economy ~= nil then
    main_economy_credits = economy.credits
    main_economy_research_points = data_item({ is_fixed = true, fixed = economy.research_points })
  elseif economy_placeholder ~= nil then
    if economy_placeholder.credits ~= nil then
      main_economy_credits = economy_placeholder.credits
    end
    if economy_placeholder.research_points ~= nil then
      main_economy_research_points = data_item({ is_fixed = true, fixed = economy_placeholder.research_points })
    end
  end

  text, income_color = economy_formulas.format_signed(main_economy_income, 0, nil, income_color)

  credits_str = str.tostr_fixed(main_economy_credits)
  readouts[1] = data_item({ text = str.concat2(credits_str, "$") })

  readouts[2] = data_item({ text = text })

  readouts[3] = main_economy_research_points

  do
    local found
    local resource_index
    local production_val = fixed.from_int(0)
    found, resource_index = map_tables.resource_code_lookup("k", resource_index)
    if found then
      found, production_val = economy_formulas.resource_map_get(main_economy_production,
        fixed.from_int(resource_index), production_val)
    end
    production_text =
      str.tostr_fixed(fixed.add(production_val, fixed.from_int(1)))
    readouts[4] = data_item({ text = str.concat2("+", production_text) })
  end

  happiness_str = str.tostr_fixed(main_economy_happiness)
  readouts[5] = data_item({ text = str.concat2(happiness_str, "%") })

  do
    local lo = fixed.flr_int(main_economy_year_ticks)
    if lo >= 1 and lo <= game_data.PERIOD_NAMES_COUNT then
      readouts[6] = data_item({ text = game_data.period_names_table[lo] })
    else
      readouts[6] = data_item({ is_nil = true })
    end
  end

  readouts[7] = main_economy_year

  readouts_state = M.score_panel_readouts()
  if readouts_state ~= nil then
    for index = 1, 7 do
      if readouts[index].text ~= nil then
        readouts_state.texts[index] = readouts[index].text
        readouts[index].text = readouts_state.texts[index]
      end
      data_item_assign(readouts_state.sub_items[index][1], readouts[index])
    end
  end
end

local function score_panel_refresh_method(self)
  M.score_panel_refresh()
end

function M.score_panel_draw(gfx, mem, self)
  local i_ = M.score_panel_frame_rows()
  local readouts_state = M.score_panel_readouts()
  local combined = {}
  local n = 0
  local i

  for k = 1, i_.count do
    n = n + 1
    combined[n] = i_.items[k].row
  end
  i = 1
  while readouts_state ~= nil and i <= readouts_state.row.count do
    n = n + 1
    combined[n] = readouts_state.row.items[i].row
    i = i + 1
  end

  draw_functions.draw_rows_default(gfx, mem, combined, n)

  if self ~= nil and self.curtain ~= nil then
    local gz = fixed.add(self.curtain, fixed.LIT(0x00003333, 0.2))
    local capped = fixed_min(gz, fixed.LIT(0x00033333, 3.2))
    local h = fixed.mul(capped, capped)

    self.curtain = gz
    hud.draw_bar(gfx, mem, fixed.from_int(0), h, 0)
    -- No colour on purpose: the bottom bar inherits the pen colour the line above leaves.
    hud.draw_bar_default(gfx, mem, fixed.sub(fixed.from_int(vram.SCREEN_HEIGHT - 1), h),
      fixed.from_int(vram.SCREEN_HEIGHT - 1))
  end
end

local function score_panel_draw_method(self, screen_pos)
  M.score_panel_draw(dispatch_context.gfx(), dispatch_context.mem(), self)
end

M.score_panel_class_def = {
  draw = score_panel_draw_method,
  new_year = score_panel_display.score_panel_new_year_method,
  refresh = score_panel_refresh_method,
  states = { [entity_class.STATE_NORMAL] = score_panel_display.score_panel_update_method },
  tags = 0,
  lineage = 0,
  skip_draw_off_screen = false,
}

-- is_shown and curtain stay nil until first set.
local function score_panel_create()
  local self

  assert(g_score_panel_up, "score_panel_init() must run first")
  self = {}
  entity.init_instance(self, M.score_panel_class_def, game_data.score_panel_archetype.state,
    game_data.score_panel_archetype.state_time)
  self.archetype = game_data.score_panel_archetype
  entity_registry.set_layer(self, self.archetype.layer)
  self.last_box = nil
  entity.construct(self)
  return self
end

function M.score_panel_init()
  M.score_panel_shutdown()

  g_score_panel_up = true
  score_panel_row_copy(g_score_panel_readouts.row, g_score_panel_readouts.items, game_data.score_panel_readouts_row)
  for i = 1, g_score_panel_readouts.row.count do
    score_panel_row_copy(g_score_panel_readouts.subs[i], g_score_panel_readouts.sub_items[i],
      game_data.score_panel_readouts_row.items[i].row)
    g_score_panel_readouts.items[i].row = g_score_panel_readouts.subs[i]
  end
  score_panel_row_copy(g_score_panel_year_banner.row, g_score_panel_year_banner.items,
    game_data.score_panel_year_banner_row)
  score_panel_row_copy(g_score_panel_year_banner.second, g_score_panel_year_banner.second_items,
    game_data.score_panel_year_banner_row.items[2].row)
  g_score_panel_year_banner.items[2].row = g_score_panel_year_banner.second
  g_score_panel_year_banner.keep_open = nil

  M.score_panel_set_init(nil)
  g_main_score_panel = nil

  score_panel_display.score_panel_display_init()
end

function M.score_panel_shutdown()
  score_panel_display.score_panel_display_shutdown()
  g_score_panel_up = false
  M.score_panel_set_init(nil)
  g_main_score_panel = nil
end

function M.score_panel_ready()
  return g_score_panel_up
end

function M.score_panel_class()
  return M.score_panel_class_def
end

function M.score_panel_new()
  return score_panel_create()
end

function M.score_panel_last_box(self)
  return entity_id.get(self.last_box)
end

function M.score_panel_set_last_box(self, box)
  self.last_box = entity_id.link(box)
end

function M.main_score_panel()
  local e = entity_id.get(g_main_score_panel)

  assert(e == nil or e.class == M.score_panel_class_def, "main_score_panel: only a score panel is ever stored")
  return e
end

function M.score_panel_set_main(instance)
  g_main_score_panel = entity_id.link(instance)
end

return M
