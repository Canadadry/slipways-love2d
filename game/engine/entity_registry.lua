-- Include cycle with engine.entity_geom: each requires the other at call time
-- only (draw_layers here); neither needs a value of the other at load.
local bit = require("bit")
local entity_class = require("engine.entity_class")
local entity_id = require("engine.entity_id")
local exit_queue = require("engine.exit_queue")
local game_data = require("game.game_data")
local fixed = require("p8.fixed")

local M = {}

M.MESSAGE_DRAW = 1
M.MESSAGE_DRAW_OVERLAY = 2
M.MESSAGE_HIT_TEST = 3
M.MESSAGE_NEW_YEAR = 4
M.MESSAGE_REFRESH = 5
M.MESSAGE_COUNT = 5

M.LAYERS = 15

local ENTITY_MESSAGE_DRAW = M.MESSAGE_DRAW
local ENTITY_MESSAGE_DRAW_OVERLAY = M.MESSAGE_DRAW_OVERLAY
local ENTITY_MESSAGE_HIT_TEST = M.MESSAGE_HIT_TEST
local ENTITY_MESSAGE_NEW_YEAR = M.MESSAGE_NEW_YEAR
local ENTITY_MESSAGE_COUNT = M.MESSAGE_COUNT
local ENTITY_REGISTRY_LAYERS = M.LAYERS
local TAG_COUNT = entity_class.TAG_COUNT
local UINT_WRAP = 4294967296

-- Emptied in place: every bucket is held by its readers between frames.
local function empty_list(list)
  for i = #list, 1, -1 do
    list[i] = nil
  end
end

-- true when e was in the list.
local function remove_first(list, e)
  for i = 1, #list do
    if list[i] == e then
      table.remove(list, i)
      return true
    end
  end
  return false
end

local g_entities_by_message = {}
for i = 1, ENTITY_MESSAGE_COUNT do
  g_entities_by_message[i] = {}
end

local g_entities_by_tag = {}
for i = 0, TAG_COUNT - 1 do
  g_entities_by_tag[i] = { seen = false, list = {} }
end

-- Layer g (1..15) is items[start[g - 1] + 1 .. start[g]]: start is keyed by layer
-- value 0..15, start[0] = 0 the empty prefix, start[entity_registry.LAYERS] the count.
local function zero_layer_order()
  local start = {}
  for g = 0, ENTITY_REGISTRY_LAYERS do
    start[g] = 0
  end
  return { items = {}, start = start }
end

local g_layer_orders = { zero_layer_order(), zero_layer_order() }
local g_layer_order_mutations = 0

local g_live_entities = {}
local g_main_camera = nil
local g_initialized = false
local g_generation = 0

M.main_camera_placeholder_class_def = { states = {}, tags = 0, lineage = 0, skip_draw_off_screen = false }

M.g_main_camera_placeholder = {}
entity_class.init_header(M.g_main_camera_placeholder, M.main_camera_placeholder_class_def)

local function layer_orders_reset()
  for g = 0, ENTITY_REGISTRY_LAYERS do
    g_layer_orders[1].start[g] = 0
    g_layer_orders[2].start[g] = 0
  end
end

function M.shutdown()
  layer_orders_reset()
  g_generation = (g_generation + 1) % UINT_WRAP
  if not g_initialized then
    return
  end

  empty_list(g_live_entities)

  for i = 1, ENTITY_MESSAGE_COUNT do
    empty_list(g_entities_by_message[i])
  end

  for i = 0, TAG_COUNT - 1 do
    empty_list(g_entities_by_tag[i].list)
    g_entities_by_tag[i].seen = false
  end

  entity_id.table_forget()

  g_main_camera = nil

  g_initialized = false
end

function M.init()
  M.shutdown()

  entity_id.table_init()

  empty_list(g_live_entities)

  for i = 1, ENTITY_MESSAGE_COUNT do
    empty_list(g_entities_by_message[i])
  end

  for i = 0, TAG_COUNT - 1 do
    empty_list(g_entities_by_tag[i].list)
  end

  g_main_camera = M.g_main_camera_placeholder

  g_initialized = true
end

function M.abandon_live_entities()
  for i = 1, #g_live_entities do
    if g_live_entities[i].class.release ~= nil then
      exit_queue.put(g_live_entities[i])
    end
  end
end

function M.forget_registries()
  entity_id.table_forget()
  layer_orders_reset()
  g_generation = (g_generation + 1) % UINT_WRAP
  empty_list(g_live_entities)
  for i = 1, ENTITY_MESSAGE_COUNT do
    empty_list(g_entities_by_message[i])
  end
  for i = 0, TAG_COUNT - 1 do
    empty_list(g_entities_by_tag[i].list)
    g_entities_by_tag[i].seen = false
  end
end

-- Changes on every register, unregister and registry reset (wraps to 0 after 2^32 - 1).
function M.generation()
  return g_generation
end

function M.live_entities()
  return g_live_entities
end

function M.entities_by_message(message)
  assert(g_initialized, "entity_registry.init() must run before entities_by_message is read")
  return g_entities_by_message[message]
end

local function is_message_name(e, message)
  local cls = e.class

  if message == ENTITY_MESSAGE_DRAW then
    return cls.draw ~= nil
  elseif message == ENTITY_MESSAGE_DRAW_OVERLAY then
    return cls.draw_overlay ~= nil
  elseif message == ENTITY_MESSAGE_HIT_TEST then
    return cls.hit_test ~= nil
  elseif message == ENTITY_MESSAGE_NEW_YEAR then
    return cls.new_year ~= nil
  end
  return cls.refresh ~= nil and (cls.has_refresh == nil or cls.has_refresh(e))
end

function M.hit_test_always(self, mouse)
  return true
end

function M.entities_by_tag(tag)
  if g_entities_by_tag[tag].seen then
    return g_entities_by_tag[tag].list
  end
  return nil
end

-- 0 when no entity of the tag was ever registered.
function M.count_tagged(tag)
  local tagged = M.entities_by_tag(tag)
  return (tagged == nil) and 0 or #tagged
end

local function is_drawn_message(message)
  return message == ENTITY_MESSAGE_DRAW or message == ENTITY_MESSAGE_DRAW_OVERLAY
end

local function layer_order_of(message)
  return g_layer_orders[(message == ENTITY_MESSAGE_DRAW) and 1 or 2]
end

-- 0 for no layer or one outside 1..15: such an entity is never drawn.
local function layer_key(e)
  if e.layer == nil then
    return 0
  end
  local layer = fixed.flr_int(e.layer)
  return (layer < 1 or layer > ENTITY_REGISTRY_LAYERS) and 0 or layer
end

-- position is 1-based.
local function layer_order_insert(order, e, key, position)
  local count = order.start[ENTITY_REGISTRY_LAYERS]
  for j = count, position, -1 do
    order.items[j + 1] = order.items[j]
  end
  order.items[position] = e
  for g = key, ENTITY_REGISTRY_LAYERS do
    order.start[g] = order.start[g] + 1
  end
  g_layer_order_mutations = (g_layer_order_mutations + 1) % UINT_WRAP
end

local function layer_order_remove(order, e)
  local count = order.start[ENTITY_REGISTRY_LAYERS]
  local position = 1

  while position <= count and order.items[position] ~= e do
    position = position + 1
  end
  if position > count then
    return
  end
  for j = position, count - 1 do
    order.items[j] = order.items[j + 1]
  end
  order.items[count] = nil
  local g = ENTITY_REGISTRY_LAYERS
  while g >= 1 and order.start[g] >= position do
    order.start[g] = order.start[g] - 1
    g = g - 1
  end
  g_layer_order_mutations = (g_layer_order_mutations + 1) % UINT_WRAP
end

-- The draw or draw_overlay members whose floored layer is layer (1..15), items[first .. first + count - 1]
-- in registration order; the view is invalidated by the next register, unregister or layer change.
function M.layer(message, layer)
  assert(is_drawn_message(message), "entity_registry.layer: only draw and draw_overlay keep a layer order")
  assert(layer >= 1 and layer <= ENTITY_REGISTRY_LAYERS, "entity_registry.layer: layers run 1..15")
  local order = layer_order_of(message)
  return {
    items = order.items,
    first = order.start[layer - 1] + 1,
    count = order.start[layer] - order.start[layer - 1],
  }
end

-- Missing or malformed data is skipped where Lua would crash: message members are tested by presence, unregistering an unknown entity does nothing, draw_layers drops an entity with no layer or one outside 1..15. Making one site fatal alone breaks consistency.
function M.register(e)
  g_generation = (g_generation + 1) % UINT_WRAP

  assert(e ~= nil, "entity_registry.register: an entity")
  g_live_entities[#g_live_entities + 1] = e

  for i = 1, ENTITY_MESSAGE_COUNT do
    if is_message_name(e, i) then
      local bucket = g_entities_by_message[i]
      bucket[#bucket + 1] = e
    end
  end

  local key = layer_key(e)
  if key ~= 0 then
    for i = 1, ENTITY_MESSAGE_COUNT do
      if is_drawn_message(i) and is_message_name(e, i) then
        local order = layer_order_of(i)
        layer_order_insert(order, e, key, order.start[key] + 1)
      end
    end
  end

  for t = 0, TAG_COUNT - 1 do
    if bit.band(e.class.tags, entity_class.TAG(t)) ~= 0 then
      g_entities_by_tag[t].seen = true
      local tagged = g_entities_by_tag[t].list
      tagged[#tagged + 1] = e
    end
  end

  return e
end

-- A message member changed since entity_registry.register() leaves the entity in a list this never removes it from.
function M.unregister(e)
  g_generation = (g_generation + 1) % UINT_WRAP

  if remove_first(g_live_entities, e) then
    exit_queue.put(e)
  end

  for i = 1, ENTITY_MESSAGE_COUNT do
    if is_message_name(e, i) then
      remove_first(g_entities_by_message[i], e)
      if is_drawn_message(i) then
        layer_order_remove(layer_order_of(i), e)
      end
    end
  end

  for i = 0, TAG_COUNT - 1 do
    if bit.band(e.class.tags, entity_class.TAG(i)) ~= 0 then
      remove_first(g_entities_by_tag[i].list, e)
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
  local index = 1
  while index <= #bucket and bucket[index] ~= e do
    if layer_key(bucket[index]) == key then
      rank = rank + 1
    end
    index = index + 1
  end
  if index > #bucket then
    return
  end
  layer_order_insert(order, e, key, order.start[key - 1] + rank + 1)
end

-- Re-sorts the draw order only for a constructed entity whose floored
-- layer changed, else costs one compare. An entity registered bare (entity_registry.register alone) must
-- not change its layer afterwards.
function M.set_layer(e, layer)
  assert(layer ~= nil, "entity_registry.set_layer: a nil layer would clear the entity's layer")
  local old_key = layer_key(e)

  e.layer = layer
  local new_key = layer_key(e)
  if new_key == old_key or entity_id.of(e) == nil then
    return
  end
  for i = 1, ENTITY_MESSAGE_COUNT do
    if is_drawn_message(i) and is_message_name(e, i) then
      layer_order_reinsert(i, e, new_key)
    end
  end
end

function M.draw_layers(message)
  local entity_geom = require("engine.entity_geom")
  local is_draw = message == ENTITY_MESSAGE_DRAW

  assert(is_draw or message == ENTITY_MESSAGE_DRAW_OVERLAY, "draw_layers: only draw and draw_overlay go through draw_layers")

  for layer = 1, ENTITY_REGISTRY_LAYERS do
    local group = M.layer(message, layer)

    if is_draw and group.count > 0 then
      local found, ci = game_data.fixed_seq_at(game_data.layer_fades_table, game_data.LAYER_FADES_COUNT, layer, nil)
      if found and g_main_camera ~= nil and g_main_camera.class.apply_camera ~= nil then
        g_main_camera.class.apply_camera(g_main_camera, ci)
      end
    end

    for index = group.first, group.first + group.count - 1 do
      local qc = group.items[index]
      local draw
      if is_draw then
        draw = qc.class.draw
      else
        draw = qc.class.draw_overlay
      end
      local mutations = g_layer_order_mutations

      assert(layer_key(qc) == layer, "draw_layers: a layer written directly, not through entity_registry.set_layer")
      if draw ~= nil and not (is_draw and qc.class.skip_draw_off_screen and not entity_geom.is_on_screen(qc)) then
        draw(qc, entity_class.pos(qc))
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

function M.set_main_camera(instance)
  if instance == g_main_camera then
    return
  end
  g_main_camera = instance
end

return M
