-- Include cycle: game.planet is required at call time (planet_is_family, planet_spacer).
local layout = require("game.layout")
local map_tables = require("game.map_tables")
local research = require("game.research")
local tech_gate = require("game.tech_gate")
local trade = require("game.trade")
local fixed = require("p8.fixed")
local str = require("p8.str")

local M = {}

local function tuple_zero()
  return { count = 0, scalars = {} }
end

local function research_project_tooltip_join(self)
  local i_list
  local names = ""
  local first = true

  i_list = self.inputs
  for index = 1, #i_list do
    local entry = i_list[index]
    local qe = { kind = trade.RESOURCE_NIL, number = 0 }
    local num_str
    local resource_names_name
    local piece = ""

    if entry ~= nil then
      qe = research.need_resource(entry)
    end
    if qe.kind == trade.RESOURCE_FIXED then
      num_str = str.tostr_fixed(qe.number)
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

    if not first then
      names = names .. ","
    end
    names = names .. piece
    first = false
  end
  return names
end

function M.research_project_tooltip(self)
  local jb

  research.research_project_assert(self)
  if self == nil then
    return nil
  end
  if self.icon_sprite ~= nil then
    return nil
  end

  jb = {}
  jb.outer = map_tables.make_data_row({})
  jb.outer_item = map_tables.make_data_item({})
  jb.inner = map_tables.make_data_row({})
  jb.inner_items = {}
  for i = 1, 3 do
    jb.inner_items[i] = map_tables.make_data_item({})
  end
  jb.inner_items[1].text = research_project_tooltip_join(self)
  jb.inner_items[2].number = 13
  jb.inner.count = 2
  jb.inner.items = jb.inner_items

  if self.output ~= nil then
    local o_str = str.tostr_fixed(self.output)
    local resource_names_o = map_tables.resource_names_lookup(o_str)

    if resource_names_o ~= nil then
      jb.inner_items[3].text = resource_names_o
      jb.inner.count = 3
    end
  end

  jb.outer_item.row = jb.inner
  jb.outer.count = 1
  jb.outer.items = { jb.outer_item }
  return jb.outer
end

local function planet_resource_row_pair(first, width)
  local tuple = tuple_zero()

  tuple.count = 2
  tuple.scalars[1] = first
  tuple.scalars[2] = layout.row_scalar_fixed(width)
  return layout.row_cell_tuple(tuple)
end

local tail_row_items = {
  map_tables.make_data_item({ is_false = true }),
  map_tables.make_data_item({ number = -4 }),
}
local tail_row = map_tables.make_data_row({ count = 2, items = tail_row_items })

local function planet_resource_row_keys(inputs, output, amount, used, spacing, out_keys)
  local planet = require("game.planet")
  local keys = layout.row_keys_zero()
  local crowded
  local is_count = (inputs ~= nil) and #inputs or 0
  local outputs = {}
  local outputs_count
  local wide

  if output == nil then
    return false, out_keys
  end

  crowded = (is_count >= 3) and fixed.from_int(1) or fixed.from_int(0)

  do
    local found_w, r_w = map_tables.resource_code_lookup("w", nil)
    local o_is_r_w = found_w and output == fixed.from_int(r_w)

    if o_is_r_w then
      outputs[1] = fixed.from_int(25)
      outputs[2] = fixed.from_int(23)
      outputs_count = 2
    else
      outputs[1] = output
      outputs_count = 1
    end
  end

  wide = fixed.mul(amount, fixed.from_int(outputs_count)) >= fixed.from_int(3)

  if inputs ~= nil then
    for index = 1, #inputs do
      local entry = inputs[index]
      local tuple = tuple_zero()
      local qe = { kind = trade.RESOURCE_NIL, number = 0 }
      local gc_zero = false

      tuple.count = 3
      if entry ~= nil then
        local i = entry

        qe = research.need_resource(i)
        gc_zero = i.satisfied ~= nil and i.satisfied == 0
      end
      if qe.kind == trade.RESOURCE_FIXED then
        tuple.scalars[1] = layout.row_scalar_fixed(qe.number)
      else
        tuple.scalars[1] = layout.row_scalar_nothing()
      end
      tuple.scalars[2] = layout.row_scalar_fixed(
        fixed.sub(fixed.from_int(3), crowded))
      tuple.scalars[3] = layout.row_scalar_fixed(fixed.from_int(gc_zero and 7 or 0))
      layout.row_keys_append(keys, layout.row_cell_tuple(tuple))
    end
  end

  if is_count > 0 then
    local use_35 = spacing == fixed.from_int(1) or wide

    layout.row_keys_append(keys, planet_resource_row_pair(layout.row_scalar_nothing(),
      fixed.add(spacing, crowded)))
    if use_35 then
      layout.row_keys_append(keys, planet_resource_row_pair(
        layout.row_scalar_fixed(fixed.from_int(35)), fixed.add(fixed.from_int(2), spacing)))
    else
      layout.row_keys_append(keys, planet_resource_row_pair(
        layout.row_scalar_fixed(fixed.from_int(32)), fixed.from_int(5)))
    end
  end

  if amount > fixed.from_int(5) then
    local amount_text = str.tostr_fixed(amount)

    layout.row_keys_append(keys, layout.row_cell_string(amount_text))
    layout.row_keys_append(keys, layout.row_cell_row(tail_row))
    keys.color = fixed.from_int(13)
    amount = fixed.from_int(1)
  end

  for output_index = 1, outputs_count do
    local qq = outputs[output_index]
    local loop_bound = (amount > fixed.from_int(1)) and amount or fixed.from_int(1)
    local n = fixed.flr_int(loop_bound)

    for i = 1, n do
      local i_fixed = fixed.from_int(i)
      local within = i_fixed <= amount and i_fixed > used
      local tuple = tuple_zero()

      tuple.count = 4
      tuple.scalars[1] = layout.row_scalar_fixed(qq)
      tuple.scalars[2] = layout.row_scalar_fixed(fixed.from_int(6))
      tuple.scalars[3] = layout.row_scalar_fixed(fixed.from_int(within and 0 or 7))
      tuple.scalars[4] = layout.row_scalar_fixed(fixed.from_int(wide and -4 or -3))
      layout.row_keys_append(keys, layout.row_cell_tuple(tuple))
    end

    layout.row_keys_append(keys, layout.row_cell_row(planet.planet_spacer()))
  end

  out_keys = keys
  return true, out_keys
end

-- Fills `out_keys` and returns true; returns false, keys untouched, when there is no output.
function M.research_display_planet_resource_row_keys(inputs, output, amount, used, spacing, out_keys)
  return planet_resource_row_keys(inputs, output, amount, used, spacing, out_keys)
end

-- Fills `out_keys` and returns true; returns false, keys untouched, when the project has no output.
function M.research_project_layout_row_keys(self, out_keys)
  local jq
  local po
  local found

  research.research_project_assert(self)
  if self == nil or not research.research_project_ready() then
    return false, out_keys
  end

  po = research.research_project_production(self)
  jq = fixed.from_int(1)
  if po ~= nil and po.count >= 1 then
    local first_production = fixed.from_int(po.values[1])
    jq = (first_production > fixed.from_int(1)) and first_production or fixed.from_int(1)
  end

  found, out_keys = planet_resource_row_keys(self.inputs, self.output, jq, fixed.from_int(0),
    fixed.from_int(1), out_keys)
  if not found then
    return false, out_keys
  end

  layout.row_keys_append(out_keys, layout.row_cell_row(research.research_project_row_tail()))
  if self.name ~= nil then
    layout.row_keys_append(out_keys, layout.row_cell_string(self.name))
  end
  return true, out_keys
end

-- Keys of the row {name, 32, {icon_sprite, 8}}, for a project with an icon sprite.
function M.research_project_icon_row_keys(self)
  local keys = layout.row_keys_zero()
  local icon = tuple_zero()

  icon.count = 2
  assert(self.icon_sprite ~= nil,
    "research_project_icon_row_keys: the row {name, 32, {icon_sprite, 8}} is a project with an icon sprite's")
  icon.scalars[1] = layout.row_scalar_fixed(self.icon_sprite)
  icon.scalars[2] = layout.row_scalar_fixed(fixed.from_int(8))
  layout.row_keys_append(keys, layout.row_cell_string(self.name))
  layout.row_keys_append(keys, layout.row_cell_fixed(fixed.from_int(32)))
  layout.row_keys_append(keys, layout.row_cell_tuple(icon))
  return keys
end

local function available_projects_predicate(b, planet_instance)
  local name_str = nil
  local mg = false

  research.research_project_assert(b)

  if not tech_gate.tech_gate_is_unlocked(b.required_tech) then
    return false
  end

  if planet_instance ~= nil then
    local planet = require("game.planet")
    local jo

    assert(planet.planet_is_family(planet_instance),
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
        mg = true
        break
      end
    end
  end
  return mg
end

function M.research_display_available_projects(planet_instance)
  local result = { items = {}, count = 0 }

  result.count = 0
  for k = 1, research.research_projects_count() do
    local b = research.research_projects_at(k)

    if available_projects_predicate(b, planet_instance) then
      local n = result.count + 1
      result.items[n] = research.research_project_id_of(b)
      result.count = n
    end
  end
  return result
end

return M
