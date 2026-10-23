local entity_class = require("engine.entity_class")
local entity_id = require("engine.entity_id")
local entity_registry = require("engine.entity_registry")
local fixed = require("p8.fixed")

local M = {}

local g_entity_up = false

function M.entity_init()
  M.entity_shutdown()
  g_entity_up = true
end

function M.entity_shutdown()
  g_entity_up = false
end

function M.entity_ready()
  return g_entity_up
end

function M.entity_set_state(self, state)
  if self == nil then
    return
  end
  self.state = state
  self.state_time = fixed.p8_fixed_from_int(0)
end

function M.entity_init_instance(self, cls, state, t)
  entity_class.entity_init_header(self, cls)
  self.state = state
  self.state_time = t
end

-- Indexed from 0; nil at ENTITY_STATE_NONE.
local names = {
  [entity_class.ENTITY_STATE_NORMAL] = "normal",
  [entity_class.ENTITY_STATE_SETTLED] = "settled",
  [entity_class.ENTITY_STATE_VANISHING] = "vanishing",
  [entity_class.ENTITY_STATE_PLACING] = "placing",
  [entity_class.ENTITY_STATE_UNFOLDING] = "unfolding",
  [entity_class.ENTITY_STATE_UNKNOWN] = "unknown",
  [entity_class.ENTITY_STATE_KNOWN] = "known",
}

function M.entity_state_name(state)
  return names[state]
end

function M.entity_construct(self)
  entity_id.entity_id_take(self)
  entity_registry.entity_register(self)
end

return M
