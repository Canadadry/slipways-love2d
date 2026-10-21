local SCANNED_DIRS = { "src/p8", "src/engine", "src/game" }
local SCANNED_FILES = { "src/devtools/state_format.c" }

local DATA_FILE = "tools/checklist.txt"
local STEP_IDS = { "1.2", "2.1" }
for n = 1, 15 do
    STEP_IDS[#STEP_IDS + 1] = "3." .. n
end
local STEP_RANK = {}
for rank, id in ipairs(STEP_IDS) do
    STEP_RANK[id] = rank
end
local UNASSIGNED_RANK = #STEP_IDS + 1

local errors = {}

local function report(message)
    errors[#errors + 1] = "checklist: " .. message
end

local function usage_failure(message)
    io.stderr:write("checklist: " .. message .. "\n")
    io.stderr:write("usage: luajit tools/checklist.lua [--root <dir>] [--listing <file>] [--step <id>]\n")
    os.exit(2)
end

local function parse_arguments(argv)
    local options = { root = "." }
    local i = 1
    while i <= #argv do
        local name, value = argv[i], argv[i + 1]
        if name == "--root" or name == "--listing" or name == "--step" then
            if value == nil then
                usage_failure(name .. " needs a value")
            end
            options[name:sub(3)] = value
            i = i + 2
        else
            usage_failure("unknown argument: " .. name)
        end
    end
    if options.step ~= nil and STEP_RANK[options.step] == nil then
        usage_failure("unknown step " .. options.step)
    end
    return options
end

local function read_file(path)
    local file = io.open(path, "rb")
    if file == nil then
        return nil
    end
    local text = file:read("*a")
    file:close()
    return text
end

local function shell_quote(text)
    return "'" .. text:gsub("'", "'\\''") .. "'"
end

-- Returns the scanned C paths, relative to root, sorted.
local function list_c_files(root)
    local paths = {}
    for _, dir in ipairs(SCANNED_DIRS) do
        local command = "find " .. shell_quote(root .. "/" .. dir)
            .. " -maxdepth 1 -type f \\( -name '*.c' -o -name '*.h' \\) 2>/dev/null"
        local pipe = io.popen(command)
        for line in pipe:lines() do
            paths[#paths + 1] = dir .. "/" .. line:match("([^/]+)$")
        end
        pipe:close()
    end
    for _, path in ipairs(SCANNED_FILES) do
        if read_file(root .. "/" .. path) ~= nil then
            paths[#paths + 1] = path
        end
    end
    table.sort(paths)
    return paths
end

-- Tokens are { kind = "id" | "string" | "char" | "number" | "punct", text, line }.
-- Comments and preprocessor lines (with their continuations) produce none.
-- Returns nil, line, message on an unterminated comment or literal.
local function tokenize(text)
    local tokens = {}
    local pos, line, length = 1, 1, #text
    local at_line_start = true

    local function count_lines(from, to)
        local _, n = text:sub(from, to):gsub("\n", "")
        line = line + n
    end

    local function skip_block_comment()
        local close = text:find("*/", pos + 2, true)
        if close == nil then
            return false
        end
        count_lines(pos, close + 1)
        pos = close + 2
        return true
    end

    local function skip_literal(quote)
        local i = pos + 1
        while i <= length do
            local c = text:sub(i, i)
            if c == "\\" then
                i = i + 2
            elseif c == quote then
                return i
            elseif c == "\n" then
                return nil
            else
                i = i + 1
            end
        end
        return nil
    end

    while pos <= length do
        local c = text:sub(pos, pos)
        if c == "\n" then
            line = line + 1
            pos = pos + 1
            at_line_start = true
        elseif c:match("%s") then
            pos = pos + 1
        elseif text:sub(pos, pos + 1) == "//" then
            local eol = text:find("\n", pos, true) or (length + 1)
            pos = eol
        elseif text:sub(pos, pos + 1) == "/*" then
            local start = line
            if not skip_block_comment() then
                return nil, start, "unterminated comment"
            end
        elseif c == "#" and at_line_start then
            local start = line
            while pos <= length do
                local d = text:sub(pos, pos)
                if d == "\\" and text:sub(pos + 1, pos + 1) == "\n" then
                    pos = pos + 2
                    line = line + 1
                elseif text:sub(pos, pos + 1) == "/*" then
                    if not skip_block_comment() then
                        return nil, start, "unterminated comment"
                    end
                elseif text:sub(pos, pos + 1) == "//" or d == "\n" then
                    break
                else
                    pos = pos + 1
                end
            end
            while pos <= length and text:sub(pos, pos) ~= "\n" do
                pos = pos + 1
            end
        else
            at_line_start = false
            if c == '"' or c == "'" then
                local close = skip_literal(c)
                if close == nil then
                    return nil, line, "unterminated literal"
                end
                tokens[#tokens + 1] = { kind = c == '"' and "string" or "char", text = text:sub(pos, close), line = line }
                pos = close + 1
            elseif c:match("[%a_]") then
                local word = text:match("^[%w_]+", pos)
                tokens[#tokens + 1] = { kind = "id", text = word, line = line }
                pos = pos + #word
            elseif c:match("%d") then
                local number = text:match("^[%w_.]+", pos)
                tokens[#tokens + 1] = { kind = "number", text = number, line = line }
                pos = pos + #number
            else
                local two = text:sub(pos, pos + 1)
                if two == "==" or two == "!=" or two == "<=" or two == ">=" or two == "->"
                    or (two:sub(2, 2) == "=" and two:sub(1, 1):match("[-+*/%%&|^]")) then
                    tokens[#tokens + 1] = { kind = "punct", text = two, line = line }
                    pos = pos + 2
                else
                    tokens[#tokens + 1] = { kind = "punct", text = c, line = line }
                    pos = pos + 1
                end
            end
        end
    end
    return tokens
end

local TYPE_KEYWORDS = { struct = true, union = true, enum = true }

-- Index of the token opening the balanced group that closes at `close`, or nil.
local function matching_open(statement, close, open_text, close_text)
    local depth = 0
    for i = close, 1, -1 do
        local text = statement[i].text
        if text == close_text then
            depth = depth + 1
        elseif text == open_text then
            depth = depth - 1
            if depth == 0 then
                return i
            end
        end
    end
    return nil
end

local function starts_with_ignored_keyword(statement)
    for _, token in ipairs(statement) do
        if token.text == "extern" or token.text == "typedef" then
            return true
        end
    end
    return false
end

-- The declarator name before the `=` at index `assign`: `name`, `name[...]`,
-- or `(*name[...])(...)` for an array of function pointers.
local function declared_name(statement, assign)
    local i = assign - 1
    while i >= 1 and statement[i].text == "]" do
        i = (matching_open(statement, i, "[", "]") or 0) - 1
    end
    if i >= 1 and statement[i].text == ")" then
        local open = matching_open(statement, i, "(", ")")
        if open ~= nil and open > 1 and statement[open - 1].text == ")" then
            local inner = matching_open(statement, open - 1, "(", ")")
            for j = open - 2, (inner or open - 1) + 1, -1 do
                if statement[j].kind == "id" then
                    return statement[j]
                end
            end
        end
        return nil
    end
    if i >= 1 and statement[i].kind == "id" then
        return statement[i]
    end
    return nil
end

-- Returns { functions = {...}, tables = {...} } (entries { name, line }),
-- or nil, line, message on C it cannot parse.
local function scan(text)
    local tokens, error_line, message = tokenize(text)
    if tokens == nil then
        return nil, error_line, message
    end
    local found = { functions = {}, tables = {} }
    local statement = {}
    local assign = nil
    local i = 1

    local function skip_braces()
        local depth = 0
        while i <= #tokens do
            local token = tokens[i].text
            if token == "{" then
                depth = depth + 1
            elseif token == "}" then
                depth = depth - 1
                if depth == 0 then
                    return true
                end
            end
            i = i + 1
        end
        return false
    end

    local function end_statement()
        if assign ~= nil and not starts_with_ignored_keyword(statement) then
            local value = statement[assign + 1]
            if value ~= nil and (value.text == "{" or value.kind == "string") then
                local name = declared_name(statement, assign)
                if name ~= nil then
                    found.tables[#found.tables + 1] = { name = name.text, line = name.line }
                end
            end
        end
        statement, assign = {}, nil
    end

    while i <= #tokens do
        local token = tokens[i]
        local last = statement[#statement]
        if token.text == ";" then
            end_statement()
        elseif token.text == "{" then
            local before_last = statement[#statement - 1]
            local is_type_body = last ~= nil and (TYPE_KEYWORDS[last.text]
                or (last.kind == "id" and before_last ~= nil and TYPE_KEYWORDS[before_last.text]))
            if last ~= nil and last.text == "=" then
                statement[#statement + 1] = token
            elseif is_type_body then
                statement[#statement + 1] = { kind = "punct", text = "{}", line = token.line }
            else
                local close = nil
                for j = #statement, 1, -1 do
                    if statement[j].text == ")" then
                        close = j
                        break
                    end
                end
                local open = close and matching_open(statement, close, "(", ")")
                local name = open and statement[open - 1]
                if assign ~= nil or name == nil or name.kind ~= "id" then
                    return nil, token.line, "top-level { that is neither a function, a type nor an initializer"
                end
                if not starts_with_ignored_keyword(statement) then
                    found.functions[#found.functions + 1] = { name = name.text, line = name.line }
                end
                statement, assign = {}, nil
            end
            if not skip_braces() then
                return nil, token.line, "unbalanced {"
            end
        elseif token.text == "}" then
            return nil, token.line, "unbalanced }"
        else
            if token.text == "=" and assign == nil then
                assign = #statement + 1
            end
            statement[#statement + 1] = token
        end
        i = i + 1
    end
    if #statement > 0 then
        return nil, statement[#statement].line, "statement not ended by ;"
    end
    return found
end

local NOT_PORTED_KINDS = { file = 3, ["function"] = 4, table = 4, declaration = 4, macro = 4, rule = 4 }

local function split_fields(text)
    local fields = {}
    for field in text:gmatch("%S+") do
        fields[#fields + 1] = field
    end
    return fields
end

-- Returns { steps = { [C path] = id }, units = { [generated path] = Lua path },
-- not_ported = { { kind, path, name, why } }, lua = { { path, name, lua_path, lua_name } } };
-- a missing data file reads as empty.
local function read_data(root)
    local data = { steps = {}, units = {}, not_ported = {}, lua = {} }
    local text = read_file(root .. "/" .. DATA_FILE) or ""
    local number = 0
    for line in (text .. "\n"):gmatch("([^\n]*)\n") do
        number = number + 1
        local head, why = line:match("^(.-) %-%- (.+)$")
        local fields = split_fields(head or line)
        local where = DATA_FILE .. ":" .. number .. ": "
        if why == nil and ((fields[1] == "step" and #fields == 3) or (fields[1] == "unit" and #fields == 4)) then
            local id, path = fields[2], fields[3]
            if fields[1] == "unit" then
                data.units[path] = fields[4]
            end
            if STEP_RANK[id] == nil then
                report(where .. "unknown step " .. id)
            elseif data.steps[path] ~= nil then
                report(path .. ": two step lines")
            else
                data.steps[path] = id
            end
        elseif why ~= nil and fields[1] == "not-ported" and NOT_PORTED_KINDS[fields[2]] == #fields then
            data.not_ported[#data.not_ported + 1] = { kind = fields[2], path = fields[3], name = fields[4], why = why }
        elseif why == nil and fields[1] == "lua" and #fields == 5 then
            data.lua[#data.lua + 1] = { path = fields[2], name = fields[3], lua_path = fields[4], lua_name = fields[5] }
        elseif #fields > 0 then
            report(where .. "cannot read: " .. line)
        end
    end
    return data
end

local function lua_path_of(c_path)
    local dir, base = c_path:match("^src/([^/]+)/([^/]+)%.[ch]$")
    return "game/" .. dir .. "/" .. base .. ".lua"
end

-- Returns { functions = set, tables = set } of the names a Lua file defines,
-- or nil when the file does not exist.
local function lua_definitions(path)
    local text = read_file(path)
    if text == nil then
        return nil
    end
    local functions, tables = {}, {}
    local function_patterns = {
        "^%s*function%s+([%w_]+)%s*%(",
        "^%s*function%s+[%w_]+[.:]([%w_]+)%s*%(",
        "^%s*local%s+function%s+([%w_]+)%s*%(",
        "^%s*local%s+([%w_]+)%s*=%s*function%f[^%w_]",
        "^%s*([%w_]+)%s*=%s*function%f[^%w_]",
        "^%s*[%w_]+%.([%w_]+)%s*=%s*function%f[^%w_]",
    }
    local table_patterns = {
        "^%s*local%s+([%w_]+)%s*=%f[^=]",
        "^%s*([%w_]+)%s*=%f[^=]",
        "^%s*[%w_]+%.([%w_]+)%s*=%f[^=]",
    }
    -- Every pattern is anchored at the line start, so a `--` line never matches.
    for line in (text .. "\n"):gmatch("([^\n]*)\n") do
        for _, pattern in ipairs(function_patterns) do
            local name = line:match(pattern)
            if name ~= nil then
                functions[name] = true
            end
        end
        for _, pattern in ipairs(table_patterns) do
            local name = line:match(pattern)
            if name ~= nil and name ~= "local" then
                tables[name] = true
            end
        end
    end
    return { functions = functions, tables = tables }
end

local function sorted_keys(map)
    local keys = {}
    for key in pairs(map) do
        keys[#keys + 1] = key
    end
    table.sort(keys)
    return keys
end

local function scan_defines(found, kind, name)
    if found == nil then
        return false
    end
    local lists = { found.functions, found.tables }
    if kind == "function" then
        lists = { found.functions }
    elseif kind == "table" then
        lists = { found.tables }
    end
    for _, list in ipairs(lists) do
        for _, item in ipairs(list) do
            if item.name == name then
                return true
            end
        end
    end
    return false
end

local function check_stale_entries(root, data, found_in, lua_items)
    for _, entry in ipairs(data.not_ported) do
        local found = found_in[entry.path]
        local exists = found ~= nil
        if entry.kind == "function" or entry.kind == "table" then
            exists = scan_defines(found, entry.kind, entry.name)
        elseif exists and (entry.kind == "declaration" or entry.kind == "macro") then
            exists = read_file(root .. "/" .. entry.path):find("%f[%w_]" .. entry.name .. "%f[^%w_]") ~= nil
        end
        if not exists then
            report(entry.path .. ": not-ported " .. entry.kind .. (entry.name and " " .. entry.name or "")
                .. " not found")
        end
        if entry.name ~= nil and lua_items[entry.path .. " " .. entry.name] ~= nil then
            report(entry.path .. ": " .. entry.name .. " both not ported and given a lua line")
        end
    end
    for _, entry in ipairs(data.lua) do
        if not scan_defines(found_in[entry.path], nil, entry.name) then
            report(entry.path .. ": lua " .. entry.name .. " not found")
        end
    end
end

-- Returns the `#include "…"` targets of a C file, as paths under src/.
local function quoted_includes(text)
    local targets = {}
    for line in (text .. "\n"):gmatch("([^\n]*)\n") do
        local target = line:match('^%s*#%s*include%s*"([^"]+)"')
        if target ~= nil then
            targets[#targets + 1] = "src/" .. target
        end
    end
    return targets
end

-- Tarjan's algorithm. `edges[node]` lists the nodes it points to; returns
-- { [node] = component number } for every node of `nodes`.
local function strongly_connected_components(nodes, edges)
    local component, index, low, on_stack, stack = {}, {}, {}, {}, {}
    local next_index, next_component = 0, 0
    local function visit(node)
        index[node], low[node] = next_index, next_index
        next_index = next_index + 1
        stack[#stack + 1], on_stack[node] = node, true
        for _, target in ipairs(edges[node] or {}) do
            if index[target] == nil then
                visit(target)
                low[node] = math.min(low[node], low[target])
            elseif on_stack[target] then
                low[node] = math.min(low[node], index[target])
            end
        end
        if low[node] == index[node] then
            next_component = next_component + 1
            repeat
                local member = table.remove(stack)
                on_stack[member] = nil
                component[member] = next_component
            until member == node
        end
    end
    for _, node in ipairs(nodes) do
        if index[node] == nil then
            visit(node)
        end
    end
    return component
end

-- A module is a .c and its .h together: only headers are included, so a
-- cycle seen between files alone would never contain a .c.
local function module_of(path)
    return path:match("^(src/.+)%.[ch]$") or path
end

-- An assigned file may include an assigned file of a later step only inside
-- one module include cycle: that is how a cycle is cut into consecutive steps.
local function check_include_order(root, c_paths, steps)
    local scanned, includes, modules, edges = {}, {}, {}, {}
    for _, path in ipairs(c_paths) do
        scanned[path] = true
    end
    for _, path in ipairs(c_paths) do
        local module = module_of(path)
        if edges[module] == nil then
            modules[#modules + 1], edges[module] = module, {}
        end
        includes[path] = {}
        for _, target in ipairs(quoted_includes(read_file(root .. "/" .. path))) do
            if scanned[target] then
                includes[path][#includes[path] + 1] = target
                table.insert(edges[module], module_of(target))
            end
        end
    end
    local component = strongly_connected_components(modules, edges)
    for _, path in ipairs(c_paths) do
        local step = steps[path]
        for _, target in ipairs(includes[path]) do
            local target_step = steps[target]
            if step ~= nil and target_step ~= nil and STEP_RANK[target_step] > STEP_RANK[step]
                and component[module_of(path)] ~= component[module_of(target)] then
                report(path .. ": includes " .. target:sub(5) .. " of step " .. target_step
                    .. ", after its step " .. step)
            end
        end
    end
end

local function write_listing(path, entries)
    table.sort(entries, function(a, b)
        if a.step ~= b.step then
            return (STEP_RANK[a.step] or UNASSIGNED_RANK) < (STEP_RANK[b.step] or UNASSIGNED_RANK)
        end
        if a.path ~= b.path then
            return a.path < b.path
        end
        if a.line ~= b.line then
            return a.line < b.line
        end
        return a.text < b.text
    end)
    local file = io.open(path, "w")
    if file == nil then
        usage_failure("cannot write " .. path)
    end
    for _, entry in ipairs(entries) do
        file:write(entry.step .. " " .. entry.text .. "\n")
    end
    file:close()
end

local function print_summary(per_step, totals, assigned_files)
    for _, id in ipairs(STEP_IDS) do
        local counts = per_step[id]
        if counts ~= nil then
            io.write(string.format("%s: %d files, %d/%d functions, %d/%d tables\n", id, counts.files,
                counts["function"][1], counts["function"][2], counts.table[1], counts.table[2]))
        end
    end
    local f, t = totals["function"], totals.table
    io.write(string.format(
        "checklist: %d ported, %d missing, %d not ported, of %d functions; "
            .. "%d ported, %d missing, %d not ported, of %d tables; %d files, each with a step\n",
        f.ported, f.missing, f.not_ported, f.ported + f.missing + f.not_ported,
        t.ported, t.missing, t.not_ported, t.ported + t.missing + t.not_ported, assigned_files))
end

local function exit_with_errors(status)
    for _, line in ipairs(errors) do
        io.stderr:write(line .. "\n")
    end
    os.exit(#errors == 0 and 0 or status)
end

local function main(argv)
    local options = parse_arguments(argv)
    local root = options.root
    local c_paths = list_c_files(root)
    if #c_paths == 0 then
        usage_failure("no C file under " .. root .. "/src")
    end

    local data = read_data(root)
    for _, path in ipairs(sorted_keys(data.units)) do
        if read_file(root .. "/" .. path) == nil then
            report(path .. ": generated unit not found")
        else
            c_paths[#c_paths + 1] = path
        end
    end
    table.sort(c_paths)
    local scanned = {}
    for _, c_path in ipairs(c_paths) do
        scanned[c_path] = true
    end
    for _, path in ipairs(sorted_keys(data.steps)) do
        if not scanned[path] and data.units[path] == nil then
            report(path .. ": step for a file that is not scanned")
        end
    end

    local not_ported_files, not_ported_items = {}, {}
    for _, entry in ipairs(data.not_ported) do
        if entry.kind == "file" then
            not_ported_files[entry.path] = entry
        else
            not_ported_items[entry.path .. " " .. entry.kind .. " " .. entry.name] = entry
        end
    end
    local lua_items = {}
    for _, entry in ipairs(data.lua) do
        lua_items[entry.path .. " " .. entry.name] = entry
    end

    local entries = {}
    local per_step = {}
    local assigned_files = 0
    local totals = {
        ["function"] = { ported = 0, missing = 0, not_ported = 0 },
        table = { ported = 0, missing = 0, not_ported = 0 },
    }
    local lua_cache = {}
    local function definitions_of(lua_path)
        if lua_cache[lua_path] == nil then
            lua_cache[lua_path] = lua_definitions(root .. "/" .. lua_path) or false
        end
        return lua_cache[lua_path]
    end
    local parse_failed = false
    local gate_rank = options.step and STEP_RANK[options.step]
    local found_in = {}

    for _, c_path in ipairs(c_paths) do
        local found, error_line, message = scan(read_file(root .. "/" .. c_path))
        if found == nil then
            report(c_path .. ":" .. error_line .. ": " .. message)
            parse_failed = true
        else
            found_in[c_path] = found
            local whole = not_ported_files[c_path]
            local step = data.steps[c_path] or "-"
            if whole ~= nil and step ~= "-" then
                report(c_path .. ": both a step and not ported")
            elseif whole == nil and step == "-" then
                report(c_path .. ": no step")
            elseif whole == nil then
                assigned_files = assigned_files + 1
                per_step[step] = per_step[step] or { files = 0, ["function"] = { 0, 0 }, table = { 0, 0 } }
                per_step[step].files = per_step[step].files + 1
            end
            local lua_path = data.units[c_path] or lua_path_of(c_path)
            local definitions = definitions_of(lua_path)
            local file_status
            if whole ~= nil then
                file_status = "not-ported"
            elseif #found.functions == 0 and #found.tables == 0 and c_path:match("%.h$") then
                file_status = "no-lua"
            else
                file_status = definitions and "present" or "MISSING"
            end
            entries[#entries + 1] = {
                step = step, path = c_path, line = -1,
                text = "file " .. file_status .. " " .. c_path .. " -> " .. lua_path
                    .. (whole and " -- " .. whole.why or ""),
            }
            for _, kind in ipairs({ "function", "table" }) do
                local items = kind == "function" and found.functions or found.tables
                for _, item in ipairs(items) do
                    local reason = whole or not_ported_items[c_path .. " " .. kind .. " " .. item.name]
                    local exception = lua_items[c_path .. " " .. item.name]
                    local item_lua_path = exception and exception.lua_path or lua_path
                    local lua_name = exception and exception.lua_name or item.name
                    local item_definitions = definitions_of(item_lua_path)
                    local defined = item_definitions
                        and (kind == "function" and item_definitions.functions or item_definitions.tables)
                    local status
                    if reason ~= nil then
                        status = "not-ported"
                        totals[kind].not_ported = totals[kind].not_ported + 1
                    elseif defined and defined[lua_name] then
                        status = "ported"
                        totals[kind].ported = totals[kind].ported + 1
                    else
                        status = "MISSING"
                        totals[kind].missing = totals[kind].missing + 1
                        if gate_rank ~= nil and step ~= "-" and STEP_RANK[step] <= gate_rank then
                            report(c_path .. ":" .. item.line .. ": " .. kind .. " " .. item.name .. " MISSING in "
                                .. item_lua_path .. ", step " .. step)
                        end
                    end
                    if per_step[step] ~= nil and reason == nil then
                        local counts = per_step[step][kind]
                        counts[2] = counts[2] + 1
                        if status == "ported" then
                            counts[1] = counts[1] + 1
                        end
                    end
                    entries[#entries + 1] = {
                        step = step, path = c_path, line = item.line,
                        text = kind .. " " .. status .. " " .. c_path .. ":" .. item.line .. " " .. item.name
                            .. " -> " .. item_lua_path .. " " .. lua_name .. (reason and " -- " .. reason.why or ""),
                    }
                end
            end
        end
    end
    if parse_failed then
        exit_with_errors(2)
    end

    check_stale_entries(root, data, found_in, lua_items)
    check_include_order(root, c_paths, data.steps)
    for _, entry in ipairs(data.not_ported) do
        if entry.kind == "declaration" or entry.kind == "macro" or entry.kind == "rule" then
            entries[#entries + 1] = {
                step = data.steps[entry.path] or "-", path = entry.path, line = 0,
                text = entry.kind .. " not-ported " .. entry.path .. " " .. entry.name .. " -- " .. entry.why,
            }
        end
    end

    if options.listing ~= nil then
        write_listing(options.listing, entries)
    end
    print_summary(per_step, totals, assigned_files)
    exit_with_errors(1)
end

main(arg)
