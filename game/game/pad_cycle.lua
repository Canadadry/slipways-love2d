-- Copy of src/game/pad_cycle.h then src/game/pad_cycle.c.
--
-- Shapes, with the C field names:
--   pad_cycle_t: { panel, landing_due }.
local M = {}

M.PAD_CYCLE_CLOSED = 0
M.PAD_CYCLE_TECH = 1
M.PAD_CYCLE_SCORE = 2

function M.pad_cycle_init(cycle)
  cycle.panel = M.PAD_CYCLE_CLOSED
  cycle.landing_due = 0
end

function M.pad_cycle_panel(cycle)
  return cycle.panel
end

function M.pad_cycle_landing_due(cycle)
  return cycle.landing_due
end

function M.pad_cycle_set(cycle, panel)
  cycle.panel = panel
  cycle.landing_due = (panel ~= M.PAD_CYCLE_CLOSED) and 1 or 0
end

function M.pad_cycle_landed(cycle)
  cycle.landing_due = 0
end

function M.pad_cycle_next(panel)
  if panel == M.PAD_CYCLE_CLOSED then
    return M.PAD_CYCLE_TECH
  elseif panel == M.PAD_CYCLE_TECH then
    return M.PAD_CYCLE_SCORE
  elseif panel == M.PAD_CYCLE_SCORE then
    return M.PAD_CYCLE_CLOSED
  end
  assert(false, "pad_cycle_next: not a pad_cycle_panel_t")
  return M.PAD_CYCLE_CLOSED
end

function M.pad_cycle_hud_button(panel)
  return (panel == M.PAD_CYCLE_TECH) and 2 or (panel == M.PAD_CYCLE_SCORE) and 3 or 0
end

return M
