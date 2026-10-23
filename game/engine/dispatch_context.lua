local M = {}

local g_gfx = nil
local g_mem = nil
local g_input = nil

function M.dispatch_context_set_gfx(gfx)
  g_gfx = gfx
end

function M.dispatch_context_gfx()
  return g_gfx
end

function M.dispatch_context_set_mem(mem)
  g_mem = mem
end

function M.dispatch_context_mem()
  return g_mem
end

function M.dispatch_context_set_input(input)
  g_input = input
end

function M.dispatch_context_input()
  return g_input
end

return M
