local entity_class = require("engine.entity_class")
local entity_id = require("engine.entity_id")
local entity_registry = require("engine.entity_registry")
local fixed = require("p8.fixed")

local M = {}

local g_entity_up = false

function M.init()
  M.shutdown()
  g_entity_up = true
end

function M.shutdown()
  g_entity_up = false
end

function M.ready()
  return g_entity_up
end

function M.set_state(self, state)
  if self == nil then
    return
  end
  self.state = state
  self.state_time = fixed.from_int(0)
end

function M.init_instance(self, cls, state, t)
  entity_class.init_header(self, cls)
  self.state = state
  self.state_time = t
end

-- Indexed from 0; nil at entity_class.STATE_NONE.
local names = {
  [entity_class.STATE_NORMAL] = "normal",
  [entity_class.STATE_SETTLED] = "settled",
  [entity_class.STATE_VANISHING] = "vanishing",
  [entity_class.STATE_PLACING] = "placing",
  [entity_class.STATE_UNFOLDING] = "unfolding",
  [entity_class.STATE_UNKNOWN] = "unknown",
  [entity_class.STATE_KNOWN] = "known",
}

function M.state_name(state)
  return names[state]
end

function M.construct(self)
  entity_id.take(self)
  entity_registry.register(self)
end

return M
