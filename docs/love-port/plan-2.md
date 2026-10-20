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

### Phase 1 — Numbers

1. `game/p8/fixed.lua`: every function of `src/p8/fixed.h`, random generator
   included. A small C binary prints the C results for generated inputs; a
   LuaJIT test in `tests/lua/` compares the Lua results over millions of
   values (edge values, overflow, division by zero, negative shifts).
   Verified by `make test` running it green.

### Phase 2 — Runtime and LÖVE glue

2. `game/p8/` (except `fixed`) and `game/love/`: palette table, canvas,
   pixel and span drawing, circles and lines with the C algorithms, fill
   pattern images, recoloured sprite sheets, text from `font.png`, the map.
   Verified by Lua files compiling and the static `love.` check.

### Phase 3 — Engine and game

3. `game/engine/`, one step per group of related C files. Verified per step:
   every C file of the group has its Lua file, every C function its Lua
   function (a checklist script compares the names), every file compiles.
4. `game/game/`, one step per group of related C files (galaxy and
   generation, planets, economy, slipways, structures, techs, HUD and panels,
   pad modes, menus and title, save, attract). Verified as step 3.
5. `game/main.lua` and `game/conf.lua`: the loop, the canvas scaled to the
   window, `--attract-after`, `debug_state()`. Verified by every file
   compiling and the checklist complete.

## 4. Done when

- The checklist maps every C function of `src/p8/`, `src/engine/`,
  `src/game/` to its Lua function (or lists it as not ported, with why).
- Every Lua file compiles; the static `love.` check is green.
- The fixed-point tests are green.

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
