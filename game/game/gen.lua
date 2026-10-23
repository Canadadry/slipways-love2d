-- Copy of src/game/gen.h then src/game/gen.c.
--
-- Shapes, with the C field names:
--   random_walk_t: { step, bias }.
--   weighted_picker_t: { tickets, tickets_count, recent, recent_set,
--     multiplier, random, random_ctx }; tickets, recent and recent_set
--     indexed from 0, created when first written; recent_set[i] is true once
--     slot i is written, nil before. A ticket and a recent entry
--     are the very planet_types_table entry; random is a function taking
--     random_ctx (a random_walk_t when random is M.random_walk_draw).
local game_data = require("game.game_data")
local cint = require("p8.cint")
local fixed = require("p8.fixed")

local M = {}

function M.random_walk_new(n_, b)
  local state = {}
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
    if not state.recent_set[i] then
      return false
    end
    if state.recent[i] == e then
      return true
    end
  end
  return false
end

local function weighted_picker_fill(state)
  state.tickets_count = 0
  for i = 1, game_data.PLANET_TYPES_COUNT do
    local entity = game_data.planet_types_table[i]
    local count = fixed.p8_fixed_flr_int(fixed.p8_fixed_mul(entity.weight, state.multiplier))

    for _ = 1, count do
      assert(state.tickets_count < M.WEIGHTED_PICKER_TICKETS_CAP, "weighted_picker_fill: the save format holds 0 to "
        .. "WEIGHTED_PICKER_TICKETS_CAP tickets (docs/save-format.md) and the decoder refuses more -- a picker past it "
        .. "cannot be saved")
      state.tickets[state.tickets_count] = entity
      state.tickets_count = state.tickets_count + 1
    end
  end
end

function M.weighted_picker_new(multiplier, r, r_ctx)
  local state = {}

  state.recent = {}
  state.recent_set = {}
  for i = 0, 2 do
    state.recent[i] = nil
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
    if not weighted_picker_recent_contains(state, state.tickets[i]) then
      return true
    end
  end
  return false
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

  if not weighted_picker_has_drawable_ticket(state) then
    weighted_picker_fill(state)
  end

  while true do
    local r_val = state.random(state.random_ctx)
    local len_fixed = fixed.p8_fixed_from_int(state.tickets_count)
    local idx_fixed =
      fixed.p8_fixed_flr(fixed.p8_fixed_add(fixed.p8_fixed_mul(r_val, len_fixed), fixed.p8_fixed_from_int(1)))
    local idx0 = fixed.p8_fixed_flr_int(idx_fixed) - 1

    e = state.tickets[idx0]
    if not weighted_picker_recent_contains(state, e) then
      break
    end
  end

  weighted_picker_tickets_del(state, e)
  do
    local slot = 3 - cint.mod(state.tickets_count, 3)
    state.recent[slot - 1] = e
    state.recent_set[slot - 1] = true
  end
  return e
end

-- The live walk the picker draws from; nil unless its source is random_walk_draw.
function M.weighted_picker_walk(state)
  if state.random ~= M.random_walk_draw then
    return nil
  end
  return state.random_ctx
end

return M
