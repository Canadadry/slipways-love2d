table.insert(log, "main")

function love.load(args)
    table.insert(log, "load " .. type(args))
end

function love.update(dt)
    table.insert(log, dt == 1 / 60 and "update 1/60" or "update " .. tostring(dt))
end

function love.draw()
    table.insert(log, "draw")
end

function debug_state()
    return table.concat(log, "\n") .. "\n"
end
