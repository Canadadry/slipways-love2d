log = {}
table.insert(log, next(love) == nil and "love empty" or "love not empty")
table.insert(log, "conf")

function love.conf(t)
    t.window.width = 160
    t.modules.audio = false
    t.audio.mixwithsystem = true
    table.insert(log, "love.conf")
end
