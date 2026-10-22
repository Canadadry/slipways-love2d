---
title: "Every surviving check runs without src/"
description: "lua-test-fixed, lua-test-modules, test_love_runtime, make state and make replay still build or run the C, so they would die with src/ in step 4."
status: needs-triage
---

## Problem Statement

Plan 4 deletes `src/` in step 4. Before that, every target the plan keeps or ports must stop needing anything built from `src/`. Today several of them still do:

- `lua-test-fixed` compares `game/p8/fixed.lua` with `tmp/fixed_reference.bin`, which `tools/fixed_reference.c` writes after linking `src/p8/fixed.c` (236 867 records, 2.4 MB, rebuilt on every clean `make test`).
- `lua-test-modules` compares every `game/` module's tables and constants with `tmp/table_reference.txt`, which `tools/table_reference.c` writes after compiling about 80 `src/` sources (41 000 lines, 2 MB).
- `unit-run-test_love_runtime` and `unit-run-test_love_runtime_slots` (part of `unit`, kept by the inventory, and of `FAKE_LOVE_CHECKS`) link seven `src/p8/` objects to draw each fixture scene with the C runtime and compare it with the Lua runtime in the fake. `test_love_runtime` is missing from `FAKE_SIDE_BINARIES`, so `fake-love-guard` never saw it.
- `make state` and `make replay`, listed as "ported" by the target inventory, still run `tmp/replay`, the C game. Step 1 left them to step 2, and step 2's PRD leaves them out of its scope, so this step has to port them.
- `make test` and `make unit` still need C unit tests and guards that scan `src/`. The inventory deletes those in step 4, so today nobody can show that `make test-all` (a target and a CI command that survive the plan) passes without `src/`.

If step 4 deletes `src/` before these are fixed, these checks have no oracle left. The `make state` and `make replay` targets cited throughout the e2e notes would also stop working.

## Solution

Freeze or port every surviving consumer of `src/`, and show it with one dry run:

- The fixed-point and random-generator results are written once by the C, as a few thousand text records with edge values included, to `tests/lua/fixed_reference.txt`. `test_fixed.lua` reads that text format, and `lua-test-fixed` depends only on the committed file.
- The table reference is frozen once to `tests/lua/table_reference.txt`. The blocks of the four generated-data modules are left out, because their committed Lua files become the source. `lua-test-modules` reads the frozen file.
- `test_love_runtime` compares the fake's frames and meter counts with expected frames and counts frozen next to each fixture. It no longer links `src/`, and it joins the fake side checked by `fake-love-guard`.
- `make replay EVENTS=<f>` and `make state EVENTS=<f>` run the fake on `game/` for that replay. They write the capture and the last frame as text, and the Lua state dump, to the same `tmp/` paths as today.
- The prerequisites of `test` and `unit` that only serve the C are grouped. They run only while `src/` exists, so after step 4 the literal `make test-all` is exactly what is left.
- A dry run in a throwaway worktree, with `src/` moved aside and `tmp/` emptied, runs every surviving target and every surviving CI command, and each one must pass. The evidence goes into the commit message and the plan's step notes.

## User Stories

1. As a developer, I want `lua-test-fixed` to read a committed reference file, so that it passes with no C compiler and no `src/`.
2. As a developer, I want the frozen fixed-point reference to hold a few thousand records, so that it stays readable and cheap to diff and load.
3. As a developer, I want every edge value the C oracle uses to stay in the frozen reference, in every input position of its section, so that the overflow, sign, `INT32_MIN` and division-by-zero cases are still checked after the C is gone.
4. As a developer, I want every section of today's reference (every name of `test_fixed.lua`'s signature table) in the frozen file, so that no fixed-point function goes unchecked.
5. As a developer, I want the random-generator sections frozen in the order the C drew them, starting from the default seed, so that the Lua generator is checked as the same stream and not as isolated calls.
6. As a developer, I want the `rnd_from_state` double-product trap (state `0xA4B668AA` with `±0x7FFFFFFF`) kept in the frozen file, so that a Lua copy that multiplies in doubles still fails.
7. As a developer, I want each record of the frozen file to be one text line with its inputs and outputs in hexadecimal, so that I can read the record a mismatch report names.
8. As a developer, I want `test_fixed.lua` to fail on a malformed line, an unknown name, a missing section or a wrong record count in the trailer, so that a damaged reference is never taken as green.
9. As a developer, I want mismatch reports to keep today's form (name, inputs, expected, got, at most five per name), so that a regression in `fixed.lua` reads the same way as before.
10. As a developer, I want `make lua-sweep-fixed` to keep working while the C exists, so that the millions-of-values comparison stays available until step 4 deletes it.
11. As a developer, I want `lua-test-modules` to read a committed table reference, so that it passes with no `src/`.
12. As a developer, I want the frozen table reference to leave out the blocks of `p8.quarter_wave`, `p8.sprite_sheet_packed`, `game.demo_data` and `game.bindings_default`, so that the repository does not hold a second copy of generated files that are now the source.
13. As a developer, I want those four modules still required by `lua-test-modules`, so that a broken generated file is still caught at load.
14. As a developer, I want every other block of today's table reference (tables, constants, sizes, state-format names) frozen byte for byte, so that the check stays as strict as it was against the C.
15. As a developer, I want `test_love_runtime` to compare the fake's frames with frozen expected frames, pixel for pixel, so that the Lua runtime is still held to what the C runtime drew.
16. As a developer, I want `test_love_runtime` to compare the fake's meter counts with frozen expected counts, so that the draw meter stays checked.
17. As a developer, I want `test_love_runtime`'s failure report to keep naming the frame, the first differing pixel and the differing pixel count, so that a runtime regression is located as before.
18. As a developer, I want `test_love_runtime` listed in `FAKE_SIDE_BINARIES`, so that `fake-love-guard` proves it links nothing from `src/`.
19. As a developer, I want `make replay EVENTS=<f>` to run the Lua game in the fake on that replay, so that I can look at any frame of a replay without the C.
20. As a developer, I want `make replay` to write the last frame as text in `tmp/replay_dump.txt`, in today's layout (a `160 120` line, then 120 rows of 160 hex digits), so that the e2e notes' "framebuffer dump of the file cut after frame k" method still works.
21. As a developer, I want `make replay` to keep the whole capture under `tmp/`, so that I can read any other frame with step 2's capture reader.
22. As a developer, I want `make state EVENTS=<f>` to write the Lua state dump of the replay's last frame to `tmp/state.txt`, so that the e2e notes' "`make state` at fk" method still works on a cut replay.
23. As a developer, I want `make state` and `make replay` to fail at once, naming the missing variable, when `EVENTS` is not given, so that they never replay a default file.
24. As a developer, I want `make state` and `make replay` to fail with the fake's message when the fake stops before the last frame, so that a Lua error is not hidden behind a stale dump.
25. As a developer, I want `make state` and `make replay` to honour `LOVE_E2E_GAME` as `make e2e` does, so that I can point them at an instrumented copy of `game/` under `tmp/`.
26. As a developer, I want each ported target's help line to say what it does on the fake, in at most 20 words, so that `make help` stays accurate.
27. As a developer, I want `make test` and `make unit` to run exactly their surviving checks when `src/` is absent, so that the literal CI command `make test-all` can be proven green before step 4.
28. As a developer, I want `make test` and `make unit` to behave exactly as today while `src/` exists, so that no C check is silently skipped before step 4 deletes it.
29. As the step-4 implementer, I want the C-only prerequisites of `test` and `unit` grouped in one place, so that deleting them is removing one group and its switch.
30. As the plan pilot, I want a dry run with `src/` moved aside and `tmp/` emptied that runs every target listed as kept or ported, step 2's new targets and every surviving CI command, so that "nothing surviving needs `src/`" is shown and not assumed.
31. As the plan pilot, I want the dry run to show that the baseline targets rewrite nothing (`git status` clean under `tests/e2e/`) and that `love-capture` leaves `tools/love_capture/` unchanged, as CI checks, so that "green" means a real pass.
32. As the plan pilot, I want the dry run done in a throwaway worktree outside `tmp/`, so that the main tree's `src/` and `tmp/` are never at risk and "restored" is guaranteed.
33. As the plan pilot, I want the dry run's list of targets, their results and the commands run recorded in the commit message and the plan's step note, so that step 4 can check against it.
34. As a developer, I want `make test`'s wall time measured under design.md's protocol, so that the "never slower" rule is checked. Fewer C builds should make it faster.
35. As a maintainer, I want every `*.sha256` and `.end-state` in `tests/e2e/` unchanged, so that the source of truth is untouched.
36. As a maintainer, I want the design.md sentences this step makes false (the fixed-point tests "compare it with the C", the table check "with the C ones") corrected, and nothing else rewritten, so that the docs stay true without doing step 5's work.

## Implementation Decisions

### Fixed-point reference

- **One text format, used everywhere.** The C generator (`tools/fixed_reference.c`, deleted in step 4) writes text instead of its binary format, in both of its modes. `test_fixed.lua` reads only that text, from a file or from standard input, so `lua-sweep-fixed` keeps working through the same reader until step 4. The binary format, its magic and `tmp/fixed_reference.bin` go away.
- **Text layout.** One record per line: `<name> <input>... = <output>...`. Each value is the 32-bit word as 8 lowercase hex digits, which keeps today's signedness rules per signature. A section of zero inputs reads `<name> = <output>...`. Lines starting with `#` are comments. The last line is `end <record count>`. Records are read and checked in file order. The random sections depend on that order, as today: the first draw is the default seed's.
- **Frozen composition.** The generator gains a third mode that writes the frozen set. Today's default mode (edge grids plus 3 000 samples per section, 236 867 records) stays only as the source of the sweep. The frozen set has between 4 000 and 12 000 records and contains:
  - the constants and the `P8_FIXED_LIT` literals, all of them;
  - for unary sections, every value of `edge_values` and of the section's extra edge list (`edge_ints`, `edge_roots`);
  - for binary sections, the full grid over a core subset of about a dozen edge values: 0, ±1 raw, ±0x8000, ±1.0, 0x18000, 0x00B504F3, 0x7FFF0000, 0x7FFFFFFF, −0x7FFFFFFF and `INT32_MIN`. Each remaining edge value is paired in both positions with 0, 1.0, −1 raw and `INT32_MIN`;
  - for `shl`, every edge shift with a core subset of values and every edge value with a handful of shifts;
  - for `mid`, the core-subset cube cut to about six values;
  - for every non-chained section, about 50 drawn samples;
  - for the random sections, the same structure as today with the sample tails cut to about 50: the default-seed stream, each edge state's set-and-draw block, the 32 default draws, the `rnd_from_state` chains over every edge state, the two `0xA4B668AA` records, and the trailing `rnd_state`.
  
  The exact subset belongs to the generator. These are its rules, and the counts are recorded in the commit message.
- **Production.** The frozen file is written once by the C generator and committed, using a command in the commit message. No new public target, so no inventory amendment. `lua-test-fixed` depends on `tests/lua/test_fixed.lua`, the `game/p8/` Lua files and `tests/lua/fixed_reference.txt` only. Its help line says it compares against the frozen reference.

### Table reference

- **Frozen file**: `tests/lua/table_reference.txt` is today's `tmp/table_reference.txt` minus the blocks of `p8.quarter_wave`, `p8.sprite_sheet_packed`, `game.demo_data` and `game.bindings_default`. Their Lua files are generator outputs that the inventory turns into the source, so comparing them with a frozen copy of themselves checks nothing. They stay loaded by the check. The block format (`= <module> <name> <bytes>` and lines) is unchanged, so `test_modules.lua`'s reader and its C test `test_lua_modules` are unchanged. A future intended table change edits the frozen block and its byte count in the same commit, and the reader's "overruns its size" message catches a wrong count.
- `lua-test-modules` reads the frozen file and no longer depends on `tmp/table_reference.txt`. Its help line says "the frozen reference" instead of "the C ones". `tools/table_reference.c` and its units stay until step 4 for `checklist`, which is deleted there.
- **Constants check**: `constants-check` (P8_FIXED_LIT against recorded PICO-8 values) only reads `src/` and is "deleted" in the inventory, so nothing is frozen for it. The Lua constants stay compared through the frozen table reference.
- **Generated Lua data**: `bindings-lua`, `demo-lua`, `sin-table` and `asset-export` are "deleted" in the inventory. Their outputs are already committed (`game/game/bindings_default.lua`, `game/game/demo_data.lua`, `game/p8/quarter_wave.lua`, `game/p8/sprite_sheet_packed.lua`, `game/sprites.png`, `game/font.png`). No surviving target regenerates them, so nothing more to do beyond the table blocks above.

### test_love_runtime

- **Frozen expectation per fixture**: next to each fixture's `scene.txt`, an expected capture (the raw frames, 9 600 bytes each, low nibble first, as the fake writes them) and an expected meter-state text. Both are written once from the current C side and committed. The test keeps its scene-frame-count argument and checks it against the expected capture's size. It writes the all-empty replay, runs the fake, then compares frames and meter counts with the frozen files, with today's report wording. It no longer parses `scene.txt` into C runtime calls: the scene stays the Lua fixture's input.
- The binary drops its `src/p8/` objects, and `test_love_runtime` joins `FAKE_SIDE_BINARIES` with its objects, so `fake-love-guard` checks it.

### `make state` and `make replay` (ported here)

- Step 2's PRD leaves them out of scope (H-2-3), and the inventory lists them as "ported", so this step ports them. Decided under the guideline rule "make the plan work".
- Both require `EVENTS` and fail at Makefile read time without it, as `replay` does today. Both run the fake once on `$(LOVE_E2E_GAME)` (default `game`) through `tools/love_e2e_run.sh`, the same single-run script as `make end-state EVENTS=`. Outputs go to their own directory under `tmp/`, and a stop of the fake fails the target with the fake's message.
- `make replay` keeps the capture and writes the last frame to `tmp/replay_dump.txt` in today's layout (a `160 120` line, then 120 rows of 160 hex digits). The frame is printed by step 2's capture reader (H-2-2).
- `make state` copies the Lua state dump of the last frame to `tmp/state.txt`.
- **Dropped options**: `SAVE_HEX`, `DIGEST`, `ATTRACT_STATE` and `REPLAY_STARTS` are C-replay options with no fake counterpart. `make state` without `EVENTS` (the zone and cap peaks over every replay) measured C memory zones and has no Lua meaning. These changes stay inside the inventory's wording ("the fake LÖVE's state dump and capture of one replay"), so this is not a category change. They are recorded in the step note. `tools/state_peaks.sh`, its test and the `STATE_RUNS` rules join the C-only group for step 4.
- The e2e notes are not rewritten here (step 5). A note line citing a C-only state line or option becomes a step-5 leftover.

### `make test` and `make unit` without `src/`

- The prerequisites of `test` that only serve the C are grouped in one Makefile variable: the inventory's deleted guards and checks (`float-guard`, `float-grep`, `alloc-guard`, `constants-check`, `pointer-guard`, `random-guard`, `button-guard`, `data-guard`, `p8-guard`) and the runs of C unit tests. `UNIT_TESTS` is split into the surviving tests and the C-game tests.
  - Surviving tests: the fake side's tests (`FAKE_SIDE_BINARIES`, now with `test_love_runtime`) and the tests of tools that surviving targets use (`test_lua_guard`, `test_lua_modules`, `test_end_state`, `test_e2e_limits`, `test_frames_check`, `test_fake_love_guard`).
  - C-game tests: everything else, including `test_state_peaks` and `test_checklist`.
- **One switch**: the C group is a prerequisite of `test` and `unit` only when `src/` exists in the tree. With `src/` present, `make test` runs exactly today's set, in today's longest-first order. With `src/` absent, it runs the surviving set. Step 4 deletes the group, its rules and the switch together. Chosen over a command-line override so that the literal CI command `make test-all` is what the dry run proves (guideline: "make the plan work").
- `fake-love-guard`, `lua-guard`, `lua-test-runtime`, `love-check`, the capture checks and step 2's targets (H-2-1, H-2-4) are outside the group.

### Dry run (the step's verification)

- **Setup**: a throwaway `git worktree` of the step's final commit, outside the repository's `tmp/`. Inside it, `src/` is moved outside the worktree and `tmp/` is emptied. The main tree is never touched, and the worktree is removed at the end, which is the "restored".
- **Targets run, each of which must pass**:
  - kept: `help`, `test`, `test-all`, `unit`, `fake-love`, `fake-love-guard`, `love`, `love-capture`, `love-check`, `lua-guard`, `lua-test-runtime`;
  - ported: `e2e`, `end-state` (all replays, and once with `EVENTS=` on a `worst-case/` game), `e2e-baseline`, `end-state-baseline`, `state EVENTS=`, `replay EVENTS=`, `lua-test-fixed`, `lua-test-modules`;
  - step 2's new targets (H-2-1, H-2-4).
- **CI commands run**: `sh tools/love_install.sh`, `make test-all`, the `love-capture` job's `make love-capture`, its two `git` checks on `tools/love_capture/`, and `make love-check`. `sanitize-net` and `full-game` replay the C, are deleted by the inventory, and are not run.
- **Extra conditions**: after `e2e-baseline` and `end-state-baseline`, `git status --porcelain tests/e2e/` is empty. After `love-capture`, `tools/love_capture/` has no diff and no untracked file. Each kept or ported target of the inventory appears in the run list. A target that cannot pass without `src/` is fixed here, or recorded as an inventory amendment in the step note before the step closes. It is never left out silently.
- **Record**: the command list and each result go into the commit message and the plan's step note "Step 1.3", along with the dropped `state` and `replay` options and the frozen file sizes and record counts.

### Docs

Only the sentences this step makes false are corrected: design.md's "Numbers and randomness" (the tests now read saved reference values) and its `lua-test-modules` description (frozen reference). `CLAUDE.md`, `README` and the e2e notes are step 5's.

## Implementation Chunks

1. **Fixed reference**: text writer in the generator (all modes) and frozen composition; committed `tests/lua/fixed_reference.txt`; `test_fixed.lua` text reader; `lua-test-fixed` and `lua-sweep-fixed` rewired. Green: `lua-test-fixed` with `src/` moved aside, `lua-sweep-fixed` with it present.
2. **Table reference**: committed `tests/lua/table_reference.txt` without the four generated-data modules; `lua-test-modules` rewired.
3. **test_love_runtime**: frozen expected captures and meter states per fixture; the test without `src/`; added to `FAKE_SIDE_BINARIES`.
4. **`make state` and `make replay` on the fake**, with their help lines.
5. **The C-only group and its switch** in `test` and `unit`; `make test` timing under design.md's protocol.
6. **Dry run** in a throwaway worktree; the step note in the plan, design.md's two sentences.

## Testing Decisions

- A good test here checks what a check reports and its exit status on a given reference file, not how the reader is built. Every frozen-data check is proven both ways: green on the committed file, and red with the expected report on a copy changed in a throwaway place (one output word flipped, one section removed, a truncated trailer, one frame pixel changed). These red cases are not committed as fixtures unless they are cheap and fit an existing C test.
- `test_fixed.lua`'s text reader: `test_lua_modules` is the prior art for testing a Lua reader through a small C test with crafted files. The fixed reader gets the same kind of fast check of its failure modes (malformed line, unknown name, missing section, bad trailer, a mismatch report). It goes in the surviving set and must not link `src/`.
- `test_lua_modules` is unchanged, since the format is unchanged.
- `test_love_runtime` is its own test. During development, one byte of an expected capture and one meter count are altered in a throwaway copy to see it fail with the frame and pixel named.
- `make state` and `make replay` are checked in the dry run on a short replay, and on a replay cut after a frame cited by an e2e note, comparing a line or two the note states.
- `make test` and `make e2e` are green at the end, and every `*.sha256` and `.end-state` in `tests/e2e/` is unchanged. `make test` is measured under design.md rule 4.
- Prior art: `tests/test_lua_modules.c` (C test of a Lua reader), `tests/test_love_e2e.c` and `tests/fake_love_cli.c` (driving the fake from a C test), `tools/love_e2e_run.sh` (one fake run with capture, hash, error and state).

## Out of Scope

- Deleting anything: `src/`, `tools/fixed_reference.c`, `tools/table_reference*`, the C unit tests, the C-only guards and targets, `lua-sweep-fixed`, `checklist`, `e2e-ref`, and the `sanitize-net` and `full-game` CI steps all stay until step 4.
- Step 2's work: the Lua regression home, the capture reader, the fix-loop skills.
- Rewriting `CLAUDE.md`, `README`, the e2e notes, the skills or design.md beyond the two sentences above (step 5).
- A frame-k state dump inside the fake or the game. A cut replay remains the method.
- Changing the inventory's categories. The only allowed outcome is a recorded amendment if the dry run forces one.

## Further Notes

- Sizes: the frozen fixed reference is expected to be a few hundred kilobytes and the frozen table reference about 500 KB (2 MB minus about 1.5 MB of generated-data blocks). Both are far smaller than today's per-build files.
- `make test` loses the cold builds of `tmp/fixed_reference`, `tmp/table_reference` (about 80 units) and the `src/p8/` objects of `test_love_runtime`, so the timing gate should pass with a gain.

### Hypotheses on step 2

- **H-2-1**: step 2 adds a Lua regression home (`tests/lua/game/test_*.lua`, one target per file, grouped under `lua-test-game`), all prerequisites of `test`, running under plain LuaJIT with an empty C module path. They need nothing from `src/` and sit outside this step's C-only group. They are in the dry-run list.
- **H-2-2**: step 2 adds a LuaJIT capture reader under `tools/` that prints frame k of a fake capture as 120 rows of 160 hex digits, low nibble first. `make replay` uses it for `tmp/replay_dump.txt` and adds the `160 120` header. If it is missing or prints another layout, this step writes the minimal last-frame printer itself in LuaJIT under `tools/`, with the same layout, and the rest does not change.
- **H-2-3**: step 2 does not port `make state` or `make replay` (its PRD says so), so this step ports them. If step 2 did port them, this step only checks them in the dry run and keeps whatever interface step 2 chose, provided it needs no `src/`.
- **H-2-4**: step 2's capture-reader test is its own target in `make test` and needs no `src/`. It is in the dry-run list.
- **H-2-5**: step 2 does not restructure `test`'s prerequisites or `UNIT_TESTS` beyond adding its own targets, so the C-only group is cut from HEAD's shape plus those additions.
- **H-2-6**: the rewritten fix-loop skills reproduce with `tmp/fake_love` directly and do not depend on `make state`, `make replay`, `tmp/fixed_reference.bin` or `tmp/table_reference.txt`, so this step's interface changes break no skill. If a skill cites `make state EVENTS=` or `make replay EVENTS=`, the ported targets keep that call form.
- **H-2-7**: step 2 does not change `tests/lua/test_fixed.lua`, `tests/lua/test_modules.lua`, `test_love_runtime` or their targets.

If a hypothesis is false when this PRD is implemented, the affected wiring follows what step 2 actually did. The decisions above do not change.
