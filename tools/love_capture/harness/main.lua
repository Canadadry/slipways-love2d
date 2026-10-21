local program_path, png_path

function love.errorhandler(message)
    io.stderr:write(debug.traceback("love_capture: " .. tostring(message), 2), "\n")
    os.exit(1)
end

function love.load(args)
    program_path, png_path = args[1], args[2]
    assert(program_path and png_path, "usage: love harness <program.lua> <file.png>")
end

local function check_environment()
    local major, minor, revision = love.getVersion()
    local version = major .. "." .. minor .. "." .. revision
    assert(major == 11 and minor == 5, "LÖVE 11.5 expected, running " .. version)
    local name, gl_version, vendor, device = love.graphics.getRendererInfo()
    local renderer = table.concat({name, gl_version, vendor, device}, " | ")
    assert(renderer:find("llvmpipe", 1, true), "Mesa llvmpipe expected, renderer is " .. renderer)
    print("love_capture: LÖVE " .. version .. ", " .. renderer)
end

function love.draw()
    check_environment()
    local canvas = love.graphics.newCanvas(160, 120)
    local program = assert(loadfile(program_path))()
    assert(type(program) == "function", program_path .. " returned a " .. type(program) .. ", not a function")
    love.graphics.setCanvas(canvas)
    program(canvas)
    love.graphics.setCanvas()
    local png = canvas:newImageData():encode("png"):getString()
    local file = assert(io.open(png_path, "wb"))
    assert(file:write(png))
    assert(file:close())
    print("love_capture: wrote " .. png_path)
    love.event.quit(0)
end
