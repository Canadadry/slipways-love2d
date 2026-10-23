-- A project's base_cost, output, upgrade_tech, byproduct and icon_sprite are nil when absent.
-- A project id is a number never reused, nil for none, stale once research_expire_project_ids runs.
-- g_project_table grows when a project is created and is never emptied: the project at an index
-- is always the same table, which project_create zeroes in place.
-- research_project_cost and research_project_production read the main economy through
-- tech_gate: they cannot run before game.probe exists.
local entity_class = require("engine.entity_class")
local game_data = require("game.game_data")
local map_tables = require("game.map_tables")
local tech_gate = require("game.tech_gate")
local trade = require("game.trade")
local fixed = require("p8.fixed")

local M = {}

local PROJECT_RECORDS_COUNT = map_tables.PROJECT_RECORDS_COUNT

local g_research_project_up = false

-- Built in place: a project's layout_row items point into the project itself.
local g_project_table = {}
local g_project_table_count = 0
local g_project_id_counter = 0
local g_project_by_id = {}
local g_project_id_by_project = {}

local project_index_of = {}

local g_projects = {}
local g_projects_count = 0
-- The name is borrowed, never copied: it must outlive the project.
local g_projects_by_name = {}
local g_projects_by_name_count = 0
local g_projects_up = false
local g_required_tech = nil

local function project_id_issue(project)
  g_project_id_counter = g_project_id_counter + 1
  g_project_by_id[g_project_id_counter] = project
  g_project_id_by_project[project] = g_project_id_counter
end

function M.research_expire_project_ids()
  g_project_by_id = {}
  g_project_id_by_project = {}
  for i = 1, g_project_table_count do
    project_id_issue(g_project_table[i])
  end
end

local function forget_projects()
  g_project_table_count = 0
  g_projects_count = 0
  g_projects_by_name_count = 0
  M.research_expire_project_ids()
end

function M.research_project_get(id)
  if id == nil then
    return nil
  end
  return g_project_by_id[id]
end

function M.research_project_id_of(project)
  local id

  if project == nil then
    return nil
  end
  id = g_project_id_by_project[project]
  assert(id ~= nil,
    "research_project_id_of: a project lives in the project table, made since the projects were last forgotten")
  return id
end

local function projects_register(index)
  local name = g_project_table[index].name

  for i = 1, g_projects_by_name_count do
    if g_projects_by_name[i].name == name then
      g_projects_by_name[i].project = index
      return
    end
  end
  local n = g_projects_by_name_count + 1
  g_projects_by_name[n] = { name = name, project = index }
  g_projects_by_name_count = n
end

M.research_project_class_def = {
  states = {},
  tags = 0,
  lineage = 0,
  skip_draw_off_screen = false,
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
  assert(found, "resource_code_of: a routed position missing from the resource codes would stay a string in a "
    .. "fixed member -- stop, do not widen the member")
  return fixed.from_int(qe)
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
    self.base_cost = fixed.from_int(ri.base_cost)
  end
  if n >= 4 then
    self.production_key = research_project_string(ri.production_key)
  end
  if n >= 5 then
    input_codes = research_project_string(ri.inputs)
  end
  if n >= 6 then
    self.output = resource_code_of(ri.output)
  end
  if n >= 7 then
    if ri.upgrade_tech_false then
      self.upgrade_tech = nil
    else
      self.upgrade_tech = ri.upgrade_tech
    end
  end
  if n >= 8 then
    self.byproduct = resource_code_of(ri.byproduct)
  end
  return input_codes
end

function M.need_resource(i)
  if i.row.resource ~= nil then
    return { kind = trade.RESOURCE_FIXED, number = i.row.resource }
  end
  return { kind = trade.RESOURCE_NIL, number = 0 }
end

-- The need's satisfied stays nil until economy_tiers counts it.
function M.need_new(resource)
  local i

  assert(resource.kind ~= trade.RESOURCE_FALSE, "need_new: a need holds a fixed resource or none, never false")
  i = {}
  i.row = map_tables.map_tables_make_data_row({})
  if resource.kind == trade.RESOURCE_FIXED then
    i.row.resource = resource.number
  end
  return i
end

-- Resets every field of self in place: the inputs table keeps its identity.
local function project_zero(self)
  local inputs = self.inputs

  self.class = nil
  self.state = 0
  self.state_time = 0
  self.is_dead = false
  self.id = nil
  self.archetype = nil
  self.name = nil
  self.planet_codes = nil
  self.base_cost = nil
  self.production_key = nil
  for i = #inputs, 1, -1 do
    inputs[i] = nil
  end
  self.output = nil
  self.upgrade_tech = nil
  self.byproduct = nil
  self.required_tech = nil
  self.icon_sprite = nil
  self.class_name = nil
  self.next_type = nil
end

local function project_create(keys)
  local self
  local input_codes = ""
  local index

  index = g_project_table_count + 1
  g_project_table_count = index
  self = g_project_table[index]
  if self == nil then
    self = { inputs = {} }
    g_project_table[index] = self
    project_index_of[self] = index
  end
  project_id_issue(self)
  project_zero(self)
  entity_class.init_header(self, M.research_project_class_def)
  self.name = nil
  self.planet_codes = nil
  self.base_cost = nil
  self.production_key = nil
  self.upgrade_tech = nil
  self.byproduct = nil
  self.required_tech = nil
  self.icon_sprite = nil
  self.class_name = nil
  self.next_type = nil
  self.archetype = game_data.research_project_archetype

  if keys ~= nil and keys.record ~= nil then
    local ri = keys.record

    input_codes = research_project_positions(self, ri, input_codes)
    if ri.icon_sprite ~= nil then
      self.icon_sprite = fixed.from_int(ri.icon_sprite)
    end
    self.class_name = ri.class_name
    self.next_type = ri.next_type
  end
  if keys ~= nil then
    self.required_tech = keys.required_tech
  end

  if self.name ~= nil then
    assert(g_projects_up,
      "project_create: a named project registers itself, so research_build_projects() runs first")
    projects_register(index)
  end

  if self.icon_sprite ~= nil then
    assert(self.name ~= nil,
      "project_create: the row {name, 32, {icon_sprite, 8}} has no hole; a project with an icon_sprite and no name "
      .. "does not fit the row -- stop, do not widen the member")
  end

  -- A character that is no resource code still makes a need, with no resource.
  for input_count = 1, #input_codes do
    local key = input_codes:sub(input_count, input_count)
    local found, r_resource = map_tables.resource_code_lookup(key, 0)

    if found then
      self.inputs[#self.inputs + 1] =
        M.need_new({ kind = trade.RESOURCE_FIXED, number = fixed.from_int(r_resource) })
    else
      self.inputs[#self.inputs + 1] = M.need_new({ kind = trade.RESOURCE_NIL, number = 0 })
    end
  end
  return self
end

function M.research_project_upgrade_tech(self)
  if self == nil then
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
  g_research_project_up = true
end

function M.research_shutdown()
  forget_projects()
  g_projects_up = false
  g_required_tech = nil
  g_research_project_up = false
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
  g_projects_up = true
  g_required_tech = nil

  for i = 1, PROJECT_RECORDS_COUNT do
    local e = map_tables.project_records_table[i]

    if e.header ~= nil then
      g_required_tech = e.header
    else
      local keys = { record = e, required_tech = g_required_tech }
      local n = g_projects_count + 1
      g_projects[n] = project_index_of[M.research_project_new(keys)]
      g_projects_count = n
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
  return g_project_table[g_projects[k]]
end

function M.research_projects_find(px)
  local i = 1

  while px ~= nil and i <= g_projects_by_name_count do
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
  if il ~= nil and tech_gate.tech_gate_is_unlocked(il) and qp ~= nil and qp.upgraded_entry ~= nil then
    local production_tables_upgraded_entry = map_tables.production_tables_lookup(qp.upgraded_entry)

    if production_tables_upgraded_entry ~= nil then
      return production_tables_upgraded_entry
    end
  end
  return qp
end

function M.research_project_cost(self)
  local od = fixed.from_int(0)
  local il

  M.research_project_assert(self)
  il = M.research_project_upgrade_tech(self)
  if self ~= nil and self.base_cost ~= nil then
    od = self.base_cost
  end
  if self ~= nil and il ~= nil and tech_gate.tech_gate_is_unlocked(il) then
    od = fixed.mul(od, fixed.LIT(0x00018000, 1.5))
  end
  if (self == nil or self.icon_sprite == nil) and tech_gate.tech_gate_is_unlocked("nanomaterials") then
    od = fixed.mul(od, fixed.LIT(0x0000d999, 0.85))
  end
  return fixed.flr(od)
end

return M
