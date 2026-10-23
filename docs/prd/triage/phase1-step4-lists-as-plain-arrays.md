---
title: "The four list kinds as plain arrays"
description: "The game's lists are still the C's `{ items, count, capacity }` headers with 0-based items, copy-on-grow and a `last_count` iterator, behind four list modules and a `collect` helper, where plain 1-based Lua arrays would do."
status: needs-triage
---

## Problem Statement

Every list in the game goes through one of four engine modules copied from
the C: `entity_list` (the registry's live, per-message and per-tag buckets,
and filter results), `pointer_list` (a planet's needs, a research project's
inputs, the invented technologies, a particle emitter's particles, a
slipway's transit dots), `entity_id_list` (a planet's slipways, a trade
route's planets and slipways, the UI's ephemerals and hover layers, a
wormhole's anchors) and `value_list` (the trade route lists). Each list is a
header `{ items, count, capacity }` whose items start at 0, with an
`_init`/`_init_reserve`/`_add`/`_del`/`_free` API, a capacity that means
nothing in Lua, and a "copy-on-grow" rule added in Lua only so that a
header copied by value keeps sharing its items until the next add that
grows the list. Walking a list takes a three-line iterator dance
(`_all`, then `more, e = _iter_next(it, e)` until `more == 0`) whose
`last_count` field quietly compensates one removal per step. `collect` adds
a second, pointer-list-only set of filter/map/concat/contains helpers, and
also hosts an unrelated money formatter.

A developer reading or changing any game module meets all of this: a
`.count` and `.items[i - 1]` where Lua has `#t` and `t[i]`, an index that is
off by one compared with every other Lua table, header copies whose
behavior depends on whether the source list happened to grow, and an
iterator that tolerates mutation in a way nobody wrote down. Phase 0
decided: the four kinds become plain 1-based Lua arrays; a traversal that
tolerates changes says so (a copy, or a removal deferred to the exit queue)
instead of the `last_count` iterator; `collect` becomes local loops; the
exit queue stays, as a plain array.

This is the plan's riskiest step for the hashes: the order in which a list
is walked, what a walk sees when its body changes the list, what a copy of a
header sees when its source changes, and when a growing list stops sharing
its items with a copy all decide which entity is drawn, hovered, offered a
trade or updated first.

## Solution

Every list of the four kinds becomes a plain Lua sequence: indices
`1 .. #t`, no holes, no `count` or `capacity` field, no `items` sub-table.
The list table itself is what was the header: whoever held the header now
holds the array. Appending is `t[#t + 1] = v`, removing the first equal
element is a search followed by `table.remove(t, i)`, emptying a list that
others hold is done in place, and walking a list is a numeric `for` or, when
the body may change the list, a walk over a copy that says so. The four list
modules and `collect` are deleted; `collect`'s money formatter moves to the
economy formulas. The exit queue becomes a plain 1-based array of entities
with no capacity.

The game plays exactly as before: every e2e capture hash and every end
state is unchanged, `make e2e` stays under 9 s and `make test` no slower.

When the step is done: `entity_list.lua`, `pointer_list.lua`,
`entity_id_list.lua`, `value_list.lua` and `collect.lua` are gone and
`grep -r last_count game/` finds nothing.

## User Stories

1. As a developer, I want a planet's slipways, needs and routes to be plain Lua arrays, so that I read them with `#t` and `t[i]` like any Lua table.
2. As a developer, I want every list of the four kinds to start at index 1, so that a list never needs a `- 1` or `+ 1` that other Lua tables don't.
3. As a developer, I want no `count` or `capacity` field on a list, so that the length has one source, `#t`, and cannot disagree with the contents.
4. As a developer, I want appending to be `t[#t + 1] = v`, so that I don't need a module call to add an element.
5. As a developer, I want removing an element to be a search and `table.remove`, so that the shift of the following elements is the standard library's.
6. As a developer, I want walking a list to be a numeric `for`, so that I don't need the `_all`/`_iter_next` dance.
7. As a developer, I want a walk whose body may change its list to walk a copy, and to say so, so that the tolerance to change is visible where it matters.
8. As a developer, I want no list ever to hold `nil`, so that `#t` is always the element count.
9. As a developer, I want a list held by several owners (a registry bucket, a planet's needs, a project's inputs, an emitter's particles) to keep its identity when it is emptied or refilled, so that every holder keeps seeing the current list, as every holder of the header did.
10. As a developer, I want each former header copy replaced by either the array itself (when the source is never read again) or a copy of the array (when both live on), so that no copy shares storage by accident.
11. As a developer, I want the copy-on-grow rule gone, so that whether two holders see the same elements no longer depends on when a list grew.
12. As a developer, I want `collect`'s filter, map, concat and contains helpers replaced by local loops at their callers, so that a two-line loop is not hidden behind a helper.
13. As a developer, I want the signed money formatter to live with the economy formulas and be tested there, so that it is found where money figures are computed.
14. As a developer, I want a resolved read of an id list to keep asserting that the id is alive, so that a dead id left in a game list is still caught at once.
15. As a developer, I want the trade route list to be a plain array of route values, with only the add that copies a route kept as a function, so that its contract (a stored route is a copy) stays stated.
16. As a developer, I want a trade route's planets and slipways arrays to stay shared between route copies and never be changed once built, so that sharing them stays safe.
17. As a developer, I want the registry's buckets to be plain arrays returned as they are, so that `entities_by_tag(tag)` gives me an array (or `nil` for a tag never seen).
18. As a developer, I want the exit queue to be a plain 1-based array of entities, so that it reads like the lists it serves.
19. As a developer, I want the exit queue's capacity and "exit queue full" fatal gone, so that only game rules limit how many entities die in a frame.
20. As a developer, I want the exit queue flush to keep releasing exactly the entities queued when the flush began and to drop anything queued during it, as today, so that no release order or count moves.
21. As a developer, I want the walks already written as index loops with a "removing skips the next one" note (entity update, particles, transit dots) to keep that exact skip, so that update timing and every replay stay as they are.
22. As a developer, I want walking order unchanged everywhere (ascending, insertion order), so that the first match, the nearest entity and the drawing order are the same.
23. As a developer, I want every predicate of a former filter, find-first or concat to be called the same number of times in the same order, so that no hidden side effect moves.
24. As a developer, I want the places that cross from a 1-based list into a 0-based data table or save array (invented technologies into a menu row or a save, route ids into save indices, a saved index into a registry bucket) to convert explicitly, so that the save format and the menus are unchanged.
25. As a developer, I want the data tables shaped `{ items, count }` (layout rows, integer sequences, the available technologies and projects results) left as they are, so that this step touches the four list kinds only and step 3.2 does the rest.
26. As a developer, I want the registry's layer order and its views left as they are, so that the draw order code changes in step 3.2 only.
27. As a player, I want the game to look and play exactly as before, so that every `.json.sha256` stays identical.
28. As a developer running `make e2e end-state`, I want every `.end-state` unchanged, so that the game state is shown not to move.
29. As a developer, I want `make e2e` under 9 s and `make test` no slower, so that the fast loop holds.
30. As a plan pilot, I want the step cut into one chunk per list kind with all its callers, and a last chunk for the exit queue and the docs, so that a failure points at one kind.
31. As a plan pilot, I want each chunk verified by `make e2e` green with every `.json.sha256` unchanged and by its restricted check, so that it can be committed on its own.
32. As a plan pilot, I want the last chunk to run the step's full check, so that the step note can record it.
33. As a plan pilot, I want a traversal whose change behavior neither a copy nor a deferred removal can reproduce to stop the step and be reported, so that a phase-0 decision that cannot be carried out is reopened with the user, as the plan's rules require.
34. As a developer reading `docs/love-port/design.md`, I want the copy-on-grow, `copy_value` and `entity_id_list_copy` sentences replaced by what lists are now, so that later steps are planned from a true design.
35. As a developer reading the `tdd` skill's examples, I want them to use a module that still exists, so that an implementer does not copy a call to a deleted module.

## Implementation Decisions

### What a list is now

- A list of the four kinds is a plain Lua sequence indexed `1 .. #t`. It never holds `nil`. It has no `count`, `capacity` or `items` field, no iterator object and no reserve. `#t` replaces every `.count` read on such a list, `t[i + 1]` every 0-based `.items[i]` read, `t[#t]` every `.items[count - 1]`, `t[1]` every `.items[0]`.
- The array is what was the header table. A field that held a header holds the array (`planet.slipways`, `planet.needs`, `economy.invented`, `emitter.particles`, `slipway.transit_dots`, a project's `inputs`, `ui.ephemerals`, each hover layer, `wormhole.anchors`, a route's `planets` and `slipways`, a planet's `incoming_routes` and `outgoing_routes`, an offer's `accepted_routes`, the registry buckets). A local "zero list" constructor (`list_zero`, `id_list_zero`, `entity_id_list_zero`, `trade_route_list_zero`, the `{ items = nil, count = 0, capacity = 0 }` literals) becomes `{}`.
- Append: `t[#t + 1] = v`. Every append site whose value is not obviously non-`nil` from the code just above gets an `assert(v ~= nil, "<what the list holds>")`: a `nil` append was counted before and would now vanish, changing `#t`.
- Remove the first element equal to `v` (old `_del`): a numeric search for the first `i` with `t[i] == v`, then `table.remove(t, i)`; the 0/1 result is kept where a caller reads it (the registry's unregister reads it to decide whether to queue the entity). Equality stays `==`, as today (entities, need tables and particles by identity, ids by number, H-1.3-1).
- Empty a list in place (old `_clear`, `_free`, `_init` or `_init_reserve` on an existing header, the registry's `forget_list`, the emitter's particle release, a project's `inputs` reset): `for i = #t, 1, -1 do t[i] = nil end` on the same table. The old operations kept the header's identity, so every holder saw the emptied list; emptying in place keeps that. A field that was *replaced* by a new zero header (`self.ephemerals = entity_id_list_zero()` at UI release, `planet.needs = needs` in the save loader) stays a replacement with a new array.
- Refill a list in place (old `pointer_list_assign(dst, src)`, used for a planet's needs by the project install and by the bonus-resource offer): empty `dst` in place, then append every element of `src` in order. `dst` keeps its identity: the offer's second walk reads a local that is the same table as `self.needs` and must see the refilled list (the new bonus need first). `src` is dropped right after in both places, so no sharing is lost.
- No shared list helper module replaces the deleted ones: loops are written where they are used (the decision says `collect` becomes local loops). `table.insert` and `table.remove` from the standard library may be used. A module that empties or searches several lists may keep one local function for it.

### Traversal order and traversal under mutation

- Order is unchanged everywhere: ascending index, which is insertion order, with removals shifting the following elements down exactly as `_del` did. No walk is reversed, sorted or short-circuited differently.
- Every walk that used `_all`/`_iter_next` (the four `entity_geom` walks, the economy's refresh, new-year and net-income walks, the `for_each_tagged` helpers of the economy and its tiers, the colony-upkeep filter, the pause-menu test, the slipway lookups of `pad_slipway` and `slipway`, the needs walks of `planet_display`, `planet_trade` and `planet_family_trade`, the slipgate offer walk, the trade rebuild walk over a planet's slipways, the two research display walks, the hover dispatch and ephemerals clearing of `ui`, the two wormhole walks over the live entities, and the walks inside the deleted filter, find-first, concat, map and contains helpers) is classified by the implementer of its chunk:
  1. **The body cannot change the walked list.** Proven by reading the body and, for any body that calls a class method or another module's function, by the temporary instrument below. Written as `for i = 1, #t do local e = t[i] ... end` (for an id list, `local e = entity_id.entity_id_resolve(t[i])`). A numeric `for` reads `#t` once, which is the same as `last_count` when nothing changes.
  2. **The body changes the list only by removing the current element and then leaving the walk** (`planet_offer_trade`'s bonus walk: `del` then `break`). Written as a numeric `for`, with the removal at the current index and the `break` right after: identical.
  3. **The body may change the list in any other way** (by reading, or because the instrument fired). Written as a walk over a copy taken just before the loop (`local snapshot = {}` filled by a numeric loop, not `unpack`), with a short comment saying the walk tolerates changes to the list. Then `make e2e`: if every hash holds, done. If not, the implementer tries deferring the removal through the exit queue where the change is a removal. If neither keeps every hash, the chunk stops and reports that the phase-0 decision cannot be carried out for that walk (plan rule), naming the walk, what changed the list, and what the old iterator did that neither form reproduces.
- **The temporary instrument**, used in each chunk on every walk of classes 1 and 3 whose body calls out of the walking function: right after each body run, assert that `#t` and the element at the current index are what they were before the body ran, and assert at each append site that the value is not `nil`. The implementer runs `make e2e` once with the instrument, records which walks it fired on (expected: none), removes it, and writes the result in the chunk's commit message. Nothing of it is committed.
- What the old iterator did, for reference when classifying: after a step whose body lowered the count, it read the same index again (right when the current element was removed; it re-visited the current element when a later one was removed); after a raise, it went on to the next index and visited appended elements. A copy reproduces the first case and nothing else; that is why class 3 is checked by the hashes.
- The walks already written as index loops over a live length with a "removing an element during this walk skips the next one" note (the entity update over the live entities, the emitter's particle walk, the slipway's transit-dot walk) keep a `while i <= #t do ... i = i + 1 end` with the same increment after a removal: the skip, and the next-frame visit it causes, are kept. Their notes stay (a trap someone would reintroduce). The save loader's reverse walk removing bursts and its structure-icon refresh walk keep their direction and bound reading, 1-based.
- The draw walk over the layer order is not a list of the four kinds and is not touched (its mutation assert stays).

### Copies of a list header

Every place that copied a header by value is decided here. The old copy shared the items table with its source and kept its own count, until either side grew; it saw neither side's later appends nor clears, but did see a removal's shift on the shared table. No removal ever happens on the lists copied below (checked at HEAD: no `_del` on ephemerals or route lists), so a copy of the array taken at the same moment is exactly the old behavior.

- `ui_set_ephemerals(self, list)`: stores `list` itself. Every caller passes a list nobody else keeps (`ui_ephemerals_new()`'s result, or the message box's local list, never used after the call).
- `ui_update`'s `ephemerals_before`: a copy of `self.ephemerals` taken at the same point (its length and membership are read later, after the hover pass).
- The trade offer's route list, in `planet_rebuild_trades` and in `slipgate_offer_trade`: the offer gets a copy of the source list before the `offer_trade` call (`lw.accepted_routes`, `gf.accepted_routes`); after the call, the source field is assigned the offer's list itself (`self.outgoing_routes = lw.accepted_routes`, `trade_offer.accepted_routes = gf.accepted_routes`). During the call the source keeps its old contents, as the old header copy did; after it, the offer is dead, so assigning the array is the old copy back.
- `pointer_list_assign`: an in-place refill, above.
- A route's `planets` and `slipways` arrays: shared between a route and its copies (`copy_route`, `trade_new` sharing the keys' arrays), as today; they are never changed once built, which is what makes the sharing safe. `planet_rebuild_trades` keeps building new key arrays at each iteration.
- The copy-on-grow `grown_items` and the one-level `copy_value` go with their modules. `trade_route_list_add` keeps copying the route it stores (one `copy_route`, which already is a new table; the second copy `value_list` made is dropped: nothing else holds that table).

### 1-based only for these lists

- Only the lists of the four kinds and the exit queue become 1-based. The data tables shaped `{ items, count }` stay 0-based until step 3.2: layout rows and their keys, integer sequences of `game_data`, `map_tables` data rows, `tech_gate_available_technologies`'s and `research_display_available_projects`'s results, the score panel rows, the registry's layer order and its `{ items, first, count }` views, the save state's arrays.
- Each boundary between a now 1-based list and a 0-based table converts explicitly, in the chunk of the list: the invented technologies into the tech-tree menu row (`items[i].text = invented[i + 1]`), into the save (`out.invented[i]` from `invented[i + 1]`) and into the state dump; route planets and slipways into save indices; a saved 0-based node index into a registry bucket (`bucket[index + 1]`); the pad selection's wrap-around (`cint.mod(start + step, n)` over 0-based positions, read at `+ 1`); the pad panel's candidate cursor over the ephemerals and the hit-test bucket (starts at 1, stops past `#t`); the economy tiers' "slipway `k`, planet `k + 1`" pairing inside a route (same offset, both 1-based).
- The state dump writes the same lines in the same order: only its reads change.

### The modules

- **`engine.value_list` deleted** with `value_list_copy` (unused). `game.trade`: a trade route list is a plain array of route values (the `{ values }` wrapper goes, a list field holds the array). `trade_route_list_add(list, route)` stays and appends `copy_route(route)`. `trade_route_list_init`, `_free`, `_clear`, `_count` and `_at` go; callers use `{}`, emptying in place (the incoming routes' per-pass reset, the first `init` of outgoing routes and the save loader's), `#list` and `list[i]`. `trade_new`, `trade_copy`, `trade_extend`, `trade_equal`, `trade_last_planet` keep their contracts.
- **`engine.pointer_list` and `engine.collect` deleted.** `collect_filter_pointer_list` (the unmet-needs filter of the inspection panel), `collect_concat_pointer_list` (the bonus need prefix) and `collect_contains_string` (the tech gate's "is invented" test over `economy.invented`) become local loops calling their callbacks in the same order and the same number of times. `collect_map_pointer_list` (unused) goes. `collect_format_signed` moves to `game.economy_formulas` as `economy_formulas_format_signed`, same signature and results (text, then color: `"+"` and the given color for a non-negative value, color 8 otherwise, suffix appended), called by the economy, the planet, the planet display and the score panel.
- **`engine.entity_id_list` deleted.** `engine.entity_id` gains `entity_id_resolve(id)`: `entity_get(id)`, asserting the result is not `nil` with the message "a game id list holds no dead id: remove it where the entity dies". Every resolved read of an id list (old `_at` and `_iter_next`) calls it. `_has` becomes a local loop with `==` that keeps returning 0/1 where a caller tests it (`~= 0`, until step 3.1); `_count`, `_concat` and `_filter` (unused outside the module) go. `trade`'s `id_list_extended` builds a plain array.
- **`engine.entity_list` deleted.** The registry's live list, per-message buckets and per-tag lists are plain arrays; `entities_by_tag` returns the array or `nil` for a tag never seen, `entities_by_message` the array, `live_entities` the array, `entity_registry_count_tagged` uses `#`. Register appends, unregister removes the first equal element (queuing the entity only when it was in the live list), shutdown, init and forget-registries empty every bucket in place. `entity_list_filter`, `_concat` and `_find_first` become local loops at their callers (colony upkeep count, pad jump and pad slipway candidates, slipway obstruction, probe targeting track), keeping every predicate call: the slipway obstruction still filters every node, then every slipway, before taking the first element of the two.
- **`engine.exit_queue`**: a plain 1-based array of entities (the `{ entity }` slot tables go). `EXIT_QUEUE_CAP`, its fatal and `exit_queue_count` (no caller, H-1.3-4) go. `exit_queue_put` keeps its `nil` assert and appends. `exit_queue_flush` reads the length once, releases elements `1 .. n` in order (id release, then class release, as now), then empties the whole array, including anything a release queued during the flush, which is dropped unreleased as today (a trap comment says so, 40 words at most). Call sites in the registry and the cart are unchanged.
- Names are otherwise unchanged (module prefixes stay until step 3.4). Predicates keep their 0/1 results (step 3.1). `-- Copy of` headers of the modules touched stay (step 3.3); a header comment describing a deleted shape (`{ items, count, capacity }`, "indexed from 0", `last_count`) is reworded or dropped per the comments rule.

### Docs

- `docs/love-port/design.md`, **Arrays and integers**: "A C array keeps its C index, from 0" gains the exception: the game's lists (registry buckets, id lists, needs, routes, particles, the exit queue) are plain 1-based Lua arrays without holes; data tables stay 0-based until the 1-based step. **Structs and out-parameters**: "(`entity_id_list_copy` shares `items`)" goes. **Memory**: the copy-on-grow sentence and the `copy_value` sentence are replaced by: a list held by several owners is emptied or refilled in place; a walk whose body may change its list walks a copy and says so; a list that outlives a copy point is copied, never shared by accident. Any other sentence naming a deleted module, `last_count`, `pointer_list`, `entity_list`, `value_list` or `collect` is fixed in the same chunk.
- `CLAUDE.md` § The game names no list kind (H-1.3-8): unchanged unless a sentence became false.
- The `tdd` skill's examples that call `entity_list` functions (in its tests and mocking notes) are rewritten on a module that still exists, keeping what each example teaches.
- No e2e note, `.json`, `.sha256` or `.end-state` is touched.

### Decided without `guideline`

- Snapshot over alias for every header copy whose source lives on: it is exactly the old behavior when no removal touches the copied list, which is the case for all of them, so no reasoning about who reads the source during an offer is needed. The cost is one small array per offer and per frame, far under the e2e budget.
- Emptying in place rather than assigning `{}`: the old operations kept the header's identity and the registry buckets, needs and particles are held by other code.
- The instrument is temporary and evidence-only: committing it would make `make e2e` slower and leave dead checks.
- `collect_format_signed` goes to the economy formulas, the module of money figures, already loaded by the game and already unit-tested.
- The exit queue capacity goes: not a game rule (phase-0 decision); the 1.3 PRD handed it to this step.
- The data tables shaped `{ items, count }` stay: they are not built by the four list modules, match nothing in the step's check, and step 3.2 converts every remaining 0-based array.

## Implementation Chunks

Each chunk is verified, before its commit, by `make e2e` green with every
`.json.sha256` unchanged, `make test` green, its restricted check, and the
instrument result recorded in its commit message. A chunk converts every
list of its kind and every caller of it, never leaving a list half
converted.

1. **`value_list` and the trade route lists.** `engine.value_list` deleted; `game.trade`'s route list as decided; every caller of the route list functions (planet, planet trade with the offer copies, planet family trade with the slipgate offer copies, economy tiers, economy score, planet display, save load, save state). Route `planets`/`slipways` stay id lists for chunk 3. Restricted check: `value_list.lua` is gone and `grep -rE 'value_list|trade_route_list_(init|free|clear|count|at|zero|value)|(list|routes)\.values\b' game/` finds nothing (`.values` alone also names data-table fields, which stay).
2. **`pointer_list` and `collect`.** Both modules deleted; needs, project inputs, invented technologies, particles, transit dots, the inspection panel's lists, the bonus prefix and the save loader's needs as plain arrays; the in-place refill of needs; `economy_formulas_format_signed` and its unit test; the tech gate's contains loop; the invented boundaries (menu row, save, state dump). Restricted check: both files are gone and `grep -rE 'pointer_list|engine\.collect|collect_(filter|map|concat|contains|format)' game/` finds nothing.
3. **`entity_id_list`.** Module deleted; `entity_id_resolve`; planet slipways, route planets and slipways, UI ephemerals (with the `ui_set_ephemerals` and `ephemerals_before` decisions) and hover layers, wormhole anchors, the message box's list, the save loader's route id lists, `entity_geom_nearest_in_ids`, the pad panel cursor, the economy tiers pairing. Restricted check: the file is gone and `grep -r entity_id_list game/` finds nothing.
4. **`entity_list` and the registry buckets.** Module deleted; the registry as decided; every reader of a bucket (`entity_geom`, economy, economy score, economy tiers, bootstrap's update walk, background, menu, pad build, pad jump, pad panel, pad select, pad slipway, pad target, probe, save load, save state, slipway, state dump, UI, wormhole). Restricted check: the file is gone, `grep -rE 'engine\.entity_list|entity_list_' game/` finds nothing and `grep -r last_count game/` finds nothing.
5. **The exit queue and the docs.** `engine.exit_queue` as decided; `design.md` and the `tdd` skill examples. Full check: the step's check (the five files gone, `grep -r last_count game/` empty), `grep -rE 'EXIT_QUEUE_CAP|exit queue full|\.items\[|\.count\b' game/` lists only data tables, views and save arrays named under "1-based only for these lists" (the implementer reads the list, it is not expected empty), `make test-all` green, `make e2e end-state` passing with every `.json.sha256` and `.end-state` unchanged, `make e2e` time recorded.

## Testing Decisions

- A good test here is the replays: the step changes the shape of every list, not what the game does, so the net is every `.json.sha256` unchanged after each chunk and every `.end-state` unchanged at the end. Lists reach the captures (draw and hover order), the end states (planets, slipways, structures, techs in bucket order) and the saves (needs, routes, invented technologies), so a wrong index, a lost element or a changed order shows.
- The temporary instrument of each chunk turns "this walk's body cannot change its list" from a reading into evidence over the 20 replays, without making `make e2e` slower once removed.
- `economy_formulas_format_signed` gets cases in the existing `tests/lua/game/test_economy_formulas.lua` (a positive value with a suffix and the given color, zero, a negative value with color 8 and no `+`): no new LuaJIT run, so `make test` is no slower. Prior art: that file's `check.expect` cases.
- No other new test: each other function deleted is a list primitive replaced by a Lua built-in or a loop; a new test file would make `make test` slower (forbidden), and the contracts that matter (order, identity, copies) are visible only in game play.
- `make test` catches a leftover `require` of a deleted module in about a second (`lua-test-modules` requires every module of `game/`), and `lua-guard` a leftover call to a deleted local helper.
- No test under `tests/` or `tools/` names a list module (checked at HEAD); none changes.
- Each chunk's red-to-green is its restricted check: false before, true after.
- Prior art: steps 1.2 and 1.3, chunked under the same e2e net.

## Out of Scope

- The data tables shaped `{ items, count }`, the layer order and its views, the save state's arrays, every other 0-based array (step 3.2).
- Booleans (step 3.1): `_has`-style tests and predicates keep 0/1.
- The `has` bits, including the `PLANET_HAS_NEEDS`, `PLANET_HAS_*_ROUTES`, `EMITTER_HAS_PARTICLES` and `UI_HAS_OWN_EPHEMERALS` bits next to these lists (step 2.1).
- Module prefixes in names (step 3.4), `-- Copy of` headers (step 3.3).
- Fixing the quirks kept on purpose: the skipped element after a removal in the update, particle and transit-dot walks, and the entities dropped unreleased when queued during a flush.
- Entity ids and the id table, beyond the new `entity_id_resolve` (step 1.3, done).

## Further Notes

### Hypotheses on step 1.3

This PRD was asked for before step 1.3 was done. While it was being
written, step 1.3 was committed (`4392683`, `d9565aa`, step note `9ad90b8`),
so each hypothesis below was checked against that commit and holds there;
they are kept as hypotheses because the PRD was planned without them.

- **H-1.3-1**: an entity id is a plain Lua number; an `entity_id_list` stores the id itself, and its `_del` and `_has` compare with `==`.
- **H-1.3-2**: `entity_get(nil)` returns `nil`; `entity_id_of(e)` returns `nil` for an entity without an entry; `entity_id_link(nil)` returns `nil` and asserts on an unconstructed entity.
- **H-1.3-3**: `entity_id_list` is otherwise as at step 1.2: `{ items, count, capacity }` with 0-based items, copy-on-grow, the `last_count` iterator, and `_at` asserting "a game id list holds no dead id: remove it where the entity dies".
- **H-1.3-4**: the exit queue is unchanged except a comment: a 0-based array of `{ entity }` slots, `EXIT_QUEUE_CAP` 315 with its fatal, `exit_queue_count` called by nobody, a flush that releases the id then calls the class release for `0 .. count - 1` and then sets the count to 0.
- **H-1.3-5**: the registry's buckets are still `entity_list` headers; `ENTITY_TABLE_CAP` and the "layer order full" fatal are gone; `entity_registry_set_layer` tests `entity_id_of(e) == nil`.
- **H-1.3-6**: `engine.entity_id_type` and every local `entity_id_none()`/`entity_id_copy` helper are gone; the UI still has `entity_id_list_zero` and `entity_id_list_copy`; the planet trade and planet family trade modules still have `trade_route_list_value`, and the planet and planet trade modules `pointer_list_assign`.
- **H-1.3-7**: step 1.3 changed no list operation or call order: every walk, header copy, append and removal listed here is where it was at step 1.2.
- **H-1.3-8**: `design.md`'s **Structs** paragraph still names `entity_id_list_copy` sharing `items`, its **Memory** paragraph copy-on-grow and `copy_value`; `CLAUDE.md` § The game says "plain number ids (`nil` for none)" and names no list kind.
- **H-1.3-9**: `make test-all` and `make e2e end-state` were green at the end of step 1.3, and `make e2e` ran in 5.2 s.
- **H-1.3-10**: predicates that returned an id-list result still return 0/1 and their callers test `~= 0`.

If H-1.3-1 or H-1.3-2 were false, chunk 3's `entity_id_resolve` and `==` searches follow whatever the id is. If H-1.3-4 were false (the queue already plain), chunk 5 has only the docs. If H-1.3-6 or H-1.3-7 were false, the implementer lists the sites as they stand (`git grep` of the list function names and of `.count`/`.items` on the fields named above) and applies the same decisions to them. If H-1.3-8 were false, the docs update fixes whatever sentence still describes the old lists. If H-1.3-9 were false, step 1.3 is not done and this step does not start.

### Other notes

- The step note records: the step's check, the instrument results per chunk (which walks, if any, fired and which form they took), and how long `make e2e` took.
- Scale at HEAD: about 25 game modules use the four kinds; 25 walks through `_iter_next`; about 60 direct `.items`/`.count` reads on these lists (many more on data tables, which stay); five header-copy sites. Chunk 4 is the widest; if it proves too big for one cycle, the implementer splits it along module lines with the registry first, never leaving a bucket reader on `.items` once the buckets are arrays (it would read `nil` at once, which the replays show).
- Expected effect on speed: no iterator table per walk and no `pairs` copy on growth, a few small snapshot arrays added; `make e2e` should stay around 5 s.
