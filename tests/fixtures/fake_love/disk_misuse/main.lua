local misuse = os.getenv("FAKE_LOVE_MISUSE")

function love.load()
    if misuse == "dot dot" then
        love.filesystem.read("../x")
    elseif misuse == "no name" then
        love.filesystem.read()
    elseif misuse == "number data" then
        love.filesystem.write("a", 1)
    elseif misuse == "bogus filter" then
        love.filesystem.getInfo("a", "bogus")
    elseif misuse == "size form" then
        love.filesystem.read("a", 10)
    end
end

function debug_state()
    return "ran\n"
end
