-- Copy of src/game/research_display.h then src/game/research_display.c.
--
-- Shapes, with the C field names:
--   available_projects_kept_t: { items, count }, items 0-indexed new research_project_id_t tables.
--   research_project_tooltip_t: the table holding { outer, outer_item, inner,
--     inner_items }, data_row_t and data_item_t tables of map_tables, inner_items 0-indexed of 3;
--     each `items` field is the table it points to.
--
-- Include cycle: game.planet is required at call time (planet_is_family, planet_spacer).
--
-- layout_row_tuple_t zeroes are local.
local pointer_list = require("engine.pointer_list")
local layout = require("game.layout")
local map_tables = require("game.map_tables")
local research = require("game.research")
local tech_gate = require("game.tech_gate")
local trade = require("game.trade")
local fixed = require("p8.fixed")
local str = require("p8.str")

local M = {}

-- layout_row_tuple_t tuple = {0}.
local function tuple_zero()
  local scalars = {}
  for i = 0, layout.LAYOUT_ROW_TUPLE_CAP - 1 do
    scalars[i] = layout.layout_row_scalar_nothing()
  end
  return { count = 0, scalars = scalars }
end

local function research_project_tooltip_join(self)
  local i_list
  local names = ""
  local first = 1
  local iter
  local entry

  i_list = self.inputs
  iter = pointer_list.pointer_list_all(i_list)
  while true do
    local more
    local qe = { kind = trade.RESOURCE_NIL, number = 0 }
    local num_str
    local resource_names_name
    local piece = ""

    more, entry = pointer_list.pointer_list_iter_next(iter, entry)
    if more == 0 then
      break
    end
    if entry ~= nil then
      qe = research.need_resource(entry)
    end
    if qe.kind == trade.RESOURCE_FIXED then
      num_str = str.p8_tostr_fixed(qe.number)
    else
      num_str = nil
    end
    if num_str ~= nil then
      resource_names_name = map_tables.resource_names_lookup(num_str)
    else
      resource_names_name = nil
    end
    if resource_names_name ~= nil then
      piece = resource_names_name
    end

    if first == 0 then
      names = names .. ","
    end
    names = names .. piece
    first = 0
  end
  return names
end

function M.research_project_tooltip(self)
  local jb

  research.research_project_assert(self)
  if self == nil then
    return nil
  end
  if self.has_icon_sprite ~= 0 then
    return nil
  end

  jb = {}
  jb.outer = map_tables.map_tables_make_data_row({})
  jb.outer_item = map_tables.map_tables_make_data_item({})
  jb.inner = map_tables.map_tables_make_data_row({})
  jb.inner_items = {}
  for i = 0, 2 do
    jb.inner_items[i] = map_tables.map_tables_make_data_item({})
  end
  jb.inner_items[0].text = research_project_tooltip_join(self)
  jb.inner_items[1].number = 13
  jb.inner.count = 2
  jb.inner.items = jb.inner_items

  if self.has_output ~= 0 then
    local o_str = str.p8_tostr_fixed(self.output)
    local resource_names_o = map_tables.resource_names_lookup(o_str)

    if resource_names_o ~= nil then
      jb.inner_items[2].text = resource_names_o
      jb.inner.count = 3
    end
  end

  jb.outer_item.row = jb.inner
  jb.outer.count = 1
  jb.outer.items = { [0] = jb.outer_item }
  return jb.outer
end

local function planet_resource_row_pair(first, width)
  local tuple = tuple_zero()

  tuple.count = 2
  tuple.scalars[0] = first
  tuple.scalars[1] = layout.layout_row_scalar_fixed(width)
  return layout.layout_row_cell_tuple(tuple)
end

local tail_row_items = {
  [0] = map_tables.map_tables_make_data_item({ is_false = 1 }),
  map_tables.map_tables_make_data_item({ number = -4 }),
}
local tail_row = map_tables.map_tables_make_data_row({ count = 2, items = tail_row_items })

local function planet_resource_row_keys(inputs, has_output, output, amount, used, spacing, out_keys)
  local planet = require("game.planet")
  local l_cap
  local keys = layout.layout_row_keys_zero()
  local crowded
  local is_count = (inputs ~= nil) and inputs.count or 0
  local outputs = {}
  local outputs_count
  local wide

  if has_output == 0 then
    return 0, out_keys
  end

  crowded = (is_count >= 3) and 1 or 0

  do
    local found_w, r_w = map_tables.resource_code_lookup("w", nil)
    local o_is_r_w = found_w ~= 0 and output == fixed.p8_fixed_from_int(r_w)

    if o_is_r_w then
      outputs[0] = fixed.p8_fixed_from_int(25)
      outputs[1] = fixed.p8_fixed_from_int(23)
      outputs_count = 2
    else
      outputs[0] = output
      outputs_count = 1
    end
  end

  wide = (fixed.p8_fixed_mul(amount, fixed.p8_fixed_from_int(outputs_count)) >= fixed.p8_fixed_from_int(3)) and 1 or 0

  do
    local amount_drawn = (amount > fixed.p8_fixed_from_int(5)) and fixed.p8_fixed_from_int(1) or amount
    local loop_bound = (amount_drawn > fixed.p8_fixed_from_int(1)) and amount_drawn or fixed.p8_fixed_from_int(1)

    l_cap = is_count + ((is_count > 0) and 2 or 0) + ((amount > fixed.p8_fixed_from_int(5)) and 2 or 0)
      + outputs_count * (fixed.p8_fixed_flr_int(loop_bound) + 1)
  end

  if inputs ~= nil then
    local it = pointer_list.pointer_list_all(inputs)
    local entry

    while true do
      local more
      local tuple = tuple_zero()
      local qe = { kind = trade.RESOURCE_NIL, number = 0 }
      local gc_zero = 0

      more, entry = pointer_list.pointer_list_iter_next(it, entry)
      if more == 0 then
        break
      end
      tuple.count = 3
      if entry ~= nil then
        local i = entry

        qe = research.need_resource(i)
        gc_zero = (i.has_satisfied ~= 0 and i.satisfied == 0) and 1 or 0
      end
      if qe.kind == trade.RESOURCE_FIXED then
        tuple.scalars[0] = layout.layout_row_scalar_fixed(qe.number)
      else
        tuple.scalars[0] = layout.layout_row_scalar_nothing()
      end
      tuple.scalars[1] = layout.layout_row_scalar_fixed(
        fixed.p8_fixed_sub(fixed.p8_fixed_from_int(3), fixed.p8_fixed_from_int(crowded)))
      tuple.scalars[2] = layout.layout_row_scalar_fixed(fixed.p8_fixed_from_int((gc_zero ~= 0) and 7 or 0))
      layout.layout_row_keys_append(keys, layout.layout_row_cell_tuple(tuple))
    end
  end

  if is_count > 0 then
    local use_35 = (spacing == fixed.p8_fixed_from_int(1) or wide ~= 0) and 1 or 0

    layout.layout_row_keys_append(keys, planet_resource_row_pair(layout.layout_row_scalar_nothing(),
      fixed.p8_fixed_add(spacing, fixed.p8_fixed_from_int(crowded))))
    if use_35 ~= 0 then
      layout.layout_row_keys_append(keys, planet_resource_row_pair(
        layout.layout_row_scalar_fixed(fixed.p8_fixed_from_int(35)), fixed.p8_fixed_add(fixed.p8_fixed_from_int(2), spacing)))
    else
      layout.layout_row_keys_append(keys, planet_resource_row_pair(
        layout.layout_row_scalar_fixed(fixed.p8_fixed_from_int(32)), fixed.p8_fixed_from_int(5)))
    end
  end

  if amount > fixed.p8_fixed_from_int(5) then
    local amount_text = str.p8_tostr_fixed(amount)

    layout.layout_row_keys_append(keys, layout.layout_row_cell_string(amount_text))
    layout.layout_row_keys_append(keys, layout.layout_row_cell_row(tail_row))
    keys.has_color = 1
    keys.color = fixed.p8_fixed_from_int(13)
    amount = fixed.p8_fixed_from_int(1)
  end

  for output_index = 0, outputs_count - 1 do
    local qq = outputs[output_index]
    local loop_bound = (amount > fixed.p8_fixed_from_int(1)) and amount or fixed.p8_fixed_from_int(1)
    local n = fixed.p8_fixed_flr_int(loop_bound)

    for i = 1, n do
      local i_fixed = fixed.p8_fixed_from_int(i)
      local within = (i_fixed <= amount and i_fixed > used) and 1 or 0
      local tuple = tuple_zero()

      tuple.count = 4
      tuple.scalars[0] = layout.layout_row_scalar_fixed(qq)
      tuple.scalars[1] = layout.layout_row_scalar_fixed(fixed.p8_fixed_from_int(6))
      tuple.scalars[2] = layout.layout_row_scalar_fixed(fixed.p8_fixed_from_int((within ~= 0) and 0 or 7))
      tuple.scalars[3] = layout.layout_row_scalar_fixed(fixed.p8_fixed_from_int((wide ~= 0) and -4 or -3))
      layout.layout_row_keys_append(keys, layout.layout_row_cell_tuple(tuple))
    end

    layout.layout_row_keys_append(keys, layout.layout_row_cell_row(planet.planet_spacer()))
  end

  assert(keys.count == l_cap,
    "planet_resource_row_keys: the row is sized at entry from the inputs, the amount and the outputs; a positional more or less "
    .. "is a miscount of that size")
  out_keys = keys
  return 1, out_keys
end

-- Fills `out_keys` and returns 1; returns 0, keys untouched, when there is no output.
function M.research_display_planet_resource_row_keys(inputs, has_output, output, amount, used, spacing, out_keys)
  return planet_resource_row_keys(inputs, has_output, output, amount, used, spacing, out_keys)
end

-- Fills `out_keys` and returns 1; returns 0, keys untouched, when the project has no output.
function M.research_project_layout_row_keys(self, out_keys)
  local jq
  local po
  local found

  research.research_project_assert(self)
  if self == nil or research.research_project_ready() == 0 then
    return 0, out_keys
  end

  po = research.research_project_production(self)
  jq = fixed.p8_fixed_from_int(1)
  if po ~= nil and po.count >= 1 then
    local first_production = fixed.p8_fixed_from_int(po.values[0])
    jq = (first_production > fixed.p8_fixed_from_int(1)) and first_production or fixed.p8_fixed_from_int(1)
  end

  found, out_keys = planet_resource_row_keys(self.inputs, self.has_output, self.output, jq, fixed.p8_fixed_from_int(0),
    fixed.p8_fixed_from_int(1), out_keys)
  if found == 0 then
    return 0, out_keys
  end

  layout.layout_row_keys_append(out_keys, layout.layout_row_cell_row(research.research_project_row_tail()))
  if self.name ~= nil then
    layout.layout_row_keys_append(out_keys, layout.layout_row_cell_string(self.name))
  end
  return 1, out_keys
end

-- Keys of the row {name, 32, {icon_sprite, 8}}, for a project with an icon sprite.
function M.research_project_icon_row_keys(self)
  local keys = layout.layout_row_keys_zero()
  local icon = tuple_zero()

  icon.count = 2
  assert(self.has_icon_sprite ~= 0,
    "research_project_icon_row_keys: the row {name, 32, {icon_sprite, 8}} is a project with an icon sprite's")
  icon.scalars[0] = layout.layout_row_scalar_fixed(self.icon_sprite)
  icon.scalars[1] = layout.layout_row_scalar_fixed(fixed.p8_fixed_from_int(8))
  layout.layout_row_keys_append(keys, layout.layout_row_cell_string(self.name))
  layout.layout_row_keys_append(keys, layout.layout_row_cell_fixed(fixed.p8_fixed_from_int(32)))
  layout.layout_row_keys_append(keys, layout.layout_row_cell_tuple(icon))
  return keys
end

local function available_projects_predicate(b, planet_instance)
  local name_str = nil
  local mg = 0

  research.research_project_assert(b)

  if tech_gate.tech_gate_is_unlocked(b.required_tech) == 0 then
    return 0
  end

  if planet_instance ~= nil then
    local planet = require("game.planet")
    local jo

    assert(planet.planet_is_family(planet_instance) ~= 0,
      "available_projects_predicate: the planet display passes its own planet")
    jo = planet_instance
    if jo.planet_type ~= nil then
      name_str = jo.planet_type.name
    end
  end

  if name_str ~= nil and #name_str == 1 and b.planet_codes ~= nil then
    local n = #b.planet_codes

    for i = 1, n do
      if string.byte(b.planet_codes, i) == string.byte(name_str, 1) then
        mg = 1
        break
      end
    end
  end
  return mg
end

function M.research_display_available_projects(planet_instance)
  local result = { items = {}, count = 0 }

  assert(research.research_projects_count() <= map_tables.PROJECT_RECORDS_COUNT,
    "research_display_available_projects: one project per record at most, so the record count bounds the result")
  result.count = 0
  for k = 1, research.research_projects_count() do
    local b = research.research_projects_at(k)

    if available_projects_predicate(b, planet_instance) ~= 0 then
      result.items[result.count] = research.research_project_id_of(b)
      result.count = result.count + 1
    end
  end
  return result
end

return M
