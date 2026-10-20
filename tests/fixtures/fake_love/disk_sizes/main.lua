local log = {}

local function log_size(name)
    local info = love.filesystem.getInfo(name)
    local content, size = love.filesystem.read(name)
    if info == nil then
        table.insert(log, string.format("%s info nil read %s", name, tostring(content)))
    else
        table.insert(log, string.format("%s info %d read %d %d", name, info.size, size, #content))
    end
end

function love.load(arguments)
    local quoted = {"arg " .. #arguments}
    for _, argument in ipairs(arguments) do
        table.insert(quoted, string.format("%q", argument))
    end
    table.insert(log, table.concat(quoted, " "))
    log_size("save.bin")
    log_size("bindings.cfg")
end

function debug_state()
    return table.concat(log, "\n") .. "\n"
end
