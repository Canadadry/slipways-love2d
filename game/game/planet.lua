-- Include cycle: game.planet_display and game.planet_trade are required at load, since their
-- methods fill planet_class_def; neither requires this module at load. game.message_box is
-- required at call time.
local bit = require("bit")
local entity = require("engine.entity")
local entity_class = require("engine.entity_class")
local entity_geom = require("engine.entity_geom")
local entity_id = require("engine.entity_id")
local entity_registry = require("engine.entity_registry")
local economy_formulas = require("game.economy_formulas")
local game_data = require("game.game_data")
local gen = require("game.gen")
local map_tables = require("game.map_tables")
local planet_display = require("game.planet_display")
local planet_trade = require("game.planet_trade")
local research = require("game.research")
local fixed = require("p8.fixed")

local M = {}

local function production_zero()
  return { amount = 0 }
end

-- In place: whoever holds dst sees src's elements.
local function needs_refill(dst, src)
  for i = #dst, 1, -1 do
    dst[i] = nil
  end
  for i = 1, #src do
    dst[i] = src[i]
  end
end

-- The row is copied, its items array shared with input's row.
local function need_copy_into(copy, input)
  local row = input.row
  copy.row = map_tables.map_tables_make_data_row({
    count = row.count,
    items = row.items,
    color = row.color,
    draw_function = row.draw_function,
    resource = row.resource,
  })
  copy.satisfied = input.satisfied
end

local g_planet_up = false

local g_type_picker = nil
local g_planet_vanish_roll = nil

-- Shared by every bare planet, written in place, until planet_install_project gives it its own.
local g_planet_production_shared = production_zero()

function M.planet_hit_test_method(self, mouse)
  return entity_geom.entity_geom_contains_cursor(self, mouse)
end

function M.planet_new_year_method(self, year)
  M.planet_new_year(self)
end

function M.planet_is_family(e)
  return bit.band(e.class.lineage, entity_class.LINEAGE(entity_class.LINEAGE_PLANET)) ~= 0
end

function M.planet_update_vanishing_method(self, t)
  M.planet_update_vanishing(self, t)
end

M.planet_class_def = {
  draw = planet_display.planet_draw_method,
  skip_draw_off_screen = true,
  hit_test = M.planet_hit_test_method,
  new_year = M.planet_new_year_method,
  refresh = planet_display.planet_refresh_method,
  release = planet_display.planet_release_method,
  left_press = planet_display.planet_left_press_method,
  accept_drop = planet_display.planet_accept_drop_method,
  attach_slipway = planet_trade.planet_attach_slipway_method,
  offer_trade = planet_trade.planet_offer_trade_method,
  states = { [entity_class.ENTITY_STATE_VANISHING] = M.planet_update_vanishing_method },
  tags = bit.bor(entity_class.TAG(entity_class.TAG_PLANET), entity_class.TAG(entity_class.TAG_NODE)),
  lineage = entity_class.LINEAGE(entity_class.LINEAGE_PLANET),
}

-- planet_id, needs, production_bonus, net_income, incoming_routes, outgoing_routes,
-- development_tier, production, targeting and emitter are nil when the planet has none.
function M.planet_init_instance(self, instance_class, archetype)
  assert(g_planet_up, "planet_init() must run first")
  entity.entity_init_instance(self, instance_class, archetype.state, archetype.state_time)
  self.archetype = archetype
  entity_registry.entity_registry_set_layer(self, self.archetype.layer)
  do
    local hit_box = self.archetype.hit_box
    self.hit_box = { left = hit_box.left, top = hit_box.top, right = hit_box.right, bottom = hit_box.bottom }
  end
  self.fold_anchor = nil
  self.shake = self.archetype.shake
  self.slipways = {}
  self.project = nil
  self.description = nil
  self.planet_type = nil
  self.layout_row = nil
  self.layout_row_signature = M.planet_row_signature_zero()
  self.targeted_by = nil
  self.is_dead = false
  self.development_tier = self.archetype.development_tier
  self.is_hovered = false
  self.synthesizable_resources = self.archetype.synthesizable_resources
end

local function planet_create(keys)
  local self = {}

  M.planet_init_instance(self, M.planet_class_def, game_data.planet_archetype)
  M.planet_copy_keys(self, keys)
  M.planet_construct(self)
  entity.entity_construct(self)
  return self
end

function M.planet_keys_set_id(keys, id)
  keys.planet_id = id
end

function M.planet_id_or_null(self)
  return self.planet_id
end

-- Copies the keys' id and positions that are given; nil keys copy nothing.
function M.planet_copy_keys(self, keys)
  if keys == nil then
    return
  end
  if keys.map_pos ~= nil then
    entity_class.entity_set_map_pos(self, keys.map_pos)
  end
  if keys.pos ~= nil then
    entity_class.entity_set_pos(self, keys.pos)
  end
  if keys.planet_id ~= nil then
    self.planet_id = keys.planet_id
  end
end

function M.planet_init()
  M.planet_shutdown()
  g_planet_up = true
  g_planet_production_shared.resource = nil
  g_planet_production_shared.amount = 0
end

function M.planet_shutdown()
  g_planet_up = false
  g_type_picker = nil
  g_planet_vanish_roll = nil
end

function M.planet_set_type_picker(picker)
  g_type_picker = picker
end

function M.planet_set_vanish_roll(ir)
  g_planet_vanish_roll = ir
end

-- nil until set, and again after planet_shutdown.
function M.planet_type_picker()
  return g_type_picker
end

function M.planet_vanish_roll()
  return g_planet_vanish_roll
end

function M.planet_ready()
  return g_planet_up
end

function M.planet_mood_rows()
  return game_data.planet_mood_rows_rows
end

function M.planet_made_resources()
  return game_data.planet_made_resources_seq
end

function M.planet_ring_colors()
  return game_data.planet_ring_colors_seq
end

function M.planet_spacer()
  return game_data.planet_spacer_row
end

function M.planet_shared_production()
  return g_planet_production_shared
end

-- Without its own production, the result is the shared production: a write through it shows on
-- every such planet.
function M.planet_production(self)
  return self.production or g_planet_production_shared
end

function M.planet_production_read(self)
  return self.production or g_planet_production_shared
end

function M.planet_class()
  return M.planet_class_def
end

function M.planet_new(keys)
  return planet_create(keys)
end

function M.planet_construct(self)
  M.planet_construct_in(self)
end

function M.planet_construct_in(self)
  if self == nil then
    return
  end

  for i = #self.slipways, 1, -1 do
    self.slipways[i] = nil
  end

  if bit.band(self.class.lineage, entity_class.LINEAGE(entity_class.LINEAGE_STRUCTURE)) ~= 0 then
    local target = research.research_projects_at(fixed.flr_int(self.archetype.project_index))

    if target ~= nil then
      M.planet_install_project(self, target)
    end
  end
end

local function planet_project_need_count(project)
  local n = 0

  if project == nil then
    return n
  end
  for i = 1, #project.inputs do
    if project.inputs[i] ~= nil then
      n = n + 1
    end
  end
  return n
end

function M.planet_install_project(self, research_project_instance)
  local jy = {}
  local n

  research.research_project_assert(research_project_instance)
  if self == nil then
    return
  end
  n = planet_project_need_count(research_project_instance)

  self.project = research.research_project_id_of(research_project_instance)
  self.description = nil

  if n > 0 then
    for i = 1, #research_project_instance.inputs do
      local input = research_project_instance.inputs[i]

      if input ~= nil then
        local copy = {}
        need_copy_into(copy, input)
        jy[#jy + 1] = copy
      end
    end
  end
  if self.needs ~= nil then
    needs_refill(self.needs, jy)
  else
    self.needs = jy
  end

  if self.production == nil then
    self.production = production_zero()
  end
  self.production.resource = (research_project_instance ~= nil) and research_project_instance.output or nil
  self.production.amount = fixed.from_int(0)
end

function M.planet_is_colonized(self)
  if self == nil then
    return false
  end
  if not self.archetype.is_planet then
    return false
  end
  return self.state == entity_class.ENTITY_STATE_SETTLED
end

function M.planet_restart_shake(self)
  if self == nil then
    return
  end
  self.shake = fixed.LIT(0x00014000, 1.25)
end

function M.planet_update_vanishing(self, t)
  if self == nil then
    return
  end
  self.is_dead = t > fixed.from_int(60)
  assert(not self.is_dead or #self.slipways == 0,
    "a linked planet never dies: the routes kept in outgoing_routes would name it, and the cart never drops them")
end

function M.planet_roll_type(self)
  local nk = nil
  if self == nil then
    return
  end

  if g_planet_vanish_roll ~= nil then
    local roll = gen.random_walk_next(g_planet_vanish_roll)
    local use_vanishing = false

    if roll < fixed.LIT(0x00007333, 0.45) then
      use_vanishing = game_data.planet_types_lookup("vanishing") ~= nil
    end
    if use_vanishing then
      nk = game_data.planet_types_lookup("vanishing")
    elseif g_type_picker ~= nil then
      nk = gen.weighted_picker_next(g_type_picker)
    end
  end
  self.planet_type = nk
  self.description = (nk ~= nil) and nk.description or nil

  if nk ~= nil and nk.state ~= entity_class.ENTITY_STATE_NONE then
    entity.entity_set_state(self, nk.state)
  else
    entity.entity_set_state(self, entity_class.ENTITY_STATE_KNOWN)
  end
  M.planet_restart_shake(self)
end

function M.planet_new_year(self)
  if self == nil then
    return
  end
  if self.net_income == nil or self.net_income == fixed.from_int(0) then
    return
  end
  if not entity_geom.entity_geom_is_on_screen(self) then
    return
  end
  do
    local message_box = require("game.message_box")
    local keys = message_box.toast_keys_zero()
    local color = 0
    local text

    keys.source = entity_id.entity_id_link(self)
    text, color = economy_formulas.economy_formulas_format_signed(self.net_income, 10, "$", color)
    message_box.toast_keys_set_text(keys, text)
    keys.color = fixed.from_int(color)
    message_box.toast_new(keys)
  end
end

local function bob_delta(t)
  return fixed.mul(fixed.sin(fixed.mul(t, fixed.LIT(0x00000147, 0.005))),
    fixed.LIT(0x00000f5c, 0.06))
end

function M.planet_bob(self, t)
  assert(self == nil or M.planet_is_family(self),
    "planet_bob: self is a planet (structure and trading hub settled updates) -- stop")
  if self ~= nil and self.pos ~= nil then
    self.pos.y = fixed.add(self.pos.y, bob_delta(t))
  end
end

-- The sum of what planet_bob adds at state times 0 up to state_time - 1;
-- wrong once state_time has wrapped past 65535 frames.
function M.planet_bob_total(state_time)
  local frames = bit.rshift(state_time, 16)
  local total = 0
  local t = 0
  for _ = 1, frames do
    total = fixed.add(total, bob_delta(t))
    t = fixed.add(t, fixed.from_int(1))
  end
  return total
end

-- A planet's keys: { planet_id, map_pos, pos }, planet_id a string, each nil when not given.
function M.planet_keys_zero()
  return {}
end

-- What planet_refresh's row is built from; need_resources and need_flags hold one entry per need.
function M.planet_row_signature_zero()
  return {
    output = 0,
    amount = 0,
    need_resources = {},
    route_count = 0,
    needs_count = 0,
    flags = 0,
    need_flags = {},
  }
end

return M
