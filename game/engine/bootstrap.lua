-- An engine module requiring game modules at load (game.mouse, game.scroll, game.ui): nothing
-- requires bootstrap back, so it is in no include cycle.
local entity = require("engine.entity")
local entity_class = require("engine.entity_class")
local entity_registry = require("engine.entity_registry")
local mouse = require("game.mouse")
local scroll = require("game.scroll")
local ui = require("game.ui")
local fixed = require("p8.fixed")

local M = {}

function M.reset_world()
  local real_main_camera
  local real_main_mouse
  local real_main_ui

  if ui.main_ui() ~= nil then
    ui.ui_forget_world(ui.main_ui())
  end
  entity_registry.entity_registry_abandon_live_entities()
  entity_registry.entity_registry_init()

  if not entity.entity_ready() then
    entity.entity_init()
  end
  if not scroll.camera_ready() then
    scroll.camera_init()
  end
  if not mouse.mouse_ready() then
    mouse.mouse_init()
  end
  if not ui.ui_ready() then
    ui.ui_init()
  end

  real_main_camera = scroll.camera_new()
  real_main_mouse = mouse.mouse_new()
  real_main_ui = ui.ui_new()

  entity_registry.entity_registry_set_main_camera(real_main_camera)
  ui.ui_set_main_mouse(real_main_mouse)
  ui.ui_set_main_ui(real_main_ui)
end

function M.bootstrap_shutdown()
  ui.ui_shutdown()
  mouse.mouse_shutdown()
  scroll.camera_shutdown()
  entity.entity_shutdown()
  entity_registry.entity_registry_shutdown()
end

function M.update_entities()
  local hv = entity_registry.live_entities()
  local i = 1

  -- Removing an element during this walk skips the next one; it is visited
  -- next frame. Compensating the index would change update timing and every
  -- replay.
  while i <= #hv do
    local qc = hv[i]

    if qc.state ~= entity_class.ENTITY_STATE_NONE and qc.class.states[qc.state] ~= nil then
      qc.class.states[qc.state](qc, qc.state_time)
    end

    if qc.is_dead then
      entity_registry.entity_unregister(qc)
    end

    qc.state_time = fixed.add(qc.state_time, fixed.from_int(1))
    i = i + 1
  end
end

return M
