-- Copy of src/game/planet_family.h then src/game/planet_family.c.
--
-- Shapes, with the C field names:
--   burst_keys_t: { planet, construct }, planet a planet_keys_t, construct a burst construct keys
--     table of game_data or nil.
--   entity_kind_t: { new_entity, record }, record the game_data kind record table.
--   planet_kinds: 0-indexed array of { class_name, kind }.
--
-- Include cycle: game.planet, game.planet_display, game.planet_trade and game.probe are required
-- at load, since their methods fill the class tables; game.probe's targeting methods are held by
-- identity (probe.c compares a class's drag with targeting_track_method), so this module does not
-- load until game.probe exists. game.scroll and game.particle are required at call time.
local bit = require("bit")
local dispatch_context = require("engine.dispatch_context")
local entity = require("engine.entity")
local entity_class = require("engine.entity_class")
local entity_registry = require("engine.entity_registry")
local vec = require("engine.vec")
local game_data = require("game.game_data")
local planet = require("game.planet")
local planet_display = require("game.planet_display")
local planet_trade = require("game.planet_trade")
local probe = require("game.probe")
local targeting = require("game.targeting")
local visual_random = require("game.visual_random")
local fixed = require("p8.fixed")
local gfx_module = require("p8.gfx")

local M = {}

-- burst_keys_t keys = {0}.
local function burst_keys_zero()
  return { planet = planet.planet_keys_zero(), construct = nil }
end

local g_family_up = false

function M.structure_update_settled_method(self, t)
  planet.planet_bob(self, t)
end

M.structure_class_def = {
  draw = planet_display.planet_draw_method,
  skip_draw_off_screen = true,
  draw_overlay = probe.targeting_tooltip_method,
  hit_test = planet.planet_hit_test_method,
  new_year = planet.planet_new_year_method,
  refresh = planet_display.planet_refresh_method,
  release = planet_display.planet_release_method,
  left_press = planet_display.planet_left_press_method,
  accept_drop = planet_display.planet_accept_drop_method,
  drag = probe.targeting_track_method,
  drop = probe.targeting_confirm_method,
  attach_slipway = planet_trade.planet_attach_slipway_method,
  offer_trade = planet_trade.planet_offer_trade_method,
  states = {
    [entity_class.ENTITY_STATE_VANISHING] = planet.planet_update_vanishing_method,
    [entity_class.ENTITY_STATE_SETTLED] = M.structure_update_settled_method,
  },
  tags = bit.bor(entity_class.TAG(entity_class.TAG_STRUCTURE), entity_class.TAG(entity_class.TAG_PLANET),
    entity_class.TAG(entity_class.TAG_NODE)),
  lineage = bit.bor(entity_class.LINEAGE(entity_class.LINEAGE_PLANET), entity_class.LINEAGE(entity_class.LINEAGE_STRUCTURE)),
}

function M.structure_init_instance(self, cls, archetype)
  planet.planet_init_instance(self, cls, archetype)
  self.targeting = targeting.targeting_zero()
  self.targeting.archetype = self.archetype.targeting
  self.targeting.cost = self.archetype.targeting.cost
  self.sprite = self.archetype.sprite
  probe.targeting_set_target(self.targeting, nil)
end

local function structure_create(keys)
  local self = {}

  assert(g_family_up, "planet_family_init() must run first")
  M.structure_init_instance(self, M.structure_class_def, game_data.structure_archetype)
  assert(keys == nil or (keys.map_pos == nil and keys.pos == nil),
    "structure_create: the only construction key is planet_id (structure_icon_left_press); another key has no member "
    .. "-- stop, do not add one")
  planet.planet_copy_keys(self, keys)
  planet.planet_construct(self)
  entity.entity_construct(self)
  return self
end

function M.trading_hub_update_settled(self, t)
  if self == nil then
    return
  end
  planet.planet_bob(self, t)

  assert(self.development_tier ~= nil,
    "trading_hub_update_settled: a settled trading hub has a development tier; only ascension_draw checks for none")
  if self.development_tier ~= nil and self.development_tier >= fixed.p8_fixed_from_int(2) then
    self.palette_block =
      fixed.p8_fixed_add(fixed.p8_fixed_from_int(8),
        fixed.p8_fixed_div(fixed.p8_fixed_mod(t, fixed.p8_fixed_from_int(42)), fixed.p8_fixed_from_int(6)))
  end
end

local function trading_hub_update_settled_method(self, t)
  M.trading_hub_update_settled(self, t)
end

-- True when e is settled and its settled update calls planet_bob.
function M.planet_family_bobs(e)
  if e == nil or e.state ~= entity_class.ENTITY_STATE_SETTLED or e.class == nil then
    return false
  end
  local settled = e.class.states[entity_class.ENTITY_STATE_SETTLED]
  return settled == M.structure_update_settled_method or settled == trading_hub_update_settled_method
end

M.trading_hub_class_def = {
  draw = planet_display.planet_draw_method,
  skip_draw_off_screen = true,
  draw_overlay = probe.targeting_tooltip_method,
  hit_test = planet.planet_hit_test_method,
  new_year = planet.planet_new_year_method,
  refresh = planet_display.planet_refresh_method,
  release = planet_display.planet_release_method,
  left_press = planet_display.planet_left_press_method,
  accept_drop = planet_display.planet_accept_drop_method,
  drag = probe.targeting_track_method,
  drop = probe.targeting_confirm_method,
  attach_slipway = planet_trade.planet_attach_slipway_method,
  offer_trade = planet_trade.planet_offer_trade_method,
  states = {
    [entity_class.ENTITY_STATE_VANISHING] = planet.planet_update_vanishing_method,
    [entity_class.ENTITY_STATE_SETTLED] = trading_hub_update_settled_method,
  },
  tags = bit.bor(entity_class.TAG(entity_class.TAG_PLANET), entity_class.TAG(entity_class.TAG_NODE)),
  lineage = bit.bor(entity_class.LINEAGE(entity_class.LINEAGE_PLANET), entity_class.LINEAGE(entity_class.LINEAGE_STRUCTURE),
    entity_class.LINEAGE(entity_class.LINEAGE_TRADING_HUB)),
}

local function trading_hub_create(keys)
  local self = {}

  assert(g_family_up, "planet_family_init() must run first")
  M.structure_init_instance(self, M.trading_hub_class_def, game_data.trading_hub_archetype)
  self.palette_block = self.archetype.palette_block
  assert(keys == nil or (keys.map_pos == nil and keys.pos == nil),
    "trading_hub_create: the only construction key is planet_id (structure_icon_left_press); another key has no member "
    .. "-- stop, do not add one")
  planet.planet_copy_keys(self, keys)
  planet.planet_construct(self)
  entity.entity_construct(self)
  return self
end

M.processor_class_def = {
  draw = planet_display.planet_draw_method,
  skip_draw_off_screen = true,
  draw_overlay = probe.targeting_tooltip_method,
  hit_test = planet.planet_hit_test_method,
  new_year = planet.planet_new_year_method,
  refresh = planet_display.planet_refresh_method,
  release = planet_display.planet_release_method,
  left_press = planet_display.planet_left_press_method,
  accept_drop = planet_display.planet_accept_drop_method,
  drag = probe.targeting_track_method,
  drop = probe.targeting_confirm_method,
  attach_slipway = planet_trade.planet_attach_slipway_method,
  offer_trade = planet_trade.planet_offer_trade_method,
  states = {
    [entity_class.ENTITY_STATE_VANISHING] = planet.planet_update_vanishing_method,
    [entity_class.ENTITY_STATE_SETTLED] = M.structure_update_settled_method,
  },
  tags = bit.bor(entity_class.TAG(entity_class.TAG_PLANET), entity_class.TAG(entity_class.TAG_NODE)),
  lineage = bit.bor(entity_class.LINEAGE(entity_class.LINEAGE_PLANET), entity_class.LINEAGE(entity_class.LINEAGE_STRUCTURE)),
}

local function processor_create(keys)
  local self = {}

  assert(g_family_up, "planet_family_init() must run first")
  M.structure_init_instance(self, M.processor_class_def, game_data.processor_archetype)
  assert(keys == nil or (keys.map_pos == nil and keys.pos == nil),
    "processor_create: the only construction key is planet_id (structure_icon_left_press); another key has no member "
    .. "-- stop, do not add one")
  planet.planet_copy_keys(self, keys)
  planet.planet_construct(self)
  entity.entity_construct(self)
  return self
end

function M.ascension_draw(gfx, mem, self, p)
  if self == nil then
    return
  end
  if self.development_tier ~= nil and self.development_tier >= fixed.p8_fixed_from_int(2) then
    require("game.particle").emitter_update(gfx, mem, self)
  end
  planet_display.planet_draw(gfx, mem, self, p)
end

function M.ascension_draw_method(self, screen_pos)
  M.ascension_draw(dispatch_context.dispatch_context_gfx(), dispatch_context.dispatch_context_mem(), self, screen_pos)
end

function M.ascension_release_method(self)
  local particle = require("game.particle")

  planet_display.planet_release_row(self)
  particle.emitter_release_particles(particle.emitter_of(self))
end

M.ascension_class_def = {
  draw = M.ascension_draw_method,
  skip_draw_off_screen = false,
  draw_overlay = probe.targeting_tooltip_method,
  hit_test = planet.planet_hit_test_method,
  new_year = planet.planet_new_year_method,
  refresh = planet_display.planet_refresh_method,
  release = M.ascension_release_method,
  left_press = planet_display.planet_left_press_method,
  accept_drop = planet_display.planet_accept_drop_method,
  drag = probe.targeting_track_method,
  drop = probe.targeting_confirm_method,
  attach_slipway = planet_trade.planet_attach_slipway_method,
  offer_trade = planet_trade.planet_offer_trade_method,
  spawn_particle = probe.probe_spawn_particle,
  states = {
    [entity_class.ENTITY_STATE_VANISHING] = planet.planet_update_vanishing_method,
    [entity_class.ENTITY_STATE_SETTLED] = M.structure_update_settled_method,
  },
  tags = bit.bor(entity_class.TAG(entity_class.TAG_ASCENSION), entity_class.TAG(entity_class.TAG_PLANET),
    entity_class.TAG(entity_class.TAG_NODE)),
  lineage = bit.bor(entity_class.LINEAGE(entity_class.LINEAGE_PLANET), entity_class.LINEAGE(entity_class.LINEAGE_STRUCTURE)),
}

function M.ascension_init_instance(self, cls, archetype)
  M.structure_init_instance(self, cls, archetype)
  self.emitter = require("game.particle").emitter_zero()
  self.emitter.archetype = self.archetype.emitter
  self.emitter.speed_factor = self.archetype.emitter.speed_factor
  self.emitter.angle_slices = self.archetype.emitter.angle_slices
  self.emitter.trail_colors = self.archetype.emitter.trail_colors
  self.emitter.base_radius = self.archetype.emitter.base_radius
  self.emitter.matrix_xx = self.archetype.emitter.matrix_xx
  self.emitter.matrix_xy = self.archetype.emitter.matrix_xy
  self.emitter.matrix_yx = self.archetype.emitter.matrix_yx
  self.emitter.matrix_yy = self.archetype.emitter.matrix_yy
end

local function ascension_create(keys)
  local self = {}

  assert(g_family_up, "planet_family_init() must run first")
  M.ascension_init_instance(self, M.ascension_class_def, game_data.ascension_archetype)
  assert(keys == nil or (keys.map_pos == nil and keys.pos == nil),
    "ascension_create: the only construction key is planet_id (structure_icon_left_press); another key has no member "
    .. "-- stop, do not add one")
  planet.planet_copy_keys(self, keys)
  require("game.particle").emitter_init(self)
  planet.planet_construct(self)
  entity.entity_construct(self)
  return self
end

function M.protostar_construct(self)
  local keys = burst_keys_zero()
  local spawned

  if self == nil then
    return
  end

  keys.planet.pos = vec.vec_add(entity_class.entity_pos(self), self.archetype.spawn_offset)
  keys.construct = game_data.protostar_burst_construct_keys

  spawned = M.burst_new(keys)
end

M.protostar_class_def = {
  draw = M.ascension_draw_method,
  skip_draw_off_screen = false,
  draw_overlay = probe.targeting_tooltip_method,
  hit_test = planet.planet_hit_test_method,
  new_year = planet.planet_new_year_method,
  refresh = planet_display.planet_refresh_method,
  release = M.ascension_release_method,
  left_press = planet_display.planet_left_press_method,
  accept_drop = planet_display.planet_accept_drop_method,
  drag = probe.targeting_track_method,
  drop = probe.targeting_confirm_method,
  attach_slipway = planet_trade.planet_attach_slipway_method,
  offer_trade = planet_trade.planet_offer_trade_method,
  spawn_particle = probe.probe_spawn_particle,
  states = {
    [entity_class.ENTITY_STATE_VANISHING] = planet.planet_update_vanishing_method,
    [entity_class.ENTITY_STATE_SETTLED] = M.structure_update_settled_method,
  },
  tags = bit.bor(entity_class.TAG(entity_class.TAG_PLANET), entity_class.TAG(entity_class.TAG_NODE)),
  lineage = bit.bor(entity_class.LINEAGE(entity_class.LINEAGE_PLANET), entity_class.LINEAGE(entity_class.LINEAGE_STRUCTURE)),
}

local function protostar_create(keys)
  local self = {}

  assert(g_family_up, "planet_family_init() must run first")
  M.ascension_init_instance(self, M.protostar_class_def, game_data.protostar_archetype)
  planet.planet_copy_keys(self, keys)
  M.protostar_construct(self)
  require("game.particle").emitter_init(self)
  planet.planet_construct(self)
  entity.entity_construct(self)
  return self
end

function M.burst_construct(self)
  local mo

  if self == nil then
    return
  end
  local particle = require("game.particle")
  particle.emitter_init(self)
  particle.emitter_spawn(self, fixed.p8_fixed_from_int(140))

  if entity_registry.main_camera() ~= nil then
    mo = require("game.scroll").main_camera_typed()
    assert(mo ~= nil, "burst_construct: the camera placeholder has no shake; a burst before reset_world() stops the run")
    mo.shake = fixed.p8_fixed_from_int(3)
  end
end

function M.burst_spawn_particle(self_ptr)
  local self = self_ptr
  local qd
  local ms
  local iu
  local py
  local p
  local v
  local a
  local particle

  qd = fixed.p8_fixed_add(visual_random.visual_random_draw(fixed.P8_FIXED_LIT(0x00018000, 1.5)),
    fixed.P8_FIXED_LIT(0x00008000, 0.5))
  if visual_random.visual_random_draw_one() < fixed.P8_FIXED_LIT(0x00009999, 0.6) then
    qd = fixed.p8_fixed_mul(fixed.p8_fixed_flr(fixed.p8_fixed_div(qd, fixed.P8_FIXED_LIT(0x00008000, 0.5))),
      fixed.P8_FIXED_LIT(0x00008000, 0.5))
  end
  p = vec.vec_from_polar(qd, visual_random.visual_random_draw_one())

  ms = self.speed
  iu = self.acceleration_factor
  py = self.emitter.speed_factor

  v = vec.vec_mul(p, ms)
  a = vec.vec_mul(p, iu)

  particle = require("game.particle").particle_take()
  particle.pos = p
  particle.velocity = v
  particle.acceleration = a
  particle.life = fixed.p8_fixed_add(visual_random.visual_random_draw(py), py)

  return particle
end

function M.burst_draw(graphics, memory, self, screen_pos)
  local particle = require("game.particle")
  local particles = nil
  local ramp
  local third_taken = false
  local screen_x
  local screen_y

  if self == nil then
    return
  end
  ramp = self.emitter.trail_colors
  assert(ramp ~= nil, "a burst always holds a trail ramp: the archetype's default or its construct keys'")

  particles = self.emitter.particles

  screen_x = fixed.p8_fixed_flr_int(screen_pos.x)
  screen_y = fixed.p8_fixed_flr_int(screen_pos.y)

  for index = 1, 3 do
    local pt = nil

    if particles ~= nil and index <= #particles then
      pt = particles[index]
    end

    if pt ~= nil then
      local idx
      local color = 0
      local trail_color
      local found

      idx = fixed.p8_fixed_flr_int(fixed.p8_fixed_flr(fixed.p8_fixed_div(pt.life, fixed.p8_fixed_from_int(2))))
      found, trail_color = game_data.int_seq_at(ramp.items, ramp.count, idx, trail_color)
      if found then
        color = trail_color
      end
      if not particle.particle_pixels_hidden() then
        gfx_module.p8_circ(graphics, memory, screen_x, screen_y, fixed.p8_fixed_flr_int(vec.vec_length(pt.pos)), color)
      end
    end

    -- A flag, not the particle: emitter_update may give it back before is_dead is written.
    third_taken = pt ~= nil
  end

  particle.emitter_update(graphics, memory, self)
  self.is_dead = not third_taken
end

local function burst_draw_method(self, screen_pos)
  M.burst_draw(dispatch_context.dispatch_context_gfx(), dispatch_context.dispatch_context_mem(), self, screen_pos)
end

local function burst_release_method(self)
  M.ascension_release_method(self)
end

M.burst_class_def = {
  draw = burst_draw_method,
  skip_draw_off_screen = false,
  hit_test = planet.planet_hit_test_method,
  new_year = planet.planet_new_year_method,
  refresh = planet_display.planet_refresh_method,
  release = burst_release_method,
  left_press = planet_display.planet_left_press_method,
  accept_drop = planet_display.planet_accept_drop_method,
  attach_slipway = planet_trade.planet_attach_slipway_method,
  offer_trade = planet_trade.planet_offer_trade_method,
  spawn_particle = M.burst_spawn_particle,
  states = { [entity_class.ENTITY_STATE_VANISHING] = planet.planet_update_vanishing_method },
  tags = 0,
  lineage = entity_class.LINEAGE(entity_class.LINEAGE_PLANET),
}

local function burst_create(keys)
  local self = {}

  assert(g_family_up, "planet_family_init() must run first")
  planet.planet_init_instance(self, M.burst_class_def, game_data.burst_archetype)
  self.emitter = require("game.particle").emitter_zero()
  self.emitter.archetype = self.archetype.emitter
  self.emitter.matrix_xx = self.archetype.emitter.matrix_xx
  self.emitter.matrix_xy = self.archetype.emitter.matrix_xy
  self.emitter.matrix_yx = self.archetype.emitter.matrix_yx
  self.emitter.matrix_yy = self.archetype.emitter.matrix_yy
  self.emitter.trail_colors = self.archetype.emitter.trail_colors
  self.acceleration_factor = self.archetype.acceleration_factor
  self.emitter.speed_factor = self.archetype.emitter.speed_factor
  self.speed = self.archetype.speed
  self.outgoing_routes = {}
  if keys ~= nil then
    planet.planet_copy_keys(self, keys.planet)
    if keys.construct ~= nil then
      self.emitter.matrix_xx = keys.construct.matrix_xx
      self.emitter.matrix_xy = keys.construct.matrix_xy
      self.emitter.matrix_yx = keys.construct.matrix_yx
      self.emitter.matrix_yy = keys.construct.matrix_yy
      self.emitter.trail_colors = keys.construct.trail_colors
      self.acceleration_factor = keys.construct.acceleration_factor
      self.emitter.speed_factor = keys.construct.speed_factor
      self.speed = keys.construct.speed
    end
  end
  M.burst_construct(self)
  planet.planet_construct_in(self)
  entity.entity_construct(self)
  return self
end

function M.planet_family_init()
  assert(planet.planet_ready(), "planet_init() must run first")
  M.planet_family_shutdown()
  g_family_up = true
end

function M.planet_family_shutdown()
  g_family_up = false
end

function M.structure_ready()
  return g_family_up
end

local function structure_kind_new(keys)
  return structure_create(keys)
end

M.structure_kind_def = { new_entity = structure_kind_new, record = game_data.structure_kind_record }

function M.structure_kind()
  return M.structure_kind_def
end

function M.structure_class()
  return M.structure_class_def
end

function M.structure_new(keys)
  return structure_create(keys)
end

function M.trading_hub_ready()
  return g_family_up
end

local function trading_hub_kind_new(keys)
  return trading_hub_create(keys)
end

M.trading_hub_kind_def = { new_entity = trading_hub_kind_new, record = game_data.trading_hub_kind_record }

function M.trading_hub_kind()
  return M.trading_hub_kind_def
end

function M.trading_hub_class()
  return M.trading_hub_class_def
end

function M.trading_hub_new(keys)
  return trading_hub_create(keys)
end

function M.processor_ready()
  return g_family_up
end

local function processor_kind_new(keys)
  return processor_create(keys)
end

M.processor_kind_def = { new_entity = processor_kind_new, record = game_data.processor_kind_record }

function M.processor_kind()
  return M.processor_kind_def
end

function M.processor_class()
  return M.processor_class_def
end

function M.processor_new(keys)
  return processor_create(keys)
end

function M.ascension_ready()
  return g_family_up
end

local function ascension_kind_new(keys)
  return ascension_create(keys)
end

M.ascension_kind_def = { new_entity = ascension_kind_new, record = game_data.ascension_kind_record }

function M.ascension_kind()
  return M.ascension_kind_def
end

function M.ascension_class()
  return M.ascension_class_def
end

function M.ascension_new(keys)
  return ascension_create(keys)
end

function M.protostar_ready()
  return g_family_up
end

local function protostar_kind_new(keys)
  return protostar_create(keys)
end

M.protostar_kind_def = { new_entity = protostar_kind_new, record = game_data.protostar_kind_record }

function M.protostar_kind()
  return M.protostar_kind_def
end

function M.protostar_class()
  return M.protostar_class_def
end

function M.protostar_new(keys)
  return protostar_create(keys)
end

function M.burst_ready()
  return g_family_up
end

-- burst_keys.planet = *keys: positions copied, the id string shared.
local function burst_kind_new(keys)
  local burst_keys = {}

  if keys == nil then
    return burst_create(nil)
  end
  burst_keys.planet = {
    planet_id = keys.planet_id,
    map_pos = keys.map_pos and vec.vec_make(keys.map_pos.x, keys.map_pos.y),
    pos = keys.pos and vec.vec_make(keys.pos.x, keys.pos.y),
  }
  burst_keys.construct = nil
  return burst_create(burst_keys)
end

M.burst_kind_def = { new_entity = burst_kind_new, record = game_data.burst_kind_record }

function M.burst_kind()
  return M.burst_kind_def
end

function M.burst_class()
  return M.burst_class_def
end

function M.burst_new(keys)
  return burst_create(keys)
end

M.planet_kinds = {
  [0] = { class_name = "protostar", kind = M.protostar_kind },
  { class_name = "burst", kind = M.burst_kind },
}
local PLANET_KINDS_COUNT = 2
assert(M.planet_kinds[PLANET_KINDS_COUNT - 1] ~= nil and M.planet_kinds[PLANET_KINDS_COUNT] == nil,
  "planet_kinds: PLANET_KINDS_COUNT is the C sizeof planet_kinds / sizeof planet_kinds[0]")

function M.planet_kind_named(fz)
  for i = 0, PLANET_KINDS_COUNT - 1 do
    if M.planet_kinds[i].class_name == fz then
      return M.planet_kinds[i].kind()
    end
  end
  return nil
end

return M
