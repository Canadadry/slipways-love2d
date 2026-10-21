---
title: "game/game/ leaves: bindings, generation, mouse, image caches, visual random"
description: "The Lua game has no bindings, no planet-type picker or random walks, no mouse entity, no grab arenas and no text or button image caches, so no game module of steps 3.5 to 3.15 that reads the pad bindings, draws cached text or buttons, or generates a galaxy can be written."
status: needs-triage
---

## Plan step (verbatim, docs/love-port/plan-2.md, Phase 3 — Engine and game)

The Phase 3 header paragraphs:

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

The step:

> 3.4. `game/game/` leaves: game data, generation, bindings, mouse, map tables,
>    images and text, draw functions.

## What the checklist assigns to this step

`luajit tools/checklist.lua --listing` (authoritative, per the Phase 3 header and step 1.1's note) gives step 3.4 **19 files, 56 functions, 8 tables**:

- `src/game/bindings.{c,h}` → `game/game/bindings.lua`: 14 functions (`bindings_read_button_number`, `bindings_mode_has_action`, `bindings_mask`, `bindings_mode_name`, `bindings_action_name`, the statics `fail`, `skip_blanks`, `trim_end`, `copy_token`, `find_mode`, `find_action`, `mode_mask`, `parse_line`, then `bindings_parse`), 3 tables (`mode_names`, `action_names`, `mode_actions`). `bindings_compiled_personal` is already a not-ported entry (plan 0's note).
- The generated unit `tmp/bindings/core.c` → `game/game/bindings_default.lua` (step 1.1's note): 1 table, `bindings_default`.
- `src/game/button_image.{c,h}` → `game/game/button_image.lua`: 4 functions, 2 tables (`g_icon_pool`, `g_wide_pool`).
- `src/game/gen.{c,h}` → `game/game/gen.lua`: 10 functions, no table.
- `src/game/grab_arena.{c,h}` → `game/game/grab_arena.lua`: 6 functions, 1 table (`grab_arena_name`).
- `src/game/mouse.{c,h}` → `game/game/mouse.lua`: 9 functions, 1 table (`mouse_class_def`).
- `src/game/mouse_button.{c,h}` → `game/game/mouse_button.lua`: 1 function.
- `src/game/text_image.{c,h}` → `game/game/text_image.lua`: 8 functions, no table.
- `src/game/visual_random.{c,h}` → `game/game/visual_random.lua`: 4 functions, no table.
- `src/game/image_slot.h` and `src/game/targeting.h`: "no-lua" (no function or table of their own), but each holds enum constants later modules need.

Departures from the step's title, all from step 1.1's note and the include graph: `game_data` and `map_tables` were ported by 3.3 (`entity_registry.c` includes `game_data.h`); `row_image` and `draw_functions` moved to 3.5 (they include files of 3.5). "Images and text" here is the text and button image caches; "generation" is `gen` (random walks and the weighted planet-type picker) and `visual_random`. None of the 19 files is in an include cycle: no file of this step is included back by a file it includes.

## Hypotheses on step 3.3 (to check against its note)

Step 3.3's code is committed (`a82f41b`, `2cf7430`, `bb1fda4`, `f6eefb4`, `f07920f`): `game/game/map_tables.lua`, `game/game/game_data.lua`, `game/engine/{entity_registry, entity_geom, entity}.lua`, the reference units `game_map_tables.c`, `game_game_data.c`, `engine_entity_registry.c`, the shared renderer `reference_data.h`, `TABLE_REFERENCE_SOURCES` extended. These are facts: data tables as module fields; zero-filling constructors placed before the tables, rejecting a field the C struct lacks through `map_tables.map_tables_check_fields`; one X-macro list per reference unit driving both the rendering and the pointer-name map; the `entity_geom` / `entity_registry` cycle written with call-time requires only, neither needing the other's value at load, both loading alone in either order; a C ternary between two function pointers written as `if`/`else`; `make checklist STEP=3.3` passing (56/56, 386/386). Chunk 1's message records `lua-test-modules` growing 7.5 ms, 1.5× the 3.1 model, mostly the JIT compiling constructors at load.

Step 3.3's last chunk (costs, the checkpoint's full protocol, the note) had not landed when this PRD was written. Everything below about its note is a hypothesis. If the note says otherwise, the implementer follows the note and records the difference in the step 3.4 note.

- **H-3.3-1 — conventions.** The `### Step 3.3` note records the conventions the committed code shows (every data table a module field; zero-filling constructors before the tables, unknown fields rejected; pointers in data are the pointed table; a `char[]` table is its Lua string; X-macro reference lists; unsigned counters wrapped with `% 4294967296`; function-local statics as module locals) and adds nothing that changes how this step writes structs, pointers, out-parameters, statics or reference units.
- **H-3.3-2 — the cycle rule.** The note extends 2.1's include-cycle convention as 3.3's PRD planned: (a) a partner's constant used inside function bodies is read through the call-time require; (b) a load-time partner value is allowed in one direction per pair, named in both top comments; (c) a pair needing load-time values both ways is cut by the step that meets it; and a cycle file can fail to load before its partner exists only under (b). This step has no cycle, so the rule does not apply here; it is listed only so the 3.4 note can confirm that.
- **H-3.3-3 — own-time gate passed, cost model kept.** 3.3's own times of `lua-guard` and `lua-test-modules` stay under 100 ms, and the gate of later steps is still computed from 3.1's per-KB and per-table costs (the header says so); if the note records a revised per-KB or per-constructor figure, this step reports its growth against both.
- **H-3.3-4 — checkpoint passed, nothing moved out.** 3.3's full rule-4 protocol (against `8dff812` and `d88bb89`) passes, with or without the 11 extra rounds, and nothing moves out of `make test` under design rule 5: the large-table comparison is still in `lua-test-modules`. If the note moved the data tables of `game.game_data` and `game.map_tables` to a separate target, this step's 8 tables still go in `lua-test-modules` (they are small) unless the note's move-out rule names all `game/game/` tables.
- **H-3.3-5 — assignment unchanged.** The note changes no file assignment: step 3.4 stays at 19 files, 56 functions, 8 tables.
- **H-3.3-6 — note place and "before" commit.** The note is `### Step 3.3` in plan-2.md § 6, after `### Step 3.2`; step 3.4's note follows it. This step's "before" commit, for the own-time gate, is the commit that lands the 3.3 note (or the last commit of 3.3 if its rework landed after the note); the implementer reads its hash from `git log`.
- **H-3.3-7 — no rework that renames.** Any rework 3.3 did after its gates (shrinking data files, a moved check) renames no module, field or function this step requires: `game.game_data` still exports `planet_types_table`, `PLANET_TYPES_COUNT`, `PLANET_RING_COLORS_COUNT`, `mouse_archetype`; `engine.entity` still exports `entity_init_instance`, `entity_construct`, `entity_ready`.

## Problem Statement

After step 3.3 the Lua game has the engine (zones, lists, entities, the registry) and all of the game's constant data, but none of the small game modules everything else stands on. The pad bindings cannot be parsed from `bindings.cfg` or read by mode and action, so no pad mode of 3.11 and no input step of 3.14 can be written; the default bindings table the game falls back on does not exist in Lua. The galaxy of 3.6 cannot pick planet types without the weighted picker and the random walks; the background of 3.9 needs a random walk too. Text and buttons are drawn through slot caches the UI of 3.5 to 3.10 calls on every frame. The mouse is an entity every hover test reads; grab arenas hold a structure being placed; on-screen-only randomness has its own stream. Plan 3, when a replay diverges on a binding, a planet type, a cached label or a mouse press, has no Lua function to set beside its C original.

## Solution

Nine new Lua modules in `game/game/`, faithful copies of their C files: `bindings.lua`, `button_image.lua`, `gen.lua`, `grab_arena.lua`, `mouse.lua`, `mouse_button.lua`, `text_image.lua`, `visual_random.lua`, plus the generated `bindings_default.lua`; and two constant modules for the headers without a `.c`, `image_slot.lua` and `targeting.lua` (3.1's rule: "a header without a `.c` whose macros Lua needs gets its own Lua file"). They follow the conventions of 2.1, 3.1, 3.2 and 3.3 (H-3.3-1) and add the few this step needs: positions in a parsed string, a generated module that is a module table, a `sizeof` of a struct field, and one shared reference renderer for entity classes. `bindings_default.lua` is written by the C tool that writes `tmp/bindings/core.c`, from the same `bindings.default.cfg`, through a new `make bindings-lua` target (outside `make test`); its stale-copy check is the data check itself. The modules join the three Phase 3 checks with no new `make test` target: `lua-guard` and `lua-test-modules` pick them up by wildcard; their constants, sizes and 8 tables join the C reference through new units in `tools/table_reference/` and their sources in `TABLE_REFERENCE_SOURCES`. `make checklist STEP=3.4` passes: 19 files, 56 of 56 functions, 8 of 8 tables, every table compared. The own-time gate is measured against the end of 3.3 and recorded in a `### Step 3.4` note.

## User Stories

1. As a pad mode implementer (3.11), I want `bindings_mask(bindings, mode, action)` to return 0 for an action the mode does not have and the mask otherwise, so that a pad press tests the same buttons as in C.
2. As a pad mode implementer, I want `bindings_mode_has_action` to return the C table's 1/0 and to assert, with the C messages, that mode and action are in range, so that a wrong constant fails at its cause.
3. As an input step implementer (3.14), I want `bindings_parse(file_name, text, out, error)` to fill the caller's `out` and `error` tables and return 1 or 0 as the C does, so that the game reads `bindings.cfg` with the C's format and verdicts.
4. As an input step implementer, I want every parse error text to be the C's, byte for byte (`<file>:<line>: no '=' in "<token>"`, `expected mode.action`, `unknown mode`, `unknown action ... in mode ...`, `already given on line`, `empty button`, `bad button ... expected a number from 1 to 12`, `listed twice`, `already bound in mode`, `line longer than 127 characters`), truncated at 255 bytes as `snprintf` into 256 bytes truncates, so that a bad file is reported the same way.
5. As an input step implementer, I want comments (`#`), blank lines, leading and trailing blanks and tabs, and a `\r` before `\n` handled as the C does, so that a file edited on Windows parses the same.
6. As an input step implementer, I want `bindings_read_button_number(text)` to return the button number and the position where the digits stop, 0 with the start position for anything outside 1 to 12, so that the C's digit rule holds.
7. As a debug and menu implementer, I want `bindings_mode_name` and `bindings_action_name` to return the C names, so that any text built from them matches.
8. As an input step implementer, I want `bindings_default` as a table with the C's masks for every mode and action, zeros included, so that the game's fallback bindings are the C ones.
9. As a maintainer, I want `bindings_default.lua` written by the same tool and from the same `bindings.default.cfg` as `tmp/bindings/core.c`, so that the two can never be edited apart.
10. As a maintainer, I want a stale `bindings_default.lua` (after an edit of `bindings.default.cfg`) to fail `make test`, so that the Lua copy cannot drift; and the fix to be one `make bindings-lua`.
11. As a galaxy implementer (3.6), I want `random_walk_new`, `random_walk_next` and `random_walk_draw` with the C's fixed-point computation and the shared random generator, so that every random walk gives the C sequence bit for bit.
12. As a galaxy implementer, I want `weighted_picker_new` to pool `planet_types_table` entries `floor(weight × multiplier)` times in table order, and `weighted_picker_next` to draw, skip the three recent types, refill when no ticket is drawable, delete the drawn ticket and record it in the C's recent slot, so that planet types come out in the C order.
13. As a galaxy implementer, I want the picker's tickets to be the very `planet_types_table` entries, so that identity tests against an entry behave as the C pointers do.
14. As a save implementer (3.12), I want `weighted_picker_walk` to return the picker's walk only when its source is `random_walk_draw`, and `nil` otherwise, so that the save writes the same walk state.
15. As a galaxy implementer, I want both allocations made from the given zone with the C's byte counts (`sizeof(random_walk_t)`, `sizeof(weighted_picker_t)`), so that zone usage matches the C.
16. As a galaxy implementer, I want the ticket pool's overflow assert with the C message, so that a larger table fails loudly in Lua too.
17. As a structure placement implementer, I want `grab_arena_open` to reuse the smallest free arena that fits (flushing it) or take a new one named "grab" from the game zone, `grab_arena_close` to spill it, and `grab_arena_construction_zone` to return it or the game zone, so that placement allocates as the C does.
18. As a structure placement implementer, I want `grab_arena_give_back` to ignore any zone not a grab arena, keep at most eight free arenas and replace the smallest by a larger one, and `grab_arena_give_back_if_placing` to act only in `ENTITY_STATE_PLACING`, so that arena reuse matches the C.
19. As a restart implementer, I want `grab_arena_forget_free` to empty the free list, so that a new game starts with none.
20. As a UI implementer, I want `mouse_new` to build a mouse entity from `mouse_archetype` at position (0, 0), registered like any entity, so that hover and draw order see it as in C.
21. As a UI implementer, I want `mouse_update(input, self)` to do nothing for a `nil` input or self, set the entity position from the input's mouse, and update the left button only while the mouse is up, so that presses and releases match the C.
22. As a UI implementer, I want `mouse_init`, `mouse_shutdown`, `mouse_ready` and `mouse_left_button` (`nil` while down) with the C flag semantics and the `entity_init()` assert, so that boot order ports unchanged.
23. As a UI implementer, I want `mouse_class()` to return the class table by identity, so that an identity test against the mouse class ports unchanged.
24. As a UI implementer, I want `mouse_button_update(button, down)` to compute `held`, `pressed` and `released` as 1/0 from the previous held state, only once `has_held` is set, so that the first frame never reports a release.
25. As a UI implementer, I want `text_image_print_outlined` to draw exactly what `p8_print_outlined` draws, through a text slot keyed by text and colour when the text is cacheable and a slot is free, so that cached labels look the same and cost the same draw calls as the C.
26. As a UI implementer, I want `text_image_cacheable` and the least-recently-used slot choice, never evicting a slot copied this frame, with the C's frame and stamp counters, so that the same slots are rebuilt on the same frames as in C.
27. As a UI implementer, I want `button_image_draw` to refuse sizes over 112 × 11 or a full pool, choose the icon pool for widths up to 16 and the wide pool otherwise, build on first sight and draw at (x, y), so that buttons are drawn and rebuilt as in C.
28. As a UI implementer, I want two button keys equal exactly when their four 32-bit words are equal, whatever sign their Lua numbers carry, so that a key built with `bit` operations matches the same key built otherwise.
29. As a UI implementer, I want `text_image_frame_begin` and `button_image_frame_begin` to start a frame as in C, so that the per-frame eviction rule holds.
30. As an effect implementer, I want `visual_random_draw`, `visual_random_draw_one`, `visual_random_state` and `visual_random_set_state` on their own stream seeded with `0x9E3779B9`, so that on-screen-only randomness never touches the game's stream.
31. As a module implementer, I want the `IMAGE_SLOT_*` constants as fields of `game.image_slot`, computed as the C enum, with the C's check that they fill the runtime's slot count as a load-time assert, so that every image cache uses the C slot numbers.
32. As a module implementer (3.8), I want the `TARGETING_TARGET_*` constants as fields of `game.targeting`, so that probes, slipways and wormholes use the C names.
33. As a module implementer, I want `BINDINGS_MODE_*`, `BINDINGS_ACTION_*`, `BUTTON_IMAGE_*`, `WEIGHTED_PICKER_TICKETS_CAP` and `GRAB_ARENAS_FREE_MAX` as module fields with the C values, compared with the C compiler, so that later modules use the C names and values.
34. As a maintainer, I want `lua-test-modules` to compare the 8 new tables (`mode_names`, `action_names`, `mode_actions`, `bindings_default`, `g_icon_pool`, `g_wide_pool`, `grab_arena_name`, `mouse_class_def`), every constant and every new `SIZEOF` entry with the C, so that a typo fails `make test`.
35. As a maintainer, I want `make checklist STEP=3.4` to pass with every table compared, so that the step is provably complete.
36. As a later step implementer (3.5 to 3.14), I want one shared C renderer for an `entity_class_t`, used by the registry's unit and the mouse's, so that the dozens of class tables still to come are each one line in their unit.
37. As a plan 3 debugger, I want each function in C order with the C name and computation, and each table under its C name, so that I can bisect a divergence function by function.
38. As a maintainer, I want the own times of `lua-guard` and `lua-test-modules` measured paired against the end of 3.3, under 100 ms and within 2× of 3.1's model, so that the step provably keeps `make test` fast.
39. As a maintainer, I want the step's conventions, counts and costs in a `### Step 3.4` note, so that step 3.5 builds on recorded decisions.
40. As a maintainer, I want no new `make test` target and no change to the three checks' scripts, so that the Phase 3 contract holds.

## Implementation Decisions

### Modules and requires

- Lua files: `game/game/{bindings, bindings_default, button_image, gen, grab_arena, image_slot, mouse, mouse_button, targeting, text_image, visual_random}.lua`. Each `.h` without functions or tables of its own contributes its constants and struct shapes to its pair's module, header first, then `.c` (2.1). `image_slot.lua` and `targeting.lua` stand alone (3.1's rule for a header without a `.c`, as `engine.zone_sizes`).
- Requires follow the C includes; a require no line uses is not written; an include for types alone gives no require. No module of this step is in an include cycle, so every require is at load.
  - `mouse_button`, `targeting`: nothing.
  - `visual_random`: `p8.fixed`.
  - `image_slot`: `game.game_data` (`PLANET_RING_COLORS_COUNT`), `p8.image` (`P8_IMAGE_SLOT_COUNT`, for the load-time assert).
  - `bindings`: `p8.input` (`P8_PAD_BUTTON_COUNT`), `bit` (as `bit_module`: the C has a local named `bit`).
  - `bindings_default`: nothing (generated literals).
  - `gen`: `game.game_data`, `engine.allocator`, `p8.fixed`, `p8.cint` (`tickets_count % 3` as `cint.mod`, 2.1's rule).
  - `grab_arena`: `engine.allocator`, `engine.entity_class` (`ENTITY_STATE_PLACING`), `engine.game_zone`.
  - `text_image`: `game.image_slot`, `p8.image`, `p8.gfx` (`p8_print_outlined`, font constants).
  - `button_image`: `game.image_slot`, `p8.image`.
  - `mouse`: `game.game_data`, `p8.input`, `engine.entity`, `engine.entity_class`, `game.mouse_button`, `engine.vec`, `engine.dispatch_context`, `engine.game_zone`, `engine.allocator`, `p8.fixed`.
- Build order (the chunks below): leaves without drawing, bindings, image caches, mouse.

### Conventions added by this step (recorded in the step 3.4 note)

- **Positions in a parsed string.** A `const char *` into a text being parsed is a 1-based byte position in the Lua string; a C range `[start, end)` is the pair of positions with `end` exclusive (the position the C end pointer points at); `*p` is `string.byte(s, p)`, and a position past `#s` reads 0, the C's `'\0'`. `memchr(start, c, end - start)` is `string.find(s, c, start, true)` kept only when below `end`; `strchr` likewise without a bound; `copy_token(destination, start, end)` returns `s:sub(start, end - 1)` (the destination buffer is the returned string). `bindings_read_button_number(text, end)` takes the string and returns `number, end_position` (2.1's out-parameter rule); `parse_line` tests `end_position <= #token` for the C's `*digits_end != '\0'`.
- **Error records.** `bindings_error_t` is `{ line, text }`, `text` a Lua string. `fail` builds the prefix with `string.format("%s:%d: ", ...)`, keeps at most `SIZEOF["bindings_error_t.text"] - 1` bytes of prefix plus message (what `snprintf` and `vsnprintf` leave in 256 bytes), sets `line`, returns 0. A prefix of 256 bytes or more returns 0 with the text cut to 255 bytes, as the C. `memset(error, 0, ...)` sets `line = 0`, `text = ""`.
- **A `sizeof` of a field** is a `SIZEOF` entry keyed `"<struct type>.<field>"` (`SIZEOF["bindings_error_t.text"]`), compared with `sizeof(((T *)0)->field)`.
- **The parameter named `error`.** `fail`, `parse_line` and `bindings_parse` keep the C parameter name `error`, which shadows Lua's `error` inside them; none of them raises (asserts use `assert`). The step 3.4 note says so, so a later edit adding a raise there uses `fatal.p8_fatal`.
- **Masks as tables.** `bindings_t` is `{ masks }`, `masks` a 0-indexed `[mode][action]` array with every entry present (zeros included). `bindings_parse` fills the caller's table (`out.masks` new zeros, 2.1's init-on-`T *`); `given_on_line` is a local 0-indexed zero array.
- **The generated defaults are a module table.** `bindings_default.lua` returns `M` with `M.bindings_default = { masks = { [0] = { [0] = ..., ... }, ... } }`, every mode and action written, one row per mode, decimal integers. Reason: a module returning a bare table must hold no string key for `lua-test-modules`, and `bindings_t` has the key `masks`. Headed `-- Generated by make bindings-lua (src/cmd/bindings_generate.c) from bindings.default.cfg: edit the configuration.`; no comments beyond the header.
- **Who writes it.** `src/cmd/bindings_generate.c` gains a `--lua` kind beside `--personal` and `--default` (same argument count, same parse, same atomic temporary-and-rename write), writing the module above from `bindings_mask` over the parsed table. A new public target `make bindings-lua` (one-line `## ` help, at most 20 words) runs it on `bindings.default.cfg` into `game/game/bindings_default.lua`, as `make sin-table` does for its Lua copy. It is outside `make test`, `make e2e` and `make test-all`.
- **The stale-copy check is the data check.** 2.1 asks for a `make test` check failing on a stale generated copy, and the Phase 3 header forbids a new `make test` target. `lua-test-modules` compares `bindings_default` with the reference rendered from `tmp/bindings/core.c`, which make regenerates from `bindings.default.cfg`: an edited configuration without `make bindings-lua` fails `make test` on that block. No other check is added.
- **Function-pointer identity.** `state->random != random_walk_draw` is `state.random ~= M.random_walk_draw`: the module field is the one function value every caller passes.
- **Names compared by address.** `zone->bump->name != grab_arena_name` compares Lua strings by value. Equal to the C's address test because no other zone in `src/` (outside `src/cmd/`, not ported) is named "grab": the others are `frame`, `game`, `load`, `ui ...`, `slot ...`, `particles ...`. The module's top comment and the note say so; a later step adding a zone named "grab" must change this.
- **Zone blocks with inline arrays.** `allocator_alloc` returns an empty table (3.1); the module creates each inline array the C first writes in it (`recent`, `recent_set`, `tickets`, 0-indexed) when it writes it. `weighted_picker_new` creates `tickets` before `weighted_picker_fill`; slots beyond `tickets_count` hold whatever was left, never read, as in C.
- **Struct copies of keys.** `slot->key = *key` copies the key into a new table (button key: `{ words }` with its four words; text key: `{ col, length, text }`). `text_image_key_t key = {0}` is `{ col = 0, length = 0, text = "" }`; `text` is a Lua string of `length` bytes (`memcpy` of `length` bytes is `str:sub(1, length)`), so `same_key`'s `memcmp` over `length` bytes is string equality after the length test.
- **32-bit words.** `same_key` for buttons compares each of the four words through `bit.tobit` on both sides (2.1: a `bit` result above 2^31−1 is normalised where compared), so `memcmp` equality holds whether a caller's word is signed or unsigned.
- **Slot statics.** `g_slots` of each cache is a 0-indexed array of zero slots (button: `{ key = { words = { [0] = 0, 0, 0, 0 } }, used = 0, stamp = 0, frame = 0 }`; text: `{ key = { col = 0, length = 0, text = "" }, stamp = 0, frame = 0 }`); `g_stamp = 0`, `g_frame = 1`. These mutable statics are not checklist tables and are not compared. The `unsigned long` counters are plain Lua numbers incremented by 1: exact far beyond any session (2^53), and the C's 64-bit wrap is unreachable; the note records this, unlike 3.3's 32-bit counters.
- **Pools by value.** `g_icon_pool` and `g_wide_pool` are module fields `{ first, count }` (compared); `slot_for(pool, ...)` reads them and never writes them (2.1: never mutate a table received by value).
- **The image draw callback.** `p8_image_build(gfx, mem, slot, w, h, draw, context)` receives a Lua function and a context table as `game/p8/image.lua` defines; `text_image_build_draw` takes the key table as its context.
- **The mouse object.** `mouse_t` is one table holding the entity header and `archetype` (3.1's first-member casts); `mouse_class_def` is an `entity_class_t` literal with `states` a 0-indexed table holding only `[ENTITY_STATE_NORMAL] = mouse_update_method` and `tags`, `lineage`, `skip_draw_off_screen` at 0, every other field `nil`, as 3.3's placeholder class; it is a module field (compared) defined after `mouse_update_method` (forward-declared `local` as 2.1 says if needed). `g_left_button` is `{ has_held = 0, held = 0, pressed = 0, released = 0 }`; `memset` sets each field to 0 in place, so a reader holding the table sees the reset. No shared class constructor is added: this step has one class; the first step with several decides.
- **Truth values.** `down && !was_held` is `(down ~= 0 and was_held == 0) and 1 or 0`; `(mouse.buttons & P8_MOUSE_LEFT) != 0` is `(bit.band(...) ~= 0) and 1 or 0`; `mode_actions` entries stay 0/1 integers.
- **Compile-time checks.** `button_image_slots_follow_each_other` and `image_slot_count_matches_the_runtime` become load-time `assert`s with their names as messages.
- **`.c`-local enums.** `BUTTON_IMAGE_SLOT_COUNT`, `OUTLINE`, `TEXT_IMAGE_HEIGHT`, `TEXT_CAP`, `BINDINGS_MAX_LINE_LENGTH`, `VISUAL_RANDOM_BOOT_SEED` are module locals, not compared (3.1's rule for a `.c`-local macro, applied to a `.c`-local enum).
- **Random state.** `visual_state` is a number in 0 … 2^32−1 (1.2's state convention); each draw is `result, visual_state = fixed.p8_fixed_rnd_from_state(visual_state, x)`.
- **Not ported, named in top comments.** `bindings.h` declares `bindings_compiled` (defined only in the replay tool's unit `tmp/bindings/default.c`, which the checklist does not scan) and `bindings_compiled_personal` (not ported since plan 0); `targeting.h` declares functions defined in `probe.c` and ported with it (3.8). Each module's top comment lists the struct shapes it defines, as `entity_class.lua` does.

### The data check for this step

- **Units.** New units in `tools/table_reference/`, each including its `.c` when it renders statics, their sources joining `TABLE_REFERENCE_SOURCES`:
  - `game_bindings.c` (includes `game/bindings.c`): the nine `BINDINGS_MODE_*` and 35 `BINDINGS_ACTION_*` constants with both counts, `SIZEOF["bindings_error_t.text"]`; `mode_names` and `action_names` (one string line per entry), `mode_actions` (every entry, zeros included), all under module `game.bindings`; and `bindings_default` under module `game.bindings_default`, read through its `extern` (the generated unit `tmp/bindings/core.c` stays linked from `ENGINE_SRCS`; it is not added to `TABLE_REFERENCE_SOURCES`, whose names derive from `src/` paths).
  - `game_button_image.c` (includes `game/button_image.c`): the three `BUTTON_IMAGE_*` constants, `g_icon_pool`, `g_wide_pool`.
  - `game_gen.c` (header): `WEIGHTED_PICKER_TICKETS_CAP`, `SIZEOF.random_walk_t`, `SIZEOF.weighted_picker_t`.
  - `game_grab_arena.c` (includes `game/grab_arena.c`): `GRAB_ARENAS_FREE_MAX`, `grab_arena_name` (one string line).
  - `game_image_slot.c` (header): every `IMAGE_SLOT_*` constant.
  - `game_mouse.c` (includes `game/mouse.c`): `mouse_class_def`, `SIZEOF.mouse_t`.
  - `game_targeting.c` (header): the five `TARGETING_TARGET_*` constants.
  - `text_image`, `mouse_button`, `visual_random` have no compared constant or table and get no unit.
- **Shared class renderer.** `render_class` moves from `engine_entity_registry.c` to a header beside `reference_data.h` (say `reference_class.h`, `static inline` as 3.3 made its helpers), taking the module name; the registry's unit and the mouse's call it. The registry's reference lines do not change.
- **No change** to `tests/lua/test_modules.lua`, `tools/table_reference.c`, `tools/lua_guard.lua`, `tools/checklist.lua`, `tools/checklist.txt` or their C tests. The Makefile changes only in `TABLE_REFERENCE_SOURCES`, the `bindings-lua` target and the generator's new kind.

### Cost and timing

- **Projection** (3.1's model): about 25 – 35 KB of Lua (bindings about 10 KB, the generated defaults about 3 KB at one row per mode, the rest small) adds about 3 ms to `lua-guard` and about 1.5 ms of load to `lua-test-modules`; 8 tables and about 700 reference lines (`mode_actions` and `bindings_default` 315 each) add under 0.5 ms of comparison. Both checks stay far under 100 ms.
- **Own-time gate** (the Phase 3 header, steps 3.3 to 3.14): at the end, the own wall times of `lua-guard` and `lua-test-modules` are measured paired against the step's "before" commit (H-3.3-6). The step passes when each stays under 100 ms and its growth within 2× of what 3.1's cost per KB and per table predicts (and, if 3.3's note revised the model, also reported against that, H-3.3-3). The KB, table and line counts, the predicted and measured growth go in the note.
- **Full protocol:** 3.4 is not a checkpoint; the full rule-4 protocol runs only if the own-time gate fails, against `8dff812` and the "before" commit, recorded as step 2.1's note does. On a failure, what to rework is chosen from the recorded own times (the Phase 3 header); no `make test` target is added.

### Docs

- `docs/love-port/plan-2.md` § 6 gains `### Step 3.4` after `### Step 3.3` (H-3.3-6): the files and counts (`make checklist STEP=3.4`), the conventions above (positions in parsed strings, error records, field `sizeof`, the `error` parameter, the generated module table and its owner, the data check as stale check, name-by-value for "grab", 64-bit counters), the new units and shared class renderer, the own-time gate figures, and every place where 3.3's note differed from H-3.3-k.

### Questions

- No Agent tool was available in this run, so no `guideline` agent was spawned. Each open point was settled from the plan, `design.md`, `CLAUDE.md`, the 2.1 to 3.2 notes, 3.3's PRD and committed code, and is recorded above: `image_slot.lua` and `targeting.lua` as constant modules; the generated defaults as a module table written by `bindings_generate --lua` through `make bindings-lua`, the data check standing as the stale-copy check; string positions for the parser; error text truncation; the field-`sizeof` key; string equality for the grab arena name, with its proof; the shared class renderer; no behaviour test.

## Implementation Chunks

1. **Leaves without drawing.** `mouse_button.lua`, `visual_random.lua`, `gen.lua` (with `SIZEOF`), `grab_arena.lua`, `targeting.lua`; units `game_gen.c`, `game_grab_arena.c`, `game_targeting.c` and their sources in `TABLE_REFERENCE_SOURCES`. Green: `lua-guard`, `lua-test-modules` (`grab_arena_name`, the constants and sizes equal), the checklist rows of these files.
2. **Bindings.** `bindings.lua` (14 functions, the three tables, the constants, `SIZEOF`); the `--lua` kind of `bindings_generate`, `make bindings-lua`, the generated `bindings_default.lua`; unit `game_bindings.c` rendering both modules, `src/game/bindings.c` in `TABLE_REFERENCE_SOURCES`. Green: `lua-test-modules` for the four tables; a throwaway check that the Lua `bindings_parse` of `bindings.default.cfg` equals `bindings_default`, that each `tests/e2e/*.cfg` parses to the same masks as the C, and that the error cases of `tests/test_bindings.c` give the C texts; editing `bindings.default.cfg` without `make bindings-lua` turns `lua-test-modules` red (then restored).
3. **Image caches.** `image_slot.lua`, `text_image.lua`, `button_image.lua`; units `game_image_slot.c`, `game_button_image.c`. Green: `lua-test-modules` (`IMAGE_SLOT_*`, `BUTTON_IMAGE_*`, the two pools), the load-time asserts, the checklist rows; a throwaway script driving each cache over a few frames under plain LuaJIT, with `p8.image`'s build and draw stubbed, to check slot choice and eviction against the C rule.
4. **Mouse.** `mouse.lua`; `reference_class.h` with the registry's unit moved onto it (its lines unchanged); unit `game_mouse.c`. `make checklist STEP=3.4` passes from here (19 files, 56/56, 8/8, all compared).
5. **Cost and record.** The own-time gate (both checks paired against the "before" commit, under 100 ms, growth within 2× of 3.1's model), the full protocol only on a failure, rework only on a failure, the `### Step 3.4` note.

## Testing Decisions

- A good test checks external behaviour only: a check's verdict on the Lua files, and the equality of Lua constants, sizes and tables with the C ones. No test looks at how a Lua function is written.
- Tested: the eleven modules through `make checklist STEP=3.4` (every file, function and table present, every table compared), `lua-guard` (compile, no undeclared global, no `love.` use), and `lua-test-modules` (load under plain LuaJIT, every registered constant, `SIZEOF` entry and the 8 tables equal to C). A TDD cycle goes red on a checklist row, a load failure, or a missing or differing reference line, and green when the faithful copy is in place. `bindings_default`'s block is also the stale-copy test of the generated file.
- No committed behaviour test, as 3.1 to 3.3 decided: the e2e replays of plan 3 are the source of truth (`CLAUDE.md`), no C unit test is ported (design.md), and the timing margin is thin (1.2's and 3.2's notes). The implementer checks behaviour with throwaway scripts: the parser against the C on the default file, the e2e `.cfg` files and the error cases of `tests/test_bindings.c`; the picker's and walks' sequences against the C for a fixed seed (as `tests/test_gen.c` drives them); the caches' slot choice; `mouse_button_update` over a press and release.
- No change to `tests/lua/test_modules.lua`, `tools/lua_guard.lua`, `tools/table_reference.c` or their C tests. No C test for the generator's `--lua` kind: its output is checked by the data check.
- Prior art: `tools/table_reference/engine_entity_registry.c` (a class table, a unit including its `.c`), `engine_zone_sizes.c` (constants and `SIZEOF` from a header), `p8_gfx_software.c`; `tools/gen_quarter_wave.c` with `make sin-table` (a C generator with a `--lua` output); `game/engine/zone_sizes.lua` (a constants module for a header without `.c`); `game/engine/allocator.lua` (arenas, the `bump` field, `SIZEOF`); `game/engine/entity.lua` and `entity_registry.lua` (classes, statics); `game/p8/image.lua` (the build callback and slot sizes).

## Out of Scope

- `row_image` and `draw_functions` (3.5), `game_data` and `map_tables` (done in 3.3), engine `bootstrap` (3.9).
- The readers of these modules: the input step (3.14), pad modes (3.11), galaxy (3.6), background (3.9), save (3.12), the UI that draws through the caches (3.5 to 3.10), `targeting`'s functions in `probe.c` (3.8).
- Reading `bindings.cfg` from the fake's disk and choosing between the parsed and default bindings (the input step and `main.lua`).
- The replay tool's `bindings_compiled` and anything in `src/cmd/` beyond the generator's new output kind.
- Running any e2e replay or `make love-e2e` (plan 3); behaviour tests; redesign for Lua (plan 5).
- Changes to the three checks' scripts, a skip mechanism in `lua-test-modules`, or a new `make test` target.
- Audio (out of scope for the whole port).

## Further Notes

- Struct shapes for later steps: `bindings_t` `{ masks }` (0-indexed `[mode][action]`); `bindings_error_t` `{ line, text }`; `random_walk_t` `{ step, bias }`; `weighted_picker_t` `{ tickets, tickets_count, recent, recent_set, multiplier, random, random_ctx }`; `mouse_t` the header plus `archetype`; `mouse_button_t` `{ has_held, held, pressed, released }`; `button_image_key_t` `{ words }` (0-indexed, four 32-bit words); `targeting_target_t` `{ kind, id }`, `targeting_t` `{ archetype, target, cost, has_cost }`, `targeting_track_target_t` the header plus `targeted_by` (shapes only; their functions come with `probe.c`).
- The caller of `bindings_read_button_number` outside the game (`src/devtools/input_json.c`) is not ported; inside the game only `parse_line` calls it, on a whole token.
- This step is small against the plan's budget (8 of 529 tables); its main risk is the parser's exact error texts, which the e2e replays do not exercise (they use valid files), hence the throwaway comparison with `tests/test_bindings.c`.
