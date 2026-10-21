---
title: "Plan 2, step 3.15: game/main.lua, game/conf.lua, the loop and debug_state()"
description: "The Lua game has every module but nothing LÖVE can run: no conf.lua, no main.lua driving the cart frame by frame, no scaled window, no --attract-after, and no debug_state() giving the C state dump."
status: needs-triage
---

## Source

Plan file: `docs/love-port/plan-2.md`, Phase 3 — Engine and game. Step, verbatim:

> 3.15. `game/main.lua` and `game/conf.lua`: the loop, the canvas scaled to the
> window, `--attract-after`, `debug_state()`. Verified as the steps above,
> the checklist complete, and `tmp/fake_love` running the game without a
> refusal over a synthetic no-input file of a few hundred frames, with a
> small `attract_after` so the attract demo plays the game, and with its
> state path so `debug_state()` runs (not an e2e replay; its output is not
> compared).

From the Phase 3 header: "Verified as the steps above" means every C file assigned to the step has its Lua file and every C function its Lua function (the checklist); every Lua file of the step loads under LuaJIT with its `require`s; `lua-guard`'s undeclared-global rule passes; `lua-test-modules` finds the step's data tables equal to the C ones. Steps 3.2 to 3.15 add files and tables to these checks and **add no `make test` target**. 3.15 is a **timing checkpoint**: "The full rule-4 protocol runs at the end of 3.3, 3.9 and 3.15, against `8dff812` and the 'before' commit of the span since the last checkpoint". The last checkpoint is 3.9, so the span's before commit is 3.9's step-note commit `457d574` (`docs: record step 3.9 of plan 2`), as 3.9 itself measured against `a880ee9`, 3.3's step-note commit.

Step notes this PRD carries:

- **Step 1.1** — "Open for 3.15: `GPU_FRAME_BUFFER_BYTES` is not ported, but `state_format.c` prints it, so `debug_state()` must still give that number." The checklist assigns `src/devtools/state_format.c` to 3.15 (`game/devtools/state_format.lua`): 30 functions, 2 tables (`SLOT_KIND_NAMES`, `PARTICLE_NAMES`), counted 0/30 and 0/2 at `ec42dda`.
- **Step 3.10** — latent planet-field finding: planet tables leave `sprite`, `palette_block`, `acceleration_factor` and `speed` nil on kinds that never write them, where the C reads 0. "Steps that serialize or dump planets (3.12 save, 3.15 `debug_state`) must not read them on kinds that do not write them, or must treat nil as 0." Step 3.12: "The trap remains for 3.15."
- **Step 2.1** — `gpu_sizes` macros stay not ported; `draw_meter.lua` holds `GPU_CLUT_SLOTS` and `GPU_PATTERN_SLOTS` as two module locals.

Step 3.14 (the cart and the input step) is **not done** when this PRD is written. Every statement below about its outcome is a hypothesis labelled `H-3.14-k`, listed under Further Notes, and checked first (chunk 1). Those taken from 3.14's own PRD (in triage, not committed) are marked so.

## Problem Statement

The Lua game is a faithful copy of the C game, module by module, up to (by 3.14) the cart and the input step. LÖVE still cannot run it: there is no `game/conf.lua` to set the window, and no `game/main.lua` to bring up the runtime, read the pad, play one game frame per `love.update`, draw it onto the game canvas in `love.draw`, and show that canvas scaled up in the window. The replays' `"attract_after"` reaches the game only as `love.load`'s `--attract-after N` argument, and nothing reads it. The fake LÖVE calls the global `debug_state()` after the last frame when it gets a state path, and nothing defines it: there is no Lua port of `src/devtools/state_format.c`, the text the C replay writes as its end state. Without these, plan 2 cannot meet its Done-when ("the game runs the synthetic no-input file in `tmp/fake_love`, `debug_state()` included, without a refusal"), and plan 3 has nothing to run `make love-e2e` on.

There is one more gap: design.md's LÖVE function list has no way to draw a canvas onto the screen, let alone scaled. `draw(image, quad, x, y)` is its only draw form, the fake refuses a Canvas where an Image belongs, and `love.graphics` is allowed only in `game/love/`. "The canvas scaled to the window" needs one new, minimal form on the list, with its real-LÖVE evidence.

## Solution

From the player's and the plan's side:

- `game/conf.lua` opens a fixed 640×480 window (the 160×120 canvas at scale 4), titled "Slipways", not resizable, LÖVE 11.5, with the audio modules off.
- `game/main.lua` is the loop of `src/cmd/replay.c` without its devtools: in `love.load` it creates the game canvas first, brings up the runtime and the LÖVE backend, installs the draw meter, reads `bindings.cfg` (the default table when it is absent), sets the dispatch context, runs `cart_load` and `cart_init`, then initialises the input step with the bindings and the `--attract-after` threshold. Each `love.update` reads the 12 pad buttons into a raw frame, runs the input step and the cart's update. Each `love.draw` draws the cart onto the canvas, closes the meter's frame and presents the canvas scaled to the window. A global `debug_state()` returns the C replay's state file text: `state_format` followed by the `cheat activations` line.
- `game/devtools/state_format.lua`: every function of `state_format.c` in C order, its two name tables compared with the C.
- The fake LÖVE gains one draw form, `draw(canvas, x, y, 0, s, s)`, accepted only onto the screen, under white and with no translation, and modelling nothing. `game_frame`'s real-LÖVE capture uses it, and its PNG stays byte-identical. `game/love/screen.lua` gets `screen_present()` and `SCREEN_SCALE`.
- Verified by the checklist complete (`make checklist`: no function or table missing), the three Phase 3 checks, a throwaway C-vs-Lua comparison of the state text, the fake running the game over a synthetic no-input file of 600 frames with `"attract_after": 60` and a state path, the 3.10 planet-field trap counting zero reads, and the full rule-4 protocol.

## User Stories

1. As the plan 2 implementer, I want every function of `state_format.c` in `game/devtools/state_format.lua` with its C name and in C order, so that the checklist reports 3.15 complete (30/30 functions).
2. As the plan 2 implementer, I want `SLOT_KIND_NAMES` and `PARTICLE_NAMES` ported as module fields and compared with the C by `lua-test-modules`, so that `make checklist STEP=3.15` passes with 2/2 tables.
3. As the plan 2 implementer, I want `make checklist` to report no function and no table missing in the whole tree, so that the plan's first Done-when holds.
4. As the plan 2 implementer, I want `game/main.lua`, `game/conf.lua` and `game/devtools/state_format.lua` scanned by `lua-guard` (compile, undeclared globals, LÖVE names), so that the static rules cover the whole game.
5. As the plan 2 implementer, I want `main.lua` to write no global but `debug_state` and the six LÖVE callbacks it defines, so that the guard's existing allowance (`debug_state` in `game/main.lua` only) is enough.
6. As a player, I want a 640×480 window showing the 160×120 game at scale 4 with nearest filtering, so that the pixels are sharp and square.
7. As a player, I want the window title "Slipways" and no audio modules loaded, so that the game opens cleanly with audio out of scope.
8. As a player, I want the game canvas created first in `love.load`, after `setDefaultFilter("nearest", "nearest")`, so that the canvas is nearest when scaled and is the fake's canvas 0.
9. As a player, I want the 12 pad buttons of the design's table read each frame from the first joystick, so that my pad drives the game as the replays do.
10. As a player without a pad, I want the game to run on an empty pad mask, so that the title and the attract demo still play.
11. As a player, I want my `bindings.cfg` in the save directory read at start, and the default bindings used when there is none, so that my bindings work as in the C game.
12. As a player, I want a malformed `bindings.cfg` to stop the game naming the parse error, so that a broken file is not silently replaced.
13. As the replay harness, I want `love.load({"--attract-after", "N"})` to set the attract threshold to N, so that `"attract_after"` replays play the demo when the C does.
14. As the replay harness, I want `love.load({})` to keep `ATTRACT_AFTER_DEFAULT`, so that replays without the member idle as in C.
15. As a developer, I want a malformed or repeated `--attract-after` refused with an error naming the value, so that a typo is not taken for the default.
16. As the plan 3 debugger, I want `love.load` to run the C replay's start in its order (vram, gfx, sprite sheet, backend, dispatch context, draw meter, `cart_load`, `cart_init`, `input_step_init`, threshold), so that the first frame matches the C's.
17. As the plan 3 debugger, I want each `love.update` to run `input_step_run` then `cart_update60`, and each `love.draw` to run `cart_draw` then `p8_draw_meter_frame_end`, so that the per-frame order is replay.c's.
18. As the plan 3 debugger, I want a new raw frame table for each update, holding the pad mask and zero mouse fields, so that no module can keep a stale frame.
19. As the plan 3 debugger, I want the dispatch context to hold a one-frame zero input before `cart_load`, as replay.c sets its recording before the cart starts, so that nothing in the inits reads a nil input.
20. As the replay harness, I want `debug_state()` to return the C replay's state file text (`state_format` then `cheat activations N`), N summed over every frame's `input_step_run`, so that plan 3 can compare it with `.end-state`.
21. As the plan 3 debugger, I want the draw meter installed from the start, so that `debug_state()`'s `gpu_*` lines carry the same numbers as the C replay run with `STATE`.
22. As the plan 3 debugger, I want `debug_state()` to print `GPU_FRAME_BUFFER_BYTES`, `GPU_BUILD_BUFFER_BYTES`, `GPU_CLUT_SLOTS` and `GPU_PATTERN_SLOTS` with the C values, so that the step 1.1 note is settled.
23. As the plan 3 debugger, I want `add_fixed` to print every 16.16 value as the C does (sign, whole part, two-digit hundredths, `-32768` included), so that no state line differs on an edge value.
24. As the plan 3 debugger, I want `state_format`'s class, state and id lookups to compare class tables by identity and test C ints with `~= 0`, so that every line names what the C names.
25. As the plan 3 debugger, I want the state text to read none of `sprite`, `palette_block`, `acceleration_factor`, `speed`, proven by the 3.12 trap, so that the 3.10 finding is closed for `debug_state`.
26. As the plan 3 debugger, I want `state_format_save_summary`, `state_format_stopped` and `state_format_cheat_activations` ported too, even where `main.lua` uses only one, so that the file is complete beside its C original.
27. As the fake LÖVE, I want one new draw form, `draw(canvas, x, y, 0, s, s)`, accepted only with the screen selected, so that the game can show its canvas in the window without the fake modelling the screen.
28. As the fake LÖVE, I want that form refused when a canvas is selected, when the colour is not white, when the translation is not zero, and when the scale is not one integer from 1 to 16 on both axes, so that each way real LÖVE would tint, shift, blur or self-draw the window is caught.
29. As the maintainer, I want the new form proven in real LÖVE 11.5 by the `game_frame` capture, its PNG byte-identical after `make love-capture`, so that the list's rule "extending it is a decision recorded here, with its real-LÖVE capture" holds without a new `make test` target.
30. As the maintainer, I want design.md to state the present form and its rule for the game, so that the contract stays written down in one place.
31. As the plan 2 implementer, I want `tmp/fake_love game <synthetic>.json <capture> <state>` to run 600 no-input frames with `"attract_after": 60` to exit 0 and write the state file, so that the plan's last Done-when holds.
32. As the plan 2 implementer, I want the smoke run to pass through the title, the attract start, and about 540 frames of the demo playing the game, so that the loop, the cart, the input step and the demo are exercised together.
33. As the maintainer, I want the smoke run kept out of `make test`, `make e2e`, `make test-all` and CI, and its capture and state never compared, so that it neither slows the loop nor does plan 3's work.
34. As the maintainer, I want the full rule-4 protocol run against `8dff812` and `457d574` and recorded, so that the checkpoint shows plan 2's checks stayed within 3 %.
35. As the maintainer, I want the own times of `lua-guard` and `lua-test-modules` recorded against 3.14's step-note commit, so that the projection the earlier notes carried gets its final figure.
36. As the maintainer, I want the step note to record which `H-3.14-k` held, the smoke run's figures, the trap's count and the comparison's coverage, so that the evidence survives the deleted throwaway programs.
37. As a future iOS porter, I want the window size in one constant shared by `conf.lua` and `main.lua`, so that a later change of scale touches one place.

## Implementation Decisions

### Modules

- **`game/devtools/state_format.lua`** (module `devtools.state_format`, from `state_format.h` + `state_format.c`). It follows the 2.1, 3.1 and later conventions. Statics come first: `M.SLOT_KIND_NAMES` and `M.PARTICLE_NAMES`, 0-indexed Lua strings, as module fields so the data check can compare them. Then every function in C order: `static` ones as `local function`, the four exported as `function M.<name>(`. The `#include`d modules are required at load: entity, entity_geom, entity_registry, exit_queue, frame_zone, game_zone, zone_sizes, economy_score, game_data, probe (for `main_economy_typed` and `main_economy_placeholder`, which 3.8 placed there), particle_zone, planet, planet_family, planet_family_trade, slipway, ui_zone, draw_meter, fixed. Each call goes to the defining module (3.6's rule). It needs no `love` and loads under plain LuaJIT, so `lua-test-modules` loads it like any module.
- **`game/main.lua`** (no C original: `module_verb[_object]` local names; skipped by `lua-test-modules` as now; loaded and run by the fake). It defines `love.load`, `love.update`, `love.draw` and the global `debug_state`. It defines neither `love.gamepadpressed` nor `love.gamepadreleased`: the pad is polled.
- **`game/conf.lua`**: defines `love.conf(t)` only.
- **`game/love/screen.lua`** (existing): adds `M.SCREEN_SCALE = 4` and `M.screen_present()`. Only `game/love/` may call `love.graphics` (lua-guard's glue rule), so the present lives here.
- **`tools/fake_love/`** (C): the new draw form, in the module that implements `love.graphics.draw`, shared by `tmp/fake_love` and `tmp/love_capture_check`.

### `state_format` in Lua

- **Sink.** A `state_sink_t` is a table `{ write_line, context }`, where `write_line(context, line)` receives one whole line with its `"\n"` (the header's contract). A `line_t` is a table `{ sink, text, length }`; `line_start` builds a new one, asserting the sink and its function with the C message. `line_add(line, format, ...)` formats with `string.format`, the same format text (`%s`, `%d`, `%ld`, `%lu`, `%02ld` all map to `%s`, `%d` and `%02d` on integral numbers), and keeps the C assert `written < LINE_CAP - length` with its message `"state line longer than LINE_CAP"`. `LINE_CAP` (256) is a module local. `line_end` adds `"\n"`, calls the sink, then resets the text.
- **`add_fixed`.** `v` is the signed raw value as a Lua number. When `v < 0`: `"-"`, then `v = -v`. Then `whole = math.floor(v / 65536)` and `hundredths = math.floor((v % 65536) * 100 / 65536)`, both exact in doubles. Never `bit` operations here: `-(-2^31)` is `2^31`, which `bit` wraps back to negative. Prints `%d` alone when the hundredths are 0, else `%d.%02d`.
- **Truth values.** `planet_family_bobs(e) ~= 0`, `entity_geom_is_on_screen(e)` printed as its int, and `entity_has(...) ~= 0 and header.infraspace ~= 0`. `count_colonies` adds `(state == ENTITY_STATE_SETTLED) and 1 or 0`. `slipway_placed` returns 1/0. Pointer tests are `~= nil`.
- **Class lookup.** `class_of` compares `e.class` by identity with `planet.planet_class()`, `planet_family.structure_class()`, and so on, each from its defining module. `write_structures` compares with `planet_family_trade.infraspace_gate_class()`.
- **Casts and fields.** `(const planet_t *)e` is the table itself. A planet's `slipways.count`, the economy's `invented.items[i]` (0-indexed, Lua strings), the entity list's `count` and `items` are read as 3.6 to 3.8 shaped them. `ra_of` returns `entity_pos(e)` or `vec_make(0, 0)`. `difficulty.name ~= nil` gates `write_difficulty`.
- **GPU sizes.** `GPU_FRAME_BUFFER_BYTES` (73728), `GPU_BUILD_BUFFER_BYTES` (14336), `GPU_CLUT_SLOTS` (6) and `GPU_PATTERN_SLOTS` (4) are module locals of `state_format.lua`, copied from `src/p8/gpu_sizes.h`. 2.1 made the same choice for the two in `draw_meter.lua`: the macros stay not ported in the checklist and are not compared by the data check. The throwaway state comparison below checks the printed numbers against the C. This closes the step 1.1 note.
- **Planet fields (3.10 finding).** `state_format` reads a planet's `header` fields, `planet_type`, `has_`/`planet_id` (through `planet_id_or_null`), `slipways`, `state` and `state_time`, and none of the four latent fields. No nil-as-0 fallback is added. The 3.12 trap (a throwaway metatable that counts reads of `sprite`, `palette_block`, `acceleration_factor`, `speed` while nil) is rerun over the state comparison and over the smoke run, and zero reads goes in the note. If it fires, the read is a divergence in an earlier module. It is fixed with `fix-bug` in its own commit, with the read treated as 0 where the C reads 0, and the note records it.
- **Reference unit.** A new `tools/table_reference/devtools_state_format.c` includes `state_format.c` (its tables are statics) and renders both name tables through the `TEXT` X-macro of 3.3. `src/devtools/state_format.c` joins `TABLE_REFERENCE_SOURCES`. If including the `.c` pulls a symbol the reference binary does not link, the unit links the objects it needs. That is decided there and recorded in the note.

### `debug_state()`

- A global function defined in `main.lua`, the only global `lua-guard` lets any game file write. It builds a sink whose `write_line` appends to a local list, calls `state_format.state_format(sink)` then `state_format.state_format_cheat_activations(sink, cheat_activations)`, and returns the concatenation. This is exactly replay.c's `write_state`. It does not flush the exit queue or change any state: replay.c's `write_state` does neither.
- `cheat_activations` is a `main.lua` local, the sum of `input_step_run`'s return over every update since `love.load` (replay.c's `cheat_activations += cheat`).
- The fake calls it once after the last frame, and only with a state path. In real LÖVE nothing calls it. It is harmless there.

### `game/conf.lua`

- It requires `love.screen` and `p8.vram`, which touch no `love` field at load, so they run while `love.graphics` is nil, in real LÖVE and in the fake. `t.version = "11.5"`, `t.identity = "slipways"` (the save directory holding `save.bin` and `bindings.cfg`), `t.window.title = "Slipways"`, `t.window.width = vram.P8_SCREEN_WIDTH * screen.SCREEN_SCALE`, `t.window.height = vram.P8_SCREEN_HEIGHT * screen.SCREEN_SCALE`, `t.window.resizable = false`, `t.window.vsync = 1`, `t.modules.audio = false`, `t.modules.sound = false`. The fake passes empty tables and reads nothing back: these lines matter to real LÖVE only.
- One `love.update` is one game frame (design.md). With vsync on a 60 Hz display that is 60 per second. Other refresh rates, fullscreen, resizing and HiDPI are out of scope (plan 5).

### `game/main.lua`

- **State.** Module locals: `gfx`, `mem` (tables the p8 inits fill), `step` (the `input_step_t` table `input_step_init` fills, `H-3.14-2`), `cheat_activations = 0`, and the pad name table `PAD_BUTTON_NAMES[1..12] = dpup, dpdown, dpleft, dpright, a, b, x, y, leftshoulder, rightshoulder, start, back` (design.md's table; button n is bit n − 1 of `pad_buttons`, as `p8.input`'s `P8_PAD_BUTTON_n`).
- **`love.load(arg)`**, in this order:
  1. `screen.screen_init()`, first: the default filter, then canvas 0, then the assets.
  2. `vram.p8_vram_init(mem)`, `gfx.p8_gfx_init(gfx)`, `sprite_sheet.p8_sprite_sheet_load(mem)`, `gfx.p8_draw_backend_set(backend.backend_get())`.
  3. `dispatch_context_set_gfx(gfx)`, `_set_mem(mem)`, then `_set_input` of a `p8_input_t` holding one zero frame (`{ frames = { [0] = zero frame }, frame_count = 1, current_frame = 0 }`).
  4. `draw_meter.p8_draw_meter_install()`. replay.c installs it only under `STATE`. The game cannot know whether a state path was given, so it always installs it; the meter only counts.
  5. `cart.cart_load()`, `cart.cart_init()` (`H-3.14-4`).
  6. The bindings: `love.filesystem.read("bindings.cfg")`. When nil, `bindings_default.bindings_default`. Otherwise `bindings.bindings_parse("bindings.cfg", text, out, err)`. On 0 it calls `fatal.p8_fatal(err.text)`, as replay.c refuses a fixture that does not parse. `input_step.input_step_init(step, chosen)`, which copies the table (`H-3.14-3`).
  7. The threshold: `input_step.input_step_set_attract_after(step, n)`. `n` is `attract.ATTRACT_AFTER_DEFAULT` unless `arg` holds `"--attract-after"` followed by a decimal string matching `^[1-9]%d*$` whose value is at most 2^31 − 1 (replay_source's rule). A missing, malformed or out-of-range value, or a second `--attract-after`, raises `fatal.p8_fatal` naming the value. Other arguments are ignored, since real LÖVE may pass its own. It is always called, as replay.c always calls it with the source's value or the default.
- **`love.update(dt)`**: `dt` is ignored. It reads the pad. `love.joystick.getJoysticks()[1]` is the pad. When there is none, the mask is 0. Otherwise each `PAD_BUTTON_NAMES[n]` held per `isGamepadDown` sets bit n − 1. It builds a new raw frame `{ mouse_x = 0, mouse_y = 0, mouse_buttons = 0, pad_buttons = mask }`. Then `cheat_activations = cheat_activations + input_step.input_step_run(step, raw)` (`H-3.14-6`), then `cart.cart_update60()`. The pad is polled and does not depend on callback order, which design.md forbids relying on.
- **`love.draw()`**: `screen.screen_frame_begin()`, `cart.cart_draw()`, `screen.screen_frame_end()`, `draw_meter.p8_draw_meter_frame_end()`, `screen.screen_present()`. Every frame ends with no canvas selected (the fake's "Canvas is active" rule). The cart's camera pairs its own `push`/`pop` through the backend.
- **Requires** at load: `bit`, `p8.vram`, `p8.gfx`, `p8.sprite_sheet`, `p8.draw_meter`, `p8.fatal`, `love.screen`, `love.backend`, `engine.dispatch_context`, `game.bindings`, `game.bindings_default`, `game.attract`, `game.cart`, `game.input_step` (`H-3.14-2`), `devtools.state_format`.

### The present: one new form on the LÖVE list

- **Contract (design.md, Images and the function list).** `love.graphics.draw(canvas, x, y, 0, s, s)` with exactly six arguments: a Canvas, two integers of magnitude below 2^20, a rotation equal to 0, and two equal integer scales from 1 to 16. It is accepted only while the screen is selected, the current colour is white `FFFFFFFF` and the current offset is 0 on both axes. In the fake it changes nothing modelled (as every screen draw). Each refusal is an error starting `love.graphics.draw: ` and changes nothing. A canvas selected: real LÖVE refuses a canvas drawn onto itself, and the fake does not model scaled draws between canvases. A non-white colour tints the window. A non-zero offset shifts it. Unequal, fractional or out-of-range scales blur or distort it. Then the usual count, type and finiteness refusals. **The rule for the game:** present canvas 0 once per frame, last in `love.draw`, with the screen selected, after `setColor(1, 1, 1)`, outside any `push`, at `(0, 0)` and scale `SCREEN_SCALE`.
- **`screen.screen_present()`** asserts `screen_init` ran, then `setCanvas()`, `setColor(1, 1, 1)`, `draw(canvas, 0, 0, 0, SCREEN_SCALE, SCREEN_SCALE)`. `setCanvas()` is a no-op after `screen_frame_end`, kept so the function holds alone.
- **Real-LÖVE evidence without a new target.** `tools/love_capture/game_frame.lua` gains the present among its existing screen draws: `setCanvas()`, white, `draw(canvas, 0, 0, 0, 4, 4)`, back to the harness canvas, before its later canvas drawing. `make love-capture` (Xvfb with Mesa, allowed for agents, outside `make test`) must leave `game_frame.png` byte-identical, which proves in real LÖVE 11.5 that the present is legal there and leaves canvas 0 untouched. `love-capture-check-game_frame`, already in `make test`, then runs the form in the fake. design.md's `game_frame` paragraph names it.
- **Fake unit coverage.** The new form's acceptance and each refusal are added as cases to the existing image draw fixtures and `tests/test_fake_love_image_draw.c`, with no new target. The form is checked in both binaries, since they share the graphics module.

### Process decisions

- No `guideline` agent was reachable while this PRD was written. Its author decided these points, recorded here:
  - adding the present form, the only way to meet "the canvas scaled to the window" within the fake's contract;
  - evidence through `game_frame` rather than a new capture, because the Phase 3 header forbids new `make test` targets;
  - fixed scale 4 in a fixed window;
  - polling the pad;
  - always installing the meter;
  - refusing a bad `bindings.cfg` and a bad `--attract-after`;
  - GPU sizes as locals;
  - the smoke run as a throwaway command, not a target.

## Implementation Chunks

1. **Check the 3.14 hypotheses.** Read 3.14's committed `game/game/cart.lua`, `game/game/input_step.lua`, its step note and the commit log. Confirm or correct each `H-3.14-k`: module names, exported functions and shapes, the bindings copy, the before commit's hash, own-time figures, the trap result, conventions added. Adjust the decisions above that depend on a corrected one before writing code. `make checklist` must show only 3.15's 30 functions and 2 tables missing. No commit of its own: the results go in the step note.
2. **`state_format`.** `game/devtools/state_format.lua`, the reference unit and its `TABLE_REFERENCE_SOURCES` entry. Verified by `make checklist STEP=3.15` (30/30, 2/2), and by `make checklist` reporting 0 missing functions and 0 missing tables of 1563 and 529, with 8 not ported. The file loads and both tables compare equal in `lua-test-modules`, and `lua-guard` is clean. A throwaway C-vs-Lua harness, built as 3.12 and 3.13 built theirs (the C game objects with the devtools memory storage on one side, plain LuaJIT, or the fake for drawn frames, on the other), drives both sides into the same worlds and compares the full `state_format` text byte for byte, with the draw meter installed on both sides so the `gpu_*` lines count. The worlds:
   - the title before and after a box;
   - each of the four difficulties started and run 0, 1 and 120 frames;
   - every record of 3.12's save corpus resumed;
   - a game with slipways, an infragate, structures, inventions and a negative credit balance;
   - a dozen demo frames;
   - `add_fixed` over edge values (0, ±1 raw, ±0x7fffffff, −0x80000000, values whose hundredths are 0 and 99).

   The planet-field trap runs over all of it and counts zero. Hand mutations of the Lua file must each be caught, and the note records how many: `add_fixed` rounding, a class swapped, `count_colonies` counting with a boolean, `slipway_placed` inverted, a GPU size off, the camera subtraction dropped.
3. **The present.** The fake's new draw form with its refusals; its cases in the image draw fixtures and test; `game_frame.lua` extended and `make love-capture` rerun with `game_frame.png` unchanged (and every other PNG unchanged); `screen.SCREEN_SCALE` and `screen.screen_present()`; design.md's Images paragraph, function list and `game_frame` paragraph updated, and its sentence "In real LÖVE the game draws its canvas scaled up …" replaced by the rule above. `make test` green.
4. **`conf.lua`, `main.lua` and the smoke run.** Both files as decided. `lua-guard` clean, with `debug_state` the only global written. The smoke run is a throwaway: a synthetic file in `tmp/` holding `"input": "pad"`, `"attract_after": 60` and 600 frames of `{ "pad": [] }` in the replays' format. Then `tmp/fake_love game tmp/<synthetic>.json tmp/<capture>.bin tmp/<state>.txt` must exit 0 with a 5,760,000-byte capture and a state file. The state file's first line is `clock year …`, its last is `cheat activations 0`, and between them are the `difficulty` and `planet` lines of the demo's game (the attract started on frame 60). The capture and the state are not compared with anything, and no e2e replay is run. The same run, from a throwaway copy of `game/` with the planet-field trap injected, counts zero reads. A refusal or Lua error found here is a bug in an earlier module or in the fake contract use. It is fixed with `fix-bug` in its own commit, behaviour checked against the C, before this chunk's commit. The note gives the exact commands, the wall time, and the state file's line count.
5. **Checkpoint timing and the step note.**
   - Own times of `lua-guard` and `lua-test-modules`, paired against 3.14's step-note commit (`H-3.14-1`), 45 pairs as 3.3 to 3.13, run twice when within 1 ms of a bound. Each must stay under 100 ms. A breach is reworked first (3.1's and 3.3's notes: move demo data out of `lua-guard`'s scan, or the large-table comparison, under rule 5).
   - Then the **full rule-4 protocol** of design.md, with no other agent running. Three worktrees under `tmp/`: the current commit (the step's last code commit), `457d574` and `8dff812`. One warming `make test` in each, then 11 rounds of one timed `make test` per worktree, back to back, rotating the first. The statistic is the median of the per-round ratios against each baseline. Both ≤ 3 % passes. Otherwise 11 more rounds decide on the 22 pooled. Still over means fail, and the rework is picked from the recorded own times, as the Phase 3 header says.
   - The `### Step 3.15` note records:
     - the files and checklist counts (plan-wide: nothing missing);
     - which `H-3.14-k` held or were corrected;
     - the present form and the unchanged `game_frame.png`;
     - the GPU sizes decision;
     - the state comparison's worlds, line count and mutations caught;
     - the trap's zero (closing the 3.10 finding);
     - the smoke run's commands and figures;
     - any `fix-bug` commit;
     - the own times (KB of Lua, blocks, lines, ms before and after);
     - the protocol's plain medians, paired medians, spreads, round count, core count and slowest new check's own time.

## Testing Decisions

- A good test checks external behaviour against the C reference: the state text byte for byte, the fake's acceptance or refusal message, the run's exit status. It never checks Lua internals. The C game is the oracle for `state_format`; real LÖVE 11.5 (through `make love-capture`) is the oracle for the new draw form.
- **Permanent checks:**
  - `state_format.lua` and its two tables join the three Phase 3 checks;
  - `main.lua` and `conf.lua` join `lua-guard`;
  - the new draw form's cases join the existing `test_fake_love_image_draw` test and its fixtures;
  - the present joins the existing `game_frame` capture.

  No `make test` target is added (Phase 3 header). Prior art: every Phase 3 step for the checks; `tests/test_fake_love_image_draw.c` and the `capture/image_draw_*.lua` fixtures for refusals; the `game_frame` capture for real-LÖVE evidence on combined calls.
- **Throwaway checks**, deleted before commit, results in the note:
  - the C-vs-Lua state comparison (prior art: the "behaviour checked against the C with throwaway programs, all byte-identical" of 3.4 to 3.13, with hand mutations as in 3.11 to 3.13);
  - the smoke run in `tmp/fake_love`;
  - the planet-field trap (3.12, 3.13).
- `main.lua` gets no unit test. Its load check is the fake's run (`lua-test-modules` skips `main` and `conf` by design), and its behaviour is plan 3's `make love-e2e`.
- The smoke run is not an e2e replay. It never reads `tests/e2e/`, its capture and state are compared with nothing, and it is not added to `make test`, `make e2e`, `make test-all` or CI: a full game in the fake over 600 frames is far over rule 2's 100 ms budget.

## Out of Scope

- Running `make love-e2e`, comparing any capture or end state with `tests/e2e/`, and fixing divergences found only that way (plan 3).
- Keyboard and mouse play, resizable or fullscreen windows, HiDPI, refresh rates other than 60 Hz, and frame pacing by `dt` (plan 5, design.md's open questions).
- `src/devtools/` other than `state_format.c` (`state.c`'s exit hook, `replay_source`, `record_start`, pad metrics, digests, captures), and `src/cmd/` (plan 2's rules). `REPLAY_STARTS=2`, `ATTRACT_STATE`, `SAVE_HEX`, `REPLAY_FILL`: replay-tool features with no game counterpart.
- The `"start": "resume"` replay header: the fake refuses `start`, so the game has no resume-start argument.
- `make love` packaging, and iOS.
- Audio.
- Changing modules of earlier steps, except through a `fix-bug` commit proven by a failing comparison or smoke run.

## Further Notes

### Hypotheses about step 3.14

Items marked "(3.14 PRD)" come from 3.14's uncommitted PRD in `docs/prd/triage/`, not from committed code.

- **H-3.14-1.** 3.14 ends with a `docs: record step 3.14 of plan 2` commit directly on its feature commits. It is this step's "before" commit for the own-time figures. Its hash is unknown here. It is not a protocol baseline: the checkpoint's baselines are `457d574` and `8dff812`.
- **H-3.14-2.** (3.14 PRD) The modules are `game.cart` and `game.input_step`, exporting under C names. `cart_load`, `cart_init`, `cart_update60` and `cart_draw` take no argument and return nothing used here. `input_step_init(step, bindings)` fills the caller's table, `input_step_set_attract_after(step, frames)` takes a plain number, and `input_step_run(step, raw)` returns the cheat's fired value as a number (0 or 1) and sets the dispatch context's input to the step's played frame.
- **H-3.14-3.** (3.14 PRD) `input_step_init` copies the bindings it is given (its own `copy_bindings`, or a 3.13 helper), so `main.lua` may hand it `bindings_default.bindings_default` or a freshly parsed table without aliasing.
- **H-3.14-4.** (3.14 PRD) The cart, as in C, needs the dispatch context's gfx, mem and input set before `cart_load`. `cart_draw` draws through gfx, mem and the installed backend and selects no LÖVE canvas, so `main.lua` brackets it with `screen_frame_begin`/`screen_frame_end`.
- **H-3.14-5.** (3.14 PRD) Both modules load under plain LuaJIT without `love`. The save goes through `game.save_storage`, which touches `love.filesystem` only at call time. `LUA_DEFERRED_MODULES` stays empty.
- **H-3.14-6.** `input_step_run` reads its raw frame's fields (`pad_buttons`, and the mouse fields if the C reads them) and keeps no reference to the table.
- **H-3.14-7.** (3.14 PRD) 3.14 reran the planet-field trap over the cart with zero reads and added no nil fallback, leaving `debug_state` as the last reader to clear.
- **H-3.14-8.** After 3.14, `lua-guard` runs at about 80–90 ms and `lua-test-modules` at about 55–60 ms (3.13's 78.9–80.6 and 54.0–54.9 ms, plus about 15–18 KB of Lua at 0.06–0.08 ms per KB). This step adds about 20 KB (state_format about 14 KB, main and conf about 5 KB), about +1.5 ms to `lua-guard` and under +1 ms to `lua-test-modules` (two small tables).
- **H-3.14-9.** 3.14's own-time gate passed without moving any check out of `make test` and without changing `tools/lua_guard.lua` or `tests/lua/test_modules.lua`. `lua-guard` still allows `debug_state` as a written global in `game/main.lua` only, and `lua-test-modules` still skips `main` and `conf`.
- **H-3.14-10.** 3.14 adds no convention that changes `main.lua`'s wiring, and changes no signature of the runtime bring-up functions (`p8_vram_init`, `p8_gfx_init`, `p8_sprite_sheet_load`, `p8_draw_backend_set`, `p8_draw_meter_install`, the `dispatch_context` setters, `bindings_parse`).
- **H-3.14-11.** After 3.14, `make checklist` shows 3.14 at 32/32 functions and 1/1 tables, and only 3.15's 30 functions and 2 tables missing.

### Expected cost and risk

- The protocol's margin against `8dff812` has run between +0.4 % and +2.9 % across plan 2 (step notes 1.2 to 3.9). This step adds about 2 ms of checks and a few fake test cases, so the paired figures should be noise. If the gate fails, rule 4's rework applies, picked from the own times.
- The smoke run is the first time the whole Lua game runs frame after frame. Errors in earlier modules that the bounded throwaway checks never reached may appear. Each is a `fix-bug` commit before chunk 4's, checked against the C.

## Guideline decision (canvas present)

The present form is approved as drafted. In the same commit, design.md's
sentence "In real LÖVE the game draws its canvas scaled up…" is replaced by
the form and its rule for the game, and the `game_frame` paragraph names the
form; `make love-capture` must leave game_frame.png and every other PNG
byte-identical.
