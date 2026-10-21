---
title: "game/engine/ the registry, entity geometry, entities (with game data and map tables)"
description: "The Lua game has no entity registry, entity geometry, entity construction, game data or map tables, so no game object can be registered, drawn by layer or built from its archetype in any later step."
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
> How Phase 3 applies design.md's timing rule 4: each step from 3.3 to 3.14
> measures the own times of `lua-guard` and `lua-test-modules`, paired
> against its "before" commit, and passes when each stays under 100 ms and
> grows within 2× of 3.1's cost per KB and per table; both figures go in its
> note. The full rule-4 protocol runs at the end of 3.3, 3.9 and 3.15, against
> `8dff812` and the "before" commit of the span since the last checkpoint,
> and after any step whose own-time gate fails; a failure picks what to rework
> from the recorded own times.

The step:

> 3.3. `game/engine/` the registry, entity geometry, entities, bootstrap.
>    On the `entity_geom` / `entity_registry` pair, this step applies 2.1's
>    include-cycle convention; its note extends it only for a partner's value
>    used at load time (constants, tables) and says whether a cycle file can
>    still fail to load before its partners exist. Later cycles, 3.6 to 3.9
>    first, follow both.

## What the checklist assigns to this step

`luajit tools/checklist.lua --listing` (authoritative, per the Phase 3 header and step 1.1's note) gives step 3.3 **10 files, 56 functions, 386 tables**:

- `src/engine/entity.{c,h}` → `game/engine/entity.lua`: 7 functions, no table.
- `src/engine/entity_geom.{c,h}` → `game/engine/entity_geom.lua`: 11 functions (one a header inline, `entity_geom_contains_cursor_inline`), no table.
- `src/engine/entity_registry.{c,h}` → `game/engine/entity_registry.lua`: 26 functions, 2 tables (`main_camera_placeholder_class_def`, `g_main_camera_placeholder`).
- `src/game/game_data.{c,h}` → `game/game/game_data.lua`: 6 functions, 215 tables.
- `src/game/map_tables.{c,h}` → `game/game/map_tables.lua`: 4 functions, 169 tables.

Two departures from the step's title, both from step 1.1's note: engine `bootstrap` is **not** here (it includes `ui`, `scroll`, `mouse`, so it moved to 3.9), and `game_data` and `map_tables` **are** here (`entity_registry.c` includes `game/game_data.h`, which includes `game/map_tables.h`). The two data files hold 384 of the plan's 529 tables: the bulk of the plan's data.

## Hypotheses on step 3.2 (checked against its note)

Step 3.2's code is committed (`86ceed9`, `e32c1e7`, `6be0b7e`): `game/engine/{entity_id_type, entity_class, entity_id, entity_id_list, entity_list, exit_queue}.lua`, their reference units, `TABLE_REFERENCE_SOURCES` extended. Those modules, their function names and shapes are facts (the entity header fields and `entity_init_header`, `{ index, generation }` ids, `entity_list_t` as `{ items, count, capacity, zone }`, iterators returning `1, e` / `0, out`). Its commits changed none of `tests/lua/test_modules.lua`, `tools/lua_guard.lua`, `tools/table_reference.c`, `tools/checklist.lua`, `tools/checklist.txt`: also a fact.

This PRD was written while 3.2's last chunk (costs, timing, note) was running, on the hypotheses below. The `### Step 3.2` note then landed (`d88bb89`) before the PRD was finished; each hypothesis is marked with what the note says. If anything in the note is later read differently, the implementer follows the note and records the difference in the step 3.3 note.

- **H-3.2-1 — conventions.** The note records 3.2's conventions as the committed code shows them (ids built new at every by-value site, the entity header and its zero value, owned inline vectors, statics, iterators), with nothing that changes how this step writes structs, pointers, out-parameters or statics. *Confirmed:* "Step 3.1's conventions held unchanged"; ids are new tables wherever the C creates, returns or stores one by value, shifts inside lists included; positions copied in and out; `entity_init_header` writes only header fields.
- **H-3.2-2 — cost model holds.** 3.2's own times of `lua-guard` and `lua-test-modules` stay within 2× of 3.1's model (`lua-guard` 2.4 ms + about 0.10 ms per KB; load 2.1 ms + about 0.043 ms per KB; data about 17 µs per table + 0.21 µs per line). *Confirmed:* `lua-guard` about 30 ms (+1.3 ms), `lua-test-modules` about 20 ms (+1.4 ms), within 2×.
- **H-3.2-3 — timing gate passed, nothing moved out.** 3.2's protocol run passes and its note assigns no move-out under design rule 5 to step 3.3 or earlier; the large-table comparison is still in `lua-test-modules`. *Confirmed, with a warning:* paired medians +2.59 % vs `78d0d19` and +2.94 % vs `8dff812`, under 3 %, "mostly noise, but the margin against `8dff812` is thin". Nothing moved out. This step's rule-5 plan (below) is therefore a live contingency, not a formality.
- **H-3.2-4 — assignment unchanged.** The note changes no file assignment: step 3.3 stays at 10 files, 56 functions, 386 tables. *Confirmed* (the note says nothing on assignment; the checklist reports those counts).
- **H-3.2-5 — no cycle rule yet.** 3.2 had no include cycle and its note adds nothing on cycles: the convention to extend is still 2.1's text, and no skip mechanism exists in `lua-test-modules`. *Confirmed.*
- **H-3.2-6 — note place.** The note is `### Step 3.2` in plan-2.md § 6, after `### Step 3.1`; step 3.3's note follows it. *Confirmed.*

## Problem Statement

Every game object of the C game is constructed by taking an entity id and registering itself; the registry keeps the live list, the per-message lists (draw, draw overlay, hit test, new year, refresh), the per-tag lists and the per-layer draw order, and `draw_layers` walks that order every frame through the main camera; entity geometry answers "on screen?", "under the cursor?", "nearest?" and applies screen shake; and every archetype, kind record, menu, score row, technology and resource table lives in `game_data.c` and `map_tables.c`. After step 3.2 the Lua game has entity headers, ids, lists and the exit queue, but none of this. No game module of steps 3.4 to 3.15 can be written without the registry and the archetype tables, and plan 3 has no Lua function or table to put next to its C original when a replay diverges on a draw order, a registration, a hover test or a data value.

## Solution

Five new Lua modules, faithful copies of their C files: `game/engine/entity_registry.lua`, `game/engine/entity_geom.lua`, `game/engine/entity.lua`, `game/game/map_tables.lua`, `game/game/game_data.lua`. They follow 2.1's, 3.1's and 3.2's conventions (H-3.2-1, confirmed) and add the few this step needs: the include-cycle extension for a partner's value used at load time, how a large const data file is written in Lua (constructors for zero-filled structs, every table a module field), and how a pointer into the middle of an array is carried. They join the three Phase 3 checks with no new target: the wildcard `LUA_GUARD_FILES` picks them up in `lua-guard` and `lua-test-modules`; their constants and all 386 tables join the C reference through new units in `tools/table_reference/` and their sources in `TABLE_REFERENCE_SOURCES`. `make checklist STEP=3.3` passes: 10 files, 56 of 56 functions, 386 of 386 tables, every table compared. The step measures the data check's cost on the largest batch of tables of the plan and records it, with the cycle convention, in a `### Step 3.3` note.

## User Stories

1. As a game step implementer, I want `entity_construct(self)` to take an id and register the entity, as the C does, so that every game object of later steps is born the same way.
2. As a game step implementer, I want `entity_init_instance(self, cls, state, t)` to write the header and then the state and state time, so that archetype-based constructors of later steps can call it unchanged.
3. As a game step implementer, I want `entity_set_state` to do nothing for a `nil` entity and reset `state_time` to fixed 0 otherwise, so that state machines behave as in C.
4. As a `debug_state()` author (3.15), I want `entity_state_name` to return the C names for every state and `nil` for `ENTITY_STATE_NONE`, so that the state dump prints the same text.
5. As a game step implementer, I want `entity_init`, `entity_shutdown` and `entity_ready` with the C flag semantics (1/0), so that the cart's boot and teardown order can be copied.
6. As a game step implementer, I want `entity_registry_init(reserve)` to accept `nil` as the C accepts `NULL`, and to reserve each list from the game zone with the reserve's counts, so that zone usage matches the C byte for byte.
7. As a game step implementer, I want `entity_register` to add the entity to the live list, to each message list whose member the class has, to the draw and overlay layer orders by floored layer, and to each tag list of the class's tags, in the C order, so that iteration orders match the C.
8. As a game step implementer, I want `entity_unregister` to put the entity in the exit queue only if it was live, and to remove it from every list and layer order it is in, so that deaths match the C.
9. As a game step implementer, I want `entities_by_tag` to return `nil` for a tag never registered and the list otherwise, and `entity_registry_count_tagged` to return 0 for such a tag, so that game code testing presence behaves as in C.
10. As a game step implementer, I want `entities_by_message` to assert, with the C message, that the registry was initialised, so that an early read fails loudly in Lua as in C.
11. As a game step implementer, I want `entity_registry_set_layer` to set the layer bit and value, and to re-sort the draw order only for a constructed entity whose floored layer changed, by registration rank, so that draw order after a re-layer matches the C.
12. As a game step implementer, I want `draw_layers(message)` to walk layers 1 to 15, apply the main camera's `apply_camera` with the layer fade before each non-empty draw layer, skip off-screen entities of classes with `skip_draw_off_screen`, and call `draw(e, entity_pos(e))`, so that frames are drawn in the C order.
13. As a game step implementer, I want `draw_layers` to assert that no draw method mutates the layer order and that no layer was written directly, with the C messages, so that a Lua port bug fails at its cause.
14. As a game step implementer, I want `entity_registry_layer(message, layer)` to return the members of one layer in registration order, so that any code reading one layer sees the C order.
15. As a game step implementer, I want `layer_order_insert` to fail through `p8_fatal` with `layer order full, limit 512` when full, so that the overflow message is the C one.
16. As a game step implementer, I want `entity_registry_shutdown`, `entity_registry_abandon_live_entities` and `entity_registry_forget_registries` with their C effects on lists, the id table, the generation counter and the main camera, so that restarts and save loads behave as in C.
17. As a game step implementer, I want `entity_registry_generation` to change on every register, unregister and reset and to wrap as an unsigned 32-bit counter, so that caches keyed on it are invalidated as in C.
18. As a game step implementer, I want `main_camera()` to return the placeholder entity after init and the camera once set, and `main_camera_placeholder_class()` to return the placeholder class table by identity, so that `scroll.c`'s identity test on the class ports unchanged.
19. As a game step implementer, I want `entity_registry_hit_test_always` to return 1, so that classes using it as their hit test port unchanged.
20. As a game step implementer, I want the `ENTITY_MESSAGE_*` constants and `ENTITY_REGISTRY_LAYERS` as module fields with the C values, compared with the C compiler, so that later modules use the C names.
21. As a game step implementer, I want `entity_geom_contains_cursor` and its inline body to choose the screen or world mouse position by layer, subtract the entity position with 32-bit wrap, and test the hit box, so that hover matches the C.
22. As a game step implementer, I want `entity_geom_is_on_screen` to use the main camera's position and the 12-pixel margin, so that off-screen culling matches the C.
23. As a game step implementer, I want `entity_geom_shake` to assert the shake bit with the C message, decay the shake by fixed 0.1, clamp at 0, and offset by `vec_from_polar`, so that screen and planet shake match the C bit for bit.
24. As a game step implementer, I want `entity_geom_nearest`, `entity_geom_nearest_except`, `entity_geom_nearest_in_ids` to return the best squared distance and the nearest entity (or `nil`) as an extra return value, and `entity_geom_any_within` to return 1/0, with the C tie and same-position rules, so that targeting matches the C.
25. As a game step implementer, I want every archetype, kind record, sequence, data row, menu table and score row of `game_data.c` as a Lua table with the C field names and values, so that planets, structures, probes, slipways, wormholes, UI and panels of later steps read the same data.
26. As a game step implementer, I want `int_seq_at`, `game_data_fixed_seq_at` (1-based index, C bounds, value as an extra return), `int_seq_has_fixed`, `planet_types_lookup`, `game_data_route_cost_factor` and `game_data_route_length_factor`, so that the C call sites port unchanged.
27. As a game step implementer, I want every resource code, resource name, production table, project record, icon tooltip and technology table of `map_tables.c` with the C field names and values, and the four lookups with their C asserts and `nil` results, so that the economy and techs of later steps read the same data.
28. As a game step implementer, I want the enum counts (`UPKEEP_BY_TIER_COUNT` … `TECHNOLOGIES_COUNT`) and `DRAW_FUNCTION_*` as module fields compared with C, so that loops and dispatch use the C names and values.
29. As a game step implementer, I want a pointer field in a data table (an archetype's `ring_colors`, a row's `items`, a kind record's `archetype`) to be the very table it points to, so that identity tests and shared reads behave as the C pointers do.
30. As a game step implementer, I want every field of every const struct present at its C value, zero included, so that `item.is_false ~= 0` and arithmetic on a field never meet an unexpected `nil`.
31. As a plan 3 debugger, I want each data table under its C name, in C order, so that I can put a Lua table next to its C original when a replay diverges.
32. As a plan 3 debugger, I want each Lua function in the C order with the C name and computation, so that a divergence can be bisected function by function.
33. As a maintainer, I want `lua-test-modules` to compare all 386 new tables with the C ones, nested tables and pointers included, so that a typo in the hand-ported data fails `make test`.
34. As a maintainer, I want the C reference to fail its build when a data pointer targets a table it does not name, so that an unregistered table cannot hide behind a pointer.
35. As a maintainer, I want `make checklist STEP=3.3` to pass with every table compared, so that the step is provably complete.
36. As a maintainer, I want the include cycle `entity_geom` / `entity_registry` to load in either order, each file alone, so that the load check covers both at this step.
37. As a later cycle implementer (3.6 to 3.9), I want a written rule for a partner's value needed at load time, and whether a cycle file can fail to load before its partner exists, so that I cut later cycles without guessing.
38. As a maintainer, I want the own time of `lua-guard` and `lua-test-modules` measured after this step and compared with 3.1's model, so that the plan's largest data batch proves the projection or triggers rule 5.
39. As a maintainer, I want the own-time gate of both checks paired against `d88bb89`, and, since 3.3 is a checkpoint, the full rule-4 protocol against `8dff812` and `d88bb89`, so that `make test` provably did not slow by more than 3 %.
40. As a maintainer, I want the step's conventions, counts, costs and timing in a `### Step 3.3` note, so that step 3.4 builds on recorded decisions.
41. As a maintainer, I want no new `make test` target and no change to the three checks' scripts, so that the Phase 3 contract holds.
42. As a maintainer, I want the Lua data files no larger than they need to be, so that `lua-guard`, the check to watch per 3.1's note, stays cheap.
43. As a reviewer, I want each compile-time check of the C data (`planet_ring_colors_count_matches`) as a load-time `assert`, so that a miscount fails at load.

## Implementation Decisions

### Modules and requires

- Lua files: `game/engine/entity_registry.lua`, `game/engine/entity_geom.lua`, `game/engine/entity.lua`, `game/game/map_tables.lua`, `game/game/game_data.lua`. The four headers the checklist marks "no-lua" (`entity.h`, `entity_registry.h`, `game_data.h`, `map_tables.h`: no function or table of their own) contribute their constants and struct shapes to their pair's module, header first, then `.c`, as 2.1 says.
- Requires follow the C includes; a require used by no line is not written; an include for types only gives no require.
  - `map_tables`: nothing (its header includes `p8/fixed.h` for the type alone; the lookups only assert).
  - `game_data`: `game.map_tables` (data row constructors), `engine.entity_class` (state constants used by the data at load), `p8.vram` (`P8_SCREEN_WIDTH`, `P8_SCREEN_HEIGHT` in initializers), `p8.cint` (integer `/` in initializers), `p8.fixed` (functions). `engine.geom` and `engine.vec` only if a line calls them (box and vector values in data are literal tables with the C field names).
  - `entity_registry`: at load `engine.entity_class`, `engine.entity_id`, `engine.entity_id_type`, `engine.entity_list`, `engine.exit_queue`, `engine.game_zone`, `engine.zone_sizes`, `game.game_data`, `p8.fatal`, `p8.fixed`; `engine.entity_geom` **at call time** (cycle).
  - `entity_geom`: at load `engine.entity_class`, `engine.entity_list`, `engine.entity_id_list`, `engine.vec`, `engine.geom`, `p8.vram`, `p8.fixed`; `engine.entity_registry` **at call time** (cycle).
  - `entity`: `engine.entity_class`, `engine.entity_id`, `engine.entity_registry`, `p8.fixed`, at load (no cycle: `entity_registry` does not include `entity.h`).
- Build order: `map_tables` (a leaf), `game_data`, then the cycle pair, then `entity`.

### The include cycle (2.1's convention, extended; recorded in the step 3.3 note)

- **Pair.** `entity_registry.c` includes `entity_geom.h` (for `entity_geom_is_on_screen` in `draw_layers`); `entity_geom.c` includes `entity_registry.h` (for `main_camera` in `entity_geom_is_on_screen`). Both uses are calls. Each module requires the other inside the function that needs it (2.1), once per call at the top of that function, not inside a loop: `draw_layers` requires `engine.entity_geom` once before its layer loop.
- **Extension, a partner's value used at load time.** (a) A partner's constant used only inside function bodies is read at call time through the call-time require (`require("engine.entity_registry").ENTITY_REGISTRY_LAYERS`); no module-level local caches a partner's value. (b) A partner's value a module-level initializer needs (a constant in a static table, a table a data table points to) is obtained by requiring that partner at load. This is allowed in one direction only per pair: the partner then reaches back only at call time, and both modules' top comments name the direction. (c) If a pair needs load-time values in both directions, the step that meets it records how it cuts the pair in its note; this step meets none.
- **Can a cycle file still fail to load before its partners exist?** Yes, under (b) only: the module that requires its partner at load fails while the partner's file is not written, so when such a pair is cut across steps, its load check runs at the cycle's last step (Phase 3 header). The first step that needs this adds the mechanism (a list of modules `lua-test-modules` does not require yet, emptied at the cycle's last step); this step needs none. Under (a) alone, each file of a cycle loads alone, in any order.
- **This step's pair.** Neither file needs a partner value at load time (`entity_geom` uses `P8_SCREEN_*` from `p8.vram`; `entity_registry`'s statics are its own), so both load alone, in either order, and `lua-test-modules` loads both at this step with no skip.

### Conventions added by this step (recorded in the step 3.3 note)

- **Every data table a module field.** In `game_data.lua` and `map_tables.lua`, every file-level table, `static` or not, is written `M.<c name> = ...` and referenced as `M.<c name>` inside the module. Reason: LuaJIT allows 200 locals per function, and the main chunk would declare 215; also `lua-test-modules` reaches a table only through its module field (`gfx_software.lua` exports its statics for the same reason). C order is kept, so a table is defined before any table pointing to it.
- **Zero-filled const structs through constructors.** A struct type whose const instances leave fields to C zero (designated initializers) gets one constructor taking the given fields and filling every other field with its C zero value: 0 for an integer or fixed, `nil` for a pointer or string, a new zero table for an inline struct (`targeting`, `emitter`, `hit_box`, `pos`, `row_step`, `spawn_offset`…), a full 0-indexed zero array for an inline array (`production_tables_entry_t.values` has four elements even when `count` is 3). `data_item_t` and `data_row_t`, declared in `map_tables.h` and used by both files, get Lua-only constructors exported by `map_tables.lua` after the C functions, marked Lua only, named `map_tables_make_data_item` and `map_tables_make_data_row`; every other constructor is a `local function` of the module that owns the type. A constructor builds a new table each call; nothing mutates a const table afterwards. Struct types whose initializers give every field positionally may use a plain table literal with the C field names.
- **Values in data.** Fixed values are the C raw literals (`0x48000`, `-0x5540`); expressions are computed as the C does (`vram.P8_SCREEN_WIDTH - 10`, `cint.div(vram.P8_SCREEN_HEIGHT, 2) - 38`); states are `entity_class.ENTITY_STATE_*`; `draw_function` is `M.DRAW_FUNCTION_*`; C escapes become their characters (`"\?"` is `"?"`). C arrays are 0-indexed (`[0] = ...`), including item arrays, sequences' `items`, `planet_mood_rows_rows`, `score_rows_table`.
- **Pointers in data.** A pointer to a table, or to the first element of an array, is that very table (`mood_rows = M.planet_mood_rows_rows`, `items = M.galaxy_menu_1_layout_row_1_items`, `archetype = M.structure_archetype`); `NULL` is `nil`. A `const char *` pointing to a named `char[]` table is the Lua string (strings have no identity in Lua; no line of `src/` compares these pointers by identity, `grep` shows none; the steps porting the readers of `required_tech` and `class_name` keep it so). A lookup returning `&table[i]` returns the element table.
- **A pointer into the middle of an array.** `entity_layer_view_t` is `{ items, first, count }`: `items` the layer order's 0-indexed items array, `first` the index the C pointer points at (2.1: a pointer into an array is an integer index; the view keeps its array beside it), readers index `items[first + i]`. Only `draw_layers` reads a view.
- **Unsigned counters.** `g_generation` and `g_layer_order_mutations` are numbers in 0 … 2^32−1 incremented as `(x + 1) % 4294967296`, the C unsigned wrap.
- **Moves within an array.** `memmove` of the layer order is a loop over the C element count, upward for a removal, downward for an insertion; `memset` of `start` sets indices 0 … `ENTITY_REGISTRY_LAYERS` to 0. No `SIZEOF` entry is needed: no byte count reaches a zone.
- **Function-local statics.** `entity_state_name`'s `names` and `entity_registry_init`'s `no_reserve` are module locals built once at load (H-3.1-5 / 3.1's statics rule): `names` a 0-indexed array with `nil` at `ENTITY_STATE_NONE`; `no_reserve` `{ live = 0, by_message = 0-indexed zeros, by_tag = 0-indexed zeros }`. The checklist does not count them as tables, so they are not compared.
- **Registry statics.** `g_entities_by_message` a 0-indexed array of `ENTITY_MESSAGE_COUNT` list tables, `g_entities_by_tag` of `TAG_COUNT` `{ seen = 0, list = <list table> }`, each list at its C zero value (`items = nil`, `count = 0`, `capacity = 0`, `zone = nil`); `g_layer_orders` a 0-indexed pair of `{ items = 0-indexed array, start = 0-indexed zeros 0 … 15 }`; `g_live_entities` a list table; `g_main_camera` starts `nil`, `g_initialized` 0, counters 0.
- **The placeholder camera.** `main_camera_placeholder_class_def` is an `entity_class_t` at its C zero value (every function field `nil`, `states` an empty table, `tags`, `lineage`, `skip_draw_off_screen` 0). `g_main_camera_placeholder` is one table (3.1's first-member casts: the cast to `entity_t *` is the identity) holding the header at its zero value with `class` the placeholder class, built at load by `entity_class.entity_init_header`. Both are module fields so `lua-test-modules` compares them; `main_camera_placeholder_class()` returns the class table by identity.
- **Truth values and calls.** Class members are tested by presence (`cls.draw ~= nil`); `has_refresh(e)` and `skip_draw_off_screen` are C ints tested with `~= 0`; `e.class.tags & TAG(i)` is `bit.band(...) ~= 0`; `e.has |= ENTITY_HAS_LAYER` is `bit.bor`. The `switch` of `is_message_name` is an `if` chain in C case order. Every `assert` keeps its C message.
- **Out-parameters.** `game_data_fixed_seq_at(seq, count, i, out)` and `int_seq_at` return `found, value`; `resource_code_lookup(key, out)` returns `found, resource`; `entity_geom_nearest*` and `nearest_step` return `best, nearest`, `nearest` reset to `nil` first as the C does; `entity_list_iter_next` / `entity_id_list_iter_next` are used as 3.2 wrote them.
- **Fixed wrap.** `(p8_fixed_t)((uint32_t)a - (uint32_t)b)` in the inline cursor test is `bit.tobit(a - b)`, the computation `fixed.p8_fixed_sub` performs.
- **Compile-time checks.** `planet_ring_colors_count_matches` becomes a load-time `assert` that the items array has `PLANET_RING_COLORS_COUNT` elements (counted from index 0).
- **How the data files are produced.** The implementer may convert `game_data.c` and `map_tables.c` to Lua, and produce the C unit's name list, with throwaway scripts (not committed, as 3.1's agents did for behaviour checks). The committed Lua files are ordinary sources, not "generated data" (no make target, no stale-copy check): the C files are hand-maintained sources too, and the data check is what keeps the two equal while the C exists.

### The data check for this step

- **Constants.** New units `tools/table_reference/game_map_tables.c` (the six counts, the seven `DRAW_FUNCTION_*` including `DRAW_FUNCTION_COUNT`), `game_game_data.c` (the fourteen counts), `engine_entity_registry.c` (the six `ENTITY_MESSAGE_*` including `ENTITY_MESSAGE_COUNT`, `ENTITY_REGISTRY_LAYERS`). Each unit includes its `.c` (for the statics); the three `.c` sources join `TABLE_REFERENCE_SOURCES` (H-3.1-9 / 3.1's rule), which drops their own objects from the reference link.
- **Tables.** Every one of the 386 tables is one block, rendered by the C unit field by field in the Lua rendering's spelling (`NAME[3].field`, `NAME.row.items`): every integer and fixed field as `table_reference_int` (zero included), every string field as `table_reference_string` (nothing for `NULL`), every function field as `table_reference_function`, every pointer as `table_reference_pointer` with the pointed table's C name (nothing for `NULL`), inline structs and arrays expanded. A `char[]` table is one line holding its string.
- **Shared C renderers.** A header beside `reference_backend.h` (say `reference_data.h`) renders `data_item_t`, `data_row_t` (inline or as a table) and `int_seq_t`; each unit renders its own struct types (archetypes, kind records, menu and score entries, project records, technologies…), one function per type.
- **Pointer names.** Each unit lists its tables once (an X-macro list of name and kind, in C order), which drives both the rendering of every block and an address → name map; a pointer whose target is not the start of a listed table makes the reference build fail with the pointer's path. The checklist's "ported but not compared" cross-check catches a table missing from the list.
- **No change** to `tests/lua/test_modules.lua`, `tools/table_reference.c`, `tools/lua_guard.lua`, `tools/checklist.lua`, `tools/checklist.txt` or their C tests; the step only feeds them. Only `TABLE_REFERENCE_SOURCES` changes in the Makefile.

### Cost and timing

- **Projection** (3.1's model, H-3.2-2): about 386 tables × 17 µs ≈ 6.6 ms, plus roughly 6,000 – 12,000 lines × 0.21 µs ≈ 1 – 3 ms, added to `lua-test-modules`; about 80 – 100 KB of Lua (data files kept near the C's 70 KB by the constructors) adds about 8 – 10 ms to `lua-guard` and 4 ms of load. Both checks stay far under the 100 ms budget of design rule 2.
- **Own-time gate** (the Phase 3 header's application of rule 4, steps 3.3 to 3.14): at the end, the own wall times of `lua-guard` and `lua-test-modules` are measured paired against the step's "before" commit, `d88bb89` (the end of 3.2). The step passes when each check stays under 100 ms and its growth stays within 2× of what 3.1's cost per KB (Lua added) and per table (tables added, lines added) predicts. Both figures, the KB, table and line counts, and the predicted growth go in the note. The end-of-plan projection of 3.1's note is redone (this step lands 386 of the 469 tables it counted).
- **Full protocol** (design rule 4, a checkpoint step: the header names 3.3, 3.9 and 3.15): runs at the end of 3.3 whatever the own-time gate says, against `8dff812` and the span's "before" commit, `d88bb89`, 11 rounds (22 if needed), recorded as step 2.1's note does (plain and paired medians, spreads, rounds, cores, the slowest new check's own time).
- **On a failure** (either gate; 3.2 left the margin against `8dff812` at +2.94 %, H-3.2-3, so this is a real possibility): what to rework is chosen from the recorded own times, as the header says. If `lua-test-modules` carries the growth, per 3.1's note the large-table comparison moves out first under design rule 5: the blocks of `game.game_data` and `game.map_tables` leave `lua-test-modules` for a separate locked target outside `make test-all` (the reference split by module), `make test` keeps the load check and every other block, `design.md` says so, and the note records it. If `lua-guard` carries it, the data files are shrunk first (constructors, no repeated literals) before anything moves. The protocol is rerun after the rework. No new `make test` target is ever added.

### Docs

- `docs/love-port/plan-2.md` § 6 gains `### Step 3.3` after `### Step 3.2` (H-3.2-6): the cycle convention extension (rules a, b, c and the answer on loading), the conventions above, the three reference units and the shared renderer, `make checklist STEP=3.3` counts, the own-time gate figures, the redone projection and the full protocol record, and every place where 3.2's note differed from H-3.2-k.

### Questions

- No Agent tool was available in this run, so no `guideline` agent was spawned. Each question met was settled from the plan, `design.md`, the 3.1 and 3.2 notes and PRD, and the code, and is recorded above: every data table a module field (LuaJIT's 200-local limit); zero-filled constructors rather than every field written out (file size, `lua-guard` cost); strings for `char[]` tables; the view as `{ items, first, count }`; the cycle rule's one-direction load-time require and the deferred skip mechanism; throwaway conversion scripts with the data check as the guard; the move-out target if the budget breaks.

## Implementation Chunks

1. **Map tables.** `map_tables.lua`: the six counts, `DRAW_FUNCTION_*`, the 169 tables in C order as module fields, the four lookups with their C asserts, the two Lua-only row and item constructors. The shared renderer header and the unit `game_map_tables.c` (constants, every table, the name list and pointer map); `src/game/map_tables.c` in `TABLE_REFERENCE_SOURCES`. Green: `lua-guard`, `lua-test-modules` (all `game.map_tables` blocks equal), the checklist rows of `map_tables`.
2. **Game data, sequences, rows and menus.** `game_data.lua` up to the archetypes (C lines up to `wormhole_too_far_target_status_text`: tier tables, layer fades, period names, planet types, galaxy and pause menus, difficulty options, score rows, sequences, data rows of planets, probes, help screen and score panel), the fourteen counts, the load-time assert, the six functions. Unit `game_game_data.c` with those tables; `src/game/game_data.c` in `TABLE_REFERENCE_SOURCES`. Green: `lua-test-modules` for every block registered so far.
3. **Game data, archetypes and kind records.** The rest of `game_data.lua`: `protostar_burst_construct_keys`, the 21 archetypes (local constructors for their zero-filled struct types, inline `targeting` and `emitter`), the required-tech and class-name strings, the nine kind records; their renderers in `game_game_data.c`. Green: the checklist rows of `game_data`, all 215 tables compared.
4. **Registry and entity geometry (the cycle).** `entity_registry.lua` (26 functions, the statics, the two placeholder tables, the message constants and `ENTITY_REGISTRY_LAYERS`) and `entity_geom.lua` (11 functions), each requiring the other at call time; unit `engine_entity_registry.c`, `src/engine/entity_registry.c` in `TABLE_REFERENCE_SOURCES`. Green: both load alone, in either order (a throwaway check), `lua-test-modules`, their checklist rows.
5. **Entity.** `entity.lua` (7 functions). `make checklist STEP=3.3` passes from here (10 files, 56/56, 386/386, all compared).
6. **Cost and record.** The own-time gate (both checks paired against `d88bb89`, under 100 ms, growth within 2× of 3.1's per-KB and per-table costs), the redone projection, the full rule-4 protocol against `8dff812` and `d88bb89` (3.3 is a checkpoint), rework only on a failure, the `### Step 3.3` note.

## Testing Decisions

- A good test checks external behaviour only: a check's verdict on the Lua files, and the equality of Lua constants and tables with the C ones. No test looks at how a Lua function is written.
- Tested: the five modules through `make checklist STEP=3.3` (presence of every file, function and table, and every table compared), `lua-guard` (compile, no undeclared global, no `love.` use), `lua-test-modules` (load under plain LuaJIT, every registered constant and all 386 tables equal to C, nested tables and pointers included). A TDD cycle goes red on a checklist row, a load failure, or a missing or differing reference line, and green when the faithful copy is in place. The data check is the real test of this step: the bulk of its work is 384 hand-ported tables.
- No behaviour test for the registry, geometry or entity modules, as steps 3.1 and 3.2 decided for theirs: Phase 3 steps are verified by the checklist and the three checks, the e2e replays of plan 3 are the source of truth (`CLAUDE.md`), and the timing margin is mostly spent (1.2's and 2.1's notes). The implementer checks the registry's layer order (register, re-layer, unregister, `draw_layers` order) and the cycle's load in both orders with throwaway scripts, as 3.1's agents did.
- No change to `tests/lua/test_modules.lua`, `tools/lua_guard.lua`, `tools/table_reference.c` or their C tests.
- Prior art: `tools/table_reference/p8_gfx_software.c` (a unit including a `.c` for its statics, one helper per table shape) and `reference_backend.h` (a shared renderer of a struct of function pointers); `engine_entity_class.c` (enum constants); `game/p8/gfx_software.lua` (statics exported as module fields for the check); `game/engine/pointer_list.lua` and `entity_list.lua` (lists and iterators); `game/p8/draw_cost.lua` (a by-value constant struct).

## Out of Scope

- Engine `bootstrap` (step 3.9, per step 1.1's note) and every game module that reads these tables (steps 3.4 onward): planets, probes, slipways, techs, economy, UI, panels.
- Behaviour of the classes the registry calls (`draw`, `apply_camera`, `release`…): they arrive with their modules.
- `debug_state()` (step 3.15), which will read `live_entities`, the registry and the game data.
- Running any e2e replay or `make love-e2e` (plan 3).
- Behaviour tests of these modules, and any redesign of the registry or the data for Lua (plan 5).
- Changes to the three checks' scripts, a skip mechanism in `lua-test-modules` (not needed by this step), or a new `make test` target.
- Audio (out of scope for the whole port).

## Further Notes

- The large-table comparison is named by 3.1's note as the first thing to move out under rule 5; this step is where it would show, since it lands about 82 % of the plan's tables. The projection says it will not be needed; the step measures rather than assumes.
- Struct shapes for later steps: `entity_layer_view_t` `{ items, first, count }`; `entity_registry_reserve_t` `{ live, by_message, by_tag }` (0-indexed arrays); `int_seq_t` `{ count, items }`; `data_row_t` `{ count, items, has_color, color, draw_function, has_resource, resource }`; `data_item_t` `{ text, number, row, is_false, is_fixed, fixed, is_nil }`. Each module's top comment lists the shapes it defines, as `entity_class.lua` does.
