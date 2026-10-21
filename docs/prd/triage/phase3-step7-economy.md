---
title: "Plan 2, step 3.7: the include cycle, economy"
description: "Port economy, economy_score and economy_tiers to Lua as faithful copies, the second slice of the 18-module include cycle, all three loading before their 3.8-3.9 partners exist."
status: needs-triage
---

## Plan step (verbatim)

From `docs/love-port/plan-2.md`, Phase 3 — Engine and game:

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
> 3.7. `game/game/` the include cycle, economy.

## Problem Statement

Plan 3 will put any Lua function next to its C original when an e2e replay
diverges, so every C function and table of the game needs a faithful Lua
copy. Step 3.6 opened the 18-module include cycle with the galaxy and the
planets. The economy is still missing: there is no clock, no yearly income,
no spending, no tier computation, no scoring, and the already ported modules
that call into it (`planet_display` spends and advances the clock, `galaxy`
reads the difficulty, `tech_gate` reads the main economy's demand) call
modules that do not exist yet.

The checklist (`luajit tools/checklist.lua --listing`) assigns 6 C files to
step 3.7, with 43 functions and 2 tables, all MISSING:

- `src/game/economy.{c,h}`: 20 functions; `economy_class_def` (a zero class)
  and `economy_archetype` (four 16.16 values).
- `src/game/economy_score.{c,h}`: 14 functions; `final_score_t`; the static
  `button_t g_difficulty`.
- `src/game/economy_tiers.{c,h}`: 9 functions.

All three are members of the cycle (the strongly connected component of the
module include graph: background, economy, economy_score, economy_tiers,
galaxy, message_box, particle, the four planet modules, probe,
research_display, scroll, slipway, ui, ui_button, wormhole). The step must
settle, with evidence from the C, whether any of them cannot load before
their partners of 3.8 and 3.9 exist, in which case it joins
`LUA_DEFERRED_MODULES`.

## Solution

Three new Lua modules under `game/game/`: `economy`, `economy_score` and
`economy_tiers`, each a faithful copy of its C pair (header, then `.c`; same
functions in the same order, same names, same computations, same tables),
following the conventions of the step notes of 2.1, 3.1, 3.3, 3.4, 3.5 and
3.6. Their tables, sizes and constants are registered with the data check;
`make checklist STEP=3.7` passes (43/43, 2/2).

Load-check finding (the decision of this step): **all three modules load now,
under plain LuaJIT, with every 3.8-3.9 partner missing; none joins
`LUA_DEFERRED_MODULES`**, which stays `game.planet_family`. Evidence from the
C:

- The only file-level values the three modules build at load are
  `economy_class_def` (`= {0}` in `economy.c`: no method, so no partner
  function to hold), `economy_archetype` (four integer literals), and
  `economy_score.c`'s static `button_t g_difficulty` (zero-initialized, built
  by a local zero constructor, no partner value). `economy_tiers.c` has no
  file-level value at all.
- Every partner use is inside a function body: `toast_keys_set_text` and
  `toast_new` (`message_box`, 3.9) in `economy_spend`; `slipway_class` and
  `slipway_served_planets_add` (`slipway`, 3.8) in
  `rebuild_incoming_routes_route_visit`; `main_economy_typed` (`probe`, 3.8,
  per 3.5's note) in `economy_score_final` and `economy_score_happiness`;
  the planet functions (`planet_is_family`, `planet_production_read`,
  `planet_production`, `planet_is_colonized`, `planet_rebuild_trades`) and
  the sibling economy modules everywhere they are used. All are required at
  call time (2.1's cycle rule).
- In the other direction, no module of the tree or of steps 3.8 to 3.14
  holds an economy function in a table or compares one by identity: a search
  of `src/game/*.c` outside the three files finds only direct calls
  (`probe.c`, `slipway.c`, `planet_display.c`, `galaxy.c`, `cart.c`,
  `menu.c`, `save_load.c`, `save_state.c`, `score_panel_display.c`) and
  `probe.c`'s `e->class == economy_class()`, a call. So no later module needs
  an economy value at load, and no two-way load need arises.
- Inside the step, `economy` calls `economy_score` and `economy_tiers`;
  neither calls back. They are cycle members, so `economy` requires them at
  call time too (2.1's rule; 3.3's extension allows a load require only for a
  load-time value, and there is none).

## User Stories

1. As the plan-3 debugger, I want `game/game/economy.lua` to hold every function of `economy.h` and `economy.c` in C order with the C names, so that I can put any economy function next to its C original when a replay diverges.
2. As the plan-3 debugger, I want `game/game/economy_score.lua` to copy upkeep, trade income, happiness, final score and the difficulty copy exactly, so that a score or income divergence is traced line by line.
3. As the plan-3 debugger, I want `game/game/economy_tiers.lua` to copy the incoming-route rebuild, the tier computation and the production amounts exactly, so that a tier divergence points at the same lines in both languages.
4. As the plan-3 debugger, I want `economy_recompute` to loop on `economy_tiers_update_tiers` exactly as the C does (until no tier increase is reported), so that the Lua terminates and converges on the same tiers.
5. As the plan-3 debugger, I want the clock (`economy_add_ticks`, `economy_advance_clock`, `economy_tick_clock`) to wrap the year ticks at 12 or 15 by the same rule, so that new years fall on the same frame.
6. As the plan-3 debugger, I want `economy_new_year` to add income and science in the same order and notify the same new-year listeners with the same year, so that yearly toasts and planet updates match.
7. As the plan-3 debugger, I want `economy_spend` to raise the same toasts with the same texts and colours, so that "-no money-" and the spent amount appear identically.
8. As the plan-3 debugger, I want the end status ("final score", "bankrupt", none) set by the same thresholds, so that the game ends on the same frame.
9. As the plan-3 debugger, I want `economy_invent` to advance the era by the same tier thresholds, so that eras change after the same invention.
10. As the plan-3 debugger, I want the crowding penalty to start from 1 and subtract 0.15 per structure as the C does, so that research discounts match.
11. As the plan-3 debugger, I want `economy_score_percent_text` to perform the same zone allocation as the C, so that a frame zone overflow fires at the same moment.
12. As the plan-3 debugger, I want `economy_score_set_difficulty` to copy only the five fields the C copies into one table that keeps its identity, so that a pointer returned earlier by `economy_score_difficulty` sees the same values as in C.
13. As the plan author, I want `make checklist STEP=3.7` to pass (43/43 functions, 2/2 tables), so that the step is verifiably complete.
14. As the plan author, I want `economy_class_def`, `economy_archetype` and every `sizeof` the modules use compared with the C, so that the values cannot drift silently.
15. As the plan author, I want all three modules loaded by `lua-test-modules` now, so that a load error in them fails `make test` at this step, not at 3.9.
16. As the plan author, I want the evidence that none of the three needs a 3.8-3.9 value at load recorded in the step note, so that the deferred list stays short and justified.
17. As a later-step agent writing `probe` (3.8), I want to know that `economy_score` requires `game.probe` at call time for `main_economy_typed`, and that `probe` may require `game.economy` (for `economy_class`) at call time only, so that no load cycle appears.
18. As a later-step agent writing `slipway` (3.8), I want to know that `economy_tiers` calls `slipway_class()` and `slipway_served_planets_add(w.served_planets, planet_id)` at call time, so that `slipway` exports both with those shapes.
19. As a later-step agent writing `message_box` (3.9), I want the local `toast_keys_zero` of `economy` recorded next to `planet`'s, so that I can fold both into the owner.
20. As a later-step agent writing `ui_button` (3.9), I want the local `button_t` zero constructor of `economy_score` recorded, with the button's own `has` as `has_`, so that I can fold it into the owner.
21. As a later-step agent writing save, state and score code (3.10, 3.12, 3.15), I want the economy's own `has` field named `has_` and recorded, so that I read the economy bits from the right field.
22. As the maintainer of the already ported `tech_gate`, I want its read of the main economy's demand bit moved to `has_`, so that the discount reads the economy bits, not the entity header bits.
23. As a player of the Lua game (plan 3 onward), I want income, research points, credits and the year to evolve exactly as in the C, so that the Lua game plays the same.
24. As a player, I want development tiers and production amounts computed identically, so that planets upgrade at the same moments.
25. As a player, I want the final score, its tier and the happiness figure identical, so that the end screen reads the same.
26. As the maintainer of `make test`, I want the step's own-time gate measured (paired, 45 pairs, against the before commit), so that `lua-guard` and `lua-test-modules` stay under 100 ms and within 2× of 3.1's per-KB and per-table costs.
27. As the maintainer of `make test`, I want no new `make test` target and no new committed test, so that the fast loop does not grow beyond the files and tables the step adds.
28. As the reviewer, I want behaviour checked against the C with throwaway programs (recompute on a seeded galaxy, new years, spending, scoring, tiers), so that the copy is shown faithful without committing behaviour tests the replays will cover.
29. As the reviewer, I want each decision this step took without a human recorded in the step note, so that the next agent does not re-litigate it.

## Implementation Decisions

### Modules and their requires

- **`game.economy`** (copy of `economy.h` then `economy.c`): module-local
  `g_economy_up`; `M.economy_class_def` (`{ states = {}, tags = 0, lineage
  = 0, skip_draw_off_screen = 0 }`, as `research_project_class_def` writes
  a zero class); `M.economy_archetype` (`{ credits = 0x640000, era =
  0x10000, year_ticks = 0x10000, year = 0xd490000 }`, raw 16.16 values; every
  field is initialized in C, so no constructor); the functions in C order,
  statics as `local function` (a static used before its definition is
  forward-declared as a `local`; exported functions called early, such as
  `economy_new_year` and `economy_recompute`, are reached as `M.` fields).
  Required at load: engine modules (`allocator`, `collect`, `entity_class`,
  `entity_id`, `entity_list`, `entity_registry`, `frame_zone`, `game_zone`,
  `pointer_list`, `vec`), `game.economy_formulas`, `game.main_economy`,
  `game.game_data`, `game.map_tables`, `game.research`, `game.tech_gate`,
  `game.trade` if needed, `p8.fixed`. Required at call time (cycle members):
  `game.economy_score`, `game.economy_tiers`, `game.message_box`,
  `game.planet`.
- **`game.economy_score`**: `final_score_t` built by a local constructor
  listing its six fields (a by-value return: a new table per call); module
  locals `g_difficulty` (one table created at load by the local button zero
  constructor) and `g_has_difficulty`; the functions in C order. Required at
  load: `engine.allocator`, `engine.entity_class`, `engine.entity_list`,
  `engine.entity_registry`, `engine.frame_zone`, `game.economy_formulas`,
  `game.main_economy`, `game.game_data`, `game.map_tables`, `game.trade`,
  `p8.fixed`, `p8.str`. Call time: `game.planet`, `game.probe`
  (`main_economy_typed`, defined in `probe.c`, per 3.5's and 3.6's rule that
  a function is called from its defining module).
- **`game.economy_tiers`**: the functions in C order. Required at load:
  `engine.entity_class`, `engine.entity_id_list`, `engine.entity_registry`,
  `engine.game_zone`, `engine.pointer_list`, `game.game_data`,
  `game.research`, `game.trade`, `p8.fixed`. Call time: `game.planet`,
  `game.planet_trade`, `game.slipway`.
- Each of the three files has its own local `for_each_tagged` (and
  `economy_score` and `economy_tiers` their own `fixed_min`), as the C
  statics are per file.
- A partner's constant (`PLANET_HAS_NEEDS`, `PLANET_HAS_INCOMING_ROUTES`,
  `PLANET_HAS_OUTGOING_ROUTES`, `PLANET_HAS_ID`) is read at call time through
  the module required in that function, never cached at module level (3.3's
  rule). Constants of modules outside the cycle (`ECONOMY_HAS_*`,
  `TAG_*`, `LINEAGE_*`, `ENTITY_MESSAGE_*`, `RESOURCE_FIXED`, the
  `game_data` counts and tables) are read through modules required at load.

### Shapes and field names

- `economy_t` is one table holding the entity header fields and the economy
  fields (first-member rule). **The economy's own `has` (the
  `ECONOMY_HAS_*` bits) is `has_`**, by 3.6's recorded rule (an object's own
  field named like an entity header field takes a trailing underscore; the
  header keeps `has`). Consequences in this step:
  - `game/game/tech_gate.lua`'s `research_cost_with_discount` reads
    `fm.has_` instead of `fm.has` (today it reads the header's bits, which
    would be wrong once a real economy exists).
  - `game/game/main_economy.lua`'s shape comment lists `has_` with that
    explanation.
- `economy_create` writes exactly the C fields, plus, Lua only, the inline
  sub-structs C writes into in place: `invented` (an empty table that
  `pointer_list_init` fills), `production` and `demand` (each `{ count = 0,
  entries = {} }`, the shape `economy_formulas` already uses; C writes
  `count = 0` into the inline bytes). The allocation goes through
  `allocator_alloc(game_zone(), SIZEOF.economy_t)`; `entity_id_take(self)`
  as the C takes a slot without registering.
- `end_status` is a Lua string or `nil` (`const char *` / `NULL`).
- `assert_economy` compares `self.class == M.economy_class_def` by identity,
  with the C assert message.
- `economy_add_ticks`'s function-pointer argument is a Lua function; the two
  callers pass `M.economy_recompute` and the local
  `economy_refresh_after_figures`.
- `economy_spend`: a local `toast_keys_zero()` (copy of `planet.lua`'s, from
  `message_box.h`'s field list, struct owned by step 3.9; may move there);
  `collect_format_signed` returns the text and the colour (out-parameter
  rule).
- `button_t` (owned by `ui_button`, step 3.9): a local zero constructor in
  `economy_score` listing every field of `button_t` at its C zero value: the
  zero header written by `entity_class.entity_init_header(t, nil)` (class
  `nil`, id `ENTITY_ID_NONE`, which is `{0, 0}`), the button's own `has` as
  `has_ = 0`, pointers `nil`, `slide_target` a zero `vec`, `project_planet`
  a new zero id, `project` a new `{ index = 0, epoch = 0 }`, every integer
  and fixed field 0. `g_difficulty` is created once at load.
  `economy_score_set_difficulty` builds the zero `copy`, sets the five fields
  the C copies, then writes every field of `copy` into `g_difficulty` in
  place (C `g_difficulty = copy` overwrites the storage a returned pointer
  still points at; same idea as 3.5's "records created at load are zeroed in
  place").
- `economy_score_percent_text` follows 3.1's zone-string rule: it calls
  `p8_tostr_fixed(zone, value)`, performs `allocator_alloc(zone, #s + 2)`,
  and returns `s .. "%"`.
- `sum_by_development_tier_ctx_t` is a table `{ total, seq, count }`
  written by the visit; `seq` is the very `game_data` table.
- `update_tiers_visit`'s `int *changed` context is a one-field table
  `{ value = 0 }` created in `economy_tiers_update_tiers`, which returns its
  `value`. No prior art in the tree; decided here.
- `economy_tiers_compute_production_amount(planet, out)` returns
  `found, out` (out-parameter rule); `update_tiers_visit` assigns the second
  result to `planet_production(planet).amount` only as the C writes it
  (the C leaves `*out` untouched on 0, so the Lua passes the current amount
  in and stores what comes back).
- The `(resource_t){ .kind = RESOURCE_FIXED, .number = ... }` compound
  literal is a new `{ kind = trade.RESOURCE_FIXED, number = ... }` table.
- `entity_list_filter` returns a new list (by value); `colony_upkeep_predicate`
  returns 1/0.
- C int predicates and flags stay 1/0 (`is_colonized_truthy`,
  `economy_ready`, `economy_spend`, `economy_tiers_update_tiers`); `if (x)`
  on them is `x ~= 0`; bit tests use `bit.band(...) ~= 0`.
- `sizeof`: `economy` has `M.SIZEOF = { economy_t = <64-bit value> }`;
  `economy_score` and `economy_tiers` use no `sizeof` (the percent-text
  allocation is a byte count).

### Data check registration

- One new reference unit `tools/table_reference/game_economy.c` including
  `game/economy.c`: `reference_class` for `economy_class_def`; the four
  fields of `economy_archetype` through `table_reference_int`
  (`economy_archetype.credits`, …, which form the `economy_archetype` block);
  `SIZEOF.economy_t`. `src/game/economy.c` is added to
  `TABLE_REFERENCE_SOURCES`.
- `economy_score` and `economy_tiers` have no file-level table, header
  constant or `sizeof` to register (`g_difficulty` is a static, not a table
  the checklist lists).

### Verification of the step

- `make checklist STEP=3.7` passes: 43/43 functions, 2/2 tables.
- `make test` green: `lua-guard` compiles the three files and finds no
  undeclared global; `lua-test-modules` loads all three (and still defers
  only `game.planet_family`) and finds `economy_class_def`,
  `economy_archetype` and `SIZEOF.economy_t` equal to the C.
- Behaviour checked against the C with throwaway programs, not committed (as
  in 3.4 to 3.6), outputs compared byte for byte: on a seeded 40-planet
  galaxy with planets settled and needs installed, `economy_new` then
  `economy_recompute` (tiers, production, demand, income, happiness, end
  status, every planet's `net_income`); a run of `economy_advance_clock` /
  `economy_tick_clock` across several new years; `economy_spend` with enough
  and too little credit (toast keys recorded by a stub); `economy_invent`
  across the era thresholds; `economy_score_final`, `economy_score_happiness`,
  `economy_score_colony_upkeep`, `economy_score_percent_text`,
  `economy_score_set_difficulty` / `economy_score_difficulty`;
  `economy_tiers_compute_tier` on hand-built planets with incoming routes and
  on a trading hub. Missing modules (`game.probe`, `game.slipway`,
  `game.message_box`, and `game.planet_family` if the galaxy needs it) are
  stubbed through `package.preload` in the throwaway script only; the C side
  links the real game.

## Implementation Chunks

1. **`economy_tiers`**: the module with its call-time requires; planet bits
   read from the planet's `has_` (3.6's field name); throwaway checks of
   `economy_tiers_compute_tier`, `economy_tiers_update_tiers` and
   `economy_tiers_compute_production_amount` on hand-built planets. Loads
   alone; nothing to register.
2. **`economy_score`**: the module, the `final_score_t` constructor, the
   local `button_t` zero constructor and the in-place difficulty copy;
   throwaway checks of upkeep, trade income, happiness, final score, percent
   text and the difficulty. Loads alone; nothing to register.
3. **`economy`**: the module, `economy_class_def`, `economy_archetype`,
   `M.SIZEOF`, the local `toast_keys_zero`, the `has_` field with the
   `tech_gate.lua` read and the `main_economy.lua` comment updated, the
   `game_economy.c` reference unit and its `TABLE_REFERENCE_SOURCES` entry;
   throwaway checks of recompute, the clock, new years, spending and
   inventions on a seeded galaxy. `make checklist STEP=3.7` passes.
4. **Own-time gate and step note**: the own times of `lua-guard` and
   `lua-test-modules`, 45 pairs against the step's before commit (the last
   commit before chunk 1, currently `05325e9`), each under 100 ms and within
   2× of 3.1's per-KB and per-table costs; KB of Lua added, blocks and
   reference lines added; the projection to the end of the plan. 3.7 is not
   a checkpoint: no full rule-4 protocol unless the gate fails. The step note
   in `docs/love-port/plan-2.md` records the files, the counts (43/43, 2/2),
   the load finding (all three load; the deferred list unchanged; the
   evidence), the `has_` field and the `tech_gate` fix, the zero constructors
   for structs owned by 3.9 (`toast_keys_t`, `button_t`), the in-place
   difficulty copy, the `{ value }` context, what 3.8 inherits (see Further
   Notes), the throwaway checks, the gate figures, and that the PRD's agent
   had no `guideline` and settled its points itself.

## Testing Decisions

- A good test checks external behaviour through the public interface, never
  module internals.
- No new committed test: the data check (`lua-test-modules`) is the committed
  check for the two tables and the size; the checklist for every function and
  table; `lua-guard` for compilation and globals. The step adds no `make test`
  target (the Phase 3 header forbids it).
- No committed behaviour tests for the game modules: the e2e replays of
  plan 3 cover them (3.1's decision, kept by 3.2 to 3.6). Behaviour is
  checked against the C with throwaway programs whose outputs are compared
  byte for byte.
- Prior art: `game/game/research.lua` (zero class table, class identity
  assert), `game/game/planet.lua` (`has_`, local `toast_keys_zero`, inline
  sub-structs created at init), `tools/table_reference/game_research.c` and
  `game_mouse.c` (class and size units), `game/game/tech_gate.lua` (call-time
  require of `game.probe`).

## Out of Scope

- Any module of steps 3.8 and 3.9 (slipway, probe, wormhole, particle zone,
  UI, UI button, message box, particle, background, scroll, research
  display, bootstrap), and loading `planet_family` before they exist.
- Changing `LUA_DEFERRED_MODULES` (unchanged by this step) or the deferral
  mechanism.
- Folding the local `toast_keys_t` and `button_t` zero constructors into
  their owners (3.9 may).
- Running any e2e replay or the Lua game in the fake (plan 3; step 3.15 for
  the synthetic run).
- The full rule-4 timing protocol (next checkpoint: 3.9), unless this step's
  own-time gate fails.
- Audio, redesign of any kind (plan 5).

## Further Notes

- No question went to a `guideline` agent (none was reachable from the PRD
  agent, and none was left open): the `has_` rename follows 3.6's
  `guideline`-decided rule; the rest is settled by the step notes and the C.
  Decisions taken here without a human: the in-place difficulty copy, the
  `button_t` zero constructor shape, the `{ value }` table for an `int *`
  context, the inline sub-struct tables created in `economy_create`.
- Expected size: about 920 lines of C, so roughly 30–35 KB of Lua at 3.6's
  ratio; `lua-guard` should grow by about 2–3 ms (to about 53–54 ms),
  `lua-test-modules` by about 2 ms (load of three files plus 3 small blocks),
  well inside the 2× gate.
- Step 3.8 inherits: `probe` may compare with `economy.economy_class()` by
  requiring `game.economy` at call time (never at load: `economy_score`
  requires `game.probe` at call time, and `probe` will be deferred on
  `particle` anyway); `slipway` must export `slipway_class` and
  `slipway_served_planets_add(served_planets, planet_id)` taking the slipway's
  inline `served_planets` table and the planet's id string.
