-- Copy of src/game/planet_family_trade.h then src/game/planet_family_trade.c.
--
-- Shapes, with the C field names:
--   int_seq_t: { count, items }, items a 0-indexed array or nil.
--   entity_kind_t: { new_entity, record }, record the game_data kind record table.
--   down_t: { stop, held }.
--   slipgate_left_press_empty_t: the entity header only, one table.
--
-- A planet is one table with its entity header: the planet's own `has` (PLANET_HAS_* bits) is
-- `has_`, since the header's `has` holds the ENTITY_HAS_* bits.
--
-- Include cycle: game.planet, game.planet_display, game.planet_trade, game.planet_family and
-- game.probe are required at load, since their methods fill the class tables by identity.
-- game.slipway, game.particle and game.trade are required at call time.
local bit = require("bit")
local allocator = require("engine.allocator")
local entity = require("engine.entity")
local entity_class = require("engine.entity_class")
local entity_id = require("engine.entity_id")
local entity_id_list = require("engine.entity_id_list")
local entity_id_type = require("engine.entity_id_type")
local frame_zone = require("engine.frame_zone")
local pointer_list = require("engine.pointer_list")
local game_data = require("game.game_data")
local grab_arena = require("game.grab_arena")
local planet = require("game.planet")
local planet_display = require("game.planet_display")
local planet_family = require("game.planet_family")
local planet_trade = require("game.planet_trade")
local probe = require("game.probe")
local fixed = require("p8.fixed")

local M = {}

M.SIZEOF = {
  planet_t = 464,
}
local SIZEOF = M.SIZEOF

local function entity_id_none()
  local none = entity_id_type.ENTITY_ID_NONE
  return { index = none.index, generation = none.generation }
end

local function down_zero()
  return { stop = 0, held = entity_id_none() }
end

-- A trade_route_list_t assigned by value: a new header sharing the items, as the C copy shares
-- its buffer.
local function trade_route_list_value(list)
  local values = list.values
  return {
    values = {
      items = values.items,
      element_size = values.element_size,
      count = values.count,
      capacity = values.capacity,
      zone = values.zone,
    },
  }
end

M.synthesizer_synthesizable_resources_spent = { count = 0, items = nil }

local g_family_trade_up = 0

function M.slipgate_takes_slipway(self)
  return (self.slipways.count < 3) and 1 or 0
end

function M.slipgate_accept_drop(self, s)
  local start_not_self
  local out = down_zero()

  if self == nil then
    return out
  end

  start_not_self = not (s ~= nil and s.class == require("game.slipway").slipway_class()
    and require("game.slipway").slipway_start(s) == self)

  out.stop = (start_not_self and M.slipgate_takes_slipway(self) ~= 0) and 1 or 0
  return out
end

local function slipgate_accept_drop_method(self, s)
  return M.slipgate_accept_drop(self, s)
end

M.slipgate_left_press_empty_class_def = {
  states = {},
  tags = 0,
  lineage = 0,
  skip_draw_off_screen = 0,
}

M.slipgate_left_press_empty = {}
entity_class.entity_init_header(M.slipgate_left_press_empty, M.slipgate_left_press_empty_class_def)

function M.slipgate_left_press(self)
  local accept_drop_result = down_zero()
  local slipway = require("game.slipway")
  local keys = slipway.slipway_keys_zero()
  local made

  if self == nil then
    return accept_drop_result
  end

  accept_drop_result = M.slipgate_accept_drop(self, M.slipgate_left_press_empty)

  if accept_drop_result.stop == 0 then
    return accept_drop_result
  end

  assert(slipway.slipway_ready() ~= 0, "slipway_init() must run before slipgate_left_press builds a slipway")
  keys.start = entity_id.entity_id_link(self)
  made = slipway.slipway_new(keys)
  accept_drop_result.held = entity_id.entity_id_link(made)
  accept_drop_result.stop = (made ~= nil) and 1 or 0
  return accept_drop_result
end

local function slipgate_left_press_method(self, mouse)
  return M.slipgate_left_press(self)
end

function M.slipgate_refresh(self)
  local slipways_count = 0
  local s_val = fixed.p8_fixed_from_int(0)

  if self == nil then
    return
  end

  slipways_count = self.slipways.count

  if self.archetype.link_sprites ~= nil then
    local found, entry = game_data.int_seq_at(self.archetype.link_sprites.items, self.archetype.link_sprites.count,
      slipways_count + 1, nil)
    if found ~= 0 then
      s_val = fixed.p8_fixed_from_int(entry)
    end
  end

  self.sprite = s_val
end

local function slipgate_refresh_method(self)
  M.slipgate_refresh(self)
end

function M.slipgate_offer_trade(self, trade_offer)
  local slipway_module = require("game.slipway")
  local trade = require("game.trade")
  local route
  local slipways = nil
  local iter
  local slipway

  if self == nil or trade_offer == nil then
    return
  end
  route = trade_offer.route

  slipways = self.slipways

  iter = entity_id_list.entity_id_list_all(slipways)
  while true do
    local more
    more, slipway = entity_id_list.entity_id_list_iter_next(iter, slipway)
    if more == 0 then
      break
    end
    if trade_offer.amount <= fixed.p8_fixed_from_int(0) then
      goto continue
    end

    if entity_id_list.entity_id_list_has(route.slipways, entity_id.entity_id_of(slipway)) ~= 0 then
      goto continue
    end

    do
      local gf = {
        route = trade_offer.route,
        amount = trade_offer.amount,
        accept = trade_offer.accept,
        accepted_routes = trade_route_list_value(trade_offer.accepted_routes),
      }
      local other

      gf.route = trade.trade_extend(frame_zone.frame_zone(), route, self, slipway)
      assert(slipway.class == slipway_module.slipway_class(), "a slipgate's slipways list holds slipways only: planet_attach_slipway adds to it")
      other = slipway_module.slipway_other_end(slipway, self)
      -- The copy is on the stack: no offer_trade method may store its offer.
      if other ~= nil and other.class.offer_trade ~= nil then
        other.class.offer_trade(other, gf)
      end
      trade_offer.amount = gf.amount
      trade_offer.accepted_routes = trade_route_list_value(gf.accepted_routes)
    end
    ::continue::
  end
end

local function slipgate_offer_trade_method(self, offer)
  M.slipgate_offer_trade(self, offer)
end

M.slipgate_class_def = {
  draw = planet_display.planet_draw_method,
  skip_draw_off_screen = 1,
  draw_overlay = probe.targeting_tooltip_method,
  hit_test = planet.planet_hit_test_method,
  new_year = planet.planet_new_year_method,
  refresh = slipgate_refresh_method,
  release = planet_display.planet_release_method,
  left_press = slipgate_left_press_method,
  accept_drop = slipgate_accept_drop_method,
  drag = probe.targeting_track_method,
  drop = probe.targeting_confirm_method,
  attach_slipway = planet_trade.planet_attach_slipway_method,
  offer_trade = slipgate_offer_trade_method,
  states = {
    [entity_class.ENTITY_STATE_VANISHING] = planet.planet_update_vanishing_method,
    [entity_class.ENTITY_STATE_SETTLED] = planet_family.structure_update_settled_method,
  },
  tags = bit.bor(entity_class.TAG(entity_class.TAG_SLIPGATE), entity_class.TAG(entity_class.TAG_NODE)),
  lineage = bit.bor(entity_class.LINEAGE(entity_class.LINEAGE_PLANET), entity_class.LINEAGE(entity_class.LINEAGE_STRUCTURE)),
}

local function slipgate_init_instance(self, cls, archetype)
  planet_family.structure_init_instance(self, cls, archetype)
end

local function slipgate_create(keys)
  local self = allocator.allocator_alloc(grab_arena.grab_arena_construction_zone(), SIZEOF.planet_t)

  assert(g_family_trade_up ~= 0, "planet_family_trade_init() must run first")
  slipgate_init_instance(self, M.slipgate_class_def, game_data.slipgate_archetype)
  planet.planet_copy_keys(self, keys)
  planet.planet_construct(self)
  entity.entity_construct(self)
  return self
end

M.infraspace_gate_class_def = {
  draw = planet_display.planet_draw_method,
  skip_draw_off_screen = 1,
  draw_overlay = probe.targeting_tooltip_method,
  hit_test = planet.planet_hit_test_method,
  new_year = planet.planet_new_year_method,
  refresh = slipgate_refresh_method,
  release = planet_display.planet_release_method,
  left_press = slipgate_left_press_method,
  accept_drop = slipgate_accept_drop_method,
  drag = probe.targeting_track_method,
  drop = probe.targeting_confirm_method,
  attach_slipway = planet_trade.planet_attach_slipway_method,
  offer_trade = slipgate_offer_trade_method,
  states = {
    [entity_class.ENTITY_STATE_VANISHING] = planet.planet_update_vanishing_method,
    [entity_class.ENTITY_STATE_SETTLED] = planet_family.structure_update_settled_method,
  },
  tags = bit.bor(entity_class.TAG(entity_class.TAG_SLIPGATE), entity_class.TAG(entity_class.TAG_NODE)),
  lineage = bit.bor(entity_class.LINEAGE(entity_class.LINEAGE_PLANET), entity_class.LINEAGE(entity_class.LINEAGE_STRUCTURE)),
}

local function infraspace_gate_create(keys)
  local self = allocator.allocator_alloc(grab_arena.grab_arena_construction_zone(), SIZEOF.planet_t)

  assert(g_family_trade_up ~= 0, "planet_family_trade_init() must run first")
  slipgate_init_instance(self, M.infraspace_gate_class_def, game_data.infraspace_gate_archetype)
  self.has = bit.bor(self.has, entity_class.ENTITY_HAS_INFRASPACE)
  self.infraspace = self.archetype.infraspace
  assert(keys == nil or (keys.has_map_pos == 0 and keys.has_pos == 0),
    "infraspace_gate_create: the only construction key is planet_id (structure_icon_left_press); another key has no member "
    .. "-- stop, do not add one")
  planet.planet_copy_keys(self, keys)
  planet.planet_construct(self)
  entity.entity_construct(self)
  return self
end

function M.synthesizer_attach_slipway(self, slipway)
  local other_end = nil

  if self ~= nil and slipway ~= nil then
    local slipway_module = require("game.slipway")
    assert(slipway.class == slipway_module.slipway_class(), "synthesizer_attach_slipway: the slipway is one slipway_drop registers")
    other_end = slipway_module.slipway_other_end(slipway, self)
  end

  if other_end ~= nil then
    local jy
    local it
    local i_val

    assert(planet.planet_is_family(other_end) ~= 0, "synthesizer_attach_slipway: the other end of a registered slipway is a planet")
    assert(bit.band(other_end.has_, planet.PLANET_HAS_NEEDS) ~= 0,
      "synthesizer_attach_slipway: the other end of a slipway to a synthesizer holds needs")
    jy = other_end.needs

    it = pointer_list.pointer_list_all(jy)
    while true do
      local more
      local i
      local gc

      more, i_val = pointer_list.pointer_list_iter_next(it, i_val)
      if more == 0 then
        break
      end
      if i_val == nil then
        goto continue
      end
      i = i_val

      gc = (i.has_satisfied ~= 0) and i.satisfied or fixed.p8_fixed_from_int(0)
      if gc ~= fixed.p8_fixed_from_int(0) then
        goto continue
      end

      if i.row.has_resource ~= 0 and self.synthesizable_resources ~= nil
        and game_data.int_seq_has_fixed(self.synthesizable_resources, i.row.resource) ~= 0 then
        local kw = planet.planet_production(self)
        kw.has_resource = 1
        kw.resource = i.row.resource
        self.synthesizable_resources = M.synthesizer_synthesizable_resources_spent
      end
      ::continue::
    end
  end

  planet_trade.planet_attach_slipway(self, slipway)
end

function M.synthesizer_set_spent(self, spent)
  assert(self ~= nil and self.class == M.synthesizer_class(), "synthesizer_set_spent: self is a synthesizer")
  self.synthesizable_resources = (spent ~= 0) and M.synthesizer_synthesizable_resources_spent
    or self.archetype.synthesizable_resources
end

local function synthesizer_attach_slipway_method(self, slipway)
  M.synthesizer_attach_slipway(self, slipway)
end

M.synthesizer_class_def = {
  draw = planet_family.ascension_draw_method,
  skip_draw_off_screen = 0,
  draw_overlay = probe.targeting_tooltip_method,
  hit_test = planet.planet_hit_test_method,
  new_year = planet.planet_new_year_method,
  refresh = planet_display.planet_refresh_method,
  release = planet_family.ascension_release_method,
  left_press = planet_display.planet_left_press_method,
  accept_drop = planet_display.planet_accept_drop_method,
  drag = probe.targeting_track_method,
  drop = probe.targeting_confirm_method,
  attach_slipway = synthesizer_attach_slipway_method,
  offer_trade = planet_trade.planet_offer_trade_method,
  spawn_particle = probe.probe_spawn_particle,
  states = {
    [entity_class.ENTITY_STATE_VANISHING] = planet.planet_update_vanishing_method,
    [entity_class.ENTITY_STATE_SETTLED] = planet_family.structure_update_settled_method,
  },
  tags = bit.bor(entity_class.TAG(entity_class.TAG_PLANET), entity_class.TAG(entity_class.TAG_NODE)),
  lineage = bit.bor(entity_class.LINEAGE(entity_class.LINEAGE_PLANET), entity_class.LINEAGE(entity_class.LINEAGE_STRUCTURE)),
}

local function synthesizer_create(keys)
  local self = allocator.allocator_alloc(grab_arena.grab_arena_construction_zone(), SIZEOF.planet_t)

  assert(g_family_trade_up ~= 0, "planet_family_trade_init() must run first")
  planet_family.ascension_init_instance(self, M.synthesizer_class_def, game_data.synthesizer_archetype)
  assert(keys == nil or (keys.has_map_pos == 0 and keys.has_pos == 0),
    "synthesizer_create: the only construction key is planet_id (structure_icon_left_press); another key has no member "
    .. "-- stop, do not add one")
  planet.planet_copy_keys(self, keys)
  require("game.particle").emitter_init(self)
  planet.planet_construct(self)
  entity.entity_construct(self)
  return self
end

function M.planet_family_trade_init()
  assert(planet_family.structure_ready() ~= 0 and planet_family.ascension_ready() ~= 0,
    "planet_family_init() must run first")
  M.planet_family_trade_shutdown()
  g_family_trade_up = 1
end

function M.planet_family_trade_shutdown()
  g_family_trade_up = 0
end

function M.slipgate_ready()
  return g_family_trade_up
end

local function slipgate_kind_new(keys)
  return slipgate_create(keys)
end

M.slipgate_kind_def = { new_entity = slipgate_kind_new, record = game_data.slipgate_kind_record }

function M.slipgate_kind()
  return M.slipgate_kind_def
end

function M.slipgate_class()
  return M.slipgate_class_def
end

function M.slipgate_new(keys)
  return slipgate_create(keys)
end

function M.infraspace_gate_ready()
  return g_family_trade_up
end

local function infraspace_gate_kind_new(keys)
  return infraspace_gate_create(keys)
end

M.infraspace_gate_kind_def = { new_entity = infraspace_gate_kind_new, record = game_data.infraspace_gate_kind_record }

function M.infraspace_gate_kind()
  return M.infraspace_gate_kind_def
end

function M.infraspace_gate_class()
  return M.infraspace_gate_class_def
end

function M.infraspace_gate_new(keys)
  return infraspace_gate_create(keys)
end

function M.synthesizer_ready()
  return g_family_trade_up
end

local function synthesizer_kind_new(keys)
  return synthesizer_create(keys)
end

M.synthesizer_kind_def = { new_entity = synthesizer_kind_new, record = game_data.synthesizer_kind_record }

function M.synthesizer_kind()
  return M.synthesizer_kind_def
end

function M.synthesizer_class()
  return M.synthesizer_class_def
end

function M.synthesizer_new(keys)
  return synthesizer_create(keys)
end

return M
