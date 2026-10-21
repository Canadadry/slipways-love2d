-- Copy of src/engine/frame_zone.h then src/engine/frame_zone.c.
local allocator = require("engine.allocator")
local zone = require("engine.zone")
local zone_sizes = require("engine.zone_sizes")

local M = {}

local frame_cells = zone.BUMP_ZONE_STORAGE(zone_sizes.FRAME_ZONE_BYTES)

local frame = { name = nil, storage = nil, capacity = 0, used = 0, half = 0, never_reset = 0 }
-- Static: an entity_list_t or pointer_list_t keeps a pointer to its zone.
local frame_handle = {}
local frame_ready = 0
local frame_generation = 0
local frame_max = 0

local function frame_init_once()
  if frame_ready ~= 0 then
    return
  end
  zone.bump_zone_init(frame, "frame", frame_cells, zone_sizes.FRAME_ZONE_BYTES)
  frame_handle = allocator.allocator_from_bump_zone(frame)
  frame_ready = 1
end

function M.frame_zone()
  frame_init_once()
  return frame_handle
end

local function sample_max()
  if frame.used > frame_max then
    frame_max = frame.used
  end
end

function M.frame_zone_reset()
  frame_init_once()
  sample_max()
  zone.bump_zone_reset(frame)
  frame_generation = frame_generation + 1
end

function M.frame_zone_mark()
  frame_init_once()
  local mark = { used = frame.used, generation = frame_generation }
  return mark
end

function M.frame_zone_rewind(mark)
  frame_init_once()
  assert(mark.generation == frame_generation,
    "rewind to a mark from before a reset: the room taken since then is no longer the marker's to give back")
  sample_max()
  zone.bump_zone_rewind(frame, mark.used)
end

function M.frame_zone_used()
  frame_init_once()
  return frame.used
end

function M.frame_zone_max()
  frame_init_once()
  sample_max()
  return frame_max
end

return M
