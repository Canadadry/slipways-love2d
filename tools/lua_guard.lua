local GLUE_ONLY = { graphics = true, image = true }

local READABLE_GLOBALS = {
    assert = true, error = true, getmetatable = true, ipairs = true, next = true, pairs = true, pcall = true,
    rawequal = true, rawget = true, rawset = true, require = true, select = true, setmetatable = true,
    tonumber = true, tostring = true, type = true, unpack = true, xpcall = true, math = true, string = true,
    table = true, love = true,
}

local jit_util = require("jit.util")
local bcnames = require("jit.vmdef").bcnames

local function opcode(name)
    return (bcnames:find(name .. " ", 1, true) - 1) / 6
end

local GGET, GSET = opcode("GGET"), opcode("GSET")

local function usage_failure(message)
    io.stderr:write("lua-guard: " .. message .. "\n")
    io.stderr:write("usage: luajit tools/lua_guard.lua <game dir> <file.lua>...\n")
    os.exit(2)
end

local function set_of(names)
    local set = {}
    for _, name in ipairs(names) do
        set[name] = true
    end
    return set
end

local LIST_PATH = (arg[0]:match("^(.*)/[^/]*$") or ".") .. "/love_function_list.lua"
local list_chunk, list_error = loadfile(LIST_PATH)
local list
if list_chunk ~= nil then
    local loaded, result = pcall(setfenv(list_chunk, {}))
    list, list_error = loaded and result, not loaded and result or nil
end
if type(list) ~= "table" or type(list.callbacks) ~= "table" or type(list.modules) ~= "table" then
    io.stderr:write("lua-guard: ", list_error or LIST_PATH .. " does not return callbacks and modules", "\n")
    os.exit(2)
end
local CALLBACKS = set_of(list.callbacks)
local FUNCTIONS = {}
for module, names in pairs(list.modules) do
    FUNCTIONS[module] = set_of(names)
end

local function blank(text)
    return (text:gsub("[^\n]", " "))
end

-- Returns text with every comment and string literal turned into spaces,
-- newlines kept, so positions and line numbers still match; then the opening
-- quote of each short string literal, in order, and a table mapping each to
-- its closing quote. Only called on text that compiles.
local function strip(text)
    local pieces = {}
    local opens, closes = {}, {}
    local pos = 1
    while true do
        local i = text:find("[%-'\"%[]", pos)
        if i == nil then
            pieces[#pieces + 1] = text:sub(pos)
            break
        end
        local c = text:sub(i, i)
        local stop
        if c == "-" then
            if text:sub(i + 1, i + 1) == "-" then
                local level = text:match("^%[(=*)%[", i + 2)
                if level ~= nil then
                    local _, close = text:find("]" .. level .. "]", i + 2, true)
                    stop = close
                else
                    stop = (text:find("\n", i, true) or #text + 1) - 1
                end
            end
        elseif c == "[" then
            local level = text:match("^%[(=*)%[", i)
            if level ~= nil then
                local _, close = text:find("]" .. level .. "]", i, true)
                stop = close
            end
        else
            local j = i + 1
            while text:sub(j, j) ~= c do
                j = j + (text:sub(j, j) == "\\" and 2 or 1)
            end
            stop = j
            opens[#opens + 1], closes[i] = i, j
        end
        if stop == nil then
            pieces[#pieces + 1] = text:sub(pos, i)
        else
            pieces[#pieces + 1] = text:sub(pos, i - 1)
            pieces[#pieces + 1] = blank(text:sub(i, stop))
            i = stop
        end
        pos = i + 1
    end
    return table.concat(pieces), opens, closes
end

-- Returns the name after a dot at pos (spaces allowed around the dot) and the
-- position after it, or the next non-space character and its position.
local function next_field(code, pos)
    local name, after = code:match("^%s*%.%s*([%a_][%w_]*)()", pos)
    if name ~= nil then
        return name, after
    end
    local at, char = code:match("^%s*()(.?)", pos)
    return nil, at, char
end

-- Returns 0 when only spaces precede pos.
local function previous_non_space(code, pos)
    local prev = pos - 1
    while prev > 0 and code:find("^%s", prev) do
        prev = prev - 1
    end
    return prev
end

-- True when the name after prev is a field: a "." (not "..") or ":" at prev.
local function is_field(code, prev)
    local prev_char = code:sub(prev, prev)
    return prev_char == ":" or (prev_char == "." and code:sub(prev - 1, prev - 1) ~= ".")
end

local function scan(path, code, in_glue, report)
    local line, counted = 1, 1
    local pos = 1
    while true do
        local start, stop = code:find("%f[%w_]love%f[^%w_]", pos)
        if start == nil then
            return
        end
        pos = stop + 1
        local _, newlines = code:sub(counted, start):gsub("\n", "")
        line, counted = line + newlines, start + 1
        local function offence(reason)
            report(path .. ":" .. line .. ": " .. reason)
        end
        local prev = previous_non_space(code, start)
        if not is_field(code, prev) then
            local module, after, char = next_field(code, pos)
            if module == nil then
                offence(char == "[" and "love is indexed with brackets" or "love is aliased")
            elseif CALLBACKS[module] then
                local defined = prev >= 8 and code:find("^%f[%w_]function", prev - 7)
                    or code:match("^%s*=%s*function%f[^%w_]", after)
                if not defined then
                    offence("love." .. module .. " is only allowed where it is defined")
                end
            elseif FUNCTIONS[module] then
                local qualified = "love." .. module
                if GLUE_ONLY[module] and not in_glue then
                    offence(qualified .. " is only allowed in game/love/")
                end
                local name, after_name, next_char = next_field(code, after)
                if name == nil then
                    offence(qualified .. (next_char == "[" and " is indexed with brackets" or " is aliased"))
                elseif not FUNCTIONS[module][name] then
                    offence(qualified .. "." .. name .. " is not on the design's list")
                elseif not code:match("^%s*%(", after_name) then
                    offence(qualified .. "." .. name .. " is not called directly")
                end
            else
                offence("love." .. module .. " is not on the design's list")
            end
        end
    end
end

-- Walks the compiled chunk and its nested prototypes: the bytecode resolves
-- scoping, so a local shadowing a global name never shows up as GGET.
local function check_globals(path, chunk, writable, report)
    local info = jit_util.funcinfo(chunk)
    for pc = 1, info.bytecodes - 1 do
        local ins = jit_util.funcbc(chunk, pc)
        local op = ins % 256
        if op == GGET or op == GSET then
            local name = jit_util.funck(chunk, -math.floor(ins / 65536) - 1)
            local offence
            if op == GGET and not READABLE_GLOBALS[name] then
                offence = " is read but not declared"
            elseif op == GSET and not writable[name] then
                offence = " is written"
            end
            if offence ~= nil then
                report(path .. ":" .. jit_util.funcinfo(chunk, pc).currentline .. ": global " .. name .. offence)
            end
        end
    end
    if info.children then
        local index = -1
        while true do
            local constant = jit_util.funck(chunk, index)
            if constant == nil then
                break
            end
            if type(constant) == "proto" then
                check_globals(path, constant, writable, report)
            end
            index = index - 1
        end
    end
end

local root = arg[1]
if root == nil then
    usage_failure("no game directory")
end
root = root:gsub("/+$", "")
local glue_prefix = root .. "/love/"
local WRITABLE_GLOBALS = { [root .. "/main.lua"] = { debug_state = true } }
local NO_GLOBALS = {}

local existence = {}
local function exists(name)
    if existence[name] == nil then
        local file = io.open(root .. "/" .. name, "rb")
        existence[name] = file ~= nil
        if file then
            file:close()
        end
    end
    return existence[name]
end

local BUILT_IN_MODULES = { bit = true }

local function line_at(text, pos)
    local _, newlines = text:sub(1, pos):gsub("\n", "")
    return newlines + 1
end

-- A require must name a literal module that resolves as LÖVE and the fake
-- resolve it, and a literal that is exactly a PNG name must name a file:
-- the .love archive ships only what is under root.
local function check_completeness(path, text, code, opens, closes, report)
    local function offence(pos, reason)
        report(path .. ":" .. line_at(text, pos) .. ": " .. reason)
    end
    local function skip_spaces(at)
        while closes[at] == nil and code:find("^%s", at) do
            at = at + 1
        end
        return at
    end
    local pos = 1
    while true do
        local start, stop = code:find("require", pos, true)
        if start == nil then
            break
        end
        pos = stop + 1
        local whole_word = not (start > 1 and code:find("^[%w_]", start - 1)) and not code:find("^[%w_]", pos)
        if whole_word and not is_field(code, previous_non_space(code, start)) then
            local quote = skip_spaces(pos)
            local paren = code:sub(quote, quote) == "("
            if paren then
                quote = skip_spaces(quote + 1)
            end
            local close = closes[quote]
            if close == nil or (paren and not code:find("^%s*%)", close + 1)) then
                offence(start, "require takes a literal module name")
            else
                local name = text:sub(quote + 1, close - 1)
                local file = name:gsub("%.", "/")
                if not (BUILT_IN_MODULES[name] or exists(file .. ".lua") or exists(file .. "/init.lua")) then
                    offence(start, "module " .. name .. " is not under " .. root .. "/")
                end
            end
        end
    end
    for _, open in ipairs(opens) do
        local name = text:sub(open + 1, closes[open] - 1)
        if name:find("^[%w_%-%./]+%.png$") and not exists(name) then
            offence(open, "image " .. name .. " is not under " .. root .. "/")
        end
    end
end

local offences = {}
local function report(message)
    offences[#offences + 1] = message
end

for i = 2, #arg do
    local path = arg[i]
    local chunk, err = loadfile(path)
    if err ~= nil then
        report(err)
    else
        local file = assert(io.open(path, "rb"))
        local text = file:read("*a")
        file:close()
        local code, opens, closes = strip(text)
        scan(path, code, path:sub(1, #glue_prefix) == glue_prefix, report)
        check_completeness(path, text, code, opens, closes, report)
        check_globals(path, chunk, WRITABLE_GLOBALS[path] or NO_GLOBALS, report)
    end
end

if #offences > 0 then
    io.stdout:write(table.concat(offences, "\n"), "\n")
    os.exit(1)
end
io.stdout:write("lua-guard: ", #arg - 1, " files clean\n")
