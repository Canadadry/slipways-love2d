# Good and Bad Tests

Tests of the Lua game live in `tests/lua/game/test_<module>.lua`: local test functions using `check.expect`, handed by name to `check.run`, which runs them all and names each failure. Tests of the fake LÖVE and the tools live in `tests/test_<module>.c`: `static void` test functions using `assert`, called from `main`. Real APIs below where they exist; other names are illustrative.

## Good Tests

**Integration-style**: test through the module's public functions, not its internals.

```lua
-- GOOD: observable behavior, through the module's returned table
local check = require("check")
local economy_formulas = require("game.economy_formulas")
local fixed = require("p8.fixed")

local function test_set_updates_an_existing_resource_in_place()
  local map = { count = 0, entries = {} }
  economy_formulas.resource_map_set(map, fixed.p8_fixed_from_int(28), 3)
  economy_formulas.resource_map_set(map, fixed.p8_fixed_from_int(28), 7)
  local found, value = economy_formulas.resource_map_get(map, fixed.p8_fixed_from_int(28), -1)
  check.expect(found, 1, "food found")
  check.expect(value, 7, "food value after update")
end

check.run({
  { "set updates an existing resource in place", test_set_updates_an_existing_resource_in_place },
})
```

The same in C, for the fake's own code:

```c
/* GOOD: observable behavior, through the public interface */
#include "../tools/fake_love/point.h"

static void test_a_pixel_centre_lights_its_pixel(void) {
    assert(point_find_pixel(0.5, 0, 160) == 0);
}
```

Characteristics:

- Tests behavior callers care about
- Requires only the module (C: includes only the public header)
- Survives internal refactors
- Name describes WHAT, not HOW
- One logical assertion per test

For a path only a player reaches (a click, a rendered frame), drive it headless through the same entry point the player hits — input, update, draw — and assert on the framebuffer or the game state, never by calling the inner function directly.

## Bad Tests

**Implementation-detail tests**: coupled to internal structure.

```lua
-- BAD: digs a local helper out of the module through the debug library
local _, fixed_max = debug.getupvalue(economy_formulas.economy_formulas_route_base_cost, 2)
check.expect(fixed_max(1, 2), 2, "max")
```

```c
/* BAD: reaches into a static helper by including the .c */
#include "../tools/fake_love/pad.c"
static void test_pad_uses_its_helper(void) {
    assert(pad_helper(3) == 3);
}
```

Red flags:

- Including a `.c` file, or a test-only header exposing statics; in Lua, `debug.getupvalue` or a module field added only for the test
- Asserting on call counts or call order
- Test breaks when refactoring without behavior change
- Test name describes HOW, not WHAT
- Verifying through a side channel instead of the interface
- Replaying a recorded session to reach a state a few calls could build

## Assertions and Tests: Complementary Layers

Tests verify behavior from the outside. Assertions enforce contracts from the inside. Both are needed.

When production code asserts, a failing test tells you not just _that_ something broke but _where_ the contract was violated:

```lua
-- Without assertion: a nil gfx fails deep inside p8_rectfill
function M.hud_draw_bar(gfx, mem, y1, y2, col)
  gfx_module.p8_rectfill(gfx, mem, 0, to_int(y1), vram.P8_SCREEN_WIDTH - 1, to_int(y2), col)
end

-- With assertion (real code): fails at the violated contract
function M.hud_draw_bar(gfx, mem, y1, y2, col)
  assert(gfx ~= nil, "hud_draw_bar: p8_rectfill dereferences the graphics state, so a NULL one is not a no-op")
  -- ...
end
```

Rule of thumb: if a test setup must respect a constraint to call a function, that constraint is asserted inside the function.

```lua
-- BAD: verifies by poking the map's storage
local function test_set_stores_the_resource_last()
  economy_formulas.resource_map_set(map, resource(28), 3)
  check.expect(map.entries[map.count - 1].value, 3, "last entry")
end

-- GOOD: verifies through the interface
local function test_a_set_resource_is_found()
  economy_formulas.resource_map_set(map, resource(28), 3)
  local found, value = economy_formulas.resource_map_get(map, resource(28), -1)
  check.expect(found, 1, "food found")
  check.expect(value, 3, "food value")
end
```
