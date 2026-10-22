# When to Use a Test Double

Replace only **system boundaries**:

- Memory: the allocator table a `game/engine/` module receives (`{ alloc, grow, release }`); a test backs it with plain tables
- Input: a scripted events file, never a real pad or mouse
- Output: the draw backend of `game/p8/gfx.lua` — the LÖVE backend's frames read back as a fake-LÖVE capture (`tools/capture_read.lua`), the software backend's through the vram — never a window

Don't replace:

- Your own modules
- Internal collaborators
- Any module of `game/` a test can call for real
- Randomness: `p8_fixed_rnd` already starts from a fixed seed, so a run is reproducible as is

## Designing for Test Doubles

**1. Pass the dependency in**

The module receives its allocator, its graphics or its input as a parameter instead of reaching for a global:

```lua
-- Easy to test: the caller chooses the allocator (real signature)
function M.entity_list_init(entity_list, zone)

-- Hard to test: hidden dependency on the game's zone
function M.entity_list_init(entity_list)
```

The test then passes a table of its own; the game passes a zone allocator:

```lua
local function table_allocator()
  return {
    alloc = function(z, n) return {} end,
    grow = function(z, p, old_n, new_n)
      local q = {}
      for k, v in pairs(p or {}) do q[k] = v end
      return q
    end,
    release = function(z, p) end,
  }
end
```

**2. One narrow function per boundary operation**

An allocator has `alloc`, `grow` and `release` — not one `do(op, ...)` entry point with a switch. Each double then implements one plain behavior:

- No conditional logic in the test setup
- Easy to see which operations a test exercises
- A refusing double (`zone.grow = allocator.allocator_grow_refused`) tests the slot-zone path in one line
