local SCANNED_DIRS = { "src/p8", "src/engine", "src/game" }
local SCANNED_FILES = { "src/devtools/state_format.c" }

local errors = {}

local function report(message)
    errors[#errors + 1] = "checklist: " .. message
end

local function usage_failure(message)
    io.stderr:write("checklist: " .. message .. "\n")
    io.stderr:write("usage: luajit tools/checklist.lua [--root <dir>] [--listing <file>]\n")
    os.exit(2)
end

local function parse_arguments(argv)
    local options = { root = "." }
    local i = 1
    while i <= #argv do
        local name, value = argv[i], argv[i + 1]
        if name == "--root" or name == "--listing" then
            if value == nil then
                usage_failure(name .. " needs a value")
            end
            options[name:sub(3)] = value
            i = i + 2
        else
            usage_failure("unknown argument: " .. name)
        end
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

local function main(argv)
    local options = parse_arguments(argv)
    local root = options.root
    local c_paths = list_c_files(root)
    if #c_paths == 0 then
        usage_failure("no C file under " .. root .. "/src")
    end

    local entries = {}
    local totals = {
        ["function"] = { ported = 0, missing = 0 },
        table = { ported = 0, missing = 0 },
    }
    local lua_cache = {}
    local parse_failed = false

    for _, c_path in ipairs(c_paths) do
        local found, error_line, message = scan(read_file(root .. "/" .. c_path))
        if found == nil then
            report(c_path .. ":" .. error_line .. ": " .. message)
            parse_failed = true
        else
            local lua_path = lua_path_of(c_path)
            if lua_cache[lua_path] == nil then
                lua_cache[lua_path] = lua_definitions(root .. "/" .. lua_path) or false
            end
            local definitions = lua_cache[lua_path]
            local file_status
            if #found.functions == 0 and #found.tables == 0 and c_path:match("%.h$") then
                file_status = "no-lua"
            else
                file_status = definitions and "present" or "MISSING"
            end
            entries[#entries + 1] = {
                step = "-", path = c_path, line = 0,
                text = "file " .. file_status .. " " .. c_path .. " -> " .. lua_path,
            }
            for _, kind in ipairs({ "function", "table" }) do
                local items = kind == "function" and found.functions or found.tables
                local defined = definitions and (kind == "function" and definitions.functions or definitions.tables)
                for _, item in ipairs(items) do
                    local status = defined and defined[item.name] and "ported" or "MISSING"
                    local counter = status == "ported" and "ported" or "missing"
                    totals[kind][counter] = totals[kind][counter] + 1
                    entries[#entries + 1] = {
                        step = "-", path = c_path, line = item.line,
                        text = kind .. " " .. status .. " " .. c_path .. ":" .. item.line .. " " .. item.name
                            .. " -> " .. lua_path .. " " .. item.name,
                    }
                end
            end
        end
    end

    if parse_failed then
        for _, line in ipairs(errors) do
            io.stderr:write(line .. "\n")
        end
        os.exit(2)
    end

    table.sort(entries, function(a, b)
        if a.step ~= b.step then
            return a.step < b.step
        end
        if a.path ~= b.path then
            return a.path < b.path
        end
        if a.line ~= b.line then
            return a.line < b.line
        end
        return a.text < b.text
    end)

    if options.listing ~= nil then
        local file = io.open(options.listing, "w")
        if file == nil then
            usage_failure("cannot write " .. options.listing)
        end
        for _, entry in ipairs(entries) do
            file:write(entry.step .. " " .. entry.text .. "\n")
        end
        file:close()
    end

    local f, t = totals["function"], totals.table
    io.write(string.format(
        "checklist: %d ported, %d missing, %d not ported, of %d functions; "
            .. "%d ported, %d missing, %d not ported, of %d tables; %d files\n",
        f.ported, f.missing, 0, f.ported + f.missing,
        t.ported, t.missing, 0, t.ported + t.missing, #c_paths))
    os.exit(0)
end

main(arg)
