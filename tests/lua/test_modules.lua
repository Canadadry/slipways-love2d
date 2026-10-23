-- Requires every module of the game directory under plain LuaJIT and reports
-- each one that fails to load.
-- Usage, from the repository root:
--   luajit tests/lua/test_modules.lua <game dir> <file.lua>...

-- Run by the fake in their own check: they define LÖVE callbacks.
local SKIPPED = { main = true, conf = true }

local function usage_failure(message)
    io.stderr:write("lua-test-modules: " .. message .. "\n")
    io.stderr:write("usage: luajit tests/lua/test_modules.lua <game dir> <file.lua>...\n")
    os.exit(2)
end

local root = arg[1]
if root == nil then
    usage_failure("missing game directory")
end
root = root:gsub("/+$", "")
package.path = root .. "/?.lua"
package.cpath = ""

local modules = {}
for i = 2, #arg do
    local path = arg[i]
    if path:sub(1, #root + 1) ~= root .. "/" or path:sub(-4) ~= ".lua" then
        usage_failure(path .. " is not a .lua file under " .. root)
    end
    local module = path:sub(#root + 2, -5):gsub("/", ".")
    if not SKIPPED[module] then
        modules[#modules + 1] = module
    end
end
table.sort(modules)

local failures = {}
for _, module in ipairs(modules) do
    local before = {}
    for name in pairs(package.loaded) do
        before[name] = true
    end
    local ok, err = pcall(require, module)
    if not ok then
        -- A failed require leaves LuaJIT's sentinel (not a table) in
        -- package.loaded: a later require of a module that failed nested would
        -- report "loop or previous error" instead of its own missing module.
        for name, value in pairs(package.loaded) do
            if not before[name] and type(value) ~= "table" then
                package.loaded[name] = nil
            end
        end
        failures[#failures + 1] = "load " .. module .. ": " .. tostring(err)
    end
end

if #failures > 0 then
    io.stdout:write(table.concat(failures, "\n"), "\n")
    io.stdout:write("lua-test-modules: ", #failures, " modules fail to load\n")
    os.exit(1)
end
io.stdout:write("lua-test-modules: ", #modules, " modules loaded\n")
