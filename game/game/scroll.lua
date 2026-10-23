-- Copy of src/game/scroll.h then src/game/scroll.c.
--
-- Shapes, with the C field names:
--   camera_t: one table holding the entity header and archetype (the game_data camera_archetype
--     table).
--
-- Include cycle: game.galaxy is required at call time (camera_pad_bound reads its map constants).
local bit = require("bit")
local allocator = require("engine.allocator")
local dispatch_context = require("engine.dispatch_context")
local entity = require("engine.entity")
local entity_class = require("engine.entity_class")
local entity_geom = require("engine.entity_geom")
local entity_registry = require("engine.entity_registry")
local vec = require("engine.vec")
local game_data = require("game.game_data")
local cint = require("p8.cint")
local fixed = require("p8.fixed")
local gfx_module = require("p8.gfx")
local vram = require("p8.vram")

local M = {}

M.SIZEOF = {
  camera_t = 88,
}
local SIZEOF = M.SIZEOF

-- The camera's map position is its position before shake; its position is the shaken one.
function M.camera_pos_before_shake(self)
  return entity_class.entity_map_pos(self)
end

local g_camera_up = 0

local function camera_update_method(self, t)
  M.camera_update(self)
end

local function camera_apply_method(self, parallax)
  M.camera_apply(dispatch_context.dispatch_context_gfx(), self, parallax)
end

M.camera_class_def = {
  states = { [entity_class.ENTITY_STATE_NORMAL] = camera_update_method },
  apply_camera = camera_apply_method,
  tags = 0,
  lineage = 0,
  skip_draw_off_screen = 0,
}

local function camera_create()
  local self = allocator.allocator_alloc(nil, SIZEOF.camera_t)

  entity.entity_init_instance(self, M.camera_class_def, game_data.camera_archetype.state,
    game_data.camera_archetype.state_time)
  self.archetype = game_data.camera_archetype
  self.has = bit.bor(self.has, entity_class.ENTITY_HAS_SHAKE)
  self.shake = self.archetype.shake
  entity_class.entity_set_map_pos(self, self.archetype.pos_before_shake)
  entity_class.entity_set_pos(self, self.archetype.pos)
  entity.entity_construct(self)
  return self
end

function M.camera_init()
  M.camera_shutdown()
  assert(entity.entity_ready() ~= 0, "camera_init: entity_init() runs first")
  g_camera_up = 1
end

function M.camera_shutdown()
  g_camera_up = 0
end

function M.camera_ready()
  return g_camera_up
end

function M.camera_class()
  return M.camera_class_def
end

function M.camera_new()
  return camera_create()
end

-- The pad has no mouse to reach the screen's edges: the bound keeps the screen centre over the galaxy's planet area.
local function camera_pad_bound(p, frame_size)
  local galaxy = require("game.galaxy")
  local scale = fixed.p8_fixed_from_int(galaxy.GALAXY_MAP_SCALE)
  local half = fixed.p8_fixed_from_int(cint.div(frame_size, 2))
  local near = fixed.p8_fixed_sub(fixed.p8_fixed_mul(galaxy.GALAXY_MAP_MARGIN, scale), half)
  local far = fixed.p8_fixed_sub(
    fixed.p8_fixed_mul(fixed.p8_fixed_add(galaxy.GALAXY_MAP_MARGIN, galaxy.GALAXY_MAP_SPAN), scale), half)

  return fixed.p8_fixed_mid(near, far, p)
end

local function camera_bounded(p)
  return vec.vec_make(camera_pad_bound(p.x, vram.P8_SCREEN_WIDTH), camera_pad_bound(p.y, vram.P8_SCREEN_HEIGHT))
end

function M.camera_update(self)
  local p

  if self == nil then
    return
  end

  p = camera_bounded(M.camera_pos_before_shake(self))
  entity_class.entity_set_map_pos(self, p)
  entity_class.entity_set_pos(self, camera_bounded(entity_geom.entity_geom_shake(p, self)))
end

-- Moves the map position and the position by whole pixels, within the camera bound.
function M.camera_pan(self, dx, dy)
  local step = vec.vec_make(fixed.p8_fixed_from_int(dx), fixed.p8_fixed_from_int(dy))

  assert(self ~= nil, "camera_pan: the caller checks for the placeholder camera")
  entity_class.entity_set_map_pos(self, camera_bounded(vec.vec_add(M.camera_pos_before_shake(self), step)))
  entity_class.entity_set_pos(self, camera_bounded(vec.vec_add(entity_class.entity_pos(self), step)))
end

function M.camera_apply(gfx, self, parallax)
  local cp

  if self == nil or gfx == nil then
    return
  end

  cp = vec.vec_mul(entity_class.entity_pos(self), parallax)
  gfx_module.p8_camera(gfx, fixed.p8_fixed_flr_int(cp.x), fixed.p8_fixed_flr_int(cp.y))
end

function M.main_camera_typed()
  local mo = entity_registry.main_camera()

  if mo == nil or mo.class == entity_registry.main_camera_placeholder_class() then
    return nil
  end
  assert(mo.class == M.camera_class(), "main_camera_typed: only the placeholder or a camera is ever stored")
  return mo
end

return M
