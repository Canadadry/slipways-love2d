---
title: "Plan 2, step 3.9: the include cycle, UI and message box, particles, background, scroll"
description: "Port particle, scroll, background, ui_button, ui, message_box, research_display and the engine bootstrap to Lua as faithful copies, the last slice of the big include cycle; all eight load at once, planet_family and probe leave LUA_DEFERRED_MODULES, and the step ends with the own-time gate and the full rule-4 checkpoint."
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
> 3.9. `game/game/` the include cycle, UI and message box, particles,
>    background, scroll.

## Problem Statement

Plan 3 will put any Lua function next to its C original when an e2e replay
diverges, so every C function and table of the game needs a faithful Lua
copy. Steps 3.6 to 3.8 ported the galaxy, the planets, the economy, the
probes, the slipways and the wormholes. What the player sees and clicks
around them is still missing: no particle emitter (probe trails, planet
bursts), no camera, no background (the star field the player clicks to
place a probe or a wormhole), no buttons or modal backdrop, no UI entity
(hover, left press, drag and drop dispatch, the main mouse), no message box
or toast (the yearly report, the price that floats up), no research display
(project rows and tooltips), and no engine bootstrap (`reset_world`,
`update_entities`). Already ported modules call into all of these at call
time (`planet_display` opens boxes and buttons, `planet` and `economy` raise
toasts, `probe` reads `ui_panel_box` and `main_mouse_typed`, `planet_family`
draws through `emitter_update`), and two ported modules still cannot load:
`game.probe` requires `game.particle` at load, and `game.planet_family`
requires `game.probe` at load (3.6 and 3.8 notes).

The checklist (`luajit tools/checklist.lua --listing`) assigns 16 C files to
step 3.9, with 170 functions and 9 tables, all MISSING:

- `src/engine/bootstrap.{c,h}`: 3 functions (step 1.1's note: engine
  bootstrap lands here because it includes `ui`, `scroll`, `mouse`).
- `src/game/background.{c,h}`: 15 functions; `background_class_def`.
- `src/game/message_box.{c,h}`: 36 functions (message box, toast, open
  box); `message_box_class_def`, `toast_class_def`.
- `src/game/particle.{c,h}`: 14 functions.
- `src/game/research_display.{c,h}`: 9 functions.
- `src/game/scroll.{c,h}`: 15 functions, the header inline
  `camera_pos_before_shake` included; `camera_class_def`.
- `src/game/ui.{c,h}`: 41 functions; `main_mouse_placeholder_class_def`,
  `g_main_mouse_placeholder`, `ui_class_def`.
- `src/game/ui_button.{c,h}`: 37 functions (button, modal backdrop);
  `button_class_def`, `modal_backdrop_class_def`.

About 2,940 lines (90 KB) of C, headers included: the largest step of the
cycle. It is also a timing checkpoint: the full rule-4 protocol runs at its
end.

## Solution

Eight new Lua modules, each a faithful copy of its C pair (header, then
`.c`; same functions in the same order, same names, same computations, same
tables), following the conventions of the step notes of 2.1, 3.1 and 3.3 to
3.8: `game/game/particle.lua`, `scroll.lua`, `background.lua`,
`ui_button.lua`, `ui.lua`, `message_box.lua`, `research_display.lua`, and
`game/engine/bootstrap.lua`. Their tables, constants and sizes are
registered with the data check; `make checklist STEP=3.9` passes (170/170,
9/9). The local zero constructors of structs these modules own move home.
The step closes with the own-time gate and the checkpoint protocol.

### Load finding (the decision of this step)

- **All eight modules load at once; none joins `LUA_DEFERRED_MODULES`.**
  Evidence from the C: every file-level value of the eight is either
  partner-free or an engine value already ported.
  - The seven class tables hold only their own module's static methods, plus
    `entity_registry_hit_test_always` (engine, ported in 3.3) for
    `background_class_def` and `modal_backdrop_class_def`;
    `button_hit_test_method` is ui_button's own static (it calls
    `entity_geom_contains_cursor` at call time).
  - ui's statics are a zero class (`main_mouse_placeholder_class_def`) and
    a placeholder whose header points at it (`g_main_mouse_placeholder`);
    `g_main_mouse` and `g_main_ui` are zero ids.
  - The other statics are ints (`g_*_up`, `g_pixels_hidden`), a zero id
    (`g_shown_message_box`), and zero work structs (`g_background_image_key`,
    `g_building_look`, the `none` keys of `button_construct`, the `empty`
    list of `ui_hovered_layer`).
  - particle's compile-time check reads `particle_zone.PARTICLE_SLOT`: a
    leaf ported in 3.8, required at load.
  - Every partner use is inside a function body: `planet_is_family`
    (particle); `probe_new`, `wormhole_new`, `wormhole_ready` (background);
    `galaxy` map constants, read at call time per 3.3's rule (scroll);
    `planet_hit_test_method`, `slipway_class`, `probe_class`,
    `wormhole_class`, `protostar_class` (ui); `main_ui`, `ui_pad_tab`
    (ui_button); `main_ui`, `ui_*`, `button_new`, `main_camera_typed`,
    `main_mouse_typed` (message_box); `planet_is_family` (research_display).
  So the step adds no deferral, and the cycle's load check runs here for
  every 3.9 module.
- **`game.planet_family` and `game.probe` load once `game.particle`
  exists, and are dropped from `LUA_DEFERRED_MODULES`**, which becomes
  `game.slipway game.wormhole` (3.10 empties it, per 3.8's note). particle
  requires `game.probe` and `game.planet` at call time only (in
  `emitter_of`; it reads the probe's and planet's `emitter` field
  directly), so no load loop appears. Evidence, measured on a scratch copy
  of the tree with a stub `game.particle` that exports only the five names
  probe and planet_family read: `game.particle`, `game.probe` and
  `game.planet_family` all load; `game.slipway` and `game.wormhole` still
  fail on `game.slipway_display` and `game.wormhole_display` (3.10). Then
  `lua-test-modules` run on that copy with only `--defer game.slipway
  --defer game.wormhole` reports 77 modules loaded, 706 names and 33,129
  lines equal to C. The committed tree gives 75 modules, 686 names and
  32,949 lines. So probe's and planet_family's 20 blocks (180 lines) already
  match the C, and they start counting in the chunk that adds particle. The
  checker fails a deferred module that loads, so the list must change in
  that same chunk.
- No `guideline` question was needed: every point was settled from the plan,
  the step notes and the C. The decisions taken without a human are listed
  under Further Notes.

### Zero constructors moved home

3.6's note let 3.8 and 3.9 move the local zero constructors of structs a
3.9 module owns into that module. All five move now, each exported as a Lua-only function after the C
functions (as `planet.planet_keys_zero`, `slipway.slipway_keys_zero` and
`targeting.targeting_zero` are). Each local, and its line in the caller's
top comment, is deleted:

- `emitter_t`: `particle.emitter_zero()` replaces the locals of `planet.lua`
  and `probe.lua` (probe already requires particle at load; planet requires
  it inside `planet_init_instance`, at call time).
- `toast_keys_t`: `message_box.toast_keys_zero()` replaces the locals of
  `planet.lua` and `economy.lua`, called through the `message_box` each
  already requires at call time at that site.
- `open_box_frame_t`: `message_box.open_box_frame_zero()` replaces the local
  of `planet_display.lua` (call time, same site).
- `button_keys_t`: `ui_button.button_keys_zero(keys)` (memset form: fills
  and returns `keys`, as the local does) replaces the local of
  `planet_display.lua` (call time; `planet_display` already requires
  `game.ui_button` at call time).
- `button_t`: `ui_button.button_zero()` replaces the local of
  `economy_score.lua`. `economy_score` builds `g_difficulty` with it at load,
  so it **requires `game.ui_button` at load**. This is allowed: `economy_score.h` includes
  `ui_button.h` and not the reverse, and none of ui_button's load-time
  requires reaches `economy_score`. It is the one-direction case of 3.3's
  rule, recorded in the step note.

Not moved: `tuple_zero` (layout's struct, 3.5), `down_zero` (engine
`down_t`), `burst_keys_zero` (planet_family's own), the list zeros (engine
lists).

## User Stories

1. As the plan-3 debugger, I want `game/game/particle.lua` to hold every function of `particle.h` and `particle.c` in C order with the C names, so that I can put any of them next to its C original when a replay diverges.
2. As the plan-3 debugger, I want `emitter_update` to read the x velocity it has just written when computing the y velocity, exactly as the C comment warns, so that every particle with a non-zero `matrix_yx` moves the same.
3. As the plan-3 debugger, I want `emitter_update` to delete an expired particle during the walk without compensating the index, so that the skipped particle is visited on the same later frame as in C.
4. As the plan-3 debugger, I want `emitter_update` to batch points by 32 into `p8_psets` and skip the pixels only when `particle_hide_pixels` is on, so that the framebuffer and the draw meter match.
5. As the plan-3 debugger, I want `emitter_spawn` to draw `visual_random_draw_one` per loop and call the class's `spawn_particle` only when the host has a particle list, so that the visual random stream stays aligned.
6. As the plan-3 debugger, I want `emitter_init_in` and `emitter_release_particles` to reserve and release the particle-zone slots with the C capacities and asserts, so that particle slot counts and overflows match.
7. As the plan-3 debugger, I want `emitter_of` to return the probe's emitter, a family planet's emitter only while `PLANET_HAS_EMITTER` is set, else nil, so that every host resolves the same way.
8. As the plan-3 debugger, I want `game/game/scroll.lua` to bound the camera with the galaxy margin, span and scale exactly as `camera_pad_bound`, and shake through `entity_geom_shake`, so that the view moves the same.
9. As the plan-3 debugger, I want `camera_pan`, `camera_apply` and `main_camera_typed` copied with their asserts, so that pad panning and parallax match.
10. As the plan-3 debugger, I want `game/game/background.lua` to construct the 32×32 tile map from the same random walk and the same `rnd` calls in the same order, so that the RNG state and the map match after a new game.
11. As the plan-3 debugger, I want the background image cache rebuilt exactly when the C rebuilds it (first draw, other vram, new map version, changed draw palette), so that the image slot builds and draw costs match.
12. As the plan-3 debugger, I want `background_left_press` to build a wormhole when a probe exists and a probe otherwise, as the C does, so that clicks on the star field place the same thing.
13. As the plan-3 debugger, I want `game/game/ui_button.lua` to create buttons with every field at its C value, apply the keys, and build the layout row, side tooltip and tooltip in the button's slot, so that ui-zone slots and button widths match.
14. As the plan-3 debugger, I want `button_update` to slide toward its target by 0.4 of the gap or snap within one unit, so that boxes slide in on the same frames.
15. As the plan-3 debugger, I want `button_draw` to try the button image cache first, with the same key words, width and refusals (`is_whole`, `row_extent`, fill pattern, palette block), and fall back to the direct body draw, so that button pixels and cache use match.
16. As the plan-3 debugger, I want the modal backdrop's create, left press and release copied, so that modal clicks stop at the same entity.
17. As the plan-3 debugger, I want `game/game/ui.lua` to build the hover layers per frame from the hit-test bucket, with the planet hit test inlined as the C does, and the layer assert, so that the hovered entity and `is_hovered` flags match.
18. As the plan-3 debugger, I want `ui_send_to_hovered` to walk layers from 15 down to 0 and stop at the first `stop`, returning the found flag, the target and the down, so that dispatch priority matches.
19. As the plan-3 debugger, I want `ui_handle_button` to hand over the held entity on press, drag it each frame and drop it on release, with the C asserts on held classes, so that drags behave the same.
20. As the plan-3 debugger, I want `ui_update` to compute the world mouse from the main mouse and camera, keep a by-value copy of the ephemerals list and serial before dispatch, and clear the ephemerals on an unhandled press, so that boxes close on the same click.
21. As the plan-3 debugger, I want `main_mouse`, `main_mouse_typed`, `ui_set_main_mouse`, `main_ui` and `ui_set_main_ui` to switch between none, placeholder and real as the C does, so that every module reading them sees the same object.
22. As the plan-3 debugger, I want `ui_set_ephemerals` to put the old owned slot on the exit queue and bump the serial, so that ui-zone slots are released on the same frame.
23. As the plan-3 debugger, I want `game/game/message_box.lua` to store up to `MESSAGE_BOX_ITEMS_CAP` articles of three kinds (scalars, layout row, shared row) with the C asserts, so that the yearly box draws the same rows.
24. As the plan-3 debugger, I want `message_box_draw_overlay` to compute the slide-in offset from the cubic of `t`, honour `keep_open`, and close itself after 10.5, so that boxes move and close on the same frames.
25. As the plan-3 debugger, I want only one message box shown at a time, the previous one marked dead on construct, so that overlapping boxes resolve the same way.
26. As the plan-3 debugger, I want toasts to follow their source until their first update, then rise by the archetype velocity and fade by 0.1 a frame until 5, so that price toasts move and vanish identically.
27. As the plan-3 debugger, I want `message_box_open_box` to clear the ephemerals, build the description row and each button by a by-value copy of its keys, step the slide targets by the row step, and hand the list to the UI, so that every panel opens with the same buttons in the same slots.
28. As the plan-3 debugger, I want `game/game/research_display.lua` to build project tooltips, resource rows and the available projects with the same frame-zone allocations and grow calls, so that frame-zone byte counts and rows match.
29. As the plan-3 debugger, I want `research_display_available_projects` to filter by unlocked tech and planet code in project order, so that the projects panel lists the same projects.
30. As the plan-3 debugger, I want `game/engine/bootstrap.lua` to reset the world in the C order (forget the UI's world, abandon entities, init registry, init each subsystem once, create camera, mouse and UI), so that ids and generations match after a reset.
31. As the plan-3 debugger, I want `update_entities` to run each entity's state method, unregister the dead and advance `state_time`, without compensating the index after a removal, so that update timing matches every replay.
32. As the plan author, I want `make checklist STEP=3.9` to pass (170/170 functions, 9/9 tables), so that the step is verifiably complete.
33. As the plan author, I want the nine tables and every constant and `sizeof` the modules use registered with the data check, so that they are compared with the C.
34. As the plan author, I want all eight modules loaded by `lua-test-modules` at once, with the evidence recorded, so that the load check of the cycle runs here.
35. As the plan author, I want `game.planet_family` and `game.probe` dropped from `LUA_DEFERRED_MODULES` in the chunk that adds particle, so that their 20 blocks are compared from that commit on and the check never fails on a deferred module that loads.
36. As a later-step agent at 3.10, I want `LUA_DEFERRED_MODULES` left as `game.slipway game.wormhole`, so that I know exactly what to drop.
37. As a maintainer of `planet.lua`, `probe.lua`, `economy.lua`, `economy_score.lua` and `planet_display.lua`, I want their local zero constructors replaced by the owners' exported ones, so that each struct has one constructor.
38. As a later-step agent (score panel, menus, help, pad modes, save), I want `toast_keys_zero`, `open_box_frame_zero`, `button_keys_zero`, `button_zero` and `emitter_zero` exported by their owners, so that I build these structs from their owner.
39. As a later-step agent, I want the button's and the UI's own `has` named `has_` (3.6's rule) and the emitter's `has` kept as `has` (a sub-table), so that I read the right fields.
40. As a player of the Lua game (plan 3 onward), I want hovering, clicking, dragging, boxes, toasts and particle trails to behave as in the C, so that the Lua game plays the same.
41. As the maintainer of `make test`, I want the step's own-time gate measured (45 pairs against the step's before commit), so that `lua-guard` and `lua-test-modules` stay under 100 ms and within 2× of 3.1's per-KB and per-table costs.
42. As the maintainer of `make test`, I want the full rule-4 protocol run at the end of the step against `a880ee9` and `8dff812`, so that the plan-wide 3 % budget is checked at this checkpoint.
43. As the maintainer of `make test`, I want no new `make test` target and no new committed test, so that the fast loop grows only by the files and tables the step adds.
44. As the reviewer, I want behaviour checked against the C with throwaway programs, so that the copy is shown faithful without committing behaviour tests the replays will cover.
45. As the reviewer, I want each decision this step took without a human recorded in the step note, so that the next agent does not re-litigate it.

## Implementation Decisions

### Modules and their requires

Every module follows 2.1's module rules (top comment naming the C files and
shapes, `function M.<c name>(` for exported functions, `local function` for
statics, forward-declared when used first, `M.SIZEOF` for every `sizeof`,
header constants as fields). A cycle partner is required inside the function
that needs it; a partner's constant is read at call time (3.3's rule).

- **`game.particle`** (copy of `particle.h` then `particle.c`):
  `EMITTER_HAS_PARTICLES`; `M.SIZEOF` with `particle_t`; the compile-time
  check as a load-time assert that `zone.ZONE_BYTES(SIZEOF.particle_t)`
  equals `particle_zone.PARTICLE_SLOT`. `EMITTER_POINT_BATCH` and
  `g_pixels_hidden` are module locals. The 14 functions follow in C order,
  then `M.emitter_zero()` (Lua only). Required at load: `bit`, engine
  (`allocator`, `dispatch_context`, `entity_class`, `entity_geom`,
  `pointer_list`, `vec`, `zone`), `game.game_data`, `game.particle_zone`,
  `game.visual_random`, `p8.fixed`, `p8.gfx`. Call time: `game.planet`
  (`planet_is_family`, `PLANET_HAS_EMITTER`); probe is reached only through
  `LINEAGE_PROBE` and the `emitter` field, so it is not required.
- **`game.scroll`** (copy of `scroll.h` then `scroll.c`):
  `M.camera_pos_before_shake` (header inline) first; `g_camera_up`;
  `M.camera_class_def` (`states[ENTITY_STATE_NORMAL] =
  camera_update_method`, `apply_camera = camera_apply_method`); the 15
  functions; `M.SIZEOF.camera_t`. Required at load: engine (`allocator`,
  `dispatch_context`, `entity`, `entity_class`, `entity_geom`,
  `entity_registry`, `game_zone`, `vec`), `game.game_data`, `p8.fixed`,
  `p8.gfx`. Call time: `game.galaxy` (the three map constants in
  `camera_pad_bound`).
- **`game.background`** (copy of `background.h` then `background.c`):
  `BACKGROUND_GRID_EDGE`; the `.c`-local `BACKGROUND_IMAGE_SIDE` as a local
  computed from the C macros; `g_background_up`; `M.background_class_def`;
  `g_background_image_key` as a module-local
  `{ built, mem, map_version, draw_palette }`, `draw_palette` a 0-indexed
  copy of `mem.draw_palette` over the C length (`memcpy`/`memcmp` are loops,
  `mem` compared by identity); the `row_values`/`row_values_set` locals as
  0-indexed arrays of 32. Required at load: engine (`allocator`,
  `dispatch_context`, `entity`, `entity_class`, `entity_registry`,
  `frame_zone`, `game_zone`), `game.game_data`, `game.gen`,
  `game.image_slot`, `p8.fixed`, `p8.gfx`, `p8.image`, `p8.vram` as needed.
  Call time: `game.probe`, `game.wormhole`.
- **`game.ui_button`** (copy of `ui_button.h` then `ui_button.c`):
  `BUTTON_HAS_SLIDE_TARGET`; `g_button_up`, `g_modal_backdrop_up`,
  `g_building_look` (`{ body, top }`); `M.button_class_def` and
  `M.modal_backdrop_class_def`; the `static const button_keys_t none` of
  `button_construct` as a module local built once by `button_keys_zero` and
  never written. The 37 functions follow in C order (both files' `ui_to_int` stay
  local to their file), then `M.button_keys_zero(keys)` and
  `M.button_zero()` (Lua only). `M.SIZEOF` holds `button_t` and
  `modal_backdrop_t`. Required at load: engine (`allocator`,
  `dispatch_context`, `entity`, `entity_class`, `entity_geom`,
  `entity_id_type`, `entity_registry`, `geom`, `vec`),
  `game.button_image`, `game.game_data`, `game.hud`, `game.layout`,
  `game.map_tables`, `game.research`, `game.ui_zone`, `p8.fixed`, `p8.gfx`.
  Call time: `game.ui` (`main_ui`, `ui_pad_tab`). None of these reaches
  `economy_score` (so economy_score may require ui_button at load).
- **`game.ui`** (copy of `ui.h` then `ui.c`): the enums and `UI_HAS_*`,
  `UI_PAD_*`, `MOUSE_SIDE_LEFT`, `SEND_TO_HOVERED_*` constants; the
  `.c`-local `MAIN_MOUSE_*` enum as locals; `g_ui_up`, `g_main_mouse_state`,
  `g_main_mouse`, `g_main_ui` (zero ids); `M.main_mouse_placeholder_class_def`
  (zero class); `M.g_main_mouse_placeholder` (header through
  `entity_class.entity_init_header(t, M.main_mouse_placeholder_class_def)`,
  as `probe.g_main_economy_placeholder`); `M.ui_class_def`; the `empty`
  static of `ui_hovered_layer` as a module-local zero list never written.
  The 41 functions follow in C order; `ui_hit_test` (a C `static inline`)
  stays a local function and compares `hit_test` with
  `planet.planet_hit_test_method` by identity. `M.SIZEOF` holds `ui_t`.
  Required at load: engine (`allocator`, `dispatch_context`, `entity`,
  `entity_class`, `entity_geom`, `entity_id`, `entity_id_list`,
  `entity_id_type`, `entity_list`, `entity_registry`, `exit_queue`,
  `frame_zone`, `game_zone`, `geom`, `vec`), `game.game_data`, `game.mouse`,
  `game.mouse_button`, `game.ui_zone`, `p8.gfx`. Call time: `game.planet`,
  `game.planet_family`, `game.probe`, `game.scroll`, `game.slipway`,
  `game.wormhole`, `game.background` and `game.ui_button` where used.
- **`game.message_box`** (copy of `message_box.h` then `message_box.c`):
  the caps (`MESSAGE_BOX_ITEMS_CAP` computed from
  `game_data.SCORE_ROWS_COUNT`), argument and article kind enums,
  `TOAST_TEXT_CAP`, `OPEN_BOX_DESCRIPTION_CAP`; `g_message_box_up`,
  `g_toast_up`, `g_shown_message_box`; `M.message_box_class_def`,
  `M.toast_class_def`; the 36 functions in C order; then
  `M.toast_keys_zero()` and `M.open_box_frame_zero()` (Lua only).
  `M.SIZEOF` holds `message_box_t`, `toast_t` and any other `sizeof` used.
  Required at load: engine (`allocator`, `dispatch_context`, `entity`,
  `entity_class`, `entity_id`, `entity_id_list`, `entity_id_type`,
  `entity_registry`, `vec`), `game.draw_functions`, `game.game_data`,
  `game.hud`, `game.layout`, `game.ui_zone`, `p8.fixed`, `p8.gfx`. Call time:
  `game.scroll`, `game.ui`, `game.ui_button`.
- **`game.research_display`** (copy of `research_display.h` then
  `research_display.c`): the 9 functions; `M.SIZEOF` with
  `research_project_tooltip_t`. Required at load: engine (`allocator`,
  `frame_zone`, `pointer_list`), `game.layout`, `game.map_tables`,
  `game.research`, `game.tech_gate`, `p8.fixed`, `p8.str`. Call time:
  `game.planet` (`planet_is_family`).
- **`engine.bootstrap`** (copy of `bootstrap.h` then `bootstrap.c`): the 3
  functions. Nothing requires bootstrap before 3.14 (cart), so it is in no
  cycle and requires at load `engine.entity`, `engine.entity_registry`,
  `game.mouse`, `game.scroll`, `game.ui`, `p8.fixed` (an engine module
  requiring game modules, as the C includes them).

### Shapes and field names

- Every entity struct is one table holding the header fields and its own
  fields (first-member rule). **`button_t`'s and `ui_t`'s own `has` are
  `has_`** (3.6's rule; `economy_score`'s `button_zero` already writes
  `has_`). The emitter's `has` stays `has` (a sub-table of its host; planet
  and probe already use it). `particle_t` has only `has_acceleration`;
  `background_t`, `camera_t`, `message_box_t`, `toast_t` and
  `modal_backdrop_t` have no own `has`.
- `ui_t.hovered_by_layer` is `{ layer_count, written, layers }`, `layers`
  a 0-indexed array of `UI_HOVER_LAYERS` zero entity id lists created by
  `ui_create`; `entity_id_list_init` fills an existing layer in place.
- **By-value `entity_id_list_t`** (the argument of `ui_set_ephemerals`,
  the result of `ui_ephemerals_new`, `ephemerals_before` in `ui_update`,
  `(entity_id_list_t){0}`) is a new table holding the four fields, sharing
  `items` as the C struct copy shares the pointer. `ephemerals_before` must
  keep its own `count` while `ui_clear_ephemerals` changes the UI's.
- Out-parameters (2.1's rule): `ui_send_to_hovered` returns
  `found, target, down`; `research_project_layout_row_keys` and
  `research_display_planet_resource_row_keys` return `found, keys` (the
  shape `planet_display` already calls); `row_extent` returns
  `ok, extent`; `emitter_host_layer_pos` returns `layer, pos`.
- `message_box_t.articles` is a 0-indexed array of
  `MESSAGE_BOX_ITEMS_CAP` article tables created at create, each
  `{ kind, draw_function, as = { scalars = { count, arguments, text },
  layout = { row, has_pos, pos, align }, row_of } }` at C zero values, with
  `arguments` a 0-indexed array of `{ kind, number }`. The union's three
  members are kept apart: only the member of `kind` is ever written or read.
  `layout_row_init` fills `as.layout.row` in place. `row_of` is a Lua
  function or nil.
- **`open_box_step`'s `button_keys_t b = *row`** is a new table holding
  every field of `row` (one level: nested tables shared), then the top-level
  fields the C writes are replaced. It is safe because `button_create` and
  `button_construct` only read the keys (and `layout_row_new` and
  `tooltip_new` copy what they keep). The copy is a local of
  `message_box.lua`.
- `button_keys_set_tooltip_row`'s `keys->tooltip_row = *row` is a new
  data-row table copying `row`'s fields (`items` shared); `has_tooltip_row`
  is `(row ~= nil) and 1 or 0`.
- `button_image_key_t` is `{ words = { [0] = … } }` (button_image's shape),
  word 2 `bit.bor(look.body, bit.lshift(look.top, 8))`.
- Char arrays (`toast_t.text`, `toast_keys_t.text`,
  `open_box_frame_t.description`, the scalars' `text`) are Lua strings,
  `""` at zero; the `strlen` asserts keep their C messages.
- `research_project_tooltip_join` performs the same `allocator_alloc` of 16
  bytes and the same `allocator_grow` calls with the C byte counts on the
  frame zone (3.1's zone-string rule), building a Lua string;
  `research_project_tooltip` allocates `SIZEOF.research_project_tooltip_t`
  and returns the `outer` data row of a table holding `outer`,
  `outer_item`, `inner`, `inner_items` (0-indexed), with `items` fields
  pointing at those tables.
- `available_projects_kept_t` is `{ items, count }`, `items` 0-indexed new
  research project ids.
- Ids are new tables wherever the C creates, returns or stores one by value
  (3.2's rule); `ENTITY_ID_NONE` and `RESEARCH_PROJECT_ID_NONE` are copied
  at each use.
- Int truth values as 2.1/3.1: `toast_update` writes `is_dead` as
  `(new_l >= 5.0) and 1 or 0`; `ui_set_pad_crosshair` stores the int.
- `(uint16_t)~BIT` clears through `bit.band(has, bit.bnot(BIT))` brought back
  to 16 bits, as 3.8's slipway does.
- A C ternary whose chosen value can be nil (`hit_test == planet_hit_test_method`
  branches, `toast_pos_or_zero`, `ui_pos_or_zero`) is an `if`/`else` (2.1's
  and 3.3's rules).

### Callers updated

- `planet.lua`, `probe.lua`: local `emitter_zero` replaced by
  `particle.emitter_zero()`; probe's top comment loses its 3.9 note.
- `planet.lua`, `economy.lua`: local `toast_keys_zero` replaced by
  `message_box.toast_keys_zero()`.
- `planet_display.lua`: local `open_box_frame_zero` and `button_keys_zero`
  replaced by the owners' functions.
- `economy_score.lua`: local `button_zero` replaced by
  `ui_button.button_zero()`, `game.ui_button` required at load (top comment
  says why).
- No other already-ported call site changes: every existing call into a 3.9
  module (`toast_new`, `toast_keys_set_text`, `message_box_open_box`,
  `open_box_frame_set_description`, `button_keys_set_tooltip_row`, the five
  research_display functions, `main_ui`, `ui_hovered`, `ui_pad_circle`,
  `ui_panel_box`, `main_mouse_typed`, `main_camera_typed`, the particle
  functions and `particle_pixels_hidden`) already uses the signature this
  PRD fixes; an implementer finding a mismatch fixes the caller to the C.

### Data check registration

- Seven new units, each including its `.c`, added to
  `TABLE_REFERENCE_SOURCES`: `src/game/particle.c`, `src/game/scroll.c`,
  `src/game/background.c`, `src/game/ui_button.c`, `src/game/ui.c`,
  `src/game/message_box.c`, `src/game/research_display.c`:
  - `game_particle.c`: `EMITTER_HAS_PARTICLES`, `SIZEOF.particle_t`.
  - `game_scroll.c`: `camera_class_def` through `reference_class`;
    `SIZEOF.camera_t`.
  - `game_background.c`: `background_class_def`, `BACKGROUND_GRID_EDGE`,
    `SIZEOF.background_t`.
  - `game_ui_button.c`: `button_class_def`, `modal_backdrop_class_def`,
    `BUTTON_HAS_SLIDE_TARGET`, `SIZEOF.button_t`,
    `SIZEOF.modal_backdrop_t`.
  - `game_ui.c`: `main_mouse_placeholder_class_def`, `ui_class_def`,
    `g_main_mouse_placeholder` through `reference_header`; the enum and
    `UI_*` constants, `MOUSE_SIDE_LEFT`, `SEND_TO_HOVERED_*`,
    `SIZEOF.ui_t`.
  - `game_message_box.c`: `message_box_class_def`, `toast_class_def`; the
    caps and kind enums; `SIZEOF.message_box_t`, `SIZEOF.toast_t`.
  - `game_research_display.c`: `SIZEOF.research_project_tooltip_t` (and any
    other `sizeof` the module uses).
- Every other `sizeof` a module's `M.SIZEOF` holds goes in that module's
  unit (3.1's rule). bootstrap has no constant or table, so it gets no unit.
- `make checklist STEP=3.9` fails on a ported table the check does not
  compare, so all nine tables must be registered.

### Deferred modules

- `LUA_DEFERRED_MODULES := game.slipway game.wormhole`, changed in the
  particle chunk. The checker is unchanged.

### Verification of the step

- `make checklist STEP=3.9` passes: 170/170 functions, 9/9 tables.
- `make test` green: `lua-guard` compiles the eight files and finds no
  undeclared global; `lua-test-modules` loads every module except slipway
  and wormhole, and compares the new blocks plus probe's and
  planet_family's.
- Behaviour checked against the C with throwaway programs, not committed (as
  in 3.4 to 3.8), outputs compared byte for byte. Only `game.slipway` and
  `game.wormhole` remain unloadable, so the throwaway script stubs
  `game.slipway_display` and `game.wormhole_display` through
  `package.preload` when a scenario needs slipway or wormhole (the C side
  links the real game). Scenarios per chunk are listed below.

## Implementation Chunks

1. **`particle`**: the module and `emitter_zero`; planet's and probe's
   locals replaced; `game_particle.c` unit and its `TABLE_REFERENCE_SOURCES`
   entry; `game.planet_family` and `game.probe` dropped from
   `LUA_DEFERRED_MODULES` (same commit; their 20 blocks now compared).
   Throwaway checks: emitter init and release on both list kinds up to the
   zone-full message, spawn on a probe and a family planet over a run of
   frames (visual random stream, particle positions, velocities, lives,
   pixels with hiding on and off), `emitter_of` on probe, family planet with
   and without emitter, and another class.
2. **`scroll` and `background`**: both modules and units. Throwaway checks:
   camera create, update with shake, pan near each bound, apply with
   parallax, `main_camera_typed` on placeholder and camera; background
   construct over several seeds (map bytes, RNG state after), draw twice
   then after an `mset` and after a palette change (image builds counted,
   framebuffer), left press with and without a probe (wormhole display
   stubbed).
3. **`ui_button`**: the module, `button_keys_zero` and `button_zero`;
   planet_display's `button_keys_zero` and economy_score's `button_zero`
   locals replaced (economy_score requires `game.ui_button` at load); the
   `game_ui_button.c` unit. Throwaway checks: buttons from zero keys, keys
   with a layout row, side tooltip row, false side tooltip and tooltip row
   (slot counts, width, hit box), slides over 20 frames, draws through the
   image cache and the fallback (fill pattern set, palette block changed,
   off-whole widths), left press with and without `on_click`, modal
   backdrop create and press.
4. **`ui`**: the module and `game_ui.c` unit. Throwaway checks on a seeded
   galaxy: main mouse switches, hover over a grid of mouse positions (hovered
   id, layers written, `is_hovered` flags), press on a planet, a button, the
   background, drag and drop of a probe onto a planet, ephemerals set,
   replaced, cleared on an unhandled press, `ui_forget_world`,
   `ui_pad_circle` blink, crosshair draw.
5. **`message_box`**: the module, `toast_keys_zero` and
   `open_box_frame_zero`; planet's, economy's and planet_display's locals
   replaced; the `game_message_box.c` unit. Throwaway checks: a box with
   scalar, layout and shared rows drawn over its whole life with and without
   `keep_open` (framebuffer, camera, close frame), a second box replacing
   the first, toasts from a source, from a position and from the mouse over
   their whole life, `message_box_open_box` with and without a description,
   fixed position, back colour, row step and `clear_ephemerals_after`
   (buttons, slide targets, ephemerals, ui-zone slots).
6. **`research_display` and `engine.bootstrap`**: both modules and the
   research_display unit. `make checklist STEP=3.9` passes. Throwaway checks:
   the 34 projects' tooltips, layout row keys and icon row keys,
   `research_display_planet_resource_row_keys` over input counts and widths,
   available projects for each planet type and tech state (frame-zone bytes
   included); `reset_world` twice (ids, generations, main camera, mouse,
   UI), `update_entities` over frames with entities dying mid-walk,
   `bootstrap_shutdown`.
7. **Own-time gate, checkpoint protocol, step note.** No other agent runs
   during any measurement of this chunk.
   - Own-time gate: the own times of `lua-guard` and `lua-test-modules`,
     45 pairs against the step's own before commit (the last commit before
     chunk 1, currently `42d7734`). Each must stay under 100 ms and grow
     within 2× of 3.1's costs: `lua-guard` 0.10 ms per KB of Lua; load
     0.043 ms per KB; data 17 µs per table (block) plus 0.21 µs per line.
     Record the KB of Lua added, the blocks and reference lines added (the
     20 formerly deferred blocks counted apart), and the projection to the
     end of the plan.
   - Checkpoint, the full rule-4 protocol of `docs/love-port/design.md`:
     three worktrees under `tmp/`: the current commit, `a880ee9` (the
     "before" commit of the span since the last checkpoint, end of 3.3),
     and `8dff812` (plan-wide reference). One warming `make test` in each,
     then 11 rounds, each timing one `make test` per worktree back to back,
     rotating which goes first. Paired r = current / baseline − 1 per round,
     median per baseline; both ≤ 3 % pass; either > 3 % → 11 more rounds,
     decided on the 22 pooled rounds; still > 3 % → fail, and the rework is
     picked from the recorded own times (rule 2 first, then rule 5:
     `lua-guard`'s per-KB cost and the large-table comparison are the
     candidates named by 3.1 and 3.3).
   - Step note in `docs/love-port/plan-2.md` (`### Step 3.9`): the files,
     the counts (170/170, 9/9), the load finding with its evidence (all
     eight load; planet_family and probe undeferred; list now
     `game.slipway game.wormhole`), the five constructors moved home and
     economy_score's new load-time require, `has_` for `button_t` and
     `ui_t`, the by-value list and keys copies, the throwaway checks, the
     own-time figures, the protocol figures (plain medians, paired medians,
     spreads, rounds, cores, slowest new check's own time), what 3.10
     inherits.

## Testing Decisions

- A good test checks external behaviour through the public interface, never
  module internals.
- No new committed test: the data check (`lua-test-modules`) is the committed
  check for the tables, constants and sizes; the checklist for every function
  and table; `lua-guard` for compilation and globals. The step adds no `make
  test` target (the Phase 3 header forbids it).
- No committed behaviour tests for the game modules: the e2e replays of
  plan 3 cover them (3.1's decision, kept by 3.2 to 3.8). Behaviour is
  checked against the C with throwaway programs whose outputs are compared
  byte for byte (scenarios per chunk above).
- The C tests of the reference tool (`test_lua_modules`, `test_lua_guard`)
  run on fixed fixtures and are not touched.
- Prior art: `game/game/probe.lua` and `tools/table_reference/game_probe.c`
  (zero class plus placeholder static through `reference_header`, a class
  holding another module's method), `game/game/slipway.lua` (exported
  Lua-only zero constructor, by-value lists), `game/game/economy.lua` (zero
  class, call-time requires in the cycle), `game/game/button_image.lua` and
  `game/game/text_image.lua` (image caches and their keys),
  `game/game/planet_display.lua` (the callers of message_box, ui_button and
  research_display).

## Out of Scope

- Any module of step 3.10 and later (trade and slipway displays, wormhole
  display, structure icons, score panel, help, pad modes, save, menus, cart),
  and loading slipway or wormhole before their display modules exist.
- Changing the deferral mechanism of `tests/lua/test_modules.lua`.
- Running any e2e replay or the Lua game in the fake (plan 3; step 3.15 for
  the synthetic run).
- Reworking checks unless the own-time gate or the checkpoint protocol
  fails.
- Audio, redesign of any kind (plan 5).

## Further Notes

- Decisions taken here without a human (no `guideline` question was
  needed, every point follows from the plan, the notes or the C): all eight
  modules load at once; the five zero constructors move home;
  economy_score requires ui_button at load (3.3's one-direction rule);
  bootstrap requires its game modules at load (no cycle); scroll reads the
  galaxy constants at call time; the `button_keys_t` by-value copy is one
  level deep; message box articles keep the union's three members apart;
  by-value `entity_id_list_t` copies its four fields and shares `items`.
- Expected size: about 2,940 lines of C, so roughly 90–100 KB of Lua at
  3.8's ratio (50.8 KB of C gave 52.8 KB of Lua). `lua-guard` should grow
  by about 6–9 ms (0.064–0.088 ms per KB) to about 66–69 ms;
  `lua-test-modules` by about 5–7 ms (eight modules loaded at about
  0.055 ms per KB, about 20 new small blocks, the 20 undeferred ones, which
  were already compiled before they failed). Both are well inside the 2×
  gate and under 100 ms. The checkpoint's last result (3.3) was +2.18 %
  against `8dff812`, with spreads up to 41 %: no other agent runs during
  the rounds.
- Step 3.10 inherits: `LUA_DEFERRED_MODULES` is `game.slipway game.wormhole`
  and 3.10 empties it; the throwaway stubs of `game.slipway_display` and
  `game.wormhole_display` describe what slipway and wormhole hold by
  identity; score panel and help build boxes, toasts and buttons through
  `message_box.toast_keys_zero`, `open_box_frame_zero` and
  `ui_button.button_keys_zero`; help hosts an emitter through
  `PARTICLE_SLOT_HELP_LIST` and `particle.emitter_zero()`. The help class
  carries `LINEAGE_PROBE`, so `emitter_of` reaches it through the probe
  branch (the C casts to `probe_t *`): the help table must hold its emitter
  in a field named `emitter`, and `emitter_of` stays unchanged.
