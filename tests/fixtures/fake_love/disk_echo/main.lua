local log = {conf_line}
local frame = 0

local function hex(bytes)
    return (bytes:gsub(".", function(byte)
        return string.format("%02x", byte:byte())
    end))
end

local function describe_read(name)
    local content, detail = love.filesystem.read(name)
    if content == nil then
        return "read nil " .. detail
    end
    return string.format("read %d %s", detail, hex(content))
end

local function describe_info(name, filter)
    local info = love.filesystem.getInfo(name, filter)
    if info == nil then
        return "info nil"
    end
    return string.format("info %s %s", info.type, tostring(info.size))
end

local function log_file(name)
    table.insert(log, string.format("%s %s %s", name, describe_info(name), describe_read(name)))
end

local function log_write(name, data)
    local ok, message = love.filesystem.write(name, data)
    table.insert(log, string.format("write %s %s %s", name, tostring(ok), tostring(message)))
end

function love.load(arguments)
    local quoted = {"arg " .. #arguments}
    for _, argument in ipairs(arguments) do
        table.insert(quoted, string.format("%q", argument))
    end
    table.insert(log, table.concat(quoted, " "))
    table.insert(log, "global arg " .. tostring(arg))
    local names = {}
    for name in pairs(love.filesystem) do
        table.insert(names, name)
    end
    table.sort(names)
    table.insert(log, "filesystem " .. table.concat(names, " "))
    log_file("save.bin")
    log_file("bindings.cfg")
    log_file("missing.bin")
    log_file("readme.txt")
    log_file("data/level.txt")
    table.insert(log, "main.lua " .. love.filesystem.getInfo("main.lua").type)
    table.insert(log, "data as file " .. describe_info("data", "file"))
    table.insert(log, "data as directory " .. describe_info("data", "directory"))
    table.insert(log, "data " .. describe_info("data") .. " " .. describe_read("data"))
    log_write("note.txt", "hello")
    log_file("note.txt")
    log_write("empty.txt", "")
    log_file("empty.txt")
    log_write("readme.txt", "mine")
    log_file("readme.txt")
    log_write("saves/a.bin", "x")
    log_file("saves/a.bin")
end

function love.update()
    if frame == 0 then
        log_write("save.bin", "\0\255")
    end
end

function love.draw()
    if frame == 1 then
        log_file("save.bin")
    end
    frame = frame + 1
end

function debug_state()
    log_file("save.bin")
    log_file("note.txt")
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
