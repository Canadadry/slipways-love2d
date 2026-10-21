---
title: "Plan 2, step 3.13: menus, title and attract in Lua (menu, pad_nav, attract, demo data and overlay, loading screen)"
description: "The Lua game has no galaxy or pause menu, no tech tree screen, no pad navigation, no attract demo and no loading screen, so step 3.14's cart and input step have nothing to call."
status: needs-triage
---

## Source

Plan file: `docs/love-port/plan-2.md`, Phase 3: Engine and game. Phase 3 header, verbatim:

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

> 3.13. `game/game/` menus, title and attract.

Step 3.12 is done and recorded (`263257a`, the current HEAD, which is this step's "before" commit). 3.13 is not a checkpoint. It runs the own-time gate, not the full rule-4 protocol.

## Problem Statement

The Lua game is a faithful copy of the C game. It now has every module up to the save, but it cannot yet show a menu, take pad input or play the attract demo. Step 3.14's cart and input step call into six C modules that have no Lua copy yet:

- `cart.c` builds the galaxy menu buttons (`galaxy_menu_button`), gates them on `pause_menu_open_box`, and draws `loading_screen_draw` and `demo_overlay_draw`.
- `input_step.c` runs `attract_frame`, decodes `attract_demo_record`, and steps `pad_nav_step` every frame.
- `pad_nav` itself calls `galaxy_menu_find_button` and `pause_menu_shown`.

The pad e2e replays, the attract replay and step 3.15's synthetic run (where the attract demo plays the game) all depend on these modules.

`make checklist STEP=3.13` assigns twelve files to the step: `menu.c`/`.h`, `attract.c`/`.h`, `demo_data.h`, `demo_overlay.c`/`.h`, `loading_screen.c`/`.h`, `pad_nav.c`/`.h`, plus the generated unit `tmp/demo/demo.c`, which maps to `game/game/demo_data.lua`. Today the checklist reports 0/135 functions and 0/9 tables:

| C file | Functions | Tables |
|---|---|---|
| `pad_nav.c` | 102 | none |
| `menu.c` | 20 | `pause_menu_open_box_class_def`, `g_pause_menu_open_box`, `galaxy_menu_on_click_table`, `galaxy_menu_refresh_table` |
| `demo_overlay.c` | 7 | `TAG_TEXT` |
| `attract.c` | 5 | none |
| `loading_screen.c` | 1 | `LOADING_TEXT` |
| `tmp/demo/demo.c` | none | `demo_data_record`, `demo_data_pad_buttons`, `demo_data_captions` |

## Solution

Six new Lua modules under `game/game/`. Each is a faithful copy of its C file under the 2.1, 3.1 and later conventions, with the same functions in the same order:

- `game.loading_screen`: the two-frame "loading" screen.
- `game.demo_overlay`: the "demo" tag and the caption band.
- `game.demo_data` is generated, not hand-written. It comes from `demo/demo.json` through a new `--lua` kind of `demo_generate` and a `make demo-lua` target, as 3.4 did for the default bindings. It holds the demo's save record, its 2,736 pad frames, its seven captions, the header constants and the three counts.
- `game.attract`: the idle counter, the demo playback and the swallow phase, and the decode of the compiled demo record.
- `game.menu`: the galaxy menu (year skip, tech tree, score, pause), the tech tree screen, the pause menu and its open-box stand-in, and the lookup helpers.
- `game.pad_nav`: the whole pad navigation state machine over the pad modes ported in 3.11.

None of the six is in an include cycle. Only `cart`, `input_step` (3.14) and devtools include them, and inside the step only `pad_nav` includes `menu`. So all six require their dependencies at load time and load at once, and `LUA_DEFERRED_MODULES` stays empty.

The step is verified by:

- the checklist (`make checklist STEP=3.13`, 135/135 and 9/9);
- the load check, the undeclared-global check and the data check in `make test`;
- throwaway C-vs-Lua programs, all byte-identical, with hand mutations caught;
- the own-time gate.

## User Stories

1. As the plan 2 implementer, I want every function of `pad_nav.c` in `game.pad_nav` with its C name and in C order, so that the checklist reports the file complete (102/102).
2. As the plan 2 implementer, I want every function of `menu.c` in `game.menu` with its C name and in C order, so that the checklist reports the file complete (20/20).
3. As the plan 2 implementer, I want every function of `attract.c`, `demo_overlay.c` and `loading_screen.c` in their Lua modules, so that the checklist reports 135/135 for the step.
4. As the plan 2 implementer, I want the nine tables of the step ported and compared with the C by `lua-test-modules`, so that `make checklist STEP=3.13` passes with 9/9.
5. As the plan 3 debugger, I want each Lua function beside its C original with the same computations, so that a diverging pad or attract replay can be traced line by line.
6. As the plan 3 developer, I want `game.demo_data` generated from `demo/demo.json` by the same tool that writes `tmp/demo/demo.c`, so that the two can never be written by hand differently.
7. As the maintainer, I want a stale `game/game/demo_data.lua` to fail `make test` through the data check, so that editing the demo without running `make demo-lua` is caught at once.
8. As the maintainer, I want `make demo-lua` listed in `make help` with a one-line description, outside `make test`, so that regenerating the Lua demo is discoverable and costs the loop nothing.
9. As a player watching the title, I want the attract demo to start after `attract_after` idle frames on the title and play the recorded pad frames, so that the Lua title behaves as the C one.
10. As a player, I want any button pressed during the demo to return to the title and be swallowed until released, so that the press that stops the demo never acts on the title.
11. As a player, I want the demo to return to the title by itself after its last frame, so that the attract loop repeats.
12. As the plan 3 debugger, I want `attract_init` to refuse a threshold below 1 with the C's assert message, so that a bad `--attract-after` fails as loudly as in C.
13. As the plan 3 debugger, I want `attract_demo_record` to decode the compiled record through `game.save_state` into a module-level save state, asserting it decodes with the C message, so that 3.14's input step can start the demo world from it.
14. As a player, I want the "demo" tag and the caption of the current demo frame drawn in the same pixels as the C, one or two centred lines, so that the overlay looks the same.
15. As the plan 3 debugger, I want `demo_overlay_find_caption` to return -1 between captions and after the last one, as the C does, so that frames without a caption draw only the tag.
16. As the plan 3 debugger, I want `demo_overlay_draw` to draw with its own copy of the graphics state (camera at 0,0, no fill pattern) and leave the caller's graphics state untouched, so that the next frame's drawing is unaffected.
17. As a player, I want the "loading" screen drawn in the same pixels as the C, with the camera, fill pattern and draw palette the caller had restored afterwards, so that the loading frames match.
18. As a player, I want the galaxy menu's four buttons placed and labelled as in the C, with the tech button's icon lit only when an affordable tech is left, so that the HUD matches.
19. As a player, I want the year-skip button to end the year at once, the score button to show the score panel behind a modal backdrop, and the pause button to open the pause menu, as in the C.
20. As a player, I want the tech tree screen to list every tech of the current era, with its cost in the side tooltip. An unaffordable tech gets a present-but-nil click, an affordable one invents it. An "N invented" row is added when techs are invented, and the "level E (a/b)" description is shown. All of it as in the C.
21. As the plan 3 debugger, I want the pause menu rows' click handlers to stay distinct, non-nil functions, so that buttons draw in colours 12/13 and `pause_menu_shown` recognises the pause menu by row 2's handler identity, as the C does.
22. As the plan 3 debugger, I want `galaxy_menu_find_button` to find the live HUD button by its `on_click` function identity among the hit-test entities, so that pad navigation clicks the right button.
23. As the plan 3 debugger, I want `menu_init` and `menu_shutdown` to set and clear the pause-menu open box exactly as the C does, so that `pause_menu_open_box` returns it only while the menu is up.
24. As the plan 3 debugger, I want `galaxy_menu_button` to assert its range and `menu_init` with the C message, so that a call before init fails as in C.
25. As a pad player, I want `pad_nav_step` to turn each frame's raw buttons into the same played frame (mouse position, mouse buttons) and the same navigation state as the C, so that jump, aim, slipway, build bar, placement, panels, pause and the year-select chord behave identically.
26. As the plan 3 debugger, I want every `pad_nav` query (`pad_nav_aim_mode`, `pad_nav_jump_mode`, `pad_nav_hovered`, `pad_nav_slipway_start`, `pad_nav_slipway_target`, `pad_nav_build_bar_open`, `pad_nav_tech_open`, `pad_nav_score_open`, `pad_nav_build_icon`, `pad_nav_placing`, `pad_nav_focus`, `pad_nav_last_acted`, `pad_nav_select_held`, `pad_nav_year_chain_open`) to return the same entity or 1/0 as the C on every state, so that the HUD, the crosshair and devtools see the same thing.
27. As the plan 3 debugger, I want `pad_nav_init` to copy the bindings table it is given by value, so that a later change to the caller's table does not reach the navigation, as in C.
28. As the plan 3 debugger, I want `pad_nav_init`, when run again on the same navigation table (as `input_step_switch_bindings` does at each attract start and return), to keep the identity of the sub-tables and re-initialise every field, so that it behaves as the C re-initialising a struct at a fixed address.
29. As the plan 3 debugger, I want the C asserts of `pad_nav` (build bar and cycle panel never open together, move or pan actions in the mode, the direction order of `aim_pan`, a non-nil bindings table) kept as Lua `assert`s with the same messages.
30. As the plan 3 debugger, I want the C out-parameters of `pad_nav` turned into extra return values that the caller assigns back to the field the C pointed at. This covers the cursor, the played mouse buttons, the shown entity, the repeat flag and the cancelled kind. Then no value is lost.
31. As the plan 2 implementer, I want the 3.10 latent planet-field finding checked for this step, so that 3.15 knows it alone still carries it.
32. As the maintainer, I want the step's own times of `lua-guard` and `lua-test-modules` measured against `263257a` and recorded, so that the end-of-plan projection stays honest.
33. As the maintainer, I want `make test` and `make e2e` not to get slower beyond the gate, so that the fast loop stays fast.
34. As the maintainer, I want the step note to record what the throwaway comparisons covered (worlds, frames, streams, scenes) and which hand mutations they caught, so that the evidence survives the throwaway programs.
35. As the maintainer, I want no permanent behaviour test added, so that the plan's rule "the C unit tests are not ported" holds and the replays of plan 3 stay the permanent check.

## Implementation Decisions

### Modules and load order

- Six modules: `game.loading_screen`, `game.demo_overlay`, `game.demo_data` (generated), `game.attract`, `game.menu`, `game.pad_nav`. Each module carries the header constants first, then the `.c` functions in C order, with a top comment naming the C files.
- No module of the step is in a cycle. `pad_nav` requires `game.menu` at load. `attract` requires `game.save_state` and `game.demo_data` at load. `menu` requires `economy`, `layout`, `message_box`, `score_panel`, `score_panel_display`, `tech_gate`, `ui`, `ui_button`, `ui_zone`, `p8.str`, `engine.frame_zone` and its other includes at load. All six load alone under plain LuaJIT.
- Header enum constants are module fields with the C names, compared by `lua-test-modules`:
  - `ATTRACT_AFTER_DEFAULT`, `ATTRACT_PHASE_*` and `ATTRACT_ACTION_*` in `game.attract`;
  - `LOADING_SCREEN_FRAMES` in `game.loading_screen`;
  - `PAD_NAV_MAP_*` and `PAD_NAV_PLACEMENT_*` in `game.pad_nav`;
  - `DEMO_DATA_FRAME_LIMIT` and `DEMO_DATA_CAPTION_TEXT_SIZE` in `game.demo_data`.
- `.c`-local enums and macros are module locals, not compared. Examples: `demo_overlay`'s layout enum and `DEMO_OVERLAY_LINE_LENGTH`; `loading_screen`'s text placement; `menu`'s `PAUSE_MENU_OPEN_BOX_HAS_*`; `pad_nav`'s `HELD_FRAMES_CAP`, `cancelled_t`, `PARKED_X`/`PARKED_Y`, `YEAR_SKIP_BUTTON`/`PAUSE_BUTTON`. `loading_screen`'s `TEXT_WIDTH` uses `sizeof LOADING_TEXT`, so it is computed from `SIZEOF.LOADING_TEXT`.
- Function-level `static const` arrays (`move_action`, `pan_action` in `pad_nav`; `pair_items`/`pair` in `tech_tree_screen_step`) are module locals built once at load. They are not file-level tables, and the checklist does not list them.
- `sizeof` uses go through each module's own `M.SIZEOF`, compared with the C:
  - `game.loading_screen`: `SIZEOF.LOADING_TEXT` (8) and `SIZEOF["p8_vram_t.draw_palette"]`;
  - `game.menu`: `SIZEOF.data_row_t`, `SIZEOF.data_item_t` and `SIZEOF.button_keys_t`, for the frame-zone allocations of `invented_lines` and `menu_show_tech_tree_screen`.

### Generated demo data

- `src/cmd/demo_generate.c` gains a `--lua` kind (`demo_generate [--lua] <demo.json> <output>`). It writes `game/game/demo_data.lua` from the same loaded `demo_source_t` as the C unit, with the same temporary-file-then-rename write. A new `make demo-lua` target runs it. It sits outside `make test`, `make e2e` and `make test-all`, and carries a `## ` help line of at most 20 words. Prior art is `bindings_generate --lua` and `make bindings-lua` (step 3.4).
- File shape:
  - The first line is `-- Generated by make demo-lua (src/cmd/demo_generate.c) from demo/demo.json: edit the demo.`
  - The file is `local M = {}` with module fields, then `return M`.
  - `M.demo_data_record` is a 0-indexed array of byte values, so `game.save_state.save_state_decode` reads it as the byte buffer of 3.12's convention.
  - `M.demo_data_pad_buttons` is a 0-indexed array of masks.
  - `M.demo_data_captions` is a 0-indexed array of `{ from, to, text }`, where `text` is the Lua string with `\n` escaped.
  - The file also holds `M.demo_data_record_count`, `M.demo_data_frame_count` and `M.demo_data_caption_count`, plus the two header macros as they are compiled into the generator.
  - Values are written in decimal, 16 per line, to keep the file small for `lua-guard`'s cost per KB. This follows `bindings_default.lua`'s decimal style, where the C unit uses hex.
- The data check compares `game.demo_data` with `tmp/demo/demo.c`, which is already linked into `tmp/table_reference` through `ENGINE_SRCS`. A stale Lua copy fails `make test`. This is the 2.1 generated-data rule.
- The generated module holds several tables, so callers require it as a module (`local demo_data = require("game.demo_data")`). It is not bound to a single table local.

### Shapes and conventions applied

- **Out-parameters** (2.1): an input argument plus an extra return after the C return value, several of them in C order. The caller assigns them back to what the C pointed at:
  - `attract_frame(attract, on_title, raw_buttons, played_buttons)` returns `action, played_buttons`.
  - `screen_point`, `centre_world_point` and `rest_screen_point` return `x, y`.
  - `pad_panel_step`'s `x, y, mouse_buttons` go back to `nav.cursor_x`, `nav.cursor_y` and `played.mouse_buttons`.
  - `pad_panel_focus_point`'s `x, y` go back to the cursor.
  - `select_tap`, `aim_mode_step`, `jump_step` and `slipway_step` return their `shown` after their C return.
  - `map_step` returns `shown, repeat_kept`.
  - `switch_panels` returns `changed, cancelled`.
  - A `void` function returns only its out values.
- **Struct pointers.** `p8_input_frame_t *played` is the caller's table, written in place, because a struct pointer is the table. `attract_t` and `pad_nav_t` are caller tables filled by their inits.
- **`pad_nav_init`.**
  - It copies the bindings by value into `nav.bindings`. That table is created when absent, and its `masks` rows are filled in place from the given table (every mode and action from 0, over the C counts), so its identity survives a re-init. `pad_nav_bindings` returns that table.
  - The inline sub-structs `build`, `cycle`, `jump`, `slipway`, `back` and `select`, and the 0-indexed `held_frames` array of `PAD_JUMP_DIRECTIONS` zeros, are created only when absent, then initialised by their own inits exactly as the C calls them. Every 3.11 init writes every field, so this matches the C re-initialising a struct at a fixed address.
  - Ids stored in `nav` (`focus`, `game`, `suspended_carry`) are new id tables, copied from `ENTITY_ID_NONE` or returned by `entity_id_link`, as 3.2 requires.
- **Graphics copies.** `p8_graphics_t own = *gfx` (in `demo_overlay_draw`) and `found = *gfx` with `*gfx = found` (in `loading_screen_draw`) are by-value struct copies. A local constructor lists the five fields `p8_gfx_init` writes (`camera_x`, `camera_y`, `fillp_pattern`, `fillp_transparent`, `pen_color`). The restore writes those five fields back into the caller's table. The draw-palette `memcpy`s are loops over `SIZEOF["p8_vram_t.draw_palette"]` from index 0.
- **Strings.** `copy_line` into a `char[33]` is `string.sub` of the caption, after the C assert (same message) on its length. `strchr(text, '\n')` is `string.find(text, "\n", 1, true)`. `strlen` is `#`. The second-newline assert keeps its message. `TAG_TEXT` and `LOADING_TEXT` are module fields holding Lua strings.
- **Function identity.**
  - The four galaxy-menu click methods, the tech button's refresh method and the three pause-row methods are distinct `local function`s.
  - `galaxy_menu_on_click_table` and `galaxy_menu_refresh_table` are 0-indexed module fields holding them; refresh entries for indices 0, 2 and 3 are `nil`.
  - `galaxy_menu_button4_on_click_method` is forward-declared, because the C declares it before the table.
  - `pause_menu_row2_on_click_method`, `pause_menu_row3_on_click_method` and `pause_menu_row4_on_click_method` have empty bodies, but they must be three different closures, never one shared function, because `pause_menu_shown` compares row 2's by identity.
  - The local array `my[]` in `galaxy_menu_button4_on_click_method` is built per call, with `score_panel.score_panel_init_field_value()` first.
- **The pause-menu open box.**
  - `pause_menu_open_box_class_def` is a zero class module field.
  - `g_pause_menu_open_box` is a module field built as 3.10's `g_score_panel_layout_block` is: `entity_class.entity_init_header` with the class, then every own field at its C zero value. Those are `has_` (the own `has` takes the trailing underscore of 3.6, since the header keeps `has`), `fixed_pos` and `row_step` as zero vectors, `back_color` 0 and `has_back_color` 0.
  - `pause_menu_open_box` returns that table (the first-member cast is the identity, per 3.1).
  - `menu_init` and `menu_shutdown` change it in place, with `bit.bor`/`bit.band` and `bit.bnot` masked to 16 bits as `(uint16_t)~` does.
  - The module-level `g_menu_up` is a local starting at 0. `g_galaxy_menu_pos` is a 0-indexed local of `GALAXY_MENU_COUNT` vectors, starting as zero vectors.
- **Frame-zone rows.**
  - `invented_lines` allocates its block through `allocator.allocator_alloc(frame_zone.frame_zone(), SIZEOF.data_row_t + n * SIZEOF.data_item_t)`. It writes `count` and `items` (a 0-indexed array of `{ text = ... }` items with every data item field at its zero value, as `score_panel`'s `data_items_zero` does) into the block it returns.
  - `menu_show_tech_tree_screen` allocates `(count + 1) * SIZEOF.button_keys_t` and fills every element with `ui_button.button_keys_zero({})`, as `planet_display`'s projects panel does (3.9/3.10 prior art).
  - `open_box_frame_t frame = {0}` is `message_box.open_box_frame_zero()`. `layout_row_keys_t keys = {0}` and `modal_backdrop_keys_t keys = {0}` use the zero constructors already exported by `layout` and `ui_button`. Where none is exported yet, a local zero constructor lists every C field.
- **Out-parameters of earlier modules.** `game_data.int_seq_at(seq, count, i, out)` returns `found, value`, as its other callers use it.
- **Truth values.** C predicates return 1/0. Comparisons used as values are `(c) and 1 or 0`. An entity pointer test is `~= nil`. `ui_set_pad_crosshair` receives 1/0.
- **Attract.** `g_demo_record` is a module-level table that `save_state_decode` fills on every call, as the C refills its static. `attract_demo_record` asserts the decode returned 1, with the C message, and returns that table.

### The 3.10 latent planet-field finding

- None of the step's six C files reads `sprite`, `palette_block`, `acceleration_factor` or `speed` on a planet. `pad_nav` uses `planet_is_family`, entity state, positions and the pad modes; `menu` reads the economy and the score panel. So the step neither reads them nor adds a nil-as-0 fallback.
- The `pad_nav` stream comparison below runs once with 3.12's instrumented trap on every planet-family table. Zero trapped reads goes in the note, and the trap stays for 3.15's `debug_state` alone.

### Verification of behaviour (throwaway, not committed)

All programs are throwaway C/Lua twins with the same text dump on both sides, deleted before commit, as in every step note from 3.4 to 3.12.

- **Leaves.**
  - Loading screen: drawn over varied starting states (camera offsets, fill patterns, a recoloured palette with transparency changes). The software framebuffer, the restored graphics fields and the restored draw palette must be byte-identical C vs Lua.
  - Demo overlay: `demo_overlay_draw` over every frame from -1 to `demo_data_frame_count + 10`, with a non-zero caller camera and fill pattern. Framebuffers and the caller's graphics must be identical.
  - `demo_overlay_find_caption` over the real captions and a synthetic overlapping list.
  - `attract_demo_record`: the decoded state dumped with 3.12's throwaway dump format must match the C's.
  - Attract streams: `attract_frame` over long random streams (idle runs past `attract_after`, presses during play, swallow, `on_title` toggles, several thresholds including 1, and full demo playback to the end). Every `(action, played_buttons, phase, idle_frames, demo_frame)` must be identical.
- **Menu.** A throwaway game brought up as `cart_start_game` does (`start_world` and `start_interface` written ad hoc, as in 3.12, with `menu_init` before them so the galaxy buttons exist) over several seeds and all four difficulties, then:
  - `galaxy_menu_button`, `galaxy_menu_find_button` and `pause_menu_open_box` before and after `menu_shutdown`;
  - the tech button's refresh over economies with and without affordable techs;
  - each galaxy click (year skip, tech tree, score, pause), then a dump of the open box, its buttons (layout rows, tooltips, side tooltips, the click handler as an index into the module's known functions), the score panel state, the economy and `pause_menu_shown`;
  - the tech tree screen across eras and invented counts, including invented techs and clicks on an affordable row.

  All dumps must be identical.
- **Pad navigation.**
  - The same throwaway game runs an ad hoc frame loop that copies `cart_update60`'s game-frame calls and `input_step_run`'s pad part, both written in the throwaway only (3.14 ports them). Each frame does: `frame_zone_reset`, flush the exit queue, `pad_nav_step` with the frame's buttons into a zeroed played frame, set it as the dispatch input, `update_entities`, `ui_update`.
  - It is driven by the pad button streams of every `tests/e2e/pad_*.json` replay and the demo's `demo_data_pad_buttons`, from fresh games, plus long structured random streams (holds, taps, chords, the cheat chord, Select holds for the year chain).
  - Every frame's dump must be identical C vs Lua: the full `pad_nav_t` (every field and sub-struct, ids as index and generation), the played frame, every query of the header, the shown message box, the pause menu flag, the ui's pad tab and crosshair, the camera, and a compact world summary (live entity count by tag, economy year and money).
  - A world reached by a corpus save record (3.12's corpus, loaded through `save_load` as `cart_resume_game` does) is also driven, so that mid-game states (placement, slipways, end of game) are covered.
- **Hand mutations.** Each module is mutated by hand a few times (a branch inverted, a call order swapped, an out value dropped, a pause-row handler shared). Every mutation must be caught by the matching comparison, and the note records the count.
- If a comparison finds a divergence in an already-ported module (as 3.7 found `tech_gate`'s `has`), it is fixed with `fix-bug` in its own commit before the step's commit, and the note says so.

### Reference units

- One unit per source added to `TABLE_REFERENCE_SOURCES`:
  - `src/game/loading_screen.c` prints `LOADING_TEXT`, `LOADING_SCREEN_FRAMES` and the two sizes.
  - `src/game/demo_overlay.c` prints `TAG_TEXT`.
  - `src/game/attract.h` prints the attract constants.
  - `src/game/demo_data.h` prints the two macros, the three counts and the three arrays from the linked demo unit, with each caption's `from`, `to` and `text`.
  - `src/game/menu.c` prints the class (`reference_class`), the open box header (`reference_header`) and its own fields as `has_` and so on, the two function tables (`table_reference_function`, nothing for `NULL`) and the three sizes.
  - `src/game/pad_nav.h` prints the two enums.
- `menu.c` and `demo_overlay.c` are then included by their units and dropped from the reference link by the existing filter, as for every `.c` source. The demo unit stays linked.

### Process

- The PRD's agent had no `guideline` agent available. It settled these points itself from the plan, the step notes' conventions and the code:
  - the generated `game.demo_data` and its `make demo-lua` target (decimal values);
  - the identity-keeping `pad_nav_init`;
  - the out-parameter returns of `pad_nav`;
  - the throwaway frame loop.

## Implementation Chunks

1. **Demo data and the leaves.** Covers:
   - `demo_generate --lua`, `make demo-lua` and the generated `game/game/demo_data.lua`;
   - `game.loading_screen`, `game.demo_overlay` and `game.attract`;
   - the four reference units (`loading_screen.c`, `demo_overlay.c`, `attract.h`, `demo_data.h`) in `TABLE_REFERENCE_SOURCES`.

   Verified when the four modules load, their blocks (3 demo tables, `TAG_TEXT`, `LOADING_TEXT`, the constants and sizes) compare equal, a hand edit of the Lua demo fails the data check, the checklist counts `attract.c`, `demo_overlay.c`, `loading_screen.c` and the demo unit complete (13 functions, 5 tables), and the leaf comparisons are byte-identical.
2. **The menu.** `game.menu` with its four tables and the `menu.c` reference unit. Verified when the module loads, its blocks compare equal, the checklist counts `menu.c` complete (20/20, 4/4), and the menu comparisons are identical.
3. **Pad navigation, part one.** `game.pad_nav` from the top of `pad_nav.c` through `leave_panel`, in C order: 63 functions, from `pad_nav_panel_open` to `leave_panel`. That is the queries, `pad_nav_init`, the panel step, aim mode, the slipway cancel, the select tap, the marker and the sub-mode unwind, plus the `pad_nav.h` reference unit. Verified when the module loads, its constants compare equal, and the header's queries plus `pad_nav_init`/`pad_nav_bindings`/`pad_nav_focus` give identical results C vs Lua over the menu chunk's worlds and over many navigation tables set field by field (each map mode, placement, cycle panel, open build bar, a dead or missing focus).
4. **Pad navigation, part two.** The rest of `pad_nav.c` in C order: 39 functions, from `map_binding_mode` to `pad_nav_step`. Verified when `make checklist STEP=3.13` passes (135/135, 9/9), every pad stream and the corpus-world streams give identical per-frame dumps, the planet-field trap counts zero reads, and the hand mutations are caught.
5. **Own-time gate and step note.**
   - Measure the own times of `lua-guard` and `lua-test-modules`, paired against `263257a` (45 pairs, as 3.3 to 3.12; run twice if the first is noisy). The gate passes when each stays under 100 ms and grows within 2× of 3.1's cost per KB and per table.
   - Write the `### Step 3.13` note in the plan. It records:
     - the modules and checklist counts;
     - the generated demo data (`make demo-lua`, decimal shape, stale-copy check);
     - the `pad_nav_init` identity rule and the out-parameter returns;
     - the planet-field result (no reads, zero trapped);
     - what each comparison covered (worlds, frames, streams, scenes) and the hand mutations caught;
     - the own-time figures (KB of Lua including the demo data, blocks, lines, ms before and after, ms per KB);
     - the projection to the end of the plan.
   - No full rule-4 protocol runs (this step is not a checkpoint) unless the gate fails.

## Testing Decisions

- A good test here checks external behaviour against the C reference: pixels drawn, the played frame, the navigation and world state afterwards, and return values. It never checks Lua internals. The C game is the oracle, and every comparison is C vs Lua on the same input.
- Permanent checks: none new. The step adds its files and tables to the three Phase 3 checks (`lua-guard`'s undeclared-global rule, plus the load check and the data check of `lua-test-modules`) and adds no `make test` target, per the Phase 3 header. The six reference units are the step's registered data. `make demo-lua` is a generator, not a check.
- Behaviour is checked by throwaway C-vs-Lua programs, deleted before commit, and their results go in the step note. Prior art: the 3.4 to 3.12 notes ("behaviour checked against the C with throwaway programs, all byte-identical", hand mutations caught as in 3.11 and 3.12). The ad hoc game start of 3.12's capture and load check is the starting point for the menu and pad worlds.
- The C tests `test_pad_nav`, `test_attract`, `test_demo_overlay`, `test_loading_screen` and `test_demo_data` are not ported (plan 2's rules). They may be read to choose comparison scenarios.
- Plan 3's pad and attract replays cover these modules permanently.

## Out of Scope

- The cart and the input step (`cart.c`, `input_step.c`, step 3.14): the title screen build, `cart_start_game`, `cart_resume_game`, `cart_update60`, `cart_draw`, `input_step_run` and the bindings switch. These are written ad hoc in the throwaway only.
- `game/main.lua`, `game/conf.lua`, `--attract-after` parsing and `debug_state()` (3.15).
- `src/devtools/` (`demo_source`, `pad_caption`, `pad_metrics`, `largest_slots`, `save_check`): not ported, per plan 2's rules. `demo_generate` is changed only to add its `--lua` output.
- Any change to `demo/demo.json`, the demo's content or the C demo unit's format.
- Running the e2e replays (plan 3); audio.

## Further Notes

- Expected size: about 1,700 C lines become about 60–70 KB of hand-written Lua (`pad_nav` the bulk), plus about 25 KB of generated demo data (3,852 record bytes, 2,736 pad frames).
- Expected cost:
  - `lua-guard`: about +6–8 ms at 3.3–3.12's 0.055–0.088 ms per KB, from about 74–78 ms, so about 82–86 ms. Still under 100 ms, and still the check to watch. 3.12 projected 82–90 ms at the end with 100–140 KB left, and this step is most of that remainder.
  - `lua-test-modules`: about +3 ms (load of about 90 KB, plus about 6,600 reference lines at 0.21 µs and a few dozen blocks).
  - If the gate fails, the full rule-4 protocol runs and the recorded own times pick what to rework. The large demo arrays are the first candidate, under rule 5.
- `attract_demo_record` decodes the 3,852-byte record on each attract start only, never per frame.
- `pad_nav.h`'s `pad_nav_t` lists `bindings` last. The Lua table has no field order, but the throwaway dump prints fields in the C order so that the two dumps compare line for line.
