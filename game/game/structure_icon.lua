-- Copy of src/game/structure_icon.h then src/game/structure_icon.c.
--
-- Shapes, with the C field names:
--   structure_icon_t: the entity header plus { archetype, kind, tooltip, is_revealed }; is_revealed
--     nil until revealed.
--   structure_icon_kind_t: the entity header plus { required_tech, sprite, state, cost },
--     returned by value: a new table each call; sprite and cost nil when the kind has none.
--   structure_icon_keys_t: { kind, tooltip }.
--   down_t: { stop, held }.
--
-- game.planet, game.probe, game.tech_gate and game.layout are required at call time.
local dispatch_context = require("engine.dispatch_context")
local entity = require("engine.entity")
local entity_class = require("engine.entity_class")
local entity_geom = require("engine.entity_geom")
local entity_id = require("engine.entity_id")
local entity_registry = require("engine.entity_registry")
local vec = require("engine.vec")
local game_data = require("game.game_data")
local targeting = require("game.targeting")
local fixed = require("p8.fixed")
local gfx_module = require("p8.gfx")
local str = require("p8.str")

local M = {}

local g_structure_icon_up = false

local g_cursor = nil

local function structure_icon_clear_cursor()
  g_cursor = nil
end
local g_id_counter = nil

local function structure_icon_to_int(x)
  return fixed.p8_fixed_flr_int(x)
end

local function structure_icon_own_kind(self)
  return (self ~= nil) and self.kind or nil
end

M.structure_icon_kind_view_class_def = {
  states = {},
  tags = 0,
  lineage = 0,
  skip_draw_off_screen = false,
}

function M.structure_icon_kind_view(self)
  local kind = structure_icon_own_kind(self)
  local view = {}

  entity_class.entity_init_header(view, M.structure_icon_kind_view_class_def)
  view.required_tech = nil
  view.state = entity_class.ENTITY_STATE_NONE
  if kind ~= nil then
    view.required_tech = kind.record.required_tech
    view.sprite = kind.record.sprite
    view.state = kind.record.state
    view.cost = kind.record.cost
  end
  return view
end

function M.structure_icon_kind_view_class()
  return M.structure_icon_kind_view_class_def
end

function M.structure_icon_refresh(self)
  local rr

  if self == nil then
    return
  end

  if self.is_revealed ~= nil and self.is_revealed ~= 0 then
    return
  end

  rr = M.structure_icon_kind_view(self).required_tech

  if require("game.tech_gate").tech_gate_is_unlocked(rr) == 0 then
    return
  end

  assert(g_structure_icon_up, "structure_icon_init() must run before structure_icon_refresh moves the shared cursor")

  if g_cursor ~= nil then
    entity_class.entity_set_pos(self, g_cursor)
  else
    entity_class.entity_clear_pos(self)
  end
  self.is_revealed = 1

  M.structure_icon_set_cursor(vec.vec_sub(M.structure_icon_cursor(),
    vec.vec_make(fixed.p8_fixed_from_int(10), fixed.p8_fixed_from_int(0))))
end

function M.structure_icon_is_revealed(e)
  if e == nil or e.is_dead or e.class ~= M.structure_icon_class() then
    return 0
  end
  local icon = e
  return (icon.is_revealed ~= nil and icon.is_revealed ~= 0) and 1 or 0
end

local function structure_icon_refresh_method(self)
  M.structure_icon_refresh(self)
end

function M.structure_icon_left_press(self)
  local planet = require("game.planet")
  local new_id
  local entity_kind
  local id_num_str
  local keys = planet.planet_keys_zero()
  local held
  local out = { stop = false, held = nil }

  assert(g_structure_icon_up,
    "structure_icon_init() must run before structure_icon_left_press bumps the shared identifier counter")
  new_id = fixed.p8_fixed_add(g_id_counter or fixed.p8_fixed_from_int(0), fixed.p8_fixed_from_int(1))
  g_id_counter = new_id

  entity_kind = structure_icon_own_kind(self)
  if entity_kind == nil then
    return out
  end

  id_num_str = str.p8_tostr_fixed(new_id)
  planet.planet_keys_set_id(keys, str.p8_concat2("s", id_num_str))
  held = entity_kind.new_entity(keys)
  out.held = entity_id.entity_id_link(held)
  out.stop = held ~= nil
  return out
end

local function structure_icon_left_press_method(self, mouse)
  return M.structure_icon_left_press(self)
end

function M.structure_icon_draw(gfx, mem, self, p)
  local view

  if self == nil or not (self.is_revealed ~= nil and self.is_revealed ~= 0) then
    return
  end
  if gfx == nil or mem == nil then
    return
  end

  gfx_module.p8_palt(mem, 3, false)

  view = M.structure_icon_kind_view(self)
  gfx_module.p8_spr_default(gfx, mem, (view.sprite ~= nil) and structure_icon_to_int(view.sprite) or 0,
    structure_icon_to_int(p.x), structure_icon_to_int(p.y))

  if self.is_hovered ~= 0 then
    if structure_icon_own_kind(self) ~= nil then
      local pb = targeting.targeting_zero()
      pb.archetype = game_data.targeting_archetype_none

      pb.cost = view.cost
      require("game.probe").targeting_tooltip_show(gfx, mem, view.state, pb)
    end
    if self.tooltip ~= nil then
      require("game.layout").tooltip_draw_at(gfx, mem, self.tooltip, fixed.p8_fixed_from_int(0),
        fixed.p8_fixed_from_int(0))
    end
  end

  gfx_module.p8_palt(mem, 3, true)
end

local function structure_icon_draw_method(self, screen_pos)
  M.structure_icon_draw(dispatch_context.dispatch_context_gfx(), dispatch_context.dispatch_context_mem(), self,
    screen_pos)
end

local function structure_icon_hit_test_method(self, mouse)
  return entity_geom.entity_geom_contains_cursor(self, mouse)
end

M.structure_icon_class_def = {
  draw = structure_icon_draw_method,
  hit_test = structure_icon_hit_test_method,
  refresh = structure_icon_refresh_method,
  left_press = structure_icon_left_press_method,
  states = {},
  tags = 0,
  lineage = 0,
  skip_draw_off_screen = false,
}

local function structure_icon_create(keys)
  local self = {}
  local archetype = game_data.structure_icon_archetype
  local hit_box

  assert(g_structure_icon_up, "structure_icon_init() must run before a structure icon is created")
  entity.entity_init_instance(self, M.structure_icon_class_def, archetype.state, archetype.state_time)
  self.archetype = archetype
  entity_registry.entity_registry_set_layer(self, self.archetype.layer)
  hit_box = self.archetype.hit_box
  self.hit_box = { left = hit_box.left, top = hit_box.top, right = hit_box.right, bottom = hit_box.bottom }
  entity_class.entity_set_pos(self, self.archetype.pos)
  self.kind = nil
  self.tooltip = nil
  self.is_hovered = 0
  if keys ~= nil then
    self.kind = keys.kind
    self.tooltip = keys.tooltip
  end
  entity.entity_construct(self)
  return self
end

function M.structure_icon_init()
  M.structure_icon_shutdown()

  g_structure_icon_up = true
  structure_icon_clear_cursor()
  g_id_counter = fixed.p8_fixed_from_int(0)
end

function M.structure_icon_shutdown()
  g_structure_icon_up = false
  structure_icon_clear_cursor()
end

function M.structure_icon_ready()
  return g_structure_icon_up
end

function M.structure_icon_class()
  return M.structure_icon_class_def
end

function M.structure_icon_new(keys)
  return structure_icon_create(keys)
end

-- The archetype's own table, nil while the module is down.
function M.structure_icon_rest_cursor()
  return g_structure_icon_up and game_data.structure_icon_archetype.rest_cursor or nil
end

function M.structure_icon_has_cursor()
  return (g_cursor ~= nil) and 1 or 0
end

-- Zero while the cursor is absent.
function M.structure_icon_cursor()
  if g_cursor ~= nil then
    return vec.vec_make(g_cursor.x, g_cursor.y)
  end
  return vec.vec_make(fixed.p8_fixed_from_int(0), fixed.p8_fixed_from_int(0))
end

function M.structure_icon_set_id_counter(id)
  assert(g_structure_icon_up,
    "structure_icon_init() must run before structure_icon_set_id_counter, or the next init resets the counter")
  g_id_counter = id
end

function M.structure_icon_id_counter(out)
  if g_id_counter == nil then
    return 0, out
  end
  return 1, g_id_counter
end

function M.structure_icon_set_cursor(cursor)
  g_cursor = vec.vec_make(cursor.x, cursor.y)
end

return M
