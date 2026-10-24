local M = {}

M.CLOSED = 0
M.TECH = 1
M.SCORE = 2

function M.init(cycle)
  cycle.panel = M.CLOSED
  cycle.landing_due = false
end

function M.panel(cycle)
  return cycle.panel
end

function M.landing_due(cycle)
  return cycle.landing_due
end

function M.set(cycle, panel)
  cycle.panel = panel
  cycle.landing_due = panel ~= M.CLOSED
end

function M.landed(cycle)
  cycle.landing_due = false
end

function M.next(panel)
  if panel == M.CLOSED then
    return M.TECH
  elseif panel == M.TECH then
    return M.SCORE
  elseif panel == M.SCORE then
    return M.CLOSED
  end
  assert(false, "pad_cycle.next: not a pad_cycle_panel_t")
  return M.CLOSED
end

function M.hud_button(panel)
  return (panel == M.TECH) and 2 or (panel == M.SCORE) and 3 or 0
end

return M
