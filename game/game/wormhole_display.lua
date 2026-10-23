-- Include cycle: game.wormhole requires this module at load (wormhole_draw_method fills
-- wormhole_class_def), so game.wormhole and game.probe (home of targeting_target) are required at
-- call time, inside the function that needs them.
local dispatch_context = require("engine.dispatch_context")
local entity_class = require("engine.entity_class")
local entity_id = require("engine.entity_id")
local game_data = require("game.game_data")
local fixed = require("p8.fixed")
local gfx_module = require("p8.gfx")

local M = {}

function M.wormhole_draw(graphics, memory, self, screen_pos)
  local wormhole = require("game.wormhole")
  local probe = require("game.probe")
  local screen_x
  local screen_y

  if self == nil then
    return
  end

  screen_x = fixed.flr_int(screen_pos.x)
  screen_y = fixed.flr_int(screen_pos.y)

  if self.state == entity_class.ENTITY_STATE_PLACING then
    local anchor = entity_id.entity_get(self.anchor)

    if probe.targeting_target(self.targeting) == nil then
      local oi = self.radius
      if graphics ~= nil and memory ~= nil then
        gfx_module.circ(graphics, memory, screen_x, screen_y, fixed.flr_int(oi), 1)
      end
    end

    -- Outside the nil-target guard on purpose: the line runs whether or not a target was found.
    -- No anchor (the cart would index nil): no line, not one to world (0,0).
    if graphics ~= nil and memory ~= nil then
      if anchor ~= nil then
        local cp = entity_class.entity_pos(anchor)
        gfx_module.line(graphics, memory, fixed.flr_int(cp.x), fixed.flr_int(cp.y), screen_x,
          screen_y, 1)
      end
      gfx_module.spr_default(graphics, memory, 7, screen_x - 4, screen_y - 4)
    end
  else
    local t = self.state_time
    local oi = self.radius
    local has_color_ramp = self.archetype.color_ramp ~= nil

    for dt = 0, 8, 2 do
      if t > fixed.from_int(dt) then
        local sr = wormhole.wormhole_reveal_radius(self, fixed.sub(t, fixed.from_int(dt)))
        local color = 0

        if has_color_ramp then
          local ratio = fixed.mul(fixed.div(sr, oi), fixed.from_int(8))
          local idx = fixed.flr_int(fixed.flr(ratio))
          local _

          _, color = game_data.int_seq_at(self.archetype.color_ramp.items, self.archetype.color_ramp.count, idx, color)
        end

        if graphics ~= nil and memory ~= nil then
          gfx_module.circ(graphics, memory, screen_x, screen_y, fixed.flr_int(sr), color)
        end
      end
    end
  end
end

function M.wormhole_draw_method(self, screen_pos)
  M.wormhole_draw(dispatch_context.dispatch_context_gfx(), dispatch_context.dispatch_context_mem(), self, screen_pos)
end

return M
