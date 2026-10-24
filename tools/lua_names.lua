-- Usage, from the repository root:
--   luajit tools/lua_names.lua [--modules '<module> <dir>.*'] <game dir> <file.lua>...
-- The files under <game dir> are its modules; every file given is scanned for
-- module field reads. --modules restricts the prefix check only.

local PREFIX_EXCEPTIONS = {
    ["game.draw_functions"] = {
        draw_row = "the verb is the module's first word; draw_functions.row would lose its action",
        draw_row_text_image = "the verb is the module's first word; draw_functions.row_text_image would lose its action",
        draw_rows_default = "the verb is the module's first word; draw_functions.rows_default would lose its action",
    },
    ["game.bindings_default"] = {
        bindings = "the stripped name is empty; the export is the default bindings table, named by its role",
    },
}

-- Keyed by path under the game directory, then alias, then field.
local SHADOWED = "a parameter or local of this name shadows the call-time require alias and holds an entity"
local FIELD_ALLOWLIST = {
    ["game/economy_score.lua"] = {
        planet = { archetype = SHADOWED, outgoing_routes = SHADOWED },
    },
    ["game/economy_tiers.lua"] = {
        planet = {
            archetype = SHADOWED, incoming_routes = SHADOWED, needs = SHADOWED, outgoing_routes = SHADOWED,
            planet_id = SHADOWED, production_bonus = SHADOWED, project = SHADOWED, state = SHADOWED,
        },
    },
    ["game/planet_family_trade.lua"] = {
        slipway = { class = SHADOWED },
    },
    ["game/planet_trade.lua"] = {
        slipway = { class = SHADOWED },
    },
}

local SKIPPED = { main = true, conf = true }

local function usage_failure(message)
    io.stderr:write("lua-names: " .. message .. "\n")
    io.stderr:write("usage: luajit tools/lua_names.lua [--modules '<module> <dir>.*'] <game dir> <file.lua>...\n")
    os.exit(2)
end

local function blank(text)
    return (text:gsub("[^\n]", " "))
end

-- A copy of tools/lua_guard.lua's, so that lua-guard stays untouched. Returns
-- text with every comment and string literal turned into spaces, newlines kept,
-- so positions still match. Only called on text that compiles.
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

local function read_file(path)
    local file = assert(io.open(path, "rb"))
    local text = file:read("*a")
    file:close()
    return text
end

-- Returns a function mapping a position in text to its line number.
local function line_counter(text)
    local starts = { 1 }
    for at in text:gmatch("\n()") do
        starts[#starts + 1] = at
    end
    return function(pos)
        local low, high = 1, #starts
        while low < high do
            local mid = math.floor((low + high + 1) / 2)
            if starts[mid] <= pos then
                low = mid
            else
                high = mid - 1
            end
        end
        return low
    end
end

local LUA_KEYWORDS = {}
for word in ("and break do else elseif end false for function goto if in local nil not or repeat return then true until while"):gmatch("%a+") do
    LUA_KEYWORDS[word] = true
end

local function words_of(file_name)
    local words = {}
    for word in file_name:gmatch("[^_]+") do
        words[#words + 1] = word
    end
    return words
end

-- The renaming rule: in p8/, drop p8_; then the longest leading run of the file
-- name's words followed by _. Case is ignored by the test and kept in the result.
local function stripped_name(module, name)
    local file_name = module:match("([^.]+)$")
    local rest = name
    if module:sub(1, 3) == "p8." and rest:lower():sub(1, 3) == "p8_" then
        rest = rest:sub(4)
    end
    local words = words_of(file_name)
    for count = #words, 1, -1 do
        local run = table.concat(words, "_", 1, count) .. "_"
        if rest:lower():sub(1, #run) == run then
            return rest:sub(#run + 1)
        end
        if rest:lower() == run:sub(1, -2) then
            return ""
        end
    end
    return rest
end

local function repeats_prefix(module, name)
    local lower = name:lower()
    local file_name = module:match("([^.]+)$")
    local first_word = words_of(file_name)[1]
    if module:sub(1, 3) == "p8." and lower:sub(1, 3) == "p8_" then
        return true
    end
    return lower == file_name or lower == first_word or lower:sub(1, #first_word + 1) == first_word .. "_"
end

local modules_option
local args = {}
do
    local i = 1
    while i <= #arg do
        if arg[i] == "--modules" then
            modules_option = arg[i + 1]
            if modules_option == nil then
                usage_failure("--modules needs a value")
            end
            i = i + 2
        else
            args[#args + 1] = arg[i]
            i = i + 1
        end
    end
end

local root = args[1]
if root == nil then
    usage_failure("no game directory")
end
root = root:gsub("/+$", "")
package.path = root .. "/?.lua"
package.cpath = ""

local files = {}
local module_of_path = {}
local modules = {}
for i = 2, #args do
    local path = args[i]
    if path:sub(-4) ~= ".lua" then
        usage_failure(path .. " is not a .lua file")
    end
    files[#files + 1] = path
    if path:sub(1, #root + 1) == root .. "/" then
        local module = path:sub(#root + 2, -5):gsub("/", ".")
        if not SKIPPED[module] then
            module_of_path[path] = module
            modules[module] = { path = path }
        end
    end
end

local checked_modules = {}
if modules_option == nil or modules_option:match("^%s*$") then
    for module in pairs(modules) do
        checked_modules[module] = true
    end
else
    for entry in modules_option:gmatch("%S+") do
        local matched = false
        for module in pairs(modules) do
            local dir = entry:match("^(.*)%.%*$")
            if module == entry or (dir ~= nil and module:sub(1, #dir + 1) == dir .. ".") then
                checked_modules[module] = true
                matched = true
            end
        end
        if not matched then
            usage_failure("--modules entry " .. entry .. " names no module under " .. root)
        end
    end
end

local findings = {}
local prefix_count, field_count, reads_checked = 0, 0, 0

local function finding(path, line, message)
    findings[#findings + 1] = { path = path, line = line, message = message }
end

local function sorted_keys(map)
    local keys = {}
    for key in pairs(map) do
        keys[#keys + 1] = key
    end
    table.sort(keys)
    return keys
end

for _, module in ipairs(sorted_keys(modules)) do
    local before = {}
    for name in pairs(package.loaded) do
        before[name] = true
    end
    local ok, value = pcall(require, module)
    if ok then
        modules[module].table = value
    else
        -- A failed require leaves LuaJIT's sentinel in package.loaded: a later
        -- require of a module that failed nested would report "loop or
        -- previous error" instead of its own error.
        for name, loaded in pairs(package.loaded) do
            if not before[name] and type(loaded) ~= "table" then
                package.loaded[name] = nil
            end
        end
        -- Its reads are still checked, against the fields the files assign.
        modules[module].table = {}
        local path = modules[module].path
        local message = tostring(value)
        local line = message:match("^" .. path:gsub("%p", "%%%0") .. ":(%d+): ")
        field_count = field_count + 1
        finding(path, tonumber(line) or 1, module .. " does not load: " .. message:gsub("^[^:]*:%d+: ", ""))
    end
end

local scanned = {}
for _, path in ipairs(files) do
    local chunk, err = loadfile(path)
    if chunk == nil then
        usage_failure(err)
    end
    local text = read_file(path)
    scanned[#scanned + 1] = { path = path, text = text, code = strip(text), line_of = line_counter(text) }
end

for _, file in ipairs(scanned) do
    local module = module_of_path[file.path]
    if module ~= nil and checked_modules[module] then
        local exceptions = PREFIX_EXCEPTIONS[module] or {}
        local exports, order = {}, {}
        local line = 0
        for code_line in (file.code .. "\n"):gmatch("([^\n]*)\n") do
            line = line + 1
            local name = code_line:match("^%s*function%s+M%s*%.%s*([%a_][%w_]*)%s*%(")
                or code_line:match("^%s*M%s*%.%s*([%a_][%w_]*)%s*=%f[^=]")
            if name ~= nil and exports[name] == nil then
                exports[name] = line
                order[#order + 1] = name
            end
        end
        table.sort(order, function(a, b)
            return exports[a] < exports[b] or (exports[a] == exports[b] and a < b)
        end)
        local by_stripped = {}
        for _, name in ipairs(order) do
            local new_name = name
            if repeats_prefix(module, name) and exceptions[name] == nil then
                new_name = stripped_name(module, name)
                prefix_count = prefix_count + 1
                finding(file.path, exports[name], module .. "." .. name .. " repeats its module's prefix (stripped: "
                    .. (new_name == "" and "empty" or new_name) .. (LUA_KEYWORDS[new_name] and ", a keyword" or "") .. ")")
            end
            local other = by_stripped[new_name]
            if other ~= nil then
                prefix_count = prefix_count + 1
                finding(file.path, exports[name], module .. "." .. other .. " and " .. module .. "." .. name
                    .. " both become " .. new_name)
            else
                by_stripped[new_name] = name
            end
        end
    end
end

-- A read passes when the loaded table holds the field or any scanned file
-- assigns it (fields set at run time).
local reads = {}
local assigned = {}
for module in pairs(modules) do
    assigned[module] = {}
end

local function after_function_keyword(code, pos)
    local prev = pos - 1
    while prev > 0 and code:find("^%s", prev) do
        prev = prev - 1
    end
    return prev >= 8 and code:sub(prev - 7, prev) == "function" and (prev == 8 or not code:find("^[%w_]", prev - 8))
end

local function note(file, module, alias, name, pos, after)
    local is_write = file.code:match("^%s*=%f[^=]", after) ~= nil or after_function_keyword(file.code, pos)
    if is_write then
        assigned[module][name] = true
    else
        reads[#reads + 1] = { file = file, module = module, alias = alias, name = name, pos = pos }
    end
end

-- Returns whether the name at pos is a field of something else (x.alias) rather
-- than a variable.
local function is_field(code, pos)
    local prev = pos - 1
    while prev > 0 and code:find("^%s", prev) do
        prev = prev - 1
    end
    local prev_char = code:sub(prev, prev)
    return prev_char == ":" or (prev_char == "." and code:sub(prev - 1, prev - 1) ~= ".")
end

for _, file in ipairs(scanned) do
    local code, text = file.code, file.text
    local aliases = {}
    for line_start, alias, line_end in code:gmatch("()[ \t]*local%s+([%a_][%w_]*)%s*=%s*require%s*%(%s*%)[ \t]*()") do
        local module = text:sub(line_start, line_end - 1):match("require%s*%(%s*[\"']([%w_.]+)[\"']%s*%)")
        local at_line_start = line_start == 1 or code:sub(line_start - 1, line_start - 1) == "\n"
        local at_line_end = line_end > #code or code:sub(line_end, line_end) == "\n"
        if module ~= nil and modules[module] ~= nil and at_line_start and at_line_end then
            if aliases[alias] ~= nil and aliases[alias] ~= module then
                field_count = field_count + 1
                finding(file.path, file.line_of(line_start), alias .. " is bound to " .. aliases[alias] .. " and "
                    .. module)
            end
            aliases[alias] = module
        end
    end
    for alias, module in pairs(aliases) do
        for pos, after_alias in code:gmatch("()%f[%w_]" .. alias .. "%f[^%w_]()") do
            local name, after = code:match("^%s*[%.:]%s*([%a_][%w_]*)()", after_alias)
            if name ~= nil and not is_field(code, pos) then
                note(file, module, alias, name, pos, after)
            end
        end
    end
    for pos, after_paren in code:gmatch("()%f[%w_]require%s*%(()") do
        local module, after_call = text:match("^%s*[\"']([%w_.]+)[\"']%s*%)()", after_paren)
        if module ~= nil and modules[module] ~= nil then
            local name, after = code:match("^%s*[%.:]%s*([%a_][%w_]*)()", after_call)
            if name ~= nil then
                note(file, module, "require(\"" .. module .. "\")", name, pos, after)
            end
        end
    end
    local own = module_of_path[file.path]
    if own ~= nil then
        for pos, after_m in code:gmatch("()%f[%w_]M%f[^%w_]()") do
            local name, after = code:match("^%s*[%.:]%s*([%a_][%w_]*)()", after_m)
            if name ~= nil and not is_field(code, pos) then
                note(file, own, "M", name, pos, after)
            end
        end
    end
end

for _, read in ipairs(reads) do
    reads_checked = reads_checked + 1
    local value = modules[read.module].table
    local holds = type(value) == "table" and rawget(value, read.name) ~= nil
    if not holds and not assigned[read.module][read.name] then
        local path = read.file.path
        local relative = path:sub(1, #root + 1) == root .. "/" and path:sub(#root + 2) or path
        local allowed = FIELD_ALLOWLIST[relative] and FIELD_ALLOWLIST[relative][read.alias]
            and FIELD_ALLOWLIST[relative][read.alias][read.name]
        if not allowed then
            field_count = field_count + 1
            finding(path, read.file.line_of(read.pos), read.alias .. "." .. read.name .. ": " .. read.module
                .. " has no field " .. read.name)
        end
    end
end

table.sort(findings, function(a, b)
    if a.path ~= b.path then
        return a.path < b.path
    end
    if a.line ~= b.line then
        return a.line < b.line
    end
    return a.message < b.message
end)
for _, item in ipairs(findings) do
    io.stdout:write(item.path, ":", item.line, ": ", item.message, "\n")
end
io.stdout:write("lua-names: ", prefix_count, " prefix findings, ", field_count, " field findings, ", reads_checked,
    " reads checked\n")
os.exit(#findings == 0 and 0 or 1)
