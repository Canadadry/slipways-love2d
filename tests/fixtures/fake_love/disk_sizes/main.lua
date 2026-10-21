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

-- tmp/fake_love fails a frame that ends with no canvas 0 or a non-palette pixel on it.
local fixture_load = love.load
function love.load(...)
    love.graphics.setCanvas(love.graphics.newCanvas(160, 120))
    love.graphics.clear(0, 0, 0)
    love.graphics.setCanvas()
    if fixture_load ~= nil then
        return fixture_load(...)
    end
end
