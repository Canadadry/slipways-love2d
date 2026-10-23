---
title: "Entity ids as plain numbers"
description: "Entity ids are still the C's `{ index, generation }` structs passed by value, rebuilt field by field at every copy and compared only through `entity_id_equal`, where a Lua number compared with `==` would do."
status: needs-triage
---

## Problem Statement

The Lua game links entities to each other by id, as the C did: a planet's
slipway ends, a probe's target, the UI's hovered and held entities, the pad
navigation's focus, the message box's source, the save's node lists and
about a hundred other fields hold an id, never the entity itself. That part
is sound. What came over from the C is the shape of the id: a
`{ index, generation }` struct passed by value. The id table is a fixed
array of 512 slots with a free-index stack and a generation per slot, bumped
on each release and above any generation handed out so far at each table
reset, so that a stale id never matches a new entity on a reused slot.

The cost shows at every use. Because a table is shared by reference in Lua,
every place that stores, returns or copies an id rebuilds it field by field
(`{ index = id.index, generation = id.generation }`, about thirty sites).
"No link" is the constant `ENTITY_ID_NONE` (`{ index = 0, generation = 0 }`),
which twenty modules wrap in their own local `entity_id_none()` copier
(about a hundred uses). Ids cannot be compared with `==`, so every
comparison goes through `entity_id_equal` or `entity_id_is_none`, each
returning `0` or `1`, and a reader has to remember that two id tables that
look equal are not `==`. A developer adding a link must learn all of this
first, and a single missed copy or a raw `==` between two ids would be a
silent bug the replays might not see. Phase 0 decided: ids become plain
numbers, never reused, resolved through one `id → entity` table that drops
an entity at its death; the `{ index, generation }` tables,
`entity_id_none()` and `entity_id_equal` go.

## Solution

An entity id is a Lua number. `entity_id_take` hands each constructed
entity the next number of a counter that only ever grows, for the whole
process, so no number is ever handed out twice, not even across games.
Resolving an id is one lookup in the current `id → entity` table; releasing
an entity (when the exit queue flushes it, as now) removes its entry, so its
id resolves to `nil` from then on. A registry reset starts an empty table,
so every id of the previous game resolves to `nil`. "No link" is `nil`. Ids
are stored, returned and copied by plain assignment and compared with `==`
and `~=`. The `{ index, generation }` tables, `ENTITY_ID_NONE`, the local
`entity_id_none()` and `entity_id_copy` helpers, `entity_id_equal`,
`entity_id_is_none` and the `entity_id_type` module go. The 512-entry entity
table capacity and its "entity table full" fatal go too: they were a memory
limit, not a game rule.

The game plays exactly as before: every e2e capture hash and every end state
is unchanged, and `make e2e` stays under 9 s.

When the step is done:
`grep -rE 'index *=[^}]*generation|generations|entity_id_equal|entity_id_none' game/`
finds nothing. The registry's change counter `entity_registry_generation` is
not an id and stays.

## User Stories

1. As a developer, I want an entity id to be a plain Lua number, so that I can store, return and copy it by assignment.
2. As a developer, I want to compare two ids with `==` and `~=`, so that I don't need `entity_id_equal` or remember that equal-looking tables differ.
3. As a developer, I want "no link" to be `nil`, so that a field with no linked entity reads as absent, the way Lua code reads.
4. As a developer, I want `entity_get(nil)` to return `nil`, so that resolving an unset link needs no guard.
5. As a developer, I want `entity_id_link(nil)` to return `nil` and `entity_id_link(e)` to keep asserting that `e` holds an id, so that a link to an unconstructed entity is still caught at once.
6. As a developer, I want `entity_id_of(e)` to return `nil` for an entity that holds no entry of the current table, so that the registry's "constructed?" test keeps working.
7. As a developer, I want ids never reused, so that a stale id can never resolve to a newer entity, without any generation bookkeeping.
8. As a developer, I want the id counter never reset, not even when the registry is reset for a new game, so that an id kept from a previous game resolves to `nil` and never to an entity of the new one.
9. As a developer, I want a registry reset to start an empty `id → entity` table, so that every id of the previous game resolves to `nil`, as the generation epoch guaranteed.
10. As a developer, I want a forgotten table (after quitting a game) to resolve every id to `nil` and to refuse a new take with the same assert as today, so that a construction outside a game is still caught.
11. As a developer, I want an entity's entry dropped exactly when the exit queue flushes it, before its class `release` runs, so that a dead-but-not-flushed entity still resolves during the frame, as the replays expect.
12. As a developer, I want releasing an entity whose entry is not its own (a stale entity, or one from a previous table) to do nothing, so that a release can never drop another entity's entry.
13. As a developer, I want an entity that takes an id twice to keep the old entry resolvable until it is released, as today, so that no replay sees a link vanish earlier.
14. As a developer, I want the economy, which takes an id without registering, to keep taking one the same way, so that links to the main economy keep resolving.
15. As a developer, I want the entity header's `id` field to start as `nil` in `entity_init_header`, so that an entity not yet constructed holds no id.
16. As a developer, I want the static placeholder objects (the targeting track target, the slipway "too far" target, the research project header) to hold `nil` ids, so that nothing builds a fake `{ index = 0, generation = 0 }`.
17. As a developer, I want every `{ index = x.index, generation = x.generation }` copy replaced by `x`, so that a copy reads as a copy.
18. As a developer, I want every module's local `entity_id_none()` and the UI's `entity_id_copy` helper removed, so that no module carries a wrapper for a value Lua already has.
19. As a developer, I want `entity_id_type` deleted, so that no module requires a module that only held the old id shape.
20. As a developer, I want an id list to hold numbers and compare them with `==` (add, delete, has, at, concat, filter), so that its items are plain values; the list itself stays until step 1.4.
21. As a developer, I want the save code's search of a saved id among the node and slipway ids to compare with `==`, so that saving keeps writing the same indices.
22. As a developer, I want the pad panel's memo key to hold the UI and economy ids as numbers (or `nil`) and compare them with `==`, so that the memo is invalidated at the same moments.
23. As a developer, I want the entity table capacity (512) and its "entity table full" fatal gone, so that only game rules limit how many entities exist.
24. As a developer, I want the registry's "layer order full" fatal, which only read that capacity, gone with it, so that the layer order grows like any Lua table.
25. As a developer, I want the live and peak-live counters of the id table gone if nothing reads them, so that the id module holds only what resolves ids.
26. As a developer, I want `entity_registry_generation` left exactly as it is, so that the registry's change counter keeps invalidating the caches that read it.
27. As a developer, I want the research project ids (`{ epoch, index }` into the project table) left as they are, so that this step touches entity ids only.
28. As a developer, I want every predicate that returned `entity_id_equal`'s or `entity_id_is_none`'s `0`/`1` to keep returning `0`/`1`, so that callers testing `~= 0` keep working until the booleans step.
29. As a player, I want the game to look and play exactly as before, so that every `.json.sha256` stays identical.
30. As a developer running `make e2e end-state`, I want every `.end-state` unchanged, so that the game state is shown not to move.
31. As a developer, I want `make e2e` under 9 s and `make test` no slower, so that the fast loop holds; numbers instead of tables should make both a little faster.
32. As a plan pilot, I want the step cut into two chunks, the id representation first and the old id API second, so that a failure points at one of them.
33. As a plan pilot, I want each chunk verified by `make e2e` green with every `.json.sha256` unchanged and by its restricted check, so that it can be committed on its own.
34. As a plan pilot, I want the last chunk to run the step's full check, so that the step note can record it.
35. As a developer reading `docs/love-port/design.md`, I want the sentences that use `ENTITY_ID_NONE`, `entity_id_none()` and `entity_id_equal` as conventions removed and one sentence saying what an entity id is now, so that later steps are planned from a true design.
36. As a developer reading `CLAUDE.md`, I want § The game to say that links to other entities are held as plain number ids, so that it describes the code as it stands.

## Implementation Decisions

### The id table (`engine.entity_id`)

- An id is a positive Lua number. A module-level counter starts at 0 when the module loads and is incremented before each take; the new value is the id. The counter is never reset: not by `entity_id_table_init`, not by `entity_id_table_forget`, not by a registry reset. LuaJIT numbers are exact up to 2^53, far beyond any run.
- The table is a plain Lua table keyed by id, holding the entity. `entity_id_table_init` replaces it with a new empty table (so every earlier id resolves to `nil`); `entity_id_table_forget` sets it to `nil`. Their call sites in the registry are unchanged.
- `entity_id_take(e)` keeps its assert that the table exists ("entity_registry_init() must run before entity_construct") and that `e` is not `nil`, takes the next id, stores `table[id] = e`, writes `e.id = id` and returns the id. It does not remove an entry `e` held under an earlier id (today's code does not either).
- `entity_id_release(e)` removes `table[e.id]` only when the table exists, `e.id` is not `nil` and `table[e.id] == e`; otherwise it does nothing.
- `entity_get(id)` returns `nil` when the table is `nil` or `id` is `nil`, else `table[id]` (`nil` for a released or unknown id).
- `entity_id_of(e)` returns `e.id` when `entity_get(e.id) == e`, else `nil`. `entity_id_link(e)` returns `nil` for a `nil` `e`, else `entity_id_of(e)` with the existing assert that the result is not `nil` (its message kept).
- The capacity constant, the "entity table full" fatal, the free-index stack, the generation array, `next_generation`, `UINT16_MAX`, the highest-generation tracker and the "generation 0 is reserved" assert go. The live and peak-live counters and their accessors go if nothing outside the module reads them after step 1.2 (H-1.2-2); if a reader remains, the live count is kept as a counter maintained by take and release.
- The exit queue's flush is unchanged: it releases the entity's id, then calls the class `release`. Its comment about "the slot before the memory" is rewritten to the trap it guards (the entity must stop resolving before its class release runs) or dropped, per the comments rule.

### The old id shape goes everywhere

- `engine.entity_id_type` is deleted: `ENTITY_ID_NONE`, `entity_id_equal`, `entity_id_is_none`. Every `require` of it goes.
- Every `{ index = x.index, generation = x.generation }` becomes `x`; every `{ index = none.index, generation = none.generation }`, every `{ index = 0, generation = 0 }` written as an id, and every local `entity_id_none()` call becomes `nil` (in a table constructor the field is simply omitted or written `= nil`, whichever keeps the constructor listing its fields; the choice is the implementer's, behavior is the same). The local `entity_id_none()` helpers and the UI's `entity_id_copy` are deleted.
- `entity_id_equal(a, b) ~= 0` becomes `a == b`, `== 0` becomes `a ~= b`; `entity_id_is_none(x) ~= 0` becomes `x == nil`, `== 0` becomes `x ~= nil`. Where a function returns the `0`/`1` of one of them as its own result, it returns `(a == b) and 1 or 0` (or the `nil` test likewise): the booleans step (3.1) removes that later.
- The entity header's `id` starts `nil` in `entity_init_header`; the research project zero header, the targeting zero, the probe's and slipway's static placeholder targets and the wormhole's anchor reset hold `nil`.
- `entity_id_list` keeps its shape, its 0-based items, its count, its capacity, its iterator and its copy-on-grow (step 1.4). Only its items change: `add` stores the id itself, `del` shifts items by assignment and compares with `==`, `has` compares with `==`, `at` still asserts the id resolves.
- No `pairs`-based shallow copy changes behavior: no code mutates an id table in place today (checked at HEAD: no `.index =` or `.generation =` assignment outside the id modules), so a copy sharing an id table and a copy holding the same number are the same.
- No call order changes: every take, link, resolve and release happens at the same point as today, so no random draw and no registry change moves.
- Names are otherwise unchanged (`entity_id_take`, `entity_get`, `entity_id_link`, … keep their module prefix: step 3.4).

### What stays

- `entity_registry_generation` and the registry's `g_generation` counter: a change counter, not an id.
- The research project ids `{ epoch, index }`, `research_expire_project_ids` and the project table: they index a fixed table of research projects, are not entity ids, and match none of the step's patterns. Decided here as out of scope; a later step may take them up.
- The exit queue's array, capacity and count (step 1.4), the `has` bits (step 2.1).
- The planet string ids (`PLANET_ID_SIZE`, `slipway_served_planets_*`): strings, not entity ids.

### The registry (`engine.entity_registry`)

- `entity_registry_set_layer` tests `entity_id.entity_id_of(e) == nil` instead of `entity_id_is_none(...) ~= 0`.
- The "layer order full" fatal goes with the capacity it read (user story 24). The layer order otherwise works as now.
- `entity_registry_init`, `entity_registry_shutdown` and `entity_registry_forget_registries` keep calling the id table's init and forget at the same points.

### Docs

- `docs/love-port/design.md`, § Conventions inherited from the C, **Structs and out-parameters**: drop "a constant such as `ENTITY_ID_NONE`" and "(`entity_id_none()`)" from the by-value sentence (the rule itself stays true for other by-value structs) and replace "Entity ids are compared only through `entity_id_equal`." with one sentence: an entity id is a number handed out by `entity_id_take`, never reused, resolved by `entity_get` through the current table, which drops an entity when the exit queue flushes it; `nil` is "no link"; ids are compared with `==`. Any other sentence naming `ENTITY_ID_NONE`, `entity_id_type`, a generation of an entity id or the entity table capacity is fixed in the same chunk.
- `CLAUDE.md` § The game: "links to other entities held as ids" becomes "links to other entities held as plain number ids (`nil` for none)".
- No e2e note, `.json`, `.sha256` or `.end-state` is touched: none names an entity id.

### Decided without `guideline`

- `nil` for "no link" rather than `0`: a `0` would be a C sentinel kept under another name; `nil` is what `entity_get` already returns for "nothing", and plain fields that may be `nil` are already how the game holds an optional entity pointer.
- The counter is never reset: resetting it at each registry init would let an id kept across games (a pad navigation field, a UI global) resolve to a new entity, which the generation epoch prevented.
- The 512 capacity goes: the plan keeps a capacity only where it is a game rule, and the 1.2 PRD hands its removal to this step.
- Research project ids stay: not entity ids, outside the step's check.
- No new unit test (see Testing Decisions).

## Implementation Chunks

Each chunk is verified, before its commit, by `make e2e` green with every `.json.sha256` unchanged, `make test` green, and its restricted check.

1. **The id representation** (the id table with every site that builds, copies or stores an id). `engine.entity_id` as decided above; `entity_class`'s header `id`; `entity_id_list`'s items; every `{ index = …, generation = … }` construction in `game/` (engine and game modules: message box, pad back, pad build, pad jump, pad nav, pad panel, pad slipway, planet, planet display, planet family trade, probe, research, save load, score panel, slipway, slipway display, structure icon, targeting, UI, UI button, wormhole); every local `entity_id_none()` helper changed to return `nil` and `entity_id_copy` deleted (its calls become the plain value). Transitional, for this chunk only: `entity_id_type` loses `ENTITY_ID_NONE` (replaced in its callers by `nil`) and keeps its two functions, `entity_id_equal(a, b)` returning `(a == b) and 1 or 0` and `entity_id_is_none(id)` returning `(id == nil) and 1 or 0`, so their callers stay unchanged. The capacity, its fatals and the generation bookkeeping go. Restricted check: `grep -rE 'index *=[^}]*generation|generations|ENTITY_ID_NONE|ENTITY_TABLE_CAP' game/` finds nothing.
2. **The old id API** (`engine.entity_id_type` with all its callers). Every `entity_id_equal` and `entity_id_is_none` call is rewritten with `==`/`~=` as decided; every local `entity_id_none()` helper is deleted and its calls become `nil`; `entity_id_type` is deleted with every `require` of it; `design.md` and `CLAUDE.md` are updated. Full check: the step's grep finds nothing, `grep -rE 'entity_id_type|entity_id_is_none' game/` finds nothing, `make test-all` green, `make e2e end-state` passing with every `.json.sha256` and `.end-state` unchanged, `make e2e` time recorded.

## Testing Decisions

- A good test here is the replays: the step changes the shape of a value, not behavior, so the net is every `.json.sha256` unchanged after each chunk and every `.end-state` unchanged at the end. Ids never reach a capture or a state dump (checked at HEAD: the dump reads only the id table's counters, which step 1.2 removes, H-1.2-2; the save writes node and slipway indices found by comparing ids, never an id).
- No new unit test file: `CLAUDE.md` forbids making `make test` slower, and each `tests/lua/game/` test is one more LuaJIT run. The contracts a replay might miss (a stale id never resolves to a newer entity; a forgotten table refuses a take) hold by construction (a counter that never resets, a fresh table at init) and by the asserts kept in `entity_id_take`.
- `make test` catches a missed `require` of the deleted `entity_id_type` in about a second (`lua-test-modules` requires every module, H-1.1-1 of the 1.2 PRD, done in step 1.1), and `lua-guard` catches a leftover use of a deleted local helper (`entity_id_none` or `entity_id_copy` becoming an undeclared global).
- `tests/lua/game/test_economy_formulas.lua` uses no entity id (checked at HEAD); no test under `tests/` or `tools/` names an entity id function, so none changes.
- Each chunk's red-to-green is its restricted grep: false before the chunk, true after.
- Prior art: step 1.2's chunked removal under the same e2e net, with a transitional contract between chunks.

## Out of Scope

- The four list kinds as plain 1-based arrays, `entity_id_list` itself, the `last_count` iterator, copy-on-grow, the exit queue's array and capacity (step 1.4). This step only changes what an id list holds.
- The `has` bits, including `ENTITY_HAS_*` read next to an id (step 2.1).
- Booleans (step 3.1): the `0`/`1` results of predicates and of `entity_id_list_has`/`del` stay, and a rewritten equality that flows out as a result keeps its `0`/`1`. 0-based arrays (3.2), `-- Copy of` headers (3.3), module prefixes in names (3.4), even in lines this step edits.
- The research project ids and the project table.
- `entity_registry_generation`.
- Zones and allocators (step 1.2).

## Further Notes

### Hypotheses on step 1.2 (not done when this PRD was written)

This PRD was written from the code at HEAD, before step 1.2. Each assumption about step 1.2's outcome:

- **H-1.2-1**: `entity_id_table_init` takes no zone; `engine.entity_id` requires neither `engine.allocator` nor `engine.zone_sizes`; the entity table capacity (512) lives in `engine.entity_id` (moved out of `zone_sizes`), the "entity table full" fatal still reads it, and the registry's "layer order full" fatal reads it from there.
- **H-1.2-2**: the state dump no longer writes the `entity_table` cap line and no longer requires `engine.entity_id`; `entity_id_max` and the peak-live counter are gone; `entity_id_live` is either gone or read by nothing outside its module.
- **H-1.2-3**: the exit queue holds entities only; its flush still calls `entity_id_release(e)` and then the class `release`, in that order.
- **H-1.2-4**: `entity_id_list` init, init with reserve, concat and filter take no zone; the list is still `{ items, count, capacity }` with 0-based items, the `last_count` iterator and copy-on-grow, and `add` still stores `{ index = id.index, generation = id.generation }`.
- **H-1.2-5**: the registry's init, shutdown and forget-registries still call `entity_id_table_init` and `entity_id_table_forget` at the same points; only the game zone argument is gone.
- **H-1.2-6**: step 1.2 changed no id-handling line except to drop a zone argument: every `{ index = …, generation = … }` construction, local `entity_id_none()` helper, `entity_id_copy`, `entity_id_equal` and `entity_id_is_none` call present at HEAD is still there, and no module was renamed.
- **H-1.2-7**: every construction (planets, slipways, probes, wormholes, structure icons, UI objects, message boxes) still reaches `entity_construct` → `entity_id_take` in the same order as at HEAD, with no grab arena around it; the economy still calls `entity_id_take` without registering.
- **H-1.2-8**: `design.md`'s **Structs and out-parameters** paragraph still names `ENTITY_ID_NONE`, `entity_id_none()` and `entity_id_equal` (step 1.2 removes only its cast sentence on zone types); `CLAUDE.md` § The game still reads "links to other entities held as ids".
- **H-1.2-9**: after step 1.2, `generations` appears in `game/` only in `engine.entity_id` (the zone modules that spoke of memory generations are deleted).
- **H-1.2-10**: `make test-all` and `make e2e end-state` are green at the end of step 1.2, and `make e2e` runs under 9 s.

If H-1.2-1 is false and the capacity is already gone, chunk 1 has nothing to remove there. If H-1.2-2 is false and the dump still reads the id table's counters, chunk 1 keeps `entity_id_live` as a counter maintained by take and release, and drops only what the dump does not read. If H-1.2-3 or H-1.2-7 is false, the order of release or construction at the new code is kept as it is, not restored to HEAD's: this step changes no order. If H-1.2-6 is false, the implementer first lists the id-handling sites as they stand (the step's grep, plus `entity_id_is_none|entity_id_copy|ENTITY_ID_NONE`) and converts those. If H-1.2-8 is false, the docs update removes whatever sentence still states the old id convention, or nothing if none is left. If H-1.2-9 is false, a remaining `generations` in a comment is reworded in chunk 1. If H-1.2-10 is false, step 1.2 is not done and this step does not start.

### Other notes

- The step note records: the full grep finding nothing, `entity_id_type` gone, and how long `make e2e` took.
- Scale at HEAD: about 25 modules handle ids; about 30 by-value copies, about 100 uses of a "none" id, about 20 local `entity_id_none()` helpers. Both chunks are mechanical; chunk 1 is the wider. If it proves too big for one cycle, the implementer splits it along module lines (the id table and the engine first, then game modules in groups), never leaving a module half-converted: a module still building `{ index, generation }` tables once `entity_get` expects numbers breaks its links at once, which the replays show.
- Expected effect on speed: no table built per id copy or comparison, no 512-entry loop at each registry init; `make e2e` should get slightly faster, never slower.
