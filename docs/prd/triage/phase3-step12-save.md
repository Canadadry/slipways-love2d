---
title: "Plan 2, step 3.12: the save in Lua (save_state, save_load, save_storage)"
description: "The Lua game cannot yet capture, encode, decode, store or reload a running game in the C's byte-identical save format."
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

> 3.12. `game/game/` save.

Step 3.11 is done (`5b3aa02`). 3.12 is not a checkpoint: it runs the own-time gate, not the full rule-4 protocol.

## Problem Statement

The Lua game, written as a faithful copy of the C game, has every module the save depends on (planets and the planet family, slipways, probes, trade routes, research projects, the economy, structure icons, the background, the type picker and walks), but no save. Without it, step 3.14's cart cannot write a record at the end of an action, cannot offer "resume" on the title, and cannot rebuild a saved game; and plan 3 cannot replay `pad_resume_at_start.json`, whose `initial_save` is a C record the fake disk hands the game as `save.bin`. Plan 2's rules require the save file to use the same format as the C (`docs/save-format.md`): a record written by one game must be read by the other, byte for byte.

The checklist assigns five C files to 3.12 (`luajit tools/checklist.lua --listing`): `save_state.c`/`.h`, `save_load.c`/`.h` and `save_storage.h`: 78 functions (58 in `save_state.c`, 20 in `save_load.c`; both files have their own static `vec_of` and `has`) and 3 tables (`magic`, `crc32_table`, and the `node_constructors` array that the checklist's tokenizer names `SAVE_NODE_KIND_COUNT`). All three headers are `no-lua` in the listing (declarations and constants only).

## Solution

Three new Lua modules under `game/game/`, each a faithful copy of its C file under the 2.1, 3.1 and later conventions:

- `game.save_state`: the record codec and the capture. Encode, decode with the full validation order of `docs/save-format.md`, the CRC-32, and the capture that reads the live game into a save state table, changing nothing.
- `game.save_load`: the world rebuild that `cart_resume_game` (3.14) calls in four stages: difficulty, background seed, world, globals.
- `game.save_storage`: the one storage backend of the Lua game, over `love.filesystem` (`read`, `write`, the design's list), with the three functions and contracts of `save_storage.h`. The C has no disk backend in this repository (only the devtools memory backend, not ported); the Lua game needs one, and the fake disk of `design.md` names its file: `save.bin`.

Verified by the checklist (`make checklist STEP=3.12`), the load check, the undeclared-global check and the data check in `make test`, a throwaway C-vs-Lua round trip over every real record the C game writes in the e2e and worst-case replays, and the own-time gate.

## User Stories

1. As the plan 2 implementer, I want every function of `save_state.c` in `game.save_state` with its C name and in C order, so that the checklist reports the file complete.
2. As the plan 2 implementer, I want every function of `save_load.c` in `game.save_load` with its C name and in C order, so that the checklist reports the file complete.
3. As the plan 2 implementer, I want the three tables of the step ported and compared with the C by `lua-test-modules`, so that `make checklist STEP=3.12` passes with 3/3 tables.
4. As the plan 3 debugger, I want each Lua save function beside its C original with the same computations, so that a diverging resume replay can be traced line by line.
5. As a player, I want the Lua game to write, after each action, the same record bytes the C game would write for the same world, so that a save moves between the two games.
6. As a player, I want the Lua game to read a C record and resume it exactly, so that "resume" on the title works with a save from either game.
7. As a player, I want a corrupted, truncated, oversized or foreign save file to be refused silently (no "resume", no crash), so that a bad file never breaks the title.
8. As the plan 3 debugger, I want decode to reject records in the exact order and on the exact rules of `docs/save-format.md`, so that the Lua and C games agree on every byte string, valid or not.
9. As the plan 3 debugger, I want decode never to raise a Lua error on any byte string, as the C never asserts on bytes, so that a fuzzed file cannot stop the game.
10. As the plan 3 debugger, I want encode to raise the C's assert messages (the 8 KiB limit, needs or routes without their flag, one-byte indices) through Lua `assert`, so that a broken state fails as loudly as in C.
11. As the plan 3 debugger, I want the CRC-32 to give `0xCBF43926` for `"123456789"` and match the C on every record, so that the trailer is right.
12. As the plan 3 debugger, I want 32-bit values written and read bit-exactly: `u32` (the RNG state, the background seed, the CRC) as unsigned 0 … 2^32−1, `i32` (every fixed-point value) as the signed raw value, so that no sign or overflow differs from the C.
13. As the plan 3 debugger, I want capture to read the same entities in the same order (nodes then slipways by tag, skipping dead, `PLACING` and mouse-point slipways; the settled probe only), so that node and slipway indices match the C.
14. As the plan 3 debugger, I want capture to raise the C's assert messages when a route, slipway or list names an uncaptured entity, so that a broken world is caught at the save.
15. As the plan 3 debugger, I want capture to set the synthesizer-spent flag by table identity of `synthesizable_resources` against the archetype's, as the C compares pointers, so that the flag matches.
16. As the plan 3 debugger, I want the description type found by table identity of the description string's owner entry, current type first, as the C does, so that a terraformed planet keeps its old type's description after a resume.
17. As the plan 3 debugger, I want the load to build each node through its kind's constructor, positions as keys for planets and protostars only, so that constructor side effects (bursts, RNG draws, registrations) happen in the C order.
18. As the plan 3 debugger, I want the load to remove the protostar bursts, register the probe before the slipways, attach slipways node-major in saved order, then undo the attach side effects, so that layers, lists, fold anchors, shakes and synthesizer state match the C.
19. As the plan 3 debugger, I want the outgoing routes rebuilt from the saved lists, never recomputed, so that trade partners chosen long ago stay the same.
20. As the plan 3 debugger, I want the type picker, its walk and the vanish walk restored after `galaxy_install_rolls`, so that future rolls draw as in the C.
21. As the plan 3 debugger, I want the invented techs restored one at a time with a structure icon refresh before and after each, never through `economy_invent`, so that icons take the cursor slot as in the C and no research points are spent.
22. As the plan 3 debugger, I want the RNG state restored last in `save_load_globals`, so that constructors before it may draw and the resumed game still continues the saved stream.
23. As the plan 3 debugger, I want a capture taken right after a load to give back the loaded record byte for byte, as in the C, so that resuming writes nothing new.
24. As the plan 3 developer, I want the storage backend to read `save.bin` through `love.filesystem.read`, so that the fake's `initial_save` seeding reaches the title.
25. As the plan 3 developer, I want a missing `save.bin`, an empty one, or one larger than 8192 bytes to read as "no record" (0), so that the C contract "0 when there is none or it cannot be read" holds.
26. As the plan 3 developer, I want erase to leave no record using only the design's three filesystem functions, so that the fake and real LÖVE behave the same.
27. As the plan 3 developer, I want write to replace the whole file and to assert `1 <= count <= 8192` with the C message, so that a shorter record leaves no stale tail.
28. As the plan 3 developer, I want the storage module never to touch `love` at load time, so that `lua-test-modules` (plain LuaJIT, no `love`) still loads it.
29. As the plan 2 implementer, I want the 3.10 latent planet-field finding settled for the save, so that 3.15 knows whether it alone still carries it.
30. As the plan 2 implementer, I want the save state table shaped as `save_state_t` with the C field names, arrays from 0 and slices into shared pools, so that a C struct dump and a Lua one compare line for line.
31. As the plan 2 implementer, I want the out-parameter conventions held (`structure_icon_id_counter`, `economy_tiers_compute_production_amount` return their out values), so that no value is silently lost.
32. As the maintainer, I want `make test` and `make e2e` not to get slower beyond the gate, so that the fast loop stays fast.
33. As the maintainer, I want the step's own times of `lua-guard` and `lua-test-modules` measured and recorded, so that the end-of-plan projection stays honest.
34. As the maintainer, I want the step note to record the corpus size, what the round trip compared and what mutations it caught, so that the evidence of byte identity survives the throwaway programs.
35. As the maintainer, I want no permanent behaviour test added (the replays of plan 3 cover the save), so that the plan's rule "no C unit test is ported" holds.

## Implementation Decisions

### Modules and shapes

- Three modules: `game.save_state` (from `save_state.h` + `save_state.c`), `game.save_load` (from `save_load.h` + `save_load.c`), `game.save_storage` (Lua only, for `save_storage.h`). None is in an include cycle (only the cart, 3.14, and devtools include them), so each requires its dependencies at load time and all three load at once; `LUA_DEFERRED_MODULES` stays empty.
- `save_state.h`'s enum constants (`SAVE_STATE_*`, the nine `SAVE_NODE_*` kinds and `SAVE_NODE_KIND_COUNT`, the eleven flag bits and `SAVE_NODE_KNOWN_FLAGS`) are module fields of `game.save_state` with the C names, compared by `lua-test-modules`. The `.c`-local enum (`HEADER_BYTES`, `TRAILER_BYTES`, `LENGTH_OFFSET`) is module locals, not compared.
- `magic` and `crc32_table` are module fields bound to locals (as `pad_jump`'s `directions_by_priority`), 0-indexed; `crc32_table` holds the 256 values as unsigned literals (0 … 2^32−1).
- `node_constructors` is a module field of `game.save_load`, a 0-indexed array of the nine constructor functions indexed by kind. The checklist's tokenizer names this table `SAVE_NODE_KIND_COUNT` (its declarator is a function-pointer array); a `lua src/game/save_load.c SAVE_NODE_KIND_COUNT game/game/save_load.lua node_constructors` exception line in `tools/checklist.txt` maps it to its C name.
- `sizeof` uses go through `M.SIZEOF` as the conventions require: `SIZEOF.magic` (4, for `sizeof magic`) and `SIZEOF["save_node_t.planet_id"]` (8, for `sizeof node->planet_id`), compared with the C.
- Two reference units, one per `.c` (both define statics named `vec_of` and `has`, so one unit cannot include both): a `save_state` unit printing the header constants, the two sizes, `magic` and `crc32_table`; a `save_load` unit printing `node_constructors` as nine function entries. Both sources join `TABLE_REFERENCE_SOURCES`.

### Data representation

- A byte buffer (`uint8_t bytes[]`) is a 0-indexed Lua array of integers 0–255, filled in place by the callee (`save_state_encode(state, buffer)` returns the count and fills `buffer`; `save_storage_read(bytes, cap)` likewise). Only `game.save_storage` converts between that array and a Lua string, with `string.byte`/`string.char` over the C length.
- `cursor_t` is a table `{ position, end_, failed }` (`end` takes the trailing underscore).
- `save_state_t` is a table with the C field names. `memset(out, 0, sizeof *out)` (capture and decode) sets every scalar to 0, the small fixed arrays (`recent`, `recent_set`, `invented`, `tickets`) to 0-indexed zero arrays of their C length, and the large arrays (`nodes`, `slipways`, `routes`, `route_indices`, `node_slipways`) to new empty tables; an element of a large array is created with every field at its C zero value (a local `save_node_zero`, `save_route_zero`, `save_slipway_zero`, Lua only, each listing every field) at the moment the C writes into it. Nothing reads an element the C has not written in a valid flow, and decode's validators only read indices below the counts it has filled. This avoids building 255 nodes and 2730 routes per save while keeping every read field defined.
- `planet_id` is a Lua string (a `char[]` is its Lua string): capture copies the planet's id string, encode writes `#planet_id` bytes bounded by `SIZEOF["save_node_t.planet_id"]` as the C loop is, decode builds the string from the validated bytes.
- `u8`/`u16` are integers; `read_u32` returns `low + high * 65536` (unsigned, never through a signed `bit` result); `read_i32` is `bit.tobit` of it; `write_*` take bytes with `bit.band(bit.rshift(v, n), 0xFF)`, which handles a signed `i32` and an unsigned `u32` alike. `save_state_crc32` computes with `bit` and returns the unsigned value (a negative `bit` result brought back by adding 2^32), so it compares equal to `read_u32` and prints as the C.
- C predicates return 1/0; `node->flags & FLAG` tests are `bit.band(...) ~= 0`. `save_state_decode` returns 1 or 0.
- Out-parameters per 2.1: `structure_icon_id_counter(counter)` returns `ok, counter`; `economy_tiers_compute_production_amount(planet, amount)` returns `ret, amount`, and `load_production` assigns the second return to `planet.production.amount`.
- `save_load_difficulty(state, out)` fills the caller's table: it resets it to `ui_button.button_zero()`'s fields (every key of a fresh zero button copied into `out`, keys absent from it cleared), then sets the five fields the C sets.
- Statics of `save_load_world` (`node_ids`, `slipway_ids`) are module-level 0-indexed tables; ids stored into them are new id tables from `entity_id_link`, as 3.2 requires.
- Name comparisons (`strcmp(...) == 0` on difficulty and technology names) are string equality; pointer comparisons of descriptions, archetype sequences and class tables are table/string identity, which 3.3's pointer rule (the very table pointed to) makes equivalent. Descriptions are Lua strings: two `planet_types_table` entries with equal description text would be indistinguishable where the C distinguishes addresses; the corpus check must confirm `description_type` matches the C on every record (if two entries share a text, the first-match loop gives the same index only when the current type is checked first, as the C does — kept in that order).

### Storage backend (Lua only)

- `game.save_storage` exports `save_storage_write(bytes, count)`, `save_storage_read(bytes, cap)`, `save_storage_erase()` with the contracts and assert messages of `save_storage.h` and the memory backend (`to clear the save, call save_storage_erase`; `a smaller buffer would lose the end of a record`). The file is `save.bin`, the name the fake disk seeds from `initial_save` (`design.md`), as a module local.
- Write: `love.filesystem.write("save.bin", string)`; returns 1 when it returns `true`, 0 otherwise (the previous record is then whatever the filesystem kept; the C contract allows a failed write).
- Read: `love.filesystem.read("save.bin")`; `nil` (missing) → 0; a content of length 0 → 0 (erased); length above `SAVE_STATE_MAX_BYTES` → 0 ("cannot be read" as a record; decode would refuse it anyway); otherwise the bytes are copied into the caller's array from index 0 and the length returned.
- Erase: the design's list has no `remove`, so erase writes an empty `save.bin`; returns 1 when the write returns `true`, 0 otherwise. An empty file is "no record" for read. In real LÖVE an empty file in the save directory behaves the same; it also shadows any `save.bin` in the game directory, which is wanted.
- `love` is only referenced inside the three functions (global read allowed by `lua-guard`; `love.filesystem` is not glue-only), so the module loads under plain LuaJIT.

### The 3.10 latent planet-field finding, applied to the save

- The record has no field for `sprite`, `palette_block`, `acceleration_factor` or `speed`; `save_state_capture` reads none of them (it reads the header, `planet_type`, `description`, `project`, `needs`, `has_`, `has_development_tier`/`development_tier`, `has_production_bonus`/`production_bonus`, `production`, `synthesizable_resources`, `archetype`, `outgoing_routes`, `slipways`, all of which `planet_init_instance` writes for every kind); `game.save_load` writes nodes only through their kind's constructor and then sets saved fields, and the functions it calls (`planet_install_project`, `economy_tiers_compute_production_amount`, `synthesizer_set_spent`, the attach methods) read none of the four. So the save must not and does not read them: no nil-as-0 fallback is added (it would mask a divergence). The trap stays for 3.15's `debug_state` alone.
- Confirmed during the round trip by an instrumented throwaway run: a metatable on every planet-family table traps a read of any of the four keys while nil, over every corpus load and capture; zero trapped reads is recorded in the note (any trap is a finding for `fix-bug`).
- The planet's own flags are `has_` (3.6): capture reads `planet.has_` for `PLANET_HAS_NEEDS`, `PLANET_HAS_PRODUCTION`, `PLANET_HAS_OUTGOING_ROUTES`; `load_needs`/`load_production`/`load_outgoing_routes` write `planet.has_`; only the header's `has` is the entity's (the 3.7 `tech_gate` bug came from mixing them).

### Verification of byte identity (throwaway, not committed)

- Corpus: a throwaway C harness links the replay tool's objects with a recording backend in place of the memory one (same three functions, appending every distinct record written, as hex, with its source replay and frame) and replays every `tests/e2e/*.json` and `worst-case/*.json`; `make replay SAVE_HEX=` on the same files adds each last record. The note records the corpus size, the largest record and which of the nine node kinds, flags and route shapes it holds; a kind or flag absent from the corpus is covered by a synthetic world in the capture check below.
- Codec: for every record, C decode and Lua decode give the same return value and the same text dump of every field of the decoded state (counts, then every used element; same throwaway dump format on both sides); Lua encode of the Lua-decoded state gives the original bytes exactly; Lua CRC equals the C's.
- Rejections: mutations of every record (single-byte changes at many offsets, truncations, extensions, header length and version changes, each with and without a recomputed CRC so validation passes stage 5), plus every flag bit, count cap and reference rule of the validation list forced once: C and Lua decode return the same value on each, and Lua never raises. Hand mutations of the Lua validators (each rule removed once) must be caught by this run.
- Capture and load: a throwaway Lua program and its C twin bring up a game the way `cart_resume_game` does (cart_load's inits, `start_world`'s calls, `start_interface`'s calls without the menus, which are 3.13's), then: (a) fresh galaxies (several seeds, all four difficulties, `galaxy_populate`) captured and encoded: Lua bytes equal C bytes; (b) for every corpus record, `save_load_difficulty`, `save_load_background_seed`, `save_load_world`, `save_load_globals`, recompute, then capture and encode: bytes equal the record on both sides, and a throwaway world dump after load (every live entity in registration order with class, state, state_time, positions, fold anchor, shake, infraspace, layer, planet fields, needs, production, routes, slipway lists; the picker, walks, economy, invented list, icon id counter, camera, RNG state) is equal C vs Lua; (c) synthetic worlds for any kind or flag absent from the corpus, captured C vs Lua. Hand mutations of the Lua capture and load (one call order swapped, one flag dropped) must be caught.
- Storage: a throwaway game directory under `tmp/` whose `main.lua` runs the storage contract of `tests/save_storage_contract.c` (empty storage reads 0; every byte value round-trips; a full 8192-byte record round-trips; a shorter overwrite leaves no stale tail; erase empties and is idempotent) through `game.save_storage` inside `tmp/fake_love`, once with no `initial_save` and once with a replay's `initial_save` seeded (read returns its bytes and decode returns 1).

### Process

- No question needed `guideline`: the plan, the conventions of the step notes, `docs/save-format.md` and `design.md` settled every point above. The PRD's agent decided the storage file shape (an empty `save.bin` for erase, since the design lists no `remove`) and the lazy large-array zeroing itself.

## Implementation Chunks

1. **The codec.** `game.save_state`'s header constants, `magic`, `crc32_table`, the cursor writers and readers, `write_indices` … `save_state_encode`, `read_economy` … `save_state_decode` with every validator, `save_state_crc32`; the `save_state` reference unit and its `TABLE_REFERENCE_SOURCES` entry. Verified: the module loads, its blocks compare equal, the codec part of the throwaway round trip (corpus decode dumps, re-encode bytes, CRC, rejection mutations) is byte-identical, hand mutations caught.
2. **The capture.** The rest of `game.save_state`: `vec_of` … `save_state_capture`, in C order after the CRC. Verified: the checklist counts `save_state.c` complete (58/58); fresh-galaxy and synthetic-world captures byte-identical C vs Lua.
3. **The load and the storage.** `game.save_load` with `node_constructors`, its checklist exception line and reference unit; `game.save_storage`. Verified: `make checklist STEP=3.12` passes (78/78, 3/3); every corpus record loads and re-captures to its own bytes in both games with equal world dumps; the instrumented planet-field run traps nothing; the storage contract passes in `tmp/fake_love`.
4. **Own-time gate and step note.** Own times of `lua-guard` and `lua-test-modules`, paired against `5b3aa02` (45 pairs, as 3.3 to 3.11); pass when each stays under 100 ms and grows within 2× of 3.1's cost per KB and per table. The `### Step 3.12` note in the plan records: the modules, the checklist counts, the checklist exception line, the storage backend's shape and erase decision, the planet-field finding (save reads none of the four fields, zero trapped reads; the trap remains for 3.15), the corpus (records, replays, largest record, kinds covered, synthetic worlds) and what each part of the round trip compared, the hand mutations caught, the own-time figures (KB of Lua, blocks, lines, ms before and after, ms per KB) and the projection to the end of the plan. No full rule-4 protocol (not a checkpoint) unless the gate fails.

## Testing Decisions

- A good test here checks external behaviour against the C reference: bytes out, return values, and the world the game holds afterwards, never the Lua internals. The C game is the oracle; every comparison is C vs Lua on the same input.
- Permanent checks: none new. The step adds its files and tables to the three Phase 3 checks (`lua-guard`'s undeclared-global rule, the load check and the data check of `lua-test-modules`) and adds no `make test` target, per the Phase 3 header. The two reference units are the step's registered data.
- Behaviour is checked by throwaway C-vs-Lua programs (prior art: every step note from 3.4 to 3.11, "behaviour checked against the C with throwaway programs, all byte-identical", with hand mutations caught as in 3.11), deleted before commit; their results go in the step note.
- The storage contract mirrors `tests/save_storage_contract.c` (prior art for every backend), run once under `tmp/fake_love` in a throwaway game directory; it is not added to `lua-test-runtime` (which covers `game/p8/` only) nor to `make test`. Plan 3's `pad_resume_at_start.json` and the other replays cover the save permanently.

## Out of Scope

- The cart (`cart.c`, 3.14): `title_resume_handler`, `cart_resume_game`, `save_at_frame_end`, the static `g_save_state`/`g_save_bytes`, and `start_world`/`start_interface` (written ad hoc in the throwaway only).
- The bindings file (`bindings.cfg` on the fake disk): read by the game's startup (3.14/3.15), not by the save.
- Menus, title and attract (3.13); `debug_state()` and its handling of the four planet fields (3.15).
- `src/devtools/` (the memory backend, `save_check`, `replay_source`): not ported, per plan 2's rules.
- Running the e2e replays (plan 3); any change to the C save format or to `docs/save-format.md` (the format is unchanged; the Lua copies it).
- Audio.

## Further Notes

- Corpus sizes to expect: `docs/save-format.md` gives 4519 bytes as the largest worst-case record and `pad_resume_at_start.txt` a 4186-byte, 92-node record; a record is written at most once per action, so the Lua byte-array approach costs well under a millisecond per save.
- Expected own-time cost: about 1,300 C lines become roughly 40–50 KB of Lua, so `lua-guard` about +3–4 ms (3.3–3.11 rate about 0.08 ms per KB) from about 78 ms, and `lua-test-modules` about +2 ms (three tables, the 256-line CRC block the largest). 3.11's note projects `lua-guard` at 85–95 ms at the end: the note must update that projection.
- If the round trip finds a divergence in an already-ported module (as 3.7 found `tech_gate`'s `has`), it is fixed with `fix-bug` in its own commit before the save commit, and the note says so.
