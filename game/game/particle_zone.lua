-- Copy of src/game/particle_zone.h then src/game/particle_zone.c.
local allocator = require("engine.allocator")
local slot_group = require("engine.slot_group")
local zone = require("engine.zone")
local zone_sizes = require("engine.zone_sizes")

local M = {}

M.PARTICLE_SLOT_PARTICLE = 0
M.PARTICLE_SLOT_LIST = 1
M.PARTICLE_SLOT_HELP_LIST = 2
M.PARTICLE_SLOT_BURST = 3
M.PARTICLE_SLOT_TRANSIT_DOT = 4
M.PARTICLE_SLOT_COUNT = 5

M.PARTICLE_SLOT = zone.ZONE_BYTES_BY_WIDTH(32, 32)
M.TRANSIT_DOTS_SLOT = zone.ZONE_BYTES_BY_WIDTH(24, 24)
M.PARTICLES_CAPACITY = 158
M.PARTICLES_HELP_SCREEN_CAPACITY = 653

M.SIZEOF = {
  pointer_list_t = 24,
  ["void *"] = 8,
}
local SIZEOF = M.SIZEOF

local PARTICLES_SIZE = allocator.ARENA_LIST_BYTES(SIZEOF.pointer_list_t, M.PARTICLES_CAPACITY, SIZEOF["void *"])
local PARTICLES_HELP_SCREEN_SIZE =
  allocator.ARENA_LIST_BYTES(SIZEOF.pointer_list_t, M.PARTICLES_HELP_SCREEN_CAPACITY, SIZEOF["void *"])
local BURST_SIZE = zone.ZONE_BYTES_BY_WIDTH(648, 424)
local PARTICLES_COUNT = 18
local PARTICLES_HELP_SCREEN_COUNT = 1
local BURST_COUNT = 4
local TRANSIT_DOTS_COUNT = 437
local PARTICLE_CAPACITY = 887

assert(PARTICLE_CAPACITY >= M.PARTICLES_HELP_SCREEN_CAPACITY, "particle_zone_holds_help_screen_t")

assert(zone.SLOT_ZONE_BYTES(M.PARTICLE_SLOT, PARTICLE_CAPACITY) + zone.SLOT_ZONE_BYTES(PARTICLES_SIZE, PARTICLES_COUNT)
  + zone.SLOT_ZONE_BYTES(PARTICLES_HELP_SCREEN_SIZE, PARTICLES_HELP_SCREEN_COUNT)
  + zone.SLOT_ZONE_BYTES(BURST_SIZE, BURST_COUNT)
  + zone.SLOT_ZONE_BYTES(M.TRANSIT_DOTS_SLOT, TRANSIT_DOTS_COUNT)
  <= zone_sizes.PARTICLES_ZONE_BYTES, "particle_zone_exceeded_t")

local particle_cells, particle_taken = zone.SLOT_ZONE_STORAGE(M.PARTICLE_SLOT, PARTICLE_CAPACITY)
local particles_cells, particles_taken = zone.SLOT_ZONE_STORAGE(PARTICLES_SIZE, PARTICLES_COUNT)
local particles_help_screen_cells, particles_help_screen_taken =
  zone.SLOT_ZONE_STORAGE(PARTICLES_HELP_SCREEN_SIZE, PARTICLES_HELP_SCREEN_COUNT)
local burst_cells, burst_taken = zone.SLOT_ZONE_STORAGE(BURST_SIZE, BURST_COUNT)
local transit_dots_cells, transit_dots_taken = zone.SLOT_ZONE_STORAGE(M.TRANSIT_DOTS_SLOT, TRANSIT_DOTS_COUNT)

M.SLOT_KINDS = {
  [M.PARTICLE_SLOT_PARTICLE] = { zone_name = "particles particle", slot_name = nil, cells = particle_cells,
    taken = particle_taken, size = M.PARTICLE_SLOT, count = PARTICLE_CAPACITY },
  [M.PARTICLE_SLOT_LIST] = { zone_name = "particles particles", slot_name = "slot particles",
    cells = particles_cells, taken = particles_taken, size = PARTICLES_SIZE, count = PARTICLES_COUNT },
  [M.PARTICLE_SLOT_HELP_LIST] = { zone_name = "particles particles_help_screen",
    slot_name = "slot particles_help_screen", cells = particles_help_screen_cells,
    taken = particles_help_screen_taken, size = PARTICLES_HELP_SCREEN_SIZE, count = PARTICLES_HELP_SCREEN_COUNT },
  [M.PARTICLE_SLOT_BURST] = { zone_name = "particles burst", slot_name = "slot burst", cells = burst_cells,
    taken = burst_taken, size = BURST_SIZE, count = BURST_COUNT },
  [M.PARTICLE_SLOT_TRANSIT_DOT] = { zone_name = "particles transit_dots", slot_name = nil,
    cells = transit_dots_cells, taken = transit_dots_taken, size = M.TRANSIT_DOTS_SLOT, count = TRANSIT_DOTS_COUNT },
}
local SLOT_KINDS = M.SLOT_KINDS

local g_group = slot_group.SLOT_GROUP(SLOT_KINDS, M.PARTICLE_SLOT_COUNT)

function M.particle_zone(slot_kind)
  return slot_group.slot_group_handle(g_group, slot_kind)
end

function M.particle_zone_slot(slot_kind)
  return slot_group.slot_group_slot(g_group, slot_kind)
end

function M.particle_zone_live(slot_kind)
  return slot_group.slot_group_live(g_group, slot_kind)
end

function M.particle_zone_max_live(slot_kind)
  return slot_group.slot_group_max_live(g_group, slot_kind)
end

function M.particle_zone_max_bytes(slot_kind)
  return slot_group.slot_group_max_bytes(g_group, slot_kind)
end

function M.particle_zone_record_elements(slot_kind, count)
  assert((slot_kind == M.PARTICLE_SLOT_LIST or slot_kind == M.PARTICLE_SLOT_HELP_LIST
    or slot_kind == M.PARTICLE_SLOT_TRANSIT_DOT), "particle_zone_record_elements: only a list has elements")
  slot_group.slot_group_record_elements(g_group, slot_kind, count)
end

function M.particle_zone_max_elements(slot_kind)
  return slot_group.slot_group_max_elements(g_group, slot_kind)
end

function M.particle_zone_used()
  return slot_group.slot_group_used(g_group)
end

function M.particle_zone_max_used()
  return slot_group.slot_group_max_used(g_group)
end

function M.particle_zone_count(slot_kind)
  return slot_group.slot_group_count(g_group, slot_kind)
end

function M.particle_zone_slot_size(slot_kind)
  return slot_group.slot_group_slot_size(g_group, slot_kind)
end

function M.particle_zone_reserved()
  return slot_group.slot_group_reserved(g_group)
end

return M
