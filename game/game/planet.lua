-- Copy of src/game/planet.h then src/game/planet.c.
--
-- Shapes, with the C field names:
--   planet_keys_t: { planet_id, has_planet_id, map_pos, has_map_pos, pos, has_pos }, planet_id a
--     Lua string ("" at its zero).
--   planet_row_signature_t: { output, amount, need_resources, route_count, needs_count, flags,
--     need_flags }, need_resources and need_flags 0-indexed of PLANET_ROW_SIGNATURE_NEEDS.
--   planet_t: one table holding the entity header and { archetype, planet_id, slipways, project,
--     description, needs, planet_type, layout_row, layout_row_signature,
--     targeted_by, production_bonus, has_production_bonus, net_income, has_net_income,
--     incoming_routes, outgoing_routes, development_tier, has_development_tier, production,
--     sprite, palette_block, acceleration_factor, speed, synthesizable_resources, has_,
--     targeting, emitter }. The planet's own `has` (PLANET_HAS_* bits) is `has_`, since the
--     header's `has` holds the ENTITY_HAS_* bits.
--
-- Declared in planet.h, defined elsewhere and ported there: planet_refresh_method,
-- planet_release_row, planet_release_method, planet_accept_drop_method, planet_draw_method,
-- planet_left_press_method, planet_left_press (game.planet_display); planet_attach_slipway_method,
-- planet_offer_trade_method (game.planet_trade); planet_kind_named (game.planet_family).
--
-- Include cycle: game.planet_display and game.planet_trade are required at load, since their
-- methods fill planet_class_def; neither requires this module at load. game.message_box and
-- game.particle are required at call time.
local bit = require("bit")
local collect = require("engine.collect")
local entity = require("engine.entity")
local entity_class = require("engine.entity_class")
local entity_geom = require("engine.entity_geom")
local entity_id = require("engine.entity_id")
local entity_id_list = require("engine.entity_id_list")
local entity_id_type = require("engine.entity_id_type")
local entity_registry = require("engine.entity_registry")
local frame_zone = require("engine.frame_zone")
local pointer_list = require("engine.pointer_list")
local vec = require("engine.vec")
local game_data = require("game.game_data")
local gen = require("game.gen")
local map_tables = require("game.map_tables")
local planet_display = require("game.planet_display")
local planet_trade = require("game.planet_trade")
local research = require("game.research")
local targeting = require("game.targeting")
local fixed = require("p8.fixed")

local M = {}

-- "s" plus the widest structure counter ("-32768"), plus the terminator.
M.PLANET_ID_SIZE = 8

M.PLANET_ROW_SIGNATURE_NEEDS = 4

M.PLANET_HAS_TARGETING = bit.lshift(1, 0)
M.PLANET_HAS_EMITTER = bit.lshift(1, 1)
M.PLANET_HAS_PRODUCTION = bit.lshift(1, 2)
M.PLANET_HAS_NEEDS = bit.lshift(1, 3)
M.PLANET_HAS_INCOMING_ROUTES = bit.lshift(1, 4)
M.PLANET_HAS_OUTGOING_ROUTES = bit.lshift(1, 5)
M.PLANET_HAS_ID = bit.lshift(1, 6)

local function entity_id_none()
  local none = entity_id_type.ENTITY_ID_NONE
  return { index = none.index, generation = none.generation }
end

-- production_t p = {0}.
local function production_zero()
  return { has_resource = 0, resource = 0, amount = 0 }
end

-- trade_route_list_t list = {0}.
local function trade_route_list_zero()
  return { values = { items = nil, element_size = 0, count = 0, capacity = 0, zone = nil } }
end

-- pointer_list_t / entity_id_list_t list = {0}.
local function list_zero()
  return { items = nil, count = 0, capacity = 0, zone = nil }
end

-- A pointer_list_t assigned by value into dst, the inline list of its owner.
local function pointer_list_assign(dst, src)
  dst.items = src.items
  dst.count = src.count
  dst.capacity = src.capacity
  dst.zone = src.zone
end

-- *copy = *input for a need_t: the row is copied, its items shared as the C copy shares the pointer.
local function need_copy_into(copy, input)
  local row = input.row
  copy.row = map_tables.map_tables_make_data_row({
    count = row.count,
    items = row.items,
    has_color = row.has_color,
    color = row.color,
    draw_function = row.draw_function,
    has_resource = row.has_resource,
    resource = row.resource,
  })
  copy.has_satisfied = input.has_satisfied
  copy.satisfied = input.satisfied
end

local g_planet_up = 0

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
  return (bit.band(e.class.lineage, entity_class.LINEAGE(entity_class.LINEAGE_PLANET)) ~= 0) and 1 or 0
end

function M.planet_update_vanishing_method(self, t)
  M.planet_update_vanishing(self, t)
end

M.planet_class_def = {
  draw = planet_display.planet_draw_method,
  skip_draw_off_screen = 1,
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

function M.planet_init_instance(self, instance_class, archetype)
  assert(g_planet_up ~= 0, "planet_init() must run first")
  entity.entity_init_instance(self, instance_class, archetype.state, archetype.state_time)
  self.archetype = archetype
  entity_registry.entity_registry_set_layer(self, self.archetype.layer)
  self.has = bit.bor(self.has, entity_class.ENTITY_HAS_HIT_BOX)
  do
    local hit_box = self.archetype.hit_box
    self.hit_box = { left = hit_box.left, top = hit_box.top, right = hit_box.right, bottom = hit_box.bottom }
  end
  self.has = bit.bor(self.has, entity_class.ENTITY_HAS_SHAKE)
  self.has = bit.bor(self.has, entity_class.ENTITY_HAS_FOLD_ANCHOR)
  self.fold_anchor = 0
  self.has = bit.bor(self.has, entity_class.ENTITY_HAS_IS_DEAD)
  self.shake = self.archetype.shake
  self.planet_id = ""
  self.slipways = list_zero()
  self.project = { index = research.RESEARCH_PROJECT_ID_NONE.index, epoch = research.RESEARCH_PROJECT_ID_NONE.epoch }
  self.description = nil
  self.needs = list_zero()
  self.planet_type = nil
  self.layout_row = nil
  self.layout_row_signature = M.planet_row_signature_zero()
  self.targeted_by = entity_id_none()
  self.production_bonus = 0
  self.has_production_bonus = 0
  self.is_dead = 0
  self.net_income = 0
  self.has_net_income = 0
  self.incoming_routes = trade_route_list_zero()
  self.outgoing_routes = trade_route_list_zero()
  self.development_tier = self.archetype.development_tier
  self.has_development_tier = self.archetype.has_development_tier
  self.has = bit.bor(self.has, entity_class.ENTITY_HAS_IS_HOVERED)
  self.is_hovered = 0
  self.production = production_zero()
  self.synthesizable_resources = self.archetype.synthesizable_resources
  self.has_ = 0
  self.targeting = targeting.targeting_zero()
  self.emitter = require("game.particle").emitter_zero()
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
  assert(#id < M.PLANET_ID_SIZE,
    "planet_keys_set_id: a planet id is \"p\"/\"s\" plus a counter -- stop, measure before widening")
  keys.planet_id = id
  keys.has_planet_id = 1
end

-- nil without PLANET_HAS_ID.
function M.planet_id_or_null(self)
  if bit.band(self.has_, M.PLANET_HAS_ID) ~= 0 then
    return self.planet_id
  end
  return nil
end

-- Copies the keys' id and positions that are given; nil keys copy nothing.
function M.planet_copy_keys(self, keys)
  if keys == nil then
    return
  end
  if keys.has_map_pos ~= 0 then
    entity_class.entity_set_map_pos(self, keys.map_pos)
  end
  if keys.has_pos ~= 0 then
    entity_class.entity_set_pos(self, keys.pos)
  end
  if keys.has_planet_id ~= 0 then
    assert(#keys.planet_id < M.PLANET_ID_SIZE, "planet_copy_keys: set the id with planet_keys_set_id")
    self.planet_id = keys.planet_id
    self.has_ = bit.bor(self.has_, M.PLANET_HAS_ID)
  end
end

function M.planet_init()
  M.planet_shutdown()
  g_planet_up = 1
  g_planet_production_shared.has_resource = 0
  g_planet_production_shared.resource = 0
  g_planet_production_shared.amount = 0
end

function M.planet_shutdown()
  g_planet_up = 0
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

-- Without PLANET_HAS_PRODUCTION, the result is the shared production: a write through it shows on
-- every such planet.
function M.planet_production(self)
  if bit.band(self.has_, M.PLANET_HAS_PRODUCTION) ~= 0 then
    return self.production
  end
  return g_planet_production_shared
end

function M.planet_production_read(self)
  if bit.band(self.has_, M.PLANET_HAS_PRODUCTION) ~= 0 then
    return self.production
  end
  return g_planet_production_shared
end

function M.planet_class()
  return M.planet_class_def
end

function M.planet_new(keys)
  return planet_create(keys)
end

function M.planet_construct(self)
  M.planet_construct_in(self, nil)
end

function M.planet_construct_in(self, zone_)
  if self == nil then
    return
  end

  entity_id_list.entity_id_list_init(self.slipways, zone_)

  if bit.band(self.class.lineage, entity_class.LINEAGE(entity_class.LINEAGE_STRUCTURE)) ~= 0 then
    local target = research.research_projects_at(fixed.p8_fixed_flr_int(self.archetype.project_index))

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
  for i = 0, project.inputs.count - 1 do
    if project.inputs.items[i] ~= nil then
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

  pointer_list.pointer_list_init(jy, nil)
  if n > 0 then
    pointer_list.pointer_list_init_reserve(jy, nil, n)
  end
  if n > 0 then
    for i = 0, research_project_instance.inputs.count - 1 do
      local input = research_project_instance.inputs.items[i]

      if input ~= nil then
        local copy = {}
        need_copy_into(copy, input)
        pointer_list.pointer_list_add(jy, copy)
      end
    end
  end
  pointer_list_assign(self.needs, jy)
  self.has_ = bit.bor(self.has_, M.PLANET_HAS_NEEDS)

  self.production.has_resource = (research_project_instance ~= nil and research_project_instance.has_output ~= 0)
    and 1 or 0
  self.production.resource = (self.production.has_resource ~= 0) and research_project_instance.output or 0
  self.production.amount = fixed.p8_fixed_from_int(0)
  self.has_ = bit.bor(self.has_, M.PLANET_HAS_PRODUCTION)
end

function M.planet_is_colonized(self)
  if self == nil then
    return 0
  end
  if self.archetype.is_planet == 0 then
    return 0
  end
  return (self.state == entity_class.ENTITY_STATE_SETTLED) and 1 or 0
end

function M.planet_restart_shake(self)
  if self == nil then
    return
  end
  self.shake = fixed.P8_FIXED_LIT(0x00014000, 1.25)
end

function M.planet_update_vanishing(self, t)
  if self == nil then
    return
  end
  self.is_dead = (t > fixed.p8_fixed_from_int(60)) and 1 or 0
  assert(self.is_dead == 0 or self.slipways.count == 0,
    "a linked planet never dies: the routes kept in outgoing_routes would name it, and the cart never drops them")
end

function M.planet_roll_type(self)
  local nk = nil
  if self == nil then
    return
  end

  if g_planet_vanish_roll ~= nil then
    local roll = gen.random_walk_next(g_planet_vanish_roll)
    local use_vanishing = 0

    if roll < fixed.P8_FIXED_LIT(0x00007333, 0.45) then
      use_vanishing = (game_data.planet_types_lookup("vanishing") ~= nil) and 1 or 0
    end
    if use_vanishing ~= 0 then
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
  if self.has_net_income == 0 or self.net_income == fixed.p8_fixed_from_int(0) then
    return
  end
  if entity_geom.entity_geom_is_on_screen(self) == 0 then
    return
  end
  do
    local message_box = require("game.message_box")
    local keys = message_box.toast_keys_zero()
    local color = 0
    local text

    keys.source = entity_id.entity_id_link(self)
    text, color = collect.collect_format_signed(frame_zone.frame_zone(), self.net_income, 10, "$", color)
    message_box.toast_keys_set_text(keys, text)
    keys.color = fixed.p8_fixed_from_int(color)
    message_box.toast_new(keys)
  end
end

local function bob_delta(t)
  return fixed.p8_fixed_mul(fixed.p8_fixed_sin(fixed.p8_fixed_mul(t, fixed.P8_FIXED_LIT(0x00000147, 0.005))),
    fixed.P8_FIXED_LIT(0x00000f5c, 0.06))
end

function M.planet_bob(self, t)
  assert(self == nil or M.planet_is_family(self) ~= 0,
    "planet_bob: self is a planet (structure and trading hub settled updates) -- stop")
  if self ~= nil and entity_class.entity_has_pos(self) ~= 0 then
    self.pos.y = fixed.p8_fixed_add(self.pos.y, bob_delta(t))
  end
end

-- The sum of what planet_bob adds at state times 0 up to state_time - 1;
-- wrong once state_time has wrapped past 65535 frames.
function M.planet_bob_total(state_time)
  local frames = bit.rshift(state_time, 16)
  local total = 0
  local t = 0
  for _ = 0, frames - 1 do
    total = fixed.p8_fixed_add(total, bob_delta(t))
    t = fixed.p8_fixed_add(t, fixed.p8_fixed_from_int(1))
  end
  return total
end

-- Lua only: planet_keys_t keys = {0}.
function M.planet_keys_zero()
  return {
    planet_id = "",
    has_planet_id = 0,
    map_pos = vec.vec_make(0, 0),
    has_map_pos = 0,
    pos = vec.vec_make(0, 0),
    has_pos = 0,
  }
end

-- Lua only: planet_row_signature_t signature = {0}.
-- What planet_refresh's row is built from; a planet with more needs than
-- PLANET_ROW_SIGNATURE_NEEDS has no valid signature, so it always rebuilds.
function M.planet_row_signature_zero()
  local need_resources = {}
  local need_flags = {}
  for i = 0, M.PLANET_ROW_SIGNATURE_NEEDS - 1 do
    need_resources[i] = 0
    need_flags[i] = 0
  end
  return {
    output = 0,
    amount = 0,
    need_resources = need_resources,
    route_count = 0,
    needs_count = 0,
    flags = 0,
    need_flags = need_flags,
  }
end

return M
