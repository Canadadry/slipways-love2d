-- Copy of src/game/grab_arena.h then src/game/grab_arena.c.
-- The C recognises a grab arena by its name's address; Lua compares by value:
-- the same while no other zone in src/ is named "grab" (others: frame, game,
-- load, ui, slot, particles). Adding one must change grab_arena_give_back.
local allocator = require("engine.allocator")
local entity_class = require("engine.entity_class")
local game_zone = require("engine.game_zone")

local M = {}

M.GRAB_ARENAS_FREE_MAX = 8

M.grab_arena_name = "grab"

local g_grab_arena = nil
local g_grab_arenas_free = {}
local g_grab_arenas_free_n = 0

function M.grab_arena_construction_zone()
  if g_grab_arena ~= nil then
    return g_grab_arena
  end
  return game_zone.game_zone()
end

function M.grab_arena_open(bytes)
  local size = allocator.arena_size_for(bytes)
  local best = -1

  assert(g_grab_arena == nil, "a grab arena is already open")
  for i = 0, g_grab_arenas_free_n - 1 do
    local free_size = allocator.arena_size(g_grab_arenas_free[i])
    if free_size >= size and (best < 0 or free_size < allocator.arena_size(g_grab_arenas_free[best])) then
      best = i
    end
  end
  if best >= 0 then
    g_grab_arena = g_grab_arenas_free[best]
    g_grab_arenas_free_n = g_grab_arenas_free_n - 1
    g_grab_arenas_free[best] = g_grab_arenas_free[g_grab_arenas_free_n]
    allocator.arena_flush(g_grab_arena)
  else
    g_grab_arena = allocator.arena_take(game_zone.game_zone(), M.grab_arena_name, size)
  end
end

function M.grab_arena_close()
  allocator.arena_spill(g_grab_arena)
  g_grab_arena = nil
end

function M.grab_arena_give_back_if_placing(zone, state)
  if state ~= entity_class.ENTITY_STATE_PLACING then
    return
  end
  M.grab_arena_give_back(zone)
end

function M.grab_arena_give_back(zone)
  if zone == nil or zone.bump == nil or zone.bump.name ~= M.grab_arena_name then
    return
  end
  if g_grab_arenas_free_n < M.GRAB_ARENAS_FREE_MAX then
    g_grab_arenas_free[g_grab_arenas_free_n] = zone
    g_grab_arenas_free_n = g_grab_arenas_free_n + 1
    return
  end
  local smallest = 0
  for i = 1, g_grab_arenas_free_n - 1 do
    if allocator.arena_size(g_grab_arenas_free[i]) < allocator.arena_size(g_grab_arenas_free[smallest]) then
      smallest = i
    end
  end
  if allocator.arena_size(g_grab_arenas_free[smallest]) < allocator.arena_size(zone) then
    g_grab_arenas_free[smallest] = zone
  end
end

function M.grab_arena_forget_free()
  g_grab_arenas_free_n = 0
end

return M
