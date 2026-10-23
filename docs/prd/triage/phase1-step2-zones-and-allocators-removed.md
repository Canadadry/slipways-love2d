---
title: "Zones and allocators removed"
description: "The Lua game still copies the C's memory zones, slot pools, arenas and byte counts, a layer that allocates nothing real, only does bookkeeping, and makes every constructor and list take a zone it never needs."
status: needs-triage
---

## Problem Statement

The Lua game is a line-by-line copy of a C program that ran inside a fixed
memory budget. That budget layer came over whole: bump, flip and slot zones
(`engine.zone`), their sizes (`engine.zone_sizes`), the per-frame and
per-game zones (`engine.frame_zone`, `engine.game_zone` with its load zone),
the allocator handles and arenas (`engine.allocator`), the slot groups and
their tallies (`engine.slot_group`, `engine.slot_tally`), and three
game-side pools built on them (`game.ui_zone`, `game.particle_zone`,
`game.grab_arena`). In Lua none of it holds memory: every "allocation"
returns a new empty table, and the zones only count bytes nobody uses, check
capacities the game never reaches, and raise `zone ... full`.

The cost is everywhere. About fifty modules require a zone module. Every
list init, every constructor and about forty functions take a `zone`
argument that is passed down and never used for anything real. Around
thirty-five modules carry an `M.SIZEOF` table of C struct sizes whose only
job is to feed those byte counts. Objects carry fields that exist only for
the bookkeeping (`slot`, `layout_row_slot`, `grab_arena`, a list's `zone`),
and the game loop flips, releases, marks and rewinds zones at precise
points. The state dump writes `zone`, `budget` and `cap` lines about pools
that are not real. A developer reading or changing any constructor first has
to understand which zone it must come from and why, which is exactly the
C-ism phase 0 decided to remove entirely, with no Lua stand-in.

## Solution

Every zone, pool, arena and allocator goes, and so does every `SIZEOF`
entry. A new object is a Lua table built where it is needed; a list is
initialized without a zone; a function that took a zone only to pass it on
loses that parameter. The game-side pools go first (UI, particles, grab
arena), then the frame and game zones, then the zone parameter of the engine
containers and helpers, then of the game modules, and finally the allocator
and zone modules themselves. The state dump stops writing `zone`, `budget`
and `cap` lines. The game plays exactly as before: every e2e capture hash and
every end state is unchanged, and `make e2e` stays under 9 s.

When the step is done:
`grep -rE 'require\("engine\.(zone|zone_sizes|frame_zone|game_zone|allocator|slot_group|slot_tally)"\)|SIZEOF' game/`
finds nothing, those seven engine modules and the three game-side pools are
gone, and a state dump holds no line starting with `zone`, `budget` or `cap`.

## User Stories

1. As a developer reading a constructor, I want it to build a plain Lua table, so that I don't have to know which zone it once came from.
2. As a developer, I want list init functions to take no zone, so that creating a list is one call with the list (and its reserve) only.
3. As a developer, I want functions that only passed a zone down (layout rows and tooltips, trade route copies, needs, random walks, weighted pickers, boxes, formatted strings) to lose that parameter, so that their signatures state what they really need.
4. As a developer, I want no `M.SIZEOF` table anywhere in `game/`, so that no C struct size survives in the Lua.
5. As a developer, I want the few sizes that are real limits (the save file magic length, the save node planet id length, the bindings error text length, the save buffer size, the loading text length, the draw palette length) kept as plainly named constants with the same values, so that the code that reads them behaves the same.
6. As a developer, I want the UI pool gone, so that a button, a modal backdrop, a message box, a toast, a row or an ephemerals list is a table and not a slot taken from a group.
7. As a developer, I want the particle pool gone, so that a particle, a particle list, a burst and a transit dot are plain tables, with no "slot kind" to choose.
8. As a developer, I want the grab arena gone, so that a planet, slipway, probe or wormhole under construction is built without opening, closing, spilling, flushing or giving back an arena.
9. As a developer, I want the frame zone gone, so that no code marks, rewinds or resets per-frame memory.
10. As a developer, I want the game zone and its load zone gone, so that starting, quitting and restarting a game do not flip, release or reset memory generations.
11. As a developer, I want object fields that existed only for the bookkeeping (a slot, a row slot, an arena, a list's zone) removed, so that an object holds only game state.
12. As a developer, I want a release function that only gave memory back to become empty or disappear from its class, so that release means "unhook from the game", nothing more.
13. As a developer, I want the exit queue to hold entities only, so that it no longer carries arena entries.
14. As a developer, I want the pool capacities (15 buttons, 48 toasts, 887 particles and so on) gone with no replacement limit, so that only game rules limit counts.
15. As a developer, I want the save format's 255 nodes and slipways kept as they are, so that a real game rule is not lost with the memory limits.
16. As a developer, I want a list that grows to keep getting a fresh items table holding a copy of its items, as now, so that list copies sharing an items table keep behaving exactly as the replays expect until the list step decides otherwise.
17. As a developer, I want every deep copy the game makes today (trade routes copied into the game zone, needs copied for a construction, values copied into a list) still made, so that no shared-table aliasing appears in this step.
18. As a developer reading the state dump, I want no `zone`, `budget` or `cap` line, so that the dump only describes the game.
19. As a developer running `make e2e end-state`, I want every `.end-state` unchanged, so that the removed lines (already filtered) prove the game state did not move.
20. As a player, I want the game to look and play exactly as before, so that every `.json.sha256` stays identical.
21. As a developer, I want `make e2e` still under 9 s and `make test` no slower, so that the fast loop holds; removing the bookkeeping should make both a little faster.
22. As a plan pilot, I want the step cut into chunks, each one removing one module group with all its callers, so that a failure points at one group.
23. As a plan pilot, I want each chunk verified by `make e2e` green with every `.json.sha256` unchanged and by the step's check restricted to its group, so that the chunk can be committed on its own.
24. As a plan pilot, I want the last chunk to run the step's full check, so that the step note can record it.
25. As a developer reading `docs/love-port/design.md`, I want the paragraphs on memory, `SIZEOF`, zone byte widths and struct casts of zone types removed or rewritten, so that later steps are planned from a true design.
26. As a developer reading `CLAUDE.md`, I want the layout line on `engine/` to stop listing zones and allocator, so that it describes the tree as it is.
27. As a developer reading the e2e notes, I want the notes that quote zone peaks or "apart from zone and cap" corrected, so that the notes describe what the dump writes.
28. As a developer, I want the state dump to stop requiring the zone modules, so that `devtools/` depends only on game state.
29. As a developer, I want the runtime draw meter left as it is (installed by `main.lua`, tested by the runtime fixture), so that this step removes only zones.
30. As a developer, I want the entity table capacity and the exit queue capacity to stay where they act (the entity id table, the exit queue) until the id and list steps, so that this step does not change how ids or the exit queue work.

## Implementation Decisions

### What goes, what stays

- Deleted modules: `engine.zone`, `engine.zone_sizes`, `engine.frame_zone`, `engine.game_zone`, `engine.allocator`, `engine.slot_group`, `engine.slot_tally`, `game.ui_zone`, `game.particle_zone`, `game.grab_arena`.
- Every `M.SIZEOF` table in `game/` goes. An entry used only to compute an allocation size is deleted with the allocation. An entry that is a real length read by game logic becomes a plain module constant with the same value and an English name (for example the save magic length, the save node planet id length, the bindings error text capacity, the save buffer byte count, the loading text length, the draw palette length). No `SIZEOF` word remains, in code or comments.
- An allocation (`allocator_alloc(zone, bytes)`, a slot taken from a pool, an arena taken) becomes a new table literal at the call site, filled as today. An `allocator_release`, `arena_release`, `arena_spill`, `arena_flush`, give-back or forget call is deleted.
- A function whose `zone` (or arena, slot, construction zone) parameter served only to allocate drops that parameter; every caller is updated. This covers the four list kinds (init, init with reserve, concat, filter, copy), `value_list` (whose `element_size` parameter served only byte counts and goes too), the box constructor, the PICO-8 string helpers (`p8_sub`, `p8_tostr_fixed`, `p8_concat2`), the collect formatter, the entity id table init, layout rows and tooltips, trade route lists and copies, needs, the random walk and weighted picker, and the remaining game helpers that take a zone (in `economy_score`, `pad_jump`, `pad_slipway`, `research`). Names are otherwise unchanged (renaming is step 3.4).
- Fields that held only bookkeeping go: a list's `zone`, an object's `slot` and `layout_row_slot`, a probe's `grab_arena`, the particle list's slot kind, and any bit or field whose only reader was zone bookkeeping (for example the UI's "owns its ephemerals" bit, if nothing else reads it). A bit or field with another reader stays unchanged (the `has` bits are step 2.1).
- A class `release` method whose only work was giving memory back is removed from its class definition when it becomes empty; one that also unhooks game state keeps that part.
- The exit queue loses its arena entries (`exit_queue_put_arena` and the arena branch of the flush). Its capacity, its count and its entity entries stay (step 1.4 makes it a plain array). Its `exit_queue_max` accessor, read only by the dump, goes.
- The entity id table keeps its 512-entry capacity and its "entity table full" fatal: the constant moves into the entity id module (step 1.3 removes it). Its peak-live counter and accessors, read only by the dump, go; `entity_id_live` stays if anything else reads it.
- The research project table's capacity, message box item caps and other fixed-size arrays are not zones and stay (later steps).
- The save format's limits (255 nodes and slipways) stay.
- The game loop's zone calls go: the frame zone reset each update, the game zone flip at game start and at cart init, the pending/release pair after flushing the exit queue, the game zone reset and grab-arena forget when quitting a game, the frame marks and rewinds in planet trades and planet rows. The rest of those functions (flushing the exit queue, abandoning live entities, forgetting registries, resetting the world) is unchanged and keeps its order.
- The registry's reserve table (list reserves at game start) stays, passed to the list init with reserve as now: reserves set list capacities, and capacities decide when a list grows (see the next point).

### Behavior that must not move

- A list that grows still replaces its `items` table with a fresh table holding a copy of its items, at the same moments as now (initial capacity 4, doubling, reserves honoured). Some list headers are copied by value and share `items` (`entity_id_list_copy`, the UI's ephemerals copy, planet list header copies): if growth reused the same table, a copy adding to its shared table would overwrite the original's items and move the replays. Only the allocator call goes; the copy-on-grow stays, as a local helper of each list module. Step 1.4 decides what replaces it.
- Every deep copy made today is still made (trade route copies, need copies, `copy_value` in value lists), only without a zone argument. Dropping a copy is not this step.
- No call order changes: allocation sites that also draw from the random generator or touch the registry keep their order.

### Transitional contract between chunks

- From the first chunk to the last, `allocator_alloc`, `allocator_grow` and `allocator_release` accept a missing zone: alloc returns a new empty table, grow returns a new table with a copy of the old one's keys when the old size is positive, release does nothing. The four list inits accept a missing zone the same way, and every assert that a zone was given (the four list inits and growths, layout rows and tooltips, value lists) is dropped in the first chunk. Callers of a removed pool or zone pass nothing in its place until the chunk that drops the parameter. The last chunk deletes the allocator, so no stand-in survives the step.
- Between chunks, the zones not yet removed keep their asserts (flip of a pending generation, rewind past a reset, zone full). Taking allocations out of a zone only lowers its use; a chunk whose replays stop on one of these asserts fixes it within the chunk.

### State dump

- The dump writes no `zone`, `budget` or `cap` line: the five memory zones, the budget line, the two GPU buffer `zone` lines, every pool `cap` line, every `elements_*` and `bytes_*` peak, the `exit_queue` and `entity_table` caps, and the `gpu_cluts` and `gpu_patterns` peaks. The pool and particle name tables of the dump go with them. Every other dump line is unchanged, in the same order.
- The draw meter stays as it is (installed by `main.lua`, read by the runtime fixture); the dump just stops reading it.
- `end_state.sh` and its unit test stay unchanged: its filter of `zone`, `budget` and `cap` lines becomes dead for the game but keeps old and new states comparable, and changing the end-state checker is not this step.

### Docs

- `docs/love-port/design.md`: delete the sentence on the 64-bit build's `SIZEOF["void *"]`, `ZONE_BYTES_BY_WIDTH` and `BUMP_ZONE_HALVES`; delete the sentence on structs reached by casting their first member (all three examples are zone types); replace the **Memory** paragraph with what stays true (a new object is a table built where it is needed; a list that grows gets a fresh items table holding a copy, and copies of a list header share that table; `copy_value` copies a list value one level deep; strings are Lua strings, compared with `==`); delete the sentence stating that every `sizeof` is an `M.SIZEOF` entry and never a bare number (H-1.1-5). Any other sentence naming a deleted module or `SIZEOF` is fixed in the same chunk as the module.
- `CLAUDE.md`: the layout line describing `engine/` drops "zones, allocator" (and "and memory"). § The game names no zone and needs no change.
- e2e notes: in `pad_planet_projects`, the "zone frame" peak is dropped from the state paragraph; in `pad_restart_resume`, "apart from zone and cap" becomes a statement that the lines are the same. No first paragraph (frame count) is touched, and no `.json`, `.sha256` or `.end-state` file is touched.

### Decided without `guideline`

- Keep `end_state.sh` unchanged (above): the plan forbids re-baselining end states and the step's check is about the dump, not the checker.
- Keep the draw meter: it is a runtime module with its own test, not a memory zone.
- Keep copy-on-grow and every deep copy: they are behavior the hashes may depend on; changing them belongs to step 1.4.
- The transitional nil-zone contract: it is what lets the game-side pools go first, as the plan orders, while the lists and layout still take a zone.

## Implementation Chunks

Each chunk removes one module group: its definitions together with all their callers. Each is verified, before its commit, by `make e2e` green with every `.json.sha256` unchanged, `make test` green, and the restricted check named in the chunk. The step's full check runs on the last chunk.

1. **UI pool** (`game.ui_zone`). Also: the transitional nil-zone contract in the allocator and the four list inits. Callers: menu, message box and toast, planet display rows, UI ephemerals, buttons and modal backdrops, the state dump. The ephemerals no longer go through the exit queue as an arena: `exit_queue_put_arena` and the arena branch of the flush go. The dump loses the `zone ui` line and the six pool `cap` lines, their `bytes_*` peaks and `elements_ephemerals`. Restricted check: `game/game/ui_zone.lua` is gone, `grep -rn 'ui_zone' game/` finds nothing, and the dump holds no `zone ui` line and no `cap` line for button, modal_backdrop, message_box, toast, row or ephemerals.
2. **Particle pool, slot groups and slot zones** (`game.particle_zone`, `engine.slot_group`, `engine.slot_tally`, and the slot-zone part of `engine.zone` and `engine.allocator`, whose only callers are gone). Callers: particles and emitters, the help screen, bursts, slipway transit dots, the state dump. Emitter init no longer takes a slot kind: the help screen's list keeps its own reserve (653) and other emitters theirs (158), as list reserves. The particle and transit-dot size asserts go. The dump loses the `zone particles` line, the five particle `cap` lines and the particle `elements_*` and `bytes_*` peaks. Restricted check: the three modules are gone, `grep -rnE 'particle_zone|slot_group|slot_tally|slot_zone' game/` finds nothing.
3. **Grab arena and arenas** (`game.grab_arena`, and the arena part of `engine.allocator`). Callers: the cart, planets and planet families, planet family trades, probes, save loading, slipways, structure icons, wormholes. Construction byte counts (the slipway construction bytes, a planet kind's construction bytes, probe and wormhole sizes passed to the arena) go. Objects built "in the construction zone" are plain tables; burst and planet releases lose their arena release. Restricted check: `game/game/grab_arena.lua` is gone and `grep -rnE 'grab_arena|arena_' game/` finds nothing.
4. **Frame zone** (`engine.frame_zone`). Callers: background, cart, draw functions, economy and economy score, galaxy, menu, pad navigation, planet, planet display, planet family trades, planet trades, probe, research display, score panel and its display, slipway, structure icon, UI, the state dump. The marks and rewinds and the per-update reset go. The dump loses `zone frame`. Restricted check: the module is gone and `grep -rn 'frame_zone' game/` finds nothing.
5. **Game zone and load zone** (`engine.game_zone`). Callers: background, cart, economy and economy tiers, galaxy, help, mouse, planet trades (with its "kept route copied into the game zone" assert), research (projects built in the load zone), save loading, score panel, scroll, structure icon, UI, the entity registry, the state dump. The flip, pending, release and reset calls go from the cart. The dump loses `zone game` and `zone load`. Restricted check: the module is gone and `grep -rn 'game_zone' game/` finds nothing.
6. **Engine containers and helpers without a zone**: the four list kinds, `value_list` (zone and element size), the box constructor, the PICO-8 string helpers, the collect formatter, the exit queue (which no longer needs the allocator), the entity id table init and its capacity constant (moved out of `engine.zone_sizes`), the entity registry. Every caller in `game/` updated. Their `SIZEOF` tables go. The dump loses the `exit_queue` and `entity_table` caps. Restricted check: `grep -rnE 'SIZEOF|allocator|zone' game/engine/ game/p8/` finds nothing other than inside `engine.allocator`, `engine.zone` and `engine.zone_sizes`, and no engine or p8 function takes a zone.
7. **Game modules without a zone; allocator and zones deleted**: layout, trade, gen, economy score, pad jump, pad slipway, research and every remaining game module drop their zone parameters and `SIZEOF` tables (real lengths renamed as constants, see above); `engine.allocator`, `engine.zone` and `engine.zone_sizes` are deleted; the dump loses the budget line and the GPU `zone` and `cap` lines; `design.md`, `CLAUDE.md` and the two e2e notes are updated. Full check: the step's grep finds nothing, all ten modules are gone, and no state file written by `make e2e end-state` holds a line starting with `zone`, `budget` or `cap`; `make test-all` green; `make e2e end-state` passing with every `.json.sha256` and `.end-state` unchanged; `make e2e` time recorded.

## Testing Decisions

- A good test here is the replays: the step changes structure, not behavior, so the net is every `.json.sha256` unchanged after each chunk and every `.end-state` unchanged at the end. A unit test of a deleted module is not written; no unit test is written for "a table is a table".
- The restricted check of each chunk is a grep over `game/` plus, where the chunk removes dump lines, a look at a state file from a replay: it shows the group gone, false before the chunk and true after.
- `make test` must stay green throughout. After step 1.1, `lua-test-modules` only requires every module (H-1.1-1, H-1.1-2, H-1.1-3): a deleted module or constant cannot fail it, but a module that still requires a deleted one does, which is what catches a missed caller in about a second.
- `lua-guard` catches a leftover reference to a removed local (an undeclared global after a deleted `local zone = require(...)`).
- The only game regression test (`tests/lua/game/test_economy_formulas.lua`) touches no zone; if a chunk's signature change reaches what it calls, the test changes with the module (CLAUDE.md: the replays are the source of truth).
- `test_end_state` and `test_lua_modules` are untouched: the first tests the checker's filter with its own literal lines, the second uses its own fake tree whose file names (`engine/zone.lua`) are fixtures, not the game (H-1.1-6).
- Prior art: plan 4's module-by-module removal of the C under the same e2e net; the step 1.1 PRD's "every module loads" check.

## Out of Scope

- Entity ids as plain numbers, the entity table and its capacity (step 1.3).
- The four list kinds as plain 1-based arrays, `collect`, the `last_count` iterator, copy-on-grow and the exit queue's array and capacity (step 1.4). This step only removes their zone.
- The `has` bits, including bits that remain readable after this step (step 2.1).
- Booleans, 1-based arrays, `-- Copy of` headers, the module prefix in names (phase 3), even in lines this step edits: a touched line keeps its `0`/`1` flags, its 0-based index and its prefixed names.
- `end_state.sh`, its filter and its unit test; the draw meter; `lua-guard`; the fixed-point library and the random generator.
- Fixed-size arrays and table capacities that are not memory zones (research projects, message box items, the save format's limits).

## Further Notes

### Hypotheses on step 1.1 (not done when this PRD was written)

- **H-1.1-1**: `lua-test-modules` only requires every `game/` module (except `main` and `conf`) under plain LuaJIT and compares no table, constant or `SIZEOF` entry with anything.
- **H-1.1-2**: `tests/lua/table_reference.txt` no longer exists; this step edits no reference file.
- **H-1.1-3**: the files handed to `lua-test-modules` are still the tree's wildcard (`LUA_GUARD_FILES`), so a deleted module drops out of the check with no Makefile edit.
- **H-1.1-4**: step 1.1 changed nothing under `game/`, `tools/` or `tests/e2e/`: the code read at HEAD for this PRD is the code this step starts from.
- **H-1.1-5**: `docs/love-port/design.md` still holds the sentence that every `sizeof` is an `M.SIZEOF` entry, never a bare number (the step 1.1 PRD keeps it for this step), while its description of `lua-test-modules` has become load-only.
- **H-1.1-6**: `tests/test_lua_modules.c` still builds its own fake tree, including a fake `engine/zone.lua`, and reads nothing of `game/`.
- **H-1.1-7**: `make test` takes no longer than before step 1.1, so this step's own effect on it is what the "never slower" rule measures.

If H-1.1-1 or H-1.1-2 is false, step 1.1 is not done and this step does not start: deleting `SIZEOF` tables would break `make test` against a frozen reference. If H-1.1-3 is false, the chunk that deletes a module also removes it from whatever list the check uses. If H-1.1-5 is false, the step removes whatever sentence of `design.md` states the `SIZEOF` convention, or nothing if none is left.

### Other notes

- The step note records: the full grep finding nothing, the ten modules gone, no `zone`/`budget`/`cap` line in the end-state run's state files, and how long `make e2e` took.
- Expected effect on speed: removing the per-allocation bookkeeping (byte rounding, slot scans over up to 887 entries, tallies) should make `make e2e` slightly faster, never slower.
- Scale: about fifty modules require a zone module and about thirty-five have a `SIZEOF` table; chunks 6 and 7 are the widest but mechanical. If a chunk proves too big for one cycle, the implementer splits it along module lines (definitions with their callers), never by leaving a caller half-converted.
