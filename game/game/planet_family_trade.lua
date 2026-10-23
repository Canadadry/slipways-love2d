-- Include cycle: game.planet, game.planet_display, game.planet_trade, game.planet_family and
-- game.probe are required at load, since their methods fill the class tables by identity.
-- game.slipway, game.particle and game.trade are required at call time.
local bit = require("bit")
local entity = require("engine.entity")
local entity_class = require("engine.entity_class")
local entity_id = require("engine.entity_id")
local game_data = require("game.game_data")
local planet = require("game.planet")
local planet_display = require("game.planet_display")
local planet_family = require("game.planet_family")
local planet_trade = require("game.planet_trade")
local probe = require("game.probe")
local fixed = require("p8.fixed")

local M = {}

local function down_zero()
  return { stop = false, held = nil }
end

local function route_list_copy(list)
  local copy = {}
  for i = 1, #list do
    copy[i] = list[i]
  end
  return copy
end

-- items is a 1-based array or nil.
M.synthesizer_synthesizable_resources_spent = { count = 0, items = nil }

local g_family_trade_up = false

function M.slipgate_takes_slipway(self)
  return #self.slipways < 3
end

function M.slipgate_accept_drop(self, s)
  local start_not_self
  local out = down_zero()

  if self == nil then
    return out
  end

  start_not_self = not (s ~= nil and s.class == require("game.slipway").class()
    and require("game.slipway").start_node(s) == self)

  out.stop = start_not_self and M.slipgate_takes_slipway(self)
  return out
end

local function slipgate_accept_drop_method(self, s)
  return M.slipgate_accept_drop(self, s)
end

M.slipgate_left_press_empty_class_def = {
  states = {},
  tags = 0,
  lineage = 0,
  skip_draw_off_screen = false,
}

M.slipgate_left_press_empty = {}
entity_class.init_header(M.slipgate_left_press_empty, M.slipgate_left_press_empty_class_def)

function M.slipgate_left_press(self)
  local accept_drop_result = down_zero()
  local slipway = require("game.slipway")
  local keys = slipway.keys_zero()
  local made

  if self == nil then
    return accept_drop_result
  end

  accept_drop_result = M.slipgate_accept_drop(self, M.slipgate_left_press_empty)

  if not accept_drop_result.stop then
    return accept_drop_result
  end

  assert(slipway.ready(), "slipway.init() must run before slipgate_left_press builds a slipway")
  keys.start = entity_id.link(self)
  made = slipway.new(keys)
  accept_drop_result.held = entity_id.link(made)
  accept_drop_result.stop = made ~= nil
  return accept_drop_result
end

local function slipgate_left_press_method(self, mouse)
  return M.slipgate_left_press(self)
end

function M.slipgate_refresh(self)
  local slipways_count = 0
  local s_val = fixed.from_int(0)

  if self == nil then
    return
  end

  slipways_count = #self.slipways

  if self.archetype.link_sprites ~= nil then
    local found, entry = game_data.int_seq_at(self.archetype.link_sprites.items, self.archetype.link_sprites.count,
      slipways_count + 1, nil)
    if found then
      s_val = fixed.from_int(entry)
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

  if self == nil or trade_offer == nil then
    return
  end
  route = trade_offer.route

  slipways = self.slipways

  for i = 1, #slipways do
    local slipway = entity_id.resolve(slipways[i])
    if trade_offer.amount <= fixed.from_int(0) then
      goto continue
    end

    do
      local slipway_id = entity_id.of(slipway)
      local on_route = false
      for j = 1, #route.slipways do
        if route.slipways[j] == slipway_id then
          on_route = true
          break
        end
      end
      if on_route then
        goto continue
      end
    end

    do
      local gf = {
        route = trade_offer.route,
        amount = trade_offer.amount,
        accept = trade_offer.accept,
        accepted_routes = route_list_copy(trade_offer.accepted_routes),
      }
      local other

      gf.route = trade.extend(route, self, slipway)
      assert(slipway.class == slipway_module.class(), "a slipgate's slipways list holds slipways only: planet_trade.attach_slipway adds to it")
      other = slipway_module.other_end(slipway, self)
      if other ~= nil and other.class.offer_trade ~= nil then
        other.class.offer_trade(other, gf)
      end
      trade_offer.amount = gf.amount
      trade_offer.accepted_routes = gf.accepted_routes
    end
    ::continue::
  end
end

local function slipgate_offer_trade_method(self, offer)
  M.slipgate_offer_trade(self, offer)
end

M.slipgate_class_def = {
  draw = planet_display.draw_method,
  skip_draw_off_screen = true,
  draw_overlay = probe.targeting_tooltip_method,
  hit_test = planet.hit_test_method,
  new_year = planet.new_year_method,
  refresh = slipgate_refresh_method,
  release = planet_display.release_method,
  left_press = slipgate_left_press_method,
  accept_drop = slipgate_accept_drop_method,
  drag = probe.targeting_track_method,
  drop = probe.targeting_confirm_method,
  attach_slipway = planet_trade.attach_slipway_method,
  offer_trade = slipgate_offer_trade_method,
  states = {
    [entity_class.STATE_VANISHING] = planet.update_vanishing_method,
    [entity_class.STATE_SETTLED] = planet_family.structure_update_settled_method,
  },
  tags = bit.bor(entity_class.TAG(entity_class.TAG_SLIPGATE), entity_class.TAG(entity_class.TAG_NODE)),
  lineage = bit.bor(entity_class.LINEAGE(entity_class.LINEAGE_PLANET), entity_class.LINEAGE(entity_class.LINEAGE_STRUCTURE)),
}

local function slipgate_init_instance(self, cls, archetype)
  planet_family.structure_init_instance(self, cls, archetype)
end

local function slipgate_create(keys)
  local self = {}

  assert(g_family_trade_up, "planet_family_trade.init() must run first")
  slipgate_init_instance(self, M.slipgate_class_def, game_data.slipgate_archetype)
  planet.copy_keys(self, keys)
  planet.construct(self)
  entity.construct(self)
  return self
end

M.infraspace_gate_class_def = {
  draw = planet_display.draw_method,
  skip_draw_off_screen = true,
  draw_overlay = probe.targeting_tooltip_method,
  hit_test = planet.hit_test_method,
  new_year = planet.new_year_method,
  refresh = slipgate_refresh_method,
  release = planet_display.release_method,
  left_press = slipgate_left_press_method,
  accept_drop = slipgate_accept_drop_method,
  drag = probe.targeting_track_method,
  drop = probe.targeting_confirm_method,
  attach_slipway = planet_trade.attach_slipway_method,
  offer_trade = slipgate_offer_trade_method,
  states = {
    [entity_class.STATE_VANISHING] = planet.update_vanishing_method,
    [entity_class.STATE_SETTLED] = planet_family.structure_update_settled_method,
  },
  tags = bit.bor(entity_class.TAG(entity_class.TAG_SLIPGATE), entity_class.TAG(entity_class.TAG_NODE)),
  lineage = bit.bor(entity_class.LINEAGE(entity_class.LINEAGE_PLANET), entity_class.LINEAGE(entity_class.LINEAGE_STRUCTURE)),
}

local function infraspace_gate_create(keys)
  local self = {}

  assert(g_family_trade_up, "planet_family_trade.init() must run first")
  slipgate_init_instance(self, M.infraspace_gate_class_def, game_data.infraspace_gate_archetype)
  self.infraspace = self.archetype.infraspace
  assert(keys == nil or (keys.map_pos == nil and keys.pos == nil),
    "infraspace_gate_create: the only construction key is planet_id (structure_icon.left_press); another key has no member "
    .. "-- stop, do not add one")
  planet.copy_keys(self, keys)
  planet.construct(self)
  entity.construct(self)
  return self
end

function M.synthesizer_attach_slipway(self, slipway)
  local other_end = nil

  if self ~= nil and slipway ~= nil then
    local slipway_module = require("game.slipway")
    assert(slipway.class == slipway_module.class(), "synthesizer_attach_slipway: the slipway is one slipway.drop registers")
    other_end = slipway_module.other_end(slipway, self)
  end

  if other_end ~= nil then
    local jy

    assert(planet.is_family(other_end), "synthesizer_attach_slipway: the other end of a registered slipway is a planet")
    assert(other_end.needs ~= nil,
      "synthesizer_attach_slipway: the other end of a slipway to a synthesizer holds needs")
    jy = other_end.needs

    for index = 1, #jy do
      local i_val = jy[index]
      local i
      local gc

      if i_val == nil then
        goto continue
      end
      i = i_val

      gc = i.satisfied or fixed.from_int(0)
      if gc ~= fixed.from_int(0) then
        goto continue
      end

      if i.row.resource ~= nil and self.synthesizable_resources ~= nil
        and game_data.int_seq_has_fixed(self.synthesizable_resources, i.row.resource) then
        local kw = planet.production(self)
        kw.resource = i.row.resource
        self.synthesizable_resources = M.synthesizer_synthesizable_resources_spent
      end
      ::continue::
    end
  end

  planet_trade.attach_slipway(self, slipway)
end

function M.synthesizer_set_spent(self, spent)
  assert(self ~= nil and self.class == M.synthesizer_class(), "synthesizer_set_spent: self is a synthesizer")
  self.synthesizable_resources = spent and M.synthesizer_synthesizable_resources_spent
    or self.archetype.synthesizable_resources
end

local function synthesizer_attach_slipway_method(self, slipway)
  M.synthesizer_attach_slipway(self, slipway)
end

M.synthesizer_class_def = {
  draw = planet_family.ascension_draw_method,
  skip_draw_off_screen = false,
  draw_overlay = probe.targeting_tooltip_method,
  hit_test = planet.hit_test_method,
  new_year = planet.new_year_method,
  refresh = planet_display.refresh_method,
  release = planet_family.ascension_release_method,
  left_press = planet_display.left_press_method,
  accept_drop = planet_display.accept_drop_method,
  drag = probe.targeting_track_method,
  drop = probe.targeting_confirm_method,
  attach_slipway = synthesizer_attach_slipway_method,
  offer_trade = planet_trade.offer_trade_method,
  spawn_particle = probe.spawn_particle,
  states = {
    [entity_class.STATE_VANISHING] = planet.update_vanishing_method,
    [entity_class.STATE_SETTLED] = planet_family.structure_update_settled_method,
  },
  tags = bit.bor(entity_class.TAG(entity_class.TAG_PLANET), entity_class.TAG(entity_class.TAG_NODE)),
  lineage = bit.bor(entity_class.LINEAGE(entity_class.LINEAGE_PLANET), entity_class.LINEAGE(entity_class.LINEAGE_STRUCTURE)),
}

local function synthesizer_create(keys)
  local self = {}

  assert(g_family_trade_up, "planet_family_trade.init() must run first")
  planet_family.ascension_init_instance(self, M.synthesizer_class_def, game_data.synthesizer_archetype)
  assert(keys == nil or (keys.map_pos == nil and keys.pos == nil),
    "synthesizer_create: the only construction key is planet_id (structure_icon.left_press); another key has no member "
    .. "-- stop, do not add one")
  planet.copy_keys(self, keys)
  require("game.particle").emitter_init(self)
  planet.construct(self)
  entity.construct(self)
  return self
end

function M.init()
  assert(planet_family.structure_ready() and planet_family.ascension_ready(),
    "planet_family.init() must run first")
  M.shutdown()
  g_family_trade_up = true
end

function M.shutdown()
  g_family_trade_up = false
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
