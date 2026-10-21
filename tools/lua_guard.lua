local CALLBACKS = {
    conf = true, load = true, update = true, draw = true, gamepadpressed = true, gamepadreleased = true,
}

local FUNCTIONS = {
    graphics = {
        clear = true, draw = true, newCanvas = true, newImage = true, newQuad = true, points = true, pop = true,
        push = true, rectangle = true, setCanvas = true, setColor = true, setDefaultFilter = true, translate = true,
    },
    image = { newImageData = true },
    joystick = { getJoysticks = true },
    filesystem = { read = true, write = true, getInfo = true },
}

local GLUE_ONLY = { graphics = true, image = true }

local function usage_failure(message)
    io.stderr:write("lua-guard: " .. message .. "\n")
    io.stderr:write("usage: luajit tools/lua_guard.lua <game dir> <file.lua>...\n")
    os.exit(2)
end

local function blank(text)
    return (text:gsub("[^\n]", " "))
end

-- Returns text with every comment and string literal turned into spaces,
-- newlines kept, so positions and line numbers still match. Only called on
-- text that compiles.
local function strip(text)
    local pieces = {}
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
    return table.concat(pieces)
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
        local prev = start - 1
        while prev > 0 and code:find("^%s", prev) do
            prev = prev - 1
        end
        local prev_char = code:sub(prev, prev)
        local is_field = prev_char == ":" or (prev_char == "." and code:sub(prev - 1, prev - 1) ~= ".")
        if not is_field then
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

local root = arg[1]
if root == nil then
    usage_failure("no game directory")
end
local glue_prefix = root:gsub("/+$", "") .. "/love/"

local offences = {}
local function report(message)
    offences[#offences + 1] = message
end

for i = 2, #arg do
    local path = arg[i]
    local _, err = loadfile(path)
    if err ~= nil then
        report(err)
    else
        local file = assert(io.open(path, "rb"))
        local text = file:read("*a")
        file:close()
        scan(path, strip(text), path:sub(1, #glue_prefix) == glue_prefix, report)
    end
end

if #offences > 0 then
    io.stdout:write(table.concat(offences, "\n"), "\n")
    os.exit(1)
end
io.stdout:write("lua-guard: ", #arg - 1, " files clean\n")
