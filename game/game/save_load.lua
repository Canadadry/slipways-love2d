-- Copy of src/game/save_load.h then src/game/save_load.c.
--
-- Every state came out of a save_state_decode that returned 1: nothing is validated again.
-- node_constructors is 0-indexed by SAVE_NODE_* kind. The ids in node_ids and slipway_ids are
-- new tables from entity_id_link.
local bit = require("bit")
local entity = require("engine.entity")
local entity_class = require("engine.entity_class")
local entity_id = require("engine.entity_id")
local entity_registry = require("engine.entity_registry")
local vec = require("engine.vec")
local economy_tiers = require("game.economy_tiers")
local galaxy = require("game.galaxy")
local game_data = require("game.game_data")
local gen = require("game.gen")
local map_tables = require("game.map_tables")
local planet_module = require("game.planet")
local planet_family = require("game.planet_family")
local planet_family_trade = require("game.planet_family_trade")
local probe_module = require("game.probe")
local research = require("game.research")
local save_state = require("game.save_state")
local slipway_module = require("game.slipway")
local structure_icon = require("game.structure_icon")
local trade = require("game.trade")
local ui_button = require("game.ui_button")
local fixed = require("p8.fixed")

local M = {}

local function vec_of(saved)
  return vec.vec_make(saved.x, saved.y)
end

local function has(node, flag)
  return (bit.band(node.flags, flag) ~= 0) and 1 or 0
end

-- Fills out the way the title fills a difficulty button.
function M.save_load_difficulty(state, out)
  local option
  assert(state ~= nil and out ~= nil)
  local zero = ui_button.button_zero()
  option = game_data.difficulty_options_table[state.difficulty - 1]
  for key in pairs(out) do
    out[key] = nil
  end
  for key, value in pairs(zero) do
    out[key] = value
  end
  out.name = option.name
  out.trade = fixed.p8_fixed_from_int(option.trade)
  out.type_bias = option.type_bias
end

-- Before the prologue builds the background: the background then draws
-- the saved tile map.
function M.save_load_background_seed(state)
  assert(state ~= nil)
  fixed.p8_fixed_set_rnd_state(state.background_seed)
end

local function load_needs(planet, node)
  local needs = {}

  if has(node, save_state.SAVE_NODE_HAS_NEEDS) == 0 then
    planet.needs = nil
    return
  end
  for i = 0, node.need_count - 1 do
    local resource = { kind = trade.RESOURCE_NIL, number = 0 }
    if node.needs[i].resource ~= nil then
      resource.kind = trade.RESOURCE_FIXED
      resource.number = node.needs[i].resource
    end
    needs[#needs + 1] = research.need_new(resource)
  end
  planet.needs = needs
end

local function load_production(planet, node)
  if has(node, save_state.SAVE_NODE_HAS_PRODUCTION) == 0 then
    planet.production = nil
    return
  end
  if planet.production == nil then
    planet.production = {}
  end
  planet.production.resource = (has(node, save_state.SAVE_NODE_PRODUCTION_HAS_RESOURCE) ~= 0)
    and node.production_resource or nil
  -- The routes rebuild of the first recompute reads the amount before update_tiers sets it.
  planet.production.amount = fixed.p8_fixed_from_int(0)
  local _, amount = economy_tiers.economy_tiers_compute_production_amount(planet, planet.production.amount)
  planet.production.amount = amount
end

M.node_constructors = {
  [save_state.SAVE_NODE_PLANET] = planet_module.planet_new,
  [save_state.SAVE_NODE_PROTOSTAR] = planet_family.protostar_new,
  [save_state.SAVE_NODE_STRUCTURE] = planet_family.structure_new,
  [save_state.SAVE_NODE_PROCESSOR] = planet_family.processor_new,
  [save_state.SAVE_NODE_SLIPGATE] = planet_family_trade.slipgate_new,
  [save_state.SAVE_NODE_INFRASPACE_GATE] = planet_family_trade.infraspace_gate_new,
  [save_state.SAVE_NODE_TRADING_HUB] = planet_family.trading_hub_new,
  [save_state.SAVE_NODE_SYNTHESIZER] = planet_family_trade.synthesizer_new,
  [save_state.SAVE_NODE_ASCENSION] = planet_family.ascension_new,
}
local node_constructors = M.node_constructors

-- Planets and protostars take their position as keys; the structure
-- constructors assert that their keys hold none, so it is set after.
local function construct_node(node)
  local positions_are_keys = (node.kind == save_state.SAVE_NODE_PLANET or node.kind == save_state.SAVE_NODE_PROTOSTAR) and 1 or 0
  local keys = planet_module.planet_keys_zero()
  local planet

  if has(node, save_state.SAVE_NODE_HAS_PLANET_ID) ~= 0 then
    planet_module.planet_keys_set_id(keys, node.planet_id)
  end
  if positions_are_keys ~= 0 and has(node, save_state.SAVE_NODE_HAS_POS) ~= 0 then
    keys.pos = vec_of(node.pos)
  end
  if positions_are_keys ~= 0 and has(node, save_state.SAVE_NODE_HAS_MAP_POS) ~= 0 then
    keys.map_pos = vec_of(node.map_pos)
  end
  planet = node_constructors[node.kind](keys)
  if positions_are_keys == 0 and has(node, save_state.SAVE_NODE_HAS_POS) ~= 0 then
    entity_class.entity_set_pos(planet, vec_of(node.pos))
  end
  if positions_are_keys == 0 and has(node, save_state.SAVE_NODE_HAS_MAP_POS) ~= 0 then
    entity_class.entity_set_map_pos(planet, vec_of(node.map_pos))
  end
  return planet
end

local function load_node(node)
  local planet = construct_node(node)

  entity.entity_set_state(planet, node.state)
  if has(node, save_state.SAVE_NODE_HAS_STATE_TIME) ~= 0 then
    planet.state_time = node.state_time
  end
  planet.planet_type = (node.planet_type ~= 0) and game_data.planet_types_table[node.planet_type - 1] or nil
  if node.project ~= 0 then
    planet_module.planet_install_project(planet, research.research_projects_at(node.project))
  end
  -- After planet_install_project, which clears it.
  planet.description = (node.description_type ~= 0) and game_data.planet_types_table[node.description_type - 1].description or nil
  load_needs(planet, node)
  planet.development_tier = nil
  if has(node, save_state.SAVE_NODE_HAS_DEVELOPMENT_TIER) ~= 0 then
    planet.development_tier = node.development_tier
  end
  planet.production_bonus = nil
  if has(node, save_state.SAVE_NODE_HAS_PRODUCTION_BONUS) ~= 0 then
    planet.production_bonus = node.production_bonus
  end
  planet.fold_anchor = node.fold_anchor
  if node.infraspace ~= 0 then
    planet.infraspace = node.infraspace
  end
  return planet
end

-- Unregistered rather than marked dead: the release waits for the next
-- frame's exit-queue flush, so the first update never removes an entity
-- mid-walk and skips the next one.
local function remove_bursts()
  local live = entity_registry.live_entities()
  for i = #live, 1, -1 do
    if live[i].class == planet_family.burst_class() then
      entity_registry.entity_unregister(live[i])
    end
  end
end

-- A probe is only ever saved settled: a held one exists before the first
-- action only, when nothing is pending.
local function load_probe(saved)
  local probe = probe_module.probe_new(nil)
  entity.entity_set_state(probe, entity_class.ENTITY_STATE_SETTLED)
  entity_class.entity_set_pos(probe, vec_of(saved.pos))
  entity_class.entity_set_map_pos(probe, vec_of(saved.map_pos))
  probe.fold_anchor = saved.fold_anchor
end

local function load_slipways(state, node_ids, out)
  for i = 0, state.slipway_count - 1 do
    local keys = slipway_module.slipway_keys_zero()
    local start = node_ids[state.slipways[i].start]
    local slipway
    keys.start = start
    slipway = slipway_module.slipway_new(keys)
    slipway_module.slipway_connect(slipway, entity_id.entity_get(node_ids[state.slipways[i].end_]))
    slipway.state_time = state.slipways[i].state_time
    out[i] = entity_id.entity_id_link(slipway)
  end
end

-- Node-major, each list in its saved order: every list comes out as saved.
local function load_attach_order(state, node_ids, slipway_ids)
  for i = 0, state.node_count - 1 do
    local node = state.nodes[i]
    local header = entity_id.entity_get(node_ids[i])
    assert(header.class.attach_slipway ~= nil, "save_load: every saved node takes slipways")
    for k = 0, node.slipway_count - 1 do
      header.class.attach_slipway(header, entity_id.entity_get(slipway_ids[state.node_slipways[node.first_slipway + k]]))
    end
  end
end

-- Attaching sets the fold anchor and restarts the shake, as a protostar's
-- archetype does on construction; a synthesizer's attach reads needs not
-- satisfied yet, so may make another resource and spend itself when the
-- saved game did not.
local function undo_attach_side_effects(state, node_ids)
  for i = 0, state.node_count - 1 do
    local node = state.nodes[i]
    local planet = entity_id.entity_get(node_ids[i])
    planet.fold_anchor = node.fold_anchor
    planet.shake = 0
    load_production(planet, node)
    if node.kind == save_state.SAVE_NODE_SYNTHESIZER then
      planet_family_trade.synthesizer_set_spent(planet, has(node, save_state.SAVE_NODE_SYNTHESIZER_SPENT))
    end
  end
end

local function route_id_list(state, first, count, ids)
  local list = {}
  for i = 0, count - 1 do
    local id = ids[state.route_indices[first + i]]
    assert(id ~= nil, "route_id_list: a saved route index names a loaded node or slipway")
    list[#list + 1] = id
  end
  return list
end

-- Never recomputed: a partner chosen long ago stays the same.
local function load_outgoing_routes(state, node_ids, slipway_ids)
  for i = 0, state.node_count - 1 do
    local node = state.nodes[i]
    local planet = entity_id.entity_get(node_ids[i])
    if has(node, save_state.SAVE_NODE_HAS_OUTGOING_ROUTES) == 0 then
      goto continue
    end
    if planet.outgoing_routes == nil then
      planet.outgoing_routes = {}
    end
    for r = #planet.outgoing_routes, 1, -1 do
      planet.outgoing_routes[r] = nil
    end
    for k = 0, node.route_count - 1 do
      local saved = state.routes[node.first_route + k]
      local keys = { planets = nil, slipways = nil, resource = nil, resource_is_bool = 0 }
      local route
      keys.planets = route_id_list(state, saved.first_planet, saved.planet_count, node_ids)
      keys.slipways = route_id_list(state, saved.first_slipway, saved.slipway_count, slipway_ids)
      keys.resource_is_bool = saved.resource_is_bool
      keys.resource = saved.resource
      route = trade.trade_new(keys)
      trade.trade_route_list_add(planet.outgoing_routes, route)
    end
    ::continue::
  end
end

local function load_rolls(state)
  local saved = state.type_picker
  local picker
  local walk
  local vanish_walk

  galaxy.galaxy_install_rolls()
  picker = planet_module.planet_type_picker()
  walk = gen.weighted_picker_walk(picker)
  vanish_walk = planet_module.planet_vanish_roll()
  assert(picker ~= nil and walk ~= nil and vanish_walk ~= nil)

  picker.tickets_count = saved.tickets_count
  for i = 0, saved.tickets_count - 1 do
    picker.tickets[i] = game_data.planet_types_table[saved.tickets[i]]
  end
  for i = 0, save_state.SAVE_STATE_RECENT_COUNT - 1 do
    picker.recent_set[i] = saved.recent_set[i]
    picker.recent[i] = (saved.recent_set[i] ~= 0) and game_data.planet_types_table[saved.recent[i]] or nil
  end
  picker.multiplier = saved.multiplier
  walk.step = saved.walk_step
  walk.bias = saved.walk_bias
  vanish_walk.step = state.vanish_walk.step
  vanish_walk.bias = state.vanish_walk.bias
end

local node_ids = {}
local slipway_ids = {}

-- Where cart_start_game calls galaxy_populate: the saved nodes, then the
-- type picker and vanish walk.
function M.save_load_world(state)
  assert(state ~= nil)
  for i = 0, state.node_count - 1 do
    node_ids[i] = entity_id.entity_id_link(load_node(state.nodes[i]))
  end
  -- A protostar's constructor bursts; the save never holds a burst.
  remove_bursts()
  -- Before the slipways: the probe and infraspace slipways share a layer, drawn in registration order.
  if state.probe ~= nil then
    load_probe(state.probe)
  end
  load_slipways(state, node_ids, slipway_ids)
  load_attach_order(state, node_ids, slipway_ids)
  undo_attach_side_effects(state, node_ids)
  load_outgoing_routes(state, node_ids, slipway_ids)
  load_rolls(state)
end

local function refresh_structure_icons()
  local refreshed = entity_registry.entities_by_message(entity_registry.ENTITY_MESSAGE_REFRESH)
  local i = 1
  while refreshed ~= nil and i <= #refreshed do
    if refreshed[i].class == structure_icon.structure_icon_class() then
      structure_icon.structure_icon_refresh(refreshed[i])
    end
    i = i + 1
  end
end

-- One tech at a time, in saved order: an icon takes the shared cursor slot
-- when revealed. Never economy_invent: it spends research points and
-- recomputes routes against a half-restored list.
local function load_invented(economy, saved)
  refresh_structure_icons()
  for i = 0, saved.invented_count - 1 do
    local name = map_tables.technologies_table[saved.invented[i]].name
    assert(name ~= nil, "economy.invented holds technology names")
    economy.invented[#economy.invented + 1] = name
    refresh_structure_icons()
  end
end

-- After the economy and the interface are built; restores rnd_state last.
function M.save_load_globals(state)
  local economy = probe_module.main_economy_typed()
  local camera = entity_registry.main_camera()

  assert(state ~= nil)
  assert(economy ~= nil, "save_load_globals: the economy is built first")
  economy.credits = state.economy.credits
  economy.research_points = state.economy.research_points
  economy.era = state.economy.era
  economy.year = state.economy.year
  economy.year_ticks = state.economy.year_ticks
  load_invented(economy, state.economy)

  structure_icon.structure_icon_set_id_counter(state.structure_id_counter)

  entity_class.entity_set_map_pos(camera, vec_of(state.camera_map_pos))
  entity_class.entity_set_pos(camera, vec_of(state.camera_map_pos))
  -- A protostar's burst shook it; the save never holds a shake.
  camera.shake = 0

  -- Last: the constructors before it may draw from the RNG.
  fixed.p8_fixed_set_rnd_state(state.rnd_state)
end

return M
