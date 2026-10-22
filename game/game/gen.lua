-- Copy of src/game/gen.h then src/game/gen.c.
--
-- Shapes, with the C field names:
--   random_walk_t: { step, bias }.
--   weighted_picker_t: { tickets, tickets_count, recent, recent_set,
--     multiplier, random, random_ctx }; tickets, recent and recent_set
--     indexed from 0, created when first written. A ticket and a recent entry
--     are the very planet_types_table entry; random is a function taking
--     random_ctx (a random_walk_t when random is M.random_walk_draw).
local allocator = require("engine.allocator")
local game_data = require("game.game_data")
local cint = require("p8.cint")
local fixed = require("p8.fixed")

local M = {}

M.SIZEOF = {
  random_walk_t = 8,
  weighted_picker_t = 592,
}
local SIZEOF = M.SIZEOF

function M.random_walk_new(zone, n_, b)
  local state = allocator.allocator_alloc(zone, SIZEOF.random_walk_t)
  state.step = n_
  state.bias = b
  return state
end

function M.random_walk_next(state)
  local one_minus_abs_b = fixed.p8_fixed_sub(fixed.p8_fixed_from_int(1), fixed.p8_fixed_abs(state.bias))
  local b_pos = (state.bias > 0) and state.bias or 0
  local v = fixed.p8_fixed_add(fixed.p8_fixed_rnd(one_minus_abs_b), b_pos)
  local half = fixed.P8_FIXED_LIT(0x00008000, 0.5)

  state.bias = fixed.p8_fixed_add(state.bias, fixed.p8_fixed_mul(fixed.p8_fixed_sub(half, v), state.step))
  return v
end

function M.random_walk_draw(ctx)
  return M.random_walk_next(ctx)
end

M.WEIGHTED_PICKER_TICKETS_CAP = 66

-- Return, not continue, at the first unwritten slot: slots fill out of order.
local function weighted_picker_recent_contains(state, e)
  for i = 0, 2 do
    if state.recent_set[i] == 0 then
      return 0
    end
    if state.recent[i] == e then
      return 1
    end
  end
  return 0
end

local function weighted_picker_fill(state)
  state.tickets_count = 0
  for i = 0, game_data.PLANET_TYPES_COUNT - 1 do
    local entity = game_data.planet_types_table[i]
    local count = fixed.p8_fixed_flr_int(fixed.p8_fixed_mul(entity.weight, state.multiplier))

    for _ = 1, count do
      assert(state.tickets_count < M.WEIGHTED_PICKER_TICKETS_CAP, "weighted_picker_fill: the ticket pool is sized for "
        .. "planet_types_table at a multiplier of 2; a larger table or multiplier needs a larger WEIGHTED_PICKER_TICKETS_CAP")
      state.tickets[state.tickets_count] = entity
      state.tickets_count = state.tickets_count + 1
    end
  end
end

function M.weighted_picker_new(zone, multiplier, r, r_ctx)
  local state = allocator.allocator_alloc(zone, SIZEOF.weighted_picker_t)

  state.recent = {}
  state.recent_set = {}
  for i = 0, 2 do
    state.recent[i] = nil
    state.recent_set[i] = 0
  end
  state.multiplier = multiplier
  state.random = r
  state.random_ctx = r_ctx
  state.tickets = {}
  weighted_picker_fill(state)
  return state
end

local function weighted_picker_has_drawable_ticket(state)
  for i = 0, state.tickets_count - 1 do
    if weighted_picker_recent_contains(state, state.tickets[i]) == 0 then
      return 1
    end
  end
  return 0
end

local function weighted_picker_tickets_del(state, e)
  local i = 0

  while state.tickets[i] ~= e do
    i = i + 1
  end
  state.tickets_count = state.tickets_count - 1
  while i < state.tickets_count do
    state.tickets[i] = state.tickets[i + 1]
    i = i + 1
  end
end

function M.weighted_picker_next(state)
  local e

  if weighted_picker_has_drawable_ticket(state) == 0 then
    weighted_picker_fill(state)
  end

  while true do
    local r_val = state.random(state.random_ctx)
    local len_fixed = fixed.p8_fixed_from_int(state.tickets_count)
    local idx_fixed =
      fixed.p8_fixed_flr(fixed.p8_fixed_add(fixed.p8_fixed_mul(r_val, len_fixed), fixed.p8_fixed_from_int(1)))
    local idx0 = fixed.p8_fixed_flr_int(idx_fixed) - 1

    e = state.tickets[idx0]
    if weighted_picker_recent_contains(state, e) == 0 then
      break
    end
  end

  weighted_picker_tickets_del(state, e)
  do
    local slot = 3 - cint.mod(state.tickets_count, 3)
    state.recent[slot - 1] = e
    state.recent_set[slot - 1] = 1
  end
  return e
end

-- The live walk the picker draws from, owned by the picker's zone; nil unless its source is random_walk_draw.
function M.weighted_picker_walk(state)
  if state.random ~= M.random_walk_draw then
    return nil
  end
  return state.random_ctx
end

return M
