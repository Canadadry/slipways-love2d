-- Copy of src/game/wormhole.h then src/game/wormhole.c.
--
-- Shapes, with the C field names:
--   wormhole_too_far_target_t: the entity header plus { status_text }, one table.
--   wormhole_t: one table holding the entity header and { archetype, has_, radius, anchors,
--     anchor, targeting }; the wormhole's own `has` (WORMHOLE_HAS_* bits) is `has_`, since the
--     header's `has` holds the ENTITY_HAS_* bits; targeting a targeting_t (game.targeting).
--
-- Declared in wormhole.h, defined in src/game/wormhole_display.c and ported there:
-- wormhole_draw_method.
--
-- Include cycle: game.wormhole_display and game.probe are required at load, since
-- wormhole_draw_method and targeting_tooltip_method fill wormhole_class_def; both must reach this
-- module at call time only. game.planet is required at call time, inside the function that needs it.
local bit = require("bit")
local entity = require("engine.entity")
local entity_class = require("engine.entity_class")
local entity_geom = require("engine.entity_geom")
local entity_id = require("engine.entity_id")
local entity_id_list = require("engine.entity_id_list")
local entity_id_type = require("engine.entity_id_type")
local entity_list = require("engine.entity_list")
local entity_registry = require("engine.entity_registry")
local vec = require("engine.vec")
local game_data = require("game.game_data")
local probe = require("game.probe")
local targeting_module = require("game.targeting")
local tech_gate = require("game.tech_gate")
local wormhole_display = require("game.wormhole_display")
local fixed = require("p8.fixed")

local M = {}

M.WORMHOLE_HAS_TOO_FAR_TARGET = bit.lshift(1, 0)

-- entity_id_list_t list = {0}.
local function id_list_zero()
  return { items = nil, count = 0, capacity = 0 }
end

local g_wormhole_up = 0

M.wormhole_too_far_target_class_def = {
  states = {},
  tags = 0,
  lineage = 0,
  skip_draw_off_screen = 0,
}

M.g_wormhole_too_far_target_wrapper = {}
entity_class.entity_init_header(M.g_wormhole_too_far_target_wrapper, M.wormhole_too_far_target_class_def)
M.g_wormhole_too_far_target_wrapper.status_text = game_data.wormhole_too_far_target_status_text

local function wormhole_is_anchor(p)
  return (p ~= nil and p.is_dead == 0 and entity_class.entity_has(p, entity_class.ENTITY_HAS_FOLD_ANCHOR) ~= 0 and p.fold_anchor ~= 0)
    and 1 or 0
end

local function wormhole_anchor_count()
  local it = entity_list.entity_list_all(entity_registry.live_entities())
  local p
  local n = 0
  local more

  while true do
    more, p = entity_list.entity_list_iter_next(it, p)
    if more == 0 then
      break
    end
    n = n + wormhole_is_anchor(p)
  end
  return n
end

function M.wormhole_construct(self)
  local ox
  local it
  local p
  local n
  local more

  if self == nil then
    return
  end

  n = wormhole_anchor_count()
  ox = self.anchors
  entity_id_list.entity_id_list_init_reserve(ox, n)
  it = entity_list.entity_list_all(entity_registry.live_entities())
  while true do
    more, p = entity_list.entity_list_iter_next(it, p)
    if more == 0 then
      break
    end
    if wormhole_is_anchor(p) ~= 0 then
      entity_id_list.entity_id_list_add(ox, entity_id.entity_id_link(p))
    end
  end
  assert(ox.count == n and ox.capacity == n)
end

function M.wormhole_drag(self, mouse)
  local p_vec
  local nearest = nil
  local nq
  local threshold

  probe.targeting_track(self, mouse)
  if self == nil then
    return
  end

  p_vec = entity_class.entity_map_pos(self)
  nq, nearest = entity_geom.entity_geom_nearest_in_ids(p_vec, self.anchors, nearest)
  self.anchor = entity_id.entity_id_link(nearest)

  threshold = tech_gate.tech_gate_if_unlocked("space folding", fixed.P8_FIXED_LIT(0x000030a3, 0.19),
    fixed.P8_FIXED_LIT(0x0000451e, 0.27))

  if nq > threshold then
    probe.targeting_set_target(self.targeting, M.wormhole_too_far_target_wrapper())
  end
end

local function wormhole_drag_method(self, mouse)
  M.wormhole_drag(self, mouse)
end

function M.wormhole_update_placing(self, t)
  local threshold
  local one
  local reach
  local min_val
  local sin_term
  local result

  if self == nil then
    return
  end

  threshold = tech_gate.tech_gate_if_unlocked("space folding", fixed.p8_fixed_from_int(29),
    fixed.p8_fixed_from_int(35))

  one = fixed.p8_fixed_from_int(1)
  reach = fixed.p8_fixed_div(t, fixed.p8_fixed_from_int(15))
  if reach < one then
    min_val = reach
  else
    min_val = one
  end
  sin_term = fixed.p8_fixed_mul(fixed.p8_fixed_sin(fixed.p8_fixed_div(t, fixed.p8_fixed_from_int(40))),
    fixed.P8_FIXED_LIT(0x00001999, 0.1))

  result = fixed.p8_fixed_mul(threshold, fixed.p8_fixed_add(min_val, sin_term))
  self.radius = result
end

local function wormhole_update_placing_method(self, t)
  M.wormhole_update_placing(self, t)
end

function M.wormhole_drop(self)
  local t
  local result

  if self == nil then
    return
  end

  t = self.state_time

  -- targeting_confirm's side effects must not run before tick 15: do not hoist it out of the else.
  if t < fixed.p8_fixed_from_int(15) then
    result = 1
  else
    result = probe.targeting_confirm(self) == 0 and 1 or 0
  end

  self.is_dead = result
end

local function wormhole_drop_method(self, dropped)
  M.wormhole_drop(self)
end

function M.wormhole_update_unfolding(self, t)
  local planet = require("game.planet")
  local sr
  local self_p
  local planet_list

  if self == nil then
    return
  end

  sr = fixed.p8_fixed_add(fixed.p8_fixed_div(M.wormhole_reveal_radius(self, t), fixed.p8_fixed_from_int(128)),
    fixed.P8_FIXED_LIT(0x000007ef, 0.031))

  self_p = entity_class.entity_map_pos(self)
  planet_list = entity_registry.entities_by_tag(entity_class.TAG_PLANET)
  if planet_list ~= nil then
    for i = 0, planet_list.count - 1 do
      local e = planet_list.items[i]
      local pt

      if e ~= nil then
        assert(planet.planet_is_family(e) ~= 0, "wormhole_update_unfolding: the planet tag holds planets only")
        pt = e

        if pt.state == entity_class.ENTITY_STATE_UNKNOWN then
          local planet_map_pos = entity_class.entity_map_pos(pt)
          local diff = vec.vec_sub(planet_map_pos, self_p)
          if vec.vec_length(diff) < sr then
            planet.planet_roll_type(pt)
          end
        end
      end
    end
  end

  self.is_dead = (t > fixed.p8_fixed_from_int(25)) and 1 or 0
end

local function wormhole_update_unfolding_method(self, t)
  M.wormhole_update_unfolding(self, t)
end

function M.wormhole_reveal_radius(self, t)
  local sq = fixed.p8_fixed_sqrt(fixed.p8_fixed_div(t, fixed.p8_fixed_from_int(15)))
  local one = fixed.p8_fixed_from_int(1)
  local min_val
  local oi = fixed.p8_fixed_from_int(0)

  if sq < one then
    min_val = sq
  else
    min_val = one
  end
  if self ~= nil then
    oi = self.radius
  end
  return fixed.p8_fixed_mul(min_val, oi)
end

M.wormhole_class_def = {
  draw = wormhole_display.wormhole_draw_method,
  draw_overlay = probe.targeting_tooltip_method,
  -- Not targeting_track_method: wormhole_drag adds the nearest-anchor search and the too-far target.
  drag = wormhole_drag_method,
  drop = wormhole_drop_method,
  states = {
    [entity_class.ENTITY_STATE_PLACING] = wormhole_update_placing_method,
    [entity_class.ENTITY_STATE_UNFOLDING] = wormhole_update_unfolding_method,
  },
  tags = entity_class.TAG(entity_class.TAG_WORMHOLE),
  lineage = 0,
  skip_draw_off_screen = 0,
}

local function wormhole_create()
  local self

  self = {}

  entity.entity_init_instance(self, M.wormhole_class_def, game_data.wormhole_archetype.state,
    game_data.wormhole_archetype.state_time)
  -- Lua only: the inline sub-struct the C writes into in place.
  self.targeting = targeting_module.targeting_zero()
  self.archetype = game_data.wormhole_archetype
  entity_registry.entity_registry_set_layer(self, self.archetype.layer)
  self.targeting.archetype = self.archetype.targeting
  self.radius = self.archetype.radius
  self.targeting.cost = self.archetype.targeting.cost
  self.targeting.has_cost = self.archetype.targeting.has_cost
  self.has_ = M.WORMHOLE_HAS_TOO_FAR_TARGET
  self.anchors = id_list_zero()
  probe.targeting_set_target(self.targeting, nil)
  self.anchor = { index = entity_id_type.ENTITY_ID_NONE.index, generation = entity_id_type.ENTITY_ID_NONE.generation }
  self.has = bit.bor(self.has, entity_class.ENTITY_HAS_IS_DEAD)
  self.is_dead = 0
  M.wormhole_construct(self)
  entity.entity_construct(self)
  return self
end

function M.wormhole_init()
  M.wormhole_shutdown()
  g_wormhole_up = 1
end

function M.wormhole_shutdown()
  g_wormhole_up = 0
end

function M.wormhole_ready()
  return g_wormhole_up
end

function M.wormhole_class()
  return M.wormhole_class_def
end

function M.wormhole_new()
  return wormhole_create()
end

-- The entities in the wormhole tag list, a dying one included until the exit queue flushes; 0 when
-- the list does not exist.
function M.wormhole_live_count()
  local wormholes = entity_registry.entities_by_tag(entity_class.TAG_WORMHOLE)
  if wormholes ~= nil then
    return wormholes.count
  end
  return 0
end

function M.wormhole_too_far_target_wrapper()
  return M.g_wormhole_too_far_target_wrapper
end

function M.wormhole_too_far_target_class()
  return M.wormhole_too_far_target_class_def
end

return M
