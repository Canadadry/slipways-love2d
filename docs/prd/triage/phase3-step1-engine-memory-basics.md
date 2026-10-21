---
title: "game/engine/ memory and basics, and the three Phase 3 checks"
description: "The Lua game has no zones, allocator, lists, vectors or geometry for any engine or game module to build on, and nothing in make test proves that a Lua module loads, uses no undeclared global, and holds the C data."
status: needs-triage
---

## Plan step (verbatim, docs/love-port/plan-2.md, Phase 3 — Engine and game)

The Phase 3 header paragraph (verification rules), as it stands with its last edit:

> One step per group of related C files, in the order of the C include graph,
> leaves first, so that each step's modules only require modules already
> written; an include cycle is cut into consecutive steps, and if one of its
> files cannot load before its partners exist (step 3.3's note says), the load
> check of that cycle runs at its last step. The checklist's file
> assignment is authoritative. Each step is verified the same way: every C file
> assigned to it has its Lua file and every C function its Lua function (the
> checklist); every Lua file of the step loads under LuaJIT with its
> `require`s; a static check in `make test` (added by step 3.1) fails on any
> read or write of an undeclared global; a check in `make test`, under the
> timing rules of `docs/love-port/design.md`, finds the step's data tables
> equal to the C ones.
> Step 3.1 creates these three checks once, each over the whole tree; steps
> 3.2 to 3.15 add their files and tables to them and add no `make test` target.
> Step 3.1's note records their cost per file and per table, against what is
> left of the plan-wide 3 % (step 1.2's note).

The step:

> 3.1. `game/engine/` memory and basics: zones, allocator, value and pointer
>    lists, slot groups, vectors and geometry.

## Hypotheses on step 2.1 (not done when this PRD was written)

Step 2.1 (`game/p8/` and `game/love/`, PRD `docs/prd/triage/phase2-step1-runtime-love-glue.md`) is in progress. Its chunk 1 is committed (`e39c79b`): `tools/lua_guard.lua`, the `make lua-guard` target in `make test` (one LuaJIT process over a file list the Makefile passes as `LUA_GUARD_FILES`, a wildcard to three directory levels, about 15 ms), and its C test `tests/test_lua_guard.c` in `UNIT_TESTS`. These are facts. Everything else 2.1 will produce is an assumption, labelled below and referred to by its label. When 2.1 lands differently, the implementer follows what 2.1 actually did and records the difference in the step 3.1 note.

- **H-2.1-1 — conventions note.** A `### Step 2.1` note in `docs/love-port/plan-2.md` § 6 records the C-to-Lua conventions of the 2.1 PRD: one module per C pair, required by its path under `game/` (`engine.zone`), returning one table `M`; exported functions written `function M.name(` at line start, `static` ones `local function name(` (a forward-referenced static declared `local name` first, defined `function name(` later); the C order, header items first; cross-module calls through a local named after the C file; structs as tables with the C field names, by-value copies as constructors listing every field (no generic copy helper); arrays indexed from 0 with the C length, never `#t` nor `ipairs`; integer `/` and `%` through `p8.cint`; `unsigned` and masks through LuaJIT `bit`; out-parameters as extra return values after the C return value (also step 1.2's note); keyword clashes take a trailing underscore; function pointers as Lua functions, `NULL` as `nil`; file-level statics as module locals; `assert(c && "msg")` as `assert(c, "msg")`; `switch` as `if` chains; `continue` as `goto continue`; `p8_fatal` through `p8.fatal`. Step 3.1 follows these and adds the engine's conventions below.
- **H-2.1-2 — `p8.cint`.** `game/p8/cint.lua` exists (it is in the working tree, uncommitted, at the time of writing) and exports `div(a, b)` (C integer division, truncation toward zero, error on a zero divisor) and `mod(a, b)` (sign of the dividend).
- **H-2.1-3 — `p8.fatal`.** `game/p8/fatal.lua` exports `p8_fatal(message)`, raising a Lua error carrying the C message text at level 0.
- **H-2.1-4 — tables reachable.** 2.1 ports its 59 file-level tables (`make checklist STEP=2.1` green: 142 functions, 59 tables) and each is reachable from outside its module, as a field of the module table under its C name, or as the module's return value (the `p8.quarter_wave` shape). Where 2.1 wrote a table as a bare `local NAME = {`, step 3.1 adds `M.NAME = NAME` to that module (the table check reads module fields only).
- **H-2.1-5 — no load-time `love`.** No module under `game/p8/` or `game/love/` reads `love` while it loads: LÖVE names are touched inside functions only, so every module can be required by a plain LuaJIT with no `love` global.
- **H-2.1-6 — runtime state tables.** `p8_graphics_t`, `p8_vram_t` and `p8_input_t` are Lua tables built by 2.1's modules; `engine.dispatch_context` only stores and returns references to them and requires none of those modules.
- **H-2.1-7 — timing record.** 2.1's step note records its timing measurements under the design's protocol (paired medians against its "before" commit and against `8dff812`), so step 3.1 can compute what is left of the plan-wide 3 % after 1.2 (+2.10 % vs `8dff812`, step 1.2's note) and 2.1.
- **H-2.1-8 — truthiness.** If 2.1's conventions note does not already say how C truth values are written, step 3.1 adds it (see Conventions added by this step); if it does, 3.1 follows it.

## Problem Statement

Plan 2 copies the C game into Lua function by function so that plan 3 can put any Lua function beside its C original when a replay diverges. Every engine and game module of Phase 3 allocates from a zone (the frame zone, the game zone and its load zone, slot groups, arenas), keeps value or pointer lists, builds strings in a zone, and computes with fixed-point vectors and segment geometry. None of that exists under `game/`, so no Phase 3 step after this one can start.

The C memory layer has no direct Lua equivalent: it hands out raw bytes, casts a handle to the struct that embeds it, finds a slot by pointer arithmetic, and counts bytes with `sizeof` values that depend on the 64-bit build. Its counts are what the C state dump prints (`zone game …`, slot peaks), and its limits are what turn into `zone … full` fatal errors. If each step improvises how a block, a handle cast or a `sizeof` is written in Lua, the "same computations" rule breaks silently, and the end-state comparison cannot catch it (`tools/end_state.sh` drops the `zone`, `budget` and `cap` lines).

Finally, the plan verifies every Phase 3 step with three checks — each Lua file loads with its `require`s, no Lua file reads or writes an undeclared global, the data tables equal the C ones — and only step 1.2's fixed-point test and 2.1's guard exist. They must be created once here, over the whole tree, cheap enough that thirteen later steps can add their files and tables without a new `make test` target and without breaking the plan-wide 3 % timing margin, of which more than two thirds was already used after Phase 1.

## Solution

1. **The faithful engine basics.** Each C file the checklist assigns to step 3.1 becomes the Lua file of the same name: `game/engine/` `zone`, `zone_sizes`, `allocator`, `frame_zone`, `game_zone`, `slot_tally`, `slot_group`, `value_list`, `pointer_list`, `collect`, `vec`, `geom`, `dispatch_context`, and `game/p8/str` (moved to 3.1 by the include graph). Same functions in the same order, same names, same computations, byte counts and limits included. `make checklist STEP=3.1` passes (27 files, 146 functions, 0 tables at the time of writing).
2. **The engine conventions.** The step 3.1 note adds to 2.1's conventions what memory and generic containers need: what a block is, how a handle cast is written, how `sizeof` is written and checked, how C truth values are written, how a zone string is written.
3. **The undeclared-global check.** `lua-guard` gains one rule: it walks the bytecode of every file it already compiles and fails on any global read outside a fixed list and any global write but `debug_state` in `game/main.lua`.
4. **The load and data check.** One new `make test` target, `lua-test-modules`, runs one LuaJIT process that requires every module under `game/` (the load check) and then compares, line by line, a text rendering of every registered Lua data table and constant with the rendering a C reference binary wrote from the C tables. It covers every table already ported (step 1.2's and step 2.1's) and the constants and sizes of step 3.1.
5. **The record.** The step 3.1 note records the conventions, where the three checks live and how a later step extends them, and their measured cost per file and per table against what is left of the 3 %.

## User Stories

1. As a Phase 3 implementer, I want `require("engine.frame_zone").frame_zone()` to give me the frame allocator, so that I copy a C call `allocator_alloc(frame_zone(), n)` line by line.
2. As a Phase 3 implementer, I want `allocator_alloc` to return a fresh Lua table I fill with my struct's fields, so that a C allocation followed by field writes copies directly.
3. As a Phase 3 implementer, I want every allocation to count exactly the C bytes (rounded to whole 8-byte cells), so that the zone counts the state dump prints are the C ones.
4. As a Phase 3 implementer, I want an allocation past a zone's capacity to fail with the C message `zone <name> full, limit <n> bytes` (or `slots`), so that a Lua overflow looks like the C one.
5. As a Phase 3 implementer, I want `allocator_grow` to return a new block holding everything the old block held, and leave the old block as it was, so that list growth behaves as the C `memcpy` into a fresh allocation.
6. As a Phase 3 implementer, I want `allocator_release` on a slot zone to find the slot of the block I pass, and to fail with the C assert message on a block outside the zone or released twice, so that release bugs are caught as in C.
7. As a Phase 3 implementer, I want slot zones to allocate round-robin from a cursor exactly as the C does, so that which slot a block lands in follows the C order.
8. As a Phase 3 implementer, I want `bump_zone_reset`, `bump_zone_rewind` and the frame zone's mark and rewind to follow the C, the generation assert included, so that frame-scoped allocation copies unchanged.
9. As a Phase 3 implementer, I want the flip zone's two generations, `flip`, `pending`, `release`, `reset`, `used` and `max` to follow the C, with the C assert messages, so that the game zone's start-of-game sequence copies unchanged.
10. As a Phase 3 implementer, I want `arena_take`, `arena_spill`, `arena_flush`, `arena_used`, `arena_size` and `arena_release` to behave as the C, the header bytes counted, so that the UI and particle arenas of later steps count the same bytes.
11. As a Phase 3 implementer, I want the arena handle, the arena struct and the block the arena lives in to be one Lua table, so that a C cast between them is the identity in Lua.
12. As a Phase 3 implementer, I want `SLOT_GROUP(kind_table, kind_count)` as a Lua function building the group table, so that `ui_zone` and `particle_zone` declare their groups as the C does.
13. As a Phase 3 implementer, I want slot groups to tally live slots, peaks and arena bytes exactly as `slot_tally.c`, so that the state dump's slot lines agree.
14. As a Phase 3 implementer, I want `value_list_add` to store a copy of the value it is given, so that a later change to my local struct does not change the list, as with the C `memcpy`.
15. As a Phase 3 implementer, I want `value_list_at` to return the stored element itself, so that `trade.c`'s read-only use copies unchanged.
16. As a Phase 3 implementer, I want `pointer_list_iter_next` to keep the C rule that an element deleted during iteration does not skip the next one, so that entity loops behave the same.
17. As a Phase 3 implementer, I want `pointer_list_iter_next` and every other C function returning a truth value to return the C number 0 or 1, so that the C code and its Lua copy read the same, and a 0 never passes as true.
18. As a Phase 3 implementer, I want `collect_filter_pointer_list`, `collect_map_pointer_list` and `collect_concat_pointer_list` with the C argument order and the C NULL tolerance, so that their callers copy unchanged.
19. As a Phase 3 implementer, I want `collect_format_signed` to return the text and then the colour, so that the C out-parameter follows the plan-wide rule.
20. As a Phase 3 implementer, I want `p8_tostr_fixed`, `p8_sub` and `p8_concat2` to return Lua strings with the C text, and to charge their zone the C byte count, so that HUD strings look and count the same.
21. As a Phase 3 implementer, I want `vec_add`, `vec_sub`, `vec_mul`, `vec_div`, `vec_dot`, `vec_length`, `vec_normalize`, `vec_perpendicular` and `vec_from_polar` to compute with `p8.fixed` bit for bit, returning new tables, so that positions match the C exactly.
22. As a Phase 3 implementer, I want `geom_side_of_line`, `geom_segments_cross`, `geom_near_segment`, `box_make` and `box_contains` to follow the C, the `geom_near_segment` early-out included, so that slipway and hover decisions match.
23. As a Phase 3 implementer, I want the C enum constants and header macros (`SIDE_POSITIVE`, `FRAME_ZONE_BYTES`, `SLOT_TALLY_KINDS_MAX`, `ARENA_HEAD_BYTES`, …) as module fields with the C names, so that a C expression using them copies unchanged.
24. As a Phase 3 implementer, I want each `sizeof` the C code uses to be a named entry of its module's `SIZEOF` table, checked against the C compiler's value, so that a wrong size cannot slip in.
25. As a Phase 3 implementer, I want `dispatch_context` to store and return the graphics, memory and input tables, so that the entity dispatch of later steps copies unchanged.
26. As a Phase 3 implementer, I want the step 3.1 note to tell me how to write a block, a handle cast, a `sizeof`, a C truth value and a zone string, so that I write them as every other step does.
27. As a plan 3 debugger, I want each Lua function of step 3.1 in the Lua file of the same name, in the C order, so that a grep for a C name lands on its copy.
28. As a plan 3 debugger, I want the Lua byte counts to equal the C ones, so that a `debug_state()` difference in a zone line points at a real divergence.
29. As a maintainer, I want `make test` to fail when a Lua file under `game/` reads a global that is not on a short fixed list, naming file, line and global, so that a typo in a name is caught before any run.
30. As a maintainer, I want `make test` to fail when a Lua file writes any global except `debug_state` in `game/main.lua`, so that modules stay self-contained.
31. As a maintainer, I want the global check to read the compiled bytecode rather than parse the source, so that it cannot be fooled by scoping it does not understand.
32. As a maintainer, I want `make test` to fail when a module under `game/` cannot be required with its `require`s by a plain LuaJIT, naming the module and the error, so that a broken `require` never waits for a game run.
33. As a maintainer, I want `make test` to fail when a ported data table differs from the C one, naming the module, the table, the first differing entries and both values, so that a typo in a copied table is caught at once.
34. As a maintainer, I want the data check to say when a table the C reference prints is missing from its Lua module, so that a forgotten export is caught.
35. As a maintainer, I want `make checklist STEP=<id>` to fail when a table the checklist reports as ported is not compared by the data check, so that no step forgets to register a table.
36. As a later step's implementer, I want to add my tables by writing one C reference unit for my C file and naming it once in the Makefile, so that I extend the check without a new target.
37. As a later step's implementer, I want my new Lua modules picked up by the load check and the global check without editing either script, so that extending them costs nothing.
38. As a later step's implementer, I want a function-pointer field to compare as "a function" and a pointer to another registered table to compare as a reference to that table by name, so that class and backend tables can be checked.
39. As a later step's implementer, I want the step 3.1 note to give each check's cost per file and per table, and what is left of the 3 %, so that I know whether my step fits.
40. As a developer reading a red check, I want at most a few differences printed per table, with a count of the rest, so that the output stays readable.
41. As a plan-pilot run, I want the step's timing measured with the design's protocol and recorded in the step note, so that the 3 % gate is applied, not assumed.
42. As a maintainer, I want the new checks' own tests to run over small fixture trees, so that the checks are known to fail when they should.
43. As a player, I want the engine's arithmetic to be the C arithmetic, so that the Lua game plays exactly like the original.

## Implementation Decisions

### Which C files, which Lua files

- The checklist is authoritative: step 3.1 owns `src/engine/` `zone`, `zone_sizes.h`, `allocator`, `frame_zone`, `game_zone`, `value_list`, `pointer_list`, `slot_tally`, `slot_group`, `vec`, `geom`, `collect`, `dispatch_context`, and `src/p8/str`.
- Lua files: `game/engine/{zone, zone_sizes, allocator, frame_zone, game_zone, value_list, pointer_list, slot_tally, slot_group, vec, geom, collect, dispatch_context}.lua` and `game/p8/str.lua`. Header inline functions (`vec.h`, `box_contains` in `geom.h`) open their module, before the `.c` functions, per H-2.1-1.
- `zone_sizes.h` has no `.c` and is "no-lua" in the checklist, but its macros are needed by `frame_zone` and `game_zone` (and later `ui_zone`, `particle_zone`, `state_format`): it gets its own `zone_sizes.lua` holding them as module fields. A header with no `.c` whose macros Lua needs gets a Lua file of its own name: this is recorded as a convention.
- Module requires follow the C includes: `p8.str` requires `engine.allocator` (the reason for its move to 3.1); `engine.geom` and `engine.vec` require `p8.fixed`; `engine.collect` requires `engine.pointer_list`, `engine.allocator` and `p8.str`.

### Conventions added by this step (recorded in the step 3.1 note; later steps follow and extend)

- **The build Lua copies.** Lua computes what the 64-bit PC build computes, the build that recorded the replays: `sizeof(void *)` is 8, a `zone_cell_t` is 8 bytes, `ZONE_BYTES_BY_WIDTH(bytes_64, bytes_32)` returns `bytes_64` (keeping the C compile-time condition on `bytes_32` as an `assert` with a message), and the ASan-only paths are not copied: `BUMP_ZONE_HALVES` is 1 and `POISON`/`UNPOISON` lines are dropped, since they compile to nothing in that build.
- **Blocks.** An allocation returns a new, empty Lua table, the block. The caller writes into it what the C writes into the bytes: struct fields by name, or array elements from index 0. Zones never read a block's contents. The byte count, the cell rounding, the capacity test and the `zone … full` message are the C ones, through `p8.fatal` (H-2.1-3).
- **Zone storage.** `BUMP_ZONE_STORAGE`, `FLIP_ZONE_STORAGE` and `SLOT_ZONE_STORAGE` become functions returning a new table standing for the static array (never read), `SLOT_ZONE_STORAGE` returning `cells, taken` with `taken` a real 0-indexed array of zeros. `ZONE_CELLS`, `ZONE_BYTES`, `SLOT_ZONE_BYTES`, `ARENA_LIST_BYTES` become module functions with the C names, like `P8_FIXED_LIT` in `p8.fixed`.
- **Pointer arithmetic on slots.** A slot zone table has two Lua-only fields documented in its module: `blocks`, the block currently handed out from each slot index (what `slot_at` returns and `slot_tally_max_bytes` reads), and `index_of`, mapping a block to its slot index. A slot allocation makes a new block for the slot and drops the previous block's mapping; release reads `index_of`, so a block never handed out by this zone, or whose slot was handed out again since, fails with the C `release of a pointer outside the zone` message, and a second release of the same block fails with the C `release of a slot already released` message.
- **Growth and copies.** `memcpy` of a whole block (`bump_grow`, `flip_zone_handle_grow`, `arena_spill_grow`) is one Lua-only local helper of `allocator.lua` copying every key of the old block into the new one. `memcpy` of one element of unknown type (`value_list_add`, `value_list_copy`) is one Lua-only local helper of `value_list.lua` copying every key of a table value one level deep, or storing a non-table value as is. These two are the only generic copies in the port: they stand for a byte copy of unknown type, unlike 2.1's typed struct copies. An inline sub-struct is shared by such a copy; this is only right where the C says the sub-struct is never changed after the copy (as `trade_route_t`'s id lists), and a later step meeting a mutable one copies it itself before the add, as the C comment in `trade.c` already does.
- **First-member casts.** A C struct whose first member is reached by a cast (`arena_t` and its `handle`, `flip_zone_allocator_t`, `slot_group_handle_t`, and the block an arena lives in) is one Lua table holding the fields of both structs: the cast is the identity. Where field names coincide they denote the same object (`arena_t.bump`, the inline zone, and `allocator_t.bump`, the pointer to it). The C "is this an arena" asserts (`arena->bump == &a->bump`) become a test that the handle carries the arena's `from` field, with the C message.
- **Struct values and statics.** A struct returned or passed by value (`allocator_t` from `allocator_from_bump_zone`, `frame_zone_mark_t`, `vec_t`, `box_t` by value in `box_contains`) is a new table (H-2.1-1). A static struct variable or an inline array of structs (`static bump_zone_t frame`, `slot_group_t.zones`) is a table, or a 0-indexed array of tables, created with every field at its C zero value (0, or `nil` for a pointer) by the enclosing module or constructor.
- **`sizeof`.** Every `sizeof` the C code of a module uses becomes an entry of that module's `M.SIZEOF` table, keyed by the C type as written (`SIZEOF["void *"]`, `SIZEOF.uint16_t`, `SIZEOF.arena_t`, `SIZEOF.box_t`), holding the 64-bit value, and the data check compares each entry with the C compiler's `sizeof`. No size is written as a bare number in code.
- **Constants.** Header macros with a numeric value and enum constants that Lua needs are module fields with the C names and values (`M.SIDE_POSITIVE = 1`, `M.FRAME_ZONE_BYTES = 20256`, `M.ENTITY_TABLE_BYTES` computed as in C), and the data check compares them. A `.c`-local macro is a module local and is not compared.
- **Compile-time checks.** A C `typedef char x[cond ? 1 : -1]` becomes an `assert(cond, "<what it guards>")` at module load.
- **C truth values** (unless H-2.1-8 says 2.1 settled it). A C `int` used as a truth value stays a number: a function returning one returns 0 or 1, a C comparison producing an `int` value is written `(a < b) and 1 or 0`, a test `if (x)` on an `int` is written `if x ~= 0`, `!x` is `x == 0`, and a pointer test `if (p)` is `if p ~= nil`. Lua `true`/`false` never stand for a C `int`.
- **Zone strings.** A C `char *` built in a zone is a Lua string: the function performs the C allocation for the C byte count (length plus the terminator) and returns the Lua string; the block is dropped. Releasing such a string is a no-op on a bump zone, as in C, and fails on a slot zone with the C "outside the zone" message (no game code does it). `strcmp(a, b) == 0` is `a == b`.
- **Out-parameters and callbacks.** Per step 1.2's note: `collect_format_signed` returns `result, out_color`; `pointer_list_iter_next(iter)` returns `1, e` or `0`; a `map_pointer_list_fn_t` callback returns `found, value`. Callback parameters keep the C order (`e, ctx`).

### Behaviour notes on the modules

- `zone.lua`: `bump_half` returns the half's offset (always 0, one half); `bump_zone_alloc`, `flip_zone_alloc`, `slot_zone_alloc` return new blocks; `flip_zone_end` keeps its C role in the arithmetic of `top`. All asserts keep their C messages.
- `allocator.lua`: the `allocator_t` table holds `alloc`, `grow`, `release`, `bump`, `slot`; `allocator_alloc`, `allocator_grow`, `allocator_release` dispatch through them with the C asserts. `arena_take` asks `from` for a block of `size` bytes, turns that block into the arena (handle fields, inline bump zone made by `bump_zone_init_single` with the C capacity minus the header cells, `from`), and returns it. `ARENA_HEAD_BYTES` is `ZONE_BYTES(SIZEOF.arena_t)`.
- `frame_zone.lua`, `game_zone.lua`: module locals stand for the C statics, initialised lazily by the copied `*_init_once` functions; `frame_zone()` and `game_zone()` return the same handle table every call, as the C returns the address of a static.
- `slot_group.lua`, `slot_tally.lua`: `SLOT_GROUP(kinds, n)` builds the group with `ready = 0` and its inline arrays sized `SLOT_TALLY_KINDS_MAX`; the forward-declared statics `slot_alloc` and `slot_release` follow H-2.1-1's forward-declaration rule; `slot_tally_init`'s `memset` sets each field explicitly.
- `value_list.lua`, `pointer_list.lua`: `items` is a block indexed from 0; growth doubles from the C initial capacity through `allocator_grow` with the C byte counts (`capacity * element_size`, `capacity * SIZEOF["void *"]`).
- `str.lua`: `p8_tostr_fixed` copies the C unsigned arithmetic (magnitude of the raw value, `ip`, `p = (mag & 0xFFFF) * 625`, round half to even on `r`, carry at 10000), formats `%s%d.%04d`, strips trailing zeros then a bare dot, as the C does; `p8_sub` clamps `i` and `j` as the C and returns `string.sub`.
- `geom.lua`, `vec.lua`: every operation goes through `p8.fixed` functions with their C names; `geom_beyond_span`'s plain integer `±0x2000` stays plain arithmetic, as the C is only reached inside ±32.
- `dispatch_context.lua`: three module locals, `nil` until set.

### Check 1 — undeclared globals (added to `lua-guard`)

- `tools/lua_guard.lua` already compiles every file it is given. For each file that compiles, it walks the compiled chunk and every nested prototype with LuaJIT's `jit.util` (`funcbc`, `funck`, `funcinfo`), and reports each `GGET` of a name off the read list and each `GSET`, as `file:line: global <name> is read but not declared` or `… is written`.
- Read list, fixed in the script: `assert`, `error`, `getmetatable`, `ipairs`, `next`, `pairs`, `pcall`, `rawequal`, `rawget`, `rawset`, `require`, `select`, `setmetatable`, `tonumber`, `tostring`, `type`, `unpack`, `xpcall`, `math`, `string`, `table`, `love`. Not on it: `bit` and `jit` (modules `require` them, as `p8.fixed` does), `print`, `io`, `os`, `debug`, `load`, `loadstring`, `loadfile`, `dofile`, `collectgarbage`, `module`, `setfenv`, `getfenv`, `_G`.
- Write list: `debug_state` in `game/main.lua` only (the fake reads that global, `tools/fake_love/host.c`; step 3.15 writes it). Defining `love.load` is a read of `love`, not a global write, so the six callbacks need no exception.
- Over the whole tree: the files are the Makefile's `LUA_GUARD_FILES`, as today; a later step's new directory level, if any, is added to that wildcard, not to a new target.
- Its tests join `tests/test_lua_guard.c`: an undeclared read, an undeclared write, a read inside a nested function, an allowed read, a local shadowing a global name, a `debug_state` write in `main.lua` (accepted) and elsewhere (refused).

### Checks 2 and 3 — load and data (`lua-test-modules`)

- **One target, one process.** `make lua-test-modules`, a prerequisite of `test` placed with the other Lua checks, with a `## ` help line of at most 20 words, runs `luajit tests/lua/test_modules.lua game <reference file> $(LUA_GUARD_FILES)`. It has no output file, so it runs on every `make test`, like `lua-test-fixed`.
- **Load check.** With `package.path` set to the game directory only and `package.cpath` empty, the script requires every listed file's module (path under `game/` with `/` turned into `.`), each under `pcall`, in sorted order, skipping `main.lua` and `conf.lua` (they define LÖVE callbacks and run under the fake in 3.15). No `love` global exists during it (H-2.1-5). Each failure is reported as `load <module>: <error>`. Step 3.3's cycle exception, if needed, becomes a skip list in the script recorded by 3.3's note, not a new target.
- **C reference.** A small C binary, `tools/table_reference.c` plus one unit per C source file that holds registered tables, built as `tmp/table_reference`, writes the reference file `tmp/table_reference.txt` as a make prerequisite (rebuilt only when a C source or unit changes, like `tmp/fixed_reference.bin`). A unit `#include`s its C source file so that it can read the file's `static` tables, and exports one function printing them; the binary links the objects of `$(ENGINE_SRCS)` minus the sources its units include, so a later step adds a unit and one name in the Makefile, and nothing else. Header constants and `SIZEOF` entries are printed from a unit including only the headers.
- **Line format, both sides.** One line per scalar: `<module> <name><path> <value>`, e.g. `p8.palette P8_PALETTE_RGB[3][1] 135`, `engine.zone_sizes FRAME_ZONE_BYTES 20256`, `engine.allocator SIZEOF.arena_t <n>`. A path is `[i]` for an integer key and `.field` for a field (`["…"]` for a key that is not an identifier, as `SIZEOF["void *"]`). Values: integers in decimal, in the Lua representation (a `p8_fixed_t` as its signed raw value, an unsigned type non-negative), strings with `%q`, `function` for a non-NULL function pointer, `&<name>` for a pointer to another registered top-level table (the Lua side recognises those tables by identity), anything else nested printed in full where it is reached. A NULL pointer prints no line, matching an absent Lua field. Lua booleans print `true`/`false` and so never match a C value.
- **Comparison.** For each `(module, name)` the reference holds, the script reads the module field (or the module itself when its return value is that table), renders it, and compares the sorted line sets. It prints up to five differences per table (`C <value>, Lua <value>`, `missing in Lua`, `extra in Lua`) and the count of the rest, then one summary line; exit 1 on any load failure or difference.
- **Coverage.** Tables the checklist reports as ported but absent from the reference file fail `make checklist STEP=<id>` (`table <name> of <file> is ported but not compared by lua-test-modules`); `make checklist` gains the reference file as a prerequisite and `tests/test_checklist.c` a test. This keeps the cross-check outside `make test`.
- **What 3.1 registers.** Every table already ported: `p8_quarter_wave` (1.2) and the 59 tables of 2.1 (H-2.1-4); and 3.1's constants and `SIZEOF` entries. Step 3.1's own C files hold no file-level table (0 in the checklist).
- **Its own tests.** `tests/test_lua_modules.c`, in `UNIT_TESTS` as `test_lua_guard` is, runs the script over fixture trees written at run time: a module that fails to load, a broken `require`, a differing value, a missing table, an extra entry, a function field, a `&name` reference, a clean tree; it checks exit codes and messages. If the measured cost puts the step over budget, this test moves to `make checklist` beside `test_checklist`, which is outside `make test`.

### Cost and timing

- Each check's own wall time is measured on the tree at the end of the step (`lua-guard` before and after the new rule; `lua-test-modules`), and divided into a cost per Lua file (load and global walk) and per registered table and per reference line (data). The note projects these to the end of the plan (every C file's Lua module, 529 tables plus constants) and compares the projection with what is left of the 3 % after 1.2 and 2.1 (H-2.1-7).
- The step then runs the design's protocol (worktrees under `tmp/`, warm-up, 11 rounds rotating order, paired medians, ≤ 3 % against the step's "before" commit and against `8dff812`, 11 more rounds if needed) and records plain medians, paired medians, spreads, rounds, core count and the slowest new check's own time.
- If the projection exceeds what is left, the note says which part moves out under design rule 5 when its step comes (the large-table comparison first: step 3.3 alone brings 386 tables), so that no later step adds a target.

### Docs

- `docs/love-port/plan-2.md` § 6 gains `### Step 3.1`: the conventions above, the three checks and how a later step extends them (a module file is picked up by the wildcard; a table needs one C reference unit and its Makefile name; a new directory level goes into `LUA_GUARD_FILES`), the global read and write lists, the costs and the timing record.

### Questions

- No `guideline` agent could be spawned (no agent tool in this run). Every question met was settled from the plan, the design file, the 2.1 PRD and the code, and is recorded above: one `make test` target for load and data checks together (one process, the plan's "add no target" rule for later steps); the global check by bytecode instead of a source parser; 64-bit sizes and no ASan paths; `SIZEOF` tables and constants checked by the data check; the two generic copy helpers; first-member casts as one table; the truth-value convention; the coverage cross-check in `make checklist` rather than `make test`; no behaviour test for the engine modules (see Testing Decisions).

## Implementation Chunks

1. **The global rule.** The bytecode walk and the two lists in `lua-guard`, its cases in `tests/test_lua_guard.c`; green on `game/` as it stands.
2. **The load and data check.** `tests/lua/test_modules.lua`, the C reference binary with its units for `p8_quarter_wave` and every table 2.1 has ported (H-2.1-4, exporting any local table), the reference file rule, `lua-test-modules` in `make test`, `tests/test_lua_modules.c`, the checklist coverage cross-check and its test. Green on `game/` as it stands.
3. **Zones and the allocator.** The step note's conventions section; `zone_sizes`, `zone`, `allocator`, with their constants and `SIZEOF` entries registered in the reference.
4. **Zone singletons and slot groups.** `frame_zone`, `game_zone`, `slot_tally`, `slot_group`.
5. **Lists and strings.** `value_list`, `pointer_list`, `p8.str`, `collect`.
6. **Vectors, geometry, dispatch context.** `vec`, `geom` (with `SIDE_*` and `SIZEOF.box_t` registered), `dispatch_context`. `make checklist STEP=3.1` passes from here.
7. **Cost and record.** Per-file and per-table costs, the projection, the timing protocol, the step note.

## Testing Decisions

- A good test here checks external behaviour only: a check's verdict and message on given Lua files (the guard's, the load and data script's), and the equality of Lua data with the C compiler's data. No test looks at how a Lua function is written.
- Tested: the global rule through `tests/test_lua_guard.c` (prior art: that file's fixture-tree cases); the load and data script through `tests/test_lua_modules.c` (prior art: `tests/test_lua_guard.c`, `tests/test_checklist.c`); the coverage cross-check through `tests/test_checklist.c`; the engine modules through the three checks and `make checklist STEP=3.1` (presence, loading, no globals, constants and sizes equal to C).
- No behaviour test is written for the engine modules: the plan verifies each Phase 3 step with the checklist and the three checks, the e2e replays of plan 3 prove behaviour (`CLAUDE.md`: they are the source of truth), and the timing margin is mostly spent. A TDD cycle on a module goes red on the checklist row, the load check or a constant/size line, and green when the faithful copy is in place. Byte accounting is the only behaviour the replays cannot see (the end-state drops zone lines); it is protected by the faithful copy, the checked `SIZEOF` entries and constants, and plan 3's function-by-function comparison.
- The C side never changes: `src/` is the reference; the only new C is the reference binary, its units and the two tests.
- Prior art for the reference binary and its file: `tools/fixed_reference.c` and `tmp/fixed_reference.bin` read by `tests/lua/test_fixed.lua`.

## Out of Scope

- Step 3.2 onward: entity classes, ids, lists, the registry and every game module; their tables, constants and sizes are added to the checks by their own steps.
- `ui_zone` and `particle_zone` (steps 3.6 and 3.8), which build slot groups on this step's code.
- `debug_state()` and the state dump text (step 3.15).
- Running any e2e replay or `make love-e2e` (plan 3).
- Behaviour tests of the engine modules, and any redesign of the memory layer for Lua (plan 5).
- Modelling ASan poisoning or 32-bit sizes.
- Audio (out of scope for the whole port).

## Further Notes

- At the time of writing, `make checklist` reports step 3.1 at `27 files, 0/146 functions, 0/0 tables`; LuaJIT 2.1 is installed and the container has 4 cores. `jit.util` reports `GGET`/`GSET` with names and line numbers on `game/p8/fixed.lua` in about 2 ms.
- Two static helpers share a name across files (`slot_alloc` and `slot_release` in `allocator.c` and `slot_group.c`, `check_kind` in `slot_group.c` and `slot_tally.c`): each stays a `local function` in its own Lua file, as in C.
- A stale block in Lua keeps its old contents where the C memory would be reused or poisoned: Lua cannot poison, and plan 3 debugs with the C beside it.
