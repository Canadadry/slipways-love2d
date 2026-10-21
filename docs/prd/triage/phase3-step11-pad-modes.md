---
title: "Plan 2, step 3.11: pad modes"
description: "The Lua game lacks the pad modes (target picking, jump cursor, panel focus, build bar, select button, slipway drag, panel cycle, back target) and the cheat chord, so the pad input step of 3.13 and 3.14 has nothing to call."
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
> 3.11. `game/game/` pad modes.

## Problem Statement

Plan 2 builds the Lua game as a faithful copy of the C one, file by file, so
that plan 3 can put any Lua function next to its C original when a replay
diverges. After step 3.10 every module of the game's object layer is in the
tree and loads (94 modules, `LUA_DEFERRED_MODULES` empty), but none of the
pad modes is: the directional target picker, the jump cursor with its
auto-repeat and edge scroll, the panel focus walker, the build-bar picker,
the select button's tap/hold/chain timer, the slipway drag state, the panel
cycle, the "back" target and the four-chord cheat. Most of the 20 replays
are pad replays; every one of them goes through these modules on every
frame, through `pad_nav` (3.13) and the input step (3.14). Until this step
lands, a plan 3 developer could not compare any of these 89 functions and
the one table with the C, and 3.13 would have nothing to require.

## Solution

Ten new Lua modules under `game/game/`, one per C pair (or lone header) the
checklist assigns to 3.11, each a faithful copy of its C file in C order
under the C-to-Lua conventions of steps 2.1 to 3.10:

- `pad_direction` (header only: the five `PAD_DIRECTION_*` constants);
- `pad_target` (4 functions), `pad_back` (3), `pad_cycle` (7), `cheat` (4);
- `pad_jump` (21 functions, 1 table: `directions_by_priority`),
  `pad_select` (7), `pad_slipway` (8);
- `pad_panel` (27 functions), `pad_build` (8).

Every header constant and the one table are registered with the data check
through reference units, so `lua-test-modules` compares them with the C, and
`make checklist STEP=3.11` passes with 89/89 functions and 1/1 tables.
`pad_nav` is not here: step 1.1's note moved it to 3.13 (it includes the
menus).

**Load finding (evidence, before any code is written): all ten modules load
at once; no `--defer`.**

- No include cycle: the C files including a 3.11 header are the 3.11 files
  themselves, `pad_nav` (3.13), `input_step.h` (3.14) and `src/devtools/`
  (not ported). No committed Lua module requires any 3.11 module.
- Every function the 3.11 C bodies call already has its Lua function in the
  tree: `planet_is_family`, `planet_class` (planet), `GALAXY_MAP_SCALE`
  (galaxy), `structure_icon_is_revealed`, `slipgate_takes_slipway`
  (planet_family_trade), `slipway_other_end`, `main_economy`,
  `main_economy_typed`, `main_economy_placeholder_class` (probe), `main_ui`,
  `main_camera_typed`, `modal_backdrop_class`, `button_class`,
  `entity_registry_hit_test_always`, `entity_registry_generation`,
  `entities_by_message`, `entity_list_filter`, `bindings_mask`,
  `bindings_mode_has_action`, the id-list iterator, `P8_PAD_BUTTON_5`,
  `P8_MOUSE_LEFT`, `P8_SCREEN_WIDTH`.
- Experiment: in a scratch copy of `game/`, ten stub modules each requiring
  at load every module its C file includes (a superset of what the port will
  require) were added and `tests/lua/test_modules.lua` run without `--defer`
  on the committed reference: `104 modules loaded, 783 names and 33456 lines
  equal to C`, exit 0 (the real tree: 94 modules, same counts). Each stub
  also loads alone in a fresh LuaJIT process.
- None of the ten holds a class table or any other load-time value from a
  partner, so nothing can make one fail to load alone. `LUA_DEFERRED_MODULES`
  stays empty.

## User Stories

1. As a plan 3 developer, I want `game/game/pad_direction.lua` holding
   `PAD_DIRECTION_NONE`, `_UP`, `_DOWN`, `_LEFT`, `_RIGHT` with the C values,
   so that every pad module names directions as the C does.
2. As a plan 3 developer, I want `pad_target_is_candidate` to return 1 only
   for a live planet-family entity that is known or settled, and 0 for nil,
   so that every mode filters targets as the C does.
3. As a plan 3 developer, I want `pad_target_pick` to score candidates with
   the C fixed-point along/perpendicular offsets and the
   `PAD_TARGET_PERPENDICULAR_WEIGHT`, breaking ties on the smaller
   perpendicular, so that a direction press picks the same planet as in C.
4. As a plan 3 developer, I want `pad_target_pick` to skip the origin, return
   nil on a nil list, and keep its three asserts with the C messages, so that
   misuse fails with the C text.
5. As a plan 3 developer, I want `pad_back_init`, `pad_back_record` and
   `pad_back_target` to keep the last acted entity as an id and resolve it
   each time, so that a dead or replaced entity is never returned.
6. As a plan 3 developer, I want `pad_back_target` to return nil when the
   last acted entity is the hovered one, so that "back" never jumps in place.
7. As a plan 3 developer, I want `pad_cycle` to cycle closed → tech → score →
   closed, raise `landing_due` on every open panel and clear it on landing,
   so that the panel cycle button behaves as in C.
8. As a plan 3 developer, I want `pad_cycle_hud_button` to return 2 for tech,
   3 for score and 0 otherwise, so that the HUD highlights the same button.
9. As a plan 3 developer, I want `pad_cycle_next` to fail with the C message
   on a value that is not a panel, so that a bad value is caught.
10. As a plan 3 developer, I want `cheat_step` to count only frames where
    both chord buttons go down together, reset on a lone edge, and apply the
    cheat on the fourth chord, so that the cheat fires on the same frame as
    in C.
11. As a plan 3 developer, I want the cheat to write 99 research points and
    200 credits into the placeholder (with its `has_` flags) or the real
    economy, by class identity, so that both game phases get the same values.
12. As a plan 3 developer, I want `cheat_chord_buttons` to return the C mask
    of buttons 5 and 6, so that the input step can mask them.
13. As a plan 3 developer, I want `pad_jump_init` to fill the caller's table
    with a fresh none id and zeros, so that a new jump state matches the C
    zero state.
14. As a plan 3 developer, I want `pad_jump_hovered` to resolve the hovered
    id and return it only while it is still a target candidate, so that a
    dead planet drops the hover.
15. As a plan 3 developer, I want `pad_jump_hover_seeded` to set `seeded`
    only when given an entity, and `pad_jump_reseed` to pick the nearest once
    seeded and the first colony otherwise, so that the cursor's first landing
    matches the C.
16. As a plan 3 developer, I want `directions_by_priority` as a 0-indexed
    module table (up, down, left, right) compared with the C, so that a frame
    pressing two directions picks the same one.
17. As a plan 3 developer, I want `pad_jump_repeat_step` to repeat a held
    direction after `PAD_JUMP_REPEAT_DELAY` frames then every
    `PAD_JUMP_REPEAT_PERIOD`, reading the masks through `bindings_mask` for
    the given mode, so that held jumps fire on the same frames as in C.
18. As a plan 3 developer, I want `pad_jump_repeat_step` to assert on a mode
    without jump actions with the C message, so that a wrong mode fails.
19. As a plan 3 developer, I want `pad_jump_inside` and `pad_jump_scroll` to
    use the C inner rectangle (the margin, and the bottom margin over the HUD
    row) and clamp each shift to `PAD_JUMP_SCROLL_SPEED`, so that the camera
    scrolls the same pixels.
20. As a plan 3 developer, I want `pad_jump_scroll` and
    `pad_jump_world_point` to return their two out values, so that callers
    follow the out-parameter convention.
21. As a plan 3 developer, I want `pad_jump_world_point` to floor
    `map_pos × GALAXY_MAP_SCALE` with the fixed-point functions, so that
    world points are bit-exact.
22. As a plan 3 developer, I want `pad_jump_nearest` to take the first entity
    at the smallest squared distance, so that ties resolve in list order as
    in C.
23. As a plan 3 developer, I want `pad_jump_candidates` to build its list
    with `entity_list_filter` in the given zone and `pad_jump_any_candidate`
    to return 0 for nil, so that candidate lists match in content and zone use.
24. As a plan 3 developer, I want `pad_select_step` to report a tap on a
    short release, a year on a 30-frame hold or on the first frame of a press
    inside the chain window, and nothing while not allowed, so that the
    select button ends years on the same frames as in C.
25. As a plan 3 developer, I want `pad_select_year_ended` and
    `pad_select_year_just_ended` to open and detect the chain window, so that
    chained years behave as in C.
26. As a plan 3 developer, I want `pad_select_needs_attention` to read the
    planet's own `has_` bits for `PLANET_HAS_NEEDS` (not the header's `has`)
    and treat a nil need, an unset or a zero `satisfied` as unmet, so that
    the attention cycle picks the same colonies.
27. As a plan 3 developer, I want `pad_select_next` to walk the list from the
    entry after the current one, wrapping with the C modulo, so that
    repeated taps visit colonies in the C order.
28. As a plan 3 developer, I want `pad_slipway_can_start` to accept a live
    settled planet-family entity, and a slipgate only when it still takes a
    slipway, so that the slipway mode starts where the C does.
29. As a plan 3 developer, I want `pad_slipway_candidates` to filter out the
    start entity by identity, so that a slipway never targets its own start.
30. As a plan 3 developer, I want `pad_slipway_between` to walk the start's
    slipway id list and find the slipway whose other end is the target, with
    the C asserts, so that a built link is recognised.
31. As a plan 3 developer, I want `pad_slipway_outcome`, `_release` and
    `_rebase` to move the drag state as the C does (built moves the start to
    the target, the target is reset to a fresh none id), so that chained
    slipways start from the same planet.
32. As a plan 3 developer, I want `pad_panel_box_open` and `pad_panel_open`
    to answer from the ui's ephemerals, the economy and the modal backdrop,
    so that the pad switches to panel mode on the same frame as in C.
33. As a plan 3 developer, I want `pad_panel_step_begin`/`_end` to keep the
    C memo (ui id, economy id, ephemerals count, registry generation) with
    its asserts, so that `pad_panel_open` reuses its answer exactly when the
    C does.
34. As a plan 3 developer, I want `pad_panel_entity_under` to build the mouse
    position the C builds (screen point, world point through the camera, both
    has bits) and call each candidate's `hit_test`, so that hit testing finds
    the same entity.
35. As a plan 3 developer, I want the panel candidate walk to read only the
    ui's ephemerals while a box is open, and the hit-test bucket otherwise,
    so that it mirrors `ui_update` as in C.
36. As a plan 3 developer, I want `pad_panel_is_item`, `pad_panel_first_item`
    and `pad_panel_neighbor` to keep only items on the top layer with a hit
    box, and score neighbours with the C box middles and span gaps, so that a
    direction press moves focus to the same button.
37. As a plan 3 developer, I want a button still sliding in to be judged at
    its slide target (the button's own `has_` bit), so that a Down on its
    first frames does not skip a row.
38. As a plan 3 developer, I want boxes of layers below the screen-space
    layer shifted by the camera, so that world-space panels and screen-space
    panels are compared in the same frame.
39. As a plan 3 developer, I want `pad_panel_step` to return the focused
    entity, then the cursor point and the mouse buttons (unchanged point when
    nothing is focused), so that the input step drives the mouse as in C.
40. As a plan 3 developer, I want `pad_build_focus` to pick the rightmost
    revealed icon when none is held, and `pad_build_pick` to move to the
    nearest revealed icon on the requested side, so that the build bar moves
    as in C.
41. As a plan 3 developer, I want `pad_build_icon` to return the held icon
    only while it is still revealed, so that a hidden icon is never chosen.
42. As a maintainer, I want every header constant of the step (directions,
    cycle panels, select events and frame counts, slipway phases and
    outcomes, jump delays, margins and speed, the any-layer value, the
    perpendicular weight, the chord count) and `directions_by_priority`
    compared with the C by `lua-test-modules`, so that a drift fails
    `make test`.
43. As a maintainer, I want `make checklist STEP=3.11` to pass with 89/89
    functions and 1/1 tables, so that the step is complete by the
    authoritative assignment.
44. As a maintainer, I want `lua-guard` to accept every new file (compiles,
    no undeclared global, no `love.` outside `game/love/`), so that the
    static rules hold.
45. As a maintainer, I want `LUA_DEFERRED_MODULES` to stay empty and every
    new module to load at once, so that the load check covers the whole tree.
46. As a maintainer, I want the own times of `lua-guard` and
    `lua-test-modules` measured against the "before" commit and recorded, so
    that the plan's timing budget is tracked.
47. As a maintainer, I want the step note to record the files, counts, load
    finding, behaviour checks and figures, so that 3.12 to 3.15 start from
    facts.
48. As a plan 3 developer, I want behaviour checked against the C with
    throwaway programs before each commit, so that faithful-copy errors are
    caught before the replays of plan 3.

## Implementation Decisions

- **Modules.** Ten new modules, each headed by a comment naming its C files
  and the shape of its structs, exported functions `function M.<c name>(`,
  statics `local function`, the one table and header constants as module
  fields, in C order (header, then `.c`). `pad_direction` follows 3.1's rule
  for a header without a `.c` whose constants Lua needs (as
  `engine.zone_sizes`, `game.targeting`). Constants live only in their
  defining module: `pad_target`, `pad_jump` and `pad_panel` read
  `pad_direction.PAD_DIRECTION_*`, never re-export them (3.6's rule).
- **Load plan.** No cycle: each module requires at load every module it
  calls, all already in the tree; none requires another 3.11 module except
  along the C include order (`pad_back`, `pad_jump`, `pad_select` →
  `pad_target`; `pad_build` → `pad_panel`; `pad_target`, `pad_jump`,
  `pad_panel` → `pad_direction`). Nothing in the tree requires them back.
  `LUA_DEFERRED_MODULES` stays empty; the `--defer` mechanism is untouched.
- **Structs.** `pad_back_t { last_acted }`, `pad_build_t { open, icon }`,
  `pad_cycle_t { panel, landing_due }`, `pad_jump_t { hovered, seeded,
  repeat_direction, repeat_frames }`, `pad_select_t { held_frames, fired,
  chain_frames }`, `pad_slipway_t { start, target, phase, linked_before }`,
  `cheat_t { previous_first, previous_second, chords }`: each init fills the
  caller's table with every field at its C value (a compound literal zeroes
  the fields it leaves out). Ids are new tables wherever the C stores one
  (`entity_id_link`, a copy of `ENTITY_ID_NONE` per use, 3.2's rule) and are
  compared only through `entity_id_equal`.
- **Field-name traps.** A planet's own bits are `has_` (`PLANET_HAS_NEEDS`),
  a button's own bits are `has_` (`BUTTON_HAS_SLIDE_TARGET`); the header's
  `has` (`ENTITY_HAS_LAYER`, `ENTITY_HAS_HIT_BOX`) is read through
  `entity_has`. `&camera->header` and `ui->header.id` are the entity table
  itself and its `id` field.
- **By-value structs.** `offset_t { along, perpendicular }`, `box_t`,
  `mouse_pos_t { world, screen, has }`, `panel_key_t { ui, economy,
  ephemerals_count, generation }` and `candidate_iter_t { from_ephemerals,
  index }` are new tables built listing every field; `best_offset = offset`
  keeps the reference (never mutated). `candidates_next` takes the iterator
  table and advances its `index` in place, as the C pointer does.
- **Statics.** `pad_panel`'s `g_memo` is a module-local table `{ in_step = 0,
  known = 0, key = <zero panel_key_t>, open = 0 }`; `pad_jump`'s
  `inner_left`, `inner_right`, `inner_top`, `inner_bottom` are module locals
  computed from `vram.P8_SCREEN_WIDTH`/`P8_SCREEN_HEIGHT` as in C, not
  compared. `.c`-local enums (`CHORD_FIRST`, `CHORD_SECOND`,
  `SCREEN_SPACE_LAYER`) and `INT_MAX` (2147483647) are module locals.
- **Out-parameters.** `pad_jump_scroll(x, y, dx, dy)` returns `dx, dy`;
  `pad_jump_world_point(e, x, y)` returns `x, y`; `pad_panel_focus_point`
  and `box_middle` return `x, y`; `pad_panel_step(focus, choose_held,
  direction, x, y, mouse_buttons)` returns `at, x, y, mouse_buttons`, with
  `x, y` handed back unchanged when nothing is focused.
- **Function pointers.** `accept` in `pad_panel_entity_under` and the
  `entity_list_filter` predicates (`is_candidate`, `is_not_start`) are Lua
  functions returning 1/0; `hit_test` results are tested `~= 0`. Class
  comparisons (`planet_class()`, `modal_backdrop_class()`, `button_class()`,
  `main_economy_placeholder_class()`) and the `panel_open_now` assert on
  `entity_registry_hit_test_always` are by identity.
- **Integers.** `(start + step) % count` in `pad_select_next` is `cint.mod`;
  bit tests use `bit.band`, `seeded |= …` is `bit.bor`. `perpendicular *
  PAD_TARGET_PERPENDICULAR_WEIGHT` and the squared distances of
  `pad_jump_nearest` stay plain Lua arithmetic: their operands are map
  coordinates and screen pixels far below 2^30 (the C would be undefined
  beyond), so no wrap is written.
- **Switches.** `offset_in`, `direction_mask`, `neighbor_score` and
  `pad_cycle_next` are `if` chains keeping the C `default`; `pad_cycle_next`
  ends with `assert(false, "<C message>")` then returns `PAD_CYCLE_CLOSED`.
- **Reference units.** One unit per C source holding a constant or the
  table, appended to `TABLE_REFERENCE_SOURCES`: `src/game/pad_direction.h`,
  `src/game/pad_target.h`, `src/game/pad_cycle.h`, `src/game/cheat.h`,
  `src/game/pad_jump.c` (the static table plus the header constants),
  `src/game/pad_select.h`, `src/game/pad_slipway.h`, `src/game/pad_panel.h`.
  `pad_back` and `pad_build` have neither: no unit. No module uses a
  `sizeof`, so no `SIZEOF` entry.
- **Assert messages** are the C ones, verbatim.
- **No guideline question was needed:** the load plan follows from the
  include graph and the stub experiment, and every convention point is
  settled by the 2.1 to 3.10 notes.

## Implementation Chunks

1. **Direction, target, back, cycle, cheat.** `pad_direction` (5 constants),
   `pad_target` (4 functions), `pad_back` (3), `pad_cycle` (7), `cheat` (4),
   with the reference units for `pad_direction`, `pad_target`, `pad_cycle`
   and `cheat`. Done when they load, their constants compare equal, and
   throwaway C-vs-Lua programs match on target picks over a galaxy for the
   four directions (ties included), back targets with dead and hovered
   entities, the cycle, and cheat chords over random button streams (both
   economy phases).
2. **Jump, select, slipway.** `pad_jump` (21 functions, 1 table),
   `pad_select` (7), `pad_slipway` (8), with their reference units. Done when
   the table and constants compare equal and throwaway programs match on
   repeat steps over random button streams for the jump and slipway modes,
   inside/scroll over the whole screen and beyond, seeds, nearest and
   candidate lists over a galaxy, select streams with `allowed` and chain
   windows, attention cycling over settled planets with needs, and slipway
   start, candidates, between, outcome, release and rebase.
3. **Panel and build bar.** `pad_panel` (27 functions), `pad_build` (8), the
   `pad_panel` reference unit. Done when `make checklist STEP=3.11` passes
   (89/89, 1/1) and throwaway programs match on panel open with and without
   the memo, entity under the cursor, items, first item and neighbours over
   message boxes, the tech panel and the score panel (sliding rows
   included), panel steps, and build-bar focus and picks over revealed icons.
4. **Own-time gate and step note.** `make test` green. Measure the own times
   of `lua-guard` and `lua-test-modules` paired against the "before" commit
   `d7c6a3f` (45 pairs, as 3.3 to 3.10 did), with KB of Lua, compared blocks
   and reference lines added; pass when each stays under 100 ms and grows
   within 2× of 3.1's cost per KB and per table. 3.11 is not a checkpoint: no
   full rule-4 protocol unless the gate fails. No other agent runs during
   the measurement. Write the step 3.11 note in `docs/love-port/plan-2.md`:
   the files, checklist counts, the load finding (all ten load at once,
   `LUA_DEFERRED_MODULES` still empty), the behaviour checks, the two
   own-time figures and the projection to the end of the plan.

## Testing Decisions

- A good test checks external behaviour only: what a function returns and
  what it leaves in its state table, compared with the C original on the
  same inputs; never the shape of the Lua code.
- As 3.1 to 3.10 decided, no permanent behaviour tests for these modules:
  the plan 3 replays cover them. The permanent checks are the existing three:
  `lua-guard`, `lua-test-modules` (every module loads, every table and
  constant equals the C), and `make checklist STEP=3.11`. No `make test`
  target is added.
- Behaviour is checked before each commit with throwaway programs that run
  the C function and the Lua function on the same inputs and diff their
  output byte for byte (prior art: the step 3.4 to 3.10 notes; the reference
  writer pattern of `tools/fixed_reference.c` and `tests/lua/test_fixed.lua`).
  They are not committed. Button streams are seeded random sequences long
  enough to hit every repeat and chain branch.
- Data tables and constants: prior art `tools/table_reference/game_targeting.c`
  (header enums), `game_bindings.c` (X-macro constant list), `game_hud.c`
  (an array of a `.c` static).

## Out of Scope

- `pad_nav` and the menus, title and attract (3.13), save (3.12), the cart
  and the input step (3.14), `main.lua` and `debug_state()` (3.15).
- `src/devtools/pad_metrics.c` and `save_check.c` (not ported).
- Running the e2e replays or `make love-e2e` (plan 3).
- Audio, redesign, and any C change.

## Further Notes

- Before commit for the gate: `d7c6a3f` (docs: record step 3.10 of plan 2).
- Projection: the C files total 35.6 KB, so about 30–36 KB of Lua; at 3.10's
  0.083 ms per KB, `lua-guard` grows from about 68 ms to about 71 ms;
  `lua-test-modules` gains ten small modules, one table and 29
  constants, a few ms at most.
- The step 3.10 note's warning on nil planet fields (`sprite`,
  `palette_block`, `acceleration_factor`, `speed`) does not reach this step:
  no 3.11 function reads them.
