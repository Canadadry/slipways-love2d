-- Include cycle: game.wormhole_display and game.probe are required at load, since
-- wormhole_display.draw_method and targeting_tooltip_method fill wormhole.class_def; both must reach this
-- module at call time only. game.planet is required at call time, inside the function that needs it.
local entity = require("engine.entity")
local entity_class = require("engine.entity_class")
local entity_geom = require("engine.entity_geom")
local entity_id = require("engine.entity_id")
local entity_registry = require("engine.entity_registry")
local vec = require("engine.vec")
local game_data = require("game.game_data")
local probe = require("game.probe")
local targeting_module = require("game.targeting")
local tech_gate = require("game.tech_gate")
local wormhole_display = require("game.wormhole_display")
local fixed = require("p8.fixed")

local M = {}

local g_wormhole_up = false

M.too_far_target_class_def = {
  states = {},
  tags = 0,
  lineage = 0,
  skip_draw_off_screen = false,
}

M.g_wormhole_too_far_target_wrapper = {}
entity_class.init_header(M.g_wormhole_too_far_target_wrapper, M.too_far_target_class_def)
M.g_wormhole_too_far_target_wrapper.status_text = game_data.wormhole_too_far_target_status_text

local function wormhole_is_anchor(p)
  return p ~= nil and not p.is_dead and p.fold_anchor ~= nil
end

local function wormhole_anchor_count()
  local live = entity_registry.live_entities()
  local n = 0

  for i = 1, #live do
    if wormhole_is_anchor(live[i]) then
      n = n + 1
    end
  end
  return n
end

function M.construct(self)
  local ox
  local live
  local n

  if self == nil then
    return
  end

  n = wormhole_anchor_count()
  ox = self.anchors
  for i = #ox, 1, -1 do
    ox[i] = nil
  end
  live = entity_registry.live_entities()
  for i = 1, #live do
    local p = live[i]
    if wormhole_is_anchor(p) then
      ox[#ox + 1] = entity_id.link(p)
    end
  end
  assert(#ox == n)
end

function M.drag(self, mouse)
  local p_vec
  local nearest = nil
  local nq
  local threshold

  probe.targeting_track(self, mouse)
  if self == nil then
    return
  end

  p_vec = entity_class.map_pos(self)
  nq, nearest = entity_geom.nearest_in_ids(p_vec, self.anchors, nearest)
  self.anchor = entity_id.link(nearest)

  threshold = tech_gate.if_unlocked("space folding", fixed.LIT(0x000030a3, 0.19),
    fixed.LIT(0x0000451e, 0.27))

  if nq > threshold then
    probe.targeting_set_target(self.targeting, M.too_far_target_wrapper())
  end
end

local function wormhole_drag_method(self, mouse)
  M.drag(self, mouse)
end

function M.update_placing(self, t)
  local threshold
  local one
  local reach
  local min_val
  local sin_term
  local result

  if self == nil then
    return
  end

  threshold = tech_gate.if_unlocked("space folding", fixed.from_int(29),
    fixed.from_int(35))

  one = fixed.from_int(1)
  reach = fixed.div(t, fixed.from_int(15))
  if reach < one then
    min_val = reach
  else
    min_val = one
  end
  sin_term = fixed.mul(fixed.sin(fixed.div(t, fixed.from_int(40))),
    fixed.LIT(0x00001999, 0.1))

  result = fixed.mul(threshold, fixed.add(min_val, sin_term))
  self.radius = result
end

local function wormhole_update_placing_method(self, t)
  M.update_placing(self, t)
end

function M.drop(self)
  local t
  local result

  if self == nil then
    return
  end

  t = self.state_time

  -- targeting_confirm's side effects must not run before tick 15: do not hoist it out of the else.
  if t < fixed.from_int(15) then
    result = true
  else
    result = not probe.targeting_confirm(self)
  end

  self.is_dead = result
end

local function wormhole_drop_method(self, dropped)
  M.drop(self)
end

function M.update_unfolding(self, t)
  local planet = require("game.planet")
  local sr
  local self_p
  local planet_list

  if self == nil then
    return
  end

  sr = fixed.add(fixed.div(M.reveal_radius(self, t), fixed.from_int(128)),
    fixed.LIT(0x000007ef, 0.031))

  self_p = entity_class.map_pos(self)
  planet_list = entity_registry.entities_by_tag(entity_class.TAG_PLANET)
  if planet_list ~= nil then
    for i = 1, #planet_list do
      local e = planet_list[i]
      local pt

      if e ~= nil then
        assert(planet.is_family(e), "wormhole.update_unfolding: the planet tag holds planets only")
        pt = e

        if pt.state == entity_class.STATE_UNKNOWN then
          local planet_map_pos = entity_class.map_pos(pt)
          local diff = vec.sub(planet_map_pos, self_p)
          if vec.length(diff) < sr then
            planet.roll_type(pt)
          end
        end
      end
    end
  end

  self.is_dead = t > fixed.from_int(25)
end

local function wormhole_update_unfolding_method(self, t)
  M.update_unfolding(self, t)
end

function M.reveal_radius(self, t)
  local sq = fixed.sqrt(fixed.div(t, fixed.from_int(15)))
  local one = fixed.from_int(1)
  local min_val
  local oi = fixed.from_int(0)

  if sq < one then
    min_val = sq
  else
    min_val = one
  end
  if self ~= nil then
    oi = self.radius
  end
  return fixed.mul(min_val, oi)
end

M.class_def = {
  draw = wormhole_display.draw_method,
  draw_overlay = probe.targeting_tooltip_method,
  -- Not targeting_track_method: wormhole.drag adds the nearest-anchor search and the too-far target.
  drag = wormhole_drag_method,
  drop = wormhole_drop_method,
  states = {
    [entity_class.STATE_PLACING] = wormhole_update_placing_method,
    [entity_class.STATE_UNFOLDING] = wormhole_update_unfolding_method,
  },
  tags = entity_class.TAG(entity_class.TAG_WORMHOLE),
  lineage = 0,
  skip_draw_off_screen = false,
}

local function wormhole_create()
  local self

  self = {}

  entity.init_instance(self, M.class_def, game_data.wormhole_archetype.state,
    game_data.wormhole_archetype.state_time)
  self.targeting = targeting_module.zero()
  self.archetype = game_data.wormhole_archetype
  entity_registry.set_layer(self, self.archetype.layer)
  self.targeting.archetype = self.archetype.targeting
  self.radius = self.archetype.radius
  self.targeting.cost = self.archetype.targeting.cost
  self.anchors = {}
  probe.targeting_set_target(self.targeting, nil)
  self.anchor = nil
  self.is_dead = false
  M.construct(self)
  entity.construct(self)
  return self
end

function M.init()
  M.shutdown()
  g_wormhole_up = true
end

function M.shutdown()
  g_wormhole_up = false
end

function M.ready()
  return g_wormhole_up
end

function M.class()
  return M.class_def
end

function M.new()
  return wormhole_create()
end

-- The entities in the wormhole tag list, a dying one included until the exit queue flushes; 0 when
-- the list does not exist.
function M.live_count()
  local wormholes = entity_registry.entities_by_tag(entity_class.TAG_WORMHOLE)
  if wormholes ~= nil then
    return #wormholes
  end
  return 0
end

function M.too_far_target_wrapper()
  return M.g_wormhole_too_far_target_wrapper
end

function M.too_far_target_class()
  return M.too_far_target_class_def
end

return M
