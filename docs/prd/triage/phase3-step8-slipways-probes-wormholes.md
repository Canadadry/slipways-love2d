---
title: "Plan 2, step 3.8: the include cycle, slipways, probes and wormholes"
description: "Port particle_zone, probe, slipway and wormhole to Lua as faithful copies, the third slice of the include cycle; particle_zone loads now, probe waits for particle (3.9), slipway and wormhole for their display modules (3.10)."
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
> 3.8. `game/game/` the include cycle, slipways, probes and wormholes.

## Problem Statement

Plan 3 will put any Lua function next to its C original when an e2e replay
diverges, so every C function and table of the game needs a faithful Lua
copy. Steps 3.6 and 3.7 ported the galaxy, the planets and the economy, the
first two slices of the include cycle. What the player builds and aims is
still missing: there is no slipway (the route dragged between two planets,
its cost, its obstruction test, its served planets), no probe (the placed
scanner and its particle trail), no wormhole (the fold placed near an
anchor, revealing planets as it unfolds), no particle slot zone, and no
targeting logic, which `probe.c` defines for every host (probes, structures,
slipways, wormholes). Already ported modules call into these: `planet_family`
holds probe's targeting methods in its class tables, `planet_display` builds
slipways and reads `targeting_of`, `economy_tiers` adds served planets,
`tech_gate`, `economy_score` and `planet_display` read the main economy
through `main_economy_typed`.

The checklist (`luajit tools/checklist.lua --listing`) assigns 8 C files to
step 3.8, with 100 functions and 13 tables, all MISSING:

- `src/game/particle_zone.{c,h}`: 12 functions; `SLOT_KINDS`.
- `src/game/probe.{c,h}`: 35 functions (probe, the targeting functions of
  `targeting.h`, the main-economy functions of `main_economy.h`);
  `main_economy_placeholder_class_def`, `g_main_economy_placeholder`,
  `targeting_track_target_class_def`, `g_targeting_track_target`,
  `probe_class_def`.
- `src/game/slipway.{c,h}`: 31 functions; `slipway_too_far_class_def`,
  `g_slipway_too_far`, `slipway_mouse_point_class_def`, `slipway_class_def`.
- `src/game/wormhole.{c,h}`: 22 functions;
  `wormhole_too_far_target_class_def`, `g_wormhole_too_far_target_wrapper`,
  `wormhole_class_def`.

probe, slipway and wormhole are members of the cycle; particle_zone is a
leaf (its includes are engine headers only) placed in 3.8 by the include
graph (step 1.1's note). The step must settle, with evidence from the C,
which modules load before their partners exist, and which join
`LUA_DEFERRED_MODULES`.

## Solution

Four new Lua modules under `game/game/`: `particle_zone`, `probe`, `slipway`
and `wormhole`, each a faithful copy of its C pair (header, then `.c`; same
functions in the same order, same names, same computations, same tables),
following the conventions of the step notes of 2.1, 3.1, 3.3 to 3.7. Their
tables, sizes and constants are registered with the data check; `make
checklist STEP=3.8` passes (100/100, 13/13).

### Load finding (the decision of this step)

- **`game.particle_zone` loads now.** Its load-time values are its constants,
  its slot storages and `SLOT_KINDS` (engine values only, built as
  `ui_zone.lua` builds its own) and the `SLOT_GROUP`; no cycle partner is
  involved.
- **`game.probe` cannot load before 3.9 and joins `LUA_DEFERRED_MODULES`.**
  `probe_class_def.draw` is `emitter_update_method`, defined in `particle.c`
  (step 3.9): the class table, built at load, holds it by identity, so probe
  requires `game.particle` at load (3.3's one-direction rule). The other
  direction is free: `particle.c` has no file-level value at all (only
  `g_pixels_hidden`), and reaches probe only inside `emitter_of`, so
  particle requires `game.probe` at call time. Every other partner use of
  probe is inside a function body (`economy_spend`, `economy_tick_clock`,
  `economy_advance_clock`, `economy_class`; `planet_is_family`,
  `planet_class`; `slipway_class`, `slipway_start`, `slipway_end`,
  `slipway_end_map_pos`, `slipway_too_far`, `slipway_too_far_class`;
  `wormhole_class`, `wormhole_too_far_target_wrapper`,
  `wormhole_too_far_target_class`; `ui_panel_box`, `main_mouse_typed`), so
  those are required at call time. probe exports `targeting_tooltip_method`,
  `targeting_track_method`, `targeting_confirm_method` and
  `probe_spawn_particle` as module fields (3.6's "For 3.8") and requires
  neither `game.planet_family` nor `game.economy` at load (3.6's and 3.7's
  "For 3.8").
- **`game.slipway` cannot load before 3.10 and joins `LUA_DEFERRED_MODULES`.**
  `slipway_class_def.draw` is `slipway_draw_method`, declared in `slipway.h`
  but defined in `slipway_display.c`, which the checklist assigns to step
  3.10 (outside the cycle). slipway therefore requires
  `game.slipway_display` at load. `slipway_display.c` has no file-level
  value, so 3.10 requires `game.slipway` from it at call time. slipway's
  other partner uses (probe's targeting functions, `economy_advance_clock`,
  `main_economy_typed`, `planet_is_family`, `planet_class`, `main_ui`,
  `ui_hovered`) are inside functions.
- **`game.wormhole` cannot load before 3.10 and joins
  `LUA_DEFERRED_MODULES`.** `wormhole_class_def` holds
  `targeting_tooltip_method` (probe, deferred to 3.9) and
  `wormhole_draw_method` (defined in `wormhole_display.c`, step 3.10, no
  file-level value). wormhole requires both at load. Its other partner uses
  (`targeting_track`, `targeting_set_target`, `targeting_confirm`,
  `planet_is_family`, `planet_roll_type`) are inside functions.
- `LUA_DEFERRED_MODULES` becomes `game.planet_family game.probe game.slipway
  game.wormhole`. 3.9 drops `game.planet_family` and `game.probe` (they load
  once `game.particle` exists); 3.10 drops `game.slipway` and
  `game.wormhole`. This amends 3.6's "3.9 empties it", recorded in the step
  note.
- Decided by the `guideline` skill, run in place (no Agent tool was
  available), on the question "hold the 3.10 draw methods by identity and
  defer slipway and wormhole to 3.10, or wrap them in Lua-only forwarders so
  the two modules load at 3.9":

  ```
  DECISION: slipway and wormhole hold slipway_display.slipway_draw_method and
  wormhole_display.wormhole_draw_method by identity, requiring those modules at
  load, and join LUA_DEFERRED_MODULES until 3.10.
  WHY: Plan 2's faithful-copy rule and 3.6's conventions (class tables hold the
  C methods; a function declared in a header but defined in a sibling module is
  called from its defining module, not re-exported); --defer already handles a
  module missing from the tree, and a forwarder is a function the C lacks.
  ```

### Zero constructors moved home

- `slipway_keys_t` (local `slipway_keys_zero` in `planet_display.lua`, 3.6)
  **moves home now**: `slipway` exports `M.slipway_keys_zero()` (Lua only,
  after the C functions, as `planet` exports `planet_keys_zero`), and
  `planet_display`'s `planet_left_press` calls it through the `slipway` it
  already requires at call time at that site; the local and its line in the
  top comment are deleted. Two later callers need it too
  (`planet_family_trade`, 3.10; `save_load`, 3.12).
- `targeting_t` (local `targeting_zero` in `planet.lua`, 3.6) **moves home
  now**: `targeting.h` owns the struct and its functions arrive with this
  step; `game.targeting` (a leaf every user already requires at load) gains
  `M.targeting_zero()` (Lua only) and `planet.lua` uses it. probe, slipway
  and wormhole use it too (four users instead of four copies).
- `probe_keys_t` gets **no** constructor: every C caller of `probe_new`
  passes `NULL` (`background.c`, `save_load.c`).
- `emitter_t` (owned by `particle`, 3.9) stays local: probe gets its own
  local `emitter_zero` (copy of `planet.lua`'s), recorded for 3.9 to fold.
- `toast_keys_t` and `button_t` (3.9) are not used by this step's modules.

## User Stories

1. As the plan-3 debugger, I want `game/game/probe.lua` to hold every function of `probe.h` and `probe.c` in C order with the C names, the targeting and main-economy functions included, so that I can put any of them next to its C original when a replay diverges.
2. As the plan-3 debugger, I want `targeting_track` to pick the nearest node under the attach distance, else the first slipway near the segment, else the throwaway target outside the panel box, else nothing, exactly as the C, so that probes and structures aim at the same targets.
3. As the plan-3 debugger, I want `targeting_set_target` and `targeting_target` to encode the four target kinds (entity, throwaway, slipway too far, wormhole too far) as the C does, so that a target survives its entity being flushed in the same way.
4. As the plan-3 debugger, I want `probe_link_target` to write `targeted_by` on the same struct with the same asserts, so that a planet's or a slipway's highlight follows the same host.
5. As the plan-3 debugger, I want `targeting_charge` to call `economy_spend` only without a target, then set the confirmed state or raise `is_dead`, so that credits are spent on the same frames.
6. As the plan-3 debugger, I want `targeting_confirm` to tick or advance the clock by the archetype's choice, so that the year moves on the same confirmation.
7. As the plan-3 debugger, I want `probe_spawn_particle` to draw `rnd(angle_slices)` before `rnd(0.5)`, then the radius draw, so that the visual random stream stays aligned with the C.
8. As the plan-3 debugger, I want `targeting_tooltip_show` to print the same status text, cost text or sprite at the same position, so that the placing tooltip draws identically.
9. As the plan-3 debugger, I want `main_economy`, `main_economy_typed`, `main_economy_placeholder` and `probe_set_main_economy` to switch between none, placeholder and real as the C does, so that every module reading the main economy sees the same object.
10. As the plan-3 debugger, I want `probe_create` to open and close the grab arena with the same byte count and apply the keys as the C does, so that the grab arena's records match.
11. As the plan-3 debugger, I want `game/game/slipway.lua` to copy the drag (hovered drop target or mouse point), the route cost and the infraspace inheritance exactly, so that the dragged slipway costs and layers the same.
12. As the plan-3 debugger, I want `slipway_obstruction` to return the too-far struct, an existing slipway between the same ends, or the first node or crossing slipway, in the C order, so that refusals match.
13. As the plan-3 debugger, I want `slipway_drop` to refuse, pay without clock, attach both ends and advance the clock once, as the C does, so that a confirmed slipway changes the same state.
14. As the plan-3 debugger, I want `slipway_served_planets_add` and `slipway_served_planets_has` to keep the same order, cap and asserts, so that served planets feed the tiers identically.
15. As the plan-3 debugger, I want `slipway_release_method` to release every transit dot to the particle zone and give back the grab arena only while placing, so that slot counts match.
16. As the plan-3 debugger, I want `slipway_other_end` to fall through to the start when the end is missing, as the C comment states, so that route walks match.
17. As the plan-3 debugger, I want `game/game/wormhole.lua` to collect the same anchors in live-list order and size its arena by their count, so that the nearest-anchor search matches.
18. As the plan-3 debugger, I want `wormhole_update_placing`, `wormhole_reveal_radius` and `wormhole_update_unfolding` to compute radii with the same fixed-point operations and roll the same unknown planets, so that unfolding reveals the same planets on the same frame.
19. As the plan-3 debugger, I want `wormhole_drop` to skip `targeting_confirm` before tick 15, so that its side effects never run early.
20. As the plan-3 debugger, I want `game/game/particle_zone.lua` to hold the five slot kinds with the C sizes, counts, names and zone-full messages, so that particle overflows fire at the same moment.
21. As the plan author, I want `make checklist STEP=3.8` to pass (100/100 functions, 13/13 tables), so that the step is verifiably complete.
22. As the plan author, I want the 13 tables and every constant and `sizeof` the modules use registered with the data check, so that they are compared with the C as soon as their module loads.
23. As the plan author, I want `particle_zone` loaded and compared by `lua-test-modules` now, so that an error in it fails `make test` at this step.
24. As the plan author, I want probe, slipway and wormhole in `LUA_DEFERRED_MODULES` with the evidence recorded, so that the deferral is justified and the check fails the moment one of them loads.
25. As a later-step agent writing `particle` (3.9), I want to know that probe requires `game.particle` at load and that particle must require `game.probe` at call time only, so that no load loop appears.
26. As a later-step agent writing `slipway_display` and `wormhole_display` (3.10), I want to know that slipway and wormhole require them at load and that they must require `game.slipway` and `game.wormhole` at call time only, so that no load loop appears.
27. As a later-step agent at 3.9 and 3.10, I want the deferred list's emptying schedule recorded (3.9 drops planet_family and probe; 3.10 drops slipway and wormhole), so that each step knows what it must drop.
28. As a later-step agent writing `planet_family_trade` (3.10) or `save_load` (3.12), I want `slipway.slipway_keys_zero()` available, so that I build slipway keys from their owner.
29. As a later-step agent writing `particle` (3.9), I want probe's local `emitter_zero` recorded next to planet's, so that I can fold both into the owner.
30. As a later-step agent writing save and state code, I want the wormhole's own `has` named `has_` and the slipway's `end` named `end_`, so that I read the right fields.
31. As the maintainer of `planet.lua` and `planet_display.lua`, I want their local `targeting_zero` and `slipway_keys_zero` replaced by the owners' constructors, so that each struct has one constructor.
32. As a player of the Lua game (plan 3 onward), I want dragging, pricing, refusing and confirming slipways to behave as in the C, so that the Lua game plays the same.
33. As a player, I want probes and wormholes to be placed, refused, paid for and revealed as in the C, so that exploration matches.
34. As the maintainer of `make test`, I want the step's own-time gate measured (paired, 45 pairs, against the before commit), so that `lua-guard` and `lua-test-modules` stay under 100 ms and within 2× of 3.1's per-KB and per-table costs.
35. As the maintainer of `make test`, I want no new `make test` target and no new committed test, so that the fast loop grows only by the files and tables the step adds.
36. As the reviewer, I want behaviour checked against the C with throwaway programs, so that the copy is shown faithful without committing behaviour tests the replays will cover.
37. As the reviewer, I want each decision this step took without a human recorded in the step note, so that the next agent does not re-litigate it.

## Implementation Decisions

### Modules and their requires

- **`game.particle_zone`** (copy of `particle_zone.h` then `particle_zone.c`,
  modelled on `ui_zone.lua`): the `PARTICLE_SLOT_*` enum as fields
  (`PARTICLE_SLOT_PARTICLE` 0 … `PARTICLE_SLOT_COUNT` 5); `PARTICLE_SLOT`
  and `TRANSIT_DOTS_SLOT` through `zone.ZONE_BYTES_BY_WIDTH`;
  `PARTICLES_CAPACITY`, `PARTICLES_HELP_SCREEN_CAPACITY`; the `.c`-local
  macros as module locals; the two compile-time checks as load-time asserts
  (3.1's rule); five `SLOT_ZONE_STORAGE` calls; `M.SLOT_KINDS` (0-indexed,
  `nil` slot names as in C); a module-local `g_group` from `SLOT_GROUP`; the
  12 functions in C order. `M.SIZEOF` holds `pointer_list_t` and `void *`.
  `particle_zone_record_elements` keeps the C assert message. Requires at
  load: `engine.allocator`, `engine.slot_group`, `engine.zone`,
  `engine.zone_sizes`.
- **`game.probe`** (copy of `probe.h` then `probe.c`): module locals
  `g_probe_up`, `g_main_economy_state` (with the `.c`-local enum as locals),
  `g_main_economy` (a zero id); `M.main_economy_placeholder_class_def` and
  `M.targeting_track_target_class_def` (zero classes, as
  `economy_class_def`); `M.g_main_economy_placeholder` and
  `M.g_targeting_track_target` (header through
  `entity_class.entity_init_header(t, <class>)`, as
  `entity_registry.g_main_camera_placeholder`, then the C fields at their
  initializer values); `M.probe_class_def` (methods by identity:
  `particle.emitter_update_method`, `M.targeting_tooltip_method`,
  `M.targeting_track_method`, `M.targeting_confirm_method`,
  `M.probe_spawn_particle`, `M.probe_release_method`; tags and lineage as
  the C); the functions in C order, statics as `local function` (forward
  declared when used first), exported ones as `M.` fields. Required at load:
  `bit`, engine modules (`allocator`, `dispatch_context`, `entity`,
  `entity_class`, `entity_geom`, `entity_id`, `entity_list`,
  `entity_registry`, `frame_zone`, `geom`, `vec`, `zone`), `game.game_data`,
  `game.grab_arena`, `game.hud`, `game.particle`, `game.targeting`,
  `game.visual_random`, `p8.fixed`, `p8.gfx`, `p8.str`. Required at call
  time: `game.economy`, `game.planet`, `game.slipway`, `game.ui`,
  `game.wormhole`. A partner's constant (`PLANET_HAS_TARGETING`) is read at
  call time through the module required in that function (3.3's rule).
- **`game.slipway`** (copy of `slipway.h` then `slipway.c`): the two enum
  constants, `SLIPWAY_CONSTRUCTION_BYTES` computed from `M.SIZEOF` and
  `zone.ZONE_BYTES` as the macro does; `M.SIZEOF` holds `slipway_t` and
  `void *`; module local `g_slipway_up`; `M.slipway_too_far_class_def`,
  `M.slipway_mouse_point_class_def` (zero classes), `M.g_slipway_too_far`
  (header with its class, `status_text = "-too far-"`, `targeted_by` a zero
  id); `M.slipway_class_def` (`draw = slipway_display.slipway_draw_method`,
  `draw_overlay`, `drag`, `drop`, `release` the local method functions,
  `tags`); the functions in C order; then `M.slipway_keys_zero()` (Lua
  only). Required at load: `bit`, engine modules (`allocator`,
  `dispatch_context`, `entity`, `entity_class`, `entity_id`,
  `entity_id_list`, `entity_list`, `entity_registry`, `frame_zone`, `geom`,
  `pointer_list`, `vec`, `zone`), `game.economy_formulas`,
  `game.grab_arena`, `game.particle_zone`, `game.slipway_display`,
  `game.targeting`, `game.tech_gate`, `p8.fixed`. Call time: `game.economy`,
  `game.planet`, `game.probe`, `game.ui`.
- **`game.wormhole`** (copy of `wormhole.h` then `wormhole.c`):
  `WORMHOLE_HAS_TOO_FAR_TARGET`; `M.SIZEOF` holds `wormhole_t` and
  `entity_id_t`; module local `g_wormhole_up`;
  `M.wormhole_too_far_target_class_def` (zero class),
  `M.g_wormhole_too_far_target_wrapper` (header with its class,
  `status_text = game_data.wormhole_too_far_target_status_text`);
  `M.wormhole_class_def` (`draw = wormhole_display.wormhole_draw_method`,
  `draw_overlay = probe.targeting_tooltip_method`, the local drag, drop and
  release methods, `states` with the placing and unfolding methods at their
  state indexes, `tags`); the functions in C order. Required at load:
  engine modules (`allocator`, `entity`, `entity_class`, `entity_geom`,
  `entity_id`, `entity_id_list`, `entity_list`, `entity_registry`, `zone`),
  `game.game_data`, `game.grab_arena`, `game.probe`, `game.targeting`,
  `game.tech_gate`, `game.wormhole_display`, `p8.fixed`. Call time:
  `game.planet`.
- probe and wormhole are required at load only by each other in one
  direction (wormhole → probe); probe reaches wormhole at call time.

### Shapes and field names

- Every entity struct is one table holding the header fields and its own
  fields (first-member rule). **The wormhole's own `has`
  (`WORMHOLE_HAS_*` bits) is `has_`** (3.6's rule; 3.6's note names
  `wormhole_t`). The emitter's `has` stays `has` (a sub-table, no clash).
- **The slipway's `end` is `end_`** (2.1's keyword rule); `start` stays.
- Inline sub-structs are created at init as 3.6 does: `targeting` by
  `targeting.targeting_zero()` then written field by field; probe's
  `emitter` by a local `emitter_zero` (from `particle.h`'s field list,
  struct owned by step 3.9; may move there); slipway's `transit_dots` and
  wormhole's `anchors` by a local list zero (`(pointer_list_t){0}`,
  `(entity_id_list_t){0}`) then filled by `pointer_list_init_reserve` /
  `entity_id_list_init_reserve`; slipway's `served_planets` as
  `{ count = 0, ids = {} }` (`ids` 0-indexed Lua strings; `strcpy` stores
  the string, the length assert is `#id < PLANET_ID_SIZE` read at call time
  from `game.planet`); slipway's `point` a new table filled by
  `entity_init_header(point, M.slipway_mouse_point_class_def)`.
- `targeting_t` is `{ archetype, target, cost, has_cost }`, `target` a
  `targeting_target_t` `{ kind, id }`; `targeting_set_target` builds a new
  `target` table (a by-value assignment) and never mutates the old one.
- A pointer compared with the address of a static (`et ==
  &g_targeting_track_target.header`, `target == slipway_too_far()`) is a
  table identity comparison; a class compared with a class (`e.class ==
  slipway_class()`, `cls.drag == targeting_track_method`) likewise.
- `probe_t *` cast from `entity_t *` is the identity; `(probe_t *)e` reads
  the same table.
- `entity_geom_nearest_except` and `entity_geom_nearest_in_ids` return the
  distance then the nearest entity (out-parameter rule, as their Lua copies
  already do); `entity_id_list_iter_next` follows its existing Lua shape.
- Predicate contexts (`targeting_track_ctx_t`,
  `slipway_obstruction_node_ctx_t`, `slipway_obstruction_slipway_ctx_t`) are
  plain tables with the C field names; predicates return 1/0.
- `entity_list_filter` and `entity_list_concat` return new lists (by value).
- `probe_target_status_text` returns a Lua string or `nil`;
  `targeting_tooltip_show`'s cost text goes through `p8_concat2` and
  `p8_tostr_fixed` on the frame zone (3.1's zone-string rule).
- `probe_host_raise_is_dead`, `slipway_drop` and `wormhole_drop` write
  `is_dead` as 1/0 ints; `wormhole_update_unfolding` writes
  `(t > 25) and 1 or 0`.
- `slipway_inherit_infraspace` clears the bit with
  `bit.band(has, bit.bnot(ENTITY_HAS_INFRASPACE))` brought back to 16 bits
  (`bit.band(…, 0xFFFF)`), as the C `(uint16_t)~` cast does.
- C ternaries choosing a value that may be `nil` (`je ~= nil and … or …`,
  `end ~= NULL ? … : zero`) are `if`/`else` when the chosen value can be
  `nil` or `false` (2.1's rule).

### Data check registration

- `tools/table_reference/reference_class.h` gains a `static inline
  reference_header(module, name, e, class_name, cls)` rendering an entity
  header and its class pointer, moved out of
  `engine_entity_registry.c`'s local `render_header` (which then calls it);
  same lines, so the registry block does not change.
- Four new units, each including its `.c`, added to
  `TABLE_REFERENCE_SOURCES` (`src/game/particle_zone.c`,
  `src/game/probe.c`, `src/game/slipway.c`, `src/game/wormhole.c`):
  - `game_particle_zone.c`: the six enum values, `PARTICLE_SLOT`,
    `TRANSIT_DOTS_SLOT`, `PARTICLES_CAPACITY`,
    `PARTICLES_HELP_SCREEN_CAPACITY`, `SLOT_KINDS` rendered as
    `game_ui_zone.c` renders its own (names, sizes, counts, every `taken`
    entry: about 1,370 lines), `SIZEOF.pointer_list_t`, `SIZEOF["void *"]`.
  - `game_probe.c`: the three classes through `reference_class`; the two
    statics through `reference_header` plus their own fields
    (`research_points`, `has_research_points`, `credits`, `has_credits`;
    `targeted_by.index`, `targeted_by.generation`); `SIZEOF.probe_t`.
  - `game_slipway.c`: the three classes; `g_slipway_too_far` (header,
    `status_text`, `targeted_by`); the two enum constants,
    `SLIPWAY_CONSTRUCTION_BYTES`, `SIZEOF.slipway_t`, `SIZEOF["void *"]`.
  - `game_wormhole.c`: the two classes; `g_wormhole_too_far_target_wrapper`
    (header, `status_text`); `WORMHOLE_HAS_TOO_FAR_TARGET`,
    `SIZEOF.wormhole_t`, `SIZEOF.entity_id_t`.
- The deferred modules' blocks are skipped by `lua-test-modules` until they
  load (3.6's mechanism); `make checklist STEP=3.8` still requires every
  table to be registered.

### Deferred modules

- `LUA_DEFERRED_MODULES := game.planet_family game.probe game.slipway
  game.wormhole`, each added in the chunk that adds its file. The checker
  is unchanged: each fails on `module 'game.particle' not found`,
  `'game.slipway_display'` or `'game.wormhole_display'` (not in the tree),
  which it accepts; any other load error fails `make test`.

### Verification of the step

- `make checklist STEP=3.8` passes: 100/100 functions, 13/13 tables.
- `make test` green: `lua-guard` compiles the four files and finds no
  undeclared global; `lua-test-modules` loads `particle_zone`, compares its
  block, and reports four deferred modules.
- Behaviour checked against the C with throwaway programs, not committed (as
  in 3.4 to 3.7), outputs compared byte for byte. The missing modules
  (`game.particle`, `game.slipway_display`, `game.wormhole_display`,
  `game.ui`, `game.message_box`) are stubbed through `package.preload` in
  the throwaway script only (`particle_take` returns a fresh table,
  `ui_panel_box` a fixed box, `main_ui` / `ui_hovered` a scripted hovered
  entity, `main_mouse_typed` `nil` or a mouse); the C side links the real
  game. Scenarios: particle_zone takes and releases on every kind up to the
  zone-full message; on a seeded galaxy, `targeting_track` at a grid of
  mouse positions (target kind, linked `targeted_by`), `targeting_confirm`
  with enough and too little credit, the clock choice, `probe_spawn_particle`
  over a run of state times (random stream), the main-economy switches;
  slipway drags to a hovered planet and to the mouse point (cost, layer,
  infraspace), `slipway_obstruction` on too far, an existing slipway, a node
  near the segment and a crossing slipway, `slipway_drop` refused and
  confirmed, served planets with duplicates, `slipway_other_end`;
  `wormhole_construct` anchors, drags under and over the threshold with and
  without "space folding", `wormhole_update_placing` and
  `wormhole_reveal_radius` over t = 0 … 60, `wormhole_drop` at t 14 and 15,
  `wormhole_update_unfolding` rolling unknown planets.

## Implementation Chunks

1. **`particle_zone`**: the module, its constants, `SLOT_KINDS` and the slot
   group; the `game_particle_zone.c` unit and its
   `TABLE_REFERENCE_SOURCES` entry; throwaway checks of takes, releases,
   counts and the zone-full messages. Loads now; its block compared.
2. **`probe`**: the module (probe, targeting and main-economy functions),
   its classes and statics, the local `emitter_zero`; `targeting_zero`
   moved into `game.targeting` and used by `planet.lua`; `reference_header`
   moved into `reference_class.h`; the `game_probe.c` unit;
   `game.probe` added to `LUA_DEFERRED_MODULES`; throwaway checks of
   targeting, charging, confirming, the particle spawn and the main-economy
   switches.
3. **`slipway`**: the module, its classes and statics,
   `slipway_keys_zero` exported and used by `planet_display.lua` (its local
   removed); the `game_slipway.c` unit; `game.slipway` added to
   `LUA_DEFERRED_MODULES`; throwaway checks of drag, obstruction, drop,
   served planets, other end and release.
4. **`wormhole`**: the module, its classes and static; the
   `game_wormhole.c` unit; `game.wormhole` added to
   `LUA_DEFERRED_MODULES`; throwaway checks of construct, drag, placing,
   drop and unfolding. `make checklist STEP=3.8` passes.
5. **Own-time gate and step note**: the own times of `lua-guard` and
   `lua-test-modules`, 45 pairs against the step's before commit (the last
   commit before chunk 1, currently `cd653b0`), each under 100 ms and within
   2× of 3.1's per-KB and per-table costs; KB of Lua added, blocks and
   reference lines added (deferred ones counted apart); the projection to
   the end of the plan. 3.8 is not a checkpoint: no full rule-4 protocol
   unless the gate fails. The step note in `docs/love-port/plan-2.md`
   records the files, the counts (100/100, 13/13), the load finding (which
   module waits for which, with the evidence), the new deferred list and its
   emptying schedule (3.9: planet_family, probe; 3.10: slipway, wormhole),
   the `guideline` decision on identity versus forwarders, the zero
   constructors moved home (`slipway_keys_t`, `targeting_t`) and kept local
   (`emitter_t`), `has_` and `end_`, `reference_header`, what 3.9 and 3.10
   inherit (see Further Notes), the throwaway checks, and the gate figures.

## Testing Decisions

- A good test checks external behaviour through the public interface, never
  module internals.
- No new committed test: the data check (`lua-test-modules`) is the committed
  check for the tables, constants and sizes; the checklist for every function
  and table; `lua-guard` for compilation and globals. The step adds no `make
  test` target (the Phase 3 header forbids it).
- No committed behaviour tests for the game modules: the e2e replays of
  plan 3 cover them (3.1's decision, kept by 3.2 to 3.7). Behaviour is
  checked against the C with throwaway programs whose outputs are compared
  byte for byte.
- The engine C test of the reference tool (`test_lua_modules`) runs on fixed
  fixtures and is not touched; moving `render_header` must leave the
  `engine.entity_registry` block byte-identical (the data check proves it).
- Prior art: `game/game/ui_zone.lua` and `tools/table_reference/game_ui_zone.c`
  (slot group zone), `game/game/planet_family.lua` (class tables holding a
  later module's methods, deferred), `game/engine/entity_registry.lua` and
  its unit (static placeholder with a header), `game/game/planet.lua`
  (inline sub-structs created at init, exported Lua-only zero constructor),
  `game/game/economy.lua` (zero class, call-time requires in the cycle).

## Out of Scope

- Any module of steps 3.9 and 3.10 (particle, UI, UI button, message box,
  background, scroll, research display, bootstrap, slipway and wormhole
  displays), and loading probe, slipway, wormhole or planet_family before
  they exist.
- Changing the deferral mechanism of `tests/lua/test_modules.lua`.
- Folding the local `emitter_t`, `toast_keys_t` and `button_t` zero
  constructors into their owners (3.9 may).
- Running any e2e replay or the Lua game in the fake (plan 3; step 3.15 for
  the synthetic run).
- The full rule-4 timing protocol (next checkpoint: 3.9), unless this step's
  own-time gate fails.
- Audio, redesign of any kind (plan 5).

## Further Notes

- One question went to the `guideline` skill, run in place since no Agent
  tool was available: identity versus forwarders for the draw methods
  defined in 3.10 (decision above). Decisions taken here without a human:
  `targeting_zero` and `slipway_keys_zero` moved home, no `probe_keys_t`
  constructor, `reference_header` shared, `end_` for the slipway's end.
- Expected size: about 1,550 lines of C (headers included), so roughly
  50–60 KB of Lua at 3.7's ratio. `lua-guard` should grow by about 3.5–5 ms
  (0.064–0.088 ms per KB); `lua-test-modules` by about 1–2 ms (one module
  loaded, one block of about 1,370 lines at 0.21 µs per line, four
  deferred modules failing early), inside the 2× gate. The deferred
  modules' 12 blocks are counted at 3.9 and 3.10.
- Step 3.9 inherits: `particle` requires `game.probe` at call time only
  (probe requires `game.particle` at load for `emitter_update_method`);
  probe's local `emitter_zero` and planet's can fold into `particle`; drop
  `game.planet_family` and `game.probe` from `LUA_DEFERRED_MODULES`; the
  throwaway stubs of `game.ui` and `game.message_box` describe the shapes
  probe and slipway call (`ui_panel_box`, `main_mouse_typed`, `main_ui`,
  `ui_hovered`).
- Step 3.10 inherits: `slipway_display` and `wormhole_display` require
  `game.slipway` and `game.wormhole` at call time only (the latter two
  require the displays at load); drop `game.slipway` and `game.wormhole`
  from `LUA_DEFERRED_MODULES`; `planet_family_trade` builds keys with
  `slipway.slipway_keys_zero()`.
