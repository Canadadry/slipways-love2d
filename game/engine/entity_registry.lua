-- Copy of src/engine/entity_registry.h then src/engine/entity_registry.c.
-- Include cycle with engine.entity_geom: each requires the other at call time
-- only (draw_layers here); neither needs a value of the other at load.
--
-- Shapes, with the C field names:
--   entity_registry_reserve_t: { live, by_message, by_tag } (by_message and
--     by_tag indexed from 0).
--   entity_layer_view_t: { items, first, count }: the C pointer into the
--     layer order is the index first into its items; read items[first + i].
local bit = require("bit")
local entity_class = require("engine.entity_class")
local entity_id = require("engine.entity_id")
local entity_list = require("engine.entity_list")
local exit_queue = require("engine.exit_queue")
local game_data = require("game.game_data")
local fixed = require("p8.fixed")

local M = {}

M.ENTITY_MESSAGE_DRAW = 0
M.ENTITY_MESSAGE_DRAW_OVERLAY = 1
M.ENTITY_MESSAGE_HIT_TEST = 2
M.ENTITY_MESSAGE_NEW_YEAR = 3
M.ENTITY_MESSAGE_REFRESH = 4
M.ENTITY_MESSAGE_COUNT = 5

M.ENTITY_REGISTRY_LAYERS = 15

local ENTITY_MESSAGE_DRAW = M.ENTITY_MESSAGE_DRAW
local ENTITY_MESSAGE_DRAW_OVERLAY = M.ENTITY_MESSAGE_DRAW_OVERLAY
local ENTITY_MESSAGE_HIT_TEST = M.ENTITY_MESSAGE_HIT_TEST
local ENTITY_MESSAGE_NEW_YEAR = M.ENTITY_MESSAGE_NEW_YEAR
local ENTITY_MESSAGE_COUNT = M.ENTITY_MESSAGE_COUNT
local ENTITY_REGISTRY_LAYERS = M.ENTITY_REGISTRY_LAYERS
local TAG_COUNT = entity_class.TAG_COUNT
local UINT_WRAP = 4294967296

local function zero_list()
  return { items = nil, count = 0, capacity = 0 }
end

local g_entities_by_message = {}
for i = 0, ENTITY_MESSAGE_COUNT - 1 do
  g_entities_by_message[i] = zero_list()
end

local g_entities_by_tag = {}
for i = 0, TAG_COUNT - 1 do
  g_entities_by_tag[i] = { seen = 0, list = zero_list() }
end

-- Group g (1..15) is items[start[g - 1] .. start[g]); start[ENTITY_REGISTRY_LAYERS] is the count.
local function zero_layer_order()
  local start = {}
  for g = 0, ENTITY_REGISTRY_LAYERS do
    start[g] = 0
  end
  return { items = {}, start = start }
end

local g_layer_orders = { [0] = zero_layer_order(), zero_layer_order() }
local g_layer_order_mutations = 0

local g_live_entities = zero_list()
local g_main_camera = nil
local g_initialized = 0
local g_generation = 0

M.main_camera_placeholder_class_def = { states = {}, tags = 0, lineage = 0, skip_draw_off_screen = 0 }

M.g_main_camera_placeholder = {}
entity_class.entity_init_header(M.g_main_camera_placeholder, M.main_camera_placeholder_class_def)

local function layer_orders_reset()
  for g = 0, ENTITY_REGISTRY_LAYERS do
    g_layer_orders[0].start[g] = 0
    g_layer_orders[1].start[g] = 0
  end
end

function M.entity_registry_shutdown()
  layer_orders_reset()
  g_generation = (g_generation + 1) % UINT_WRAP
  if g_initialized == 0 then
    return
  end

  entity_list.entity_list_free(g_live_entities)

  for i = 0, ENTITY_MESSAGE_COUNT - 1 do
    entity_list.entity_list_free(g_entities_by_message[i])
  end

  for i = 0, TAG_COUNT - 1 do
    entity_list.entity_list_free(g_entities_by_tag[i].list)
    g_entities_by_tag[i].seen = 0
  end

  entity_id.entity_id_table_forget()

  g_main_camera = nil

  g_initialized = 0
end

local no_reserve = { live = 0, by_message = {}, by_tag = {} }
for i = 0, ENTITY_MESSAGE_COUNT - 1 do
  no_reserve.by_message[i] = 0
end
for i = 0, TAG_COUNT - 1 do
  no_reserve.by_tag[i] = 0
end

function M.entity_registry_init(reserve)
  if reserve == nil then reserve = no_reserve end

  M.entity_registry_shutdown()

  entity_id.entity_id_table_init()

  entity_list.entity_list_init_reserve(g_live_entities, reserve.live)

  for i = 0, ENTITY_MESSAGE_COUNT - 1 do
    entity_list.entity_list_init_reserve(g_entities_by_message[i], reserve.by_message[i])
  end

  for i = 0, TAG_COUNT - 1 do
    entity_list.entity_list_init_reserve(g_entities_by_tag[i].list, reserve.by_tag[i])
  end

  g_main_camera = M.g_main_camera_placeholder

  g_initialized = 1
end

function M.entity_registry_abandon_live_entities()
  for i = 0, g_live_entities.count - 1 do
    if g_live_entities.items[i].class.release ~= nil then
      exit_queue.exit_queue_put(g_live_entities.items[i])
    end
  end
end

local function forget_list(list)
  list.items = nil
  list.count = 0
  list.capacity = 0
end

function M.entity_registry_forget_registries()
  entity_id.entity_id_table_forget()
  layer_orders_reset()
  g_generation = (g_generation + 1) % UINT_WRAP
  forget_list(g_live_entities)
  for i = 0, ENTITY_MESSAGE_COUNT - 1 do
    forget_list(g_entities_by_message[i])
  end
  for i = 0, TAG_COUNT - 1 do
    forget_list(g_entities_by_tag[i].list)
    g_entities_by_tag[i].seen = 0
  end
end

-- Changes on every register, unregister and registry reset (an unsigned counter that wraps).
function M.entity_registry_generation()
  return g_generation
end

function M.live_entities()
  return g_live_entities
end

function M.entities_by_message(message)
  assert(g_initialized ~= 0, "entity_registry_init() must run before entities_by_message is read")
  return g_entities_by_message[message]
end

local function is_message_name(e, message)
  local cls = e.class

  if message == ENTITY_MESSAGE_DRAW then
    return (cls.draw ~= nil) and 1 or 0
  elseif message == ENTITY_MESSAGE_DRAW_OVERLAY then
    return (cls.draw_overlay ~= nil) and 1 or 0
  elseif message == ENTITY_MESSAGE_HIT_TEST then
    return (cls.hit_test ~= nil) and 1 or 0
  elseif message == ENTITY_MESSAGE_NEW_YEAR then
    return (cls.new_year ~= nil) and 1 or 0
  end
  return (cls.refresh ~= nil and (cls.has_refresh == nil or cls.has_refresh(e) ~= 0)) and 1 or 0
end

function M.entity_registry_hit_test_always(self, mouse)
  return 1
end

function M.entities_by_tag(tag)
  if g_entities_by_tag[tag].seen ~= 0 then
    return g_entities_by_tag[tag].list
  end
  return nil
end

-- 0 when no entity of the tag was ever registered.
function M.entity_registry_count_tagged(tag)
  local tagged = M.entities_by_tag(tag)
  return (tagged == nil) and 0 or tagged.count
end

local function is_drawn_message(message)
  return (message == ENTITY_MESSAGE_DRAW or message == ENTITY_MESSAGE_DRAW_OVERLAY) and 1 or 0
end

local function layer_order_of(message)
  return g_layer_orders[(message == ENTITY_MESSAGE_DRAW) and 0 or 1]
end

-- 0 for no layer or one outside 1..15: such an entity is never drawn.
local function layer_key(e)
  if entity_class.entity_has(e, entity_class.ENTITY_HAS_LAYER) == 0 then
    return 0
  end
  local layer = fixed.p8_fixed_flr_int(e.layer)
  return (layer < 1 or layer > ENTITY_REGISTRY_LAYERS) and 0 or layer
end

local function layer_order_insert(order, e, key, index)
  local count = order.start[ENTITY_REGISTRY_LAYERS]
  for j = count - 1, index, -1 do
    order.items[j + 1] = order.items[j]
  end
  order.items[index] = e
  for g = key, ENTITY_REGISTRY_LAYERS do
    order.start[g] = order.start[g] + 1
  end
  g_layer_order_mutations = (g_layer_order_mutations + 1) % UINT_WRAP
end

local function layer_order_remove(order, e)
  local count = order.start[ENTITY_REGISTRY_LAYERS]
  local index = 0

  while index < count and order.items[index] ~= e do
    index = index + 1
  end
  if index == count then
    return
  end
  for j = index, count - 2 do
    order.items[j] = order.items[j + 1]
  end
  local g = ENTITY_REGISTRY_LAYERS
  while g >= 1 and order.start[g] > index do
    order.start[g] = order.start[g] - 1
    g = g - 1
  end
  g_layer_order_mutations = (g_layer_order_mutations + 1) % UINT_WRAP
end

-- The draw or draw_overlay members whose floored layer is layer (1..15), in registration order;
-- the view is invalidated by the next register, unregister or layer change.
function M.entity_registry_layer(message, layer)
  assert(is_drawn_message(message) ~= 0, "entity_registry_layer: only draw and draw_overlay keep a layer order")
  assert(layer >= 1 and layer <= ENTITY_REGISTRY_LAYERS, "entity_registry_layer: layers run 1..15")
  local order = layer_order_of(message)
  return {
    items = order.items,
    first = order.start[layer - 1],
    count = order.start[layer] - order.start[layer - 1],
  }
end

-- Missing or malformed data is skipped where Lua would crash: message members are tested by presence, unregistering an unknown entity does nothing, draw_layers drops an entity with no layer or one outside 1..15. Making one site fatal alone breaks consistency.
function M.entity_register(e)
  g_generation = (g_generation + 1) % UINT_WRAP

  entity_list.entity_list_add(g_live_entities, e)

  for i = 0, ENTITY_MESSAGE_COUNT - 1 do
    if is_message_name(e, i) ~= 0 then
      entity_list.entity_list_add(g_entities_by_message[i], e)
    end
  end

  local key = layer_key(e)
  local i = 0
  while i < ENTITY_MESSAGE_COUNT and key ~= 0 do
    if is_drawn_message(i) ~= 0 and is_message_name(e, i) ~= 0 then
      local order = layer_order_of(i)
      layer_order_insert(order, e, key, order.start[key])
    end
    i = i + 1
  end

  for t = 0, TAG_COUNT - 1 do
    if bit.band(e.class.tags, entity_class.TAG(t)) ~= 0 then
      g_entities_by_tag[t].seen = 1
      entity_list.entity_list_add(g_entities_by_tag[t].list, e)
    end
  end

  return e
end

-- A message member changed since entity_register() leaves the entity in a list this never removes it from.
function M.entity_unregister(e)
  g_generation = (g_generation + 1) % UINT_WRAP

  if entity_list.entity_list_del(g_live_entities, e) ~= 0 then
    exit_queue.exit_queue_put(e)
  end

  for i = 0, ENTITY_MESSAGE_COUNT - 1 do
    if is_message_name(e, i) ~= 0 then
      entity_list.entity_list_del(g_entities_by_message[i], e)
      if is_drawn_message(i) ~= 0 then
        layer_order_remove(layer_order_of(i), e)
      end
    end
  end

  for i = 0, TAG_COUNT - 1 do
    if bit.band(e.class.tags, entity_class.TAG(i)) ~= 0 then
      entity_list.entity_list_del(g_entities_by_tag[i].list, e)
    end
  end
end

-- Registration rank: the bucket members before e that share its key.
local function layer_order_reinsert(message, e, key)
  local bucket = g_entities_by_message[message]
  local order = layer_order_of(message)
  local rank = 0

  layer_order_remove(order, e)
  if key == 0 then
    return
  end
  local index = 0
  while index < bucket.count and bucket.items[index] ~= e do
    if layer_key(bucket.items[index]) == key then
      rank = rank + 1
    end
    index = index + 1
  end
  if index == bucket.count then
    return
  end
  layer_order_insert(order, e, key, order.start[key - 1] + rank)
end

-- Also sets ENTITY_HAS_LAYER. Re-sorts the draw order only for a constructed entity whose floored
-- layer changed, else costs one compare. An entity registered bare (entity_register alone) must
-- not change its layer afterwards.
function M.entity_registry_set_layer(e, layer)
  local old_key = layer_key(e)

  e.has = bit.bor(e.has, entity_class.ENTITY_HAS_LAYER)
  e.layer = layer
  local new_key = layer_key(e)
  if new_key == old_key or entity_id.entity_id_of(e) == nil then
    return
  end
  for i = 0, ENTITY_MESSAGE_COUNT - 1 do
    if is_drawn_message(i) ~= 0 and is_message_name(e, i) ~= 0 then
      layer_order_reinsert(i, e, new_key)
    end
  end
end

function M.draw_layers(message)
  local entity_geom = require("engine.entity_geom")
  local is_draw = message == ENTITY_MESSAGE_DRAW

  assert(is_draw or message == ENTITY_MESSAGE_DRAW_OVERLAY, "draw_layers: only draw and draw_overlay go through draw_layers")

  for layer = 1, ENTITY_REGISTRY_LAYERS do
    local group = M.entity_registry_layer(message, layer)

    if is_draw and group.count > 0 then
      local found, ci = game_data.game_data_fixed_seq_at(game_data.layer_fades_table, game_data.LAYER_FADES_COUNT, layer, nil)
      if found ~= 0 and g_main_camera ~= nil and g_main_camera.class.apply_camera ~= nil then
        g_main_camera.class.apply_camera(g_main_camera, ci)
      end
    end

    for index = 0, group.count - 1 do
      local qc = group.items[group.first + index]
      local draw
      if is_draw then
        draw = qc.class.draw
      else
        draw = qc.class.draw_overlay
      end
      local mutations = g_layer_order_mutations

      assert(layer_key(qc) == layer, "draw_layers: a layer written directly, not through entity_registry_set_layer")
      if draw ~= nil and not (is_draw and qc.class.skip_draw_off_screen ~= 0 and entity_geom.entity_geom_is_on_screen(qc) == 0) then
        draw(qc, entity_class.entity_pos(qc))
      end
      assert(g_layer_order_mutations == mutations,
        "draw_layers: a draw method must not register, unregister or re-layer an entity -- it walks the kept order in place")
    end
  end
end

function M.main_camera_placeholder_class()
  return M.main_camera_placeholder_class_def
end

function M.main_camera()
  return g_main_camera
end

function M.entity_registry_set_main_camera(instance)
  if instance == g_main_camera then
    return
  end
  g_main_camera = instance
end

return M
