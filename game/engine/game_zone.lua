-- Copy of src/engine/game_zone.h then src/engine/game_zone.c.
local allocator = require("engine.allocator")
local zone = require("engine.zone")
local zone_sizes = require("engine.zone_sizes")

local M = {}

local game_zone_cells = zone.FLIP_ZONE_STORAGE(zone_sizes.GENERATION_ZONE_BYTES)
local load_zone_cells = zone.BUMP_ZONE_STORAGE(zone_sizes.LOAD_ZONE_BYTES)

local game_zone_storage = { name = nil, storage = nil, capacity = 0, bottom = 0, top = 0, current_top = 0,
  pending = 0, max = 0 }
-- Static: an entity_list_t or pointer_list_t keeps a pointer to its zone.
local game_zone_handle = {}
local game_zone_allocator = nil
local load_zone_storage = { name = nil, storage = nil, capacity = 0, used = 0, half = 0 }
local load_zone_handle = {}
local load_zone_max_used = 0

local function game_zone_init_once()
  if game_zone_allocator ~= nil then
    return
  end
  zone.flip_zone_init(game_zone_storage, "game", game_zone_cells, zone_sizes.GENERATION_ZONE_BYTES)
  game_zone_allocator = allocator.allocator_from_flip_zone(game_zone_handle, game_zone_storage)
  zone.bump_zone_init(load_zone_storage, "load", load_zone_cells, zone_sizes.LOAD_ZONE_BYTES)
  load_zone_handle = allocator.allocator_from_bump_zone(load_zone_storage)
end

local function load_zone_record_max()
  if load_zone_storage.used > load_zone_max_used then
    load_zone_max_used = load_zone_storage.used
  end
end

function M.game_zone()
  game_zone_init_once()
  return game_zone_allocator
end

function M.game_zone_flip()
  game_zone_init_once()
  zone.flip_zone_flip(game_zone_storage)
end

function M.game_zone_pending()
  game_zone_init_once()
  return zone.flip_zone_pending(game_zone_storage)
end

function M.game_zone_release()
  game_zone_init_once()
  zone.flip_zone_release(game_zone_storage)
end

function M.game_zone_reset()
  game_zone_init_once()
  zone.flip_zone_reset(game_zone_storage)
  load_zone_record_max()
  zone.bump_zone_reset(load_zone_storage)
end

function M.game_zone_used()
  game_zone_init_once()
  return zone.flip_zone_used(game_zone_storage)
end

function M.game_zone_max()
  game_zone_init_once()
  return zone.flip_zone_max(game_zone_storage)
end

function M.game_zone_load_zone()
  game_zone_init_once()
  return load_zone_handle
end

function M.load_zone_used()
  game_zone_init_once()
  return load_zone_storage.used
end

function M.game_zone_load_zone_max()
  game_zone_init_once()
  load_zone_record_max()
  return load_zone_max_used
end

return M
