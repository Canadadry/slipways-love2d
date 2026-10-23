-- Copy of src/game/slipway_display.h then src/game/slipway_display.c.
--
-- Shapes, with the C field names:
--   transit_dot_t: { pos, velocity, life }.
--   slipway_line_ends_t: { start, end_ }, passed by value; `end` is `end_` (a Lua keyword).
--
-- Include cycle: game.slipway requires this module at load (slipway_draw_method fills
-- slipway_class_def), so game.slipway, game.planet and game.probe (home of targeting_target) are
-- required at call time, inside the functions that need them.
local dispatch_context = require("engine.dispatch_context")
local entity_class = require("engine.entity_class")
local entity_geom = require("engine.entity_geom")
local entity_id = require("engine.entity_id")
local vec = require("engine.vec")
local game_data = require("game.game_data")
local particle = require("game.particle")
local visual_random = require("game.visual_random")
local fixed = require("p8.fixed")
local gfx_module = require("p8.gfx")

local M = {}

local function slipway_line_ends_make(start, end_)
  return { start = start, end_ = end_ }
end

local function to_int(x)
  return fixed.p8_fixed_flr_int(x)
end

function M.slipway_spawn_transit_dot(self, f, t)
  local slipway = require("game.slipway")
  local planet = require("game.planet")
  local t_id

  if self == nil then
    return
  end
  -- The visual draw must stay before the served planets test, or the visual stream desynchronises.
  if visual_random.visual_random_draw_one() >= fixed.P8_FIXED_LIT(0x0000147a, 0.08) then
    return
  end

  -- The mouse point slipway_drag hangs the end on is not a planet.
  if t ~= nil and planet.planet_is_family(t) ~= 0 then
    t_id = planet.planet_id_or_null(t)
  else
    t_id = nil
  end
  if slipway.slipway_served_planets_has(self.served_planets, t_id) ~= 0 then
    local f_pos = slipway.slipway_end_pos(f)
    local t_pos = slipway.slipway_end_pos(t)
    local d = vec.vec_sub(t_pos, f_pos)
    local v = vec.vec_normalize(d)
    local dot = {}

    dot.pos = vec.vec_add(f_pos, vec.vec_perpendicular(v))
    dot.velocity = v
    dot.life = vec.vec_length(d)

    self.transit_dots[#self.transit_dots + 1] = dot
  end
end

function M.slipway_line_ends(self)
  local slipway = require("game.slipway")
  local t = self.state_time
  local start = vec.vec_add(slipway.slipway_end_pos(slipway.slipway_start(self)),
    vec.vec_from_polar(fixed.P8_FIXED_LIT(0x00018000, 1.5), fixed.p8_fixed_div(t, fixed.p8_fixed_from_int(126))))
  local end_ = vec.vec_add(slipway.slipway_end_pos(slipway.slipway_end(self)),
    vec.vec_from_polar(fixed.P8_FIXED_LIT(0x00018000, 1.5), fixed.p8_fixed_div(t, fixed.p8_fixed_from_int(176))))
  return slipway_line_ends_make(start, end_)
end

local function floored_far_from_the_wrap(p)
  local floored = fixed.p8_fixed_flr_int(p)
  assert(floored >= -32765 and floored <= 32765,
    "int adds of the offsets match p8_fixed_add only away from its +-32768 wrap")
  return floored
end

function M.slipway_draw_line(gfx, mem, ends, l, h, s, c)
  local x1
  local y1
  local x2
  local y2

  assert(s > 0, "slipway_draw_line only ascends: a zero step hangs, a negative one draws nothing")
  assert(gfx ~= nil and mem ~= nil,
    "p8_lines dereferences the graphics state and memory: neither may be NULL here")

  x1 = floored_far_from_the_wrap(ends.start.x)
  y1 = floored_far_from_the_wrap(ends.start.y)
  x2 = floored_far_from_the_wrap(ends.end_.x)
  y2 = floored_far_from_the_wrap(ends.end_.y)

  local bundle = {}
  for i = 0, 3 do
    bundle[i] = { x1 = 0, y1 = 0, x2 = 0, y2 = 0 }
  end
  local count = 0
  for dx = l, h, s do
    for dy = l, h, s do
      local segment = bundle[count]
      count = count + 1
      segment.x1 = x1 + dx
      segment.y1 = y1 + dy
      segment.x2 = x2 + dx
      segment.y2 = y2 + dy
      if count == 4 then
        gfx_module.p8_lines(gfx, mem, bundle, count, c)
        count = 0
      end
    end
  end
  if count > 0 then
    gfx_module.p8_lines(gfx, mem, bundle, count, c)
  end
end

function M.slipway_draw(graphics, memory, self)
  local slipway = require("game.slipway")
  local probe = require("game.probe")
  local start_node
  local end_node

  if self == nil then
    return
  end

  start_node = slipway.slipway_start(self)
  end_node = slipway.slipway_end(self)

  if entity_geom.entity_geom_is_on_screen(start_node) ~= 0 or entity_geom.entity_geom_is_on_screen(end_node) ~= 0 then
    local fade_in_step = self.fade_in_step
    local fade_in_step_i = to_int(fade_in_step)
    local fade_color = fixed.p8_fixed_from_int(0)
    local target_or_targeted_by
    local infraspace_truthy
    local line_color
    local border_color
    local dot_color

    assert(fade_in_step_i >= 1,
      "trail colors are 1-based: a fade-in step of 0 would silently draw the slipway black")

    if self.archetype.trail_colors ~= nil then
      local found, entry = game_data.int_seq_at(self.archetype.trail_colors.items, self.archetype.trail_colors.count,
        fade_in_step_i, nil)
      if found ~= 0 then
        fade_color = fixed.p8_fixed_from_int(entry)
      end
    end

    if fade_in_step < fixed.p8_fixed_from_int(20) then
      self.fade_in_step = fixed.p8_fixed_add(fade_in_step, fixed.p8_fixed_from_int(1))
    end

    target_or_targeted_by = probe.targeting_target(self.targeting) ~= nil or entity_id.entity_get(self.targeted_by) ~= nil
    infraspace_truthy = entity_class.entity_has(self, entity_class.ENTITY_HAS_INFRASPACE)

    if target_or_targeted_by then
      line_color = fixed.p8_fixed_from_int(2)
    elseif infraspace_truthy ~= 0 then
      line_color = fixed.p8_fixed_from_int(0)
    else
      line_color = fade_color
    end
    border_color = (infraspace_truthy ~= 0) and fade_color or fixed.p8_fixed_from_int(0)
    dot_color = (infraspace_truthy ~= 0) and fixed.p8_fixed_from_int(5) or fixed.p8_fixed_from_int(13)

    local ends = M.slipway_line_ends(self)
    M.slipway_draw_line(graphics, memory, ends, -1, 2, 3, to_int(border_color))
    M.slipway_draw_line(graphics, memory, ends, 0, 1, 1, to_int(line_color))

    do
      local ts = self.transit_dots
      local i = 1
      while i <= #ts do
        local t = ts[i]
        local new_pos
        local new_l

        if t == nil then
          goto continue
        end

        new_pos = vec.vec_add(t.pos, t.velocity)
        t.pos = new_pos

        new_l = fixed.p8_fixed_sub(t.life, fixed.p8_fixed_from_int(1))
        t.life = new_l

        -- No `else`: an expired particle is still drawn once more at its final position.
        if new_l <= fixed.p8_fixed_from_int(0) then
          -- Removing an element during this walk skips the next one; it is visited next frame. Compensating the index would change update timing and every replay.
          for j = 1, #ts do
            if ts[j] == t then
              table.remove(ts, j)
              break
            end
          end
        end

        if particle.particle_pixels_hidden() == 0 then
          local x = to_int(new_pos.x)
          local y = to_int(new_pos.y)
          gfx_module.p8_rectfill(graphics, memory, x, y, x + 1, y + 1, 0)
          gfx_module.p8_pset(graphics, memory, x, y, to_int(dot_color))
        end

        ::continue::
        i = i + 1
      end
    end

    M.slipway_spawn_transit_dot(self, start_node, end_node)
    M.slipway_spawn_transit_dot(self, end_node, start_node)
  end

  self.targeted_by = nil
end

function M.slipway_draw_method(self, screen_pos)
  M.slipway_draw(dispatch_context.dispatch_context_gfx(), dispatch_context.dispatch_context_mem(), self)
end

return M
