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
files cannot load before its partners exist (2.1's conventions, extended by
3.3's note, say), the load check of that cycle runs at its last step. The checklist's file
assignment is authoritative. Each step is verified the same way: every C file
assigned to it has its Lua file and every C function its Lua function (the
checklist); every Lua file of the step loads under LuaJIT with its
`require`s; a static check in `make test` (added by step 3.1) fails on any
read or write of an undeclared global; a check in `make test`, under the
timing rules of `docs/love-port/design.md`, finds the step's data tables
equal to the C ones.
Step 3.1 creates these three checks once, each over the whole tree; steps
3.2 to 3.15 add their files and tables to them and add no `make test` target.
Step 3.1's note records their cost per file and per table, against what is
left of the plan-wide 3 % (step 1.2's note).
How Phase 3 applies design.md's timing rule 4: each step from 3.3 to 3.14
measures the own times of `lua-guard` and `lua-test-modules`, paired
against its "before" commit, and passes when each stays under 100 ms and
grows within 2× of 3.1's cost per KB and per table; both figures go in its
note. The full rule-4 protocol runs at the end of 3.3, 3.9 and 3.15, against
`8dff812` and the "before" commit of the span since the last checkpoint,
and after any step whose own-time gate fails; a failure picks what to rework
from the recorded own times.

3.1. `game/engine/` memory and basics: zones, allocator, value and pointer
   lists, slot groups, vectors and geometry.
3.2. `game/engine/` entity classes, ids, id lists, entity lists, exit queue.
3.3. `game/engine/` the registry, entity geometry, entities, bootstrap.
   On the `entity_geom` / `entity_registry` pair, this step applies 2.1's
   include-cycle convention; its note extends it only for a partner's value
   used at load time (constants, tables) and says whether a cycle file can
   still fail to load before its partners exist. Later cycles, 3.6 to 3.9
   first, follow both.
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

### Step 2.1

- `game/p8/`: palette, vram, input, sprite_sheet (+ generated
  `sprite_sheet_packed.lua`, `make asset-export`, stale copy fails
  `test_asset_image`), gfx, gfx_software, image, draw_cost, draw_meter, plus
  `fatal` and the Lua-only `cint`. `make checklist STEP=2.1` passes
  (142/142, 59/59). `game/love/`: screen, backend, pattern, sheet, text,
  slot. `make lua-guard` (in `make test`, ~15 ms): every game file compiles,
  only design.md's `love.` names, no aliasing, `love.graphics`/`love.image`
  only in `game/love/`.
- Pixel check: `tests/test_love_runtime.c` plays one scene file through
  `src/p8/` and through `tmp/fake_love`, compares every pixel and the draw
  meter's `debug_state()` text. Split in two parallel `FAKE_LOVE_CHECKS`
  (one scene reached 71–78 ms): `love_runtime` (16 frames, 66 ms) and
  `love_runtime_slots` (3 frames, 35 ms); the second fixture's `main.lua`
  links the first. Lua-only checks: `tests/lua/test_runtime.lua`
  (`lua-test-runtime`, ~25 ms).
- Deviations: fill patterns follow the C, aligned to world coordinates,
  patterned discs drawn by spans (design.md corrected). Recoloured sheets and
  the font image are filled lazily (one 8×8 cell, or the font, on first
  draw) to fit the check's budget; same pixels. `sheet.lua` reads
  `sprites.png`, not `mem.sprite_sheet`: equal because nothing writes the
  sheet at runtime. A fatal inside an image build leaves the build open
  (the C exits). The PRD's agent could not reach `guideline` and settled
  three points itself (port `draw_cost`/`draw_meter`; `gpu_sizes` macros
  stay not ported, two locals in `draw_meter.lua`; world-aligned patterns).
  A verifier caught `cint.div(0, negative)` returning `-0`; fixed with its
  test before commit instead of stopping the run.
- Timing protocol, 4 cores, 11 rounds: plain medians 5.866 s (current),
  5.704 s (`4891cfd`, before), 5.845 s (`8dff812`); paired medians +1.84 %
  vs before, +0.36 % vs `8dff812`; spreads 7.9 %, 10.4 %, 9.8 %; slowest
  check `love_runtime`, 66 ms.

#### C-to-Lua conventions (later steps follow and add to them)

- **Modules.** One Lua module per C pair, required by its path under
  `game/` (`p8.gfx`), returning `M`; a top comment names the C files.
  Exported functions `function M.<c name>(` at line start; `static` ones
  `local function <c name>(` (forward-declared `local name` when used
  first); file-level tables `M.NAME = {` or `local NAME = {`; header
  constants `M.<MACRO> = value`. C order: header, then `.c`. Functions of a
  ported header defined in an unported directory are named in the top
  comment, not written. A Lua-only accessor another module needs is
  `function M.p8_<file>_<what>(` after the C functions, marked Lua only.
  `game/love/` modules have no C original: `module_verb[_object]` names.
- **Calls.** `local gfx = require("p8.gfx")`, called `gfx.p8_pset(...)`; a
  local clashing with a C parameter name is `<name>_module`. A generated
  data module returning one bare table is bound to a local with the C table
  name. No globals, LuaJIT `bit` only, no FFI.
- **Include cycles.** A module of a cycle requires its partner at call time,
  inside the function that needs it, not at load (gfx → gfx_software →
  image → gfx).
- **Generated data.** Written by the tool that owns it (`make sin-table`,
  `make asset-export`), headed `-- Generated by ...`, with a `make test`
  check failing on a stale copy.
- **Structs.** A table with the C field names; a pointer to a struct is that
  table. An init on `T *` fills the caller's table; `memset` sets each field
  to 0. By-value structs (passed, returned, assigned, constant `FREE`) are
  new tables built by a constructor listing every field; never mutate a
  table received by value.
- **Arrays.** C index unchanged, from 0 (`[0] = ...`); 2-D `t[y][x]`; never
  `#t` or `ipairs` on them; `memcpy`/`memcmp` are loops over the C length. A
  pointer into an array is an integer index, `NULL` is `nil`. Lists handed to
  LÖVE are plain 1-based.
- **Integers.** C `int` `/` is `cint.div` (truncates, never `-0`, raises on
  0), `%` is `cint.mod` (dividend's sign); `math.floor` and Lua `%` only
  where the C floors. `(unsigned char)x` is `bit.band(x, 0xFF)`; a `bit`
  result above 2^31−1 is brought back to unsigned where compared or
  printed. Fixed point as `fixed.lua`.
- **Truth values.** C int predicates return 1/0; `if (x)` on an int is
  `if x ~= 0`; a comparison used as a value is `(c) and 1 or 0`.
- **Out-parameters.** An input argument plus an extra return value after the
  C one, several in C order; a `void` function returns only them.
- **Names.** A C identifier that is a Lua keyword takes a trailing
  underscore (`end_`).
- **Characters and strings.** A `char` is its byte value; a `const char *`
  is a Lua string, a walk to `'\0'` a loop over `1 .. #s`.
- **Function pointers.** Lua functions, `NULL` is `nil`; a vtable struct is
  a table with the C field names. An X-macro list is a module function
  calling the entry function once per entry, in order.
- **Statics.** Module-level `local`s, initialised as the C does.
- **Control flow.** `assert(c && "msg")` is `assert(c, "msg")`; a
  compile-time check is a load-time `assert`; `switch` is an `if` chain or a
  lookup table built once; `continue` is `goto continue`; a C `for` is a
  numeric `for` only when the body changes neither counter nor bound;
  `c ? a : b` is `(c) and a or b` only when `a` is never `nil`/`false`.
- **Fatal errors.** `fatal.p8_fatal(msg)` raises `error(msg, 0)`;
  `snprintf` messages use `string.format` with the same text.
- **Lua-only tests.** In `tests/lua/test_runtime.lua`, one process; checks
  against C results use a C reference writer, as `test_fixed.lua` does.

### Step 3.1

- `game/engine/`: zone, zone_sizes, allocator, frame_zone, game_zone,
  slot_tally, slot_group, value_list, pointer_list, collect, vec, geom,
  dispatch_context; `game/p8/str.lua`. `make checklist STEP=3.1` passes
  (146/146; the step has no tables).
- The three Phase 3 checks, each over the whole tree: the undeclared-global
  rule in `lua-guard` (bytecode walk, about 1 ms); `lua-test-modules` (one
  process: requires every module without `love`, then compares every ported
  table, constant and `SIZEOF` entry with `tmp/table_reference.txt`, written
  by `tools/table_reference.c` from one unit per C source in
  `tools/table_reference/`). A step registers its constants by adding a unit
  and its source to `TABLE_REFERENCE_SOURCES`. `make checklist STEP=` fails
  on a ported table the check does not compare. The data check compares
  each table as one sorted block (the PRD's line-by-line comparison took
  78 ms); its agent had no `guideline` and decided that itself.
- Costs: `lua-guard` 2.4 ms fixed + about 0.10 ms per KB of Lua code;
  load 2.1 ms + about 0.043 ms per KB; data about 17 µs per table + 0.21 µs
  per line. Now `lua-guard` 26 ms, `lua-test-modules` 16 ms. Projected at
  the end of the plan (about 600 KB more Lua, 469 tables): `lua-guard` about
  88 ms, `lua-test-modules` about 54 ms, both under 100 ms; `lua-guard` is
  the one to watch, and the large-table comparison is the first thing to
  move out under rule 5 if needed. The C tests `test_lua_guard` and
  `test_lua_modules` take about 100 ms each on fixed fixtures and do not
  grow; kept in `make test` (unit tests, not fake checks).
- Timing protocol, 4 cores, 11 rounds: plain medians 5.794 s (current),
  5.918 s (`2e8bdd4`, before), 5.704 s (`8dff812`); paired medians −0.98 %
  vs before, +1.40 % vs `8dff812`; spreads 9.4 %, 10.1 %, 9.0 %.
- No behaviour tests for engine modules (the replays of plan 3 cover them);
  agents checked behaviour with throwaway scripts.

#### Engine conventions (add to 2.1's)

- **The build Lua copies.** The 64-bit PC build without ASan (the one that recorded the replays): `sizeof(void *)` 8, a `zone_cell_t` 8; `ZONE_BYTES_BY_WIDTH(b64, b32)` returns `b64` after a load-time `assert` on the C condition on `b32`; `BUMP_ZONE_HALVES` 1; `POISON`/`UNPOISON` dropped.
- **Blocks.** An allocation returns a new, empty table, the block; the caller writes what the C writes into the bytes (fields by name, array elements from 0). Zones never read a block. Byte counts, cell rounding, capacity test and the `zone %s full, limit %d bytes|slots` message are the C ones, through `fatal.p8_fatal`. C address arithmetic stays as byte offsets nothing reads.
- **Zone storage and macros.** `BUMP_ZONE_STORAGE`/`FLIP_ZONE_STORAGE` return a new table standing for the cells, never read; `SLOT_ZONE_STORAGE(slot_size, count)` returns `cells, taken` (`taken` 0-indexed zeros). Function-like macros are module functions with the C names.
- **Slots.** A slot zone has two Lua-only fields: `blocks` (the block handed out per slot index, what `slot_at` returns) and `index_of` (block → slot index). Release of a block never handed out, or whose slot was handed out again, fails `release of a pointer outside the zone`; a second release `release of a slot already released`.
- **Copies of unknown type.** A whole-block `memcpy` is `copy_block` (local to `allocator.lua`, every key); a one-element `memcpy` of unknown type is one local of `value_list.lua` (one level deep). The only generic copies; an inline mutable sub-struct is copied by the caller before the add.
- **First-member casts.** A struct reached by casting its first member (`flip_zone_allocator_t`, `arena_t`, `slot_group_handle_t`) is one table holding both structs' fields; the cast is the identity; coinciding names are one object. Assigning a handle by value copies its five fields (`store_handle`). `assert(arena->bump == &a->bump)` becomes `assert(arena ~= nil and a.from ~= nil, "<C message>")`.
- **Statics.** A static struct or inline array of structs is a table (or 0-indexed array of tables) with every field at its C zero value (0, or nil for a pointer).
- **`sizeof`.** Every `sizeof` a module uses is an entry of `M.SIZEOF`, keyed by the C type as written (`SIZEOF["void *"]`, `SIZEOF.arena_t`), 64-bit value, read through `local SIZEOF = M.SIZEOF`, never a bare number; compared with the C compiler by `lua-test-modules`.
- **Constants.** Numeric header macros and enum constants are module fields with the C names, computed as the C does, compared by `lua-test-modules`; a `.c`-local macro is a module local, not compared. A header without a `.c` whose macros Lua needs gets its own Lua file (`engine.zone_sizes`).
- **Truth values.** As 2.1; `!x` is `(x == 0) and 1 or 0` as a value, `x == 0` as a test; a pointer test is `p ~= nil`; Lua booleans never stand for a C int.
- **Zone strings.** A zone-built `char *` is a Lua string: the function performs the C allocation for length + 1 bytes and returns the string; releasing it is a no-op on a bump zone and fails "outside the zone" on a slot zone; `strcmp(a, b) == 0` is `a == b`.
- **Registering constants and sizes.** A unit `tools/table_reference/<dir>_<file>.c` including the header (or the `.c` for statics) prints each with `table_reference_int(module, value, "NAME")` or `"SIZEOF.type"`; add the source to `TABLE_REFERENCE_SOURCES`.

### Step 3.2

- `game/engine/`: entity_id_type, entity_class, entity_id, entity_id_list,
  entity_list, exit_queue (21.2 KB of Lua). `make checklist STEP=3.2`
  passes (60/60, no tables); five reference units added (enums, has bits,
  `ENTITY_ID_NONE`, `EXIT_QUEUE_CAP`, sizes). Step 3.1's conventions held
  unchanged.
- Ids are new tables wherever the C creates, returns or stores one by value,
  shifts inside lists included (a verifier caught `del` moving the table),
  and are compared only through `entity_id_equal`; `ENTITY_ID_NONE` is
  copied at each use. Positions are copied in and out (game code changes
  `self.pos.y` in place). `entity_init_header` writes only the header fields.
  The PRD's agent had no `guideline` and settled these itself.
- Checks, paired against `78d0d19`: `lua-guard` +1.3 ms (about 30 ms),
  `lua-test-modules` +1.4 ms (about 20 ms), within 2× of 3.1's per-KB costs.
- Timing protocol, 4 cores, 11 rounds: plain medians 5.732 s (current),
  5.796 s (`78d0d19`, before), 5.637 s (`8dff812`); paired medians +2.59 %
  vs before, +2.94 % vs `8dff812`; spreads 11.1 %, 12.4 %, 10.4 %. The step
  adds about 3 ms of checks, so the paired figures are mostly noise, but the
  margin against `8dff812` is thin.

### Step 3.3

- `game/game/`: map_tables (169 tables), game_data (215 tables, its six
  functions); `game/engine/`: entity_registry, entity_geom, entity.
  `make checklist STEP=3.3` passes (56/56, 386/386); the data check now
  compares 540 blocks, 30,845 lines.
- Data files: every table is a module field (`M.name`: LuaJIT allows 200
  locals per function); structs the C partly initializes go through local
  constructors that fill every missing field with its C zero value and reject
  a field the struct lacks (`map_tables.map_tables_check_fields`, Lua only);
  constructors come before the tables, which call them at load. A struct
  with a positional C initializer gets a positional constructor. Pointers are
  the very table pointed to; a `char[]`, and a pointer to one, is its Lua
  string. Reference units list each table once in an X-macro (`ARRAY`,
  `STRUCT`, `TEXT`) driving both the pointer-name map and the rendering; a
  pointer to an unlisted table stops the reference build. Helpers in
  `reference_data.h` are `static inline`.
- Include cycle `entity_geom` / `entity_registry`: each requires the other at
  call time only; neither needs a partner's value at load, so both load
  alone in either order and the load check runs here. Extension of 2.1's
  rule for later cycles: a partner's constant used inside functions is read
  at call time, never cached at module level; a load-time initializer
  needing a partner's value requires that partner at load, in one direction
  per pair only (the step meeting a two-way need decides and records how);
  only that one-direction case can make a cycle file fail to load alone.
- A C ternary choosing between two function pointers is an `if`/`else`,
  never `a and b or c` (a nil `b` falls through).
- Own-time gate, paired against `d88bb89` (45 pairs; +99.4 KB Lua, +420
  blocks, +5,467 lines): `lua-guard` 28.6 → 36.5 ms (+7.8, 0.79× the 3.1
  model, about 0.078 ms per KB); `lua-test-modules` 19.0 → 36.2 ms (+15.8,
  1.26×; load 0.055 ms per KB, data about 20 µs per block). Pass.
- Projection to the end (about 580 KB more Lua, 83 tables left):
  `lua-guard` about 82 ms (95 ms at the 3.1 rate), `lua-test-modules` about
  70 ms. With 82 % of the tables in, the risk is now `lua-guard`'s cost per
  KB of code, not the large-table comparison.
- Checkpoint, rule-4 protocol: first 11 rounds +3.25 % vs `8dff812`, so 22;
  pooled plain medians 5.945 s (current), 5.977 s (`d88bb89`), 5.846 s
  (`8dff812`); paired −0.87 % vs before, +2.18 % vs `8dff812`; spreads
  14.8 %, 18.3 %, 41.0 % (one 7.8 s outlier). Pass. A PRD agent ran during
  the first rounds and likely added noise: no other agent runs during a
  protocol.
- The PRD's agent had no `guideline`; it settled the cycle extension and the
  data-file shape itself.

### Step 3.4

- `game/game/`: mouse_button, visual_random, gen, grab_arena, targeting
  (constants only), bindings, bindings_default (generated), image_slot
  (constants only), text_image, button_image, mouse. `make checklist
  STEP=3.4` passes (56/56, 8/8).
- `make bindings-lua` (outside `make test`) writes
  `game/game/bindings_default.lua` through a new `--lua` kind of
  `bindings_generate`; the data check compares it with `tmp/bindings/core.c`,
  so a stale copy fails `make test`. The Lua parser matches the C byte for
  byte on 60 cases (positions 1-based with exclusive ends, a NUL ends the
  text as `strlen` does, error texts cut to 255 bytes as `snprintf` does).
- A C name compared by address is compared by value in Lua (`grab_arena`:
  no other zone is named "grab"). The class renderer moved to
  `tools/table_reference/reference_class.h`, shared by later class tables.
- Behaviour checked against the C with throwaway programs: picker, random
  walk and visual stream byte-identical (359 lines); text and button caches
  byte-identical on 784 events.
- Own-time gate, paired against `a880ee9` (45 pairs; +33.7 KB Lua, +79
  blocks, +755 lines): `lua-guard` 36.1 → 39.2 ms (+3.0, about 0.088 ms per
  KB); `lua-test-modules` 33.2 → 34.9 ms (+1.3, below the model: small
  blocks). Pass; not a checkpoint, so no full protocol. `lua-guard`
  projected at about 87 ms at the end.
- The PRD's agent had no `guideline` and settled its points itself.

### Step 3.5

- `game/game/`: main_economy (constants only: its functions live in
  `probe.c`, step 3.8), economy_formulas, trade, tech_gate, research,
  data_init, hud, draw_functions, layout, row_image. `make checklist
  STEP=3.5` passes (130/130, 5/5).
- `tech_gate` requires `game.probe` at call time: it loads now and runs from
  3.8 (so do `research_project_cost` and `research_project_production`).
- A value copy of a trade route shares its id lists (the header's
  immutability contract); a resource map's entries change in place, so a
  copy of a map copies them. Pointer differences into a table go through
  Lua-only reverse maps built at load; records created at load are zeroed in
  place (identity kept). The research project epoch wraps at 16 bits, the
  layout row serial at 32. Layout cells are `{ kind, as }`, never changed
  after build.
- The row image hash puts 0 where the C mixes in the vram address: the hash
  is read only by `same_key`, which also compares `mem` and every entry, so
  behaviour cannot differ (4,000 draws matched the C on slots, meter and
  screens).
- Behaviour checked against the C with throwaway programs, all
  byte-identical: economy and trade, the 34 research projects, 86 HUD and
  draw-function scenes, layout rows and tooltips.
- Own-time gate, paired against `06dec0b` (45 pairs; +59.3 KB Lua, +27
  blocks, +324 reference lines): `lua-guard` 41.5 → 45.5 ms (+4.2, about
  0.071 ms per KB); `lua-test-modules` 38.1 → 41.0 ms (+2.7, 0.88× the 3.1
  model). Pass. Absolute times ran about 2 ms higher than 3.4's on the same
  tree; the paired figures count. `lua-guard` projected at 80–94 ms at the
  end (about 487 KB of Lua left, 70 tables).
- The PRD's agent had no `guideline` and settled its points itself.

### Step 3.6

- `game/game/`: ui_zone (a leaf, not in the cycle), planet_trade,
  planet_display, planet, galaxy, planet_family. `make checklist STEP=3.6`
  passes (140/140, 15/15).
- Cycle loading: planet requires planet_display and planet_trade at load
  (its class holds their methods), one direction only; they, and galaxy,
  require their partners at call time. planet_family cannot load before
  3.9: its class tables hold probe's targeting methods by identity (`probe.c`
  compares `drag` with `targeting_track_method`), and probe's own class needs
  `particle` (3.9). The load check takes `--defer <module>` from the
  Makefile's `LUA_DEFERRED_MODULES` (now `game.planet_family`): a deferred
  module is skipped, with its reference blocks, only when a module not yet in
  the tree is missing; any other error fails; a deferred module that loads
  fails until dropped from the list, so 3.9 empties it.
- Conventions: an object's own field named like an entity header field takes
  a trailing underscore (the planet's `has` is `has_`, the header keeps
  `has`; `button_t`, `ui_t`, `wormhole_t` and the particle structs meet the
  same clash), decided by `guideline`. Structs owned by a later step's module
  get local zero constructors there (`button_keys_t`, `toast_keys_t`, …); 3.8
  and 3.9 may move them home. A function declared in a header but defined in
  a sibling module is called from its defining module, not re-exported.
  planet exports `planet_keys_zero` and `planet_row_signature_zero` (Lua
  only); `planet_init_instance` creates the sub-tables later code fills.
- For 3.8: probe must export `targeting_tooltip_method`,
  `targeting_track_method`, `targeting_confirm_method` and
  `probe_spawn_particle` as module fields and not require planet_family at
  load; it will itself be deferred until `particle` exists.
- Behaviour checked against the C with throwaway programs, all
  byte-identical: ui_zone slots and overflows, trade offers and row
  signatures, a 40-planet galaxy with rolls and installed projects, planet
  family bobbing, kind lookup and burst particles.
- Own-time gate, paired against `bb34696` (45 pairs; +85.4 KB Lua, +39
  blocks of which 14 deferred, +364 lines): `lua-guard` 45.8 → 51.3 ms
  (+5.5, about 0.064 ms per KB); `lua-test-modules` 38.3 → 40.9 ms (+2.4;
  planet_family stops at its probe require). Pass. Projection at the end:
  `lua-guard` 77–91 ms (about 402 KB left), `lua-test-modules` 63–70 ms.
- The PRD's agent had no `guideline` and settled the load plan itself.

### Step 3.7

- `game/game/`: economy_tiers, economy_score, economy. `make checklist
  STEP=3.7` passes (43/43, 2/2). All three load now: their only load-time
  values (a zero class, a four-literal archetype, a zero button) need no
  partner; message_box, slipway, probe, planet and the siblings are required
  at call time. `LUA_DEFERRED_MODULES` stays `game.planet_family`.
- The economy's own bits are `has_`. That exposed a step 3.5 bug: tech_gate
  read the header's `has` for `ECONOMY_HAS_DEMAND`, so the crowding discount
  never applied; fixed in the economy commit (`d420259`).
- A C `int *` context is a `{ value = 0 }` table. `economy_score_set_difficulty`
  clears and refills `g_difficulty` in place, so a pointer handed out earlier
  sees the new values.
- For 3.8: probe requires `game.economy` at call time only; slipway must
  export `slipway_class` and `slipway_served_planets_add`.
- Behaviour checked against the C with throwaway programs, all
  byte-identical: tiers over 387 planets, scores over 160 planets and seven
  economy states, 30 clock calls across new years with spending, all 19
  inventions and both end states.
- Own-time gate, paired against `05325e9` (45 pairs; +34.4 KB Lua, +3
  blocks, +11 lines): `lua-guard` +0.7 ms wall, +2.4 CPU (55 ms);
  `lua-test-modules` −0.3 wall, +0.1 CPU (45 ms); within noise and the
  model. Pass. Projection at the end: `lua-guard` 79–88 ms,
  `lua-test-modules` 65–72 ms.
- The PRD's agent had no `guideline` and settled its points itself.

### Step 3.8

- `game/game/`: particle_zone, probe (with the main economy and targeting
  functions of `probe.c`), slipway, wormhole. `make checklist STEP=3.8`
  passes (100/100, 13/13).
- Deferred loads: only particle_zone loads now. probe requires `game.particle`
  at load (its class draws with `emitter_update_method`), so it waits for
  3.9. slipway and wormhole hold `slipway_draw_method` and
  `wormhole_draw_method` by identity, defined in `slipway_display.c` and
  `wormhole_display.c`, which the checklist assigns to 3.10 (outside the
  cycle); so they wait for 3.10 (decided by `guideline`: hold the C methods,
  defer, no forwarders). `LUA_DEFERRED_MODULES` is now `game.planet_family
  game.probe game.slipway game.wormhole`: 3.9 drops planet_family and probe,
  3.10 drops slipway and wormhole, so 3.10 (not 3.9, as 3.6 said) empties it.
  With the missing modules stubbed, all their blocks already match the C.
- The load check clears the non-table entries a failed require leaves in
  `package.loaded` (LuaJIT's sentinel), so a module first required by a
  failing deferred one reports its own missing module, not "loop or previous
  error".
- Field names: the slipway's `end` is `end_`, the wormhole's own `has` is
  `has_`. `targeting_zero` lives in `game.targeting`, `slipway_keys_zero` in
  `game.slipway`; probe keeps a local `emitter_zero` for 3.9 to fold into
  particle. `reference_header` is in `tools/table_reference/reference_class.h`.
- For 3.9: particle requires probe at call time (`emitter_of`); dropping
  planet_family and probe from the list makes their 20 blocks count.
- Behaviour checked against the C with throwaway programs, all
  byte-identical: particle zone slots and overflows; targeting over a
  49×34 grid, tooltips, probe particles and the main economy; slipway drags,
  drops and obstructions; wormhole anchors, 494 drags, drops and unfolding.
- Own-time gate, paired against `cd653b0` (45 pairs; +51.6 KB Lua, +12
  compared blocks, +19 deferred, +1,377 compared lines): `lua-guard` 56.5 →
  59.3 ms (+4.0, about 0.078 ms per KB); `lua-test-modules` 43.2 → 46.6 ms
  (+3.7; deferred files are compiled before they fail). Pass. Projection at
  the end: `lua-guard` 77–87 ms, `lua-test-modules` 63–69 ms.

### Step 3.9

- `game/game/`: particle, scroll, background, ui_button, ui, message_box,
  research_display; `game/engine/bootstrap.lua` (requires game.mouse,
  game.scroll and game.ui at load, as the C includes them; nothing requires
  it back). `make checklist STEP=3.9` passes (170/170, 9/9); the PRD's
  per-file counts were off by one in places (ui 43, message_box 35,
  ui_button 36), the total right.
- All eight modules load at once. With particle in the tree, planet_family
  and probe load and their 20 blocks compare equal to the C:
  `LUA_DEFERRED_MODULES` is now `game.slipway game.wormhole`, which 3.10
  empties.
- Zero constructors moved home: `emitter_zero` to particle,
  `button_keys_zero` and `button_zero` to ui_button (economy_score builds its
  difficulty button at load, so it requires game.ui_button at load, one
  direction only), `toast_keys_zero` and `open_box_frame_zero` to
  message_box. The size of a struct field keeps the `type.field` key
  (`SIZEOF["p8_vram_t.draw_palette"]`).
- Behaviour checked against the C with throwaway programs, all
  byte-identical: particles over 70 probe frames, the camera and background,
  buttons, ui hover, presses, drags and drops (1,004 lines), boxes and
  toasts, and research display over every project, planet type and
  technology with world resets (96,681 lines).
- Own-time gate, paired against `42d7734` (45 pairs; +87.0 KB Lua, +61
  compared blocks of which 20 formerly deferred, +282 lines): `lua-guard`
  60.8 → 67.9 ms (+6.7, about 0.077 ms per KB); `lua-test-modules` 46.8 →
  47.7 ms (+2.6; the deferred files were already compiled). Pass.
  Projection at the end (about 229 KB left): `lua-guard` 83–88 ms,
  `lua-test-modules` 55–61 ms.
- Checkpoint, rule-4 protocol, 4 cores, 11 rounds: plain medians 6.194 s
  (current), 6.054 s (`a880ee9`, end of 3.3), 5.923 s (`8dff812`); paired
  +0.23 % vs `a880ee9`, +1.93 % vs `8dff812`; spreads 10.5 %, 10.1 %,
  10.2 %. Pass.

### Step 3.10

- `game/game/`: slipway_display, wormhole_display, planet_family_trade,
  structure_icon, help, score_panel, score_panel_display. `make checklist
  STEP=3.10` passes (122/122, 15/15).
- With the two display modules in the tree, slipway and wormhole load:
  `LUA_DEFERRED_MODULES` is empty (the `--defer` mechanism stays), and every
  ported module loads and is compared (94 modules, 783 names). score_panel
  requires score_panel_display at load (its class holds the display's
  methods); planet_family_trade and help require the planet and probe
  modules whose methods their classes hold.
- No C code compares the score panel's text addresses (audited across the
  modules that receive them), so Lua strings are equivalent.
- `fix-bug` finding, stopped with no change (latent): planet tables leave
  `sprite`, `palette_block`, `acceleration_factor` and `speed` nil on kinds
  that never write them, where the C reads 0; every reader, C and Lua,
  guards by lineage or class, and instrumented runs trapped no read. Steps
  that serialize or dump planets (3.12 save, 3.15 `debug_state`) must not
  read them on kinds that do not write them, or must treat nil as 0.
- Behaviour checked against the C with throwaway programs, all
  byte-identical: 600 slipway and 120 wormhole frames, the trade structures,
  structure icons and the help screen, the score panel over 18 economy
  states and 31 new years.
- Own-time gate, paired against `457d574` (45 pairs; +65.3 KB Lua, +36
  compared blocks of which 13 formerly deferred, +225 lines): `lua-guard`
  63.3 → 67.6 ms (+5.4, about 0.083 ms per KB); `lua-test-modules` 45.7 →
  47.4 ms (+2.7). Pass. Projection at the end (364 functions left):
  `lua-guard` 78–85 ms, `lua-test-modules` 57–59 ms.

### Step 3.11

- `game/game/`: pad_direction (constants only), pad_target, pad_back,
  pad_cycle, cheat, pad_jump, pad_select, pad_slipway, pad_panel, pad_build.
  `make checklist STEP=3.11` passes (89/89, 1/1). All ten load at once
  (no cycle: only pad_nav, input_step and devtools include them);
  `LUA_DEFERRED_MODULES` stays empty.
- Out-parameters come back as extra returns (`pad_panel_step` returns `at,
  x, y, mouse_buttons`; `pad_jump_scroll` returns `dx, dy`); the planet's and
  the button's own flags are read from `has_`.
- Behaviour checked against the C with throwaway programs, all
  byte-identical, with hand mutations caught: target picks over 3,000
  random galaxies (ties exercised), back, cycle and cheat streams (85,002
  lines); jump, select and slipway streams (205,153 lines); panel and build
  over 3,000 random worlds of real buttons and icons.
- Own-time gate, paired against `d7c6a3f` (45 pairs, run twice; +32.2 KB
  Lua, +30 blocks, +33 lines): `lua-guard` +3.3 then +0.8 ms (77.7 ms);
  `lua-test-modules` +0.7 then +1.4 ms (56.4 ms). Pass. Absolute times ran
  2–6 ms higher than 3.10's on the same tree; on that scale `lua-guard` is
  projected at 85–95 ms at the end (about 160–200 KB of Lua left, demo data
  included), still the check to watch.

### Step 3.12

- `game/game/`: save_state (codec and capture), save_load, and the Lua-only
  save_storage. `make checklist STEP=3.12` passes (78/78, 3/3) with the first
  `lua` exception line, for `save_load`'s `node_constructors` (indexed by
  kind, compared through its table reference unit).
- save_storage keeps the record in `save.bin` through `love.filesystem.read`
  and `write` only; the design's list has no `remove`, so erase writes an
  empty `save.bin`, which read treats as no record (as it does a missing or
  oversized file). 17 storage checks pass in `tmp/fake_love`.
- The 3.10 planet-field finding holds for the save: capture and load read
  none of `sprite`, `palette_block`, `acceleration_factor`, `speed`; an
  instrumented trap counted 0 reads over every capture and load below (an
  injected read fires it). The trap remains for 3.15.
- Byte identity, throwaway C-vs-Lua programs: the 230 distinct records the
  e2e and worst-case replays write (all nine node kinds, all eleven flags,
  largest 4,519 bytes) decode alike, re-encode and CRC byte-identically, and
  234,721 mutated records are accepted or refused alike; 400 captures (200
  fresh galaxies, 200 synthetic worlds with every structure kind, probes,
  routes, techs and the skip cases) encode identically; 630 records load to
  the same bytes and the same full world dump, and every corpus record
  re-captures to its own bytes (the synthetic worlds, captured without a
  recompute, do not, in C as in Lua). Hand mutations caught: codec 48/51,
  capture 11/15, load 16/24; each one missed cannot change a result.
- Module locals that clash with a C local take the `_module` suffix
  (`planet_module`, `background_module`), as 2.1 says.
- Own-time gate, paired against `5b3aa02` (45 pairs, run twice; +58.1 KB
  Lua, +36 blocks, +303 lines): `lua-guard` +3.2 then +4.4 ms (73.7–77.4
  ms, 0.055–0.076 ms/KB); `lua-test-modules` +3.3 then +4.1 ms (51.4–55.3
  ms). Pass. With about 100–140 KB of Lua left, `lua-guard` is projected at
  82–90 ms at the end, still the check to watch.

### Step 3.13

- `game/game/`: demo_data (generated), loading_screen, demo_overlay,
  attract, menu, pad_nav. `make checklist STEP=3.13` passes (135/135, 9/9).
  Nothing is in a cycle; `LUA_DEFERRED_MODULES` stays empty.
- `demo_generate --lua` and `make demo-lua` (outside `make test`, as
  `bindings-lua`) write `demo_data.lua` in decimal; the data check compares
  it with the C unit, so a stale copy fails `make test`.
- New conventions: a forward-declared `static` is a `local name` filled
  later by `name = function`, never `local function name`; a C table entry
  naming a forward-declared function is stored right after its definition
  (`galaxy_menu_on_click_table[3]`); a by-value `p8_graphics_t` is a local
  `graphics_copy` of its five fields. `pad_nav_init` keeps its sub-tables
  and bindings table across a re-init and copies the bindings by value with
  a local `copy_bindings` (not exported).
- Behaviour checked against the C with throwaway programs, all
  byte-identical: loading screen, overlay and attract (244,940 lines;
  mutations 16/17), menu over 24 games and 960 steps (21,215 lines; 18/18),
  pad_nav queries (6,897 lines; 20/21), and a frame loop copying the cart's
  pad path over every pad replay stream, the demo's buttons, random
  streams, the 230 save-corpus worlds and three binding sets (637,366
  frames, 96.9% of pad_nav.c's lines run; 71/82). Each mutation missed is
  unreachable or in a state no stream reached. The planet-field trap
  counted 0 reads over the pad comparison.
- fix-bug: the frame loop hit `entity_id_list_at`'s dead-id assert in
  `wormhole_drag`, in C and Lua alike; reproduced with the unmodified
  `tmp/replay` from the title (a bindings file with `aim.back` off 5 and 6).
  A probe refused on the frame before stayed registered and became a new
  wormhole's anchor; `wormhole_is_anchor` now skips `is_dead` entities, in C
  and Lua, with a test in `test_wormhole.c`; every e2e hash unchanged.
- Own-time gate, paired against `263257a` (45 pairs, run twice; +91.5 KB
  Lua, 23.3 KB of it demo data, +31 blocks, +6,671 lines): `lua-guard` +7.1
  then +5.6 ms (78.9–80.6 ms, 0.061–0.078 ms/KB); `lua-test-modules` +4.3
  then +3.5 ms (54.0–54.9 ms). Pass. With about 20–30 KB of Lua left,
  `lua-guard` is projected at 82–86 ms at the end.

### Step 3.14

- `game/game/`: cart, input_step. `make checklist STEP=3.14` passes (32/32,
  1/1); both load at once, `LUA_DEFERRED_MODULES` stays empty. Every
  `H-3.13-k` of the PRD held. `game.save_state` exports `save_state_zero`
  for the cart's static save state.
- `input_step` copies the session bindings by value with a local
  `copy_bindings` (3.13 exports none). `input_step_init` gives `cheat`,
  `pad_nav` and `attract` new tables but zeroes `played_frame` and `played`
  in place, so the input the dispatch context holds reads empty after a
  re-init, as C's memset leaves it.
- Behaviour checked against the C with throwaway scenarios mirroring
  `test_cart.c` and `test_input_step.c`, all byte-identical with
  framebuffers: the cart over 40 scenarios (208,471 lines, every corpus
  resume included; mutations 35/47), the input step over 18 (24,559 lines,
  a full demo included; 18/25). Each mutation missed is an independent
  reorder or unreachable. The planet-field trap counted 0 reads.
- Calling `cart_init` on the title with no flush between fails, in C and Lua
  alike, on the help screen's particle slot; the game never takes that
  path (restart comes from a game, the return to the title flushes first).
- Own-time gate, paired against `ec42dda` (45 pairs, run twice; +18.5 KB
  Lua, +5 blocks, +19 lines): `lua-guard` +0.6 then +1.9 ms (79.1–80.5 ms);
  `lua-test-modules` +1.0 then +1.2 ms (54.0–54.5 ms). Pass. 3.15 adds about
  20 KB: `lua-guard` projected at 81–84 ms at the end.
