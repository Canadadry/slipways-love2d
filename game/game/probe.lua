-- Include cycle: game.particle is required at load, since emitter_update_method fills
-- probe_class_def; particle never requires this module. game.economy, game.planet,
-- game.slipway, game.ui and game.wormhole are required at call time, inside the functions that need
-- them. planet_family and wormhole hold this module's methods by identity, so it requires neither.
local bit = require("bit")
local dispatch_context = require("engine.dispatch_context")
local entity = require("engine.entity")
local entity_class = require("engine.entity_class")
local entity_geom = require("engine.entity_geom")
local entity_id = require("engine.entity_id")
local entity_registry = require("engine.entity_registry")
local geom = require("engine.geom")
local vec = require("engine.vec")
local game_data = require("game.game_data")
local hud = require("game.hud")
local particle_module = require("game.particle")
local targeting_module = require("game.targeting")
local visual_random = require("game.visual_random")
local fixed = require("p8.fixed")
local gfx_module = require("p8.gfx")
local str = require("p8.str")

local M = {}

local g_probe_up = false
local MAIN_ECONOMY_NONE = 0
local MAIN_ECONOMY_PLACEHOLDER = 1
local MAIN_ECONOMY_REAL = 2

local g_main_economy_state = MAIN_ECONOMY_NONE
local g_main_economy = nil

M.main_economy_placeholder_class_def = {
  states = {},
  tags = 0,
  lineage = 0,
  skip_draw_off_screen = false,
}

-- research_points and credits stay nil until the cheat sets them.
M.g_main_economy_placeholder = {}
entity_class.init_header(M.g_main_economy_placeholder, M.main_economy_placeholder_class_def)

local function targeting_track_target_is_holder_class(cls)
  return cls.drag == M.targeting_track_method or cls == require("game.wormhole").wormhole_class()
end

M.targeting_track_target_class_def = {
  states = {},
  tags = 0,
  lineage = 0,
  skip_draw_off_screen = false,
}

M.g_targeting_track_target = {}
entity_class.init_header(M.g_targeting_track_target, M.targeting_track_target_class_def)
M.g_targeting_track_target.targeted_by = nil

local function probe_to_int(x)
  return fixed.flr_int(x)
end

local function targeting_track_slipway_predicate(e, ctx_ptr)
  local slipway = require("game.slipway")
  local ctx = ctx_ptr
  local sw
  local start_p
  local end_p

  if e == nil then
    return false
  end
  assert(e.class == slipway.slipway_class(), "the slipway tag holds slipways only")
  sw = e

  start_p = slipway.slipway_end_map_pos(slipway.slipway_start(sw))
  end_p = slipway.slipway_end_map_pos(slipway.slipway_end(sw))

  return geom.near_segment(start_p, end_p, ctx.self_map_pos, fixed.LIT(0x000000d1, 0.0032))
end

local function probe_panel_box_contains(rp)
  local hw = require("game.ui").ui_panel_box()

  if hw == nil then
    return false
  end
  return geom.box_contains(hw, rp)
end

local function probe_is_target_class(cls)
  return (cls.states[entity_class.STATE_SETTLED] ~= nil or cls == require("game.planet").class() or
    cls == require("game.slipway").slipway_class() or cls == M.targeting_track_target_class())
end

function M.probe_link_target(et, self)
  local planet = require("game.planet")
  local slipway = require("game.slipway")
  local wormhole = require("game.wormhole")

  if planet.is_family(et) then
    assert(self.class.drag == M.targeting_track_method or self.class == slipway.slipway_class(),
      "probe_link_target: a planet's targeted_by takes NULL, a host whose drag is targeting_track_method "
        .. "or a slipway, nothing else -- stop, do not widen the member")
    et.targeted_by = entity_id.link(self)
    return
  end
  if et.class == slipway.slipway_class() then
    assert(self.class.drag == M.targeting_track_method or self.class == slipway.slipway_class() or
      self.class == wormhole.wormhole_class(),
      "probe_link_target: a slipway's targeted_by takes NULL, a host whose drag is targeting_track_method, "
        .. "a wormhole or another slipway, nothing else -- stop")
    et.targeted_by = entity_id.link(self)
    return
  end
  if et.class == slipway.slipway_too_far_class() then
    assert(self.class == slipway.slipway_class(),
      "probe_link_target: the too-far slipway struct's targeted_by takes NULL or a slipway, nothing else "
        .. "-- stop, do not widen the member")
    et.targeted_by = entity_id.link(self)
    return
  end
  assert(et == M.g_targeting_track_target,
    "probe_link_target: the target is a planet, a slipway or the static throwaway struct, nothing else")
  assert(targeting_track_target_is_holder_class(self.class),
    "probe_link_target: the throwaway target's targeted_by takes NULL, a host whose drag is "
      .. "targeting_track_method or a wormhole, nothing else -- stop, do not widen the member")
  M.g_targeting_track_target.targeted_by = entity_id.link(self)
end

function M.targeting_target(self)
  local kind = self.target.kind

  if kind == targeting_module.TARGETING_TARGET_ENTITY then
    return entity_id.get(self.target.id)
  elseif kind == targeting_module.TARGETING_TARGET_TRACK_THROWAWAY then
    return M.g_targeting_track_target
  elseif kind == targeting_module.TARGETING_TARGET_SLIPWAY_TOO_FAR then
    return require("game.slipway").slipway_too_far()
  elseif kind == targeting_module.TARGETING_TARGET_WORMHOLE_TOO_FAR then
    return require("game.wormhole").wormhole_too_far_target_wrapper()
  end
  return nil
end

function M.targeting_set_target(self, target)
  local slipway = require("game.slipway")
  local wormhole = require("game.wormhole")
  local value = { kind = targeting_module.TARGETING_TARGET_NONE, id = nil }

  if target == M.g_targeting_track_target then
    value.kind = targeting_module.TARGETING_TARGET_TRACK_THROWAWAY
  elseif target == slipway.slipway_too_far() then
    value.kind = targeting_module.TARGETING_TARGET_SLIPWAY_TOO_FAR
  elseif target == wormhole.wormhole_too_far_target_wrapper() then
    value.kind = targeting_module.TARGETING_TARGET_WORMHOLE_TOO_FAR
  elseif target ~= nil then
    value.kind = targeting_module.TARGETING_TARGET_ENTITY
    value.id = entity_id.link(target)
  end
  self.target = value
end

function M.targeting_of(e)
  local planet_module = require("game.planet")

  if bit.band(e.class.lineage, entity_class.LINEAGE(entity_class.LINEAGE_PROBE)) ~= 0 then
    return e.targeting
  end
  if planet_module.is_family(e) then
    return e.targeting
  end
  if e.class == require("game.slipway").slipway_class() then
    return e.targeting
  end
  if e.class == require("game.wormhole").wormhole_class() then
    return e.targeting
  end
  return nil
end

function M.targeting_track(self, mouse)
  local targeting
  local mouse_world_vec
  local p_vec
  local distance
  local candidate
  local target

  if self == nil then
    return
  end
  targeting = M.targeting_of(self)
  assert(targeting ~= nil, "targeting_track: a class that carries no targeting is no receiver -- stop")

  mouse_world_vec = entity_class.mouse_pos_world(mouse)
  p_vec = vec.div(mouse_world_vec, fixed.from_int(128))
  assert(bit.band(self.class.lineage, entity_class.LINEAGE(entity_class.LINEAGE_PROBE)) ~= 0 or
    require("game.planet").is_family(self) or self.class == require("game.wormhole").wormhole_class(),
    "targeting_track: a host neither a probe, of the planet family nor a wormhole, none of whose drag runs it -- stop")
  if entity_class.mouse_pos_has_world(mouse) then
    entity_class.set_pos(self, mouse_world_vec)
  else
    entity_class.clear_pos(self)
  end
  entity_class.set_map_pos(self, p_vec)

  candidate = nil
  distance, candidate = entity_geom.nearest_except(p_vec,
    entity_registry.entities_by_tag(entity_class.TAG_NODE), self, candidate)

  target = nil

  if distance < targeting.archetype.attach_distance and candidate ~= nil then
    target = candidate
  else
    local ctx = {}
    local first = nil
    local slipways = entity_registry.entities_by_tag(entity_class.TAG_SLIPWAY)

    ctx.self_map_pos = p_vec
    if slipways ~= nil then
      for i = 1, #slipways do
        if targeting_track_slipway_predicate(slipways[i], ctx) then
          first = slipways[i]
          break
        end
      end
    end

    if first ~= nil then
      target = first
    else
      if not probe_panel_box_contains(entity_class.mouse_pos_screen(mouse)) then
        target = M.g_targeting_track_target
      else
        target = nil
      end
    end
  end

  assert(target == nil or probe_is_target_class(target.class),
    "targeting_track: the target is NULL or what it aims at (a planet, a class with a settled state, a slipway, "
      .. "or the static throwaway struct), nothing else")
  M.targeting_set_target(targeting, target)

  if M.targeting_target(targeting) ~= nil then
    M.probe_link_target(M.targeting_target(targeting), self)
  end
end

function M.targeting_track_method(self, mouse)
  M.targeting_track(self, mouse)
end

-- nil for a target that carries no status text.
local function probe_target_status_text(et)
  local slipway = require("game.slipway")
  local wormhole = require("game.wormhole")

  if et.class == wormhole.wormhole_too_far_target_class() then
    return et.status_text
  end
  if et.class == slipway.slipway_too_far_class() then
    return et.status_text
  end
  return nil
end

function M.targeting_tooltip(gfx, mem, self)
  if self == nil then
    return
  end
  assert(M.targeting_of(self) ~= nil, "targeting_tooltip: a class that carries no targeting is no receiver -- stop")

  M.targeting_tooltip_show(gfx, mem, self.state, M.targeting_of(self))
end

function M.targeting_tooltip_show(gfx, mem, state, pb)
  local mp
  local je
  local display = ""

  if state ~= entity_class.STATE_PLACING then
    return
  end

  je = require("game.ui").main_mouse_typed()
  if je ~= nil then
    mp = entity_class.pos(je)
  else
    mp = vec.make(fixed.from_int(0), fixed.from_int(0))
  end

  if M.targeting_target(pb) ~= nil then
    local qs = probe_target_status_text(M.targeting_target(pb))
    if qs ~= nil then
      display = qs
    else
      if gfx ~= nil and mem ~= nil then
        gfx_module.spr_default(gfx, mem, 31, probe_to_int(fixed.sub(mp.x, fixed.from_int(3))),
          probe_to_int(fixed.sub(mp.y, fixed.from_int(8))))
      end
    end
  elseif pb.cost ~= nil then
    display = str.concat2(str.tostr_fixed(pb.cost), "$")
  end

  if gfx ~= nil and mem ~= nil then
    local a = fixed.LIT(0x00008000, 0.5)
    hud.hud_print_outlined(gfx, mem, display, mp.x, fixed.sub(mp.y, fixed.from_int(8)), 8, a)
  end
end

function M.targeting_tooltip_method(self, screen_pos)
  M.targeting_tooltip(dispatch_context.gfx(), dispatch_context.mem(), self)
end

local function probe_host_raise_is_dead(self)
  self.is_dead = true
end

local function targeting_charge(self)
  local targeting
  local target
  local spend_ok = false

  if self == nil then
    return nil
  end
  targeting = M.targeting_of(self)
  assert(targeting ~= nil, "targeting_confirm: a class that carries no targeting is no receiver -- stop")

  target = M.targeting_target(targeting)

  if target == nil then
    local cost
    if targeting.cost ~= nil then
      cost = targeting.cost
    else
      cost = fixed.from_int(0)
    end
    spend_ok = require("game.economy").economy_spend(M.main_economy_typed(), cost)
  end

  assert(target == nil or not spend_ok,
    "targeting_confirm: economy_spend must stay short-circuited when a target is set")

  if target == nil and spend_ok then
    local confirmed = targeting.archetype.confirmed_state

    if confirmed ~= entity_class.STATE_NONE then
      entity.set_state(self, confirmed)
    else
      entity.set_state(self, entity_class.STATE_NORMAL)
    end
    return targeting
  end

  probe_host_raise_is_dead(self)
  return nil
end

function M.targeting_confirm(self)
  local economy = require("game.economy")
  local targeting = targeting_charge(self)

  if targeting == nil then
    return false
  end
  if targeting.archetype.clock_skips_recompute then
    economy.economy_tick_clock(M.main_economy_typed(), targeting.archetype.clock_ticks)
  else
    economy.economy_advance_clock(M.main_economy_typed(), targeting.archetype.clock_ticks)
  end
  return true
end

-- targeting_confirm without its clock advance: the caller advances the clock.
function M.targeting_confirm_without_clock(self)
  return targeting_charge(self) ~= nil
end

function M.targeting_confirm_method(self, dropped)
  M.targeting_confirm(self)
end

function M.probe_spawn_particle(self)
  local emitter = nil
  local angle_slices, state_time, base_radius, speed_factor, lifetime = 0, 0, 0, 0, 0
  local pk = false
  local a, rnd_angle_slices, rnd_half
  local p, v
  local particle

  if self ~= nil then
    emitter = particle_module.emitter_of(self)
    assert(emitter ~= nil, "probe_spawn_particle: a class that carries no particle emitter spawns nothing -- stop")
    angle_slices = emitter.angle_slices
    state_time = self.state_time
    base_radius = emitter.base_radius
    speed_factor = emitter.speed_factor
    lifetime = emitter.archetype.lifetime
    pk = emitter.archetype.quarter_turn
  end

  -- Two statements: rnd(angle_slices) must be drawn before rnd(0.5), or the replays move.
  rnd_angle_slices = visual_random.draw(angle_slices)
  rnd_half = visual_random.draw(fixed.LIT(0x00008000, 0.5))
  a = fixed.add(
    fixed.div(fixed.add(fixed.flr(rnd_angle_slices), rnd_half), angle_slices),
    fixed.mul(state_time, fixed.LIT(0x00000041, 0.001)))

  p = vec.from_polar(fixed.add(base_radius, visual_random.draw_one()), a)
  v = vec.mul(p, speed_factor)

  if pk then
    v = vec.perpendicular(v)
  end

  particle = particle_module.particle_take()
  particle.pos = p
  particle.velocity = v
  particle.life = lifetime

  return particle
end

function M.probe_release_method(self)
  particle_module.emitter_release_particles(particle_module.emitter_of(self))
end

M.probe_class_def = {
  draw = particle_module.emitter_update_method,
  draw_overlay = M.targeting_tooltip_method,
  drag = M.targeting_track_method,
  drop = M.targeting_confirm_method,
  spawn_particle = M.probe_spawn_particle,
  release = M.probe_release_method,
  states = {},
  tags = entity_class.TAG(entity_class.TAG_PROBE),
  lineage = entity_class.LINEAGE(entity_class.LINEAGE_PROBE),
  skip_draw_off_screen = false,
}

function M.probe_init_instance(self, cls, archetype)
  entity.init_instance(self, cls, archetype.state, archetype.state_time)
  self.emitter = particle_module.emitter_zero()
  self.targeting = targeting_module.targeting_zero()
  self.archetype = archetype
  entity_registry.set_layer(self, self.archetype.layer)
  self.fold_anchor = self.archetype.fold_anchor
  self.emitter.archetype = self.archetype.emitter
  if self.archetype.pos ~= nil then
    entity_class.set_pos(self, self.archetype.pos)
  end
  self.targeting.archetype = self.archetype.targeting
  M.targeting_set_target(self.targeting, nil)
  self.is_dead = false
  self.targeting.cost = self.archetype.targeting.cost
  self.emitter.speed_factor = self.archetype.emitter.speed_factor
  self.emitter.angle_slices = self.archetype.emitter.angle_slices
  self.emitter.trail_colors = self.archetype.emitter.trail_colors
  self.emitter.matrix_xx = self.archetype.emitter.matrix_xx
  self.emitter.matrix_xy = self.archetype.emitter.matrix_xy
  self.emitter.matrix_yx = self.archetype.emitter.matrix_yx
  self.emitter.matrix_yy = self.archetype.emitter.matrix_yy
  self.emitter.base_radius = self.archetype.emitter.base_radius
end

local function probe_create(keys)
  local self

  assert(g_probe_up, "probe_init() must run first")
  self = {}
  M.probe_init_instance(self, M.probe_class_def, game_data.probe_archetype)
  if keys ~= nil then
    self.state_time = keys.state_time
    if keys.angle_slices ~= nil then
      self.emitter.angle_slices = keys.angle_slices
    end
    if keys.base_radius ~= nil then
      self.emitter.base_radius = keys.base_radius
    end
    if keys.speed_factor ~= nil then
      self.emitter.speed_factor = keys.speed_factor
    end
  end
  particle_module.emitter_init(self)
  entity.construct(self)
  return self
end

function M.probe_init()
  M.probe_shutdown()

  g_probe_up = true

  M.g_main_economy_placeholder.research_points = nil
  M.g_main_economy_placeholder.credits = nil
  M.probe_set_main_economy(M.g_main_economy_placeholder)
end

function M.probe_shutdown()
  g_probe_up = false
  M.probe_set_main_economy(nil)
end

function M.probe_ready()
  return g_probe_up
end

function M.probe_class()
  return M.probe_class_def
end

function M.probe_new(keys)
  return probe_create(keys)
end

function M.main_economy_placeholder_class()
  return M.main_economy_placeholder_class_def
end

function M.main_economy()
  if g_main_economy_state == MAIN_ECONOMY_PLACEHOLDER then
    return M.g_main_economy_placeholder
  elseif g_main_economy_state == MAIN_ECONOMY_REAL then
    return entity_id.get(g_main_economy)
  end
  return nil
end

function M.main_economy_typed()
  local e = nil

  if g_main_economy_state == MAIN_ECONOMY_REAL then
    e = entity_id.get(g_main_economy)
  end
  assert(e == nil or e.class == require("game.economy").economy_class(),
    "main_economy_typed: only the placeholder or an economy is ever stored")
  return e
end

function M.main_economy_placeholder()
  if g_main_economy_state == MAIN_ECONOMY_PLACEHOLDER then
    return M.g_main_economy_placeholder
  end
  return nil
end

function M.probe_set_main_economy(instance)
  if instance == nil then
    g_main_economy_state = MAIN_ECONOMY_NONE
    g_main_economy = nil
  elseif instance == M.g_main_economy_placeholder then
    g_main_economy_state = MAIN_ECONOMY_PLACEHOLDER
    g_main_economy = nil
  else
    g_main_economy_state = MAIN_ECONOMY_REAL
    g_main_economy = entity_id.link(instance)
  end
end

function M.targeting_track_target_class()
  return M.targeting_track_target_class_def
end

return M
