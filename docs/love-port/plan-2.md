# Plan 2 — The Lua game, a faithful copy of the C

## 1. Goal

Write the whole game in Lua under `game/`, as a **faithful copy** of the C
game (`src/p8/`, `src/engine/`, `src/game/`), drawing through the LÖVE
functions of `docs/love-port/design.md`. Faithful so that plan 3 can put any Lua
function next to its C original when a replay diverges. Read
`docs/love-port/design.md` first.

## 2. Rules

- **Faithful copy:** each C file becomes a Lua file of the same name under the
  same directory (`src/game/planet.c` → `game/game/planet.lua`), same
  functions in the same order, same names, same computations, same data
  tables. No redesign: plan 5 does that.
- **Numbers:** every value that is a `p8_fixed_t` in C is a 16.16 fixed-point
  number in Lua, through `game/p8/fixed.lua` (LuaJIT `bit`), bit-exact. The
  random generator is the C one.
- **Drawing:** only the LÖVE functions of `docs/love-port/design.md`. What PICO-8
  draws and LÖVE does not (circles, lines, fill patterns, see-through colours,
  colour swaps, text) is done by the game, as that file describes, in
  `game/love/`. A static check in `make test` fails on any `love.` call
  outside that list.
- Save and bindings files: the same formats as the C (`docs/save-format.md`,
  `bindings.default.cfg`).
- `debug_state()` returns the same text as the C state dump
  (`src/devtools/state_format.c`).
- The e2e are **not** run during this plan; the replays are plan 3's work.
- Not ported: `src/platform/`, `src/devtools/` (except the state format, as
  `debug_state()`), `src/cmd/`, the C unit tests.

## 3. Phases and steps

### Phase 1 — Checklist and numbers

1.1. The checklist script: lists every C function of `src/p8/`, `src/engine/`,
   `src/game/` and `src/devtools/state_format.c`, header inline functions
   and the two generated units (default bindings, demo) included, with its
   Lua function, or as not ported with why (seeded from the step notes
   below); lists every file-level data table with its Lua table; and assigns
   each C file of `src/engine/` and `src/game/`, headers and generated units
   included, to its Phase 3 step. Verified by it running, reporting every C
   function missing, and every C file having a step.
1.2. `game/p8/fixed.lua`: every function of `src/p8/fixed.h`, random generator
   included. A small C binary prints the C results for generated inputs; a
   LuaJIT test in `tests/lua/` compares the Lua results over millions of
   values (edge values, overflow, division by zero, negative shifts).
   The protocol's plan-wide reference commit, `2e7215c`, is not in this
   repository: this step first names a replacement that is, in
   `docs/love-port/design.md` and its step note. Verified by `make test`
   running it green, within the `make test` rules and timing protocol of
   `docs/love-port/design.md` (recorded in a step note); a sweep over budget keeps the edge values in `make test` and moves
   to a separate target outside `make test-all`. The checklist covers
   `fixed.h`.

### Phase 2 — Runtime and LÖVE glue

2.1. `game/p8/` (except `fixed`) and `game/love/`: palette table, canvas,
   pixel and span drawing, circles and lines with the C algorithms, fill
   pattern images, recoloured sprite sheets, text from `font.png`, the map.
   Verified by Lua files compiling, the static `love.` check, the checklist
   covering `src/p8/`, and one fake check in `make test`: a fixed scene of
   runtime calls drawn through `game/p8/` in `tmp/fake_love`, compared pixel
   for pixel with the C runtime's framebuffer for the same calls. This step
   settles how C constructs Lua lacks are written (struct copies, integer
   division and modulo, 0-based arrays) and records it in its step note;
   later steps follow that note and add to it.

### Phase 3 — Engine and game

One step per group of related C files, in the order of the C include graph,
leaves first, so that each step's modules only require modules already
written; an include cycle is cut into consecutive steps, and if one of its
files cannot load before its partners exist (step 3.3's note says), the load
check of that cycle runs at its last step. The checklist's file
assignment is authoritative. Each step is verified the same way: every C file
assigned to it has its Lua file and every C function its Lua function (the
checklist); every Lua file of the step loads under LuaJIT with its
`require`s; a static check in `make test` (added by step 3.1) fails on any
read or write of an undeclared global; a check in `make test`, under the
timing rules of `docs/love-port/design.md`, finds the step's data tables
equal to the C ones.

3.1. `game/engine/` memory and basics: zones, allocator, value and pointer
   lists, slot groups, vectors and geometry.
3.2. `game/engine/` entity classes, ids, id lists, entity lists, exit queue.
3.3. `game/engine/` the registry, entity geometry, entities, bootstrap.
   This step settles how modules in an include cycle load in Lua, on the
   `entity_geom` / `entity_registry` pair, and records it in its step note;
   later cycles, 3.6 to 3.9 first, follow that note.
3.4. `game/game/` leaves: game data, generation, bindings, mouse, map tables,
   images and text, draw functions.
3.5. `game/game/` techs, HUD and layout.
3.6. `game/game/` the include cycle, galaxy and planets.
3.7. `game/game/` the include cycle, economy.
3.8. `game/game/` the include cycle, slipways, probes and wormholes.
3.9. `game/game/` the include cycle, UI and message box, particles,
   background, scroll.
3.10. `game/game/` displays and panels built on the cycle: trade and slipway
   displays, structure icons, score panel, help.
3.11. `game/game/` pad modes.
3.12. `game/game/` save.
3.13. `game/game/` menus, title and attract.
3.14. `game/game/` the cart and the input step.
3.15. `game/main.lua` and `game/conf.lua`: the loop, the canvas scaled to the
   window, `--attract-after`, `debug_state()`. Verified as the steps above,
   the checklist complete, and `tmp/fake_love` running the game without a
   refusal over a synthetic no-input file of a few hundred frames, with a
   small `attract_after` so the attract demo plays the game, and with its
   state path so `debug_state()` runs (not an e2e replay; its output is not
   compared).

## 4. Done when

- The checklist maps every C function of `src/p8/`, `src/engine/`,
  `src/game/` and `src/devtools/state_format.c` to its Lua function (or lists it as not ported, with why).
- Every Lua file loads; the static `love.` and undeclared-global checks and
  the data table checks are green.
- The fixed-point tests and the drawing check are green, and the game runs
  the synthetic no-input file in `tmp/fake_love`, `debug_state()` included,
  without a refusal.

## 5. Open questions

None.

## 6. Step notes

### From plan 0

Dead since plan 0, each a not-ported entry for the checklist (only
deleted binaries used it): `bindings_compiled_personal`
(`src/game/bindings.h`), `src/p8/format.c`, `GPU_FRAME_BUFFER_BYTES`
and the GPU backend budgets of `src/p8/gpu_sizes.h` (still measured by
`draw_meter.c` and `state_format.c`), the width-multiple-of-16 rule of
`src/p8/image.h`.

### Step 1.1

- `tools/checklist.lua` (LuaJIT, its own C tokenizer) with `tools/checklist.txt`
  (file → step, the two generated units, not-ported entries, Lua-name
  exceptions); `make checklist` writes `tmp/checklist.txt` and prints per-step
  counts; `make checklist STEP=<id>` fails on any function or table still
  MISSING at that step or an earlier one. Outside `make test`; its test
  `tmp/test_checklist` (22 tests) runs under `make checklist`.
- A Lua function is found by the same file, same name rule, as a global,
  `local function`, or module field (`M.name = function` / `function M.name`).
- Real tree: 1563 functions (8 not ported: `format.c`), 529 tables, 206
  files, each with a step. Generated units: `tmp/bindings/core.c` →
  `game/game/bindings_default.lua` (3.4), `tmp/demo/demo.c` →
  `game/game/demo_data.lua` (3.13); `tmp/bindings/default.c` is the replay
  tool's unit, not scanned.
- `src/p8/` files are assigned too (`fixed`, `quarter_wave` → 1.2, the rest
  → 2.1), so `STEP=` works for Phases 1 and 2; `state_format.c` → 3.15.
- The include graph moved files off the step their title names: `game_data`
  and `map_tables` → 3.3; `trade` and the economy formulas, `row_image` and
  `draw_functions` → 3.5; `ui_zone` → 3.6, `particle_zone` → 3.8; engine
  `bootstrap` → 3.9 (includes `ui`, `scroll`, `mouse`); `pad_nav` → 3.13;
  `src/p8/str` → 3.1 (includes `engine/allocator.h`). The checklist is
  authoritative, per the Phase 3 header.
- Include cycles are taken between modules (a `.c` and its `.h`), since only
  headers are included.
- Open for 3.15: `GPU_FRAME_BUFFER_BYTES` is not ported, but `state_format.c`
  prints it, so `debug_state()` must still give that number.

### Step 1.2

- Reference commit: `8dff812` (merge of PR #1, last commit before plan 2's
  work, with the fake checks; `0a04920` lacks them), named in `design.md`
  rule 4. Measured beside `e2f8aca`: 5.60 s and 5.63 s, 4 cores, 11 rounds.
- `game/p8/fixed.lua` (`p8.fixed`): every function of `fixed.h` and
  `fixed.c` in C order, exported as `function M.<c name>(`, private helpers
  as `local function`; a fixed value is a Lua number holding the signed
  32-bit raw value, the random state a number in 0 … 2^32−1; LuaJIT `bit`
  only. `game/p8/quarter_wave.lua` is generated by `make sin-table`
  (indexed from 0 as in C). `make checklist STEP=1.2` passes (26/26, 1/1).
- Out-parameters: a C pointer out-parameter becomes an input argument, and
  its new value comes back after the C return value
  (`result, state = p8_fixed_rnd_from_state(state, x)`); several come back in
  C parameter order; a `void` function returns only them.
- `div` truncates the double quotient directly: exact below 2^48 (the PRD's
  remainder correction moved no record). `magnitude * state` in the random
  generator reaches 2^63 and is split into 16-bit halves.
- Tests: `tools/fixed_reference.c` writes the C results, read by
  `tests/lua/test_fixed.lua`. `lua-test-fixed` in `make test`: 236,867
  records, 39 ms. `make lua-sweep-fixed` (locked, outside `make test-all`):
  8,176,867 records, about 3 s.
- Timing protocol, 4 cores, 11 rounds: plain medians 5.740 s (current),
  5.767 s (`2becd24`, before), 5.755 s (`8dff812`); paired medians −0.17 %
  vs before, +2.10 % vs `8dff812`; spreads 8.6 %, 9.5 %, 12.1 %; slowest new
  check `lua-test-fixed`, 39 ms. Over two thirds of the plan-wide 3 % is
  used (mostly noise at these spreads), so later checks must stay cheap.
