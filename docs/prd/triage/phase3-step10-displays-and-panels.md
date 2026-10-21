---
title: "Plan 2, step 3.10: displays and panels built on the cycle"
description: "The Lua game lacks the slipway and wormhole displays, the trade structures, the structure icons, the score panel and the help screen, so slipway and wormhole still cannot load."
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
> 3.10. `game/game/` displays and panels built on the cycle: trade and slipway
>    displays, structure icons, score panel, help.

## Problem Statement

Plan 2 builds the Lua game as a faithful copy of the C one, file by file, so
that plan 3 can put any Lua function next to its C original when a replay
diverges. After step 3.9 the include cycle (galaxy, planets, economy,
slipways, probes, wormholes, UI, particles) is in the tree, but the modules
built on top of it are not: the slipway and wormhole draw code, the three
trade structures (slipgate, infraspace gate, synthesizer), the build-menu
structure icons, the score panel with its yearly banner and score box, and
the help screen. Two committed modules, `game.slipway` and `game.wormhole`,
hold `slipway_draw_method` and `wormhole_draw_method` by identity in their
class tables; those methods live in the missing display modules, so both
modules still fail to load and are listed in the Makefile's
`LUA_DEFERRED_MODULES`, their 13 reference blocks skipped by the data check.
Until this step lands, a plan 3 developer could not load a slipway or a
wormhole in Lua at all, nor compare any of these 122 functions and 15 tables
with the C.

## Solution

Seven new Lua modules under `game/game/`, one per C pair the checklist
assigns to 3.10, each a faithful copy of its C file in C order under the
C-to-Lua conventions of steps 2.1 to 3.9:

- `slipway_display` (7 functions) and `wormhole_display` (2 functions);
- `planet_family_trade` (33 functions, 9 tables);
- `structure_icon` (25 functions, 2 tables);
- `score_panel` (25 functions, 3 tables) and `score_panel_display`
  (15 functions);
- `help` (15 functions, 1 table).

With the two display modules in the tree, `game.slipway` and
`game.wormhole` load; `LUA_DEFERRED_MODULES` becomes empty, as the step 3.8
note requires (the load check fails on a deferred module that loads). Every
table and constant of the step is registered with the data check through
reference units, so `lua-test-modules` compares them with the C, and
`make checklist STEP=3.10` passes with 122/122 functions and 15/15 tables.

**Load finding (evidence, before any code is written).** In a scratch copy of
`game/`, two stubs (`slipway_display` exporting only `slipway_draw_method`,
`wormhole_display` exporting only `wormhole_draw_method`) were added and
`tests/lua/test_modules.lua` run on the tree with the committed reference:

- without `--defer`: `89 modules loaded, 760 names and 33310 lines equal to
  C`, exit 0;
- with the Makefile's current `--defer game.slipway --defer game.wormhole`:
  `game.slipway loads now: drop it from the deferred modules`, the same for
  `game.wormhole`, exit 1;
- on the real tree today: `85 modules loaded, 2 deferred: game.slipway,
  game.wormhole (13 names skipped), 747 names and 33231 lines equal to C`.

So slipway and wormhole load as soon as their display modules exist, their 13
blocks (79 lines) already equal the C, and the stubbed `slipway_class().draw`
is the display module's `slipway_draw_method` by identity. Nothing else in
their load chain is missing.

## User Stories

1. As a plan 3 developer, I want `game/game/slipway_display.lua` to hold
   every function of `slipway_display.c` in C order with the C names, so
   that I can put the Lua slipway drawing next to the C one when a replay
   diverges.
2. As a plan 3 developer, I want `game/game/wormhole_display.lua` to hold
   `wormhole_draw` and `wormhole_draw_method`, so that a wormhole draws in
   Lua exactly as in C.
3. As a plan 3 developer, I want `game.slipway` and `game.wormhole` to load
   under plain LuaJIT, so that the slipway and wormhole classes are usable and
   their data blocks are compared with the C.
4. As a maintainer, I want `LUA_DEFERRED_MODULES` empty after this step, so
   that no module of the tree is excused from loading and the step 3.8 note's
   promise is kept.
5. As a maintainer, I want the load check to keep failing on any module that
   does not load, so that a regression in a cycle module is caught at once.
6. As a plan 3 developer, I want `slipway_spawn_transit_dot` to draw from the
   visual random stream before its served-planets test, as the C does, so
   that the visual stream stays synchronised with the C.
7. As a plan 3 developer, I want transit dots allocated from the particle
   zone's transit-dot slot and recorded with `particle_zone_record_elements`,
   so that zone usage and the state dump match the C.
8. As a maintainer, I want the C compile-time check that a transit dot fills
   its slot to be a load-time `assert` on `SIZEOF.transit_dot_t` and
   `TRANSIT_DOTS_SLOT`, so that a size drift fails at load.
9. As a plan 3 developer, I want `slipway_line_ends` to return a new table
   with `start` and `end_` built by value, so that a caller never mutates
   another's ends.
10. As a plan 3 developer, I want `floored_far_from_the_wrap` to keep its C
    assert and message, so that the wrap trap stays guarded.
11. As a plan 3 developer, I want `wormhole_draw` to compute its screen
    position and circle radii with the C fixed-point operations, so that the
    pixels match.
12. As a plan 3 developer, I want `game/game/planet_family_trade.lua` with
    the slipgate, infraspace gate and synthesizer classes, kinds and
    constructors, so that trade structures can be built in Lua.
13. As a plan 3 developer, I want `slipgate_class_def`,
    `infraspace_gate_class_def` and `synthesizer_class_def` to hold the very
    methods of planet, planet_display, planet_trade, planet_family and probe
    (by identity, from their defining modules), so that identity comparisons
    such as `drag == targeting_track_method` behave as in C.
14. As a plan 3 developer, I want `slipgate_accept_drop` to compare the
    dropped entity's class with `slipway_class()` at call time, so that the
    module loads without requiring slipway at load.
15. As a plan 3 developer, I want `synthesizer_set_spent` and
    `synthesizer_attach_slipway` to switch `synthesizable_resources` to the
    very `synthesizer_synthesizable_resources_spent` table, so that the
    "spent" test by identity works as in C.
16. As a plan 3 developer, I want the three `*_create` functions to keep the
    C asserts and messages (init must run first, the only construction key),
    so that misuse fails with the C text.
17. As a plan 3 developer, I want `slipgate_kind_def`,
    `infraspace_gate_kind_def` and `synthesizer_kind_def` to be
    `{ new_entity, record }` tables pointing at the `game_data` kind records,
    as `planet_family`'s kinds are, so that the kind lookups are uniform.
18. As a plan 3 developer, I want `slipgate_left_press_empty` to be a header
    table whose class is `slipgate_left_press_empty_class_def`, so that the
    empty press result is the same object every time, as in C.
19. As a plan 3 developer, I want `planet_family_trade_init` to assert that
    `planet_family_init` ran first, so that the init order matches the C.
20. As a plan 3 developer, I want `game/game/structure_icon.lua` with the
    build-menu icons, their cursor and id counter, so that the build menu's
    logic exists in Lua.
21. As a plan 3 developer, I want the cursor statics (`g_cursor`,
    `g_has_cursor`) and the id counter statics as module locals initialised
    as in C, so that `structure_icon_cursor` returns zero while absent.
22. As a plan 3 developer, I want `structure_icon_id_counter` to return the
    C int and the out value (`ok, id`), per the out-parameter convention, so
    that save code in 3.12 can call it uniformly.
23. As a plan 3 developer, I want `structure_icon_kind_view` to return a new
    table each call with every field of `structure_icon_kind_t`, so that the
    by-value view is never shared.
24. As a plan 3 developer, I want `structure_icon_rest_cursor` to return the
    archetype's `rest_cursor` table itself while the module is up, and nil
    otherwise, so that the pointer semantics match.
25. As a plan 3 developer, I want `structure_icon_is_revealed` to return 0
    for nil or any other entity, so that callers can test any entity.
26. As a plan 3 developer, I want `structure_icon_left_press` to build the
    structure through the kind's `new_entity` with the planet id key, as the C
    does, so that build placement matches.
27. As a plan 3 developer, I want `game/game/help.lua` with the help screen
    (a probe-shaped entity), its class, constructor and the start/resume
    callbacks, so that the title screen's help exists in Lua.
28. As a plan 3 developer, I want `help_set_start_game(nil)` to restore the
    placeholder and `help_set_resume_game(nil)` to store nil, as in C, so
    that the title has no resume row after init.
29. As a plan 3 developer, I want `screen_create` to emit particles in
    `PARTICLE_SLOT_HELP_LIST`, so that the help screen's particle zone usage
    matches the C.
30. As a plan 3 developer, I want `help_screen_construct` to build one button
    per difficulty option from `difficulty_options_table`, so that the menu
    rows match.
31. As a plan 3 developer, I want `game/game/score_panel.lua` with the
    readouts, year banner and layout block statics, so that the HUD score
    panel exists in Lua.
32. As a plan 3 developer, I want `score_panel_init` to copy the generated
    rows into the module's own readout and banner rows (items 0-indexed,
    sub-rows linked by the very tables), so that later edits do not touch
    `game_data`.
33. As a plan 3 developer, I want `score_panel_refresh` to store each readout
    text as a Lua string in `texts[index]`, after the C length assert, so
    that the readout strings match the C buffers.
34. As a plan 3 developer, I want `g_score_panel_layout_block` to be one
    module table, returned by `score_panel_layout_block()` while up, so that
    the menu (3.13) changes the same table the C changes.
35. As a plan 3 developer, I want `score_panel_draw` to keep the curtain
    computation and the bottom bar that inherits the pen colour, so that the
    end-of-game curtain matches the C pixels.
36. As a plan 3 developer, I want `main_score_panel` to keep its assert that
    only a score panel is stored, so that a wrong store fails with the C
    message.
37. As a plan 3 developer, I want `game/game/score_panel_display.lua` with
    the new-year banner, the score box update, the toggle and the keep-open
    counter, so that the yearly flow exists in Lua.
38. As a plan 3 developer, I want `score_panel_new_year_method` and
    `score_panel_update_method` defined in `score_panel_display` and read by
    `score_panel_class_def` from there, by identity, so that the class holds
    the C methods.
39. As a plan 3 developer, I want `score_panel_toggle_on_click` to return the
    very `score_panel_toggle_on_click_method`, so that a backdrop compares it
    by identity as in C.
40. As a maintainer, I want every table of the step compared with the C by
    `lua-test-modules` through new reference units, so that a data drift
    fails `make test`.
41. As a maintainer, I want the step's header constants
    (`SCORE_PANEL_LAYOUT_BLOCK_HAS_FIXED_POS`,
    `SCORE_PANEL_LAYOUT_BLOCK_HAS_ROW_STEP`, `SCORE_PANEL_TEXT_CAP`) and every
    `SIZEOF` entry the step's modules use compared with the C compiler, so
    that sizes and bits cannot drift.
42. As a maintainer, I want `make checklist STEP=3.10` to pass with 122/122
    functions and 15/15 tables, so that the step is complete by the
    authoritative assignment.
43. As a maintainer, I want `lua-guard` to accept every new file (compiles,
    no undeclared global, no `love.` outside `game/love/`), so that the
    static rules hold.
44. As a maintainer, I want the own times of `lua-guard` and
    `lua-test-modules` measured against the "before" commit and recorded, so
    that the plan's timing budget is tracked.
45. As a maintainer, I want the step note to record the load finding, the
    load directions chosen and the figures, so that 3.11 to 3.15 start from
    facts.
46. As a plan 3 developer, I want behaviour checked against the C with
    throwaway programs before each commit, so that faithful-copy errors are
    caught before the replays of plan 3.

## Implementation Decisions

- **Modules.** Seven new modules, one per C pair, each headed by a comment
  naming its C files, exported functions `function M.<c name>(`, statics
  `local function`, file-level tables as module fields, header constants as
  module fields, in C order (header, then `.c`). Each header holds no inline
  function; the seven `.h` files add only constants and struct shapes.
- **Load directions (3.3's cycle rule).**
  - `game.slipway` already requires `game.slipway_display` at load;
    `slipway_display` requires `game.slipway` (and `game.planet`, for
    `planet_is_family` and `planet_id_or_null`) at call time only. Its
    load-time needs are `particle_zone` (`TRANSIT_DOTS_SLOT`, for the size
    assert) and `engine.zone`, which do not reach slipway.
  - `game.wormhole` already requires `game.wormhole_display` at load;
    `wormhole_display` requires `game.wormhole` at call time only and has no
    load-time value.
  - `score_panel_class_def` holds `score_panel_new_year_method` and
    `score_panel_update_method`, declared in `score_panel.h` but defined in
    `score_panel_display.c`: `score_panel` requires `game.score_panel_display`
    at load, one direction only; `score_panel_display` requires
    `game.score_panel` at call time. Per 3.6's rule, the two methods are not
    re-exported from `score_panel`.
  - `score_panel_init`/`score_panel_shutdown` call
    `score_panel_display_init`/`_shutdown` (declared in `score_panel.h`) on
    `score_panel_display`.
  - `planet_family_trade` requires at load the modules whose methods its
    class tables hold: `game.planet`, `game.planet_display`,
    `game.planet_trade`, `game.planet_family`, `game.probe` (each method
    taken from its defining module, as `planet_family.lua` does). `game.slipway`,
    `game.particle` (`emitter_init`) and `game.trade` are required at call
    time. Nothing in the tree requires `planet_family_trade`.
  - `structure_icon` and `help` hold only their own methods and probe's
    (`help`: `targeting_tooltip_method`, `targeting_track_method`,
    `targeting_confirm_method`, `probe_spawn_particle`, `probe_release_method`),
    so `help` requires `game.probe` at load; `structure_icon` requires its
    partners (planet, probe, tech_gate, layout) at call time.
  - No step-3.10 module is in a two-way load need, so none can fail to load
    alone once its partners exist; all seven load at once, and the load check
    runs at this step.
- **Deferred list.** `LUA_DEFERRED_MODULES` is set to empty in the
  Makefile. The `--defer` mechanism of `tests/lua/test_modules.lua` and its C
  test `test_lua_modules` stay unchanged (3.11 to 3.14 may meet another
  cycle); the comment above the variable is kept as it is, still true.
- **Structs and values.** `transit_dot_t` is a table `{ pos, velocity, life }`
  written into the block the allocator returns; `slipway_line_ends_t` a new
  table `{ start, end_ }` (keyword field takes a trailing underscore) built by
  a local constructor; `structure_icon_t`, `structure_icon_kind_t`,
  `score_panel_t`, `score_panel_layout_block_t` keep their C field names; a
  field named like an entity header field takes a trailing underscore
  (`score_panel_layout_block_t.has` becomes `has_`, the header keeps `has`),
  as 3.6 decided for planets.
- **Statics.** `g_score_panel_readouts` and `g_score_panel_year_banner` are
  module tables with every field at its C zero value: `row`/`second`/`subs`
  as data-row tables, `items`, `second_items`, `sub_items` 0-indexed arrays
  of data-item tables, `texts` a 0-indexed array of Lua strings (`""`
  initially), `year` a Lua string, `keep_open` nil. `score_panel_row_copy`
  copies each item table into the destination array (new tables, every field)
  and the row's fields into `dst`, then points `dst.items` at the
  destination array. `g_score_panel_layout_block` is one module field built at
  load with the C initializer. `g_main_score_panel` holds an id table, copied
  from `ENTITY_ID_NONE` at each reset.
- **Readout strings.** `strcpy(texts[i], text)` then `text = texts[i]` is: the
  C length assert, then `texts[i] = text`, and the readout item's `text` set to
  that same string. Zone-built strings follow 3.1's zone-string rule.
- **Function pointers and placeholders.** `help`'s `g_start_game_method`,
  `g_resume_game_method` and `score_panel`'s `g_init_method` are module
  locals; the `_field_value` getters return them; setters take nil as the C
  takes `NULL`.
- **Kinds.** `*_kind_def = { new_entity = <local *_kind_new>, record =
  game_data.<name>_kind_record }`, exactly as `planet_family.lua`.
- **Compile-time checks.** `transit_dot_fills_its_slot_t` becomes a load-time
  `assert(zone.ZONE_BYTES(SIZEOF.transit_dot_t) == particle_zone.TRANSIT_DOTS_SLOT, "transit_dot_fills_its_slot_t")`.
- **`sizeof`.** Every `sizeof` used is an entry of the module's `M.SIZEOF`
  keyed by the C type, compared by the data check (at least
  `SIZEOF.transit_dot_t`, `SIZEOF.planet_t` in `planet_family_trade`,
  `SIZEOF.structure_icon_t`, `SIZEOF.score_panel_t`, `SIZEOF.probe_t` in
  `help`, and any other the bodies use).
- **Reference units.** One unit per C source with a table or constant:
  `game_slipway_display.c` (`SIZEOF` entries), `game_planet_family_trade.c`
  (three classes via `reference_class`, three kinds rendered as in
  `game_planet_family.c` with the kind records named, the empty class and
  its header via `reference_header`, the empty `int_seq` with count and a nil
  pointer, sizes), `game_structure_icon.c` (two classes, sizes),
  `game_score_panel.c` (two classes, the layout block header, `has_` bits,
  vectors and colour, the three constants, sizes), `game_help.c` (the class,
  sizes). `wormhole_display` and `score_panel_display` have no table and no
  constant: no unit unless a `SIZEOF` appears. Their sources are appended to
  `TABLE_REFERENCE_SOURCES`.
- **Assert messages and fatal texts** are the C ones, verbatim.
- **No guideline question was needed:** the load directions follow from the
  committed code and the 3.3/3.6 rules (no two-way need exists), and the
  stub experiment settled the load finding. Like earlier PRD agents, this one
  settled its points itself.

## Implementation Chunks

1. **Slipway and wormhole displays, empty deferred list.**
   `slipway_display` (7 functions, transit-dot size assert) and
   `wormhole_display` (2 functions); reference unit for slipway_display's
   sizes; `LUA_DEFERRED_MODULES` emptied. Done when `lua-test-modules`
   reports every module loaded, none deferred, the 13 former slipway and
   wormhole blocks compared equal; throwaway C-vs-Lua programs match on
   slipway lines, transit dots over many frames (visual stream and zone
   counts) and wormhole draws.
2. **Trade structures.** `planet_family_trade` (33 functions, 9 tables) and
   its reference unit. Done when its tables compare equal and a throwaway
   program matches the C on slipgate/infraspace gate/synthesizer creation,
   accept-drop, left press (made slipway or empty result), refresh, offer
   trade, synthesizer attach and spent switching.
3. **Structure icons and help.** `structure_icon` (25 functions, 2 tables)
   and `help` (15 functions, 1 table), their reference units. Done when the
   tables compare equal and throwaway programs match the C on icon refresh,
   reveal, left press, cursor and id counter, kind view, and help screen
   construction, draws and particles.
4. **Score panel.** `score_panel` (25 functions, 3 tables, 3 constants) and
   `score_panel_display` (15 functions), the reference unit. Done when the
   tables and constants compare equal and throwaway programs match the C on
   init copies, refresh readouts over several economy states and the
   placeholder, draw with and without the curtain, new year banners, update
   rows, toggle and final score text.
5. **Own-time gate and step note.** `make checklist STEP=3.10` passes
   (122/122, 15/15); `make test` green. Measure the own times of `lua-guard`
   and `lua-test-modules` paired against the "before" commit `457d574`
   (45 pairs, as 3.3 to 3.9 did), with KB of Lua, compared blocks and
   reference lines added; pass when each stays under 100 ms and grows within
   2× of 3.1's cost per KB and per table. 3.10 is not a checkpoint: no full
   rule-4 protocol unless the gate fails. No other agent runs during the
   measurement. Write the step 3.10 note in `docs/love-port/plan-2.md`: the
   files, checklist counts, the load finding (slipway and wormhole load,
   `LUA_DEFERRED_MODULES` empty), load directions, the behaviour checks, the
   two own-time figures and the projection to the end of the plan.

## Testing Decisions

- A good test checks external behaviour only: what a module returns, the
  tables it holds, the pixels and zone counts it produces, compared with the
  C original; never the shape of the Lua code.
- As 3.1 to 3.9 decided, no permanent behaviour tests for these modules: the
  plan 3 replays cover them. The permanent checks are the existing three:
  `lua-guard` (compiles, globals, `love.` names), `lua-test-modules` (every
  module loads, every table and constant equals the C), and
  `make checklist STEP=3.10`. No `make test` target is added.
- Behaviour is checked before each commit with throwaway programs that run
  the C function and the Lua function on the same inputs and diff their
  output byte for byte (prior art: the step 3.4 to 3.9 notes; the C reference
  writer pattern of `tools/fixed_reference.c` and `tests/lua/test_fixed.lua`).
  They are not committed.
- Data tables: prior art `tools/table_reference/game_planet_family.c` (kinds),
  `game_slipway.c` and `game_wormhole.c` (classes and headers),
  `reference_class.h`.

## Out of Scope

- Pad modes (3.11), save (3.12), menus, title and attract (3.13: the menu's
  use of `score_panel_layout_block`, the title's use of the help callbacks),
  the cart and input step (3.14), `main.lua` and `debug_state()` (3.15).
- Running the e2e replays or `make love-e2e` (plan 3).
- Removing the `--defer` mechanism from the load check.
- Audio, redesign, and any C change.

## Further Notes

- Before commit for the gate: `457d574` (docs: record step 3.9 of plan 2).
- Projection: about 60–70 KB of Lua at 3.9's 0.077 ms per KB brings
  `lua-guard` from about 68 ms to about 73 ms; `lua-test-modules` gains the 13
  formerly skipped blocks plus about 20 new ones, a few ms.
- The step 3.8 note's "3.10 empties it" and the Phase 3 header's rule that a
  cycle's load check runs at its last step are both met here: the cycle's
  last deferred members load at 3.10.
