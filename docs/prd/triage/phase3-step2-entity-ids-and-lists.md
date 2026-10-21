---
title: "game/engine/ entity classes, ids, id lists, entity lists, exit queue"
description: "The Lua game has no entity header, entity ids, id or entity lists, or exit queue, so the registry and every game module of later steps have nothing to build on."
status: needs-triage
---

## Plan step (verbatim, docs/love-port/plan-2.md, Phase 3 — Engine and game)

The Phase 3 header paragraph:

> One step per group of related C files, in the order of the C include graph,
> leaves first, so that each step's modules only require modules already
> written; an include cycle is cut into consecutive steps, and if one of its
> files cannot load before its partners exist (2.1's conventions, extended by
> 3.3's note, say), the load check of that cycle runs at its last step. The checklist's file
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

> 3.2. `game/engine/` entity classes, ids, id lists, entity lists, exit queue.

## Hypotheses on step 3.1 (its note not written when this PRD was written)

Step 3.1's code is committed (`8cbdaff` and before): the 13 `game/engine/` modules and `game/p8/str.lua`, the global rule in `tools/lua_guard.lua`, `tests/lua/test_modules.lua` with `make lua-test-modules` in `make test`, the C reference binary (`tools/table_reference.c`, units in `tools/table_reference/`, `TABLE_REFERENCE_SOURCES` in the Makefile), the checklist's "ported but not compared" cross-check, and `make checklist` reporting step 3.1 at 146/146 functions. These are facts, and so are the mechanics they show: `LUA_GUARD_FILES` is a wildcard over three directory levels, so a new `game/engine/*.lua` file joins `lua-guard` and `lua-test-modules` with no Makefile edit; a constant joins the data check through one unit in `tools/table_reference/` and one source name in `TABLE_REFERENCE_SOURCES`.

Its last chunk (costs, timing, the `### Step 3.1` note) is still running. Everything below about that note is an assumption, labelled and referred to by its label. When the note lands differently, the implementer follows the note and records the difference in the step 3.2 note.

- **H-3.1-1 — build.** Lua copies the 64-bit PC build without ASan: `sizeof(void *)` 8, `POISON`/`UNPOISON` dropped.
- **H-3.1-2 — blocks.** An allocation returns a new, empty table, the block; the caller writes into it what the C writes into the bytes (fields by name, array elements from 0); zones never read a block; C address arithmetic stays as byte offsets nothing reads.
- **H-3.1-3 — copies.** The only generic copies are `copy_block` (in `allocator.lua`) and the one-element copy of `value_list.lua`; every other by-value struct copy is a constructor listing every field (2.1's rule).
- **H-3.1-4 — first-member casts.** A struct reached by casting its first member is one table holding both structs' fields; the cast is the identity. (For this step: a game object whose first member is an `entity_t` is one table holding the header fields and its own.)
- **H-3.1-5 — statics.** A static struct or inline array of structs is a table, or a 0-indexed array of tables, with every field at its C zero value (0, or `nil` for a pointer).
- **H-3.1-6 — `sizeof`.** Every `sizeof` a module uses is an entry of `M.SIZEOF`, keyed by the C type as written, 64-bit value, read through `local SIZEOF = M.SIZEOF`, never a bare number, compared with the C compiler by `lua-test-modules`.
- **H-3.1-7 — constants.** Numeric header macros and enum constants are module fields with the C names, computed as the C does, compared by `lua-test-modules`; a `.c`-local macro is a module local, not compared. Function-like macros are module functions with the C names.
- **H-3.1-8 — truth values.** As 2.1; `!x` is `(x == 0) and 1 or 0` as a value and `x == 0` as a test; a pointer test is `p ~= nil`; Lua booleans never stand for a C int.
- **H-3.1-9 — registering.** A unit `tools/table_reference/<dir>_<file>.c` including the header (or the `.c` for statics) prints each constant with `table_reference_int(module, value, "NAME")` or `"SIZEOF.type"`; its source is added to `TABLE_REFERENCE_SOURCES`.
- **H-3.1-10 — cost per file and per table.** The note gives a cost per Lua file (compile, global walk, load) and per registered line, small enough that this step's 5 files and about 40 constant lines cost a few milliseconds at most, under 1 % of `make test`.
- **H-3.1-11 — timing margin.** The note's projection to the end of the plan fits in what is left of the 3 %, or, if it does not, it names a part to move out under design rule 5 at a later step than 3.2 (the large-table comparison at 3.3). Step 3.2 then moves nothing out.
- **H-3.1-12 — note place.** The note is `### Step 3.1` in plan-2.md § 6, after `### Step 2.1`; step 3.2's note follows it.

## Problem Statement

Every game object of the C game starts with an `entity_t` header, is named across frames by an `entity_id_t` resolved through the entity id table, is kept in entity lists or id lists, and dies through the exit queue. The Lua game, after step 3.1, has zones, the allocator, lists, vectors and geometry, but none of these five entity foundations. Step 3.3 (registry, entity geometry, entities, bootstrap) and every game step after it require them; without them no later module can load, and plan 3 has no Lua function to put next to the C one when a replay diverges on an id, a list order or an exit.

## Solution

Six new Lua modules under `game/engine/`, faithful copies of their C files: `entity_id_type`, `entity_class`, `entity_id`, `entity_id_list`, `entity_list`, `exit_queue`. They follow 2.1's conventions and 3.1's (H-3.1-1 … H-3.1-9), and add the few this step needs (entity ids by value, a block carved into several arrays, the entity header's zero value). They join the three Phase 3 checks with no new target: the wildcard picks them up in `lua-guard` (compile, no `love.` misuse, no undeclared global) and `lua-test-modules` (load), and their constants and `SIZEOF` entries join the C reference. `make checklist STEP=3.2` passes: 10 files, 60 of 60 functions, 0 tables.

## User Stories

1. As the plan-3 debugger, I want `game/engine/entity_id.lua` beside `src/engine/entity_id.c`, same functions in the same order, so that I can step through an id lookup in both when a replay diverges.
2. As the plan-3 debugger, I want the entity id table to hand out indexes from the same free stack as the C, so that ids printed by `debug_state()` match the C dump.
3. As the plan-3 debugger, I want generations to start each new table at the same epoch as the C, so that a stale id resolves to `nil` exactly where the C resolves it to `NULL`.
4. As the plan-3 debugger, I want `next_generation` to wrap from 65535 to 1 as the C does, so that a long game never hands out generation 0.
5. As a step 3.3 implementer, I want `entity_id_table_init`, `entity_id_take` and `entity_id_release` in Lua, so that the registry can construct and destroy entities.
6. As a step 3.3 implementer, I want `entity_get` to return `nil` for the none id, a free slot, a stale generation or no table, so that the registry's checks read as the C.
7. As a game step implementer, I want `entity_id_link` to return the none id for `nil` and assert on an entity without a slot, with the C message, so that a broken link fails at once in Lua too.
8. As a game step implementer, I want `entity_id_of` to return the none id for an entity of a previous table, so that a reloaded game frees nothing it does not own.
9. As a game step implementer, I want `entity_id_live` and `entity_id_max` with the C values, so that the state dump's live and peak counts match.
10. As a game step implementer, I want the table-full error to carry the C text `entity table full, limit 512` through `p8.fatal`, so that a runaway game fails the same way in both.
11. As a game step implementer, I want `entity_id_equal` and `entity_id_is_none` in `engine.entity_id_type`, so that id comparisons never compare table identity.
12. As a game step implementer, I want `ENTITY_ID_NONE` as a module field with the C values, so that game code can name the "no link" id.
13. As a game step implementer, I want every entity id handed to me to be a table I may keep, so that a later change elsewhere never changes my copy.
14. As a game step implementer, I want `entity_init_header` to set every header field to its C zero value and the class, without touching the object's other fields, so that a planet table keeps its own fields.
15. As a game step implementer, I want `entity_has`, `entity_has_pos`, `entity_has_map_pos` to return 1 or 0, so that C truth tests copy as `~= 0`.
16. As a game step implementer, I want `entity_pos` and `entity_map_pos` to return the zero vector when the bit is clear, as new tables, so that a caller changing the result never moves the entity.
17. As a game step implementer, I want `entity_set_pos` to store its own copy of the position, so that the planet bob (which changes `self.pos.y` in place) never changes the caller's vector.
18. As a game step implementer, I want `entity_clear_pos` and `entity_clear_map_pos` to clear only their bit, with the C 16-bit mask, so that `has` keeps the C value.
19. As a game step implementer, I want `mouse_pos_has_world`, `mouse_pos_has_screen`, `mouse_pos_world`, `mouse_pos_screen` with the C results, so that hit tests and drags port as written.
20. As a game step implementer, I want the state, tag and lineage enums, `ENTITY_HAS_*` and `MOUSE_POS_HAS_*` bits as module fields with the C values, so that archetypes of later steps use the C names.
21. As a game step implementer, I want `TAG(t)` and `LINEAGE(l)` as module functions, so that archetype tag and lineage masks are computed as in C.
22. As a game step implementer, I want the shapes of `entity_t`, `entity_class_t` (with `states` indexed from 0), `down_t`, `mouse_pos_t` and `entity_kind_t` written in `entity_class.lua`'s top comment, so that later modules build them with the C field names.
23. As a game step implementer, I want `entity_list_*` with the C growth (initial capacity 4, doubling through `allocator_grow` with the C byte counts), so that zone usage matches the C.
24. As a game step implementer, I want `entity_list_init_reserve` with capacity 0 to allocate nothing, as in C, so that empty reserved lists cost nothing.
25. As a game step implementer, I want `entity_list_del` to remove the first match and shift the rest, so that list order matches the C.
26. As a game step implementer, I want `entity_list_has` to return 0 for a `nil` list, and `entity_list_concat` to assert on a `nil` list with the C message, so that the asymmetry survives.
27. As a game step implementer, I want `entity_list_filter` and `entity_list_find_first` to call the predicate as `predicate(e, ctx)` and test its result with `~= 0`, so that predicates of later steps return C ints.
28. As a game step implementer, I want `entity_list_find_first` to stop asking after the first match, so that predicates with effects behave as in C.
29. As a game step implementer, I want `entity_list_iter_next(iter, out)` to return `1, e` or `0, out` and to survive a removal during the walk the way the C does, so that loops that delete while iterating visit the same entities.
30. As a game step implementer, I want `entity_id_list_*` with the same shapes and growth, holding ids by value, so that id lists of trade routes and panels port as written.
31. As a game step implementer, I want `entity_id_list_at` and `entity_id_list_iter_next` to resolve each id and assert on a dead one with the C message, so that a dead id in a game list fails where the C fails.
32. As a game step implementer, I want `entity_id_list_filter` to add the id at the iterator's index, not the resolved entity's current id, so that the result matches the C even when they differ.
33. As a game step implementer, I want `exit_queue_put`, `exit_queue_put_arena`, `exit_queue_count`, `exit_queue_max` and `exit_queue_flush`, so that entities and arenas die at the end of the frame as in C.
34. As the plan-3 debugger, I want `exit_queue_flush` to free the id slot before calling the class's `release`, so that no slot points at a returned block, as the C comment requires.
35. As the plan-3 debugger, I want the exit-queue-full error to carry `exit queue full, limit 315`, so that the failure reads the same.
36. As a maintainer, I want every constant and `SIZEOF` entry of this step compared with the C compiler by `lua-test-modules`, so that a typo in a bit or an enum fails `make test`.
37. As a maintainer, I want this step to add no `make test` target, so that the loop stays as fast as the plan allows.
38. As a maintainer, I want `make checklist STEP=3.2` green, so that the step's coverage is proven mechanically.
39. As a maintainer, I want each new module to require only modules already written, following the C includes, so that the load check passes with no cycle exception.
40. As a maintainer, I want the step's conventions and its cost and timing in a `### Step 3.2` note, so that step 3.3 builds on recorded decisions.

## Implementation Decisions

### Which C files, which Lua files

- The checklist is authoritative: step 3.2 owns `src/engine/entity_class.h`, `entity_id_type.h`, `entity_id.{c,h}`, `entity_id_list.{c,h}`, `entity_list.{c,h}`, `exit_queue.{c,h}`: 10 files, 60 functions, 0 file-level tables.
- Lua files: `game/engine/{entity_id_type, entity_class, entity_id, entity_id_list, entity_list, exit_queue}.lua`. The two header-only files get their own modules (the checklist expects them, and `entity_id_type` is included alone by game headers).
- Requires follow the C includes, with no cycle: `entity_id_type` requires nothing; `entity_class` requires `p8.fixed`, `engine.vec`, `engine.entity_id_type` (it includes `geom.h` for the `box_t` type alone, so `engine.geom` is not required); `entity_id` requires `engine.allocator`, `engine.zone_sizes`, `engine.entity_id_type`, `p8.fatal`; `entity_id_list` requires `engine.allocator`, `engine.entity_id_type`, `engine.entity_id`; `entity_list` requires `engine.allocator`; `exit_queue` requires `engine.allocator`, `engine.entity_id`, `p8.fatal`. A require used by no line is not written: `entity_class.h` included by a `.c` for types only gives no require.
- `slipway_class()` and `probe_class()`, declared in `entity_class.h` but defined in `src/game/`, belong to their own files' steps; `entity_class.lua` does not write them and its top comment names them.

### Conventions added by this step (recorded in the step 3.2 note)

- **Entity ids by value.** An `entity_id_t` is `{ index, generation }`. Every place the C creates, returns or stores one by value builds a new table listing both fields (2.1's struct rule): `entity_id_take` builds it, `entity_id_of` and `entity_id_link` return a new copy, `entity_id_list_add` stores a copy of its argument, the none id is a new copy of `M.ENTITY_ID_NONE` at each use (`{ index = NONE.index, generation = NONE.generation }`, as `draw_cost.FREE`). `e.id` is a table no code mutates field by field (no C line writes `.index` or `.generation` of an id). Ids are compared only through `entity_id_equal`, never by identity.
- **The entity header.** An `entity_t` is a table with the C field names (`class`, `state`, `state_time`, `has`, `layer`, `pos`, `map_pos`, `hit_box`, `infraspace`, `shake`, `fold_anchor`, `is_dead`, `is_hovered`, `id`); a game object whose first member is the header is that same table (H-3.1-4). `entity_init_header(self, cls)` copies `*self = (entity_t){.class = cls}`: it sets `class` to `cls` and each other header field to its C zero value (numbers 0; `pos`, `map_pos` new zero vectors; `hit_box` a new box of four zeros; `id` a new none id), and leaves non-header keys alone, since the C assignment writes the header bytes only.
- **Inline vectors are owned.** `entity_set_pos` and `entity_set_map_pos` store a new `vec_make` copy; `entity_pos`, `entity_map_pos`, `mouse_pos_world`, `mouse_pos_screen` return a new copy (or the new zero vector). Reason: game code changes `self.pos.y` in place (`planet.c`), so a shared table would move the caller's vector.
- **One block carved into several arrays.** `entity_id_table_init` asks the zone for `ENTITY_TABLE_BYTES` (the C byte count and capacity test); the block is `entities` (indexed 0 … 511, `nil` for a free slot); `generations` and `free_indexes` are new 0-indexed tables held by the table state, standing for the C offsets into the same block, which nothing reads. A later step carving a block the same way follows this.
- **16-bit masks.** `has &= (uint16_t)~BIT` is `bit.band(has, bit.band(bit.bnot(BIT), 0xFFFF))`; `has |= BIT` is `bit.bor`; `TAG(t)` and `LINEAGE(l)` are `bit.lshift(1, t)` (below 2^31 for every enum value).
- **Statics.** `entity_id.c`'s `g_table` is a module-local table with `entities = nil`, `generations = nil`, `free_indexes = nil`, `free_count = 0`; `entity_id_table_forget` resets those four fields; `g_max_live` and `g_highest_generation` are module locals at 0. `exit_queue.c`'s `g_queue` is a 0-indexed array of `EXIT_QUEUE_CAP` tables with `entity = nil`, `arena = nil`, built at load (H-3.1-5); `put(entry)` copies the two fields into the slot, as the by-value assignment does.
- **Iterators and callbacks.** As `pointer_list`: `*_all` returns a new iterator table with the C field names (`entity_list`/`list`, `index`, `last_count`); `*_iter_next(iter, out)` returns `1, e` or `0, out`. Predicates are called `predicate(e, ctx)` and their C int tested with `~= 0`. C parameter names stay, `qk` and `ow` included (faithful copy; plan 5 renames).
- **Constants and sizes registered.** New units `tools/table_reference/engine_entity_class.c` (every `state_t`, `tag_t`, `lineage_t` constant including `STATE_COUNT` and `TAG_COUNT`, the nine `ENTITY_HAS_*`, the two `MOUSE_POS_HAS_*`), `engine_entity_id_type.c` (`ENTITY_ID_NONE.index`, `ENTITY_ID_NONE.generation`), `engine_entity_id_list.c` (`SIZEOF.entity_id_t`), `engine_entity_list.c` (`SIZEOF["entity_t *"]`), `engine_exit_queue.c` (`EXIT_QUEUE_CAP`); their headers join `TABLE_REFERENCE_SOURCES` (H-3.1-9). `ENTITY_TABLE_CAP` and `ENTITY_TABLE_BYTES` stay in `engine.zone_sizes`, already registered. `.c`-local macros (`ENTITY_ID_LIST_INITIAL_CAPACITY`, `ENTITY_LIST_INITIAL_CAPACITY`) are module locals, not compared.

### Behaviour notes on the modules

- `entity_id.lua`: `entity_id_take` asserts with the C messages, fails through `fatal.p8_fatal(string.format("entity table full, limit %d", ENTITY_TABLE_CAP))`, pops `free_indexes[free_count - 1]`, stores `e`, builds the id, asserts generation ≠ 0, writes `e.id` and updates `g_max_live`. `slot_of` returns `-1` unless `entity_get(e.id) == e` (identity of the entity table, which stands for the C pointer). `entity_get` checks `entities ~= nil`, `index >= ENTITY_TABLE_CAP`, the generation, in the C order.
- `entity_id_list.lua`, `entity_list.lua`: `items` is a block indexed from 0, grown through `allocator_grow` with `capacity * SIZEOF.entity_id_t` and `capacity * SIZEOF["entity_t *"]`; `entity_list_concat` uses `entity_list_init` (no reserve) and `entity_id_list_concat` uses `init_reserve`, as their C do. Every assert keeps its C message.
- `exit_queue.lua`: `exit_queue_flush` walks `0 … g_count - 1` with the bound read once as the C `for` does (the body changes neither); an arena entry calls `allocator.arena_release`; an entity entry reads `e.class.release`, calls `entity_id.entity_id_release(e)` first, then `release(e)` if non-`nil`; `g_count` back to 0. No C line in the body can put into the queue during a flush, so the numeric `for` is faithful.

### Checks (no new target)

- `lua-guard` and `lua-test-modules` pick the six files up through `LUA_GUARD_FILES`; only `TABLE_REFERENCE_SOURCES` changes in the Makefile. No skip list: no cycle in this step.
- `make checklist STEP=3.2` passes (60/60, 0/0) at the end of chunk 3.

### Cost and timing

- Measured at the end: the own wall time of `lua-guard` and `lua-test-modules` before and after the step, compared with H-3.1-10's per-file and per-line costs; any gap over 2× is explained in the note.
- The design's protocol (rule 4) is run once at the end against the step's "before" commit and `8dff812`, 11 rounds (22 if needed), and recorded as step 2.1's note does. The step adds load cost to a `make test` check, which rule 4's "every step that adds one measures it" covers; the run is a few minutes of agent time.
- If the gate fails, or H-3.1-11 turns out false and 3.1's note assigns a move-out to 3.2, the step applies what the note says (design rule 5) and records it; no new `make test` target is ever added.

### Docs

- `docs/love-port/plan-2.md` § 6 gains `### Step 3.2` after `### Step 3.1` (H-3.1-12): the conventions above, the five reference units, `make checklist STEP=3.2` counts, the costs and the timing record, and every place where 3.1's note differed from H-3.1-k.

### Questions

- No Agent tool was available in this run, so no `guideline` agent was spawned. Each question met was settled from the plan, `design.md`, the 3.1 PRD, the conventions scratch and the code, and is recorded above: ids copied by constructor at every by-value site; inline vectors copied in and out (because `planet.c` mutates `self.pos.y`); `entity_init_header` writes header fields only; the carved block's first array is the block; no behaviour test (below); the full timing protocol run once at step end.

## Implementation Chunks

1. **Id type and entity class.** `entity_id_type.lua` (`ENTITY_ID_NONE`, `entity_id_equal`, `entity_id_is_none`), `entity_class.lua` (enums, bits, `TAG`, `LINEAGE`, the 14 inline functions, struct shapes in the top comment), units `engine_entity_id_type.c` and `engine_entity_class.c` in `TABLE_REFERENCE_SOURCES`. Green: `lua-guard`, `lua-test-modules`, the checklist rows of those two files.
2. **Id table and id lists.** `entity_id.lua` (11 functions), `entity_id_list.lua` (14 functions), unit `engine_entity_id_list.c`.
3. **Entity lists and exit queue.** `entity_list.lua` (13 functions), `exit_queue.lua` (6 functions), units `engine_entity_list.c` and `engine_exit_queue.c`. `make checklist STEP=3.2` passes from here.
4. **Cost and record.** Own times of the two checks, the timing protocol, the `### Step 3.2` note.

## Testing Decisions

- A good test checks external behaviour only: a check's verdict on the Lua files, and the equality of Lua constants and sizes with the C compiler's. No test looks at how a Lua function is written.
- Tested: the six modules through `make checklist STEP=3.2` (presence of every file and function), `lua-guard` (compile, no undeclared global, no `love.` use), `lua-test-modules` (load under plain LuaJIT, every registered constant and `SIZEOF` entry equal to C). A TDD cycle goes red on a checklist row, a load failure or a missing/differing reference line, and green when the faithful copy is in place.
- No behaviour test for these modules, as step 3.1 decided for its own: the plan verifies Phase 3 steps by the checklist and the three checks, the e2e replays of plan 3 are the source of truth (`CLAUDE.md`), and the timing margin is mostly spent (step 1.2's and 2.1's notes). The id table's generations and free stack are protected by the faithful copy and by plan 3's function-by-function comparison against `entity_id.c`.
- No change to `tests/lua/test_modules.lua`, `tools/lua_guard.lua` or their C tests: the step only feeds them.
- Prior art: 3.1's units `tools/table_reference/engine_geom.c` (enum constants and a `SIZEOF`) and `engine_pointer_list.c` (a pointer `SIZEOF`); `game/engine/pointer_list.lua` for the list and iterator shape; `game/p8/draw_cost.lua` for a by-value constant struct (`FREE`).

## Out of Scope

- Step 3.3 onward: the registry (`entity_registry_init`, `entity_construct`), entity geometry, entities, bootstrap, and every class and archetype table (`slipway_class`, `probe_class` and the rest).
- `debug_state()` and the state dump (step 3.15), which will read `entity_id_live`, `entity_id_max`, `exit_queue_count` and `exit_queue_max`.
- Running any e2e replay or `make love-e2e` (plan 3).
- Behaviour tests of these modules, and any redesign of ids or lists for Lua (plan 5).
- Changes to the three checks themselves, or a new `make test` target.
- Audio (out of scope for the whole port).

## Further Notes

- At the time of writing, `make checklist` reports step 3.2 at `10 files, 0/60 functions, 0/0 tables`; 4 cores.
- Two static helpers named `grow` / `entity_list_grow` and `put` stay `local function` in their own files, as in C.
- `entity_id.c` uses `snprintf` into a 64-byte buffer; both messages fit, so `string.format` gives the same text.
