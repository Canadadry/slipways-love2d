-- Copy of src/game/ui.h then src/game/ui.c.
--
-- Shapes, with the C field names:
--   ui_t: one table holding the entity header and its own fields; mouse_world and
--     hovered_by_layer nil until the first ui_update and hover pass write them.
--   ui_hovered_by_layer_t: { layer_count, written, layers }, layers a 0-indexed array of
--     UI_HOVER_LAYERS plain 1-based arrays of ids created by the first hover pass.
--   ephemerals: a plain 1-based array of ids.
--   mouse_pos_t: { world, screen, has }; down_t: { stop, held }.
--
-- Include cycle: game.scroll, game.planet, game.planet_family, game.probe, game.slipway and
-- game.wormhole are required at call time.
local bit = require("bit")
local dispatch_context = require("engine.dispatch_context")
local entity = require("engine.entity")
local entity_class = require("engine.entity_class")
local entity_geom = require("engine.entity_geom")
local entity_id = require("engine.entity_id")
local entity_registry = require("engine.entity_registry")
local vec = require("engine.vec")
local game_data = require("game.game_data")
local mouse_module = require("game.mouse")
local cint = require("p8.cint")
local fixed = require("p8.fixed")
local gfx_module = require("p8.gfx")
local vram = require("p8.vram")

local M = {}

M.UI_HOVER_LAYERS = 16

M.MOUSE_SIDE_LEFT = 0

M.UI_PAD_BLINK_PERIOD = 32
M.UI_PAD_CROSSHAIR_ARM = 2
M.UI_PAD_CROSSHAIR_COLOUR = 7

M.SEND_TO_HOVERED_LEFT_PRESS = 0
M.SEND_TO_HOVERED_ACCEPT_DROP = 1

local function zero_vec()
  return vec.vec_make(fixed.p8_fixed_from_int(0), fixed.p8_fixed_from_int(0))
end

local g_ui_up = 0
local MAIN_MOUSE_NONE = 0
local MAIN_MOUSE_PLACEHOLDER = 1
local MAIN_MOUSE_REAL = 2

local g_main_mouse_state = MAIN_MOUSE_NONE
local g_main_mouse = nil
local g_main_ui = nil

function M.ui_panel_box()
  if g_ui_up ~= 0 then
    return game_data.ui_archetype.panel_box
  end
  return nil
end

function M.ui_ephemerals_new()
  return {}
end

function M.ui_set_ephemerals(self, list)
  self.ephemerals = list
  self.ephemerals_serial = bit.band(self.ephemerals_serial + 1, 0xFFFF)
end

local function ui_mouse_pos(self, mouse)
  local out = { world = zero_vec(), screen = zero_vec(), has = 0 }

  if self.mouse_world ~= nil then
    out.world = vec.vec_make(self.mouse_world.x, self.mouse_world.y)
    out.has = bit.bor(out.has, entity_class.MOUSE_POS_HAS_WORLD)
  end
  if mouse ~= nil then
    out.screen = vec.vec_make(mouse.pos.x, mouse.pos.y)
    out.has = bit.bor(out.has, entity_class.MOUSE_POS_HAS_SCREEN)
  end
  return out
end

M.main_mouse_placeholder_class_def = {
  states = {},
  tags = 0,
  lineage = 0,
  skip_draw_off_screen = 0,
}

M.g_main_mouse_placeholder = {}
entity_class.entity_init_header(M.g_main_mouse_placeholder, M.main_mouse_placeholder_class_def)

local function ui_to_int(x)
  return fixed.p8_fixed_flr_int(x)
end

local function ui_pos_or_zero(e)
  if e ~= nil then
    return entity_class.entity_pos(e)
  end
  return vec.vec_make(fixed.p8_fixed_from_int(0), fixed.p8_fixed_from_int(0))
end

local function ui_main_mouse_button()
  local b

  assert(M.main_mouse() ~= nil, "ui_main_mouse_button: reading a button of an absent main mouse stops the run")
  if M.main_mouse_typed() == nil then
    return nil
  end
  b = mouse_module.mouse_left_button()
  if b ~= nil and b.held ~= nil then
    return b
  end
  return nil
end

local function ui_draw_overlay_method(self, screen_pos)
  M.ui_draw_overlay(dispatch_context.dispatch_context_gfx(), dispatch_context.dispatch_context_mem(), self)
end

local function ui_ephemerals_release(self)
  self.ephemerals = {}
end

local function ui_release_method(self)
  ui_ephemerals_release(self)
end

M.ui_class_def = {
  draw_overlay = ui_draw_overlay_method,
  release = ui_release_method,
  states = {},
  tags = 0,
  lineage = 0,
  skip_draw_off_screen = 0,
}

local function ui_create()
  local self

  assert(g_ui_up ~= 0, "ui_init() must run first")
  self = {}
  entity.entity_init_instance(self, M.ui_class_def, game_data.ui_archetype.state, game_data.ui_archetype.state_time)
  self.archetype = game_data.ui_archetype
  entity_registry.entity_registry_set_layer(self, self.archetype.layer)
  self.ephemerals_serial = 0
  self.ephemerals = {}
  self.hovered = nil
  self.previous_hovered = nil
  self.left_held = nil
  self.pad_selection = nil
  self.pad_selection_frames = 0
  self.pad_tab = nil
  self.pad_crosshair = 0
  entity.entity_construct(self)
  return self
end

function M.ui_init()
  M.ui_shutdown()
  g_ui_up = 1

  M.ui_set_main_mouse(M.g_main_mouse_placeholder)
end

function M.ui_shutdown()
  g_ui_up = 0
  M.ui_set_main_mouse(nil)
  g_main_ui = nil
end

function M.main_mouse_placeholder_class()
  return M.main_mouse_placeholder_class_def
end

function M.ui_ready()
  return g_ui_up
end

function M.ui_class()
  return M.ui_class_def
end

function M.ui_new()
  return ui_create()
end

-- Each nil when absent or once its entity was flushed.
function M.ui_hovered(self)
  return entity_id.entity_get(self.hovered)
end

function M.ui_previous_hovered(self)
  return entity_id.entity_get(self.previous_hovered)
end

function M.ui_left_held(self)
  return entity_id.entity_get(self.left_held)
end

function M.ui_pad_selection(self)
  return entity_id.entity_get(self.pad_selection)
end

-- nil selection for none. frames_shown counts from the frame the selection was last shown.
function M.ui_set_pad_selection(self, selection, frames_shown)
  assert(frames_shown >= 0, "ui_set_pad_selection: frames_shown counts up from 0")
  self.pad_selection = entity_id.entity_id_link(selection)
  self.pad_selection_frames = frames_shown
end

function M.ui_pad_tab(self)
  return entity_id.entity_get(self.pad_tab)
end

-- nil for none: the open panel's HUD button, drawn as if hovered.
function M.ui_set_pad_tab(self, button)
  self.pad_tab = entity_id.entity_id_link(button)
end

-- Nonzero draws a crosshair at the screen centre.
function M.ui_set_pad_crosshair(self, shown)
  self.pad_crosshair = shown
end

-- The pad selection while its circle's blink is on; nil otherwise.
function M.ui_pad_circle(self)
  if cint.mod(self.pad_selection_frames, M.UI_PAD_BLINK_PERIOD) < cint.div(M.UI_PAD_BLINK_PERIOD, 2) then
    return M.ui_pad_selection(self)
  end
  return nil
end

-- Empties the ui's own ephemerals list (kept, not replaced) and its hover layers: their ids name
-- entities of the world reset_world() abandons.
function M.ui_forget_world(self)
  local ephemerals = self.ephemerals
  for i = #ephemerals, 1, -1 do
    ephemerals[i] = nil
  end
  if self.hovered_by_layer ~= nil then
    self.hovered_by_layer.written = 0
  end
end

local empty = {}

-- The layer's list from the last hover pass; a shared empty list for a layer that pass did not
-- write, before the first pass, or after ui_forget_world.
function M.ui_hovered_layer(self, layer)
  assert(layer >= 0 and layer < M.UI_HOVER_LAYERS)
  if self.hovered_by_layer ~= nil and bit.band(self.hovered_by_layer.written, bit.lshift(1, layer)) ~= 0 then
    return self.hovered_by_layer.layers[layer]
  end
  return empty
end

function M.main_mouse()
  if g_main_mouse_state == MAIN_MOUSE_PLACEHOLDER then
    return M.g_main_mouse_placeholder
  elseif g_main_mouse_state == MAIN_MOUSE_REAL then
    return entity_id.entity_get(g_main_mouse)
  end
  return nil
end

function M.main_mouse_typed()
  local e = nil

  if g_main_mouse_state == MAIN_MOUSE_REAL then
    e = entity_id.entity_get(g_main_mouse)
  end
  assert(e == nil or e.class == mouse_module.mouse_class(),
    "main_mouse_typed: only the placeholder or a mouse is ever stored")
  return e
end

-- Never frees the outgoing instance: a click inside ui_update replaces it while `self` is still the old one.
function M.ui_set_main_mouse(instance)
  if instance == nil then
    g_main_mouse_state = MAIN_MOUSE_NONE
    g_main_mouse = nil
  elseif instance == M.g_main_mouse_placeholder then
    g_main_mouse_state = MAIN_MOUSE_PLACEHOLDER
    g_main_mouse = nil
  else
    g_main_mouse_state = MAIN_MOUSE_REAL
    g_main_mouse = entity_id.entity_id_link(instance)
  end
end

function M.main_ui()
  local e = entity_id.entity_get(g_main_ui)

  assert(e == nil or e.class == M.ui_class_def, "main_ui: only a ui is ever stored")
  return e
end

-- Never frees the outgoing ui, for the same reason as ui_set_main_mouse().
function M.ui_set_main_ui(ui)
  g_main_ui = entity_id.entity_id_link(ui)
end

-- Called once per frame by `cart_update60()`. TODO: `mem` is no longer read; drop it.
function M.ui_update(mem, self)
  local scroll = require("game.scroll")
  local ephemerals_before
  local serial_before
  local qa
  local hit
  local left
  local je
  local mo

  if self == nil then
    return
  end

  ephemerals_before = {}
  for i = 1, #self.ephemerals do
    ephemerals_before[i] = self.ephemerals[i]
  end
  serial_before = self.ephemerals_serial

  je = M.main_mouse_typed()
  mo = scroll.main_camera_typed()
  qa = vec.vec_add(ui_pos_or_zero(je), ui_pos_or_zero(mo))
  self.mouse_world = qa

  M.ui_update_hover(self)

  hit = (#ephemerals_before == 0)
  for i = 1, #ephemerals_before do
    if ephemerals_before[i] == self.hovered then
      hit = true
      break
    end
  end
  if hit then
    M.ui_handle_button(self, M.MOUSE_SIDE_LEFT)
  end

  left = ui_main_mouse_button()
  if left ~= nil and left.pressed ~= 0 and self.ephemerals_serial == serial_before then
    M.ui_clear_ephemerals(self)
  end

  self.previous_hovered = self.hovered
end

local function ui_draw_pad_crosshair(gfx, mem)
  local x = cint.div(vram.P8_SCREEN_WIDTH, 2)
  local y = cint.div(vram.P8_SCREEN_HEIGHT, 2)
  for d = 1, M.UI_PAD_CROSSHAIR_ARM do
    gfx_module.p8_pset(gfx, mem, x - d, y, M.UI_PAD_CROSSHAIR_COLOUR)
    gfx_module.p8_pset(gfx, mem, x + d, y, M.UI_PAD_CROSSHAIR_COLOUR)
    gfx_module.p8_pset(gfx, mem, x, y - d, M.UI_PAD_CROSSHAIR_COLOUR)
    gfx_module.p8_pset(gfx, mem, x, y + d, M.UI_PAD_CROSSHAIR_COLOUR)
  end
end

function M.ui_draw_overlay(gfx, mem, self)
  if self == nil or gfx == nil or mem == nil then
    return
  end
  if self.pad_crosshair ~= 0 then
    ui_draw_pad_crosshair(gfx, mem)
  end
end

local function ui_hit_test(e, mouse)
  local hit_test = e.class.hit_test

  if hit_test == require("game.planet").planet_hit_test_method then
    return entity_geom.entity_geom_contains_cursor_inline(e, mouse)
  end
  return (hit_test ~= nil and hit_test(e, mouse) ~= 0) and 1 or 0
end

function M.ui_update_hover(self)
  local mouse
  local hit_test_bucket
  local previous
  local hovered = nil

  if self == nil then
    return
  end

  if self.hovered_by_layer == nil then
    self.hovered_by_layer = { layer_count = M.UI_HOVER_LAYERS, written = 0, layers = {} }
    for layer = 0, M.UI_HOVER_LAYERS - 1 do
      self.hovered_by_layer.layers[layer] = {}
    end
  end
  self.hovered_by_layer.written = 0
  self.hovered = nil

  mouse = ui_mouse_pos(self, M.main_mouse_typed())

  hit_test_bucket = entity_registry.entities_by_message(entity_registry.ENTITY_MESSAGE_HIT_TEST)
  local index = 1
  while index <= #hit_test_bucket do
    local e = hit_test_bucket[index]

    if ui_hit_test(e, mouse) ~= 0 then
      if entity_class.entity_has(e, entity_class.ENTITY_HAS_LAYER) ~= 0 then
        local fa = ui_to_int(e.layer)

        assert(fa >= 0 and fa < M.UI_HOVER_LAYERS,
          "ui_update_hover: a hovered entity's layer outside 0..15, the layers ui_send_to_hovered walks -- stop, do not widen")
        if bit.band(self.hovered_by_layer.written, bit.lshift(1, fa)) == 0 then
          local layer_ids = self.hovered_by_layer.layers[fa]
          for i = #layer_ids, 1, -1 do
            layer_ids[i] = nil
          end
          self.hovered_by_layer.written = bit.band(bit.bor(self.hovered_by_layer.written, bit.lshift(1, fa)), 0xFFFF)
        end
        local layer_ids = self.hovered_by_layer.layers[fa]
        layer_ids[#layer_ids + 1] = entity_id.entity_id_link(e)
      end
      hovered = e
      self.hovered = entity_id.entity_id_link(e)
    end
    index = index + 1
  end

  -- Clear before set, so an entity hovered two frames in a row stays hovered.
  previous = M.ui_previous_hovered(self)
  if previous ~= nil then
    assert(entity_class.entity_has(previous, entity_class.ENTITY_HAS_IS_HOVERED) ~= 0,
      "ui_update_hover: the previously hovered entity has no is_hovered member -- stop, do not add one")
    previous.is_hovered = 0
  end

  if hovered ~= nil then
    assert(entity_class.entity_has(hovered, entity_class.ENTITY_HAS_IS_HOVERED) ~= 0,
      "ui_update_hover: the hovered entity has no is_hovered member -- stop, do not add one")
    hovered.is_hovered = 1
  end
end

local function ui_is_held_class(cls)
  local planet_family = require("game.planet_family")
  local probe = require("game.probe")
  local slipway = require("game.slipway")
  local wormhole = require("game.wormhole")

  return (cls == slipway.slipway_class() or cls == probe.probe_class() or cls == wormhole.wormhole_class() or
    (cls.states[entity_class.ENTITY_STATE_SETTLED] ~= nil and cls ~= planet_family.protostar_class())) and 1 or 0
end

-- nil clears; anything else must be of a class a left press can hand over (asserted).
function M.ui_set_left_held(self, held)
  assert(held == nil or ui_is_held_class(held.class) ~= 0,
    "ui_set_left_held: only a class a left press hands over can be held on the left")
  self.left_held = entity_id.entity_id_link(held)
end

function M.ui_handle_button(self, side)
  local button
  local held_inst
  local mouse

  if self == nil then
    return
  end

  button = ui_main_mouse_button()
  mouse = M.main_mouse_typed()

  if button ~= nil and button.pressed ~= 0 then
    local kv = nil
    local kn = nil
    local given
    local found

    found, kv, kn = M.ui_send_to_hovered(self, M.SEND_TO_HOVERED_LEFT_PRESS, ui_mouse_pos(self, mouse), nil, kv, kn)
    if found ~= 0 then
      given = entity_id.entity_get(kn.held)
      if given ~= nil then
        assert(ui_is_held_class(given.class) ~= 0,
          "ui_handle_button: a left press hands over a slipway; a probe or a wormhole from the background; a " ..
          "structure, processor, slipgate, infraspace_gate, trading_hub, synthesizer or ascension from an icon")
        self.left_held = kn.held
      end
    end
  end

  held_inst = M.ui_left_held(self)

  if held_inst ~= nil and held_inst.class.drag ~= nil then
    held_inst.class.drag(held_inst, ui_mouse_pos(self, mouse))
  end

  if button ~= nil and button.released ~= 0 and held_inst ~= nil then
    local target = nil
    local down = nil
    local _

    _, target, down = M.ui_send_to_hovered(self, M.SEND_TO_HOVERED_ACCEPT_DROP,
      { world = zero_vec(), screen = zero_vec(), has = 0 }, held_inst, target, down)

    if held_inst.class.drop ~= nil then
      held_inst.class.drop(held_inst, target)
    end
    self.left_held = nil
  end
end

-- Layers descend from 15 to 0 and the first `stop` wins: that order is the dispatch priority.
-- Returns found, out_target, out_down; both outs as passed when nothing stops.
function M.ui_send_to_hovered(self, message, mouse, held, out_target, out_down)
  if self == nil then
    return 0, out_target, out_down
  end

  if self.hovered_by_layer == nil then
    return 0, out_target, out_down
  end

  for layer = M.UI_HOVER_LAYERS - 1, 0, -1 do
    local hovered_ids = M.ui_hovered_layer(self, layer)

    for i = 1, #hovered_ids do
      local h = entity_id.entity_id_resolve(hovered_ids[i])
      local kn = { stop = 0, held = nil }

      if message == M.SEND_TO_HOVERED_LEFT_PRESS then
        if h.class.left_press ~= nil then
          kn = h.class.left_press(h, mouse)
        end
      elseif message == M.SEND_TO_HOVERED_ACCEPT_DROP then
        if h.class.accept_drop ~= nil then
          kn = h.class.accept_drop(h, held)
        end
      end
      if kn.stop ~= 0 then
        return 1, h, { stop = kn.stop, held = kn.held }
      end
    end
  end

  return 0, out_target, out_down
end

function M.ui_clear_ephemerals(self)
  if self == nil then
    return
  end

  do
    local ephemerals = self.ephemerals
    for i = 1, #ephemerals do
      entity_registry.entity_unregister(entity_id.entity_id_resolve(ephemerals[i]))
    end
  end

  M.ui_set_ephemerals(self, M.ui_ephemerals_new())
end

return M
