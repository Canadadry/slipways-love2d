-- Runs tools/lua_names.lua on the fixture game directories of
-- tests/fixtures/lua_names/ and checks its finding lines and exit status.
-- Usage, from the repository root: luajit tests/lua/test_lua_names.lua

local FIXTURES = "tests/fixtures/lua_names/"

local failures = 0

local function run(case, modules)
    local dir = FIXTURES .. case
    local list = assert(io.popen("find " .. dir .. " -name '*.lua' | LC_ALL=C sort"))
    local files = list:read("*a"):gsub("\n", " ")
    list:close()
    local command = "luajit tools/lua_names.lua"
    if modules ~= nil then
        command = command .. " --modules '" .. modules .. "'"
    end
    local pipe = assert(io.popen(command .. " " .. dir .. " " .. files .. " 2>&1; echo \"exit $?\""))
    local lines = {}
    for line in pipe:lines() do
        lines[#lines + 1] = line
    end
    pipe:close()
    local status = tonumber(table.remove(lines):match("^exit (%d+)$"))
    return lines, status
end

local function expect(case, modules, want_lines, want_status)
    local label = case .. (modules and " --modules '" .. modules .. "'" or "")
    local lines, status = run(case, modules)
    local ok = status == want_status and #lines == #want_lines
    for i = 1, #want_lines do
        ok = ok and lines[i] == want_lines[i]
    end
    if not ok then
        failures = failures + 1
        io.stdout:write("FAIL ", label, ": exit ", tostring(status), " (want ", want_status, ")\n")
        io.stdout:write("  got:\n    ", table.concat(lines, "\n    "), "\n")
        io.stdout:write("  want:\n    ", table.concat(want_lines, "\n    "), "\n")
    end
end

expect("clean", nil, {
    "lua-names: 0 prefix findings, 0 field findings, 8 reads checked",
}, 0)

local P = FIXTURES .. "prefixed/"
expect("prefixed", nil, {
    P .. "game/draw_functions.lua:5: game.draw_functions.draw_functions_by_id repeats its module's prefix (stripped: by_id)",
    P .. "game/planet.lua:3: game.planet.PLANET_COUNT repeats its module's prefix (stripped: COUNT)",
    P .. "game/planet.lua:5: game.planet.planet_init_instance repeats its module's prefix (stripped: init_instance)",
    P .. "game/trade.lua:5: game.trade.offer and game.trade.trade_offer both become offer",
    P .. "game/trade.lua:5: game.trade.trade_offer repeats its module's prefix (stripped: offer)",
    P .. "p8/gfx.lua:3: p8.gfx.p8_rectfill repeats its module's prefix (stripped: rectfill)",
    P .. "p8/gfx.lua:5: p8.gfx.P8_SCREEN_WIDTH repeats its module's prefix (stripped: SCREEN_WIDTH)",
    "lua-names: 7 prefix findings, 0 field findings, 1 reads checked",
}, 1)

expect("prefixed", "p8.*", {
    P .. "p8/gfx.lua:3: p8.gfx.p8_rectfill repeats its module's prefix (stripped: rectfill)",
    P .. "p8/gfx.lua:5: p8.gfx.P8_SCREEN_WIDTH repeats its module's prefix (stripped: SCREEN_WIDTH)",
    "lua-names: 2 prefix findings, 0 field findings, 1 reads checked",
}, 1)

expect("prefixed", "game.draw_functions game.planet", {
    P .. "game/draw_functions.lua:5: game.draw_functions.draw_functions_by_id repeats its module's prefix (stripped: by_id)",
    P .. "game/planet.lua:3: game.planet.PLANET_COUNT repeats its module's prefix (stripped: COUNT)",
    P .. "game/planet.lua:5: game.planet.planet_init_instance repeats its module's prefix (stripped: init_instance)",
    "lua-names: 3 prefix findings, 0 field findings, 1 reads checked",
}, 1)

local S = FIXTURES .. "misspelled/"
expect("misspelled", nil, {
    S .. "game/hud.lua:5: screen is bound to p8.vram and p8.fixed",
    S .. "game/planet.lua:4: fixed.from_intx: p8.fixed has no field from_intx",
    S .. "game/planet.lua:4: game.planet does not load: attempt to call field 'from_intx' (a nil value)",
    S .. "game/planet.lua:5: fixed.ONE_: p8.fixed has no field ONE_",
    S .. "game/planet.lua:10: fixed_late.ad: p8.fixed has no field ad",
    S .. "game/planet.lua:12: require(\"p8.fixed\").ONEE: p8.fixed has no field ONEE",
    S .. "game/planet.lua:13: M.helper: game.planet has no field helper",
    "lua-names: 0 prefix findings, 7 field findings, 9 reads checked",
}, 1)

expect("misspelled", "p8.*", {
    S .. "game/hud.lua:5: screen is bound to p8.vram and p8.fixed",
    S .. "game/planet.lua:4: fixed.from_intx: p8.fixed has no field from_intx",
    S .. "game/planet.lua:4: game.planet does not load: attempt to call field 'from_intx' (a nil value)",
    S .. "game/planet.lua:5: fixed.ONE_: p8.fixed has no field ONE_",
    S .. "game/planet.lua:10: fixed_late.ad: p8.fixed has no field ad",
    S .. "game/planet.lua:12: require(\"p8.fixed\").ONEE: p8.fixed has no field ONEE",
    S .. "game/planet.lua:13: M.helper: game.planet has no field helper",
    "lua-names: 0 prefix findings, 7 field findings, 9 reads checked",
}, 1)

local A = FIXTURES .. "allowlist/"
expect("allowlist", nil, {
    A .. "game/economy_tiers.lua:9: planet.needz: game.planet has no field needz",
    A .. "game/hud.lua:7: planet.needs: game.planet has no field needs",
    "lua-names: 0 prefix findings, 2 field findings, 5 reads checked",
}, 1)

expect("prefixed", "game.plant", {
    "lua-names: --modules entry game.plant names no module under " .. FIXTURES .. "prefixed",
    "usage: luajit tools/lua_names.lua [--modules '<module> <dir>.*'] <game dir> <file.lua>...",
}, 2)

if failures > 0 then
    io.stdout:write("test_lua_names: ", failures, " cases fail\n")
    os.exit(1)
end
io.stdout:write("test_lua_names: all cases pass\n")
