-- Copy of src/engine/entity_class.h.
-- slipway_class() and probe_class(), declared there, are defined in
-- src/game/slipway.c and src/game/probe.c and ported with those files.
--
-- Shapes, with the C field names (a pointer is the table, NULL is nil):
--   entity_t: { class, state, state_time, has, layer, pos, map_pos, hit_box,
--     infraspace, shake, fold_anchor, is_dead, is_hovered, id }; a game object
--     whose first member is the header is that same table.
--   entity_class_t: { draw, draw_overlay, hit_test, new_year, refresh,
--     has_refresh, left_press, accept_drop, drag, drop, attach_slipway,
--     offer_trade, draw_at, spawn_particle, states (indexed from 0, STATE_COUNT
--     entries), tags, lineage, apply_camera, release, skip_draw_off_screen }.
--     skip_draw_off_screen 1: draw_layers calls draw only when
--     entity_geom_is_on_screen holds, so draw may trust it.
--   down_t: { stop, held } (held an entity id).
--   mouse_pos_t: { world, screen, has }.
--   entity_kind_t: { new_entity, record }.
-- pos and map_pos are owned by the entity: set copies them in, get copies
-- them out, because game code changes self.pos.y in place.
local bit_module = require("bit")
local fixed = require("p8.fixed")
local vec = require("engine.vec")

local M = {}

M.ENTITY_STATE_NONE = 0
M.ENTITY_STATE_NORMAL = 1
M.ENTITY_STATE_SETTLED = 2
M.ENTITY_STATE_VANISHING = 3
M.ENTITY_STATE_PLACING = 4
M.ENTITY_STATE_UNFOLDING = 5
M.ENTITY_STATE_UNKNOWN = 6
M.ENTITY_STATE_KNOWN = 7
M.STATE_COUNT = 8

M.TAG_STRUCTURE = 0
M.TAG_ASCENSION = 1
M.TAG_SLIPGATE = 2
M.TAG_PROBE = 3
M.TAG_MESSAGE_BOX = 4
M.TAG_SLIPWAY = 5
M.TAG_WORMHOLE = 6
M.TAG_PLANET = 7
M.TAG_NODE = 8
M.TAG_COUNT = 9

function M.TAG(t)
  return bit_module.lshift(1, t)
end

M.LINEAGE_PLANET = 0
M.LINEAGE_STRUCTURE = 1
M.LINEAGE_TRADING_HUB = 2
M.LINEAGE_PROBE = 3

function M.LINEAGE(l)
  return bit_module.lshift(1, l)
end

M.ENTITY_HAS_LAYER = bit_module.lshift(1, 0)
M.ENTITY_HAS_HIT_BOX = bit_module.lshift(1, 1)
M.ENTITY_HAS_INFRASPACE = bit_module.lshift(1, 3)
M.ENTITY_HAS_FOLD_ANCHOR = bit_module.lshift(1, 4)
M.ENTITY_HAS_SHAKE = bit_module.lshift(1, 5)
M.ENTITY_HAS_IS_DEAD = bit_module.lshift(1, 6)
M.ENTITY_HAS_IS_HOVERED = bit_module.lshift(1, 7)
M.ENTITY_HAS_POS = bit_module.lshift(1, 8)
M.ENTITY_HAS_MAP_POS = bit_module.lshift(1, 9)

-- Writes the header fields only: the object's own fields stay, as the C
-- assignment writes the header bytes alone.
function M.entity_init_header(self, cls)
  self.class = cls
  self.state = 0
  self.state_time = 0
  self.has = 0
  self.layer = 0
  self.pos = vec.vec_make(0, 0)
  self.map_pos = vec.vec_make(0, 0)
  self.hit_box = { left = 0, top = 0, right = 0, bottom = 0 }
  self.infraspace = 0
  self.shake = 0
  self.fold_anchor = 0
  self.is_dead = 0
  self.is_hovered = 0
  self.id = nil
end

function M.entity_has(self, bit)
  return (bit_module.band(self.has, bit) ~= 0) and 1 or 0
end

function M.entity_has_pos(self)
  return M.entity_has(self, M.ENTITY_HAS_POS)
end

function M.entity_has_map_pos(self)
  return M.entity_has(self, M.ENTITY_HAS_MAP_POS)
end

-- The zero vector when the position is absent.
function M.entity_pos(self)
  if M.entity_has_pos(self) ~= 0 then
    return vec.vec_make(self.pos.x, self.pos.y)
  end
  return vec.vec_make(fixed.p8_fixed_from_int(0), fixed.p8_fixed_from_int(0))
end

-- The zero vector when the map position is absent.
function M.entity_map_pos(self)
  if M.entity_has_map_pos(self) ~= 0 then
    return vec.vec_make(self.map_pos.x, self.map_pos.y)
  end
  return vec.vec_make(fixed.p8_fixed_from_int(0), fixed.p8_fixed_from_int(0))
end

function M.entity_set_pos(self, pos)
  self.pos = vec.vec_make(pos.x, pos.y)
  self.has = bit_module.bor(self.has, M.ENTITY_HAS_POS)
end

function M.entity_clear_pos(self)
  self.has = bit_module.band(self.has, bit_module.band(bit_module.bnot(M.ENTITY_HAS_POS), 0xFFFF))
end

function M.entity_set_map_pos(self, map_pos)
  self.map_pos = vec.vec_make(map_pos.x, map_pos.y)
  self.has = bit_module.bor(self.has, M.ENTITY_HAS_MAP_POS)
end

function M.entity_clear_map_pos(self)
  self.has = bit_module.band(self.has, bit_module.band(bit_module.bnot(M.ENTITY_HAS_MAP_POS), 0xFFFF))
end

M.MOUSE_POS_HAS_WORLD = bit_module.lshift(1, 0)
M.MOUSE_POS_HAS_SCREEN = bit_module.lshift(1, 1)

function M.mouse_pos_has_world(mouse)
  return (bit_module.band(mouse.has, M.MOUSE_POS_HAS_WORLD) ~= 0) and 1 or 0
end

function M.mouse_pos_has_screen(mouse)
  return (bit_module.band(mouse.has, M.MOUSE_POS_HAS_SCREEN) ~= 0) and 1 or 0
end

-- The zero vector when the world point is absent.
function M.mouse_pos_world(mouse)
  if M.mouse_pos_has_world(mouse) ~= 0 then
    return vec.vec_make(mouse.world.x, mouse.world.y)
  end
  return vec.vec_make(fixed.p8_fixed_from_int(0), fixed.p8_fixed_from_int(0))
end

-- The zero vector when the screen point is absent.
function M.mouse_pos_screen(mouse)
  if M.mouse_pos_has_screen(mouse) ~= 0 then
    return vec.vec_make(mouse.screen.x, mouse.screen.y)
  end
  return vec.vec_make(fixed.p8_fixed_from_int(0), fixed.p8_fixed_from_int(0))
end

return M
