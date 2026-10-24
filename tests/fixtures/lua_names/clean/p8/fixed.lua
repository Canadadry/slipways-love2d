local M = {}

M.ONE = 65536

function M.from_int(n)
    return n * M.ONE
end

function M.add(a, b)
    return a + b
end

return M
