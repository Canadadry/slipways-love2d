-- Include cycle: game.galaxy is required at call time (camera_pad_bound reads its map constants).
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

-- The camera's map position is its position before shake; its position is the shaken one.
function M.camera_pos_before_shake(self)
  return entity_class.map_pos(self)
end

local g_camera_up = false

local function camera_update_method(self, t)
  M.camera_update(self)
end

local function camera_apply_method(self, parallax)
  M.camera_apply(dispatch_context.gfx(), self, parallax)
end

M.camera_class_def = {
  states = { [entity_class.STATE_NORMAL] = camera_update_method },
  apply_camera = camera_apply_method,
  tags = 0,
  lineage = 0,
  skip_draw_off_screen = false,
}

local function camera_create()
  local self = {}

  entity.init_instance(self, M.camera_class_def, game_data.camera_archetype.state,
    game_data.camera_archetype.state_time)
  self.archetype = game_data.camera_archetype
  self.shake = self.archetype.shake
  entity_class.set_map_pos(self, self.archetype.pos_before_shake)
  entity_class.set_pos(self, self.archetype.pos)
  entity.construct(self)
  return self
end

function M.camera_init()
  M.camera_shutdown()
  assert(entity.ready(), "camera_init: entity.init() runs first")
  g_camera_up = true
end

function M.camera_shutdown()
  g_camera_up = false
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
  local scale = fixed.from_int(galaxy.GALAXY_MAP_SCALE)
  local half = fixed.from_int(cint.div(frame_size, 2))
  local near = fixed.sub(fixed.mul(galaxy.GALAXY_MAP_MARGIN, scale), half)
  local far = fixed.sub(
    fixed.mul(fixed.add(galaxy.GALAXY_MAP_MARGIN, galaxy.GALAXY_MAP_SPAN), scale), half)

  return fixed.mid(near, far, p)
end

local function camera_bounded(p)
  return vec.make(camera_pad_bound(p.x, vram.SCREEN_WIDTH), camera_pad_bound(p.y, vram.SCREEN_HEIGHT))
end

function M.camera_update(self)
  local p

  if self == nil then
    return
  end

  p = camera_bounded(M.camera_pos_before_shake(self))
  entity_class.set_map_pos(self, p)
  entity_class.set_pos(self, camera_bounded(entity_geom.shake(p, self)))
end

-- Moves the map position and the position by whole pixels, within the camera bound.
function M.camera_pan(self, dx, dy)
  local step = vec.make(fixed.from_int(dx), fixed.from_int(dy))

  assert(self ~= nil, "camera_pan: the caller checks for the placeholder camera")
  entity_class.set_map_pos(self, camera_bounded(vec.add(M.camera_pos_before_shake(self), step)))
  entity_class.set_pos(self, camera_bounded(vec.add(entity_class.pos(self), step)))
end

function M.camera_apply(gfx, self, parallax)
  local cp

  if self == nil or gfx == nil then
    return
  end

  cp = vec.mul(entity_class.pos(self), parallax)
  gfx_module.camera(gfx, fixed.flr_int(cp.x), fixed.flr_int(cp.y))
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
