-- Include cycle: game.probe and game.wormhole are required at call time (background_left_press).
local dispatch_context = require("engine.dispatch_context")
local entity = require("engine.entity")
local entity_class = require("engine.entity_class")
local entity_id = require("engine.entity_id")
local entity_registry = require("engine.entity_registry")
local game_data = require("game.game_data")
local gen = require("game.gen")
local image_slot = require("game.image_slot")
local fixed = require("p8.fixed")
local gfx_module = require("p8.gfx")
local image = require("p8.image")
local vram = require("p8.vram")

local M = {}

M.BACKGROUND_GRID_EDGE = 262

local DRAW_PALETTE_LENGTH = 16

local BACKGROUND_IMAGE_SIDE = vram.P8_MAP_WIDTH * gfx_module.P8_SPRITE_SIZE

local g_background_up = false

local function background_to_int(x)
  return fixed.p8_fixed_flr_int(x)
end

local function background_draw_method(self, screen_pos)
  M.background_draw(dispatch_context.dispatch_context_gfx(), dispatch_context.dispatch_context_mem(), self)
end

local function background_left_press_method(self, mouse)
  return M.background_left_press(self)
end

M.background_class_def = {
  draw = background_draw_method,
  hit_test = entity_registry.entity_registry_hit_test_always,
  left_press = background_left_press_method,
  states = {},
  tags = 0,
  lineage = 0,
  skip_draw_off_screen = false,
}

local function background_create()
  local self = {}

  entity.entity_init_instance(self, M.background_class_def, game_data.background_archetype.state,
    game_data.background_archetype.state_time)
  self.archetype = game_data.background_archetype
  entity_registry.entity_registry_set_layer(self, self.archetype.layer)
  self.is_hovered = false
  self.seed = fixed.p8_fixed_rnd_state()
  M.background_construct(dispatch_context.dispatch_context_mem(), self)
  entity.entity_construct(self)
  return self
end

function M.background_init()
  M.background_shutdown()
  g_background_up = true
end

function M.background_shutdown()
  g_background_up = false
end

function M.background_ready()
  return g_background_up
end

function M.background_class()
  return M.background_class_def
end

-- Records the RNG state as its seed, then draws its tile map from the RNG.
function M.background_new()
  return background_create()
end

-- The live background, or nil when none is up.
function M.background_live()
  local live = entity_registry.live_entities()
  for i = 1, #live do
    local candidate = live[i]
    if candidate.class == M.background_class_def then
      return candidate
    end
  end
  return nil
end

function M.background_construct(memory, self)
  local row_values = {}
  local row_values_set = {}
  local value = fixed.p8_fixed_from_int(0)
  local walk
  local pl

  if self ~= nil and self.archetype.tile_choices ~= nil then
    pl = self.archetype.tile_choices
  else
    pl = game_data.background_tile_choices_seq
  end

  if memory == nil then
    return
  end

  for x = 0, 31 do
    row_values_set[x] = false
  end

  walk = gen.random_walk_new(fixed.P8_FIXED_LIT(0x00001999, 0.1), fixed.p8_fixed_from_int(0))

  for y = 0, 31 do
    for x = 0, 31 do
      local prev = row_values_set[x] and row_values[x] or fixed.p8_fixed_from_int(1)
      local step = fixed.p8_fixed_sub(fixed.p8_fixed_mul(gen.random_walk_next(walk), fixed.p8_fixed_from_int(8)),
        fixed.p8_fixed_from_int(4))
      local roll
      local tile

      value = fixed.p8_fixed_add(step, fixed.p8_fixed_mul(fixed.p8_fixed_add(value, prev),
        fixed.P8_FIXED_LIT(0x00008000, 0.5)))
      row_values[x] = value
      row_values_set[x] = true

      roll = fixed.p8_fixed_rnd_default()
      if roll < fixed.P8_FIXED_LIT(0x00000106, 0.004) then
        tile = 122 + background_to_int(fixed.p8_fixed_rnd(fixed.p8_fixed_from_int(5)))
      else
        local idx_f = fixed.p8_fixed_flr(fixed.p8_fixed_add(value, fixed.p8_fixed_rnd(fixed.p8_fixed_from_int(4))))
        local idx = background_to_int(idx_f)
        local _

        tile = 0
        _, tile = game_data.int_seq_at(pl.items, pl.count, idx, tile)
      end

      gfx_module.p8_mset(memory, x, y, tile)
    end
  end
end

local g_background_image_key = { built = false, mem = nil, map_version = 0, draw_palette = {} }
for i = 0, DRAW_PALETTE_LENGTH - 1 do
  g_background_image_key.draw_palette[i] = 0
end

local function background_draw_map(gfx, mem, context)
  gfx_module.p8_map(gfx, mem, 0, 0, 0, 0, vram.P8_MAP_WIDTH, vram.P8_MAP_HEIGHT)
end

local function draw_palette_differs(key, mem)
  for i = 0, DRAW_PALETTE_LENGTH - 1 do
    if key.draw_palette[i] ~= mem.draw_palette[i] then
      return true
    end
  end
  return false
end

local function background_draw_map_image(gfx, mem)
  local key = g_background_image_key

  if not key.built or key.mem ~= mem or key.map_version ~= mem.map_version or draw_palette_differs(key, mem) then
    image.p8_image_build(gfx, mem, image_slot.IMAGE_SLOT_BACKGROUND, BACKGROUND_IMAGE_SIDE, BACKGROUND_IMAGE_SIDE,
      background_draw_map, nil)
    key.built = true
    key.mem = mem
    key.map_version = mem.map_version
    for i = 0, DRAW_PALETTE_LENGTH - 1 do
      key.draw_palette[i] = mem.draw_palette[i]
    end
  end
  image.p8_image_draw(gfx, mem, image_slot.IMAGE_SLOT_BACKGROUND, 0, 0)
end

function M.background_draw(gfx, mem, self)
  if gfx == nil or mem == nil then
    return
  end

  background_draw_map_image(gfx, mem)
  gfx_module.p8_fillp(gfx, 0xA5A5)
  for xy = 3, M.BACKGROUND_GRID_EDGE, 32 do
    gfx_module.p8_rectfill(gfx, mem, xy, 0, xy, M.BACKGROUND_GRID_EDGE, 1)
    gfx_module.p8_rectfill(gfx, mem, 0, xy, M.BACKGROUND_GRID_EDGE, xy, 1)
  end
  gfx_module.p8_fillp_reset(gfx)
  gfx_module.p8_rect(gfx, mem, 0, 0, M.BACKGROUND_GRID_EDGE, M.BACKGROUND_GRID_EDGE, 5)
end

function M.background_left_press(self)
  local g_tag
  local count
  local made
  local out = {}

  g_tag = entity_registry.entities_by_tag(entity_class.TAG_PROBE)
  count = (g_tag ~= nil) and #g_tag or 0
  if count > 0 then
    local wormhole = require("game.wormhole")
    assert(wormhole.wormhole_ready(),
      "wormhole_init() must run before background_left_press builds a wormhole")
    made = wormhole.wormhole_new()
  else
    made = require("game.probe").probe_new(nil)
  end
  out.stop = made ~= nil
  out.held = entity_id.entity_id_link(made)
  return out
end

return M
