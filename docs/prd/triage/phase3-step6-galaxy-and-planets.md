---
title: "Plan 2, step 3.6: the include cycle, galaxy and planets"
description: "Port galaxy, planet, planet_family, planet_trade, planet_display and ui_zone to Lua as faithful copies, opening the 18-module include cycle, with a load check that defers the one file that cannot load until step 3.9."
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
> 3.6. `game/game/` the include cycle, galaxy and planets.

## Problem Statement

Plan 3 will put any Lua function next to its C original when an e2e replay
diverges, so every C function and table of the game must have a faithful Lua
copy. Steps 3.1 to 3.5 ported the engine and the game's leaves, techs, HUD and
layout. What remains of the game is built around one big include cycle of 18
modules (galaxy, the four planet modules, economy, slipways, probes,
wormholes, UI, message box, particles, background, scroll, research display),
cut into steps 3.6 to 3.9. Nothing of it exists in Lua yet: there are no
planets, no galaxy generation, no planet panels or drawing, and no UI zone.

The checklist (`luajit tools/checklist.lua --listing`) assigns 12 C files to
step 3.6, with 140 functions and 15 tables, all MISSING:

- `src/game/galaxy.{c,h}` — 2 functions, 3 constants.
- `src/game/planet.{c,h}` — 40 functions, `planet_class_def`, the planet
  constants and `planet_t`, `planet_keys_t`, `planet_row_signature_t`.
- `src/game/planet_family.{c,h}` — 59 functions, 13 tables (six class
  definitions, six kind definitions, `planet_kinds`).
- `src/game/planet_trade.{c,h}` — 7 functions, `trade_offer_t`.
- `src/game/planet_display.{c,h}` — 25 functions.
- `src/game/ui_zone.{c,h}` — 12 functions, `SLOT_KINDS` (moved to 3.6 by
  step 1.1's note; it includes only engine headers, so it is a leaf, not a
  cycle member).

Because these modules sit inside a cycle whose other members (3.7 to 3.9) do
not exist yet, the step must also settle, with evidence from the C, which of
its files can load alone, and how the load check treats any that cannot.

## Solution

Six new Lua modules under `game/game/` — `galaxy`, `planet`,
`planet_family`, `planet_trade`, `planet_display`, `ui_zone` — each a
faithful copy of its C pair: same functions in the same order, same names,
same computations, same tables, following the conventions of the step notes
of 2.1, 3.1, 3.3, 3.4 and 3.5. Their tables and constants are registered with
the data check; `make checklist STEP=3.6` passes.

Load-check finding (the decision of this step):

- `galaxy`, `planet`, `planet_trade`, `planet_display` and `ui_zone` load
  now, under plain LuaJIT, with every one of their partners from 3.7 to 3.9
  missing. Their only load-time partner values are inside the step:
  `planet_class_def` holds methods of `planet_display` (draw, refresh,
  release, left press, accept drop) and of `planet_trade` (attach slipway,
  offer trade), so `planet` requires those two at load, in that one direction;
  `planet_display` and `planet_trade` require `planet` at call time only, and
  have no file-level table needing a partner. `galaxy` has no table.
  `ui_zone` needs only engine modules.
- `planet_family` cannot load before step 3.9. Its six class tables hold
  `targeting_tooltip_method`, `targeting_track_method` and
  `targeting_confirm_method` (all three defined in `probe.c`, step 3.8), and
  the ascension and protostar classes hold `probe_spawn_particle` (also
  `probe.c`). These are load-time partner values, so 3.3's extension requires
  `game.probe` at load. A call-time trampoline is ruled out by the C itself:
  `probe.c` line 38 (`cls->drag == targeting_track_method`) and lines 87 and
  94 compare a class's `drag` field with probe's own function by identity, so
  the structure classes must hold that very function. In turn,
  `probe_class_def.draw` is `emitter_update_method`, defined in `particle.c`
  (step 3.9), so `probe` requires `game.particle` at load. `particle` has no
  file-level table, so it loads alone and the chain stops there. The pairs
  are one-directional (`planet_family` → `probe` → `particle`; nothing in 3.6
  to 3.9 needs a `planet_family` value at load: `ui.c` compares `hit_test`
  with `planet_hit_test_method`, a `planet` function, at call time), so no
  two-way need arises in this step.
- So the load check skips `planet_family` until 3.9, and the data check skips
  its 13 tables until then. This step is the first that needs the skip, so it
  builds it, per 3.3's note: a deferred-module list given to
  `lua-test-modules`, strict both ways (see Implementation Decisions). Step
  3.9 empties it, and the check fails if it does not.

## User Stories

1. As the plan-3 debugger, I want `game/game/planet.lua` to hold every function of `planet.h` and `planet.c` in C order with the C names, so that I can put any planet function next to its C original when a replay diverges.
2. As the plan-3 debugger, I want `game/game/planet_family.lua` to hold the structure, trading hub, processor, ascension, protostar and burst code in C order, so that a divergence in a structure's behaviour is traced line by line.
3. As the plan-3 debugger, I want `game/game/planet_trade.lua` to copy the trade offer and rebuild logic exactly, so that trade route divergences point at the same lines in both languages.
4. As the plan-3 debugger, I want `game/game/planet_display.lua` to copy the row signature, refresh, inspection and projects panels and the planet drawing exactly, so that a pixel divergence on a planet is traced to the same computation.
5. As the plan-3 debugger, I want `game/game/galaxy.lua` to draw from the random generator in exactly the C order, so that planet positions of a seeded game match the C bit for bit.
6. As the plan-3 debugger, I want `game/game/ui_zone.lua` to build the same six slot kinds with the same sizes and counts, so that a "zone full" fatal fires at the same moment as in C.
7. As the plan author, I want `make checklist STEP=3.6` to pass (140/140 functions, 15/15 tables), so that the step is verifiably complete.
8. As the plan author, I want every one of the 15 tables and every new header constant and `sizeof` registered with the data check, so that the C and Lua values cannot drift silently.
9. As the plan author, I want `planet_class_def` and `SLOT_KINDS` compared with the C now, so that what can be checked at 3.6 is checked at 3.6.
10. As the plan author, I want the 13 `planet_family` tables rendered on the C side now, so that the checklist's "ported but not compared" gate passes and step 3.9 only has to drop the deferral to compare them.
11. As the plan author, I want the load check to skip `planet_family` only for the reason the C forces (a missing later-step module), so that any other load error in it still fails `make test`.
12. As the plan author, I want the load check to fail once a deferred module loads, so that step 3.9 cannot forget to remove the deferral.
13. As the plan author, I want the load check to fail on a deferred name that is not a file of the tree, so that a typo or a renamed module cannot hide a failure.
14. As the plan author, I want the deferral list to live in one Makefile variable passed to `lua-test-modules`, so that the deferral is visible where the check is run and easy to empty.
15. As a later-step agent (3.7–3.9), I want the step note to record which 3.6 files load alone and why `planet_family` does not, so that I follow the same reasoning for economy, probe, slipway, wormhole and the UI.
16. As a later-step agent, I want every require of a module from 3.7 to 3.9 written at call time, inside the function that needs it, so that the 3.6 modules load while their partners are missing.
17. As a later-step agent, I want the one-direction load requires of this step listed (`planet` → `planet_display`, `planet` → `planet_trade`, `planet_family` → `planet`, `planet_display`, `planet_trade`, `probe`), so that I never add the reverse direction at load.
18. As a later-step agent writing `probe` (3.8), I want to know that `planet_family` requires `game.probe` at load and reads `targeting_tooltip_method`, `targeting_track_method`, `targeting_confirm_method` and `probe_spawn_particle` from it, so that `probe` must export them as module fields and must not require `planet_family` at load.
19. As a later-step agent writing `ui_button` and `message_box` (3.9), I want the zero constructors this step writes for `button_keys_t`, `open_box_frame_t` and `toast_keys_t` documented, so that I can fold them into the owning module if I choose.
20. As a player of the Lua game (plan 3 onward), I want planets to bob, shake, vanish and roll their types exactly as the C does, so that the Lua game looks and plays the same.
21. As a player, I want the planet discs and development rings drawn from the same cached images with the same fallbacks under fill patterns, so that planets look identical.
22. As a player, I want the inspection and projects panels to list the same rows in the same order, so that the UI reads the same.
23. As a player, I want the yearly income toast to appear only for on-screen planets with nonzero income, so that toasts match the C.
24. As a player, I want bursts to spawn the same particles from the visual random stream, so that explosions look the same without touching the game's random state.
25. As the maintainer of `make test`, I want the step's own-time gate measured (paired, 45 pairs, against the before commit), so that `lua-guard` and `lua-test-modules` stay under 100 ms and within 2× of 3.1's per-KB and per-table costs.
26. As the maintainer of `make test`, I want the deferral's new unit-test case to stay one small fixture in the existing `test_lua_modules` test, so that `make test` does not grow a target.
27. As the reviewer, I want each decision this step took without a human (zero constructors, inline sub-structs, the deferral design) recorded in the step note, so that the next agent does not re-litigate it.
28. As the reviewer, I want behaviour checked against the C with throwaway programs (galaxy population, type rolls, bobbing, row signatures, trade offers where reachable), so that the copy is shown faithful without committing behaviour tests the replays will cover.
29. As the plan-3 debugger, I want `planet_bob_total` to wrap exactly as the C does past 65535 frames, so that a long replay diverges (or not) identically.
30. As the plan-3 debugger, I want `planet_keys_set_id` and `planet_copy_keys` to enforce the same id length rule with the same assert messages, so that a too-long id fails the same way.

## Implementation Decisions

### Modules and their requires

- **`game.ui_zone`** (copy of `ui_zone.h` then `ui_zone.c`): the slot kind enum and `UI_EPHEMERALS_RESERVE` as fields; the `.c`-local size and count macros as module locals (through `zone.ZONE_BYTES_BY_WIDTH` and `allocator.ARENA_LIST_BYTES`); the compile-time `ui_zone_exceeded_t` check as a load-time `assert` against `zone_sizes.UI_ZONE_BYTES`; the six `SLOT_ZONE_STORAGE` pairs as module locals; `M.SLOT_KINDS` a 0-indexed array of `{ zone_name, slot_name, cells, taken, size, count }` (the field names `allocator_init_slot_kinds` already reads); `g_group` a module local built by `slot_group.SLOT_GROUP`. Requires only engine modules, at load.
- **`game.galaxy`**: the three map constants as fields (`GALAXY_MAP_MARGIN`, `GALAXY_MAP_SPAN` as raw 16.16 values, `GALAXY_MAP_SCALE`); `galaxy_populate` and `galaxy_install_rolls`. Requires `game.planet` and `game.economy_score` at call time (cycle partners); everything else already written is required at load.
- **`game.planet_trade`**: `trade_offer_t` shape `{ route, amount, accept, accepted_routes }` in the top comment; its functions in C order. Requires `game.planet` and `game.slipway` at call time.
- **`game.planet_display`**: its functions in C order, `planet_draw_on_screen` forward-declared as a local. Requires `game.planet`, `game.planet_family`, `game.economy`, `game.message_box`, `game.research_display`, `game.slipway`, `game.ui`, `game.ui_button` and `game.probe` (home of `main_economy_typed`, per 3.5's note) at call time; `game.ui_zone` (a leaf of this step) and all already written modules at load.
- **`game.planet`**: header constants first (`PLANET_ID_SIZE`, `PLANET_ROW_SIGNATURE_NEEDS`, the seven `PLANET_HAS_*` bits), then the `.c` in order. Requires `game.planet_display` and `game.planet_trade` **at load** (their methods fill `planet_class_def`); `game.message_box` at call time (`toast_new`, `toast_keys_set_text`).
- **`game.planet_family`**: its functions and tables in C order. Requires `game.planet`, `game.planet_display`, `game.planet_trade` and **`game.probe` at load** (class tables); `game.scroll` and `game.particle` (and `game.particle_zone` if that is where `particle_burst_slot_take` lands) at call time. It does not load until 3.9 (see the load-check finding in Solution).
- The one-direction load requires of this step are exactly: `planet` → `planet_display`, `planet` → `planet_trade`, `planet_family` → `planet`, `planet_display`, `planet_trade`, `probe`. Every reverse direction is call time. A partner's constant (for example `PLANET_HAS_NEEDS` read by `planet_display`) is read at call time, never cached at module level (3.3's rule).

### Shapes and Lua-only additions

- `planet_t` is one table holding the entity header fields and the planet fields (2.1's first-member rule): `&self->header` is `self`. Its top comment lists the fields with the C names.
- `planet_init_instance` writes exactly what the C writes, plus, Lua only, the two inline sub-structs that later code writes into field by field: `targeting` (`{ archetype, target, cost, has_cost }`, `target` a `{ kind, id }` with `ENTITY_ID_NONE` copied) and `emitter` (every field of `emitter_t` at its C zero, `particles` an empty pointer list shape). A Lua block has no bytes to write into, so the sub-table must exist; C never reads either before writing it, so behaviour is unchanged. Every other field the C leaves unwritten (`sprite`, `palette_block`, `speed`, `acceleration_factor`) stays unwritten, as 3.2 did with `entity_init_header`.
- `planet_id` (a `char[8]`) is a Lua string; `memset` to zero is `""`; `strlen(id) < PLANET_ID_SIZE` and the `memchr` check become length tests with the C assert messages.
- Zero constructors (C `= {0}` / `memset`), each listing every field of the C struct at its C zero value:
  - Owned by this step and exported as Lua-only functions after the C functions: `planet.planet_keys_zero()` (used by `galaxy`, `planet_display`, `planet_family`) and `planet.planet_row_signature_zero()` (used by `planet_init_instance` and `planet_display`; `need_resources` and `need_flags` 0-indexed arrays of `PLANET_ROW_SIGNATURE_NEEDS`).
  - Owned by an existing module: `layout` gains a Lua-only export `layout_row_keys_zero()` wrapping its local `keys_zero`, used for the nested `layout_row_keys_t` fields.
  - Owned by a later step (`button_keys_t` and `ui_button`, 3.9; `open_box_frame_t`, `toast_keys_t` and `message_box`, 3.9; `slipway_keys_t` and `slipway`, 3.8; `burst_keys_t`, this step): a local constructor in the using module, from the C header's field list. Recorded in the step note so that 3.8 and 3.9 may fold them into the owner.
  - `production_t` (`{ has_resource, resource, amount }`) and `trade_route_list_t` zeroes: local constructors.
- Struct assignment of a `trade_route_list_t` (`lw.accepted_routes = self->outgoing_routes` and back in `planet_rebuild_trades`) copies the list header into new tables sharing the item storage, as the C copies share their buffer (3.5's value-copy rule). `trade_offer_t lw` is a new table per loop iteration.
- `burst_kind_new` copies `*keys` into a new `planet_keys_t` table (positions copied, id string shared).
- `planet_kinds` is `M.planet_kinds`, a 0-indexed array of `{ class_name, kind }`; its C length (`sizeof` over the array) is a module-local count of 2 checked by a load-time `assert` against the table.
- Kind definitions are module fields `{ new_entity, record }`, `record` the very `game_data` kind record table; class definitions are module fields with the C field names, `states` a 0-indexed array indexed by the entity state constants, `tags` and `lineage` computed with `entity_class`'s helpers as the C computes them.
- `planet_family_bobs` compares the settled state function with the module's own two locals by identity, as the C does.
- `planet_draw_ring_image`'s `void *context` (a pointer to one ring colour) is the colour value itself; the disc builder's `NULL` context is `nil`.
- The function-local `static const` `gap_items`/`gap` of `planet_inspection_panel` is a module local built once at load with `map_tables`' Lua-only item and row constructors; it is not a file-level table and is not compared.
- `planet_bob_total`'s `(uint32_t)state_time >> 16` is a logical shift (`bit.rshift`), the loop counter compared as unsigned.
- `planet_row_signature`'s `uint8_t`/`uint16_t` fields hold plain integers; the C assert bounds `route_count`, and `needs_count` never exceeds `PLANET_ROW_SIGNATURE_NEEDS`.
- `sizeof`: every `sizeof` a module uses is an entry of its own `M.SIZEOF`: `planet` (`planet_t`, `need_t`, `void *`), `planet_family` (`planet_t`), `planet_display` (`button_keys_t`), `ui_zone` (`entity_id_list_t`, `entity_id_t`). All compared with the C compiler.

### The load-check deferral (built here, first needed here)

- `lua-test-modules` takes an optional `--defer <module>` argument (repeatable), passed by the Makefile from one variable listing `game.planet_family` until step 3.9.
- For a deferred module: if its `require` fails with a `module '<name>' not found` error whose `<name>` is not a file of the given tree, it counts as deferred: no load failure, and every reference block of that module is skipped (not reported missing). Any other load error fails as today. If it loads, the check fails with a message telling to drop it from the deferred list. A deferred name that is not one of the given files fails.
- The summary line names the deferred modules and skipped blocks (for example `… modules loaded, 1 deferred (13 names skipped), …`), so the deferral is visible on every run.
- The checklist needs no change: its "ported but not compared" gate reads the C reference file, which renders all 15 tables from this step on.
- Step 3.9 empties the variable; the "loads now" failure enforces it.

### Data check registration

- New reference units in `tools/table_reference/`, each added to `TABLE_REFERENCE_SOURCES`: `game_ui_zone.c` (includes `ui_zone.c`: enum, reserve, `SLOT_KINDS` rendered as `zone_name`, `slot_name`, `size`, `count` and each `taken[i]` at 0 — `cells` renders no line on either side — plus its `SIZEOF` entries), `game_galaxy.c` (the three constants), `game_planet.c` (includes `planet.c`: `planet_class_def` through `reference_class.h`, the constants, `SIZEOF` entries), `game_planet_display.c` (`SIZEOF.button_keys_t`), `game_planet_family.c` (includes `planet_family.c`: six classes through `reference_class.h`, six kind definitions as `new_entity function` plus `record &<kind record name>`, `planet_kinds`, `SIZEOF.planet_t`). `planet_trade` has nothing to register.
- A pointer to an unlisted table stops the reference build (3.3's rule); the kind records are already listed by the `game_data` unit.

### Verification of the step

- `make checklist STEP=3.6` passes: 140/140 functions, 15/15 tables.
- `make test` green: `lua-guard` compiles all six files and finds no undeclared global; `lua-test-modules` loads five of them, defers `planet_family`, and finds `planet_class_def`, `SLOT_KINDS` and every new constant and size equal to C.
- Behaviour checked against the C with throwaway programs, not committed (as in 3.4 and 3.5): galaxy population of a seeded game (ids, map positions, positions) and the installed pickers; `planet_roll_type` over a stream of rolls; `planet_bob`/`planet_bob_total`; row signatures and their equality; `planet_offer_trade`/`planet_rebuild_trades` on hand-built planets with stubbed slipway ends; ring and disc images. `planet_family` behaviour is checked with `game.probe` (and anything else missing) stubbed through `package.preload` in the throwaway script only.

## Implementation Chunks

1. **`ui_zone`** — the module, its load-time size check, the `game_ui_zone.c` reference unit. `SLOT_KINDS` and its constants compared.
2. **`planet_trade` and `planet_display`** — both modules (call-time requires of `planet` and every 3.7–3.9 module), the later-step zero constructors they need, `layout.layout_row_keys_zero`, the `game_planet_display.c` unit. Both load alone.
3. **`planet` and `galaxy`** — `planet` with its load-time requires of the chunk-2 modules and `planet_class_def`, its two Lua-only zero constructors and the inline sub-struct tables; `galaxy`; the `game_planet.c` and `game_galaxy.c` units. All five modules of chunks 1–3 load; throwaway behaviour checks for galaxy, rolls, bobbing, signatures and trade.
4. **The load-check deferral** — `--defer` in `tests/lua/test_modules.lua`, the Makefile variable (empty in this chunk, or listing a name only once chunk 5 lands, so every commit stays green), new fixture cases in `tests/test_lua_modules.c`: a deferred module failing on a missing module is skipped with its blocks; failing otherwise still fails; loading fails with the "drop it" message; an unknown deferred name fails.
5. **`planet_family`** — the module with its load-time require of `game.probe`, the 13 tables, the `game_planet_family.c` unit, `game.planet_family` added to the deferred list. Throwaway behaviour checks with `game.probe` stubbed.
6. **Own-time gate and step note** — `make checklist STEP=3.6`; the own times of `lua-guard` and `lua-test-modules`, 45 pairs against the step's before commit (the last commit before chunk 1, currently `bb34696`), each under 100 ms and within 2× of 3.1's per-KB and per-table costs; KB of Lua added, blocks and reference lines added (noting that 13 of the 15 new blocks are deferred); projection to the end of the plan. 3.6 is not a checkpoint: no full rule-4 protocol unless the gate fails. The step note in `docs/love-port/plan-2.md` records the files, the counts, the load-check finding and the deferral mechanism, the one-direction load requires, the zero-constructor and inline sub-struct decisions, the throwaway checks, the gate figures, and that the PRD's agent had no `guideline` and settled its points itself.

## Testing Decisions

- A good test checks external behaviour through the public interface (the check's command line, output and exit code), never the module internals.
- Committed tests: only the new cases of `tests/test_lua_modules.c` for the deferral (fixture trees under `tmp/`, as its existing cases). They are unit tests, one small fixture each, not new `make test` targets; the step note records their cost.
- The data check (`lua-test-modules`) is the committed check for every table, constant and size; the checklist for every function and table.
- No committed behaviour tests for the game modules: the e2e replays of plan 3 cover them (3.1's decision, kept by 3.2–3.5). Behaviour is checked against the C with throwaway programs whose outputs are compared byte for byte, as in 3.4 and 3.5.
- Prior art: `tests/test_lua_modules.c` (fixture cases driving `tests/lua/test_modules.lua`), `tools/table_reference/game_mouse.c` and `reference_class.h` (class tables), `game_game_data.c` and `reference_data.h` (pointer names), `game/game/mouse.lua` (a class module), `game/game/grab_arena.lua` (a zone module).

## Out of Scope

- Any module of steps 3.7 to 3.9 (economy, economy score and tiers, slipway, probe, wormhole, particle zone, UI, UI button, message box, particle, background, scroll, research display, bootstrap), and loading `planet_family` before they exist.
- Removing the deferral: step 3.9's job.
- Running any e2e replay or the Lua game in the fake (plan 3; step 3.15 for the synthetic run).
- Folding the later-step zero constructors into their owners (3.8, 3.9 may).
- The full rule-4 timing protocol (next checkpoint: 3.9), unless this step's own-time gate fails.
- Audio, redesign of any kind (plan 5).

## Further Notes

- No question went to a `guideline` agent: none was left that the plan, the step notes and the C did not settle. Decisions taken here without a human: the deferral design, the zero constructors for structs owned by later steps, the inline sub-struct tables created in `planet_init_instance`, the ring-image context as a value.
- Expected size: about 2,360 lines of C, so roughly 80–95 KB of Lua at 3.5's ratio; `lua-guard` should grow by about 6–9 ms (to about 52–55 ms), well inside the 2× gate. `lua-test-modules` grows less than its per-KB model, since `planet_family` (the largest file) is not loaded and 13 of the 15 new blocks are skipped until 3.9; step 3.9's gate will pay for them.
- Step 3.8 inherits: `probe` must export `targeting_tooltip_method`, `targeting_track_method`, `targeting_confirm_method` and `probe_spawn_particle` as module fields, require `game.particle` at load (for `probe_class_def.draw`), and never require `planet_family` at load; at 3.8 `probe` will itself fail to load on the missing `game.particle` and should be deferred the same way.
