---
title: "game/p8/ and game/love/: the PICO-8 runtime in Lua and its LÖVE drawing glue"
description: "The Lua game has no runtime to draw with: no palette, canvas, primitives, sprites, text, map or image slots, so no game module of Phase 3 can draw anything the C draws."
status: needs-triage
---

## Plan step (verbatim, docs/love-port/plan-2.md, Phase 2 — Runtime and LÖVE glue)

> 2.1. `game/p8/` (except `fixed`) and `game/love/`: palette table, canvas,
>    pixel and span drawing, circles and lines with the C algorithms, fill
>    pattern images, recoloured sprite sheets, text from `font.png`, the map.
>    Verified by Lua files compiling, the static `love.` check, the checklist
>    covering `src/p8/`, and one fake check in `make test`: a fixed scene of
>    runtime calls drawn through `game/p8/` in `tmp/fake_love`, compared pixel
>    for pixel with the C runtime's framebuffer for the same calls. This step
>    settles how C constructs Lua lacks are written (struct copies, integer
>    division and modulo, 0-based arrays) and records it in its step note;
>    later steps follow that note and add to it.

## Hypotheses on step 1.2 (not done when this PRD was written)

Step 1.2 (`game/p8/fixed.lua`, PRD `docs/prd/triage/phase1-step2-fixed-lua.md`) is not implemented yet. Every assumption this PRD makes about its outcome is labelled below and referred to by its label. When 1.2 lands differently, the implementer follows what 1.2 actually did and notes the difference in the step 2.1 note.

- **H-1.2-1 — module shape.** `game/p8/fixed.lua` is required as `p8.fixed` (the game directory is the Lua root, as under LÖVE and in `tmp/fake_love`, whose `package.path` is `<game dir>/?.lua`). It returns one module table; exported functions are module fields with the exact C names, written at line start as `function M.p8_fixed_add(` so the checklist sees them; file-local helpers are `local function`s with the C names. Step 2.1 adopts this shape for every `game/p8/` module.
- **H-1.2-2 — representation.** A fixed value is a Lua number holding the signed 32-bit raw value; the random state is a number in 0 … 2^32−1. No `src/p8/` file of step 2.1 computes with `p8_fixed_t` (the drawing API takes `int`s; `str.c` moved to 3.1), so 2.1 depends on this only through the conventions note, which must not contradict it.
- **H-1.2-3 — out-parameters.** A C out-parameter (`T *out`) becomes an extra Lua return value, after the C return value if any (`p8_fixed_rnd_from_state(state, x)` returns `result, new_state`). Step 2.1 adopts it (first use: `visible_cells` in `gfx.c` returns `first, end_`) and writes it into the conventions note as the plan-wide rule.
- **H-1.2-4 — Lua tests.** `tests/lua/` exists and holds LuaJIT test files run by `luajit` from the repository root, each extending `package.path` to `game/` and run by its own `make test` prerequisite target; step 2.1 adds its Lua-only unit test there the same way.
- **H-1.2-5 — timing reference.** Step 1.2 replaced the plan-wide reference commit `2e7215c` by `8dff812` in `docs/love-port/design.md` rule 4, with its measured median on the container. Step 2.1's timing gate runs against its own "before" commit and `8dff812`.
- **H-1.2-6 — step notes.** A `### Step 1.2` note exists in `docs/love-port/plan-2.md` § 6; the `### Step 2.1` note goes after it.
- **H-1.2-7 — checklist gate.** `make checklist STEP=2.1` fails on anything MISSING at step 2.1 *or earlier*, so it passes only if step 1.2 left `make checklist STEP=1.2` green (26/26 functions, 1/1 table). This PRD assumes it did; step 2.1 adds no `fixed` item.
- **H-1.2-8 — `bit` only.** `game/p8/fixed.lua` uses LuaJIT's `bit` and plain numbers, no FFI, no globals; step 2.1's modules follow the same rule.

## Problem Statement

Plan 2 copies the C game into Lua, function by function, so that plan 3 can put any Lua function beside its C original when a replay diverges. Every Phase 3 module draws through the PICO-8 runtime of `src/p8/`: `cls`, `pset`, `line`, `rect`, `rectfill`, `circ`, `circfill` with fill patterns, `spr` with see-through colours and colour swaps, `map`, `print` and outlined print, and the image slots (`p8_image_build` / `p8_image_draw`) the HUD, text and button images are baked into. Nothing under `game/` provides that runtime, and nothing turns those calls into the few LÖVE functions the design allows (`points`, integer `rectangle`, quads of images), which draw pixel-exact only when used by the design's rules. Without it, no Phase 3 step can write a drawing function, and plan 3 cannot tell a drawing bug from a game bug.

The C runtime also carries pieces Lua has no direct equivalent for (structs copied by value, truncating integer division, 0-based arrays, out-parameters, function-pointer tables), and every Phase 3 step will meet them. If each step improvises, the "same computations" rule of the plan breaks silently.

## Solution

1. **The faithful runtime, `game/p8/`.** Each C file of `src/p8/` assigned to step 2.1 by the checklist becomes the Lua file of the same name, same functions in the same order with the same names, same data tables: the palette, the vram (draw palette, sprite sheet, map cells, screen), the input frame reader, the software rasteriser with its font rows and image slot store, the draw-backend dispatch, the image slot builder and its guard, the draw-cost model and the draw meter. `make checklist STEP=2.1` passes.
2. **The LÖVE glue, `game/love/`.** A LÖVE draw backend, installed through the same `p8_draw_backend_set` the C uses, draws every runtime call onto the game canvas with only the listed LÖVE functions, by the design's rules: palette colours as `byte / 255`, pixels as `points(x + 0.5, y + 0.5)`, spans as integer rectangles, lines and circles computed with the C algorithms, fill patterns as cached pattern images, colour swaps and see-through colours as cached recoloured copies of `sprites.png`, text as quads of a white-lit image built from `font.png`, image slots as images built from the software rasteriser's slot bytes.
3. **The proof.** A fixed scene of runtime calls, written once in a text file, is played by the C runtime (software framebuffer) and by `game/p8/` with the LÖVE backend in `tmp/fake_love`; one `make test` check compares every pixel of every frame, and the draw meter's counts, and names the first difference.
4. **The guard.** A static check in `make test` fails when a Lua file under `game/` does not compile, or names a `love.` function outside the design's list, or draws (`love.graphics`, `love.image`) outside `game/love/`.
5. **The conventions note.** The step note of 2.1 records how each C construct is written in Lua; Phase 3 follows it.

## User Stories

1. As a Phase 3 implementer, I want `require("p8.gfx")` to give me `p8_cls`, `p8_pset`, `p8_line`, `p8_rect`, `p8_rectfill`, `p8_circ`, `p8_circfill`, `p8_spr`, `p8_map`, `p8_print` and the rest with their C names and C arguments, so that I copy a C drawing function line by line.
2. As a plan 3 debugger, I want every `src/p8/` function of step 2.1 to have a Lua function of the same name in the Lua file of the same name, in the same order, so that a grep for a C name lands on its copy.
3. As a plan 3 debugger, I want the Lua data tables (palette, font rows, slot sizes, packed sprite sheet) to hold the C values under the C names, so that a table can be compared with its original.
4. As a Phase 3 implementer, I want the Lua graphics state (`p8_graphics_t`: camera, fill pattern, transparent flag, pen colour) and vram (`p8_vram_t`) to be tables with the C field names, so that game code reads and writes them as the C does (`mem.draw_palette[col]`).
5. As a Phase 3 implementer, I want `mem.draw_palette` indexed 0–15 like the C array, so that `hud.c`'s direct palette writes copy unchanged.
6. As a Phase 3 implementer, I want `p8_vram_init` to set colour 0 see-through and give a fresh map version, so that the start state equals the C one.
7. As a Phase 3 implementer, I want `p8_mset` and `p8_map` with the C bounds and the C visible-cell computation, so that the map draws exactly the cells the C draws.
8. As a Phase 3 implementer, I want `p8_palt` and `p8_gfx_is_transparent` to ignore colours outside 0–15 as the C does, so that out-of-range calls agree.
9. As a Phase 3 implementer, I want `p8_fillp`, `p8_fillp_transparent` and `p8_fillp_reset` to mask the pattern to 16 bits as the C does, so that pattern state agrees.
10. As a Phase 3 implementer, I want `p8_image_build` to call my draw function once per 160×120 window with the camera at the window origin and restore the camera, and to fail on a cls, a nested build or a draw during a build, so that the HUD and text images bake exactly as in C.
11. As a Phase 3 implementer, I want `p8_image_draw` to fail on a slot never built, so that a missing build is caught at once.
12. As a Phase 3 implementer, I want `p8_input_init`, `p8_input_step` and `p8_input_read_mouse` with the C semantics, so that the input step copies unchanged.
13. As the 3.15 author of `debug_state()`, I want `p8_draw_meter_install`, `p8_draw_meter_frame_end`, `p8_draw_meter_last_frame` and `p8_draw_meter_peak` to return the C counts for the same calls, so that the state dump's GPU lines print the C numbers.
14. As the 3.15 author, I want the draw-cost functions to give the C packet sizes and primitive counts, the circle iteration count included, so that the meter agrees.
15. As a player, I want every frame the Lua game draws to be the C frame pixel for pixel, so that the port looks exactly like the original.
16. As a player, I want the colours to be the 16 PICO-8 colours byte for byte, so that nothing looks off.
17. As a Lua game developer, I want a single place that creates the game canvas first in `love.load`, after `setDefaultFilter("nearest", "nearest")`, and selects and unselects it around each frame's drawing, so that the design's canvas rules hold without each module knowing them.
18. As a Lua game developer, I want `pset` and `psets` drawn as `points(x + 0.5, y + 0.5)` after the camera, so that each lights exactly its pixel.
19. As a Lua game developer, I want lines drawn from the C Bresenham loop's own pixel sequence, so that every line pixel matches.
20. As a Lua game developer, I want `rect` drawn as four integer rectangles on the C edges, so that outlines match at any size, degenerate ones included.
21. As a Lua game developer, I want `rectfill` without a pattern drawn as one integer rectangle clipped to the screen, so that it is cheap and exact.
22. As a Lua game developer, I want `circ` drawn from the C midpoint loop's eight-way points, so that rings match, radius 0 and negative radius included.
23. As a Lua game developer, I want `circfill` drawn from the C midpoint loop's spans, so that discs match.
24. As a Lua game developer, I want patterned `rectfill` and `circfill` to show the pattern aligned exactly as the C aligns it (on the coordinates before the camera), with the high nibble as the second colour and see-through bits when transparent, so that patterned fills match under any camera.
25. As a Lua game developer, I want each pattern image built once per pattern, colour byte and transparent flag, and kept, so that patterned fills do not rebuild images every frame.
26. As a Lua game developer, I want `spr` to draw a quad of a copy of `sprites.png` recoloured through the current draw palette, see-through colours at alpha 0, built the first time that draw palette is used and kept, so that colour swaps and transparency match.
27. As a Lua game developer, I want `spr` boxes that reach past the sprite sheet to draw the off-sheet part as colour 0 through the draw palette, as the C's `sheet_get` does, so that edge sprites match.
28. As a Lua game developer, I want `print` to place each character from `font.png` with the 4-pixel advance, lowercase as uppercase, `v` as `U`, and the placeholder block for every code without a glyph, so that text matches the C font rows.
29. As a Lua game developer, I want `print_outlined` drawn as the eight neighbour prints in colour 0 then the print in its colour, so that outlined text matches.
30. As a Lua game developer, I want image slots built by the faithful software rasteriser into the slot store, then turned into one image per slot build, see-through where the build wrote nothing, so that baked images match without reading a canvas back.
31. As a Lua game developer, I want `p8_image_draw` to draw the slot image under the camera, clipped to the screen, so that HUD rows land where the C puts them.
32. As a Lua game developer, I want every LÖVE coordinate the backend passes to be an integer inside the canvas (or an integer plus 0.5 for points), so that no call meets the fake's 2^20 bound or a sub-pixel tie.
33. As a Lua game developer, I want the backend to set the colour before every primitive and white before every image draw, so that no leftover tint produces a foreign colour.
34. As a Lua game developer, I want a colour outside 0–15 passed to `pset`, `line`, `rect`, `circ`, `print` or a slot pixel to fail at once naming the call, so that a bug the C's `dump.c` assert would catch is caught in Lua too.
35. As a Phase 3 implementer, I want the software backend to stay the default backend (`p8_draw_backend_set(nil)`), as in C, and the LÖVE backend to be installed explicitly, so that both are the C's structure.
36. As a Phase 3 implementer, I want the conventions note to say how to write struct copies, integer division and modulo, 0-based arrays, unsigned casts, bit operations, out-parameters, function-pointer tables, file-level statics, `assert`, `switch`, `continue`, `char` and strings, and Lua keyword clashes, so that every step writes them the same way.
37. As a Phase 3 implementer, I want small Lua helpers for C integer division and remainder (truncating toward zero), so that `/` and `%` on C `int`s are never written as `math.floor` or Lua `%` by mistake.
38. As a maintainer, I want the static guard to fail on a Lua file that does not compile, naming file and line, so that a syntax error never waits for a run.
39. As a maintainer, I want the static guard to fail on any `love.` name outside the design's list, naming file, line and name, so that the function-list contract is checked before the fake runs.
40. As a maintainer, I want the static guard to fail on `love.graphics` or `love.image` outside `game/love/`, so that all drawing stays in the glue, as the plan's drawing rule says.
41. As a maintainer, I want the guard to refuse aliasing a LÖVE module into a local (`local g = love.graphics`) and indexing `love` with brackets, so that the static check cannot be bypassed by accident.
42. As a maintainer, I want the scene of the check to be one text file read by both the C test and the Lua scene program, so that the two sides can never play different calls.
43. As a developer reading a red check, I want the first differing pixel named with frame, position, C colour and Lua colour, and the count of differing pixels, so that I can go to the faulty primitive.
44. As a developer reading a red check, I want the fake's own error (a refused LÖVE call, a foreign pixel) shown when the Lua side fails, so that I can tell a rule violation from a wrong pixel.
45. As a developer, I want the scene to cover every primitive, inside and outside image builds, with camera offsets, partial and full clipping, patterns, colour swaps, see-through colours, the map, every font code, outlined text and a build of several windows, so that a passing check means the whole runtime agrees.
46. As a developer, I want the scene to span several frames with state carried over (graphics state, draw palette, cached images, meter peaks), so that caches and persistence are checked too.
47. As a developer, I want the check to be its own target in `FAKE_LOVE_CHECKS` and under 100 ms, so that `make test` stays fast.
48. As a plan-pilot run, I want the step's timing measured with the design's protocol and recorded in the step note, so that the ≤ 3 % gate is applied, not assumed.
49. As a maintainer, I want the Lua copy of the packed sprite sheet generated from the C data by `make asset-export` and checked for staleness by `make test`, so that the two never drift.
50. As a maintainer, I want game modules under `game/love/` never named after a LÖVE module, so that real LÖVE's own `love.*` loaders cannot shadow them.
51. As a maintainer, I want `docs/love-port/design.md` to say what the code actually does about fill-pattern alignment and patterned discs, so that the design file stays true.

## Implementation Decisions

### Which C files, which Lua files

- The checklist (`tools/checklist.txt`, step 1.1) is authoritative: step 2.1 owns every `src/p8/` file except `fixed.*`, `quarter_wave.h` (1.2), `str.*` (3.1) and the not-ported `format.*`. Headers with no function or table (`draw_backend.h`, `draw_cost.h`, `draw_meter.h`, `fatal.h`, `gfx.h`, `gpu_sizes.h`, `image.h`, `input.h`, `palette.h`, `sprite_sheet.h`, `vram.h`) are "no-lua" in the checklist: their constants, struct shapes and comments-as-contracts go into the Lua file of their `.c` (`P8_SCREEN_WIDTH` into `vram.lua`, the slot list macros into `image.lua`, `P8_FONT_*` into `gfx.lua`, the GPU packet sizes into `draw_cost.lua`), exported as module fields where the header exports them.
- Lua files to write in `game/p8/`: `palette`, `vram`, `input`, `sprite_sheet`, `sprite_sheet_packed`, `gfx`, `gfx_software`, `image`, `draw_cost`, `draw_meter`, plus `fatal` (see below) and the Lua-only helper `cint` (see Conventions). `make checklist STEP=2.1` must pass (142 functions, 59 tables at the time of writing) — see H-1.2-7.
- `src/p8/input.h` declares `p8_input_load`, `p8_input_save`, `p8_input_append`, defined in `src/devtools/` (not ported, plan 2 rules). They are not written; the checklist only counts definitions.
- `p8_fatal` is declared in `fatal.h` and defined in `src/platform/` (not ported). Lua: `game/p8/fatal.lua` exports `p8_fatal(message)`, which raises a Lua error carrying the message at level 0 (no extra position), so an uncaught fatal ends a `tmp/fake_love` run with exit 1 and the C message text.
- `gpu_sizes.h`: its five macros are not-ported entries of the checklist, and `gpu_sizes.lua` is not created. `draw_meter.lua` needs two of them as table sizes: it declares `GPU_CLUT_SLOTS = 6` and `GPU_PATTERN_SLOTS = 4` as its own locals with the C names (and `CLUT_TABLE`, `PATTERN_TABLE` from them as in C). The frame and build buffer numbers stay 3.15's question (step 1.1's note). The checklist data is not edited.
- `sprite_sheet_packed.lua`, the 8192-byte `p8_sprite_sheet_packed_bytes` indexed from 0, is generated, not hand-written: `make asset-export` writes it beside `game/sprites.png` and `game/font.png` from the same C data, and the existing asset staleness test (`test_asset_image`) gains the same staleness check for it. It is committed.

### Conventions for C constructs (recorded in the step 2.1 note; Phase 3 follows and extends)

- **Modules.** One Lua module per C pair, required by its path under `game/` (`p8.gfx`), returning one table. Exported (non-`static`) functions are written `function M.name(` at line start; `static` functions are `local function name(` (a forward-referenced static is declared `local name` before its first use and defined `function name(` later); file-level tables are `local NAME = {` or `M.NAME = {` at line start. The order is the C order: header items first, then the `.c`. (H-1.2-1.)
- **Calls across modules.** `local gfx = require("p8.gfx")` at the top, calls written `gfx.p8_pset(...)`; the local is named after the C file. No globals are read or written.
- **Structs.** A C struct is a table with the C field names. A pointer to a struct is the table itself. A struct passed, returned or assigned **by value** is a new table: the Lua code never mutates a table it received as a by-value parameter, and a copy is written as a table constructor listing every C field in declaration order (no generic shallow-copy helper), arrays inside it copied element by element. `memset(&s, 0, sizeof s)` sets each field to 0 explicitly.
- **Arrays.** A C array is a Lua table indexed with the C index unchanged, from 0; nested for 2-D arrays (`mem.screen[y][x]`, `mem.sprite_sheet[y][x]`, `mem.map_cells[y][x]`). `#t` and `ipairs` are never used on them: the length is the C constant. `memcpy`/`memcmp` of arrays are explicit loops over the C length.
- **Integer arithmetic.** A C `int` is a Lua number holding an integer; `+ - *` are plain Lua arithmetic (C signed overflow is undefined and never relied upon). C `/` on integers is `cint.div(a, b)` (truncation toward zero; a zero divisor raises an error, as C would trap), C `%` is `cint.mod(a, b)` (sign of the dividend). `math.floor` division and Lua `%` are used only where the C itself floors (as in `floor_div_sprite`'s hand-written branches, which are copied as written).
- **Unsigned and narrow types.** `(unsigned char)x` is `bit.band(x, 0xFF)`; masks and shifts use LuaJIT `bit` (`band`, `bor`, `bxor`, `lshift`, `rshift`, `arshift`) and a value above 2^31−1 coming out of `bit` is brought back to the unsigned range explicitly when the C type is unsigned and the value is compared or printed. `unsigned long` counters (map version, meter bytes, pattern key) are plain Lua numbers: they stay far below 2^53. (Consistent with H-1.2-2 and H-1.2-8.)
- **Out-parameters.** Extra return values after the C return value (H-1.2-3); `visible_cells` returns `first, end_`.
- **Lua keywords.** A C identifier that is a Lua keyword (`end`, `repeat`, `until`, `local`, `function`, `nil`, `not`, `and`, `or`, `then`, `elseif`, `in`, `do`, `goto`, …) takes a trailing underscore.
- **Characters and strings.** A C `char` is its byte value as a Lua number (`string.byte`), compared with `string.byte("A")` or the decimal code; a C `char` above 127 is the C signed value only where the C relies on the sign (nowhere in `src/p8/`). A `const char *` is a Lua string; walking `p` until `'\0'` is a loop over `1 … #s`.
- **Function pointers and backend tables.** A function pointer is a Lua function; `NULL` is `nil`. A `p8_draw_backend_t` is a table with the 16 C field names, each a function with the C parameters in the C order.
- **File-level statics.** Module-level `local`s, initialised as C initialises them (zero unless the C says otherwise).
- **Control flow.** `assert(cond && "msg")` is `assert(cond, "msg")`; `switch` is an `if`/`elseif` chain, or a lookup table built once when the cases are pure data (as `p8_font_glyph_rows`, whose identity comparison with the placeholder table is kept for `p8_font_has_glyph`); `continue` is `goto continue` with a `::continue::` label at the loop end (LuaJIT supports `goto`); a C `for` becomes a numeric `for` only when the body changes neither the counter nor the bound, otherwise a `while`.
- **Fatal errors.** `p8_fatal(msg)` → `fatal.p8_fatal(msg)`; `snprintf`-built messages are built with `string.format` and the same text.

### `game/p8/` behaviour notes

- `gfx.lua` keeps the backend dispatch exactly as `gfx.c`: `p8_draw_backend_set(backend)` returns the previous backend, `nil` installs the software backend, `p8_draw_backend_get` never returns `nil`. Lua has no process-wide state other than module locals, which play that role.
- `gfx_software.lua` is the faithful software rasteriser: it writes `mem.screen` (or the slot being built), keeps the slot store (`SLOT_PIXELS`, `IMAGE_EMPTY` = 0xFF) and the font rows. It is the default backend and is used by the LÖVE backend for every image build (see below).
- `image.lua` keeps the guard backend (`GUARD`) and the slot sizes generated from the same slot list as `image.h`; the "width multiple of 16" rule stays not ported (checklist).
- `draw_cost.lua` and `draw_meter.lua` are faithful copies; nothing in the game installs the meter before 3.15, but the scene check installs it (see Testing Decisions).

### `game/love/`: the LÖVE glue

- **Module names.** `game/love/` modules are required as `love.<name>`; a name is never the name of a LÖVE module (`graphics`, `image`, `font`, `filesystem`, `joystick`, `timer`, `audio`, `sound`, `data`, `math`, `system`, `window`, `event`, `keyboard`, `mouse`, `physics`, `thread`, `touch`, `video`), because real LÖVE preloads those `love.*` names. Planned modules: `screen`, `backend`, `pattern`, `sheet`, `text`, `slot` (names may be adjusted under the same rule).
- **`love.screen`.** `screen_init()`, called first in `love.load`: `setDefaultFilter("nearest", "nearest")`, then `newCanvas(160, 120)` (the first canvas, canvas 0), then loads `sprites.png` and `font.png` once by bare name with `love.image.newImageData`. `screen_frame_begin()` selects the canvas; `screen_frame_end()` unselects it. It also holds the 16 palette colours as `byte / 255` triples taken from `palette.lua`'s `P8_PALETTE_RGB`. Scaling the canvas to the window is 3.15's (`main.lua`), not this step's.
- **`love.backend`.** One table with the 16 `p8_draw_backend_t` fields, installed with `gfx.p8_draw_backend_set`. It reads `gfx` and `mem` and never writes them, like a C backend. Every primitive applies the camera itself (screen = world − camera) and clips to 0–159 × 0–119 in Lua before calling LÖVE; it never calls `translate`, `push` or `pop`. It sets the colour before every primitive and `setColor(1, 1, 1)` before every image draw.
  - `cls`: `clear(0, 0, 0)` (opaque black, colour 0).
  - `pset`, `psets`: `points(sx + 0.5, sy + 0.5)` for each on-screen pixel; `psets` groups consecutive points of one colour into one `points` table call, in list order.
  - `line`, `lines`: the C Bresenham loop, copied, produces the pixel sequence; on-screen pixels go into one `points` call per segment.
  - `rect`: four integer `rectangle("fill", …)` calls on the C edges (min/max of the corners), each clipped.
  - `rectfill`, `circfill`: the C span decomposition (one span for `rectfill`; the four spans per midpoint iteration for `circfill`, with the C loop copied). A span is drawn as one integer rectangle in colour `col & 0x0F` when the pattern is 0, otherwise as one quad of the pattern image (see below).
  - `circ`: the C midpoint loop's eight-way points, one `points` call.
  - `spr`: one quad of the recoloured sheet for the current draw palette; the part of the 8w × 8h box outside the 128×128 sheet (negative `n`, `n` ≥ 256, a box past the right or bottom edge) is colour 0 looked up through the draw palette: nothing when see-through, otherwise integer rectangles of `draw_palette[0] & 0x0F`. `w` or `h` ≤ 0 draws nothing.
  - `print`, `print_outlined`: see Text.
  - `image_begin`, `image_window`, `image_end` and every primitive called between `image_begin` and `image_end`: forwarded to the software backend (which writes the slot store), then `image_end` builds the slot image (see Slots). The backend therefore knows whether a build is open.
  - `image_draw`: one quad of the slot image, clipped to the screen, under the camera.
  - A colour outside 0–15 reaching `pset`, `psets`, `line`, `lines`, `rect`, `circ`, `print` or a slot pixel raises `fatal.p8_fatal` naming the call and the colour (the C stores the byte and only `dump.c`'s assert catches it; the e2e prove the game never does it). `rectfill` and `circfill` mask as the C's `fillp_color` does.
- **Fill patterns (`love.pattern`).** The C aligns the pattern on the coordinates *before* the camera (`fillp_bit_set` is called with world `x, y`; `plot` subtracts the camera afterwards), so in an image build the phase is the image's own coordinates. The pattern image for a key (pattern & 0xFFFF, colour byte & 0xFF, transparent flag — the C's `p8_draw_cost_pattern_key`) is 164 × 124 pixels, pixel (i, j) being bit `15 − ((j % 4) * 4 + i % 4)` of the pattern: set → `(col >> 4) & 0x0F`, or alpha 0 when transparent; clear → `col & 0x0F`. A span clipped to screen columns `sx0 … sx1` and row `sy` draws the quad at origin `(wx0 mod 4, wy mod 4)` (non-negative modulo of the world coordinates of its first pixel) of size `(sx1 − sx0 + 1) × 1` (a `rectfill` span: its whole clipped height) at `(sx0, sy)`. Built on first use, kept for the run.
  - This replaces two sentences of `docs/love-port/design.md` § How the Lua game draws, which this step corrects: the pattern is aligned to the **world** coordinates (the C's), not to the screen, and a patterned disk is drawn span by span from the pattern image rather than built once per radius (a per-radius image would need one copy per pattern phase as well; spans reuse the rectangle path and give the same pixels).
- **Recoloured sheets (`love.sheet`).** At `screen_init`, `sprites.png` is read once into a 128 × 128 table of palette indices (`getPixel` compared with `byte / 255`; a pixel matching no palette colour raises an error naming it). For each distinct draw palette (its 16 bytes as the cache key) a copy is built on first use: index `c` → entry `draw_palette[c]`; transparent bit set → alpha 0; otherwise palette colour `entry & 0x0F`, alpha 1. Kept for the run.
- **Text (`love.text`).** At `screen_init`, `font.png` (48 × 30) becomes one white-lit image: a pixel of colour 7 → white `FF`, every other pixel → alpha 0. `print(str, x, y, col)` draws, for each byte `b` of `str`, the 3 × 5 cell of code `b` when 32 ≤ `b` ≤ 126, else cell 127, at `(x + 4i, y)` under the camera with `setColor` of palette colour `col`, skipping glyphs wholly off screen and clipping the rest by quad. `print_outlined` is the eight neighbour prints in colour 0 in the C order, then the print in `col`. The C's `p8_font_glyph_rows` and `font.png` agree by construction (`make asset-export`), and the scene checks it.
- **Slots (`love.slot`).** At `image_end`, the built `w × h` region of the slot store becomes an `ImageData` (`IMAGE_EMPTY` → alpha 0, byte `n` → palette colour `n`), then an `Image` kept per slot until the next build of that slot. A build of zero width or height keeps no image, and its draw draws nothing.
- **Cache lifetime.** Caches (pattern images, recoloured sheets, slot images) live in module locals for the run and are never invalidated except slot images on rebuild: the C has no sprite-sheet writes after load and no runtime font change.

### The static guard (`lua-guard`)

- A LuaJIT script in `tools/` with its own small tokenizer (strings, long strings and comments removed before matching), run as `luajit tools/<script> game` by a new `make test` prerequisite target with a `## ` help line of at most 20 words. It fails (exit 1, one line per offence: `file:line: reason`) when:
  1. a `.lua` file under the directory does not compile (`loadfile`, never run);
  2. a `love.` name is not on the design's list: the six callbacks (`conf`, `load`, `update`, `draw`, `gamepadpressed`, `gamepadreleased`) only when defined (`function love.load(` or `love.load = function`), and `love.graphics.{clear, draw, newCanvas, newImage, newQuad, points, pop, push, rectangle, setCanvas, setColor, setDefaultFilter, translate}`, `love.image.newImageData`, `love.joystick.getJoysticks`, `love.filesystem.{read, write, getInfo}` only when called directly;
  3. a module is aliased (`love.graphics` not followed by `.name`), or `love` is indexed with brackets;
  4. `love.graphics` or `love.image` appears outside `game/love/`.
- Methods on LÖVE objects (`:setPixel`, `:getPixel`, `:isGamepadDown`) cannot be typed statically; they stay guarded by the fake at run time.
- Step 3.1 adds the undeclared-global check (plan) to the same script.

### The fake check

- **Scene file.** One text file, one runtime call per line, `#` comments, integers in decimal or `0x` hex; text arguments run to the end of the line. Operations (names follow the C calls): `frame` (ends the current frame), `cls`, `camera x y`, `pset x y c`, `psets x y c …`, `line x1 y1 x2 y2 c`, `lines c x1 y1 x2 y2 …`, `rect`, `rectfill` (x1 y1 x2 y2 c), `circ`, `circfill` (cx cy r c), `fillp p`, `fillp_transparent p`, `fillp_reset`, `palt c t`, `palette c byte` (a direct `draw_palette` write, as `hud.c` does), `spr n x y w h`, `mset x y v`, `map cx cy sx sy w h`, `print x y c text`, `print_outlined x y c text`, `build slot w h` … `end` (the enclosed calls are the build's draw function, replayed for each window), `image slot x y`.
- **Scene program.** A fixture game directory under `tests/fixtures/` holds `main.lua` and the scene file, and relative symlinks to `game/p8`, `game/love`, `game/sprites.png` and `game/font.png` (precedent: the capture harness's `files` symlink), so the fake's `package.path` reaches the real modules. `love.load` calls `screen_init`, sets up `p8_vram_init`, `p8_gfx_init`, `p8_sprite_sheet_load`, installs the LÖVE backend then the meter (as the C replay does: meter around the active backend), and reads the scene with `love.filesystem.read`. `love.draw` of frame k plays the k-th frame of the scene between `screen_frame_begin` and `screen_frame_end`, then calls `p8_draw_meter_frame_end`. `debug_state()` returns the meter's last-frame and peak counts as two text lines (`bytes build_bytes primitives cluts patterns`).
- **C side and comparison.** One C test, `tests/test_love_runtime.c` (not named `test_fake_love*`, so it is not on the fake side of `fake-love-guard`: it links `src/p8/` objects — gfx, gfx_software, vram, sprite_sheet, palette, image, draw_cost, draw_meter — with a local `p8_fatal`, and drives `tmp/fake_love` through `tests/fake_love_cli.c`). It parses the same scene file, plays each frame through the C runtime with the meter installed, packs `mem.screen` as the fake packs canvas 0 (even x in the low nibble, 9 600 bytes per frame), runs `tmp/fake_love <fixture dir> <replay> <capture> <state>` with a replay of as many empty pad frames as the scene has, and compares every frame and the state text. A mismatch prints frame, first differing pixel (x, y), C and Lua colours, the frame's count of differing pixels; a fake failure prints the fake's stderr. The test also asserts that the scene parses to the expected frame count, so an empty or truncated scene cannot pass.
- **Make.** `unit-run-test_love_runtime` joins `FAKE_LOVE_CHECKS`; the test binary depends on every file of `game/p8/`, `game/love/`, the fixture and the scene, so an edit reruns it.
- **Budget.** 100 ms wall time on the container (design rule 2). The scene keeps the number of distinct draw palettes, pattern keys and large slot builds small (each costs up to ~20 000 `setPixel` calls); if the check is over budget, the scene is split into two scene files and two checks (two targets) before the step closes.

### Timing gate

- The step runs the design's protocol (worktrees under `tmp/`, warm-up, 11 rounds rotating order, paired medians; ≤ 3 % against the step's "before" commit and against the reference of H-1.2-5; 11 more rounds if needed). Over 3 % after 22 rounds → shrink or split the scene, or move the slowest part out under rule 5. The step note records plain medians, paired medians, spreads, rounds, core count and the slowest new check's own time.

### Docs

- `docs/love-port/plan-2.md` § 6 gains `### Step 2.1`: the conventions above (the plan-wide rule set Phase 3 follows and extends), the `game/love/` module names and the LÖVE-module-name trap, the pattern alignment finding, the `gpu_sizes` decision, the scene check's location and budget, the measurements.
- `docs/love-port/design.md` § How the Lua game draws: the two corrected sentences (world-aligned patterns; patterned discs by spans).

### Questions

- The `guideline` agent could not be spawned (no agent tool in this run). Every question met was settled from the plan, the design file and the code, and is recorded above: porting `draw_cost`/`draw_meter` (required by the plan's `debug_state()` rule, since the state dump prints the meter counts); keeping the `gpu_sizes` macros as checklist not-ported entries with two locals in `draw_meter.lua`; world-aligned patterns and patterned discs by spans (the C is the reference over the design file's wording).

## Implementation Chunks

1. **Static guard.** The `lua-guard` script (compile check, `love.` list, aliasing, `love.graphics`/`love.image` only in `game/love/`), its C test over fixture trees (prior art `tests/test_checklist.c`), the `make test` target and help line. Runs green on `game/` as it stands (`fixed.lua`, H-1.2-1).
2. **Conventions and the plain runtime.** The step note's conventions section; `p8.cint` with its LuaJIT test in `tests/lua/` (H-1.2-4); `palette`, `vram`, `input`, `fatal`, `sprite_sheet`; the generated `sprite_sheet_packed.lua` through `make asset-export` and its staleness check.
3. **Drawing core in Lua.** `gfx.lua` (dispatch, state functions, map, the wrappers), `gfx_software.lua` (rasteriser, font rows, slot store), `image.lua` (slot sizes, guard, build, draw), `draw_cost.lua`, `draw_meter.lua`. `make checklist STEP=2.1` passes from here; the guard keeps them compiling.
4. **Check harness with the first LÖVE calls.** `love.screen` (filter, canvas, assets, frame begin/end, palette colours), `love.backend` with `cls`, `pset`, `psets` and plain `rectfill`; the scene format, the fixture directory and its symlinks, the scene program, `tests/test_love_runtime.c`, `unit-run-test_love_runtime` in `FAKE_LOVE_CHECKS`; a first scene of those calls, camera and clipping included, green.
5. **Lines and circles.** `line`, `lines`, `rect`, `circ`, plain `circfill`, with their scene lines (degenerate, off-screen, negative radius).
6. **Fill patterns.** `love.pattern` and the patterned `rectfill`/`circfill` paths, transparent patterns, high-nibble colours, under several camera phases; the design.md correction.
7. **Sprites, colour swaps and the map.** `love.sheet`, `spr` (multi-cell, off-sheet parts, `palt`, direct palette writes), `mset`/`map` with camera, over several frames to exercise the cache.
8. **Text.** `love.text`, `print` and `print_outlined` with every code 32–127, lowercase, bytes outside the range, clipping at the screen edges.
9. **Image slots.** `love.slot`, the backend's build forwarding and `image_draw`; scene builds containing every primitive (software path, font rows included), a build of several windows, a rebuild of a slot, drawn under camera and clipped.
10. **Meter, budget and record.** The scene program's meter install and `debug_state()`, the C test's state comparison; measure the check against the 100 ms budget (split the scene if needed); run the timing protocol; finish the step note; `make checklist STEP=2.1` green.

## Testing Decisions

- A good test here checks external behaviour only: the pixels the runtime puts on the canvas and the counts the meter reports, against the C runtime for the same calls; the guard's verdict on given Lua files; `cint`'s results on given integers. No test inspects how a Lua function computes or which LÖVE calls it makes (the fake's own rules already refuse misuse).
- Tested modules: the whole `game/p8/` runtime and `game/love/` glue, through the scene check (one fake check, `tests/test_love_runtime.c`); the guard script, through its C test with fixture trees; `p8.cint`, through a LuaJIT test in `tests/lua/` (H-1.2-4); the generated packed sheet, through the asset staleness test.
- The scene is the oracle's input: each chunk first adds its scene lines and sees the check fail (missing backend field, then wrong pixels), then writes the code. The C side never changes: `src/p8/` is the reference.
- `input.lua`, `vram.lua`'s map version and `palette.lua` are covered by compilation, the checklist, and (palette, vram) the scene; `input.lua`'s three functions get no dedicated test (they are three-line copies; Phase 3's input step exercises them, plan 3's replays prove them).
- Prior art: `tests/test_love_e2e.c` and `tests/fake_love_cli.c` (driving `tmp/fake_love` from a C test and comparing captures); `tests/test_fake_love_frame_capture.c` (the canvas packing); `tests/test_checklist.c` (a C test driving a LuaJIT tool over fixture trees); `tools/fake_love/harness/` (a fixture game directory with a relative symlink); `tests/test_asset_image.c` (asset staleness); `tests/test_gfx.c` and `tests/test_image.c` (the C runtime's own expectations, the seed of the scene's edge cases).
- No C unit test is added for `src/p8/` and none is changed; the e2e are not run in this step (plan 2 rule), and `make e2e` is untouched since no C game code changes. The C change is limited to `asset_export` writing one more file.

## Out of Scope

- `game/p8/fixed.lua` and `quarter_wave.lua` (step 1.2), `game/p8/str.lua` (step 3.1, include graph), `format.*` (not ported).
- `main.lua`, `conf.lua`, scaling the canvas to the window, `debug_state()` of the game, the GPU buffer constants in the state dump (step 3.15).
- Reading the pad (`love.joystick`, the callbacks) and files (`love.filesystem` beyond the scene program's read): steps 3.4, 3.12, 3.14, 3.15.
- The undeclared-global static check (step 3.1) and the data-table equality checks of Phase 3.
- Running any e2e replay or `make love-e2e` (plan 3).
- Audio (out of scope for the whole port).
- Performance of the runtime inside the game beyond the check's budget (plans 3 and 5).
- Extending the LÖVE function list or the fake.

## Further Notes

- The plan's "fill pattern images, recoloured sprite sheets, text from `font.png`" follow `docs/love-port/design.md` § How the Lua game draws, except the two corrections above, which follow the C reference.
- The software rasteriser is kept as the default backend and as the builder of every image slot: it makes slot images exact without reading a canvas back (no listed function reads a canvas), and it keeps `gfx_software.c`'s functions and font rows ported, as the checklist requires.
- At the time of writing, `make checklist` reports step 2.1 at `21 files, 0/142 functions, 0/59 tables`; LuaJIT 2.1 is installed and the container has 4 cores.
