-- Copy of src/devtools/state_format.h then src/devtools/state_format.c.
--
-- A state_sink_t is a table { write_line, context }: write_line(context, line) receives one whole
-- line, its "\n" included. A line_t is a table { sink, text, length }.
local entity = require("engine.entity")
local entity_class = require("engine.entity_class")
local entity_geom = require("engine.entity_geom")
local entity_registry = require("engine.entity_registry")
local vec = require("engine.vec")
local economy_score = require("game.economy_score")
local game_data = require("game.game_data")
local planet = require("game.planet")
local planet_family = require("game.planet_family")
local planet_family_trade = require("game.planet_family_trade")
local probe = require("game.probe")
local slipway = require("game.slipway")
local fixed = require("p8.fixed")

local M = {}

local LINE_CAP = 256

local function line_start(sink)
  assert(sink ~= nil and sink.write_line ~= nil)
  return { sink = sink, text = "", length = 0 }
end

local function line_add(line, format, ...)
  local written = string.format(format, ...)
  assert(#written < LINE_CAP - line.length, "state line longer than LINE_CAP")
  line.text = line.text .. written
  line.length = line.length + #written
end

local function line_end(line)
  line_add(line, "\n")
  line.sink.write_line(line.sink.context, line.text)
  line.text = ""
  line.length = 0
end

-- Plain arithmetic, never bit: -(-2^31) is 2^31, which bit wraps back to negative.
local function add_fixed(line, x)
  local v = x
  if v < 0 then
    line_add(line, "-")
    v = -v
  end
  local whole = math.floor(v / 65536)
  local hundredths = math.floor((v % 65536) * 100 / 65536)
  if hundredths == 0 then
    line_add(line, "%d", whole)
  else
    line_add(line, "%d.%02d", whole, hundredths)
  end
end

local function ra_of(e)
  if e ~= nil then
    return entity_class.entity_pos(e)
  end
  return vec.vec_make(0, 0)
end

-- World y is printed at rest, without the bob: it moves with frame timing alone.
local function add_position(line, e, camera)
  local ra = ra_of(e)
  local rest_y = ra.y
  if planet_family.planet_family_bobs(e) ~= 0 then
    rest_y = fixed.p8_fixed_sub(ra.y, planet.planet_bob_total(e.state_time))
  end
  line_add(line, " world ")
  add_fixed(line, ra.x)
  line_add(line, " ")
  add_fixed(line, rest_y)
  line_add(line, " screen ")
  add_fixed(line, fixed.p8_fixed_sub(ra.x, camera.x))
  line_add(line, " ")
  add_fixed(line, fixed.p8_fixed_sub(ra.y, camera.y))
  line_add(line, " visible %d", entity_geom.entity_geom_is_on_screen(e))
end

local function class_of(e)
  local c = e.class
  if c == planet.planet_class() then return "planet" end
  if c == planet_family.structure_class() then return "structure" end
  if c == planet_family.trading_hub_class() then return "trading_hub" end
  if c == planet_family.processor_class() then return "processor" end
  if c == planet_family.ascension_class() then return "ascension" end
  if c == planet_family.protostar_class() then return "protostar" end
  if c == planet_family_trade.synthesizer_class() then return "synthesizer" end
  if c == planet_family.burst_class() then return "burst" end
  if c == planet_family_trade.slipgate_class() then return "slipgate" end
  if c == planet_family_trade.infraspace_gate_class() then return "infraspace_gate" end
  return nil
end

local function planet_state(state)
  if state == entity_class.ENTITY_STATE_NONE then return "-" end
  if state == entity_class.ENTITY_STATE_SETTLED then return "colonized" end
  if state == entity_class.ENTITY_STATE_VANISHING then return "empty" end
  return entity.entity_state_name(state)
end

local function id_of(e)
  if e == nil or class_of(e) == nil then return "-" end
  local id = planet.planet_id_or_null(e)
  if id ~= nil then
    return id
  end
  return "-"
end

local function count_of(l)
  if l ~= nil then
    return l.count
  end
  return 0
end

local function write_clock(sink)
  local line = line_start(sink)
  local economy = probe.main_economy_typed()
  if economy == nil then
    line_add(line, "clock year - month -")
    line_end(line)
    return
  end
  line_add(line, "clock year ")
  add_fixed(line, economy.year)
  line_add(line, " month ")
  add_fixed(line, economy.year_ticks)
  line_end(line)
end

local function write_difficulty(sink)
  local line = line_start(sink)
  local difficulty = economy_score.economy_score_difficulty()
  local running = probe.main_economy_typed() ~= nil and difficulty ~= nil and difficulty.name ~= nil
  line_add(line, "difficulty %s", running and difficulty.name or "-")
  line_end(line)
end

local function write_resources(sink)
  local line = line_start(sink)
  local economy = probe.main_economy_typed()
  local placeholder = probe.main_economy_placeholder()
  line_add(line, "resources credits ")
  if economy ~= nil then
    add_fixed(line, economy.credits)
  elseif placeholder ~= nil and placeholder.has_credits ~= 0 then
    add_fixed(line, placeholder.credits)
  else
    line_add(line, "-")
  end
  line_end(line)
  line_add(line, "resources science ")
  if economy ~= nil then
    add_fixed(line, economy.research_points)
  elseif placeholder ~= nil and placeholder.has_research_points ~= 0 then
    add_fixed(line, placeholder.research_points)
  else
    line_add(line, "-")
  end
  line_end(line)
  line_add(line, "resources income ")
  if economy ~= nil then
    add_fixed(line, economy.income)
  else
    line_add(line, "-")
  end
  line_end(line)
end

local function write_techs(sink)
  local economy = probe.main_economy_typed()
  if economy == nil then return 0 end
  local line = line_start(sink)
  for i = 1, #economy.invented do
    line_add(line, "techs %s", economy.invented[i])
    line_end(line)
  end
  return #economy.invented
end

local function write_camera(sink)
  local line = line_start(sink)
  local camera = ra_of(entity_registry.main_camera())
  line_add(line, "camera world ")
  add_fixed(line, camera.x)
  line_add(line, " ")
  add_fixed(line, camera.y)
  line_end(line)
  return camera
end

local function write_planets(sink, camera)
  local line = line_start(sink)
  local planets = entity_registry.entities_by_tag(entity_class.TAG_PLANET)
  for i = 0, count_of(planets) - 1 do
    local p = planets.items[i]
    local cls = class_of(p)
    local type_name = "-"
    if p.planet_type ~= nil and p.planet_type.name ~= nil then
      type_name = p.planet_type.name
    end
    line_add(line, "planet %s %s %s %s", id_of(p), planet_state(p.state), cls ~= nil and cls or "-", type_name)
    add_position(line, p, camera)
    line_end(line)
  end
end

local function slipway_placed(e)
  return (e.state ~= entity_class.ENTITY_STATE_PLACING) and 1 or 0
end

local function write_slipways(sink)
  local line = line_start(sink)
  local jc = entity_registry.entities_by_tag(entity_class.TAG_SLIPWAY)
  local n = 0
  for i = 0, count_of(jc) - 1 do
    local s = jc.items[i]
    if slipway_placed(s) == 0 then goto continue end
    line_add(line, "slipway %s %s %s", id_of(slipway.slipway_start(s)), id_of(slipway.slipway_end(s)),
             (entity_class.entity_has(s, entity_class.ENTITY_HAS_INFRASPACE) ~= 0 and s.infraspace ~= 0) and "infragate" or "normal")
    line_end(line)
    n = n + 1
    ::continue::
  end
  return n
end

local function write_structures(sink, camera)
  local line = line_start(sink)
  local nr = entity_registry.entities_by_tag(entity_class.TAG_SLIPGATE)
  for i = 0, count_of(nr) - 1 do
    local g = nr.items[i]
    line_add(line, "structure %s %s", id_of(g),
             (g.class == planet_family_trade.infraspace_gate_class()) and "infragate" or "slipgate")
    add_position(line, g, camera)
    line_add(line, " links %d", #g.slipways)
    line_end(line)
  end
end

-- Not planet_is_colonized(): it skips labs, whose archetype has no is_planet.
local function count_colonies()
  local planets = entity_registry.entities_by_tag(entity_class.TAG_PLANET)
  local n = 0
  for i = 0, count_of(planets) - 1 do
    n = n + ((planets.items[i].state == entity_class.ENTITY_STATE_SETTLED) and 1 or 0)
  end
  return n
end

local function write_count(sink, name, count)
  local line = line_start(sink)
  line_add(line, "count %s %d", name, count)
  line_end(line)
end

function M.state_format(sink)
  write_clock(sink)
  write_difficulty(sink)
  write_resources(sink)
  local techs = write_techs(sink)
  local camera = write_camera(sink)
  write_planets(sink, camera)
  local slipways = write_slipways(sink)
  write_structures(sink, camera)
  write_count(sink, "planets", count_of(entity_registry.entities_by_tag(entity_class.TAG_PLANET)))
  write_count(sink, "colonies", count_colonies())
  write_count(sink, "slipways", slipways)
  write_count(sink, "structures", count_of(entity_registry.entities_by_tag(entity_class.TAG_SLIPGATE)))
  write_count(sink, "techs", techs)
  write_count(sink, "nodes", count_of(entity_registry.entities_by_tag(entity_class.TAG_NODE)))
end

-- Reads only state, which came out of a save_state_decode that returned 1: its difficulty, clock
-- year, resources credits and count nodes lines in state_format's format, each after prefix.
function M.state_format_save_summary(sink, state, prefix)
  assert(state ~= nil and prefix ~= nil)
  assert(state.difficulty >= 1 and state.difficulty <= game_data.DIFFICULTY_OPTIONS_COUNT)
  local line = line_start(sink)
  line_add(line, "%sdifficulty %s", prefix, game_data.difficulty_options_table[state.difficulty - 1].name)
  line_end(line)
  line_add(line, "%sclock year ", prefix)
  add_fixed(line, state.economy.year)
  line_add(line, " month ")
  add_fixed(line, state.economy.year_ticks)
  line_end(line)
  line_add(line, "%sresources credits ", prefix)
  add_fixed(line, state.economy.credits)
  line_end(line)
  line_add(line, "%scount nodes %d", prefix, state.node_count)
  line_end(line)
end

function M.state_format_stopped(sink, frame)
  local line = line_start(sink)
  line_add(line, "stopped p8_fatal frame %d", frame)
  line_end(line)
end

function M.state_format_cheat_activations(sink, count)
  local line = line_start(sink)
  line_add(line, "cheat activations %d", count)
  line_end(line)
end

return M
