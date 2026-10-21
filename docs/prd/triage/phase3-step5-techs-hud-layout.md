---
title: "Plan 2 step 3.5: techs, HUD and layout in Lua"
description: "The Lua game lacks the faithful copies of the C techs, trade routes, economy formulas, HUD, draw functions, layout rows, tooltips and row images that every later game step requires."
status: needs-triage
---

## Problem Statement

Plan 2 (`docs/love-port/plan-2.md`) writes the whole game in Lua under `game/` as a faithful copy of the C game, one Phase 3 step per group of C files, in include-graph order. Steps 3.1 to 3.4 are done: the engine, the data files (`map_tables`, `game_data`) and the game leaves (bindings, mouse, image caches, generation) load and pass their checks. The next group in the include graph has no Lua copy: the technology gate, research projects, trade routes, the economy formulas, the HUD primitives, the draw-function rows, the layout rows and tooltips, and the row-image cache. Steps 3.6 to 3.10 (galaxy, planets, economy, slipways, UI, displays) all require these modules, so nothing past this point can be written until they exist, and plan 3 cannot set a Lua function beside its C original for any of them.

The step, verbatim from the plan, with the Phase 3 header paragraphs that define its verification and timing:

> ### Phase 3 — Engine and game
>
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
>
> 3.5. `game/game/` techs, HUD and layout.

## Solution

Ten Lua modules under `game/game/`, each the faithful copy of one C pair of `src/game/`, as the checklist assigns them to step 3.5 (19 C files, 130 functions, 5 tables; per step 1.1's note the include graph moved `trade`, the economy formulas, `row_image` and `draw_functions` into this step):

| C files | Lua module | Functions | Tables |
|---|---|---|---|
| `main_economy.h` | `game.main_economy` (constants only) | 0 (its functions are `probe.c`'s, step 3.8) | 0 |
| `economy_formulas.{h,c}` | `game.economy_formulas` | 6 | 0 |
| `trade.{h,c}` | `game.trade` | 17 | 0 |
| `tech_gate.{h,c}` | `game.tech_gate` | 8 | 0 |
| `research.{h,c}` | `game.research` | 26 | `research_project_class_def` |
| `data_init.{h,c}` | `game.data_init` | 2 | 0 |
| `hud.{h,c}` | `game.hud` | 12 | `palette_blocks` |
| `draw_functions.{h,c}` | `game.draw_functions` | 14 | `draw_functions_by_id` |
| `layout.{h,c}` | `game.layout` | 37 | `layout_row_class_def`, `tooltip_class_def` |
| `row_image.{h,c}` | `game.row_image` | 8 | 0 |

When the step is done, `make checklist STEP=3.5` passes (130/130 functions, 5/5 tables), every module loads under LuaJIT with its `require`s, `lua-guard` finds no undeclared global, `lua-test-modules` finds the five tables, the step's header constants and its `SIZEOF` entries equal to the C ones, and the own-time gate of `lua-guard` and `lua-test-modules` passes and is recorded in a step 3.5 note in the plan.

## User Stories

1. As the plan-3 debugger, I want `game/game/tech_gate.lua` to hold every function of `tech_gate.c` in C order with the C names, so that I can put any of them beside its C original when a replay diverges.
2. As the plan-3 debugger, I want `research.lua`, `trade.lua`, `economy_formulas.lua`, `hud.lua`, `draw_functions.lua`, `layout.lua`, `row_image.lua` and `data_init.lua` to follow the same rule, so that every function of the step has one obvious Lua twin.
3. As the author of step 3.6 (galaxy and planets), I want `research_projects_find`, `research_project_cost` and `research_project_production` to exist, so that planets can name their project.
4. As the author of step 3.7 (economy), I want `resource_map_get`, `resource_map_set`, `economy_formulas_fill_resource_map`, `economy_formulas_route_base_cost` and `economy_formulas_cost_scale` to exist with the C arithmetic, so that the economy's numbers stay bit-exact.
5. As the author of step 3.7, I want `ECONOMY_HAS_PRODUCTION` and `ECONOMY_HAS_DEMAND` in a `game.main_economy` module, so that the economy and the tech gate read the same bits.
6. As the author of step 3.8 (slipways), I want trade routes (`trade_new`, `trade_extend`, `trade_copy`, `trade_equal`, the route list) to exist, so that slipways can build and compare routes.
7. As the author of step 3.9 (UI and message box), I want layout rows, tooltips and the draw-function rows to exist, so that message boxes can draw their articles.
8. As the author of step 3.10 (displays and panels), I want `hud_draw_bar`, `hud_print_outlined*`, `hud_draw_help_frame` and the palette blocks, so that panels draw as the C does.
9. As the plan-3 debugger, I want `palette_blocks` compared with the C table by `lua-test-modules`, so that a wrong colour swap can never come from a typo in the table.
10. As the plan-3 debugger, I want `draw_functions_by_id` compared with the C table (which slots hold a function), so that a row with a draw function always dispatches to a function in Lua when it does in C.
11. As the plan-3 debugger, I want the three class tables (`research_project_class_def`, `layout_row_class_def`, `tooltip_class_def`) rendered by the shared class renderer, so that a missing `draw_at` method fails `make test`.
12. As the plan-3 debugger, I want the header constants of the step (`RESOURCE_MAP_CAP`, the `RESOURCE_NIL`/`RESOURCE_FALSE`/`RESOURCE_FIXED` kinds, the layout item, cell and scalar kinds, `LAYOUT_ROW_TUPLE_CAP`, `LAYOUT_ROW_KEYS_CAP`, `RESEARCH_PROJECT_ID_NONE`, `ECONOMY_HAS_*`) compared with the C values, so that no enum is renumbered by hand.
13. As the plan-3 debugger, I want every `sizeof` the step's code passes to an allocator to be a compared `SIZEOF` entry, so that zone fill levels and "zone full" fatals match the C byte for byte.
14. As the plan-3 debugger, I want a research project's id to carry the C epoch with its 16-bit wrap, so that a stale id returns `nil` exactly when the C returns `NULL`.
15. As the plan-3 debugger, I want `research_build_projects` to build the projects in the record order of `project_records_table`, with the header entries setting `required_tech`, so that project indexes match the C.
16. As the plan-3 debugger, I want a project's input codes to make one need per character, a non-resource character making a need with no resource, so that need counts match the C.
17. As the plan-3 debugger, I want `tech_gate_available_technologies` to write the affordable flag of every entry before any early return, so that `tech_gate_affordable_flags` reads back what the C reads back.
18. As the plan-3 debugger, I want the research cost discount (threshold 0.55, demand of the technology's icon resource) computed through `fixed.lua`, so that tech costs are bit-exact.
19. As the plan-3 debugger, I want `economy_formulas_route_base_cost` to clamp at 0.5 and scale by the slipway count's square root exactly as the C, so that route costs are bit-exact.
20. As the plan-3 debugger, I want `resource_map_set` to fail with the C assert message when a seventeenth key arrives, so that the trap stays visible in Lua.
21. As the plan-3 debugger, I want `trade_resource` to return a new `resource_t` table each call, with the C kinds, so that callers comparing resources use `resource_equal` and never table identity.
22. As the plan-3 debugger, I want `trade_route_list_add` to store a copy of the route even when the route was read from the same list, so that the C's "copy before the add" trap holds.
23. As the plan-3 debugger, I want `trade_equal` to compare first and last planets as the C does, by entity identity, so that route deduplication matches.
24. As the plan-3 debugger, I want `hud_draw_bar` to set the pen colour before the rectangle, so that later default-colour draws use the same colour as the C.
25. As the plan-3 debugger, I want `hud_print_outlined` to align text with `a * 4 * strlen` in fixed point, so that centred and right-aligned text lands on the C pixel.
26. As the plan-3 debugger, I want a draw-function row's number argument to print through `p8_tostr_fixed` on the frame zone, so that a numeric first argument prints as the C prints it.
27. As the plan-3 debugger, I want layout rows to measure text as `strlen * 4 + 3` and sprites from their tuple (width, palette block, spacing), with the last spacing removed from the width, so that every row lays out on the C pixels.
28. As the plan-3 debugger, I want each fill of a layout row to take a new, never-zero 32-bit serial, so that row caches keyed by serial behave as in C.
29. As the plan-3 debugger, I want tooltips to draw their patterned bar and their lines through the text-image cache, centred, so that a tooltip's pixels match.
30. As the plan-3 debugger, I want `row_image_draw_at` to pick, build and stamp row slots as the C does (same key test, least-recently-used slot), so that the image-slot contents match the C at every frame.
31. As the plan-3 debugger, I want the row-image key hash computed with the C FNV-1a arithmetic modulo 2^32, so that the cache's shortcut test reads like the C one.
32. As the step implementer, I want each module's top comment to name its C files and list the shapes of its structs with the C field names, so that the next step reads a module without opening the C.
33. As the step implementer, I want the step cut into chunks in dependency order, so that each TDD cycle ends with modules that load.
34. As the step implementer, I want every decision this PRD takes recorded here, so that I never need to ask anyone.
35. As the maintainer, I want `make test` to stay within the timing rules, so that the fast loop stays fast.
36. As the maintainer, I want the own-time figures of `lua-guard` and `lua-test-modules` recorded in a step 3.5 note, so that the projection to the end of the plan stays current.
37. As the plan pilot, I want `make checklist STEP=3.5` to pass at the end of the step, so that I can close the step mechanically.

## Implementation Decisions

All conventions of the plan's step notes apply unchanged: 2.1's C-to-Lua conventions, 3.1's engine conventions, 3.3's data-file conventions and include-cycle extension, 3.2's id rules, 3.4's class renderer. The decisions below are what this step adds. No `guideline` agent was reachable (no agent tool in the PRD writer's session); every point was settled here.

### Modules and their requires

- One Lua module per C pair, named after the C file, required by path (`game.layout`), returning `M`, functions in C order (header, then `.c`), exported as `function M.<c name>(`, statics as `local function <c name>(`.
- A local holding `p8.gfx` is `gfx_module`, since the C parameter is named `gfx` (2.1's rule; `text_image.lua`'s rename of the parameter is not the model).
- Load-time requires go only to modules already written (steps 1.2 to 3.4) or earlier in this step's order: `main_economy`, `economy_formulas`, `trade`, `tech_gate`, `research`, `data_init`, `hud`, `draw_functions`, `layout`, `row_image`. There is no include cycle inside the step.
- **The main economy.** `main_economy.h` declares `main_economy_typed`, `main_economy_placeholder` and the rest, but `probe.c` defines them, and the checklist puts them in `game.probe` (step 3.8). `game.main_economy` holds only the header's two macros, `ECONOMY_HAS_PRODUCTION` and `ECONOMY_HAS_DEMAND` (3.1's rule: a header without a `.c` whose macros Lua needs gets its own file); its top comment says where the functions live. `tech_gate` reads the economy through `require("game.probe")` inside the functions that need it, at call time, never at load: `tech_gate` and everything calling it (`research_project_cost`, `research_project_production`) load now but cannot run until step 3.8 writes `game.probe`. This is not an include cycle, so no load check is deferred: every module of the step loads at this step.
- `economy_formulas` reads the slipway list through `entity_registry.entities_by_tag(entity_class.TAG_SLIPWAY)` (step 3.3) and the factors through `game_data` (step 3.3).

### Shapes (listed in each module's top comment)

- `resource_map_t`: `{ count, entries }`, `entries` 0-indexed of `{ resource, value }`; it is filled in place by its init and set functions (2.1's "init on `T *`").
- `resource_t`: by value, a new `{ kind, number }` table at every creation; kinds are the module constants `RESOURCE_NIL`, `RESOURCE_FALSE`, `RESOURCE_FIXED` of `game.trade`.
- `trade_route_t` and `trade_keys_t`: `{ planets, slipways, resource, has_resource, resource_is_bool }`. A value copy of a route (`trade_new`, `trade_copy`'s `copy = *self`, `trade_route_list_add`'s local copy, the value list's element copy) is a new table with these five fields, and its two `entity_id_list_t` tables are **shared**, not copied: the header's contract says the id lists are immutable once built and value copies share their items, so sharing the struct table is the same thing in Lua. `id_list_extended` builds new lists, as in C; an empty one is a zeroed `entity_id_list_t` table with no zone. Ids stored in the lists follow 3.2 (new tables, compared through `entity_id_equal`).
- `trade_route_list_t`: `{ values }`, a `value_list_t` of `SIZEOF.trade_route_t`; `trade_route_list_at` returns the stored table (valid until the next add, as in C).
- `available_technologies_kept_t`: `{ items, count }`, `items` 0-indexed, each the very `technologies_table` entry.
- `research_project_id_t`: by value, `{ index, epoch }`, both 16-bit; `RESEARCH_PROJECT_ID_NONE` is a module table `{ index = 0, epoch = 0 }`, copied at each use as 3.2 does for `ENTITY_ID_NONE`, and registered in the reference as `.index` and `.epoch`.
- `need_t`: `{ row, has_satisfied, satisfied }`, `row` a `data_row_t` built with `map_tables.map_tables_make_data_row` (every field at its C zero), allocated as a block of `SIZEOF.need_t` on the load zone.
- `research_project_t`: header fields (3.1's first-member rule) plus the C fields, `inputs` an inline `pointer_list_t` table.
- `layout_row_item_t`: `{ kind, text, color, sprite_id, sprite_palette_block, offset }`.
- `layout_row_scalar_t`: `{ kind, value }`. `layout_row_tuple_t`: `{ count, scalars }`, `scalars` 0-indexed. `layout_row_cell_t`: `{ kind, as }`, the union `as` a table holding the member the kind uses (`fixed`, `text` or `tuple`). Scalars, tuples and cells are by-value structs that no code mutates after its constructor: each constructor returns a new table, and passing or storing one shares it (2.1: never mutate a table received by value). `row_measure_item`'s reassignment of its `cell` parameter is a new local table.
- `layout_row_keys_t`: `{ items, count, has_color, color, has_background, background_is_bool, background, has_score, score, has_pos, pos }`, `items` 0-indexed; `= {0}` is a local constructor listing every field at its C zero.
- `layout_row_t`: header fields plus `{ zone, color, has_color, background, has_background, background_is_bool, score, has_score, placed_items, placed_items_count, width, serial }`. `tooltip_t`: header plus `{ zone, rows, rows_count }`.
- `row_image_key_t`: `{ mem, hash, count, extent, entries }`, `entries` 0-indexed; `row_image_slot_t`: `{ key, stamp }`. The statics `g_slots` are a 0-indexed array of `IMAGE_SLOT_ROW_COUNT` slot tables with every field at its C zero (`mem` nil), as `text_image.lua` does.

### Arithmetic and identity traps

- **Pointer differences.** `t - technologies_table` (`tech_gate`) and `project - g_project_table` (`research_project_id_of`) become indexes found through a Lua-only reverse map (entry table → C index), built at load from the 0-indexed array; the C asserts keep their messages and fail when the table is not in the map or its index is out of range. `g_affordable_flags` is a 0-indexed array of `TECHNOLOGIES_COUNT` zeros; the reset sets each to 0.
- **The project table.** `g_project_table` is a 0-indexed array of `PROJECT_RECORDS_COUNT` tables created once at load; `project_create` reuses the table at its index and its `memset` sets every field to its C zero on that same table (the inline `inputs` table keeps its identity and is zeroed too), as the C reuses the bytes. `research_project_get` returns that table or `nil` as the C returns the address or `NULL`.
- **16- and 32-bit counters.** `g_project_epoch` wraps at 2^16 to 0, then is set to 1, as `uint16_t` does; the index is cut to 16 bits as the C cast does. `g_row_serial` wraps at 2^32 to 0, then 1. The row-image `g_stamp` (`unsigned long`) is a plain Lua number, exact to 2^53, as `text_image.lua` decided.
- **The row-image hash.** FNV-1a over `uint32_t` values: XOR and multiply by 16777619 modulo 2^32, the multiply split into 16-bit halves (the product passes 2^53), the result kept unsigned. The C mixes in the `p8_vram_t` address; Lua has no address, so the mem term is 0. This changes no slot choice: `same_key` also compares `mem` by identity and the entries one by one, so two keys are equal in Lua exactly when they are in C; the hash is only a shortcut.
- `entry_of` packs with `bit.bor`/`bit.lshift` and is brought back to unsigned where compared, per 2.1.
- `hud_select_palette_block`'s `memcpy` is a loop over the 16 entries of `mem.draw_palette`; `hud_palette_block_default_in_force` compares 16 entries and returns 1/0.
- `aligned_x` keeps the C order of fixed-point operations (`a * 4`, then `* strlen`), `strlen` stopping at a NUL byte as `text_image.lua` does.
- C `assert(gfx != NULL ...)` followed by `if (gfx != NULL)` in `hud_draw_bar` is kept as written.

### Out-parameters (2.1's rule)

- `resource_map_get(map, qe, out)` returns `found, out`, `out` unchanged when not found; callers pass their current value in.
- `trade_last_planet(self, out)` returns `found, out`.
- `row_measure_item(self, cell, out_item, out_w, out_spacing)`: `out_item` is the caller's table, filled in place after its `memset` (every field to its C zero); `out_w` and `out_spacing` come back as the two return values.
- `key_of(row, mem, key)` fills the caller's key table and returns 1/0; `slot_for(key, built)` returns `index, built`.
- `research_project_positions(self, ri, input_codes)` returns the new `input_codes`.
- `resource_code_lookup` already returns `found, value` (step 3.3); callers follow that.

### Blocks and sizes

- Every allocation keeps the C byte count through `M.SIZEOF` entries compared by `lua-test-modules`: `layout_row_item_t`, `layout_row_t`, `tooltip_t`, `layout_row_t *`, `trade_route_t`, `need_t`, and any other `sizeof` the code uses. `layout_row_construct` allocates `max(n, 1)` items and creates `placed_items[i]` as a new table before measuring it; `tooltip_construct` allocates `max(n, 1)` pointers. `row_copy_text` performs the C allocation of length + 1 bytes and returns the Lua string (3.1's zone-string rule). Address-only `sizeof`s (`sizeof g_project_table[0]` in `research_project_id_of`, the `memcmp` length in `same_key`) become loops or index lookups and need no entry.

### Function pointers and classes

- `draw_functions_by_id` is a module field (so the data check sees it), 0-indexed by the `DRAW_FUNCTION_*` constants of `map_tables`, `[DRAW_FUNCTION_NONE]` absent; `draw_rows_dispatch_one` chooses between `print_outlined` and the table with an `if`/`else` (3.3: never `a and b or c` between functions). `print_outlined_with` takes the print function as a parameter.
- The three class tables are module fields built like `mouse_class_def`: `research_project_class_def = { states = {}, tags = 0, lineage = 0, skip_draw_off_screen = 0 }`; `layout_row_class_def` and `tooltip_class_def` add their `draw_at` method. `layout.c` forward-declares `layout_row_class_def` before use: Lua forward-declares nothing for a module field read at call time, but the methods are local functions defined before the table that holds them.
- Methods read `dispatch_context.dispatch_context_gfx()` and `dispatch_context_mem()` as the C does.

### Reference units

- New units in `tools/table_reference/`, each added to `TABLE_REFERENCE_SOURCES`: `game_main_economy.c` (the two macros), `game_economy_formulas.c` (`RESOURCE_MAP_CAP`), `game_trade.c` (the resource kinds, `SIZEOF.trade_route_t`), `game_research.c` (includes `research.c`: class table through `reference_class`, `RESEARCH_PROJECT_ID_NONE`, `SIZEOF.need_t`), `game_hud.c` (includes `hud.c`: `palette_blocks` through the data helpers), `game_draw_functions.c` (includes `draw_functions.c`: one `table_reference_function` per slot of `draw_functions_by_id`), `game_layout.c` (includes `layout.c`: two classes, the kind enums, the caps, the layout sizes). `tech_gate`, `data_init` and `row_image` have no exported constant or table and get no unit; `row_image`'s and `layout.c`'s `.c`-local enums are module locals, not compared.

## Implementation Chunks

1. **Economy leaves.** `game.main_economy`, `game.economy_formulas`, `game.trade`, `game.tech_gate`, with the reference units for `main_economy`, `economy_formulas` and `trade`. Done when the four modules load, `lua-guard` and `lua-test-modules` are green and the checklist shows their 31 functions ported.
2. **Research.** `game.research` and `game.data_init`, the `research` reference unit (class table, `RESEARCH_PROJECT_ID_NONE`, `SIZEOF.need_t`). Done when both load, the class table compares equal, and a throwaway run of `data_init` builds the same projects (count, names, required techs, need counts, costs before any tech) as the C.
3. **HUD and draw functions.** `game.hud` and `game.draw_functions`, their two reference units (`palette_blocks`, `draw_functions_by_id`). Done when both load and both tables compare equal.
4. **Layout and row images.** `game.layout` and `game.row_image`, the `layout` reference unit (two classes, kinds, caps, sizes). Done when both load, the tables, constants and sizes compare equal, and `make checklist STEP=3.5` passes (130/130, 5/5).
5. **Own-time gate and step note.** Measure the own times of `lua-guard` and `lua-test-modules`, paired against the step's "before" commit (the commit before chunk 1; `06dec0b` when this PRD was written), as step 3.4 did (45 pairs). Pass when each stays under 100 ms and grows within 2× of 3.1's model (`lua-guard` about 0.10 ms per KB of Lua; load about 0.043 ms per KB, data about 17 µs per table plus 0.21 µs per line). 3.5 is not a checkpoint: no full rule-4 protocol unless the gate fails. Write the step 3.5 note in the plan: modules, checklist counts, reference units added, the decisions above that later steps rely on (shared id lists in route copies, the reverse-map pointer differences, the hash mem term, `tech_gate` needing `game.probe` at step 3.8), both own-time figures with KB, blocks and lines added, and the updated projection to the end of the plan. No agent runs during the measurement.

## Testing Decisions

- A good check tests what the plan says the step must give: the files and functions exist (checklist), they load (`lua-test-modules`), they use no global (`lua-guard`), their tables and constants equal the C ones (`lua-test-modules` against `tmp/table_reference.txt`). Nothing tests implementation details.
- No new `make test` target and no committed behaviour test, as in steps 3.1 to 3.4: plan 3's replays cover behaviour. The step adds only reference units and their table entries to the existing checks.
- Behaviour is checked against the C with throwaway programs, not committed, during the chunks: resource maps and route base cost for a set of positions and slipway counts (registry with no slipway gives count 0); route new, extend, copy, equal and the route list; the project build of `data_init`; layout rows built from the `map_tables` tooltip rows (placed items, offsets, widths, serials) and tooltips; `row_image_cacheable` on those rows. Functions that read the main economy cannot run before step 3.8 and are not exercised (a throwaway stub of `game.probe` may stand in, removed afterwards). Drawing functions are checked at most through `tmp/fake_love` scratch scenes, never committed.
- Prior art: the reference units of steps 3.3 and 3.4 (`game_map_tables.c`, `game_mouse.c`, `reference_class.h`, `reference_data.h`), `tests/lua/test_modules.lua`, and step 3.4's throwaway comparisons of the text and button caches.

## Out of Scope

- `probe.c` and the main-economy functions (step 3.8); the economy entity and its formulas beyond `economy_formulas.c` (step 3.7); trade and slipway displays (step 3.10).
- Running `tech_gate` or anything that reads the main economy before step 3.8.
- The e2e replays (plan 3) and any C change; the C unit tests are not ported.
- The full rule-4 timing protocol, unless the own-time gate fails.
- Audio.

## Further Notes

- C size of the step: 1,984 lines over 19 files; the Lua is expected around 60 KB, which at 3.4's rate adds about 5 ms to `lua-guard` and a few ms to `lua-test-modules` (five tables, about 300 lines of reference).
- `data_init` is the first Lua entry point that builds game state from the data files; step 3.14 (the cart) calls it.
- `research.c`'s comment that a project's layout row items point into the project itself concerns later callers; in Lua those items reference the same project table, which keeps its identity across `memset`, as the decision above requires.
