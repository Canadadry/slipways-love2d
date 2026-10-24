-- An entity's header: { class, state, state_time, layer, pos, map_pos, hit_box,
--     infraspace, shake, fold_anchor, is_dead, is_hovered, id }, held in the
--     game object's own table. Every field but
--     class, state, state_time and is_dead is nil when the entity lacks it.
--     infraspace and fold_anchor are true or nil, never false: a slipway
--     compares two infraspaces.
-- A class table: { draw, draw_overlay, hit_test, new_year, refresh,
--     has_refresh, left_press, accept_drop, drag, drop, attach_slipway,
--     offer_trade, draw_at, spawn_particle, states (keyed by entity_class.STATE_*, from 0,
--     STATE_COUNT entries), tags, lineage, apply_camera, release, skip_draw_off_screen }.
--     skip_draw_off_screen true: draw_layers calls draw only when
--     entity_geom.is_on_screen holds, so draw may trust it.
-- A left_press answer: { stop, held } (held an entity id).
-- A mouse position: { world, screen, has }.
-- A kind: { new_entity, record }.
-- pos and map_pos are owned by the entity, nil when absent: set copies them
-- in, get copies them out, because game code changes self.pos.y in place.
local bit_module = require("bit")
local fixed = require("p8.fixed")
local vec = require("engine.vec")

local M = {}

M.STATE_NONE = 0
M.STATE_NORMAL = 1
M.STATE_SETTLED = 2
M.STATE_VANISHING = 3
M.STATE_PLACING = 4
M.STATE_UNFOLDING = 5
M.STATE_UNKNOWN = 6
M.STATE_KNOWN = 7
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

-- Writes class, state, state_time, is_dead and id only: every other field,
-- an optional header part included, stays as it was.
function M.init_header(self, cls)
  self.class = cls
  self.state = 0
  self.state_time = 0
  self.is_dead = false
  self.id = nil
end

function M.has_pos(self)
  return self.pos ~= nil
end

function M.has_map_pos(self)
  return self.map_pos ~= nil
end

-- The zero vector when the position is absent.
function M.pos(self)
  if self.pos ~= nil then
    return vec.make(self.pos.x, self.pos.y)
  end
  return vec.make(fixed.from_int(0), fixed.from_int(0))
end

-- The zero vector when the map position is absent.
function M.map_pos(self)
  if self.map_pos ~= nil then
    return vec.make(self.map_pos.x, self.map_pos.y)
  end
  return vec.make(fixed.from_int(0), fixed.from_int(0))
end

function M.set_pos(self, pos)
  self.pos = vec.make(pos.x, pos.y)
end

function M.clear_pos(self)
  self.pos = nil
end

function M.set_map_pos(self, map_pos)
  self.map_pos = vec.make(map_pos.x, map_pos.y)
end

function M.clear_map_pos(self)
  self.map_pos = nil
end

M.MOUSE_POS_HAS_WORLD = bit_module.lshift(1, 0)
M.MOUSE_POS_HAS_SCREEN = bit_module.lshift(1, 1)

function M.mouse_pos_has_world(mouse)
  return bit_module.band(mouse.has, M.MOUSE_POS_HAS_WORLD) ~= 0
end

function M.mouse_pos_has_screen(mouse)
  return bit_module.band(mouse.has, M.MOUSE_POS_HAS_SCREEN) ~= 0
end

-- The zero vector when the world point is absent.
function M.mouse_pos_world(mouse)
  if M.mouse_pos_has_world(mouse) then
    return vec.make(mouse.world.x, mouse.world.y)
  end
  return vec.make(fixed.from_int(0), fixed.from_int(0))
end

-- The zero vector when the screen point is absent.
function M.mouse_pos_screen(mouse)
  if M.mouse_pos_has_screen(mouse) then
    return vec.make(mouse.screen.x, mouse.screen.y)
  end
  return vec.make(fixed.from_int(0), fixed.from_int(0))
end

return M
