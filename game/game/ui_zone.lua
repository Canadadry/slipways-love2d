-- Copy of src/game/ui_zone.h then src/game/ui_zone.c.
local allocator = require("engine.allocator")
local slot_group = require("engine.slot_group")
local zone = require("engine.zone")
local zone_sizes = require("engine.zone_sizes")

local M = {}

M.SLOT_KIND_BUTTON = 0
M.SLOT_KIND_MODAL_BACKDROP = 1
M.SLOT_KIND_MESSAGE_BOX = 2
M.SLOT_KIND_TOAST = 3
M.SLOT_KIND_ROW = 4
M.SLOT_KIND_EPHEMERALS = 5
M.SLOT_KIND_COUNT = 6

M.UI_EPHEMERALS_RESERVE = 8

M.SIZEOF = {
  entity_id_list_t = 24,
  entity_id_t = 4,
}
local SIZEOF = M.SIZEOF

local BUTTON_SIZE = zone.ZONE_BYTES_BY_WIDTH(6056, 3944)
local BUTTON_COUNT = 15
local MODAL_BACKDROP_SIZE = zone.ZONE_BYTES_BY_WIDTH(280, 160)
local MODAL_BACKDROP_COUNT = 2
local MESSAGE_BOX_SIZE = zone.ZONE_BYTES_BY_WIDTH(5128, 3072)
local MESSAGE_BOX_COUNT = 4
local TOAST_SIZE = zone.ZONE_BYTES_BY_WIDTH(344, 192)
local TOAST_COUNT = 48
local ROW_SIZE = zone.ZONE_BYTES_BY_WIDTH(1184, 624)
local ROW_COUNT = 66
local EPHEMERALS_SIZE = allocator.ARENA_LIST_BYTES(SIZEOF.entity_id_list_t, M.UI_EPHEMERALS_RESERVE, SIZEOF.entity_id_t)
local EPHEMERALS_COUNT = 9

assert(zone.SLOT_ZONE_BYTES(BUTTON_SIZE, BUTTON_COUNT) + zone.SLOT_ZONE_BYTES(MODAL_BACKDROP_SIZE, MODAL_BACKDROP_COUNT)
  + zone.SLOT_ZONE_BYTES(MESSAGE_BOX_SIZE, MESSAGE_BOX_COUNT) + zone.SLOT_ZONE_BYTES(TOAST_SIZE, TOAST_COUNT)
  + zone.SLOT_ZONE_BYTES(ROW_SIZE, ROW_COUNT) + zone.SLOT_ZONE_BYTES(EPHEMERALS_SIZE, EPHEMERALS_COUNT)
  <= zone_sizes.UI_ZONE_BYTES, "ui_zone_exceeded_t")

local button_cells, button_taken = zone.SLOT_ZONE_STORAGE(BUTTON_SIZE, BUTTON_COUNT)
local modal_backdrop_cells, modal_backdrop_taken = zone.SLOT_ZONE_STORAGE(MODAL_BACKDROP_SIZE, MODAL_BACKDROP_COUNT)
local message_box_cells, message_box_taken = zone.SLOT_ZONE_STORAGE(MESSAGE_BOX_SIZE, MESSAGE_BOX_COUNT)
local toast_cells, toast_taken = zone.SLOT_ZONE_STORAGE(TOAST_SIZE, TOAST_COUNT)
local row_cells, row_taken = zone.SLOT_ZONE_STORAGE(ROW_SIZE, ROW_COUNT)
local ephemerals_cells, ephemerals_taken = zone.SLOT_ZONE_STORAGE(EPHEMERALS_SIZE, EPHEMERALS_COUNT)

M.SLOT_KINDS = {
  [M.SLOT_KIND_BUTTON] = { zone_name = "ui button", slot_name = "slot button", cells = button_cells,
    taken = button_taken, size = BUTTON_SIZE, count = BUTTON_COUNT },
  [M.SLOT_KIND_MODAL_BACKDROP] = { zone_name = "ui modal_backdrop", slot_name = "slot modal_backdrop",
    cells = modal_backdrop_cells, taken = modal_backdrop_taken, size = MODAL_BACKDROP_SIZE,
    count = MODAL_BACKDROP_COUNT },
  [M.SLOT_KIND_MESSAGE_BOX] = { zone_name = "ui message_box", slot_name = "slot message_box",
    cells = message_box_cells, taken = message_box_taken, size = MESSAGE_BOX_SIZE, count = MESSAGE_BOX_COUNT },
  [M.SLOT_KIND_TOAST] = { zone_name = "ui toast", slot_name = "slot toast", cells = toast_cells,
    taken = toast_taken, size = TOAST_SIZE, count = TOAST_COUNT },
  [M.SLOT_KIND_ROW] = { zone_name = "ui row", slot_name = "slot row", cells = row_cells, taken = row_taken,
    size = ROW_SIZE, count = ROW_COUNT },
  [M.SLOT_KIND_EPHEMERALS] = { zone_name = "ui ephemerals", slot_name = "slot ephemerals",
    cells = ephemerals_cells, taken = ephemerals_taken, size = EPHEMERALS_SIZE, count = EPHEMERALS_COUNT },
}
local SLOT_KINDS = M.SLOT_KINDS

local g_group = slot_group.SLOT_GROUP(SLOT_KINDS, M.SLOT_KIND_COUNT)

function M.ui_zone(slot_kind)
  return slot_group.slot_group_handle(g_group, slot_kind)
end

function M.ui_zone_slot(slot_kind)
  return slot_group.slot_group_slot(g_group, slot_kind)
end

function M.ui_zone_live(slot_kind)
  return slot_group.slot_group_live(g_group, slot_kind)
end

function M.ui_zone_max_live(slot_kind)
  return slot_group.slot_group_max_live(g_group, slot_kind)
end

function M.ui_zone_max_bytes(slot_kind)
  return slot_group.slot_group_max_bytes(g_group, slot_kind)
end

function M.ui_zone_used()
  return slot_group.slot_group_used(g_group)
end

function M.ui_zone_max_used()
  return slot_group.slot_group_max_used(g_group)
end

function M.ui_zone_record_ephemerals(count)
  slot_group.slot_group_record_elements(g_group, M.SLOT_KIND_EPHEMERALS, count)
end

function M.ui_zone_max_ephemerals()
  return slot_group.slot_group_max_elements(g_group, M.SLOT_KIND_EPHEMERALS)
end

function M.ui_zone_count(slot_kind)
  return slot_group.slot_group_count(g_group, slot_kind)
end

function M.ui_zone_slot_size(slot_kind)
  return slot_group.slot_group_slot_size(g_group, slot_kind)
end

function M.ui_zone_reserved()
  return slot_group.slot_group_reserved(g_group)
end

return M
