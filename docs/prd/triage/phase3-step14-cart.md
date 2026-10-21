---
title: "Plan 2, step 3.14: the cart and the input step in Lua (cart, input_step)"
description: "The Lua game has every module the game needs except the two that tie them together: the cart that loads, starts, resumes, updates, saves and draws a game, and the input step that turns a raw pad frame into the played frame."
status: needs-triage
---

## Source

Plan file: `docs/love-port/plan-2.md`, Phase 3 — Engine and game. Phase 3 header, verbatim:

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

Step, verbatim:

> 3.14. `game/game/` the cart and the input step.

Step 3.12 is done (`263257a`, its step note). Step 3.13 (menus, title and attract) is **not done** when this PRD is written: every statement below about its outcome is a hypothesis labelled `H-3.13-k` (listed under Further Notes), to be checked against 3.13's committed code and step note before implementation starts. The "before" commit of this step is 3.13's step-note commit (`H-3.13-1`). 3.14 is not a checkpoint: it runs the own-time gate, not the full rule-4 protocol (unless the gate fails).

The checklist (`make checklist STEP=3.14`, `tools/checklist.txt`) assigns four C files to 3.14: `src/game/cart.c`/`.h` and `src/game/input_step.c`/`.h`. At `263257a` it reports `3.14: 4 files, 0/32 functions, 0/1 tables`: 23 functions in `cart.c` (`quit_game_zone`, `cart_load`, `forget_game_zone`, `cart_flush_exit_queue`, `cart_shutdown`, `resume_saved_game`, `title_resume_handler`, `cart_init`, `cart_title_shown`, `cart_return_to_title`, `cart_cut_save`, `cart_set_demo_frame`, `cart_init_method`, `start_world`, `start_interface`, `recompute_economy`, `cart_start_game`, `cart_request_start_game`, `cart_loading`, `cart_resume_game`, `save_at_frame_end`, `cart_update60`, `cart_draw`), 9 in `input_step.c` (`input_step_init`, `input_step_set_attract_after`, `input_step_switch_bindings`, `input_step_start_record`, `input_step_return_to_title`, `input_step_run`, `input_step_played_frame`, `input_step_attract_phase`, `input_step_attract_demo_frame`), and one table, `cart_game_registry_reserve`. Both headers are `no-lua` (declarations, one macro, one enum).

## Problem Statement

The Lua game, written as a faithful copy of the C game, now holds every module below the top of the include graph: the engine, the world, the economy, the displays, the pad modes, the save and (by 3.13) the menus, title, loading screen, demo overlay, attract mode and pad navigation. Nothing ties them together. There is no Lua function that runs the inits in the C order, builds the title, starts a game from a difficulty button through the two loading frames, resumes a saved game, writes the save at the end of an action's frame, returns to the title, updates and draws one frame; and nothing turns a raw pad frame into the frame the game plays (attract mode, pad navigation, the cheat, the bindings switch between the session and the default table). Without these two modules, step 3.15's `game/main.lua` has no loop body to call, and plan 3 cannot put the cart beside its C original when a replay diverges at a game start, a resume, a save or an attract demo.

## Solution

Two new Lua modules under `game/game/`, each a faithful copy of its C file under the 2.1, 3.1 and later conventions of the step notes:

- `game.cart`: every function of `cart.c` in C order, its statics as module locals, `CART_NO_DEMO_FRAME` and the `cart_game_registry_reserve` table as module fields.
- `game.input_step`: every function of `input_step.c` in C order, the two `INPUT_STEP_BINDINGS_*` constants as module fields, an `input_step_t` as a table with the C field names.

Neither is in an include cycle (only `src/cmd/`, `src/devtools/` and the C tests include their headers), so both require their dependencies at load time and load at once; `LUA_DEFERRED_MODULES` stays empty (`H-3.13-5`).

Verified by `make checklist STEP=3.14` (32/32 functions, 1/1 tables), the load check, the undeclared-global check and the data check in `make test`, throwaway C-vs-Lua programs that drive both modules through the scenarios of `tests/test_cart.c` and `tests/test_input_step.c` and compare every observable result byte for byte, and the own-time gate paired against 3.13's step-note commit.

## User Stories

1. As the plan 2 implementer, I want every function of `cart.c` in `game.cart` with its C name and in C order, so that the checklist reports the file complete (23/23).
2. As the plan 2 implementer, I want every function of `input_step.c` in `game.input_step` with its C name and in C order, so that the checklist reports the file complete (9/9).
3. As the plan 2 implementer, I want `cart_game_registry_reserve` ported and compared with the C by `lua-test-modules`, so that `make checklist STEP=3.14` passes with 1/1 tables.
4. As the plan 2 implementer, I want `CART_NO_DEMO_FRAME`, `INPUT_STEP_BINDINGS_SESSION`, `INPUT_STEP_BINDINGS_DEFAULT` and `SIZEOF.g_save_bytes` compared with the C, so that no constant drifts.
5. As the plan 3 debugger, I want each Lua cart and input-step function beside its C original with the same calls in the same order, so that a replay diverging at a start, a resume or a demo can be traced line by line.
6. As the plan 3 debugger, I want `cart_load` to run every init in the C order, with the same `assert` messages on the class order, so that a reordered init fails as loudly as in C.
7. As the plan 3 debugger, I want `cart_load` to call `data_init` only when the dispatch context holds a memory, as the C does, so that a load without a runtime behaves the same.
8. As the plan 3 debugger, I want `cart_load` to install `cart_request_start_game` and `cart_init_method` into help and the score panel after their inits, and `cart_init` to assert by function identity that they are still installed, so that the bridge contract of the C holds.
9. As a player, I want the title built by `cart_init` with the same world reset, project expiry, cursor reset, closed message box, cleared main economy and score panel, and help screen, so that the Lua title is the C title.
10. As a player, I want "resume" offered on the title only when the storage holds a record that decodes, so that a missing, garbled or flipped save hides it and is left alone.
11. As a player, I want a title difficulty button to show the loading screen on its press frame and the next, and the galaxy generated on the frame after, so that the start looks and draws like the C one (`LOADING_SCREEN_FRAMES` frames).
12. As the plan 3 debugger, I want `cart_request_start_game` to copy exactly the five fields the C copies (`name`, `has_trade`, `trade`, `has_type_bias`, `type_bias`) into a fresh zero button, so that a later change to the title button cannot reach the started game.
13. As the plan 3 debugger, I want `cart_start_game` to call, in order, the difficulty set, the game zone flip, the world reset with `cart_game_registry_reserve`, the project expiry, the background, `galaxy_populate(90)`, the interface (main economy, score panel, galaxy menu buttons when the pause menu is open, the seven structure icons in the C kind order with their tooltips), and the recompute, so that the RNG stream and the registration order match the C.
14. As a player, I want a resumed game to be rebuilt by `cart_resume_game` through the four save-load stages and the same start-world and interface calls, with the C's assert on the background seed, so that resuming in Lua gives the C game.
15. As a player, I want the game written to the storage at the end of the frame of each action, held back while a wormhole is live, erased when the game ends, and never written for a game with no difficulty, so that the save behaves as in C.
16. As the plan 3 debugger, I want the save at frame end to encode into the cart's own byte buffer and write exactly the encoded count, so that the record bytes are the C's.
17. As the plan 3 debugger, I want a cut save (`cart_cut_save`, set by an attract start) to write and erase nothing until `cart_load` or the return to the title ends the cut, so that a demo never touches the player's save.
18. As the plan 3 debugger, I want `cart_return_to_title` to abandon, flush, forget the registries, end the cut, clear the demo frame and run `cart_init`, in the C order, so that the returned title is the boot title.
19. As the plan 3 debugger, I want `cart_title_shown` to return 1 only while the title is built, no loading frame runs, no message box is shown and none is registered, so that attract mode counts idle frames exactly as in C.
20. As the plan 3 debugger, I want `cart_update60` to reset the frame zone, flush the exit queue, update the entities and the ui, then save, in the C order, and to play the loading frames as the C does, so that every update frame matches.
21. As the plan 3 debugger, I want `cart_draw` to draw the loading screen while loading, and otherwise begin the text and button image frames, clear the screen, select the default palette block, draw both layers and the demo overlay with the cart's demo frame, so that every drawn frame matches.
22. As the plan 3 debugger, I want `cart_shutdown` to run every shutdown in the C order, so that a load after a shutdown starts clean.
23. As the plan 3 debugger, I want `cart_flush_exit_queue` to release a pending game zone only after the exit queue flush, so that the zone flip contract holds.
24. As the plan 3 debugger, I want `input_step_init` to reset the whole step, initialise the cheat, hand the bindings to pad navigation, keep its own copy of the session table and set the attract threshold to `ATTRACT_AFTER_DEFAULT`, so that a fresh step is the C's.
25. As the plan 3 debugger, I want the input step's session table to be its own copy, never an alias of the caller's table, so that a later change to the caller's bindings does not reach the game.
26. As the plan 3 debugger, I want `input_step_switch_bindings` to reset the step as init does with the default or the session table while keeping the session copy and the attract state, and to assert on any other value with the C message, so that the core start and the return behave as in C.
27. As the plan 3 debugger, I want `input_step_run` to run attract mode on the raw pad buttons, start the demo record (default bindings, resume, cut save) or return to the title on its actions, then build the played frame from the attract's played buttons through pad navigation and the cheat, and set it as the dispatch context's input, so that every played frame matches the C.
28. As the plan 3 debugger, I want the played input re-pointed at the step's own played frame on every run, as the C does, so that a copied step plays its own frame.
29. As the plan 3 debugger, I want `input_step_run` to return the cheat's fired value and set the cart's demo frame to the attract demo frame minus one while the demo plays, `CART_NO_DEMO_FRAME` otherwise, so that the overlay captions show on the C frames.
30. As the plan 3.15 implementer, I want `game.cart` and `game.input_step` to load under plain LuaJIT without `love`, and to need nothing but a dispatch context and a bindings table from the caller, so that `game/main.lua` only wires the loop.
31. As the plan 3.15 implementer, I want the 3.10 planet-field finding settled for the cart, so that I know `debug_state` alone still carries it.
32. As the maintainer, I want `make test` and `make e2e` not to get slower beyond the gate, so that the fast loop stays fast.
33. As the maintainer, I want the own times of `lua-guard` and `lua-test-modules` measured against 3.13's step-note commit and recorded, with the projection to the end of the plan, so that 3.15's checkpoint knows its margin.
34. As the maintainer, I want the step note to record what the throwaway programs compared and which hand mutations they caught, so that the evidence survives the deleted programs.
35. As the maintainer, I want no permanent behaviour test added (the replays of plan 3 cover the cart and the input step), so that the plan's rule that no C unit test is ported holds.

## Implementation Decisions

### Modules and load order

- `game.cart` (from `cart.h` + `cart.c`) and `game.input_step` (from `input_step.h` + `input_step.c`). No `game/` module requires either: help and the score panel receive cart functions through `help_set_start_game` and `score_panel_set_init` at `cart_load` time, never by `require`. So both require every dependency at load, as the C includes them, and nothing is deferred.
- `game.cart` requires, at load, the already-ported modules it calls (engine: bootstrap, dispatch_context, entity, entity_registry, entity_class, exit_queue, frame_zone, game_zone; game: background, data_init, economy, economy_score, galaxy, grab_arena, help, hud, layout, map_tables, message_box, mouse, planet, planet_family, planet_family_trade, probe, research, save_load, save_state, save_storage, score_panel, scroll, slipway, structure_icon, text_image, button_image, trade, ui, ui_button, wormhole; runtime: fixed, gfx) and the 3.13 modules `game.menu`, `game.loading_screen`, `game.demo_overlay` (`H-3.13-2`). Each call is made on the module that defines the C function (3.6's rule: a function declared in one header and defined in a sibling is called from its defining module), e.g. `camera_init` from `game.scroll`, `toast_init` from `game.message_box`, `modal_backdrop_init` from `game.ui_button`, `structure_kind` from `game.planet_family`, `slipgate_kind` from `game.planet_family_trade`, `tooltip_new` from `game.layout`, `icon_tooltips_table` from `game.map_tables`, `main_economy_typed` from `game.probe`, `reset_world`/`update_entities` from `engine.bootstrap`, `draw_layers` from `engine.entity_registry`. `GALAXY_MENU_COUNT` is read from `game.game_data`, `LOADING_SCREEN_FRAMES` from `game.loading_screen` (`H-3.13-3`).
- `game.input_step` requires `game.cart`, `game.cheat`, `game.bindings`, `game.bindings_default`, `engine.dispatch_context`, and the 3.13 modules `game.attract` and `game.pad_nav` (`H-3.13-2`), at load.

### Constants, table and sizes

- `M.CART_NO_DEMO_FRAME = -1` in `game.cart`; `M.INPUT_STEP_BINDINGS_SESSION = 0`, `M.INPUT_STEP_BINDINGS_DEFAULT = 1` in `game.input_step`. All compared by `lua-test-modules`.
- `M.cart_game_registry_reserve` is an `entity_registry_reserve_t` table `{ live, by_message, by_tag }` (the shape `engine.entity_registry` documents), `by_message` 0-indexed over `ENTITY_MESSAGE_COUNT`, `by_tag` 0-indexed over `TAG_COUNT`, every index present: the designated values of the C (329; draw 322, draw overlay 265, hit test 125, new year 111, refresh 120; structure 8, ascension 3, slipgate 14, probe 2, message box 4, slipway 216, wormhole 3, planet 110, node 110), each keyed by its enum constant read from `engine.entity_registry` and `engine.entity_class` at load, every other index 0. It is built once at load and never changed; `reset_world` receives it by reference, as the C passes a `const` pointer.
- `title_resume_handler`'s `(int)sizeof g_save_bytes` is `SIZEOF.g_save_bytes` (8192), through `local SIZEOF = M.SIZEOF`, compared with the C. The `memset` calls of `input_step.c` set fields (2.1's rule) and need no `SIZEOF` entry.
- Two reference units, joined to `TABLE_REFERENCE_SOURCES`: `src/game/cart.c` (the unit includes `cart.c` to reach the static `g_save_bytes`; prints `CART_NO_DEMO_FRAME`, `SIZEOF.g_save_bytes`, and `cart_game_registry_reserve.live`, `.by_message[%d]`, `.by_tag[%d]` through `table_reference_int`), and `src/game/input_step.h` (the two enum constants).

### `game.cart`: statics and shapes

- Module locals, initialised as the C: `g_title_built = 0`, `g_save_cut = 0`, `g_demo_frame = M.CART_NO_DEMO_FRAME`, `g_loading_frame = 0`, `g_has_requested_option = 0`, `g_requested_option = ui_button.button_zero()`.
- `g_save_bytes` is a 0-indexed array of `SAVE_STATE_MAX_BYTES` zeros built at load (3.1's statics rule: a static array at its C zero), filled in place by `save_storage_read` and `save_state_encode`, as 3.12 decided for byte buffers.
- `g_save_state` is a module-level table filled at load by `save_state_zero`, which `game.save_state` exports for this purpose as a Lua-only module field after its C functions (its existing local, unchanged; the same move 3.6 and 3.9 made for `planet_keys_zero` and `button_zero`). That is the only change to a 3.12 module. Decode and capture reset it as the C's `memset` does.
- `cart_request_start_game` assigns `g_requested_option` a new `ui_button.button_zero()` table on each call (the C's `(button_t){0}` by value), then copies the five fields when the option is not `nil`; `g_has_requested_option` is `(chosen_option ~= nil) and 1 or 0`. `cart_update60` passes `g_requested_option` when `g_has_requested_option ~= 0`, `nil` otherwise (an `if`/`else`).
- `cart_resume_game` builds `difficulty` as a new table that `save_load_difficulty(state, difficulty)` fills (3.12's contract: it resets the caller's table to a zero button, then sets the five fields).
- `start_interface`'s `kind_table` is a 0-indexed local array of seven kind tables in the C order; each icon's `structure_icon_keys_t` is a new `{ kind = ..., tooltip = ... }` table (the C's `= {0}` then two writes), `tooltip_new(game_zone.game_zone(), map_tables.icon_tooltips_table[index])`. `(entity_t *)economy_new()` is the table itself (casts are the identity).
- `cart_init` passes `structure_icon_rest_cursor()`'s table to `structure_icon_set_cursor`, which copies its fields (`vec_make`): the C's `*cursor` by value needs no copy at the call.
- Function identity replaces function-pointer comparison: `cart_load` installs `M.cart_request_start_game` and `M.cart_init_method`, and `cart_init`'s assert compares `help.help_start_game_field_value() == M.cart_request_start_game` and `score_panel.score_panel_init_field_value() == M.cart_init_method`, with the C message verbatim.
- `title_resume_handler` is an `if`/`else` returning the local `resume_saved_game` or `nil` (3.3's rule on ternaries over function pointers), its test `count > 0 and save_state.save_state_decode(g_save_bytes, count, g_save_state) ~= 0`.
- Truth values per 2.1/3.1: `cart_title_shown` and `cart_loading` return 1/0; `game_zone_pending()`, `economy_ready()`, `structure_ready()` and the other `*_ready` predicates are tested `~= 0`; pointer tests `~= nil`; `economy.has_end_status ~= 0 and economy.end_status ~= nil`; `economy.save_pending ~= 0`; `economy.save_pending = 0` writes the int.
- Every C `assert` keeps its message verbatim (`assert(c, "msg")`); the two message-less asserts of `input_step.c` become `assert(c)`.
- `(void)` discarded returns are plain calls.

### `game.input_step`: shapes

- `input_step_t` is a table `{ cheat, pad_nav, played_frame, played, session_bindings, attract }`. `input_step_init(step, bindings)` fills the caller's table (2.1: an init on `T *` fills it): the C's `memset` gives each field its zero (new sub-tables: `cheat` and `pad_nav` and `attract` empty tables their own inits fill; `played_frame` `{ mouse_x = 0, mouse_y = 0, mouse_buttons = 0, pad_buttons = 0 }`; `played` `{ frames = nil, frame_count = 0, current_frame = 0 }`), then `cheat_init`, `pad_nav_init(step.pad_nav, bindings)`, the session copy, `attract_init(step.attract, attract.ATTRACT_AFTER_DEFAULT)`.
- `bindings_t` is copied by value with a local `copy_bindings` (Lua only), a new `{ masks }` whose `masks[mode][action]` are copied over `BINDINGS_MODE_COUNT` × `BINDINGS_ACTION_COUNT` from 0 (2.1: `memcpy` is a loop over the C length), never an alias. If 3.13 exports a bindings copy for `pad_nav_init` (`H-3.13-4`), `input_step` calls that one instead and has no local copy.
- `input_step_switch_bindings` copies `step.session_bindings` with `copy_bindings` and `step.attract` as a new four-field table `{ attract_after, idle_frames, phase, demo_frame }` (a by-value struct, `H-3.13-4`), chooses the default table (`bindings_default_module.bindings_default`) or the local session copy with an `if`/`else`, re-inits the step, then stores both copies back. Its assert keeps the C message.
- `input_step_run`: `attract_frame(step.attract, cart.cart_title_shown(), raw.pad_buttons, buttons)` returns `action, buttons` (the out-parameter rule; `buttons` starts at 0, `H-3.13-4`); the action branches are an `if` chain on `ATTRACT_ACTION_START` / `ATTRACT_ACTION_RETURN`; the `memset` of the played frame zeroes its four fields in place (the table keeps its identity, so `input_step_played_frame` keeps returning the step's own frame); `pad_nav_step(step.pad_nav, buttons, step.played_frame)` fills the frame in place (`H-3.13-4`); then `step.played.frames = { [0] = step.played_frame }` (a `p8_input_t`'s frames are 0-indexed, `game/p8/input.lua`), `frame_count = 1`, `current_frame = 0`, set every run as the C does; `cheat_step`; `dispatch_context_set_input(step.played)`; the demo frame through `cart_set_demo_frame` with an `if`/`else`.
- `input_step_start_record(step, state)` passes `attract_demo_record()`'s table to `cart_resume_game` unchanged; the cart and the save load only read it (`H-3.13-4`).

### The 3.10 planet-field finding, applied to the cart

- The cart reads no planet field (it calls modules that 3.12 already cleared: `save_state_capture`, the four `save_load` stages) and the input step touches no planet. No nil-as-0 fallback is added. The instrumented trap of 3.12 (a metatable trapping reads of `sprite`, `palette_block`, `acceleration_factor`, `speed` while nil) is rerun over the throwaway scenarios below; zero trapped reads goes in the note; the trap remains for 3.15's `debug_state`.

### Verification of behaviour (throwaway, not committed)

- A throwaway C harness linking the game objects and its Lua twin bring the runtime up the way `src/cmd/replay.c` does (`dispatch_context_set_gfx`/`_mem`/`_input`, `cart_load`, `cart_init`, `input_step_init` with the default bindings and a small `input_step_set_attract_after`), with an in-memory storage on both sides (the C devtools memory backend; on the Lua side a stub table in place of `game.save_storage`, swapped through `package.loaded` in the throwaway only). Logic scenarios run under plain LuaJIT; scenarios that draw run in a throwaway game directory under `tmp/fake_love`, as 2.1 and 3.5 did.
- Scenarios, C vs Lua, each mirroring a test of `tests/test_cart.c` or `tests/test_input_step.c`: the title with an empty, garbled, one-byte-flipped and valid storage (resume handler present or not, storage left alone); a start from each of the four difficulty buttons and from `nil`, frame by frame through the loading frames; a started game with no action (nothing written), with one action (written at the end of its frame), with a live wormhole (held back until it is gone), ending (erased, also during a wormhole), bankruptcy, restart; every record of 3.12's corpus resumed (writes nothing, hides the title); a box open over the title; return to the title; cut save; `cart_load` ending a cut; `cart_shutdown` then `cart_load`; the input step over synthetic pad streams (a few hundred frames each) covering the pad mask, the cheat chords, the bindings switch both ways, the attract start on the threshold frame, a return mid-demo, a press held from the demo, and two consecutive attract starts.
- Compared each frame: the return values (`cart_title_shown`, `cart_loading`, `input_step_run`'s fired value, the attract phase and demo frame), the played frame's four fields, the storage contents after the frame (bytes), the cart's demo frame, the RNG state, and a throwaway world dump (the 3.12 dump: every live entity in registration order with class, state, positions, layer and the planet and economy fields; the registries' counts by message and tag). For the drawing scenarios (title, both loading frames, the first frames of each started game, a dozen demo frames with captions), the framebuffer bytes and the draw meter's text are compared too.
- Hand mutations of the Lua cart and input step (two init calls swapped, `start_interface`'s kind order changed, the save-at-frame-end wormhole test dropped, the cut ignored, the played frame not re-pointed, the demo frame off by one, the session table aliased instead of copied) must each be caught; the note records how many.
- A divergence found in an earlier module (as 3.7 found `tech_gate`'s `has`) is fixed with `fix-bug` in its own commit before the step's commit, and the note says so. These checks are bounded scenarios, not the e2e replays: they never read `tests/e2e/*.sha256`, and running the replays stays plan 3's work.

### Process

- No question needed `guideline`: the plan, the conventions of the step notes, the C sources and the C tests settled every point above. The PRD's agent decided itself: exporting `save_state_zero` from `game.save_state` for the cart's static save state; the local `copy_bindings` (unless 3.13 provides one); the one-element frames array rebuilt every run.

## Implementation Chunks

1. **Check the 3.13 hypotheses.** Read 3.13's committed modules and step note; confirm or correct each `H-3.13-k` below (module names, exported shapes, conventions added, `LUA_DEFERRED_MODULES`, own-time figures, the before commit's hash) and adjust the decisions above that depend on a corrected one before writing code. No commit of its own: the corrections go in the step note.
2. **The cart.** `game.cart` in C order with its statics, `cart_game_registry_reserve`, `SIZEOF`, and the `save_state_zero` export in `game.save_state`; the `cart.c` reference unit and its `TABLE_REFERENCE_SOURCES` entry. Verified: loads, blocks compare equal, `cart.c` counted complete (23/23, 1/1); the cart scenarios that need no input step (title and resume handler, starts and loading frames, save at frame end, resumes, return to title, cut save, shutdown and reload, drawing) byte-identical; the planet-field trap counts zero; hand mutations caught.
3. **The input step.** `game.input_step` with its constants and `copy_bindings` (it requires `game.cart`, hence after chunk 2); the `input_step.h` reference unit and its `TABLE_REFERENCE_SOURCES` entry. Verified: `make checklist STEP=3.14` passes (32/32, 1/1); the input-step scenarios, the attract start and return through the cart included, byte-identical; hand mutations caught.
4. **Own-time gate and step note.** Own times of `lua-guard` and `lua-test-modules`, paired against 3.13's step-note commit (`H-3.13-1`), 45 pairs as 3.3 to 3.12 (run twice if the first run is within 1 ms of a bound, as 3.11 and 3.12 did); pass when each stays under 100 ms and grows within 2× of 3.1's cost per KB (0.10 ms per KB for `lua-guard`, 0.043 ms per KB load for `lua-test-modules`) and per table (about 17 µs per block plus 0.21 µs per line). The `### Step 3.14` note in the plan records: the modules, the checklist counts, the `save_state_zero` export, which `H-3.13-k` held and which were corrected, the load order (nothing deferred), the planet-field result (zero trapped reads; the trap remains for 3.15), what the throwaway scenarios compared and the hand mutations caught, any `fix-bug` commit, the own-time figures (KB of Lua, blocks, lines, ms before and after, ms per KB) and the projection to the end of the plan for 3.15's checkpoint. No full rule-4 protocol unless the gate fails; if it fails (either time at or above 100 ms, or growth beyond 2×), the full protocol runs and the rework is picked from the recorded own times, as the Phase 3 header says.

## Testing Decisions

- A good test here checks external behaviour against the C reference: return values, the played frame, the storage bytes, the drawn framebuffer and the world the game holds after each frame, never the Lua internals. The C game is the oracle; every comparison is C vs Lua on the same input.
- Permanent checks: none new. The step adds its two files, one table and four constants to the three Phase 3 checks (`lua-guard`'s undeclared-global rule, the load check and the data check of `lua-test-modules`) and adds no `make test` target, per the Phase 3 header. The two reference units are the step's registered data.
- Behaviour is checked by throwaway C-vs-Lua programs (prior art: every step note from 3.4 to 3.12, "behaviour checked against the C with throwaway programs, all byte-identical", hand mutations caught as in 3.11 and 3.12), deleted before commit; their results go in the step note. Their scenarios mirror `tests/test_cart.c` and `tests/test_input_step.c`, which stay C-only (plan 2's rules port no C unit test).
- The drawing scenarios run in `tmp/fake_love` with a throwaway game directory (prior art: 2.1's pixel check, 3.5's row-image screens); no fake check is added to `make test` or `make e2e`.

## Out of Scope

- `game/main.lua`, `game/conf.lua`, the loop, the canvas scaled to the window, `--attract-after`, reading `bindings.cfg`, `debug_state()` and `src/devtools/state_format.c` (3.15).
- Menus, title, attract, demo data and overlay, loading screen, pad navigation (3.13): used here, not written or changed here, except through a `fix-bug` commit if a throwaway scenario proves a divergence there.
- `src/devtools/` (the memory storage backend, `record_start`, `pad_metrics`, `save_check`, `largest_slots`) and `src/cmd/` (the replay tool's loop): not ported, per plan 2's rules.
- Running the e2e replays or `make love-e2e`, and any change to `tests/e2e/` (plan 3).
- Audio.

## Further Notes

### Hypotheses about step 3.13

- **H-3.13-1.** 3.13 ends with a `docs: record step 3.13 of plan 2` commit directly on top of its feature commits; that commit is this step's "before" commit for the own-time gate. Its hash is unknown when this PRD is written.
- **H-3.13-2.** 3.13's modules are `game.menu`, `game.attract`, `game.demo_overlay`, `game.loading_screen`, `game.pad_nav` and the generated `game.demo_data` (the checklist's unit line names `game/game/demo_data.lua`), each exporting its C functions under their C names (`menu_init`, `menu_shutdown`, `pause_menu_open_box`, `galaxy_menu_button`; `attract_init`, `attract_frame`, `attract_phase`, `attract_demo_frame`, `attract_demo_record`; `demo_overlay_draw`; `loading_screen_draw`; `pad_nav_init`, `pad_nav_step`).
- **H-3.13-3.** `LOADING_SCREEN_FRAMES` (2) is a field of `game.loading_screen`; `ATTRACT_AFTER_DEFAULT`, `ATTRACT_PHASE_*` and `ATTRACT_ACTION_*` are fields of `game.attract`, all compared by `lua-test-modules`.
- **H-3.13-4.** 3.13 follows 2.1's conventions without new ones affecting this step: `attract_init(attract, attract_after)` and `pad_nav_init(nav, bindings)` fill the caller's table, and `pad_nav_init` copies the bindings into `nav.bindings` (no alias); `attract_frame(attract, on_title, raw_buttons, played_buttons)` returns `action, played_buttons`; `pad_nav_step(nav, raw_buttons, played)` fills `played` in place and returns nothing; an `attract_t` is `{ attract_after, idle_frames, phase, demo_frame }` of plain numbers; `attract_demo_record()` returns the module's own decoded save-state table, which callers only read. If 3.13 adds a bindings copy helper, an attract-state copy helper, or a convention that changes one of these shapes, 3.14 uses it and the note says so.
- **H-3.13-5.** Every 3.13 module loads under plain LuaJIT without `love`, and `LUA_DEFERRED_MODULES` is still empty at 3.13's end, so 3.14 needs no `--defer`.
- **H-3.13-6.** After 3.13, `lua-guard` is about 80–92 ms and `lua-test-modules` about 55–62 ms (3.12's 73.7–77.4 and 51.4–55.3 ms, plus 3.13's 135 functions and the generated demo data, at about 0.07 ms per KB). If `lua-guard` is already above about 95 ms at the before commit, this step's roughly +1–2 ms may cross 100 ms: the gate then fails and the full protocol picks the rework (moving demo data out of `lua-guard`'s scan, or the large-table comparison, first, per 3.1's and 3.3's notes).
- **H-3.13-7.** 3.13 changes no module this step calls with a different signature (help, score panel, save load, save state, bootstrap), and its menu, not the cart, owns the galaxy menu buttons' handlers.

### Expected cost

About 460 C lines become roughly 15–18 KB of Lua: `lua-guard` about +1.1–1.6 ms (3.3–3.12 rate, 0.055–0.088 ms per KB); `lua-test-modules` about +1 ms (two small modules whose dependencies are already loaded, one table of 15 values, four constants, and the 8,192-entry zero buffer built at load, a few tens of microseconds). The note updates 3.12's projection (`lua-guard` 82–90 ms at the end) with 3.13's and 3.14's real figures for 3.15, which adds `debug_state` and `main.lua` and runs the checkpoint.
