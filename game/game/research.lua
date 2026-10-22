-- Copy of src/game/research.h then src/game/research.c.
--
-- Shapes, with the C field names:
--   need_t: { row, has_satisfied, satisfied }, row a data_row_t of map_tables.
--   research_project_t: one table holding the entity header and { archetype, name,
--     planet_codes, base_cost, has_base_cost, production_key, inputs, output, has_output,
--     upgrade_tech, has_upgrade_tech, byproduct, has_byproduct, required_tech, icon_sprite,
--     has_icon_sprite, class_name, next_type }, inputs an inline pointer_list_t of need_t.
--   research_project_id_t: { index, epoch }, both 16-bit, a new table at every creation.
--   research_project_keys_t: { record, required_tech }.
-- g_project_table holds PROJECT_RECORDS_COUNT tables created once at load: a project is always
-- the same table, which project_create zeroes in place, as the C reuses the bytes.
-- research_project_cost and research_project_production read the main economy through
-- tech_gate: they cannot run before game.probe exists.
local bit = require("bit")
local allocator = require("engine.allocator")
local entity_class = require("engine.entity_class")
local game_zone = require("engine.game_zone")
local pointer_list = require("engine.pointer_list")
local vec = require("engine.vec")
local game_data = require("game.game_data")
local map_tables = require("game.map_tables")
local tech_gate = require("game.tech_gate")
local trade = require("game.trade")
local fatal = require("p8.fatal")
local fixed = require("p8.fixed")

local M = {}

M.SIZEOF = {
  need_t = 48,
}
local SIZEOF = M.SIZEOF

local PROJECT_RECORDS_COUNT = map_tables.PROJECT_RECORDS_COUNT

local RESEARCH_PROJECT_ID_NONE = { index = 0, epoch = 0 }
M.RESEARCH_PROJECT_ID_NONE = RESEARCH_PROJECT_ID_NONE

local function research_project_id_none()
  return { index = RESEARCH_PROJECT_ID_NONE.index, epoch = RESEARCH_PROJECT_ID_NONE.epoch }
end

local g_research_project_up = 0

-- Built in place: a project's layout_row items point into the project itself.
local g_project_table = {}
for i = 0, PROJECT_RECORDS_COUNT - 1 do
  g_project_table[i] = { inputs = { items = nil, count = 0, capacity = 0, zone = nil } }
end
local g_project_table_count = 0
local g_project_epoch = 1

-- Lua only: project - g_project_table, the C index of each table.
local project_index_of = {}
for i = 0, PROJECT_RECORDS_COUNT - 1 do
  project_index_of[g_project_table[i]] = i
end

local g_projects = {}
for i = 0, PROJECT_RECORDS_COUNT - 1 do
  g_projects[i] = 0
end
local g_projects_count = 0
-- The name is borrowed, never copied: it must outlive the project.
local g_projects_by_name = {}
for i = 0, PROJECT_RECORDS_COUNT - 1 do
  g_projects_by_name[i] = { name = nil, project = 0 }
end
local g_projects_by_name_count = 0
local g_projects_up = 0
local g_required_tech = nil

function M.research_expire_project_ids()
  g_project_epoch = bit.band(g_project_epoch + 1, 0xFFFF)
  if g_project_epoch == 0 then
    g_project_epoch = 1
  end
end

local function forget_projects()
  g_project_table_count = 0
  g_projects_count = 0
  g_projects_by_name_count = 0
  M.research_expire_project_ids()
end

function M.research_project_get(id)
  if id.epoch ~= g_project_epoch or id.index >= g_project_table_count then
    return nil
  end
  return g_project_table[id.index]
end

function M.research_project_id_of(project)
  local index

  if project == nil then
    return research_project_id_none()
  end
  index = project_index_of[project]
  assert(index ~= nil and index < g_project_table_count and project == g_project_table[index],
    "research_project_id_of: a project lives in the project table, made since the projects were last forgotten")
  return { index = bit.band(index, 0xFFFF), epoch = g_project_epoch }
end

local function projects_register(index)
  local name = g_project_table[index].name

  for i = 0, g_projects_by_name_count - 1 do
    if g_projects_by_name[i].name == name then
      g_projects_by_name[i].project = index
      return
    end
  end
  assert(g_projects_by_name_count < PROJECT_RECORDS_COUNT,
    "projects_register: every name comes from the project records, which have fewer names than records")
  g_projects_by_name[g_projects_by_name_count].name = name
  g_projects_by_name[g_projects_by_name_count].project = index
  g_projects_by_name_count = g_projects_by_name_count + 1
end

M.research_project_class_def = {
  states = {},
  tags = 0,
  lineage = 0,
  skip_draw_off_screen = 0,
}

local function research_project_string(s)
  assert(s ~= nil, "research_project_string: a position that is not a string does not fit the member's type "
    .. "-- stop, do not widen the member")
  return s
end

local function resource_code_of(s)
  local qe = 0
  local found

  assert(s ~= nil, "resource_code_of: a position that is not a number does not fit the member's type "
    .. "-- stop, do not widen the member")
  found, qe = map_tables.resource_code_lookup(s, qe)
  assert(found ~= 0, "resource_code_of: a routed position missing from the resource codes would stay a string in a "
    .. "fixed member -- stop, do not widen the member")
  return fixed.p8_fixed_from_int(qe)
end

local function research_project_positions(self, ri, input_codes)
  local n = ri.npos

  assert(ri.header == nil,
    "research_project_positions: a header entry sets the required_tech of the records after it, it is not a project")
  assert(n <= 8, "research_project_positions: a record has eight positions, name to byproduct")
  if n >= 1 then
    self.name = research_project_string(ri.name)
  end
  if n >= 2 then
    self.planet_codes = research_project_string(ri.planet_codes)
  end
  if n >= 3 then
    self.base_cost = fixed.p8_fixed_from_int(ri.base_cost)
    self.has_base_cost = 1
  end
  if n >= 4 then
    self.production_key = research_project_string(ri.production_key)
  end
  if n >= 5 then
    input_codes = research_project_string(ri.inputs)
  end
  if n >= 6 then
    self.output = resource_code_of(ri.output)
    self.has_output = 1
  end
  if n >= 7 then
    if ri.upgrade_tech_false ~= 0 then
      self.upgrade_tech = nil
    else
      self.upgrade_tech = ri.upgrade_tech
    end
    self.has_upgrade_tech = 1
  end
  if n >= 8 then
    self.byproduct = resource_code_of(ri.byproduct)
    self.has_byproduct = 1
  end
  return input_codes
end

function M.need_resource(i)
  if i.row.has_resource ~= 0 then
    return { kind = trade.RESOURCE_FIXED, number = i.row.resource }
  end
  return { kind = trade.RESOURCE_NIL, number = 0 }
end

function M.need_new(zone, resource)
  local i

  assert(resource.kind ~= trade.RESOURCE_FALSE, "need_new: a need holds a fixed resource or none, never false")
  i = allocator.allocator_alloc(zone, SIZEOF.need_t)
  i.row = map_tables.map_tables_make_data_row({})
  i.has_satisfied = 0
  i.satisfied = 0
  if resource.kind == trade.RESOURCE_FIXED then
    i.row.has_resource = 1
    i.row.resource = resource.number
  end
  return i
end

-- memset(self, 0, sizeof *self) on the same table: the inline inputs table keeps its identity.
local function project_zero(self)
  local inputs = self.inputs

  self.class = nil
  self.state = 0
  self.state_time = 0
  self.has = 0
  self.layer = 0
  self.pos = vec.vec_make(0, 0)
  self.map_pos = vec.vec_make(0, 0)
  self.hit_box = { left = 0, top = 0, right = 0, bottom = 0 }
  self.infraspace = 0
  self.shake = 0
  self.fold_anchor = 0
  self.is_dead = 0
  self.is_hovered = 0
  self.id = { index = 0, generation = 0 }
  self.archetype = nil
  self.name = nil
  self.planet_codes = nil
  self.base_cost = 0
  self.has_base_cost = 0
  self.production_key = nil
  inputs.items = nil
  inputs.count = 0
  inputs.capacity = 0
  inputs.zone = nil
  self.output = 0
  self.has_output = 0
  self.upgrade_tech = nil
  self.has_upgrade_tech = 0
  self.byproduct = 0
  self.has_byproduct = 0
  self.required_tech = nil
  self.icon_sprite = 0
  self.has_icon_sprite = 0
  self.class_name = nil
  self.next_type = nil
end

local function project_create(keys)
  local load_zone = game_zone.game_zone_load_zone()
  local self
  local input_codes = ""
  local index

  if g_project_table_count >= PROJECT_RECORDS_COUNT then
    fatal.p8_fatal("research project table full")
  end
  index = g_project_table_count
  g_project_table_count = g_project_table_count + 1
  self = g_project_table[index]
  project_zero(self)
  entity_class.entity_init_header(self, M.research_project_class_def)
  self.name = nil
  self.planet_codes = nil
  self.base_cost = 0
  self.has_base_cost = 0
  self.production_key = nil
  self.output = 0
  self.has_output = 0
  self.upgrade_tech = nil
  self.has_upgrade_tech = 0
  self.byproduct = 0
  self.has_byproduct = 0
  self.required_tech = nil
  self.icon_sprite = 0
  self.has_icon_sprite = 0
  self.class_name = nil
  self.next_type = nil
  self.archetype = game_data.research_project_archetype

  if keys ~= nil and keys.record ~= nil then
    local ri = keys.record

    input_codes = research_project_positions(self, ri, input_codes)
    if ri.has_icon_sprite ~= 0 then
      self.icon_sprite = fixed.p8_fixed_from_int(ri.icon_sprite)
      self.has_icon_sprite = 1
    end
    self.class_name = ri.class_name
    self.next_type = ri.next_type
  end
  if keys ~= nil then
    self.required_tech = keys.required_tech
  end

  if self.name ~= nil then
    assert(g_projects_up ~= 0,
      "project_create: a named project registers itself, so research_build_projects() runs first")
    projects_register(index)
  end

  if self.has_icon_sprite ~= 0 then
    assert(self.name ~= nil,
      "project_create: the row {name, 32, {icon_sprite, 8}} has no hole; a project with an icon_sprite and no name "
      .. "does not fit the row -- stop, do not widen the member")
  end

  -- A character that is no resource code still makes a need, with no resource.
  if #input_codes == 0 then
    pointer_list.pointer_list_init(self.inputs, load_zone)
  else
    pointer_list.pointer_list_init_reserve(self.inputs, load_zone, #input_codes)
  end
  for input_count = 1, #input_codes do
    local key = input_codes:sub(input_count, input_count)
    local found, r_resource = map_tables.resource_code_lookup(key, 0)

    if found ~= 0 then
      pointer_list.pointer_list_add(self.inputs,
        M.need_new(load_zone, { kind = trade.RESOURCE_FIXED, number = fixed.p8_fixed_from_int(r_resource) }))
    else
      pointer_list.pointer_list_add(self.inputs, M.need_new(load_zone, { kind = trade.RESOURCE_NIL, number = 0 }))
    end
  end
  return self
end

function M.research_project_upgrade_tech(self)
  if self == nil or self.has_upgrade_tech == 0 then
    return nil
  end
  return self.upgrade_tech
end

function M.research_project_assert(self)
  assert(self == nil or self.class == M.research_project_class_def,
    "research_project_assert: every listed project, every planet's project and every row's project is a research "
    .. "project; anything else would be read through the wrong layout")
end

function M.research_init()
  M.research_shutdown()
  g_research_project_up = 1
end

function M.research_shutdown()
  forget_projects()
  g_projects_up = 0
  g_required_tech = nil
  g_research_project_up = 0
end

function M.research_project_ready()
  return g_research_project_up
end

function M.research_project_class()
  return M.research_project_class_def
end

function M.research_project_row_tail()
  return game_data.research_project_archetype.row_tail
end

function M.research_project_new(keys)
  return project_create(keys)
end

function M.research_build_projects()
  forget_projects()
  g_projects_up = 1
  g_required_tech = nil

  for i = 0, PROJECT_RECORDS_COUNT - 1 do
    local e = map_tables.project_records_table[i]

    if e.header ~= nil then
      g_required_tech = e.header
    else
      local keys = { record = e, required_tech = g_required_tech }
      g_projects[g_projects_count] = M.research_project_id_of(M.research_project_new(keys)).index
      g_projects_count = g_projects_count + 1
    end
  end
end

function M.research_projects_count()
  return g_projects_count
end

function M.research_projects_at(k)
  if k < 1 or k > g_projects_count then
    return nil
  end
  return g_project_table[g_projects[k - 1]]
end

function M.research_projects_find(px)
  local i = 0

  while px ~= nil and i < g_projects_by_name_count do
    if g_projects_by_name[i].name == px then
      return g_project_table[g_projects_by_name[i].project]
    end
    i = i + 1
  end
  return nil
end

function M.research_project_production(self)
  local il
  local qp = nil

  M.research_project_assert(self)
  if self == nil then
    return nil
  end
  if self.production_key ~= nil then
    qp = map_tables.production_tables_lookup(self.production_key)
  end

  il = M.research_project_upgrade_tech(self)
  if il ~= nil and tech_gate.tech_gate_is_unlocked(il) ~= 0 and qp ~= nil and qp.upgraded_entry ~= nil then
    local production_tables_upgraded_entry = map_tables.production_tables_lookup(qp.upgraded_entry)

    if production_tables_upgraded_entry ~= nil then
      return production_tables_upgraded_entry
    end
  end
  return qp
end

function M.research_project_cost(self)
  local od = fixed.p8_fixed_from_int(0)
  local il
  local has_icon_sprite

  M.research_project_assert(self)
  il = M.research_project_upgrade_tech(self)
  if self ~= nil and self.has_base_cost ~= 0 then
    od = self.base_cost
  end
  if self ~= nil and il ~= nil and tech_gate.tech_gate_is_unlocked(il) ~= 0 then
    od = fixed.p8_fixed_mul(od, fixed.P8_FIXED_LIT(0x00018000, 1.5))
  end
  has_icon_sprite = self ~= nil and self.has_icon_sprite ~= 0
  if not has_icon_sprite and tech_gate.tech_gate_is_unlocked("nanomaterials") ~= 0 then
    od = fixed.p8_fixed_mul(od, fixed.P8_FIXED_LIT(0x0000d999, 0.85))
  end
  return fixed.p8_fixed_flr(od)
end

return M
