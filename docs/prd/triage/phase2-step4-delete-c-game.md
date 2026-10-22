---
title: "Delete the C game and everything that only served it"
description: "After step 3 frees every surviving check from src/, src/, the C unit tests, the C-only tools and guards, their Makefile targets and CI steps still cost build time and maintenance and serve nothing."
status: needs-triage
---

## Problem Statement

Once step 3 is done, every target that plan 4 keeps or ports runs without `src/`. The repository still carries the C game, though:

- `src/` has about 250 tracked files. `tests/` has 134 C unit tests (step 3's `C_UNIT_TESTS`) and helpers that only exercise it, plus 19 fixtures that only those tests read.
- `tools/` still holds the C-only generators (`fixed_reference.c`, `table_reference.c` and its 80 units, `gen_quarter_wave.c`), the guards that scan `src/` (float, alloc, pointer, random, button, constants) with their exception lists, and the coverage, e2e-order, frames-reference, state-peaks and checklist scripts.
- About two thirds of the Makefile only builds or checks the C: five object trees (plain, ASan, UBSan, sanitize, coverage), the engine source lists, the generated bindings and demo units, `tmp/replay` and its variants, `frames_tool`, and 22 public targets that the inventory marks "deleted, C-only".
- CI's `test` job still runs `make sanitize-net` and `make full-game`, which replay the C.
- Step 3's `ifneq ($(wildcard src),)` block keeps all of this in `make test` while `src/` exists, so a developer's loop still builds the C on every clean tree.
- Step 3 left orphans: the `STATE_RUNS` / `state-run-%` rules, their variables and `tools/state_peaks.sh` are no longer used by any target, but they still name `tmp/replay`.

A developer reading the repository cannot tell what is live. Every new change also has to keep a dead C build green.

## Solution

Delete the C side in one step, and keep exactly what the target inventory (as amended by step 3's note) lists as kept or ported:

- `src/` goes entirely.
- Every C unit test that is not a surviving test goes, with its helpers and the fixtures only it reads. The fake LÖVE's own tests and the tests of the tools that surviving targets use stay.
- The C-only tools, guards, generators, exception lists and scripts go.
- The Makefile loses every rule, variable and target that only served them, including step 3's C-only block, its `src/` switch, and the orphaned state-peaks runs. `make help` then lists exactly the kept and ported targets plus the step-2 Lua targets.
- CI's `test` job runs `make test-all` only. The `love-capture` job is unchanged.

Then the step proves the result: no `src/` left, every kept or ported target present and green, `make e2e` under 9 s, `make test` measured under design.md rule 4, and CI green on the pushed branch.

## User Stories

1. As a developer, I want `src/` gone from the tree and from git's index, so that the repository holds the Lua game, the fake LÖVE and their tests only.
2. As a developer, I want every C unit test of the game, engine, runtime and devtools deleted, so that `make test` stops compiling code nobody ships.
3. As a developer, I want the fake LÖVE's own tests (`test_fake_love*`, `test_love_e2e`, `test_love_runtime`) kept and green, so that the oracle that runs every replay stays checked.
4. As a developer, I want the tests of the surviving shell and Lua tools (`test_lua_guard`, `test_lua_modules`, `test_lua_fixed`, `test_end_state`, `test_e2e_limits`, `test_frames_check`, `test_fake_love_guard`) kept and green, so that the scripts `make e2e`, `make end-state`, `lua-guard`, `lua-test-modules` and `fake-love-guard` rely on stay tested.
5. As a developer, I want the test helpers that only C game tests use (`heap_allocator`, `png_reader`, `save_storage_contract`) deleted, so that nothing in `tests/` is orphaned.
6. As a developer, I want the fake's helpers (`fake_love_cli`, `fake_love_png`) kept, so that the fake's tests still build.
7. As a developer, I want the fixtures only the deleted C tests read deleted (the `events_*.json`, `demo_events.json`, `screencast_capture_events.json`, `title_pick_*.json`, and the `*_refused.json`, `wormhole*.json` fixtures of `test_live_slots`), so that `tests/fixtures/` holds only what a surviving test opens.
8. As a developer, I want `tests/fixtures/fake_love/` and `tests/fixtures/love_runtime*/` kept unchanged, so that the fake's tests and `test_love_runtime` keep their inputs and frozen expectations.
9. As a developer, I want the C-only guards (`float-guard`, `float-grep`, `float-link`, `alloc-guard`, `pointer-guard`, `random-guard`, `button-guard`, `data-guard`, `p8-guard`, `constants-check`) deleted with their scripts and exception lists, so that no check scans a directory that no longer exists.
10. As a developer, I want the generators built from `src/` (`asset-export`, `sin-table`, `bindings-lua`, `demo-lua`) deleted, so that the committed Lua files they wrote are the source from now on.
11. As a developer, I want `coverage`, `e2e-order` and `e2e-order-check` deleted with their scripts and `tests/e2e/order.txt`, so that no file ranks replays by C lines.
12. As a developer, I want `e2e-ref` deleted with its frames tool and script, so that a divergence is read from the Lua state dumps and the capture alone, as the plan rules.
13. As a developer, I want `checklist` and `lua-sweep-fixed` deleted with the checklist script, its listing, and the fixed and table reference generators, so that nothing compares the Lua with a C that is gone.
14. As a developer, I want `sanitize-net` and `full-game` deleted, so that no target replays the C.
15. As a developer, I want step 3's `ifneq ($(wildcard src),)` block (`C_ONLY_UNIT`, `C_ONLY_TEST_FIRST`, `C_ONLY_TEST_LAST`), its uses in `test` and `unit`, and `C_UNIT_TESTS` / `C_UNIT_RUNS` removed, along with the orphaned `STATE_RUNS`, `state-run-%` rules, `STATE_DIR`, `STATE_EVENTS`, `state_file` and `tools/state_peaks.sh`, so that the Makefile has a single unconditional `test` and `unit` and no rule still names `tmp/replay`.
16. As a developer, I want the object trees for ASan, UBSan, sanitize and coverage and their pattern rules removed, so that only the plain object tree remains.
17. As a developer, I want the engine, core, cart-only, replay and state source lists, and the generated bindings and demo units, removed, so that no variable names a path under `src/`.
18. As a developer, I want `-Isrc` dropped from the compiler flags, so that the surviving C (the fake and its tests) compiles without any include path into a deleted tree.
19. As a developer, I want the dependency-file includes cut down to the plain object tree's `tests/` and `tools/` fragments, so that the Makefile reads no fragment of a deleted tree.
20. As a developer, I want `lua-sweep-fixed` removed from the `flock` goal list, so that the lock list names only existing targets.
21. As a developer, I want `make help` to list exactly the kept, ported and step-2 targets, so that it describes the repository as it is.
22. As a developer, I want each remaining help line still at most 20 words, with `test`'s approximate duration updated when the measurement moves it by a second or more, so that `make help` stays accurate.
23. As a developer, I want Makefile comments that describe deleted rules removed with those rules, and the stale ones fixed (the "longest first" remark over `UNIT_TESTS`, which no longer holds for the reordered survivors; the `float-guard` remark over `test`; the five object trees; the frames captures; step 3's "What only serves the C" comment), so that the comments the CLAUDE.md rules allow stay true.
24. As a developer, I want `make test` to stay green on a tree whose `tmp/` still holds objects and dependency files from the C era, so that a developer who pulls the change is not forced to clean.
25. As a developer, I want `make test` to stay green on an emptied `tmp/`, so that a fresh clone and CI pass.
26. As a developer, I want `make e2e` under 9 s, measured the way step 1.1 measured N, so that the plan's speed rule holds after the deletion.
27. As a developer, I want `make test` measured under design.md rule 4, against this step's "before" commit and the plan reference, so that the "never slower" rule is checked rather than assumed.
28. As a maintainer, I want every `*.sha256` and `.end-state` in `tests/e2e/` and `worst-case/` unchanged, so that the source of truth is untouched.
29. As a maintainer, I want `worst-case/` kept, so that `make end-state EVENTS=worst-case/...` still runs the long games.
30. As a maintainer, I want `bindings.default.cfg` and `demo/` kept for now, so that the README and the e2e notes that cite them stay true until step 5 decides their wording.
31. As a maintainer, I want CI's `test` job to run only the install and `make test-all`, so that CI checks the Lua repository and nothing that replays the C.
32. As a maintainer, I want CI's `love-capture` job unchanged, so that the fake keeps being checked against real LÖVE.
33. As the plan pilot, I want CI green on the pushed branch after this step, so that the deletion is proven outside my own machine.
34. As the plan pilot, I want every target listed as kept or ported in the inventory, as amended by step 3's note, run once after the deletion with its result recorded, so that "still present and green" is shown target by target.
35. As the plan pilot, I want a search of the Makefile and the CI workflow finding no path under `src/` and no deleted target, so that no dangling rule survives.
36. As the plan pilot, I want the step note "Step 2.4" in the plan to record the deleted file counts, the target results, the `make e2e` runs and the `make test` timing, so that step 5 and later readers have the evidence.
37. As the step-5 implementer, I want the docs, `CLAUDE.md`, `README`, skills and e2e notes left untouched except where the build requires it, so that step 5 rewrites them in one pass.
38. As a developer using the fix loop, I want `UNIT_TESTS` still to be the list of C tests that `make test` runs, so that `check-e2e`, `fix-bug` and `tdd` keep registering a fake test the way they already describe.

## Implementation Decisions

### What is deleted

- **`src/`**, entirely.
- **C tests**: every test of `C_UNIT_TESTS` (134, `test_state_peaks` and `test_checklist` among them) and any other `tests/test_*.c` that is not in `UNIT_TESTS`, and the helpers only they use: `heap_allocator`, `png_reader`, `save_storage_contract` (both `.c` and `.h`).
- **Fixtures** read only by deleted tests: the 19 files directly under `tests/fixtures/` (the `events_*.json` family, `demo_events.json`, `screencast_capture_events.json`, `title_pick_forgiving.json`, `title_pick_reasonable.json`, `grabs_refused.json`, `probes_refused.json`, `slipways_refused.json`, `wormhole_abandoned.json`, `wormholes.json`). Before deleting each one, a search confirms no surviving test, tool or Makefile rule names it.
- **Tools** (a search confirmed no kept target, script or test needs any of them; the kept files that name `fixed_reference.c` or `table_reference.c` do so only in comments, see "Docs touched"): `alloc_guard.sh` and its exceptions, `button_guard.sh`, `checklist.lua` and `checklist.txt`, `constants_check.sh`, `constants_compare.sh`, `constants_expected_gaps.txt`, `pico8_reference_probe_out.txt`, `coverage.sh`, `e2e_order.sh`, `fixed_reference.c`, `float_grep.sh` and its exclusions, `float_guard.sh` and `float_guard.txt`, `float_link.sh`, `frames_ref.sh`, `gen_quarter_wave.c`, `pointer_guard.sh` and its exceptions, `random_guard.sh` and its exceptions, `state_peaks.sh`, `table_reference.c` and the `table_reference/` directory.
- **`tests/e2e/order.txt`**: an output of `e2e-order`, read by nothing else.
- **Makefile**: the 22 "deleted, C-only" targets of the inventory and everything only they reach:
  - the four extra object trees with their flags, pattern rules and `.d` includes;
  - `ENGINE_SRCS`, `CORE_SRCS`, `CART_ONLY_SRCS`, `DATA_ENGINE_SRCS`, `REPLAY_SRCS`, `STATE_SRCS`, the bindings and demo generated units, `tmp/replay` and its sanitize and coverage variants;
  - `frames_tool` and the `FRAMES_*` captures, with their `.SECONDARY` and `-include` lines;
  - the sanitize, full-game, coverage and e2e-order variables and runs;
  - the orphaned state-peaks runs step 3 left behind: `STATE_DIR`, `STATE_EVENTS`, `STATE_RUNS`, `state_file`, the `state-run-%` rules and their `.PHONY`. No target uses them since step 3 ported `state` (H-3-4). `FULL_GAME_EVENTS` goes with `full-game` and them;
  - the per-test link rules of deleted tests, the `ASAN_OPTIONS` special runs (`UNIT_RUNS_SPECIAL` keeps only `unit-run-test_love_runtime`, whose run takes arguments), and the ordering lines between deleted tests;
  - the fixed and table reference build rules (`tmp/fixed_reference`, `tmp/fixed_reference.bin`, `tmp/table_reference`, `tmp/table_reference.txt`), `FIXED_SWEEP_*` and `TABLE_REFERENCE_*`. Since step 3, only `lua-sweep-fixed` and `checklist` use them, and both are "deleted" in the inventory. `LUA_DEFERRED_MODULES` stays: `lua-test-modules` still passes it;
  - step 3's `ifneq ($(wildcard src),)` block with `C_ONLY_UNIT`, `C_ONLY_TEST_FIRST` and `C_ONLY_TEST_LAST`, their references in `test` and `unit`, `C_UNIT_TESTS`, `C_UNIT_RUNS`, and `$(C_UNIT_RUNS)` in the pattern rule's filter and the `.PHONY` line (H-3-1).
- **CI**: the `make sanitize-net` and `make full-game` steps of the `test` job. No whole job only ran C, so no job is removed.

### What stays

- **Surviving C tests**: the 33 of step 3's `UNIT_TESTS` (the name the fix-loop skills already use), unchanged: every `test_fake_love*`, `test_love_e2e`, `test_love_runtime` (H-3-2), `test_lua_guard`, `test_lua_modules`, `test_lua_fixed` (H-3-5), `test_end_state`, `test_e2e_limits`, `test_frames_check`, `test_fake_love_guard`. Their helpers `fake_love_cli` and `fake_love_png` stay.
- **Tools**: `capture_read.lua`, `e2e_limits.sh` and `e2e_long_exceptions.txt`, `end_state.sh`, `fake_love/`, `fake_love_guard.sh`, `frames_check.sh`, `love_capture/`, `love_e2e_report.sh`, `love_e2e_run.sh`, `love_e2e_state_report.sh`, `love_install.sh`, `lua_guard.lua`, `progress_check.sh` (plan-pilot's commit hook, which no target uses).
- **Data**: `tests/lua/` (with step 3's frozen `fixed_reference.txt` and `table_reference.txt`, H-3-3), `tests/fixtures/fake_love/`, `tests/fixtures/love_runtime*/` (with step 3's `expected_capture.bin` and `expected_state.txt`), `tests/e2e/` minus `order.txt`, `worst-case/`.
- **`bindings.default.cfg` and `demo/`**: their only build consumers (`bindings-lua`, `demo-lua` and C tests) go, but the README documents the buttons through `bindings.default.cfg`, and the e2e notes and two `.cfg` fixtures cite both. Deleting them would make those texts false before step 5 can rewrite them. Keeping them is also the smaller change. Decided under the guideline rule "between two options that both work, take the smaller change". They are a recorded leftover for step 5.
- **`game/`** is not touched, including the four generated-file headers that name deleted targets (`make bindings-lua`, `make demo-lua`, `make sin-table`, `make asset-export`) and the "Copy of src/…" headers. Step 5 covers tracked files outside `game/` only, and a comment change in `game/` is not needed for the build. These lines are recorded in the step note as leftovers for plan 5.
- **Targets**, as the inventory lists them (amended by step 3's note, H-3-6). Kept: `help`, `test`, `test-all`, `unit`, `fake-love`, `fake-love-guard`, `love`, `love-capture`, `love-check`, `lua-guard`, `lua-test-runtime`. Ported: `e2e`, `end-state`, `e2e-baseline`, `end-state-baseline`, `state`, `replay`, `lua-test-fixed`, `lua-test-modules`. Plus step 2's `lua-test-game` and `lua-test-capture-read`.

### Makefile shape after the step

- `CFLAGS` no longer gets `-Isrc`. The plain object tree, `objs`, `DEPFLAGS`, `NPROC`, the `flock` wrapper, `SHA256_CMD`, `.DELETE_ON_ERROR`, the `love-e2e` run rules and `FORCE` stay.
- `test` lists, longest first: `unit`, `$(FAKE_LOVE_CHECKS)`, `lua-test-fixed`, `lua-test-runtime`, `lua-guard`, `lua-test-modules`, the Lua game test runs, `lua-test-capture-read`, `fake-love-guard`. This is step 3's line minus `$(C_ONLY_TEST_FIRST)` and `$(C_ONLY_TEST_LAST)`. The order is re-measured once on an emptied `tmp/`, with the longest first. The comment above `test` says only what the new order rests on, or goes.
- `unit` runs every entry of `UNIT_TESTS`, which is already the surviving list. The "longest first" comment over `UNIT_TESTS` is stale: step 3 put the shell-tool tests first. Either the list is reordered by measured time (one cold run per test on an emptied `tmp/`) to match the comment, or the comment goes. The smaller change, removing it, is taken unless the measurement shows that the order moves `make test`'s wall time.
- `LOCKED_GOALS` drops `lua-sweep-fixed`. The first `.PHONY` line drops `e2e-ref`.
- The `-include` of dependency fragments keeps only the plain tree's `tests/` and `tools/` fragments. Stale fragments of deleted tests in an old `tmp/obj/tests/` are harmless: `-MP` wrote phony header targets, and no rule asks for those objects. User story 24 checks this.
- `fake-love-guard` and `tools/fake_love_guard.sh` are unchanged. With no `src/` they keep guarding against its return, and the inventory keeps them.
- `tests/lua/test_fixed.lua`'s standard-input mode, which only `lua-sweep-fixed` used, is left as it is. Changing a test reader is not needed to delete the C, and `test_lua_fixed` covers that reader.

### Docs touched

- Only what the build needs: nothing in `CLAUDE.md`, `README` or `docs/` is required for `make` or CI to work, so none of it changes here. Their mentions of deleted targets (`lua-sweep-fixed` in `CLAUDE.md`'s lock list, `make coverage` in two e2e notes, `src/` throughout) are step 5's. So are the comments in kept test files that name deleted generators: the header of `tests/lua/fixed_reference.txt` and the format comments of `tests/lua/test_fixed.lua`, `tests/lua/test_modules.lua` and `tests/test_lua_modules.c` (`tools/fixed_reference.c`, `tools/table_reference.c`). Also step 5's are the leftovers step 3's note names: `lua-test-modules` printing "equal to C", and the README and e2e notes citing `SAVE_HEX` and bare `make state`. All of these are listed in the step note.
- The plan file gets its "Step 2.4" note (the verification record), as steps 1.1 and 1.2 did.

### If step 3 left something behind

If, at implementation time, a kept or ported target still needs `src/` (a hypothesis below is false), this step frees it the way step 3's PRD decided, before deleting `src/`, and records it in the step note. It never drops the target. Decided under the guideline rule "make the plan work": the step's check requires every kept or ported target green.

## Implementation Chunks

1. **Delete the C and its Makefile side.** Remove `src/`, the C tests, helpers, fixtures, tools and `tests/e2e/order.txt` listed above, together with every Makefile rule, variable, target, comment and include that served them, step 3's `ifneq ($(wildcard src),)` block and `C_UNIT_*` lists, the orphaned state-peaks runs, and `-Isrc`. Fix the stale `UNIT_TESTS` comment. Green: `make test` and `make e2e` on an emptied `tmp/` and on a `tmp/` left from before the deletion. `make help` lists exactly the expected targets. A search of the Makefile finds no `src/` path and no deleted target name.
2. **CI.** Remove the `sanitize-net` and `full-game` steps from the `test` job.
3. **Prove and record.** Run every kept, ported and step-2 target once on an emptied `tmp/`. `make e2e` six cold runs, as in step 1.1 (`tmp/love-e2e` removed, fake built, 4 cores), median under 9 s. `make test` timed under design.md rule 4. Every `*.sha256` and `.end-state` is unchanged. After the push, the branch's CI run is green. Write the "Step 2.4" note in the plan.

## Testing Decisions

- This step adds no behavior, so it adds no test. Its tests are the surviving ones. A good check here runs a public target and reads its exit status and output, not the Makefile's internals.
- **Target sweep**: each kept, ported and step-2 target is run once after the deletion, on an emptied `tmp/`, with the same extra conditions step 3's dry run used. `e2e-baseline` and `end-state-baseline` leave `git status --porcelain tests/e2e/` empty. `love-capture` leaves `tools/love_capture/` with no diff and no untracked file. `state EVENTS=` and `replay EVENTS=` run on one short replay. `end-state EVENTS=` runs on one `worst-case/` game.
- **Stale-tmp check**: `make test` is run once on a `tmp/` produced by `make test` at the "before" commit, to show the leftover objects and dependency fragments of deleted tests break nothing.
- **Dangling-reference check**: a search of the Makefile and `.github/workflows/` for `src/`, for each deleted target name and for each deleted tool file name must find nothing. Exceptions are `fake-love-guard`'s own help line and arguments, which name `src/` on purpose. A search of the surviving `tools/` and `tests/` for the deleted tool and fixture names must likewise find nothing, except `tests/e2e/*.txt` notes and the two `.cfg` fixtures, which are step 5's.
- **Timing**: `make e2e`, six cold runs, median and each run recorded, median under 9 s. `make test`, design.md rule 4 against the step's "before" commit and `8dff812`. Step 3 measured 5.239 s current against 5.049 s reference (+3.01 %, kept on step 1.2's `guideline` decision). The "before" and reference checkouts both have `src/` and run the C group, while this step's checkout has none. A large gain is expected.
- **CI**: the run of the pushed branch is read through the GitHub tools or `gh run list`/`gh run view`. Both jobs must pass. A red job is fixed in this step.
- Prior art: step 3's dry run (target list and extra conditions), step 1.1's N measurement, step 1.2's rule-4 timing record.

## Out of Scope

- Rewriting `CLAUDE.md`, `README`, `docs/love-port/design.md`, the skills or the e2e notes (step 5), including the `lua-sweep-fixed` lock-list entry in `CLAUDE.md`, the warn-review and rework-comments skills that scan `src/`, and the `make coverage` lines of two e2e notes.
- Any change under `game/`, including the comments naming `src/` or deleted targets (plan 5).
- Deleting `bindings.default.cfg` or `demo/`.
- Changing what `make e2e`, `make end-state`, `state`, `replay` or the frozen-reference checks do.
- New checks, new guards, or a guard against `src/` coming back beyond `fake-love-guard`.
- Changing the inventory's categories. A forced change is recorded as an amendment in the step note.

## Further Notes

- Sizes after step 3: 250 tracked files under `src/`, 134 C tests in `C_UNIT_TESTS` against 33 survivors in `UNIT_TESTS`, and 80 units under `tools/table_reference/`. The step note records the counts actually deleted.
- The skills' `UNIT_TESTS` wording (`check-e2e`, `tdd`) stays true, since the variable keeps its name and meaning for the fake's C tests.
- `warn-review` runs at the end of this phase and reads `src/`. With `src/` gone it finds nothing there, which is correct, and its wording is step 5's.

### Hypotheses on step 3

Step 3 is done: commits `a060b0c` to `60e62c1` and the plan's "Step 1.3" note. Each hypothesis has been checked against the committed tree.

- **H-3-1** (corrected): the C-only group is three variables, `C_ONLY_UNIT`, `C_ONLY_TEST_FIRST` and `C_ONLY_TEST_LAST`, set inside one `ifneq ($(wildcard src),)` block. `C_ONLY_UNIT` is `$(C_UNIT_RUNS)`, built from the separate `C_UNIT_TESTS` list of 134 tests. `UNIT_TESTS` holds the 33 survivors. This step deletes the block, the three variables and their uses in `test` and `unit`, and `C_UNIT_TESTS` / `C_UNIT_RUNS` with their uses in the pattern rule's filter and `.PHONY`.
- **H-3-2** (confirmed): `test_love_runtime` links only `tests/test_love_runtime.c` and `tests/fake_love_cli.c`. It reads `expected_capture.bin` and `expected_state.txt` per fixture and is a fake-side binary.
- **H-3-3** (confirmed): `lua-test-fixed` reads `tests/lua/fixed_reference.txt` (5291 records) and `lua-test-modules` reads `tests/lua/table_reference.txt` (876 blocks). `tools/fixed_reference.c` is used only by `lua-sweep-fixed`, and the `tools/table_reference*` build only by `checklist`. Both targets are "deleted" in the inventory, so both generators go. Kept files name them in comments only.
- **H-3-4** (corrected): `make state` and `make replay` run the fake with `EVENTS=` required, and the C-only options and the no-`EVENTS` peaks mode are dropped. However, `STATE_RUNS`, the `state-run-%` rules, `STATE_DIR`, `STATE_EVENTS`, `state_file` and `tools/state_peaks.sh` are not in the C-only group. They are orphaned, since no target uses them, and they still name `tmp/replay`. This step deletes them explicitly. `test_state_peaks` is in `C_UNIT_TESTS` and goes with it.
- **H-3-5** (confirmed, with a name): the fixed reader's C test is `test_lua_fixed`, in `UNIT_TESTS`. It stays.
- **H-3-6** (confirmed): the "Step 1.3" note records the dry run and makes no inventory amendment. The inventory stands as written in the plan.
- **H-3-7** (confirmed): the dry run (worktree, `src/` removed, `tmp/` empty) showed every kept and ported target, step 1.2's targets and every surviving CI command green, and the baselines left `tests/e2e/` untouched.
- **H-3-8** (confirmed, wider than assumed): step 3 rewrote more of `docs/love-port/design.md` than two sentences, to describe the frozen references. It left `CLAUDE.md`, the README and CI unchanged. It deleted its own PRD from `docs/prd/triage/`.
- **H-3-9** (confirmed): every C-only fixture and tool listed above is still present.

Remaining known leftovers from step 3, handled here: the stale "longest first" comment over `UNIT_TESTS`, the `float-guard` remark over `test`, and step 3's "What only serves the C" comment, which goes with its block.

If the tree differs from the above at implementation time, the affected wiring follows what is actually there, under "If step 3 left something behind". The decisions above do not change.
