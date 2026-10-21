-- Requires every module of the game directory, then compares its data tables
-- and constants with the C rendering written by tools/table_reference.c.
-- Usage, from the repository root:
--   luajit tests/lua/test_modules.lua [--defer <module>]... <game dir> <reference file> <file.lua>...
-- The reference holds one block per table: "= <module> <name> <bytes>", then
-- lines "<module> <name><path> <value>" in the order render() writes them.
-- A deferred module may fail to load only on a module not yet in the tree,
-- and its blocks are then skipped; it fails the check once it loads.
local MAX_REPORTED = 5

-- Run by the fake in their own check: they define LÖVE callbacks.
local SKIPPED = { main = true, conf = true }

local buffer = require("string.buffer")

local floor = math.floor
local sort = table.sort

local function usage_failure(message)
    io.stderr:write("lua-test-modules: " .. message .. "\n")
    io.stderr:write("usage: luajit tests/lua/test_modules.lua [--defer <module>]... <game dir> <reference file>"
        .. " <file.lua>...\n")
    os.exit(2)
end

local deferred, deferred_names = {}, {}
local first = 1
while arg[first] == "--defer" do
    if arg[first + 1] == nil then
        usage_failure("--defer needs a module name")
    end
    deferred[arg[first + 1]] = true
    deferred_names[#deferred_names + 1] = arg[first + 1]
    first = first + 2
end

local root, reference_path = arg[first], arg[first + 1]
if root == nil or reference_path == nil then
    usage_failure("missing arguments")
end
root = root:gsub("/+$", "")
package.path = root .. "/?.lua"
package.cpath = ""

local failures = {}
local function report(message)
    failures[#failures + 1] = message
end

local modules = {}
local in_tree = {}
for i = first + 2, #arg do
    local path = arg[i]
    if path:sub(1, #root + 1) ~= root .. "/" or path:sub(-4) ~= ".lua" then
        usage_failure(path .. " is not a .lua file under " .. root)
    end
    local module = path:sub(#root + 2, -5):gsub("/", ".")
    in_tree[module] = true
    if not SKIPPED[module] then
        modules[#modules + 1] = module
    end
end
sort(modules)

local checked = {}
for _, module in ipairs(modules) do
    checked[module] = true
end
for _, module in ipairs(deferred_names) do
    if not checked[module] then
        report(module .. " is deferred but is not a file of the tree")
    end
end

local load_failures, loaded = 0, 0
local skipped_modules, skipped_names = {}, {}
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
    end
    if ok then
        loaded = loaded + 1
        if deferred[module] then
            report(module .. " loads now: drop it from the deferred modules")
        end
    else
        local missing = tostring(err):match("module '([^']+)' not found")
        if deferred[module] and missing ~= nil and not in_tree[missing] then
            skipped_modules[module] = true
            skipped_names[#skipped_names + 1] = module
        else
            load_failures = load_failures + 1
            report("load " .. module .. ": " .. tostring(err))
        end
    end
end

-- The C side writes the same quoting: one line per string whatever it holds.
local function quote(text)
    return '"' .. text:gsub('[%c"\\\128-\255]', function(c)
        return string.format("\\%03d", c:byte())
    end) .. '"'
end

-- tostring spells an integer below 10^14 exactly, and -0 as "-0", which no C
-- integer prints.
local function scalar(value)
    local kind = type(value)
    if kind == "number" then
        if value == floor(value) and value > -1e14 and value < 1e14 then
            return tostring(value)
        end
        return string.format("%.17g", value)
    elseif kind == "string" then
        return quote(value)
    elseif kind == "function" then
        return "function"
    end
    return tostring(value)
end

local function string_step(key)
    if type(key) == "string" and key:match("^[%a_][%w_]*$") then
        return "." .. key
    elseif type(key) == "string" then
        return "[" .. quote(key) .. "]"
    end
    return "[" .. tostring(key) .. "]"
end

local function exact_integer(value)
    return type(value) == "number" and value == floor(value) and value > -1e14 and value < 1e14
end

-- Appends to the buffer out one line "<path> <value>\n" per scalar: integer
-- keys ascending, then the other keys by the spelling of their step, depth
-- first, as the C side sorts. Returns the count of lines it appended.
local function render(out, prefix, value, registered, visiting)
    if type(value) ~= "table" then
        out:put(prefix, " ", scalar(value), "\n")
        return 1
    end
    local n = 0
    visiting[value] = true
    local count, low, high = 0, nil, nil
    local steps, keys = {}, {}
    for key in pairs(value) do
        if type(key) == "number" and key == floor(key) then
            count = count + 1
            if low == nil or key < low then low = key end
            if high == nil or key > high then high = key end
        else
            local step = string_step(key)
            steps[#steps + 1] = step
            keys[step] = key
        end
    end
    local function child(path, item)
        if type(item) ~= "table" then
            n = n + 1
            out:put(path, " ", scalar(item), "\n")
        elseif registered[item] ~= nil then
            n = n + 1
            out:put(path, " &", registered[item], "\n")
        elseif visiting[item] then
            n = n + 1
            out:put(path, " cycle\n")
        else
            n = n + render(out, path, item, registered, visiting)
        end
    end
    if count > 0 and count == high - low + 1 then
        for i = low, high do
            local item = value[i]
            if exact_integer(item) then
                n = n + 1
                out:put(prefix, "[", i, "] ", item, "\n")
            else
                child(prefix .. "[" .. i .. "]", item)
            end
        end
    elseif count > 0 then
        local integers = {}
        for key in pairs(value) do
            if type(key) == "number" and key == floor(key) then
                integers[#integers + 1] = key
            end
        end
        sort(integers)
        for _, key in ipairs(integers) do
            child(prefix .. "[" .. key .. "]", value[key])
        end
    end
    sort(steps)
    for _, step in ipairs(steps) do
        child(prefix .. step, value[keys[step]])
    end
    visiting[value] = nil
    return n
end

local function read_file(path)
    local file = io.open(path, "rb")
    if file == nil then
        usage_failure("cannot read " .. path)
    end
    local text = file:read("*a")
    file:close()
    return text
end

local function read_blocks(text)
    local blocks = {}
    local position = 1
    while position <= #text do
        local module, name, bytes, body = text:match("^= (%S+) ([%a_][%w_]*) (%d+)\n()", position)
        if module == nil then
            usage_failure(reference_path .. ": no block header at byte " .. position)
        end
        local finish = body + tonumber(bytes) - 1
        if finish > #text or text:sub(finish, finish) ~= "\n" then
            usage_failure(reference_path .. ": block " .. module .. " " .. name .. " overruns its size")
        end
        blocks[#blocks + 1] = { id = module .. " " .. name, module = module, name = name, first = body, last = finish }
        position = finish + 1
    end
    return blocks
end

-- A module whose return value is the table itself (generated data) holds no
-- string key, unlike a module table M.
local function is_bare_data(module_value)
    for key in pairs(module_value) do
        if type(key) == "string" then
            return false
        end
    end
    return true
end

local function lua_value(block)
    local module_value = package.loaded[block.module]
    if type(module_value) ~= "table" then
        return nil
    end
    local value = module_value[block.name]
    if value == nil and is_bare_data(module_value) then
        return module_value
    end
    return value
end

-- Each line "<key> <value>": the key ends at the first space outside a
-- quoted step.
local function split_line(line)
    if line:find('"', 1, true) == nil then
        return line:match("^(%S+ %S+) (.*)$")
    end
    local position = line:find(" ", 1, true) + 1
    while position <= #line do
        local c = line:sub(position, position)
        if c == " " then
            return line:sub(1, position - 1), line:sub(position + 1)
        elseif line:sub(position, position + 1) == '["' then
            position = (line:find('"]', position + 2, true) or #line) + 2
        else
            position = position + 1
        end
    end
    return nil
end

local function read_lines(text)
    local values, keys = {}, {}
    for line in text:gmatch("[^\n]+") do
        local key, value = split_line(line)
        if key == nil then
            usage_failure(reference_path .. ": cannot read: " .. line)
        end
        if values[key] == nil then
            keys[#keys + 1] = key
            values[key] = value
        end
    end
    return values, keys
end

local function differences_of(c_text, lua_text)
    local c_values, c_keys = read_lines(c_text)
    local lua_values, lua_keys = read_lines(lua_text)
    local differences = {}
    for _, key in ipairs(c_keys) do
        local c_value, lua = c_values[key], lua_values[key]
        if lua == nil then
            differences[#differences + 1] = key .. ": missing in Lua, C " .. c_value
        elseif lua ~= c_value then
            differences[#differences + 1] = key .. ": C " .. c_value .. ", Lua " .. lua
        end
    end
    for _, key in ipairs(lua_keys) do
        if c_values[key] == nil then
            differences[#differences + 1] = key .. ": extra in Lua, Lua " .. lua_values[key]
        end
    end
    return differences
end

local text = read_file(reference_path)
local blocks = read_blocks(text)
local registered = {}
for _, block in ipairs(blocks) do
    block.value = lua_value(block)
    if type(block.value) == "table" then
        registered[block.value] = block.name
    end
end

local differing, compared_lines, skipped_blocks = 0, 0, 0
for _, block in ipairs(blocks) do
    if skipped_modules[block.module] then
        skipped_blocks = skipped_blocks + 1
    elseif block.value == nil then
        differing = differing + 1
        report(block.id .. ": missing in Lua")
    else
        local rendered = buffer.new()
        local count = render(rendered, block.id, block.value, registered, {})
        local c_text, lua_text = text:sub(block.first, block.last), rendered:tostring()
        if lua_text == c_text then
            compared_lines = compared_lines + count
        else
            local differences = differences_of(c_text, lua_text)
            if #differences == 0 then
                compared_lines = compared_lines + count
            else
                differing = differing + 1
                for i = 1, math.min(#differences, MAX_REPORTED) do
                    report(differences[i])
                end
                if #differences > MAX_REPORTED then
                    report(block.id .. ": " .. (#differences - MAX_REPORTED) .. " more differences")
                end
            end
        end
    end
end

if #failures > 0 then
    io.stdout:write(table.concat(failures, "\n"), "\n")
    io.stdout:write("lua-test-modules: ", load_failures, " modules fail to load, ", differing, " of ", #blocks,
        " names differ from C\n")
    os.exit(1)
end
local deferral = ""
if #skipped_names > 0 then
    deferral = " " .. #skipped_names .. " deferred: " .. table.concat(skipped_names, ", ") .. " (" .. skipped_blocks
        .. " names skipped),"
end
io.stdout:write("lua-test-modules: ", loaded, " modules loaded,", deferral, " ", #blocks - skipped_blocks,
    " names and ", compared_lines, " lines equal to C\n")
