-- Copy of src/game/slipway.h then src/game/slipway.c, plus the Lua-only slipway_keys_zero.
--
-- Shapes, with the C field names:
--   slipway_keys_t: { start }.
--   slipway_served_planets_t: { count, ids }, ids 0-indexed Lua strings.
--   slipway_mouse_point_t: an entity header, one table.
--   slipway_t: one table holding the entity header and { archetype, fade_in_step, start,
--     transit_dots, served_planets, end_, end_is_point, targeted_by, targeting, point }; `end` is
--     `end_` (a Lua keyword), targeting a targeting_t (game.targeting).
--   slipway_too_far_t: the entity header plus { status_text, targeted_by }, one table.
--   slipway_obstruction_node_ctx_t: { start_point, end_point }.
--   slipway_obstruction_slipway_ctx_t: { start_point, end_point, self_has_infraspace,
--     self_infraspace }.
--
-- Declared in slipway.h, defined in src/game/slipway_display.c and ported there:
-- slipway_draw_method.
--
-- Include cycle: game.slipway_display is required at load, since slipway_draw_method fills
-- slipway_class_def; slipway_display must reach this module at call time only. game.economy,
-- game.planet, game.probe and game.ui are required at call time, inside the functions that need
-- them.
local bit = require("bit")
local allocator = require("engine.allocator")
local dispatch_context = require("engine.dispatch_context")
local entity = require("engine.entity")
local entity_class = require("engine.entity_class")
local entity_id = require("engine.entity_id")
local entity_id_list = require("engine.entity_id_list")
local entity_id_type = require("engine.entity_id_type")
local entity_list = require("engine.entity_list")
local entity_registry = require("engine.entity_registry")
local frame_zone = require("engine.frame_zone")
local geom = require("engine.geom")
local pointer_list = require("engine.pointer_list")
local vec = require("engine.vec")
local zone = require("engine.zone")
local economy_formulas = require("game.economy_formulas")
local game_data = require("game.game_data")
local grab_arena = require("game.grab_arena")
local slipway_display = require("game.slipway_display")
local targeting_module = require("game.targeting")
local tech_gate = require("game.tech_gate")
local fixed = require("p8.fixed")

local M = {}

M.SIZEOF = {
  slipway_t = 960,
  ["void *"] = 8,
}
local SIZEOF = M.SIZEOF

M.SLIPWAY_SERVED_PLANETS_CAP = 90

M.SLIPWAY_TRANSIT_DOTS_RESERVE = 24

M.SLIPWAY_CONSTRUCTION_BYTES = zone.ZONE_BYTES(SIZEOF.slipway_t)
  + zone.ZONE_BYTES(M.SLIPWAY_TRANSIT_DOTS_RESERVE * SIZEOF["void *"])

local function entity_id_none()
  local none = entity_id_type.ENTITY_ID_NONE
  return { index = none.index, generation = none.generation }
end

-- pointer_list_t list = {0}.
local function list_zero()
  return { items = nil, count = 0, capacity = 0, zone = nil }
end

local g_slipway_up = 0

M.slipway_too_far_class_def = {
  states = {},
  tags = 0,
  lineage = 0,
  skip_draw_off_screen = 0,
}

M.g_slipway_too_far = {}
entity_class.entity_init_header(M.g_slipway_too_far, M.slipway_too_far_class_def)
M.g_slipway_too_far.status_text = "-too far-"
M.g_slipway_too_far.targeted_by = { index = 0, generation = 0 }

M.slipway_mouse_point_class_def = {
  states = {},
  tags = 0,
  lineage = 0,
  skip_draw_off_screen = 0,
}

function M.slipway_end_map_pos(end_)
  if end_ ~= nil then
    return entity_class.entity_map_pos(end_)
  end
  return vec.vec_make(fixed.p8_fixed_from_int(0), fixed.p8_fixed_from_int(0))
end

function M.slipway_end_pos(end_)
  if end_ ~= nil then
    return entity_class.entity_pos(end_)
  end
  return vec.vec_make(fixed.p8_fixed_from_int(0), fixed.p8_fixed_from_int(0))
end

-- nil once the planet was flushed.
function M.slipway_start(self)
  return entity_id.entity_get(self.start)
end

-- The planet, the slipway's own mouse point, or nil.
function M.slipway_end(self)
  if self.end_is_point ~= 0 then
    return self.point
  end
  return entity_id.entity_get(self.end_)
end

function M.slipway_too_far()
  return M.g_slipway_too_far
end

local function slipway_copy_infraspace(self, from)
  if from == nil or entity_class.entity_has(from, entity_class.ENTITY_HAS_INFRASPACE) == 0 then
    return 0
  end
  self.infraspace = from.infraspace
  self.has = bit.bor(self.has, entity_class.ENTITY_HAS_INFRASPACE)
  return 1
end

-- The start's infraspace, else the end's, and the layer that follows.
local function slipway_inherit_infraspace(self)
  -- An infraspace of 0 still counts as one.
  self.infraspace = fixed.p8_fixed_from_int(0)
  self.has = bit.band(bit.band(self.has, bit.bnot(entity_class.ENTITY_HAS_INFRASPACE)), 0xFFFF)
  if slipway_copy_infraspace(self, M.slipway_start(self)) == 0 then
    slipway_copy_infraspace(self, M.slipway_end(self))
  end
  if entity_class.entity_has(self, entity_class.ENTITY_HAS_INFRASPACE) ~= 0 then
    entity_registry.entity_registry_set_layer(self, fixed.p8_fixed_from_int(2))
  else
    entity_registry.entity_registry_set_layer(self, fixed.p8_fixed_from_int(4))
  end
end

-- Copies id; the caller keeps its own string.
function M.slipway_served_planets_add(hg, id)
  assert(id ~= nil,
    "slipway_served_planets_add: a NULL identifier does not fit the member's type -- stop, do not widen the member")
  if M.slipway_served_planets_has(hg, id) ~= 0 then
    return
  end
  assert(hg.count < M.SLIPWAY_SERVED_PLANETS_CAP,
    "slipway_served_planets_add: more planets than the galaxy holds (90) -- stop, do not widen the member")
  assert(#id < require("game.planet").PLANET_ID_SIZE,
    "slipway_served_planets_add: a planet id wider than any the cart makes -- stop, do not widen the copy")
  hg.ids[hg.count] = id
  hg.count = hg.count + 1
end

function M.slipway_served_planets_has(hg, id)
  if hg == nil or id == nil then
    return 0
  end
  for i = 0, hg.count - 1 do
    if hg.ids[i] == id then
      return 1
    end
  end
  return 0
end

function M.slipway_construct(self)
  if self == nil then
    return
  end

  pointer_list.pointer_list_init_reserve(self.transit_dots, grab_arena.grab_arena_construction_zone(),
    M.SLIPWAY_TRANSIT_DOTS_RESERVE)
  self.served_planets.count = 0
end

function M.slipway_drag(self, mouse)
  local probe = require("game.probe")
  local ui = require("game.ui")
  local ok
  local hovered

  if self == nil then
    return
  end

  ok = ui.main_ui()
  if ok ~= nil then
    hovered = ui.ui_hovered(ok)
  else
    hovered = nil
  end

  if hovered ~= nil and hovered.class.accept_drop ~= nil then
    self.end_ = entity_id.entity_id_link(hovered)
    self.end_is_point = 0
  else
    local point_pos = entity_class.mouse_pos_world(mouse)

    if entity_class.mouse_pos_has_world(mouse) ~= 0 then
      entity_class.entity_set_pos(self.point, point_pos)
    else
      entity_class.entity_clear_pos(self.point)
    end
    entity_class.entity_set_map_pos(self.point, vec.vec_div(point_pos, fixed.p8_fixed_from_int(128)))
    self.end_ = entity_id_none()
    self.end_is_point = 1
  end

  self.targeting.cost = economy_formulas.economy_formulas_route_base_cost(
    M.slipway_end_map_pos(M.slipway_start(self)), M.slipway_end_map_pos(M.slipway_end(self)))
  self.targeting.has_cost = 1

  slipway_inherit_infraspace(self)

  probe.targeting_set_target(self.targeting, M.slipway_obstruction(self))
  if probe.targeting_target(self.targeting) ~= nil then
    probe.probe_link_target(probe.targeting_target(self.targeting), self)
  end
end

-- Joins a built slipway to end as a confirmed drag would, without paying, advancing the clock,
-- attaching either end or touching the fade.
function M.slipway_connect(self, end_)
  assert(self ~= nil and end_ ~= nil, "slipway_connect: a resumed slipway joins two saved nodes")
  self.end_ = entity_id.entity_id_link(end_)
  self.end_is_point = 0
  slipway_inherit_infraspace(self)
  entity.entity_set_state(self, entity_class.ENTITY_STATE_NORMAL)
end

function M.slipway_drop(self, end_planet)
  local probe = require("game.probe")
  local refused

  if self == nil then
    return
  end

  -- The charge spends credits: keep it behind the NULL test. The clock advances once, after both attaches.
  refused = (end_planet == nil) or probe.targeting_confirm_without_clock(self) == 0
  if refused then
    self.is_dead = 1
    return
  end

  self.fade_in_step = fixed.p8_fixed_from_int(1)
  self.end_ = entity_id.entity_id_link(end_planet)
  self.end_is_point = 0

  do
    local qu = M.slipway_start(self)
    local start_registered = qu ~= nil and qu.class.attach_slipway ~= nil
    local end_registered
    if start_registered then
      qu.class.attach_slipway(qu, self)
    end
    end_registered = end_planet ~= nil and end_planet.class.attach_slipway ~= nil
    if end_registered then
      end_planet.class.attach_slipway(end_planet, self)
    end
    assert(start_registered and end_registered,
      "slipway_drop: both endpoints must have attach_slipway, or the slipway never joins their slipways lists")
    require("game.economy").economy_advance_clock(probe.main_economy_typed(), fixed.p8_fixed_from_int(1))
  end
end

local function slipway_obstruction_node_predicate(e, ctx_ptr)
  local ctx = ctx_ptr

  if e == nil then
    return 0
  end
  return geom.geom_near_segment(ctx.start_point, ctx.end_point, M.slipway_end_map_pos(e),
    fixed.P8_FIXED_LIT(0x000000d1, 0.0032))
end

local function slipway_obstruction_slipway_predicate(e, ctx_ptr)
  local ctx = ctx_ptr
  local sw

  if e == nil then
    return 0
  end
  assert(e.class == M.slipway_class(), "the slipway tag holds slipways only")
  sw = e
  if entity_class.entity_has(sw, entity_class.ENTITY_HAS_INFRASPACE) ~= ctx.self_has_infraspace or
    (entity_class.entity_has(sw, entity_class.ENTITY_HAS_INFRASPACE) ~= 0 and sw.infraspace ~= ctx.self_infraspace) then
    return 0
  end
  return geom.geom_segments_cross(ctx.start_point, ctx.end_point, M.slipway_end_map_pos(M.slipway_start(sw)),
    M.slipway_end_map_pos(M.slipway_end(sw)))
end

function M.slipway_obstruction(self)
  local start_node
  local end_node
  local start_point
  local end_point
  local threshold

  if self == nil then
    return nil
  end

  start_node = M.slipway_start(self)
  end_node = M.slipway_end(self)
  start_point = M.slipway_end_map_pos(start_node)
  end_point = M.slipway_end_map_pos(end_node)

  threshold = tech_gate.tech_gate_if_unlocked("space folding", fixed.P8_FIXED_LIT(0x00008000, 0.5),
    fixed.P8_FIXED_LIT(0x0001147a, 1.08))

  if vec.vec_length_squared(vec.vec_sub(start_point, end_point)) > threshold then
    return M.g_slipway_too_far
  end

  do
    local oe = nil

    if start_node ~= nil then
      assert(require("game.planet").planet_is_family(start_node) ~= 0,
        "a slipway's start is a planet: slipway_create asserts it")
      oe = start_node.slipways
    end
    if oe ~= nil then
      local it = entity_id_list.entity_id_list_all(oe)
      local w
      while true do
        local more
        more, w = entity_id_list.entity_id_list_iter_next(it, w)
        if more == 0 then
          break
        end
        assert(w.class == M.slipway_class(),
          "the start's slipways list holds slipways: only planet_attach_slipway adds to it, and only slipways")
        if M.slipway_other_end(w, start_node) == end_node then
          return w
        end
      end
    end
  end

  do
    local node_ctx = {}
    local slipway_ctx = {}
    local part1
    local part2
    local ds
    local result = nil

    node_ctx.start_point = start_point
    node_ctx.end_point = end_point
    slipway_ctx.start_point = start_point
    slipway_ctx.end_point = end_point
    slipway_ctx.self_has_infraspace = entity_class.entity_has(self, entity_class.ENTITY_HAS_INFRASPACE)
    slipway_ctx.self_infraspace = self.infraspace

    part1 = entity_list.entity_list_filter(frame_zone.frame_zone(),
      entity_registry.entities_by_tag(entity_class.TAG_NODE), slipway_obstruction_node_predicate, node_ctx)
    part2 = entity_list.entity_list_filter(frame_zone.frame_zone(),
      entity_registry.entities_by_tag(entity_class.TAG_SLIPWAY), slipway_obstruction_slipway_predicate, slipway_ctx)
    ds = entity_list.entity_list_concat(frame_zone.frame_zone(), part1, part2)

    if ds.count > 0 then
      result = ds.items[0]
    end
    return result
  end
end

function M.slipway_other_end(self, planet_instance)
  if self == nil then
    return nil
  end

  -- A NULL end falls through to the start even when `planet_instance` is the start.
  if planet_instance == M.slipway_start(self) and M.slipway_end(self) ~= nil then
    return M.slipway_end(self)
  end
  return M.slipway_start(self)
end

function M.slipway_draw_overlay(self)
  if self == nil then
    return
  end

  if M.slipway_start(self) ~= M.slipway_end(self) then
    require("game.probe").targeting_tooltip(dispatch_context.dispatch_context_gfx(),
      dispatch_context.dispatch_context_mem(), self)
  end
end

local function slipway_drag_method(self, mouse)
  M.slipway_drag(self, mouse)
end

local function slipway_drop_method(self, dropped)
  M.slipway_drop(self, dropped)
end

local function slipway_draw_overlay_method(self, screen_pos)
  M.slipway_draw_overlay(self)
end

local function slipway_release_method(self)
  local jc = self
  local zone_ = jc.transit_dots.zone

  jc.transit_dots.count = 0
  grab_arena.grab_arena_give_back_if_placing(zone_, jc.state)
end

local function slipway_is_endpoint_class(cls)
  return (cls.states[entity_class.ENTITY_STATE_SETTLED] ~= nil or cls == require("game.planet").planet_class())
    and 1 or 0
end

M.slipway_class_def = {
  draw = slipway_display.slipway_draw_method,
  draw_overlay = slipway_draw_overlay_method,
  drag = slipway_drag_method,
  drop = slipway_drop_method,
  release = slipway_release_method,
  states = {},
  tags = entity_class.TAG(entity_class.TAG_SLIPWAY),
  lineage = 0,
  skip_draw_off_screen = 0,
}

local function slipway_create(keys)
  local self

  grab_arena.grab_arena_open(M.SLIPWAY_CONSTRUCTION_BYTES)
  self = allocator.allocator_alloc(grab_arena.grab_arena_construction_zone(), SIZEOF.slipway_t)

  entity.entity_init_instance(self, M.slipway_class_def, game_data.slipway_archetype.state,
    game_data.slipway_archetype.state_time)
  -- Lua only: the inline sub-structs the C writes into in place.
  self.targeting = targeting_module.targeting_zero()
  self.served_planets = { count = 0, ids = {} }
  self.point = {}
  self.archetype = game_data.slipway_archetype
  entity_registry.entity_registry_set_layer(self, self.archetype.layer)
  self.targeting.archetype = self.archetype.targeting
  self.fade_in_step = self.archetype.fade_in_step
  self.start = entity_id_none()
  self.transit_dots = list_zero()
  self.served_planets.count = 0
  self.end_ = entity_id_none()
  self.end_is_point = 0
  self.targeting.cost = self.archetype.targeting.cost
  self.targeting.has_cost = self.archetype.targeting.has_cost
  self.infraspace = 0
  require("game.probe").targeting_set_target(self.targeting, nil)
  self.targeted_by = entity_id_none()
  self.has = bit.bor(self.has, entity_class.ENTITY_HAS_IS_DEAD)
  self.is_dead = 0
  entity_class.entity_init_header(self.point, M.slipway_mouse_point_class_def)
  entity_class.entity_set_map_pos(self.point, vec.vec_make(fixed.p8_fixed_from_int(0), fixed.p8_fixed_from_int(0)))
  if keys ~= nil and entity_id.entity_get(keys.start) ~= nil then
    assert(slipway_is_endpoint_class(entity_id.entity_get(keys.start).class) ~= 0,
      "slipway_create: a start key that is neither NULL nor a planet or a structure "
        .. "does not fit the member's type -- stop, do not widen the member")
    self.start = keys.start
  end
  M.slipway_construct(self)
  grab_arena.grab_arena_close()
  entity.entity_construct(self)
  return self
end

function M.slipway_init()
  M.slipway_shutdown()
  g_slipway_up = 1
end

function M.slipway_shutdown()
  g_slipway_up = 0
end

function M.slipway_ready()
  return g_slipway_up
end

function M.slipway_class()
  return M.slipway_class_def
end

function M.slipway_new(keys)
  return slipway_create(keys)
end

function M.slipway_too_far_class()
  return M.slipway_too_far_class_def
end

function M.slipway_mouse_point_class()
  return M.slipway_mouse_point_class_def
end

-- Lua only: slipway_keys_t keys = {0}.
function M.slipway_keys_zero()
  return { start = entity_id_none() }
end

return M
