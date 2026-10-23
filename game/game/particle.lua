-- Copy of src/game/particle.h then src/game/particle.c.
--
-- Shapes, with the C field names:
--   particle_t: { pos, velocity, has_acceleration, acceleration, life }.
--   emitter_t: { archetype, has, angle_slices, base_radius, speed_factor, trail_colors, matrix_xx,
--     matrix_xy, matrix_yx, matrix_yy, particles }, the `emitter` field of its
--     host (a probe, a help screen or a planet), particles a pointer_list_t.
--   p8_point_t: { x, y, col }, the batch a 0-indexed array of them.
--
-- Include cycle: game.planet is required at call time (emitter_of). A probe is reached through
-- LINEAGE_PROBE and its `emitter` field only, so game.probe is not required.
local bit = require("bit")
local dispatch_context = require("engine.dispatch_context")
local entity_class = require("engine.entity_class")
local entity_geom = require("engine.entity_geom")
local pointer_list = require("engine.pointer_list")
local vec = require("engine.vec")
local game_data = require("game.game_data")
local visual_random = require("game.visual_random")
local fixed = require("p8.fixed")
local gfx_module = require("p8.gfx")

local M = {}

M.EMITTER_HAS_PARTICLES = bit.lshift(1, 0)

M.PARTICLES_RESERVE = 158
M.PARTICLES_HELP_SCREEN_RESERVE = 653

local EMITTER_POINT_BATCH = 32

local g_pixels_hidden = 0

-- Off by default; only devtools turns it on. Hides the pixels of emitter
-- particles, burst rings and transit dots, and nothing else: their state and
-- every random draw are unchanged.
function M.particle_hide_pixels(hidden)
  g_pixels_hidden = (hidden ~= 0) and 1 or 0
end

function M.particle_pixels_hidden()
  return g_pixels_hidden
end

-- A class property, not the position bit: a probe dragged without a mouse position carries a position that is absent.
local function emitter_host_carries_pos(self)
  return (bit.band(self.class.lineage,
    bit.bor(entity_class.LINEAGE(entity_class.LINEAGE_PROBE), entity_class.LINEAGE(entity_class.LINEAGE_PLANET))) ~= 0)
    and 1 or 0
end

local function emitter_host_layer_pos(self)
  assert(entity_class.entity_has(self, entity_class.ENTITY_HAS_LAYER) ~= 0 and emitter_host_carries_pos(self) ~= 0,
    "emitter_update: a host that carries no layer or no pos, none of whose draw runs it -- stop")
  return self.layer, entity_class.entity_pos(self)
end

local function particle_to_int(x)
  return fixed.p8_fixed_flr_int(x)
end

function M.particle_take()
  return {}
end

-- Points into the host: a probe, a help screen or a planet that carries one;
-- nil for any other class. Valid while the host lives: never store it.
function M.emitter_of(e)
  local planet = require("game.planet")

  if bit.band(e.class.lineage, entity_class.LINEAGE(entity_class.LINEAGE_PROBE)) ~= 0 then
    return e.emitter
  end
  if planet.planet_is_family(e) ~= 0 then
    if bit.band(e.has_, planet.PLANET_HAS_EMITTER) ~= 0 then
      return e.emitter
    end
    return nil
  end
  return nil
end

function M.emitter_init(self)
  M.emitter_init_in(self, M.PARTICLES_RESERVE)
end

-- The reserve sets the list's first capacity, and so when it grows.
function M.emitter_init_in(self, reserve)
  local emitter

  if self == nil then
    return
  end
  emitter = M.emitter_of(self)
  assert(emitter ~= nil, "emitter_init_in: a class that carries no particle emitter has no particle list -- stop")

  M.emitter_release_particles(emitter)
  pointer_list.pointer_list_init_reserve(emitter.particles, nil, reserve)
  emitter.has = bit.bor(emitter.has, M.EMITTER_HAS_PARTICLES)
end

function M.emitter_release_particles(emitter)
  local qy = emitter.particles

  if bit.band(emitter.has, M.EMITTER_HAS_PARTICLES) == 0 then
    return
  end
  qy.items = nil
  qy.count = 0
  qy.capacity = 0
  qy.zone = nil
  emitter.has = bit.band(bit.band(emitter.has, bit.bnot(M.EMITTER_HAS_PARTICLES)), 0xFFFF)
end

function M.emitter_spawn(self, n)
  local emitter
  local qy

  if self == nil then
    return
  end
  emitter = M.emitter_of(self)
  qy = nil
  if emitter ~= nil and bit.band(emitter.has, M.EMITTER_HAS_PARTICLES) ~= 0 then
    qy = emitter.particles
  end

  while visual_random.visual_random_draw_one() < n do
    if qy ~= nil and self.class.spawn_particle ~= nil then
      pointer_list.pointer_list_add(qy, self.class.spawn_particle(self))
    end
    n = fixed.p8_fixed_sub(n, fixed.p8_fixed_from_int(1))
  end
end

function M.emitter_update(graphics, memory, self)
  local emitter
  local layer
  local screen_pos
  local center_x
  local center_y
  local matrix_xx
  local matrix_yx
  local matrix_xy
  local matrix_yy
  local colors
  local particles

  if self == nil then
    return
  end
  emitter = M.emitter_of(self)
  assert(emitter ~= nil, "emitter_update: a class that carries no particle emitter is no receiver -- stop")

  layer, screen_pos = emitter_host_layer_pos(self)
  if layer < fixed.p8_fixed_from_int(10) and entity_geom.entity_geom_is_on_screen(self) == 0 then
    return
  end

  center_x = screen_pos.x
  center_y = screen_pos.y
  matrix_xx = emitter.matrix_xx
  matrix_yx = emitter.matrix_yx
  matrix_xy = emitter.matrix_xy
  matrix_yy = emitter.matrix_yy
  colors = emitter.trail_colors

  if bit.band(emitter.has, M.EMITTER_HAS_PARTICLES) ~= 0 then
    local points = {}
    local point_count = 0
    local index = 0
    particles = emitter.particles
    while index < particles.count do
      local particle = particles.items[index]
      local pv

      if particle ~= nil then
        local velocity_x
        pv = vec.vec_make(particle.velocity.x, particle.velocity.y)

        particle.pos = vec.vec_add(particle.pos, pv)

        velocity_x = fixed.p8_fixed_add(fixed.p8_fixed_mul(matrix_xx, pv.x), fixed.p8_fixed_mul(matrix_xy, pv.y))
        -- Reads the x just written, not pv.x: "fixing" it moves every particle whose matrix_yx is not 0.
        particle.velocity = vec.vec_make(velocity_x,
          fixed.p8_fixed_add(fixed.p8_fixed_mul(matrix_yx, velocity_x), fixed.p8_fixed_mul(matrix_yy, pv.y)))

        if particle.has_acceleration ~= 0 then
          particle.velocity = vec.vec_add(particle.velocity, particle.acceleration)
        end

        particle.life = fixed.p8_fixed_sub(particle.life, fixed.P8_FIXED_LIT(0x000023d7, 0.14))

        if particle.life <= fixed.p8_fixed_from_int(1) then
          -- Removing an element during this walk skips the next one; it is visited next frame. Compensating the index would change update timing and every replay.
          pointer_list.pointer_list_del(particles, particle)
        else
          local px = particle_to_int(fixed.p8_fixed_add(center_x, particle.pos.x))
          local py = particle_to_int(fixed.p8_fixed_add(center_y, particle.pos.y))
          local col = 0
          local idx = particle_to_int(fixed.p8_fixed_flr(particle.life))

          if colors ~= nil then
            local found, value = game_data.int_seq_at(colors.items, colors.count, idx, col)
            if found ~= 0 then
              col = value
            end
          end

          if g_pixels_hidden == 0 then
            points[point_count] = { x = px, y = py, col = col }
            point_count = point_count + 1
            if point_count == EMITTER_POINT_BATCH then
              gfx_module.p8_psets(graphics, memory, points, point_count)
              point_count = 0
            end
          end
        end
      end
      index = index + 1
    end
    if point_count > 0 then
      gfx_module.p8_psets(graphics, memory, points, point_count)
    end
  end

  M.emitter_spawn(self, emitter.archetype.spawn_rate)
end

function M.emitter_update_method(self, screen_pos)
  M.emitter_update(dispatch_context.dispatch_context_gfx(), dispatch_context.dispatch_context_mem(), self)
end

-- Lua only: emitter_t at its C zero, the inline sub-struct later code writes field by field.
function M.emitter_zero()
  return {
    archetype = nil,
    has = 0,
    angle_slices = 0,
    base_radius = 0,
    speed_factor = 0,
    trail_colors = nil,
    matrix_xx = 0,
    matrix_xy = 0,
    matrix_yx = 0,
    matrix_yy = 0,
    particles = { items = nil, count = 0, capacity = 0, zone = nil },
  }
end

return M
