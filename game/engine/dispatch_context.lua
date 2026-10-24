local M = {}

local g_gfx = nil
local g_mem = nil
local g_input = nil

function M.set_gfx(gfx)
  g_gfx = gfx
end

function M.gfx()
  return g_gfx
end

function M.set_mem(mem)
  g_mem = mem
end

function M.mem()
  return g_mem
end

function M.set_input(input)
  g_input = input
end

function M.input()
  return g_input
end

return M
