-- Copy of src/game/save_state.h then src/game/save_state.c.
--
-- Shapes, with the C field names:
--   A byte buffer is a 0-indexed array of integers 0 .. 255, owned and filled in place as the C's.
--   cursor_t: { position, end_, failed }.
--   save_vec_t: { x, y }; save_need_t: { resource }, nil when absent. A save_route_t's resource
--     is nil when absent too, and so is save_state_t's probe { pos, map_pos, fold_anchor }; the
--     flag bytes are written from presence.
--   save_state_t: { difficulty, economy, rnd_state, background_seed, type_picker, vanish_walk,
--     structure_id_counter, probe, camera_map_pos, node_count, nodes, slipway_count,
--     slipways, route_count, routes, route_index_count, route_indices, node_slipway_count,
--     node_slipways }, every array 0-indexed. memset leaves nodes, slipways, routes,
--     route_indices and node_slipways empty: an element is created at its C zero value when the
--     C first writes into it, and nothing reads one past its count.
--   save_node_t.planet_id is a Lua string; u32 values (rnd_state, background_seed, the CRC) are
--     unsigned 0 .. 2^32-1, i32 values the signed raw value.
local bit = require("bit")
local entity_class = require("engine.entity_class")
local entity_id = require("engine.entity_id")
local entity_registry = require("engine.entity_registry")
local background_module = require("game.background")
local economy_score = require("game.economy_score")
local game_data = require("game.game_data")
local gen = require("game.gen")
local map_tables = require("game.map_tables")
local planet_module = require("game.planet")
local planet_family = require("game.planet_family")
local planet_family_trade = require("game.planet_family_trade")
local probe = require("game.probe")
local research = require("game.research")
local structure_icon = require("game.structure_icon")
local cint = require("p8.cint")
local fixed = require("p8.fixed")

local M = {}

M.SAVE_STATE_MAX_BYTES = 8192
M.SAVE_STATE_VERSION = 1
M.SAVE_STATE_RECENT_COUNT = 3
M.SAVE_STATE_NODES_CAP = 255
M.SAVE_STATE_SLIPWAYS_CAP = 255
M.SAVE_STATE_ROUTES_CAP = cint.div(M.SAVE_STATE_MAX_BYTES, 3)
M.SAVE_STATE_ROUTE_INDICES_CAP = M.SAVE_STATE_MAX_BYTES
M.SAVE_STATE_NODE_SLIPWAYS_CAP = 2 * M.SAVE_STATE_SLIPWAYS_CAP
M.SAVE_STATE_NEEDS_CAP = 8
M.SAVE_STATE_PLANET_ID_MAX = 7

M.SAVE_NODE_PLANET = 0
M.SAVE_NODE_PROTOSTAR = 1
M.SAVE_NODE_STRUCTURE = 2
M.SAVE_NODE_PROCESSOR = 3
M.SAVE_NODE_SLIPGATE = 4
M.SAVE_NODE_INFRASPACE_GATE = 5
M.SAVE_NODE_TRADING_HUB = 6
M.SAVE_NODE_SYNTHESIZER = 7
M.SAVE_NODE_ASCENSION = 8
M.SAVE_NODE_KIND_COUNT = 9

M.SAVE_NODE_HAS_POS = bit.lshift(1, 0)
M.SAVE_NODE_HAS_MAP_POS = bit.lshift(1, 1)
M.SAVE_NODE_HAS_PLANET_ID = bit.lshift(1, 2)
M.SAVE_NODE_HAS_NEEDS = bit.lshift(1, 3)
M.SAVE_NODE_HAS_DEVELOPMENT_TIER = bit.lshift(1, 4)
M.SAVE_NODE_HAS_PRODUCTION_BONUS = bit.lshift(1, 5)
M.SAVE_NODE_HAS_PRODUCTION = bit.lshift(1, 6)
M.SAVE_NODE_PRODUCTION_HAS_RESOURCE = bit.lshift(1, 7)
M.SAVE_NODE_SYNTHESIZER_SPENT = bit.lshift(1, 8)
M.SAVE_NODE_HAS_OUTGOING_ROUTES = bit.lshift(1, 9)
M.SAVE_NODE_HAS_STATE_TIME = bit.lshift(1, 10)
M.SAVE_NODE_KNOWN_FLAGS = bit.lshift(1, 11) - 1

-- planet_id holds up to 8 bytes on encode; a decoded one is a Lua string.
-- planet_type, description_type and project are 1-based, 0 for none;
-- description_type is the type whose description the planet holds, which
-- terraform leaves on the old type. The route and slipway
-- slices index the state's pools.
-- Both route slices index route_indices: node indices, then slipway indices.
-- Every fixed-point value is its raw 16.16 int32_t; every reference is a
-- 0-based table index, except difficulty (1-based in difficulty_options_table).

local MAGIC_LENGTH = 4
local NODE_PLANET_ID_LENGTH = 8

local HEADER_BYTES = 8
local TRAILER_BYTES = 4
local LENGTH_OFFSET = 6

M.magic = { [0] = string.byte("S"), string.byte("L"), string.byte("W"), string.byte("Y") }
local magic = M.magic

local crc32_table

local function zero_array(count)
  local array = {}
  for i = 0, count - 1 do
    array[i] = 0
  end
  return array
end

-- Lua only: memset(out, 0, sizeof *out) on a save_state_t.
local function save_state_zero(out)
  out.difficulty = 0
  out.economy = {
    credits = 0,
    research_points = 0,
    era = 0,
    year = 0,
    year_ticks = 0,
    invented_count = 0,
    invented = zero_array(map_tables.TECHNOLOGIES_COUNT),
  }
  out.rnd_state = 0
  out.background_seed = 0
  out.type_picker = {
    tickets_count = 0,
    tickets = zero_array(gen.WEIGHTED_PICKER_TICKETS_CAP),
    recent = zero_array(M.SAVE_STATE_RECENT_COUNT),
    recent_set = zero_array(M.SAVE_STATE_RECENT_COUNT),
    multiplier = 0,
    walk_step = 0,
    walk_bias = 0,
  }
  out.vanish_walk = { step = 0, bias = 0 }
  out.structure_id_counter = 0
  out.probe = nil
  out.camera_map_pos = { x = 0, y = 0 }
  out.node_count = 0
  out.nodes = {}
  out.slipway_count = 0
  out.slipways = {}
  out.route_count = 0
  out.routes = {}
  out.route_index_count = 0
  out.route_indices = {}
  out.node_slipway_count = 0
  out.node_slipways = {}
end

-- Lua only: a save_node_t at its C zero value.
local function save_node_zero()
  local needs = {}
  for i = 0, M.SAVE_STATE_NEEDS_CAP - 1 do
    needs[i] = {}
  end
  return {
    kind = 0,
    state = 0,
    flags = 0,
    pos = { x = 0, y = 0 },
    map_pos = { x = 0, y = 0 },
    fold_anchor = 0,
    infraspace = 0,
    planet_id = "",
    planet_type = 0,
    description_type = 0,
    project = 0,
    need_count = 0,
    needs = needs,
    development_tier = 0,
    production_bonus = 0,
    production_resource = 0,
    state_time = 0,
    first_route = 0,
    route_count = 0,
    first_slipway = 0,
    slipway_count = 0,
  }
end

-- Lua only: a save_route_t at its C zero value.
local function save_route_zero()
  return {
    first_planet = 0,
    planet_count = 0,
    first_slipway = 0,
    slipway_count = 0,
    resource_is_bool = 0,
  }
end

-- Lua only: a save_slipway_t at its C zero value.
local function save_slipway_zero()
  return { start = 0, end_ = 0, state_time = 0 }
end

-- Writing past SAVE_STATE_MAX_BYTES asserts. Reading past end sets failed and
-- returns 0, so a parser checks failed once at the end.

local function write_u8(bytes, writer, value)
  assert(writer.position >= 0 and writer.position < M.SAVE_STATE_MAX_BYTES, "save_state_encode: the record must fit SAVE_STATE_MAX_BYTES, 8 KiB")
  bytes[writer.position] = bit.band(value, 0xFF)
  writer.position = writer.position + 1
end

local function write_u16(bytes, writer, value)
  write_u8(bytes, writer, value)
  write_u8(bytes, writer, bit.rshift(value, 8))
end

local function write_u32(bytes, writer, value)
  write_u16(bytes, writer, value)
  write_u16(bytes, writer, bit.rshift(value, 16))
end

local function write_i32(bytes, writer, value)
  write_u32(bytes, writer, value)
end

local function write_vec(bytes, writer, vec)
  write_i32(bytes, writer, vec.x)
  write_i32(bytes, writer, vec.y)
end

local function read_u8(bytes, reader)
  if reader.position >= reader.end_ then
    reader.failed = 1
    return 0
  end
  local value = bytes[reader.position]
  reader.position = reader.position + 1
  return value
end

local function read_u16(bytes, reader)
  local low = read_u8(bytes, reader)
  return low + read_u8(bytes, reader) * 0x100
end

local function read_u32(bytes, reader)
  local low = read_u16(bytes, reader)
  return low + read_u16(bytes, reader) * 0x10000
end

local function read_i32(bytes, reader)
  return bit.tobit(read_u32(bytes, reader))
end

local function read_vec(bytes, reader)
  local vec = {}
  vec.x = read_i32(bytes, reader)
  vec.y = read_i32(bytes, reader)
  return vec
end

local function write_indices(bytes, writer, state, first, count)
  assert(count >= 0 and count <= 0xFF and first >= 0 and first + count <= state.route_index_count)
  write_u8(bytes, writer, count)
  for i = 0, count - 1 do
    write_u8(bytes, writer, state.route_indices[first + i])
  end
end

local function write_route(bytes, writer, state, route)
  write_indices(bytes, writer, state, route.first_planet, route.planet_count)
  write_indices(bytes, writer, state, route.first_slipway, route.slipway_count)
  write_u8(bytes, writer, bit.bor((route.resource ~= nil) and 1 or 0, bit.lshift(route.resource_is_bool, 1)))
  if route.resource ~= nil then
    write_i32(bytes, writer, route.resource)
  end
end

local function write_node(bytes, writer, state, node)
  write_u8(bytes, writer, node.kind)
  write_u8(bytes, writer, node.state)
  write_u16(bytes, writer, node.flags)
  if bit.band(node.flags, M.SAVE_NODE_HAS_POS) ~= 0 then
    write_vec(bytes, writer, node.pos)
  end
  if bit.band(node.flags, M.SAVE_NODE_HAS_MAP_POS) ~= 0 then
    write_vec(bytes, writer, node.map_pos)
  end
  if bit.band(node.flags, M.SAVE_NODE_HAS_PLANET_ID) ~= 0 then
    local length = 0
    while length < NODE_PLANET_ID_LENGTH and (string.byte(node.planet_id, length + 1) or 0) ~= 0 do
      length = length + 1
    end
    write_u8(bytes, writer, length)
    for i = 0, length - 1 do
      write_u8(bytes, writer, string.byte(node.planet_id, i + 1))
    end
  end
  write_i32(bytes, writer, node.fold_anchor)
  write_i32(bytes, writer, node.infraspace)
  write_u8(bytes, writer, node.planet_type)
  write_u8(bytes, writer, node.description_type)
  write_u8(bytes, writer, node.project)
  if bit.band(node.flags, M.SAVE_NODE_HAS_NEEDS) ~= 0 then
    assert(node.need_count >= 0 and node.need_count <= 0xFF)
    write_u8(bytes, writer, node.need_count)
    local i = 0
    while i < node.need_count and i < M.SAVE_STATE_NEEDS_CAP do
      write_u8(bytes, writer, (node.needs[i].resource ~= nil) and 1 or 0)
      if node.needs[i].resource ~= nil then
        write_i32(bytes, writer, node.needs[i].resource)
      end
      i = i + 1
    end
  else
    assert(node.need_count == 0, "save_state_encode: needs without SAVE_NODE_HAS_NEEDS would be dropped")
  end
  if bit.band(node.flags, M.SAVE_NODE_HAS_DEVELOPMENT_TIER) ~= 0 then
    write_i32(bytes, writer, node.development_tier)
  end
  if bit.band(node.flags, M.SAVE_NODE_HAS_PRODUCTION_BONUS) ~= 0 then
    write_i32(bytes, writer, node.production_bonus)
  end
  if bit.band(node.flags, M.SAVE_NODE_PRODUCTION_HAS_RESOURCE) ~= 0 then
    write_i32(bytes, writer, node.production_resource)
  end
  if bit.band(node.flags, M.SAVE_NODE_HAS_STATE_TIME) ~= 0 then
    write_i32(bytes, writer, node.state_time)
  end
  if bit.band(node.flags, M.SAVE_NODE_HAS_OUTGOING_ROUTES) ~= 0 then
    assert(node.route_count >= 0 and node.route_count <= 0xFFFF and node.first_route >= 0 and
           node.first_route + node.route_count <= state.route_count)
    write_u16(bytes, writer, node.route_count)
    for i = 0, node.route_count - 1 do
      write_route(bytes, writer, state, state.routes[node.first_route + i])
    end
  else
    assert(node.route_count == 0, "save_state_encode: routes without SAVE_NODE_HAS_OUTGOING_ROUTES would be dropped")
  end
  assert(node.slipway_count >= 0 and node.slipway_count <= 0xFF and node.first_slipway >= 0 and
         node.first_slipway + node.slipway_count <= state.node_slipway_count)
  write_u8(bytes, writer, node.slipway_count)
  for i = 0, node.slipway_count - 1 do
    write_u8(bytes, writer, state.node_slipways[node.first_slipway + i])
  end
end

local function write_payload(bytes, writer, state)
  local economy = state.economy
  local picker = state.type_picker

  assert(economy.invented_count >= 0 and economy.invented_count <= map_tables.TECHNOLOGIES_COUNT)
  assert(picker.tickets_count >= 0 and picker.tickets_count <= gen.WEIGHTED_PICKER_TICKETS_CAP)

  write_u8(bytes, writer, state.difficulty)

  write_i32(bytes, writer, economy.credits)
  write_i32(bytes, writer, economy.research_points)
  write_i32(bytes, writer, economy.era)
  write_i32(bytes, writer, economy.year)
  write_i32(bytes, writer, economy.year_ticks)
  write_u8(bytes, writer, economy.invented_count)
  for i = 0, economy.invented_count - 1 do
    write_u8(bytes, writer, economy.invented[i])
  end

  write_u32(bytes, writer, state.rnd_state)
  write_u32(bytes, writer, state.background_seed)

  write_u8(bytes, writer, picker.tickets_count)
  for i = 0, picker.tickets_count - 1 do
    write_u8(bytes, writer, picker.tickets[i])
  end
  for i = 0, M.SAVE_STATE_RECENT_COUNT - 1 do
    write_u8(bytes, writer, picker.recent[i])
  end
  for i = 0, M.SAVE_STATE_RECENT_COUNT - 1 do
    write_u8(bytes, writer, picker.recent_set[i])
  end
  write_i32(bytes, writer, picker.multiplier)
  write_i32(bytes, writer, picker.walk_step)
  write_i32(bytes, writer, picker.walk_bias)

  write_i32(bytes, writer, state.vanish_walk.step)
  write_i32(bytes, writer, state.vanish_walk.bias)

  write_i32(bytes, writer, state.structure_id_counter)

  write_u8(bytes, writer, (state.probe ~= nil) and 1 or 0)
  if state.probe ~= nil then
    write_vec(bytes, writer, state.probe.pos)
    write_vec(bytes, writer, state.probe.map_pos)
    write_i32(bytes, writer, state.probe.fold_anchor)
  end

  write_vec(bytes, writer, state.camera_map_pos)

  assert(state.node_count >= 0 and state.node_count <= M.SAVE_STATE_NODES_CAP, "save_state_encode: node indices are one byte")
  write_u8(bytes, writer, state.node_count)
  for i = 0, state.node_count - 1 do
    write_node(bytes, writer, state, state.nodes[i])
  end

  assert(state.slipway_count >= 0 and state.slipway_count <= M.SAVE_STATE_SLIPWAYS_CAP, "save_state_encode: slipway indices are one byte")
  write_u8(bytes, writer, state.slipway_count)
  for i = 0, state.slipway_count - 1 do
    write_u8(bytes, writer, state.slipways[i].start)
    write_u8(bytes, writer, state.slipways[i].end_)
    write_i32(bytes, writer, state.slipways[i].state_time)
  end
end

-- Returns the record's byte count; the caller owns buffer. Writes what it is
-- given: only save_state_decode validates.
function M.save_state_encode(state, buffer)
  local writer = { position = 0, end_ = M.SAVE_STATE_MAX_BYTES, failed = 0 }
  local length = { position = LENGTH_OFFSET, end_ = LENGTH_OFFSET + 2, failed = 0 }
  for i = 0, MAGIC_LENGTH - 1 do
    write_u8(buffer, writer, magic[i])
  end
  write_u16(buffer, writer, M.SAVE_STATE_VERSION)
  write_u16(buffer, writer, 0)
  write_payload(buffer, writer, state)
  write_u16(buffer, length, writer.position - HEADER_BYTES)
  write_u32(buffer, writer, M.save_state_crc32(buffer, writer.position))
  return writer.position
end

local function read_economy(bytes, reader, economy)
  economy.credits = read_i32(bytes, reader)
  economy.research_points = read_i32(bytes, reader)
  economy.era = read_i32(bytes, reader)
  economy.year = read_i32(bytes, reader)
  economy.year_ticks = read_i32(bytes, reader)
  economy.invented_count = read_u8(bytes, reader)
  if economy.invented_count > map_tables.TECHNOLOGIES_COUNT then
    reader.failed = 1
    return
  end
  for i = 0, economy.invented_count - 1 do
    economy.invented[i] = read_u8(bytes, reader)
  end
end

local function read_type_picker(bytes, reader, picker)
  picker.tickets_count = read_u8(bytes, reader)
  if picker.tickets_count > gen.WEIGHTED_PICKER_TICKETS_CAP then
    reader.failed = 1
    return
  end
  for i = 0, picker.tickets_count - 1 do
    picker.tickets[i] = read_u8(bytes, reader)
  end
  for i = 0, M.SAVE_STATE_RECENT_COUNT - 1 do
    picker.recent[i] = read_u8(bytes, reader)
  end
  for i = 0, M.SAVE_STATE_RECENT_COUNT - 1 do
    picker.recent_set[i] = read_u8(bytes, reader)
  end
  picker.multiplier = read_i32(bytes, reader)
  picker.walk_step = read_i32(bytes, reader)
  picker.walk_bias = read_i32(bytes, reader)
end

-- Returns the slice's first index into route_indices, then count.
local function read_indices(bytes, reader, out, count)
  local first = out.route_index_count
  count = read_u8(bytes, reader)
  if first + count > M.SAVE_STATE_ROUTE_INDICES_CAP then
    reader.failed = 1
    count = 0
    return first, count
  end
  for _ = 0, count - 1 do
    out.route_indices[out.route_index_count] = read_u8(bytes, reader)
    out.route_index_count = out.route_index_count + 1
  end
  return first, count
end

local function read_route(bytes, reader, out, route)
  route.first_planet, route.planet_count = read_indices(bytes, reader, out, route.planet_count)
  route.first_slipway, route.slipway_count = read_indices(bytes, reader, out, route.slipway_count)
  local resource_flags = read_u8(bytes, reader)
  if bit.band(resource_flags, bit.bnot(3)) ~= 0 then
    reader.failed = 1
    return
  end
  route.resource_is_bool = bit.band(bit.rshift(resource_flags, 1), 1)
  if bit.band(resource_flags, 1) ~= 0 then
    route.resource = read_i32(bytes, reader)
  end
end

local function read_node(bytes, reader, out, node)
  node.kind = read_u8(bytes, reader)
  node.state = read_u8(bytes, reader)
  node.flags = read_u16(bytes, reader)
  if bit.band(node.flags, M.SAVE_NODE_HAS_POS) ~= 0 then
    node.pos = read_vec(bytes, reader)
  end
  if bit.band(node.flags, M.SAVE_NODE_HAS_MAP_POS) ~= 0 then
    node.map_pos = read_vec(bytes, reader)
  end
  if bit.band(node.flags, M.SAVE_NODE_HAS_PLANET_ID) ~= 0 then
    local length = read_u8(bytes, reader)
    if length == 0 or length > M.SAVE_STATE_PLANET_ID_MAX then
      reader.failed = 1
      return
    end
    for _ = 0, length - 1 do
      local character = read_u8(bytes, reader)
      if character < 0x21 or character > 0x7E then
        reader.failed = 1
        return
      end
      node.planet_id = node.planet_id .. string.char(character)
    end
  end
  node.fold_anchor = read_i32(bytes, reader)
  node.infraspace = read_i32(bytes, reader)
  node.planet_type = read_u8(bytes, reader)
  node.description_type = read_u8(bytes, reader)
  node.project = read_u8(bytes, reader)
  if bit.band(node.flags, M.SAVE_NODE_HAS_NEEDS) ~= 0 then
    node.need_count = read_u8(bytes, reader)
    if node.need_count > M.SAVE_STATE_NEEDS_CAP then
      reader.failed = 1
      return
    end
    for i = 0, node.need_count - 1 do
      local resource_flag = read_u8(bytes, reader)
      if resource_flag > 1 then
        reader.failed = 1
        return
      end
      if resource_flag == 1 then
        node.needs[i].resource = read_i32(bytes, reader)
      end
    end
  end
  if bit.band(node.flags, M.SAVE_NODE_HAS_DEVELOPMENT_TIER) ~= 0 then
    node.development_tier = read_i32(bytes, reader)
  end
  if bit.band(node.flags, M.SAVE_NODE_HAS_PRODUCTION_BONUS) ~= 0 then
    node.production_bonus = read_i32(bytes, reader)
  end
  if bit.band(node.flags, M.SAVE_NODE_PRODUCTION_HAS_RESOURCE) ~= 0 then
    node.production_resource = read_i32(bytes, reader)
  end
  if bit.band(node.flags, M.SAVE_NODE_HAS_STATE_TIME) ~= 0 then
    node.state_time = read_i32(bytes, reader)
  end
  node.first_route = out.route_count
  if bit.band(node.flags, M.SAVE_NODE_HAS_OUTGOING_ROUTES) ~= 0 then
    node.route_count = read_u16(bytes, reader)
    if node.first_route + node.route_count > M.SAVE_STATE_ROUTES_CAP then
      reader.failed = 1
      return
    end
    local i = 0
    while i < node.route_count and reader.failed == 0 do
      local route = save_route_zero()
      out.routes[out.route_count] = route
      out.route_count = out.route_count + 1
      read_route(bytes, reader, out, route)
      i = i + 1
    end
  end
  node.first_slipway = out.node_slipway_count
  node.slipway_count = read_u8(bytes, reader)
  if node.first_slipway + node.slipway_count > M.SAVE_STATE_NODE_SLIPWAYS_CAP then
    reader.failed = 1
    return
  end
  for _ = 0, node.slipway_count - 1 do
    out.node_slipways[out.node_slipway_count] = read_u8(bytes, reader)
    out.node_slipway_count = out.node_slipway_count + 1
  end
end

local function read_payload(bytes, reader, out)
  out.difficulty = read_u8(bytes, reader)
  read_economy(bytes, reader, out.economy)
  if reader.failed ~= 0 then
    return
  end
  out.rnd_state = read_u32(bytes, reader)
  out.background_seed = read_u32(bytes, reader)
  read_type_picker(bytes, reader, out.type_picker)
  if reader.failed ~= 0 then
    return
  end
  out.vanish_walk.step = read_i32(bytes, reader)
  out.vanish_walk.bias = read_i32(bytes, reader)
  out.structure_id_counter = read_i32(bytes, reader)
  local probe_flag = read_u8(bytes, reader)
  if probe_flag > 1 then
    reader.failed = 1
    return
  end
  if probe_flag == 1 then
    out.probe = {}
    out.probe.pos = read_vec(bytes, reader)
    out.probe.map_pos = read_vec(bytes, reader)
    out.probe.fold_anchor = read_i32(bytes, reader)
  end
  out.camera_map_pos = read_vec(bytes, reader)
  out.node_count = read_u8(bytes, reader)
  local i = 0
  while i < out.node_count and reader.failed == 0 do
    out.nodes[i] = save_node_zero()
    read_node(bytes, reader, out, out.nodes[i])
    i = i + 1
  end
  out.slipway_count = read_u8(bytes, reader)
  for k = 0, out.slipway_count - 1 do
    out.slipways[k] = save_slipway_zero()
    out.slipways[k].start = read_u8(bytes, reader)
    out.slipways[k].end_ = read_u8(bytes, reader)
    out.slipways[k].state_time = read_i32(bytes, reader)
  end
end

local function is_flag(value)
  return (value == 0 or value == 1) and 1 or 0
end

local function economy_is_valid(economy)
  local seen = zero_array(map_tables.TECHNOLOGIES_COUNT)
  for i = 0, economy.invented_count - 1 do
    local tech = economy.invented[i]
    if tech >= map_tables.TECHNOLOGIES_COUNT or seen[tech] ~= 0 then
      return 0
    end
    seen[tech] = 1
  end
  return 1
end

local function type_picker_is_valid(picker)
  for i = 0, picker.tickets_count - 1 do
    if picker.tickets[i] >= game_data.PLANET_TYPES_COUNT then
      return 0
    end
  end
  for i = 0, M.SAVE_STATE_RECENT_COUNT - 1 do
    if is_flag(picker.recent_set[i]) == 0 or (picker.recent_set[i] ~= 0 and picker.recent[i] >= game_data.PLANET_TYPES_COUNT) then
      return 0
    end
  end
  return 1
end

local function node_state_is_valid(state)
  return (state ~= entity_class.ENTITY_STATE_NONE and state ~= entity_class.ENTITY_STATE_PLACING and
          state ~= entity_class.ENTITY_STATE_UNFOLDING and state < entity_class.STATE_COUNT) and 1 or 0
end

local function has(node, flag)
  return (bit.band(node.flags, flag) ~= 0) and 1 or 0
end

-- A planet reads its clock only while vanishing; every other kind animates with it.
local function node_has_state_time(kind, state)
  return (state == entity_class.ENTITY_STATE_VANISHING or kind ~= M.SAVE_NODE_PLANET) and 1 or 0
end

local function node_flags_are_valid(node)
  if bit.band(node.flags, bit.bnot(M.SAVE_NODE_KNOWN_FLAGS)) ~= 0 then
    return 0
  end
  if has(node, M.SAVE_NODE_PRODUCTION_HAS_RESOURCE) ~= 0 and has(node, M.SAVE_NODE_HAS_PRODUCTION) == 0 then
    return 0
  end
  if has(node, M.SAVE_NODE_SYNTHESIZER_SPENT) ~= 0 and node.kind ~= M.SAVE_NODE_SYNTHESIZER then
    return 0
  end
  return (has(node, M.SAVE_NODE_HAS_STATE_TIME) == node_has_state_time(node.kind, node.state)) and 1 or 0
end

local function is_resource_code(resource)
  for i = 0, map_tables.RESOURCE_CODE_COUNT - 1 do
    if resource == bit.lshift(map_tables.resource_codes[i].resource, 16) then
      return 1
    end
  end
  return 0
end

local function resources_are_valid(node)
  for i = 0, node.need_count - 1 do
    local need = node.needs[i]
    if need.resource ~= nil and is_resource_code(need.resource) == 0 then
      return 0
    end
  end
  return (has(node, M.SAVE_NODE_PRODUCTION_HAS_RESOURCE) == 0 or is_resource_code(node.production_resource) ~= 0) and 1 or 0
end

local function node_is_valid(node)
  return (node.kind < M.SAVE_NODE_KIND_COUNT and node_state_is_valid(node.state) ~= 0 and node_flags_are_valid(node) ~= 0 and
          node.planet_type <= game_data.PLANET_TYPES_COUNT and node.description_type <= game_data.PLANET_TYPES_COUNT and
          node.project <= map_tables.PROJECT_RECORDS_COUNT and resources_are_valid(node) ~= 0) and 1 or 0
end

local function nodes_are_valid(state)
  for i = 0, state.node_count - 1 do
    if node_is_valid(state.nodes[i]) == 0 then
      return 0
    end
  end
  return 1
end

local function indices_are_below(state, first, count, limit)
  for i = 0, count - 1 do
    if state.route_indices[first + i] >= limit then
      return 0
    end
  end
  return 1
end

local function routes_are_valid(state)
  for i = 0, state.route_count - 1 do
    local route = state.routes[i]
    if indices_are_below(state, route.first_planet, route.planet_count, state.node_count) == 0 or
       indices_are_below(state, route.first_slipway, route.slipway_count, state.slipway_count) == 0 then
      return 0
    end
  end
  return 1
end

local function slipways_are_valid(state)
  for i = 0, state.slipway_count - 1 do
    local slipway = state.slipways[i]
    if slipway.start >= state.node_count or slipway.end_ >= state.node_count or slipway.start == slipway.end_ then
      return 0
    end
  end
  return 1
end

local function node_slipways_are_valid(state)
  for i = 0, state.node_count - 1 do
    local node = state.nodes[i]
    local seen = zero_array(M.SAVE_STATE_SLIPWAYS_CAP)
    for k = 0, node.slipway_count - 1 do
      local index = state.node_slipways[node.first_slipway + k]
      if index >= state.slipway_count or seen[index] ~= 0 or
         (state.slipways[index].start ~= i and state.slipways[index].end_ ~= i) then
        return 0
      end
      seen[index] = 1
    end
  end
  return 1
end

local function state_is_valid(state)
  return (state.difficulty >= 1 and state.difficulty <= game_data.DIFFICULTY_OPTIONS_COUNT and economy_is_valid(state.economy) ~= 0 and
          type_picker_is_valid(state.type_picker) ~= 0 and nodes_are_valid(state) ~= 0 and
          slipways_are_valid(state) ~= 0 and routes_are_valid(state) ~= 0 and node_slipways_are_valid(state) ~= 0) and 1 or 0
end

-- Returns 1 for a valid record, 0 otherwise, never asserts on the bytes. On 0,
-- out is unspecified.
function M.save_state_decode(bytes, count, out)
  local reader = { position = MAGIC_LENGTH, end_ = count, failed = 0 }
  local trailer = { position = count - TRAILER_BYTES, end_ = count, failed = 0 }
  if count < HEADER_BYTES + TRAILER_BYTES or count > M.SAVE_STATE_MAX_BYTES then
    return 0
  end
  for i = 0, MAGIC_LENGTH - 1 do
    if bytes[i] ~= magic[i] then
      return 0
    end
  end
  if read_u16(bytes, reader) ~= M.SAVE_STATE_VERSION then
    return 0
  end
  if read_u16(bytes, reader) ~= count - HEADER_BYTES - TRAILER_BYTES then
    return 0
  end
  reader.end_ = count - TRAILER_BYTES
  if read_u32(bytes, trailer) ~= M.save_state_crc32(bytes, reader.end_) then
    return 0
  end
  save_state_zero(out)
  read_payload(bytes, reader, out)
  return (reader.failed == 0 and reader.position == reader.end_ and state_is_valid(out) ~= 0) and 1 or 0
end

M.crc32_table = {
  [0] = 0x00000000, 0x77073096, 0xEE0E612C, 0x990951BA, 0x076DC419, 0x706AF48F,
  0xE963A535, 0x9E6495A3, 0x0EDB8832, 0x79DCB8A4, 0xE0D5E91E, 0x97D2D988,
  0x09B64C2B, 0x7EB17CBD, 0xE7B82D07, 0x90BF1D91, 0x1DB71064, 0x6AB020F2,
  0xF3B97148, 0x84BE41DE, 0x1ADAD47D, 0x6DDDE4EB, 0xF4D4B551, 0x83D385C7,
  0x136C9856, 0x646BA8C0, 0xFD62F97A, 0x8A65C9EC, 0x14015C4F, 0x63066CD9,
  0xFA0F3D63, 0x8D080DF5, 0x3B6E20C8, 0x4C69105E, 0xD56041E4, 0xA2677172,
  0x3C03E4D1, 0x4B04D447, 0xD20D85FD, 0xA50AB56B, 0x35B5A8FA, 0x42B2986C,
  0xDBBBC9D6, 0xACBCF940, 0x32D86CE3, 0x45DF5C75, 0xDCD60DCF, 0xABD13D59,
  0x26D930AC, 0x51DE003A, 0xC8D75180, 0xBFD06116, 0x21B4F4B5, 0x56B3C423,
  0xCFBA9599, 0xB8BDA50F, 0x2802B89E, 0x5F058808, 0xC60CD9B2, 0xB10BE924,
  0x2F6F7C87, 0x58684C11, 0xC1611DAB, 0xB6662D3D, 0x76DC4190, 0x01DB7106,
  0x98D220BC, 0xEFD5102A, 0x71B18589, 0x06B6B51F, 0x9FBFE4A5, 0xE8B8D433,
  0x7807C9A2, 0x0F00F934, 0x9609A88E, 0xE10E9818, 0x7F6A0DBB, 0x086D3D2D,
  0x91646C97, 0xE6635C01, 0x6B6B51F4, 0x1C6C6162, 0x856530D8, 0xF262004E,
  0x6C0695ED, 0x1B01A57B, 0x8208F4C1, 0xF50FC457, 0x65B0D9C6, 0x12B7E950,
  0x8BBEB8EA, 0xFCB9887C, 0x62DD1DDF, 0x15DA2D49, 0x8CD37CF3, 0xFBD44C65,
  0x4DB26158, 0x3AB551CE, 0xA3BC0074, 0xD4BB30E2, 0x4ADFA541, 0x3DD895D7,
  0xA4D1C46D, 0xD3D6F4FB, 0x4369E96A, 0x346ED9FC, 0xAD678846, 0xDA60B8D0,
  0x44042D73, 0x33031DE5, 0xAA0A4C5F, 0xDD0D7CC9, 0x5005713C, 0x270241AA,
  0xBE0B1010, 0xC90C2086, 0x5768B525, 0x206F85B3, 0xB966D409, 0xCE61E49F,
  0x5EDEF90E, 0x29D9C998, 0xB0D09822, 0xC7D7A8B4, 0x59B33D17, 0x2EB40D81,
  0xB7BD5C3B, 0xC0BA6CAD, 0xEDB88320, 0x9ABFB3B6, 0x03B6E20C, 0x74B1D29A,
  0xEAD54739, 0x9DD277AF, 0x04DB2615, 0x73DC1683, 0xE3630B12, 0x94643B84,
  0x0D6D6A3E, 0x7A6A5AA8, 0xE40ECF0B, 0x9309FF9D, 0x0A00AE27, 0x7D079EB1,
  0xF00F9344, 0x8708A3D2, 0x1E01F268, 0x6906C2FE, 0xF762575D, 0x806567CB,
  0x196C3671, 0x6E6B06E7, 0xFED41B76, 0x89D32BE0, 0x10DA7A5A, 0x67DD4ACC,
  0xF9B9DF6F, 0x8EBEEFF9, 0x17B7BE43, 0x60B08ED5, 0xD6D6A3E8, 0xA1D1937E,
  0x38D8C2C4, 0x4FDFF252, 0xD1BB67F1, 0xA6BC5767, 0x3FB506DD, 0x48B2364B,
  0xD80D2BDA, 0xAF0A1B4C, 0x36034AF6, 0x41047A60, 0xDF60EFC3, 0xA867DF55,
  0x316E8EEF, 0x4669BE79, 0xCB61B38C, 0xBC66831A, 0x256FD2A0, 0x5268E236,
  0xCC0C7795, 0xBB0B4703, 0x220216B9, 0x5505262F, 0xC5BA3BBE, 0xB2BD0B28,
  0x2BB45A92, 0x5CB36A04, 0xC2D7FFA7, 0xB5D0CF31, 0x2CD99E8B, 0x5BDEAE1D,
  0x9B64C2B0, 0xEC63F226, 0x756AA39C, 0x026D930A, 0x9C0906A9, 0xEB0E363F,
  0x72076785, 0x05005713, 0x95BF4A82, 0xE2B87A14, 0x7BB12BAE, 0x0CB61B38,
  0x92D28E9B, 0xE5D5BE0D, 0x7CDCEFB7, 0x0BDBDF21, 0x86D3D2D4, 0xF1D4E242,
  0x68DDB3F8, 0x1FDA836E, 0x81BE16CD, 0xF6B9265B, 0x6FB077E1, 0x18B74777,
  0x88085AE6, 0xFF0F6A70, 0x66063BCA, 0x11010B5C, 0x8F659EFF, 0xF862AE69,
  0x616BFFD3, 0x166CCF45, 0xA00AE278, 0xD70DD2EE, 0x4E048354, 0x3903B3C2,
  0xA7672661, 0xD06016F7, 0x4969474D, 0x3E6E77DB, 0xAED16A4A, 0xD9D65ADC,
  0x40DF0B66, 0x37D83BF0, 0xA9BCAE53, 0xDEBB9EC5, 0x47B2CF7F, 0x30B5FFE9,
  0xBDBDF21C, 0xCABAC28A, 0x53B39330, 0x24B4A3A6, 0xBAD03605, 0xCDD70693,
  0x54DE5729, 0x23D967BF, 0xB3667A2E, 0xC4614AB8, 0x5D681B02, 0x2A6F2B94,
  0xB40BBE37, 0xC30C8EA1, 0x5A05DF1B, 0x2D02EF8D,
}
crc32_table = M.crc32_table

-- CRC-32 IEEE 802.3, reflected: "123456789" gives 0xCBF43926.
function M.save_state_crc32(bytes, count)
  local crc = 0xFFFFFFFF
  for i = 0, count - 1 do
    crc = bit.bxor(crc32_table[bit.band(bit.bxor(crc, bytes[i]), 0xFF)], bit.rshift(crc, 8))
  end
  crc = bit.bxor(crc, 0xFFFFFFFF)
  if crc < 0 then
    crc = crc + 0x100000000
  end
  return crc
end


local function vec_of(vec)
  local out = {}
  out.x = vec.x
  out.y = vec.y
  return out
end

local function difficulty_index()
  local difficulty = economy_score.economy_score_difficulty()
  assert(difficulty ~= nil and difficulty.name ~= nil, "save_state_capture: a running game has a difficulty")
  for i = 0, game_data.DIFFICULTY_OPTIONS_COUNT - 1 do
    if game_data.difficulty_options_table[i].name == difficulty.name then
      return i + 1
    end
  end
  assert(false, "save_state_capture: the difficulty is not a difficulty_options_table entry")
  return 0
end

local function technology_index(name)
  for i = 0, map_tables.TECHNOLOGIES_COUNT - 1 do
    if map_tables.technologies_table[i].name == name then
      return i
    end
  end
  assert(false, "save_state_capture: an invented tech is not a technologies_table entry")
  return 0
end

-- Lua only: planet_types_table entry -> its C index.
local planet_type_index_of = {}
for i = 0, game_data.PLANET_TYPES_COUNT - 1 do
  planet_type_index_of[game_data.planet_types_table[i]] = i
end

local function planet_type_index(type)
  assert(planet_type_index_of[type] ~= nil,
    "save_state_capture: a planet type is not a planet_types_table entry")
  return planet_type_index_of[type]
end

-- The 1-based planet_types_table entry that owns the description pointer
-- (the current type first, terraform leaves the old one's); 0 for NULL.
local function description_type_index(planet)
  if planet.description == nil then
    return 0
  end
  if planet.planet_type ~= nil and planet.description == planet.planet_type.description then
    return planet_type_index(planet.planet_type) + 1
  end
  for i = 0, game_data.PLANET_TYPES_COUNT - 1 do
    if planet.description == game_data.planet_types_table[i].description then
      return i + 1
    end
  end
  assert(false, "save_state_capture: a planet description is not a planet_types_table entry's")
  return 0
end

local function capture_economy(out)
  local economy = probe.main_economy_typed()
  assert(economy ~= nil, "save_state_capture: a running game has an economy")
  assert(#economy.invented <= map_tables.TECHNOLOGIES_COUNT)
  out.credits = economy.credits
  out.research_points = economy.research_points
  out.era = economy.era
  out.year = economy.year
  out.year_ticks = economy.year_ticks
  out.invented_count = #economy.invented
  for i = 0, #economy.invented - 1 do
    out.invented[i] = technology_index(economy.invented[i + 1])
  end
end

local function capture_type_picker(out)
  local picker = planet_module.planet_type_picker()
  local walk
  assert(picker ~= nil, "save_state_capture: a running game has a type picker")
  walk = gen.weighted_picker_walk(picker)
  assert(walk ~= nil, "save_state_capture: the type picker draws from a random walk")
  out.tickets_count = picker.tickets_count
  for i = 0, picker.tickets_count - 1 do
    out.tickets[i] = planet_type_index(picker.tickets[i])
  end
  for i = 0, M.SAVE_STATE_RECENT_COUNT - 1 do
    out.recent_set[i] = picker.recent_set[i] and 1 or 0
    out.recent[i] = picker.recent_set[i] and planet_type_index(picker.recent[i]) or 0
  end
  out.multiplier = picker.multiplier
  out.walk_step = walk.step
  out.walk_bias = walk.bias
end

-- The tag's entities in registration order; 0 entities when the tag has no list.
local function tagged_count(tag)
  local list = entity_registry.entities_by_tag(tag)
  return (list ~= nil) and #list or 0
end

local function tagged_at(tag, index)
  return entity_registry.entities_by_tag(tag)[index]
end

local function capture_probe(out)
  for i = 1, tagged_count(entity_class.TAG_PROBE) do
    local e = tagged_at(entity_class.TAG_PROBE, i)
    if e.class ~= probe.probe_class() or e.is_dead or e.state ~= entity_class.ENTITY_STATE_SETTLED then
      goto continue
    end
    out.probe = {
      pos = vec_of(entity_class.entity_pos(e)),
      map_pos = vec_of(entity_class.entity_map_pos(e)),
      fold_anchor = e.fold_anchor and 0x10000 or 0,
    }
    do return end
    ::continue::
  end
end

local function is_saved(e)
  return (not e.is_dead and e.state ~= entity_class.ENTITY_STATE_PLACING) and 1 or 0
end

local function node_kind(cls)
  local classes = {}
  classes[M.SAVE_NODE_PLANET] = planet_module.planet_class()
  classes[M.SAVE_NODE_PROTOSTAR] = planet_family.protostar_class()
  classes[M.SAVE_NODE_STRUCTURE] = planet_family.structure_class()
  classes[M.SAVE_NODE_PROCESSOR] = planet_family.processor_class()
  classes[M.SAVE_NODE_SLIPGATE] = planet_family_trade.slipgate_class()
  classes[M.SAVE_NODE_INFRASPACE_GATE] = planet_family_trade.infraspace_gate_class()
  classes[M.SAVE_NODE_TRADING_HUB] = planet_family.trading_hub_class()
  classes[M.SAVE_NODE_SYNTHESIZER] = planet_family_trade.synthesizer_class()
  classes[M.SAVE_NODE_ASCENSION] = planet_family.ascension_class()
  for i = 0, M.SAVE_NODE_KIND_COUNT - 1 do
    if cls == classes[i] then
      return i
    end
  end
  assert(false, "save_state_capture: a TAG_NODE entity is not one of the nine node classes")
  return 0
end

local function project_index(id)
  local project = research.research_project_get(id)
  if project == nil then
    return 0
  end
  for k = 1, research.research_projects_count() do
    if research.research_projects_at(k) == project then
      return k
    end
  end
  assert(false, "save_state_capture: a planet's project is not in the research project table")
  return 0
end

-- The ids of the captured nodes and slipways, in save order:
-- { nodes, node_count, slipways, slipway_count }, both id arrays 0-indexed.

local function index_of(ids, count, id)
  for i = 0, count - 1 do
    if ids[i] == id then
      return i
    end
  end
  assert(false, "save_state_capture: a route, slipway or slipway list names an entity the save did not capture")
  return 0
end

local function capture_node_fields(node, planet)
  local header = planet
  local id = planet_module.planet_id_or_null(planet)

  node.kind = node_kind(header.class)
  node.state = bit.band(header.state, 0xFF)
  if entity_class.entity_has_pos(header) then
    node.flags = bit.bor(node.flags, M.SAVE_NODE_HAS_POS)
    node.pos = vec_of(header.pos)
  end
  if entity_class.entity_has_map_pos(header) then
    node.flags = bit.bor(node.flags, M.SAVE_NODE_HAS_MAP_POS)
    node.map_pos = vec_of(header.map_pos)
  end
  if id ~= nil then
    assert(#id <= M.SAVE_STATE_PLANET_ID_MAX)
    node.flags = bit.bor(node.flags, M.SAVE_NODE_HAS_PLANET_ID)
    node.planet_id = id
  end
  -- Saved as the cart's fixed-point 1.0 or 0.
  node.fold_anchor = header.fold_anchor and 0x10000 or 0
  node.infraspace = header.infraspace and 0x10000 or 0
  if planet.planet_type ~= nil then
    node.planet_type = planet_type_index(planet.planet_type) + 1
  end
  node.description_type = description_type_index(planet)
  node.project = project_index(planet.project)
  if planet.needs ~= nil then
    assert(#planet.needs <= M.SAVE_STATE_NEEDS_CAP)
    node.flags = bit.bor(node.flags, M.SAVE_NODE_HAS_NEEDS)
    node.need_count = #planet.needs
    for i = 0, #planet.needs - 1 do
      local need = planet.needs[i + 1]
      assert(need ~= nil)
      node.needs[i].resource = need.row.resource
    end
  end
  if planet.development_tier ~= nil then
    node.flags = bit.bor(node.flags, M.SAVE_NODE_HAS_DEVELOPMENT_TIER)
    node.development_tier = planet.development_tier
  end
  if planet.production_bonus ~= nil then
    node.flags = bit.bor(node.flags, M.SAVE_NODE_HAS_PRODUCTION_BONUS)
    node.production_bonus = planet.production_bonus
  end
  if planet.production ~= nil then
    node.flags = bit.bor(node.flags, M.SAVE_NODE_HAS_PRODUCTION)
    if planet.production.resource ~= nil then
      node.flags = bit.bor(node.flags, M.SAVE_NODE_PRODUCTION_HAS_RESOURCE)
      node.production_resource = planet.production.resource
    end
  end
  if planet.synthesizable_resources ~= planet.archetype.synthesizable_resources then
    node.flags = bit.bor(node.flags, M.SAVE_NODE_SYNTHESIZER_SPENT)
  end
  if node_has_state_time(node.kind, node.state) ~= 0 then
    node.flags = bit.bor(node.flags, M.SAVE_NODE_HAS_STATE_TIME)
    node.state_time = header.state_time
  end
end

-- Returns first, then the C's *added.
local function add_route_indices(out, list, ids, count)
  local first = out.route_index_count
  assert(#list <= 0xFF and first + #list <= M.SAVE_STATE_ROUTE_INDICES_CAP)
  for i = 1, #list do
    out.route_indices[out.route_index_count] = index_of(ids, count, list[i])
    out.route_index_count = out.route_index_count + 1
  end
  return first, #list
end

local function capture_node_links(out, node, planet, ids)
  node.first_route = out.route_count
  if planet.outgoing_routes ~= nil then
    local count = #planet.outgoing_routes
    node.flags = bit.bor(node.flags, M.SAVE_NODE_HAS_OUTGOING_ROUTES)
    assert(out.route_count + count <= M.SAVE_STATE_ROUTES_CAP)
    for i = 1, count do
      local route = planet.outgoing_routes[i]
      local saved = save_route_zero()
      out.routes[out.route_count] = saved
      out.route_count = out.route_count + 1
      saved.first_planet, saved.planet_count = add_route_indices(out, route.planets, ids.nodes, ids.node_count)
      saved.first_slipway, saved.slipway_count = add_route_indices(out, route.slipways, ids.slipways, ids.slipway_count)
      saved.resource_is_bool = (route.resource_is_bool ~= 0) and 1 or 0
      saved.resource = route.resource
    end
    node.route_count = count
  end
  node.first_slipway = out.node_slipway_count
  assert(#planet.slipways <= 0xFF and out.node_slipway_count + #planet.slipways <= M.SAVE_STATE_NODE_SLIPWAYS_CAP)
  for i = 1, #planet.slipways do
    out.node_slipways[out.node_slipway_count] = index_of(ids.slipways, ids.slipway_count, planet.slipways[i])
    out.node_slipway_count = out.node_slipway_count + 1
  end
  node.slipway_count = #planet.slipways
end

local function capture_world(out, ids)
  for i = 1, tagged_count(entity_class.TAG_NODE) do
    local e = tagged_at(entity_class.TAG_NODE, i)
    if is_saved(e) ~= 0 then
      assert(ids.node_count < M.SAVE_STATE_NODES_CAP, "save_state_capture: node indices are one byte")
      ids.nodes[ids.node_count] = entity_id.entity_id_link(e)
      ids.node_count = ids.node_count + 1
    end
  end
  for i = 1, tagged_count(entity_class.TAG_SLIPWAY) do
    local e = tagged_at(entity_class.TAG_SLIPWAY, i)
    local slipway = e
    local saved
    if is_saved(e) == 0 or slipway.end_is_point then
      goto continue
    end
    assert(ids.slipway_count < M.SAVE_STATE_SLIPWAYS_CAP, "save_state_capture: slipway indices are one byte")
    ids.slipways[ids.slipway_count] = entity_id.entity_id_link(e)
    ids.slipway_count = ids.slipway_count + 1
    saved = save_slipway_zero()
    out.slipways[out.slipway_count] = saved
    out.slipway_count = out.slipway_count + 1
    saved.start = index_of(ids.nodes, ids.node_count, slipway.start)
    saved.end_ = index_of(ids.nodes, ids.node_count, slipway.end_)
    saved.state_time = e.state_time
    ::continue::
  end
  for i = 0, ids.node_count - 1 do
    local planet = entity_id.entity_get(ids.nodes[i])
    local node = save_node_zero()
    out.nodes[out.node_count] = node
    out.node_count = out.node_count + 1
    capture_node_fields(node, planet)
    capture_node_links(out, node, planet, ids)
  end
end

function M.save_state_capture(out)
  local ids = {}
  local vanish_walk = planet_module.planet_vanish_roll()
  local background = background_module.background_live()
  local ok, counter

  assert(vanish_walk ~= nil, "save_state_capture: a running game has a vanish walk")
  assert(background ~= nil, "save_state_capture: a running game has a background")
  save_state_zero(out)
  ids.nodes = {}
  ids.node_count = 0
  ids.slipways = {}
  ids.slipway_count = 0
  out.difficulty = difficulty_index()
  capture_economy(out.economy)
  out.rnd_state = fixed.p8_fixed_rnd_state()
  out.background_seed = background.seed
  capture_type_picker(out.type_picker)
  out.vanish_walk.step = vanish_walk.step
  out.vanish_walk.bias = vanish_walk.bias
  ok, counter = structure_icon.structure_icon_id_counter(0)
  out.structure_id_counter = ok and counter or 0
  capture_probe(out)
  out.camera_map_pos = vec_of(entity_registry.main_camera().map_pos)
  capture_world(out, ids)
end

-- Lua only: the cart's static save_state_t starts at its C zero through this.
M.save_state_zero = save_state_zero

return M
