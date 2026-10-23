-- Copy of src/engine/entity_geom.h then src/engine/entity_geom.c.
-- Include cycle with engine.entity_registry: each requires the other at call
-- time only (entity_geom_is_on_screen here); neither needs a value of the
-- other at load.
local bit = require("bit")
local entity_class = require("engine.entity_class")
local entity_id = require("engine.entity_id")
local vec = require("engine.vec")
local geom = require("engine.geom")
local vram = require("p8.vram")
local cint = require("p8.cint")
local fixed = require("p8.fixed")

local M = {}

-- The body of entity_geom_contains_cursor, inline so the hover loop makes no call.
function M.entity_geom_contains_cursor_inline(e, mouse)
  local chosen
  local point

  if e == nil or e.hit_box == nil then
    return false
  end
  if e.layer ~= nil and e.layer >= fixed.P8_FIXED_INT(10) then
    chosen = entity_class.mouse_pos_screen(mouse)
  else
    chosen = entity_class.mouse_pos_world(mouse)
  end
  point = vec.vec_make(chosen.x, chosen.y)
  if e.pos ~= nil then
    point.x = bit.tobit(chosen.x - e.pos.x)
    point.y = bit.tobit(chosen.y - e.pos.y)
  end
  return geom.box_contains(e.hit_box, point)
end

local function zero_vec()
  return vec.vec_make(fixed.p8_fixed_from_int(0), fixed.p8_fixed_from_int(0))
end

local function pos_of(e)
  if e ~= nil then
    return entity_class.entity_pos(e)
  end
  return zero_vec()
end

local function map_pos_of(e)
  if e ~= nil then
    return entity_class.entity_map_pos(e)
  end
  return zero_vec()
end

function M.entity_geom_contains_cursor(e, mouse)
  return M.entity_geom_contains_cursor_inline(e, mouse)
end

function M.entity_geom_is_on_screen(e)
  local entity_registry = require("engine.entity_registry")
  local e_pos = pos_of(e)
  local main_camera_pos = pos_of(entity_registry.main_camera())
  local half_w = fixed.p8_fixed_from_int(cint.div(vram.P8_SCREEN_WIDTH, 2))
  local half_h = fixed.p8_fixed_from_int(cint.div(vram.P8_SCREEN_HEIGHT, 2))
  local margin = fixed.p8_fixed_from_int(12)

  return (fixed.p8_fixed_abs(fixed.p8_fixed_sub(fixed.p8_fixed_sub(e_pos.x, main_camera_pos.x), half_w)) <= fixed.p8_fixed_add(half_w, margin) and
          fixed.p8_fixed_abs(fixed.p8_fixed_sub(fixed.p8_fixed_sub(e_pos.y, main_camera_pos.y), half_h)) <= fixed.p8_fixed_add(half_h, margin))
end

function M.entity_geom_shake(p, e)
  local decremented
  local clamped
  local t = fixed.p8_fixed_from_int(0)

  if e ~= nil then
    assert(e.shake ~= nil,
      "entity_geom_shake: the entity is a planet (planet_draw) or the camera (camera_update)")
  end
  decremented = fixed.p8_fixed_sub((e ~= nil) and e.shake or fixed.p8_fixed_from_int(0), fixed.P8_FIXED_LIT(0x00001999, 0.1))
  clamped = (decremented > 0) and decremented or 0
  if e ~= nil then
    t = e.state_time
    e.shake = clamped
  end
  return vec.vec_add(p, vec.vec_from_polar(clamped, fixed.p8_fixed_mul(t, fixed.P8_FIXED_LIT(0x00003333, 0.2))))
end

local function nearest_step(p, o, best, out_nearest)
  local o_p = map_pos_of(o)
  local d = vec.vec_length_squared(vec.vec_sub(p, o_p))
  local same_position = (p.x == o_p.x and p.y == o_p.y)

  if not same_position and d < best then
    return d, o
  end
  return best, out_nearest
end

function M.entity_geom_nearest(p, j_, out_nearest)
  local mk = fixed.p8_fixed_from_int(32767)

  out_nearest = nil
  if j_ == nil then
    return mk, out_nearest
  end
  for i = 1, #j_ do
    mk, out_nearest = nearest_step(p, j_[i], mk, out_nearest)
  end
  return mk, out_nearest
end

-- Answers entity_geom_nearest(p, list, &n) <= threshold for any threshold under its 32767 bound; false for a NULL list.
function M.entity_geom_any_within(p, list, threshold)
  if list == nil then
    return false
  end
  for i = 1, #list do
    local o_p = map_pos_of(list[i])
    local same_position = (p.x == o_p.x and p.y == o_p.y)

    if not same_position and vec.vec_length_squared(vec.vec_sub(p, o_p)) <= threshold then
      return true
    end
  end
  return false
end

-- Skips only `excluded`, by identity: another entity on the very point counts.
function M.entity_geom_nearest_except(p, j_, excluded, out_nearest)
  local best = fixed.p8_fixed_from_int(32767)

  out_nearest = nil
  if j_ == nil then return best, out_nearest end
  for i = 1, #j_ do
    local o = j_[i]
    local d = vec.vec_length_squared(vec.vec_sub(p, map_pos_of(o)))
    if o ~= excluded and d < best then
      out_nearest = o
      best = d
    end
  end
  return best, out_nearest
end

function M.entity_geom_nearest_in_ids(p, ids, out_nearest)
  local best = fixed.p8_fixed_from_int(32767)

  out_nearest = nil
  if ids == nil then return best, out_nearest end
  for i = 1, #ids do
    best, out_nearest = nearest_step(p, entity_id.entity_id_resolve(ids[i]), best, out_nearest)
  end
  return best, out_nearest
end

return M
