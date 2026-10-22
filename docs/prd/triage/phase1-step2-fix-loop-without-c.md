---
title: "A fix loop that needs no C"
description: "A red e2e is still diagnosed against the C, reproduced with tmp/replay and pinned by a C unit test, so the fix loop dies with src/."
status: needs-triage
---

## Problem Statement

Plan 4 deletes the C game. Today, when `make e2e` goes red, the unattended fix loop (`check-e2e` → `fix-bug` → `help-me-debug` → `tdd`) is built entirely on the C: the bug report reproduces with `./tmp/replay` and locates the first divergent frame with `make e2e-ref` (which replays the C at an older commit), the debug agent reads the framebuffer through `make replay`'s text dump and is told "the C is the reference", and every regression test must be a C unit test in `tests/test_<module>.c` registered in `UNIT_TESTS`. None of that can test, or even reach, the Lua game in `game/`. The only Lua tests that exist (`tests/lua/`) check `game/p8/` or compare tables with the C; there is no place for a regression test of a `game/` module.

Once step 1 has made `make e2e` run the fake LÖVE, a red replay means a bug in the Lua game (or the fake), and the loop would either produce a C test that cannot catch it, or stall. When `src/` goes in step 4, the loop stops working at all.

## Solution

Give the Lua game a regression-test home inside `make test`, and rewrite the four skills of the fix loop around the Lua game and the fake LÖVE:

- A directory of Lua tests of `game/` modules, discovered by wildcard, each file its own parallel make target, all of them prerequisites of `make test`, running under plain LuaJIT with nothing from `src/` or from a C build.
- A small capture reader, so that a frame of a fake-LÖVE capture can be read as text and two captures compared frame by frame: it replaces `make replay`'s framebuffer dump and `make e2e-ref`'s "first divergent frame".
- `check-e2e`, `fix-bug`, `help-me-debug` and `tdd` reproduce with the fake LÖVE (`tmp/fake_love` with a capture and a state path), read a divergence from the Lua state dump and the capture alone, judge a bug against the replays' committed references and the Lua tree's conventions instead of the C, and demand a RED test in the new home (or, for a bug proven in the fake itself, a test of the fake).
- The whole loop is proven once, in a throwaway worktree, on a replay broken by a one-line Lua change.

## User Stories

1. As the `check-e2e` orchestrator, I want to split the red output of the fake-LÖVE `make e2e` into one failure per replay, so that I handle one failure at a time as before.
2. As the `check-e2e` orchestrator, I want to treat a replay where the fake stopped before the last frame as a failure, like a divergence, so that a Lua error is never skipped.
3. As the `check-e2e` orchestrator, I want to pick the cheapest failure by the replay's frame count read from its `.txt` ("<N> frames."), so that I no longer depend on `tmp/frames/<events>.bin`, which only the C writes.
4. As the `check-e2e` orchestrator, I want the bug report's reproduction to be a fake-LÖVE command on `game/`, so that the debug agent reproduces the failure without `src/`.
5. As the `check-e2e` orchestrator, I want the bug report to name a known-good reference commit for the failing replay, so that the debug agent can build a reference capture and state from that commit's `game/` instead of `make e2e-ref`.
6. As the `check-e2e` orchestrator, I want the RED-test constraint to name the Lua regression home, so that `fix-bug` never ships a C unit test for a Lua bug.
7. As the `check-e2e` orchestrator at the commit gate, I want to check that the diff adds or extends a test in the Lua home (or a fake test in `UNIT_TESTS` for a fake bug), so that every e2e bug leaves a test `make test` runs.
8. As the `check-e2e` orchestrator at the commit gate, I want to prove the new Lua test fails on the pre-fix code by running it with plain LuaJIT in a throwaway worktree holding only the diff's `tests/` changes, so that a test passing both ways is refused.
9. As the `check-e2e` orchestrator at the commit gate, I want to time the new test and refuse one over 50 ms or one that runs the fake on a `tests/e2e/` replay, so that `make test` stays fast.
10. As the `check-e2e` orchestrator, I want to keep "never re-baseline, never edit a `.sha256` or `.end-state`", so that a divergence remains a bug.
11. As the `fix-bug` orchestrator, I want the constraints I pass to every sub-agent to say "the replays' references and the Lua tree are the yardstick", not "the C is the reference", so that no agent reads `src/` to judge a fix.
12. As the `fix-bug` orchestrator, I want the constraint "throwaway programs go in `tmp/`, never in `game/` or `tests/`", so that the debug agent's instrumentation never lands in the game.
13. As the `fix-bug` orchestrator, I want the verifier's "pre-fix copy" step to be a run of the new Lua test against the pre-fix `game/`, so that verification needs no C build.
14. As the `fix-bug` orchestrator, I want the sanitizer point to apply only when the proven cause is in the fake's C code, so that it is not demanded for a Lua bug.
15. As a `help-me-debug` agent, I want to reproduce a failure with one fake-LÖVE command that writes a capture and a state dump, so that I establish the symptom as a raw fact.
16. As a `help-me-debug` agent, I want to read frame k of a capture as text, one hex digit per pixel in pixel order, so that I don't misread the packed nibbles.
17. As a `help-me-debug` agent, I want to get the first frame where two captures differ, and how many pixels differ there, so that I locate a divergence without the C.
18. As a `help-me-debug` agent, I want to build a reference capture and state by running the fake on the reference commit's `game/`, extracted under `tmp/`, so that I compare the broken game with a known-good one.
19. As a `help-me-debug` agent, I want to get the Lua state at frame k by running the fake on a copy of the replay cut after frame k, so that I see the game state where the divergence starts.
20. As a `help-me-debug` agent, I want instrumented copies of `game/` to live under `tmp/` and run with the same fake command, so that I can add prints without touching `game/`.
21. As a `help-me-debug` agent, I want the e2e `.txt` notes to remain my description of what each replay does and at which frames, so that I know what a frame should show.
22. As a `tdd` agent, I want "where tests live" to point at the Lua regression home for `game/` code, so that my RED test exercises the Lua the game runs.
23. As a `tdd` agent, I want an example Lua test in the skill's test guide, so that I write tests that drive a module's public functions, not its internals.
24. As a `tdd` agent, I want "where tests live" to still allow a C test in `UNIT_TESTS` for the fake's own code, so that later PRDs of this plan, which change the fake or the Makefile, are not blocked.
25. As a `tdd` agent, I want `make test` to stay the final check, so that a test file run alone is never mistaken for green.
26. As a test author, I want to add a regression test by adding one file to the home, with no Makefile registration, so that the fix stays small.
27. As a test author, I want a shared helper giving `expect`-style assertions and a runner that reports each failing test by name, so that failure messages are uniform and point at the test.
28. As a test author, I want the home to set the module path to `game/` only, with no C modules, so that my test loads exactly what the game ships.
29. As a test author, I want a test file that errors at load to fail its target, so that a broken test is never silently green.
30. As a developer, I want each test file to be its own make target run in parallel by `make test`, so that more regression tests do not serialize the fast loop.
31. As a developer, I want a public target for the whole home with a `## ` help line of at most 20 words, so that I can run only the Lua regression tests.
32. As a developer, I want the home to ship with at least one real regression test of a `game/game/` module, so that the home is exercised from the first commit.
33. As a developer, I want `make test`'s wall time measured before and after under the protocol of `docs/love-port/design.md`, so that the "never slower" rule is checked, not assumed.
34. As a developer, I want the home's targets to run with `src/` moved aside, so that they survive step 4.
35. As a developer, I want the capture reader to be checked by its own fast test in `make test`, so that a misread frame cannot mislead a debug agent.
36. As the plan pilot, I want the step proven by one `check-e2e` run in a throwaway worktree on a replay broken by a one-line Lua change, so that I know the loop works without the C before anything is deleted.
37. As the plan pilot, I want that dry run's evidence (the broken line, the failure, the bug report's reproduction, the regression test, its red/green proof and its run with `src/` moved aside) recorded in the step's commit message, so that the proof survives the throwaway worktree.
38. As the plan pilot, I want this step to leave every `*.sha256` and `.end-state` of `tests/e2e/` unchanged, so that the source of truth is untouched.
39. As a maintainer reading the skills after plan 4, I want no fix-loop instruction to name `tmp/replay`, `make replay`, `make e2e-ref`, `UNIT_TESTS` as the only home, or the C as reference, so that step 5's search finds nothing left in these four skills.

## Implementation Decisions

- **The Lua regression home** is a dedicated directory under the Lua tests (`tests/lua/` gains a `game/` subdirectory), one file per `game/` module tested, named `test_<module>.lua` after the module's file name; a basename shared by two `game/` directories is prefixed by its directory (`test_engine_<module>.lua`). Other files of that directory (the shared helper) do not match `test_*.lua`.
- **Discovery and parallelism**: the Makefile finds the files by wildcard; each file is its own target (one LuaJIT process per file), all listed as direct prerequisites of `test` and grouped under one public target, `lua-test-game`, with a help line. No registration list, no recipe looping over files (design.md rule 1 applied to Lua tests). Each target depends on its file, the helper and the `game/` Lua files, and on nothing built from `src/`.
- **Runtime**: plain `luajit`, module path `game/?.lua` plus the home's own directory for the helper, C module path empty, no `love` global. A test needing the p8 runtime (vram, gfx) sets it up itself as `tests/lua/test_runtime.lua` does.
- **Helper interface**: an `expect(actual, expected, what)` that raises with both values, and a runner taking an ordered list of named test functions, running all of them under `pcall`, printing each failure with its test name and the error, and exiting non-zero on any failure. A file that errors before the runner fails the target by LuaJIT's own exit status.
- **Seed test**: the home ships with one real test of a pure `game/game/` module (implementer's choice, e.g. `economy_formulas`), driving its public functions, to exercise the target end to end.
- **Capture reader**: a LuaJIT script under `tools/`, no C, with two uses: print frame k of a fake capture as 120 lines of 160 hex digits (one palette number per pixel, left to right, unpacking the low nibble first as `frame_capture_pack` packs it), and report the first frame where two captures differ with its count of differing pixels (or that they are identical, or that one is shorter). Frame size is 160 × 120 / 2 = 9600 bytes. It has its own Lua test, its own target in `make test`.
- **Reproduction of one replay** (in every skill): `make fake-love`, then `./tmp/fake_love <game dir> <events> <capture> <state>` with outputs under `tmp/`. This command exists at HEAD and `fake-love` is kept by the plan's target inventory, so the skills do not depend on step 1's single-replay interface. If step 1 provides one (H-1-3), the skills may cite it as an alternative, never as the only way.
- **Reference for a divergence**: the bug report names a reference commit: the current branch's upstream when it exists (prd-autopilot pushes only after a green `check-e2e`), otherwise the last commit that changed the replay's `.sha256`. The debug agent extracts that commit's `game/` under `tmp/` (`git archive`, no worktree needed), runs the fake on it, and first checks that this reference capture matches the committed `.sha256`; if it does not, the reference is not good and it says so (`UNPROVEN:` with the missing observation) rather than compare against it.
- **State at frame k**: by running the fake on a copy of the replay cut after frame k (the method the e2e notes already use for `make replay`); no new interface in the fake or the game.
- **`check-e2e` changes**: failure lines are those of step 1's `make e2e` (H-1-1, H-1-2); frame count from the `.txt`'s "<N> frames."; reproduction and reference as above, `make e2e-ref` and `tmp/replay` removed; RED test constraint = a test in the Lua home (a fake test in `UNIT_TESTS` only when the cause is proven in the fake); gate point 1 checks the file is in the home (or `UNIT_TESTS`); gate point 2 times the test with `luajit` directly; gate point 3 runs the test in a throwaway worktree of `HEAD` with only the diff's `tests/` (and `Makefile`, for a fake test) changes applied — for a Lua test no build is needed; gate point 4 unchanged (`make test` and `make e2e` green). The "never edit a `.sha256`" rule extends to `.end-state`.
- **`fix-bug` changes**: "The C is the reference" becomes: a bug is judged against the replays' committed `.sha256` and `.end-state`, the e2e `.txt` notes, and the Lua tree's own conventions cited from a real site in `game/`; the decompiled cart stays a source of intent only. "Never in `src/` or `tests/`" becomes "never in `game/`, `tests/` or `tools/`". Stage 3's pre-fix check runs the new test against the pre-fix `game/`. The sanitizer point is kept only for a cause proven in the fake's C code.
- **`help-me-debug` changes**: the "Who runs the test" section describes the fake run, the state dump, the cut replay, the capture reader and the reference game; instrumented copies of `game/` under `tmp/`; never change `game/` or `tests/`.
- **`tdd` changes**: "Where tests live" names the Lua home for `game/` code and keeps C tests in `UNIT_TESTS` for the fake's own code and the tools; the test guide (`tests.md`) gets a Lua example in the home's style; `mocking.md`'s "anything in `src/game/` or `src/engine/`" becomes the `game/` modules. `interfaces-design.md` and `refactoring.md` are left to step 5 (no `src/` path, only C examples).
- **What "make test runs it without `src/`" means in this step**: `make test` as a whole still builds the C unit tests until step 4, so the verification is that the regression test's target (and `lua-test-game`) is a prerequisite of `test` and goes green with `src/` moved aside and `tmp/` emptied. Decided under the guideline rule "make the plan work, take the smaller change".
- **The dry run**: in a throwaway `git worktree` under `tmp/`, one line of a `game/game/` module is changed so that at least one replay diverges or fails in `make e2e` while `make test` stays green (a break `make test` already catches does not exercise the net). `check-e2e` is run once there, unattended, until `fix-bug` has a fake-LÖVE reproduction in its report and a regression test in the home has gone red then green; the commit gate may accept inside the worktree. The regression test is then run with `src/` moved aside. The worktree is removed; nothing from it is merged. The evidence goes into the step's commit message and the plan's step notes.
- **Speed**: `make test` is measured under design.md's protocol (rule 4, both baselines) after the home, the seed test and the capture reader's test are added; the result is recorded in the plan's step notes. Over the gate → the targets are reworked (rule 1 and 2), never moved out silently.
- **No CLAUDE.md or design.md rewrite here**: the Layout line about `tests/` and the design notes are step 5's; this step touches only the Makefile, the new tests and tool, and the four skills (with `tdd`'s two guides).

## Implementation Chunks

1. **The Lua regression home**: helper, runner, per-file parallel targets, `lua-test-game`, the seed test; green with `src/` moved aside.
2. **The capture reader** and its test target.
3. **The skills**: `help-me-debug`, `tdd` (with `tests.md`, `mocking.md`), `fix-bug`, `check-e2e`, rewritten around chunks 1 and 2 and step 1's `make e2e`.
4. **Measure and prove**: the `make test` timing protocol, then the dry run in a throwaway worktree; evidence into the commit message and step notes.

## Testing Decisions

- A good test here drives a `game/` module through its public functions and checks what it returns or what it leaves in state the game reads; it never inspects locals, never replays a `tests/e2e/` file, and runs well under 50 ms.
- Tested: the home itself (the seed test proves a passing file is green; a deliberately failing file, made in a throwaway copy during development, proves the target goes red with the test's name — not committed), and the capture reader (a Lua test building small captures under `tmp/` and checking frame printing, nibble order, first-difference, identical and shorter cases).
- The skills are not unit-tested: they are proven by the dry run (chunk 4), which is the step's verification.
- Prior art: `tests/lua/test_runtime.lua` (plain-LuaJIT tests of `game/p8/`, its `expect` and module path), the `FAKE_LOVE_CHECKS` pattern and design.md's rules for parallel checks in `make test`, `tools/fake_love/frame_capture.h` for the capture layout.
- Every `*.sha256` and `.end-state` in `tests/e2e/` stays unchanged; `make test` and `make e2e` green at the end.

## Out of Scope

- Step 1's work: switching `make e2e` and `make end-state` to the fake, the N-seconds rule.
- Freezing the C references of `lua-test-fixed`, `lua-test-modules` and the other checks fed by `src/` (step 3); porting `make state`, `make replay`, `e2e-baseline`, `end-state-baseline` (the inventory lists them as ported; whichever of steps 1 and 3 owns them, not this one).
- Deleting anything: `make e2e-ref`, `tmp/replay`, the C unit tests and `src/` stay until step 4.
- Other skills (`prd-autopilot`, `plan-pilot`, `git-commit`, `warn-review`, …), `CLAUDE.md`, `README`, design.md: step 5.
- New e2e replays; a frame-k state dump inside the fake or the game.

## Further Notes

### Hypotheses on step 1

- **H-1-1**: after step 1, `make e2e` runs `tmp/fake_love` on `game/` for each replay of `tests/e2e/` and reports a divergence per replay with the `frames_check.sh` line `frames: DIVERGENT <events> -- …` followed by the expected and actual hashes, as `make love-e2e` does today.
- **H-1-2**: a replay where the fake stops before the last frame is reported by `make e2e` with a per-replay `FAILED <events>` line followed by the fake's stderr indented, as `love_e2e_report.sh` does today (prefix possibly `e2e:` instead of `love-e2e:`); `check-e2e` treats it as one failure.
- **H-1-3**: step 1 keeps or provides a one-replay mode (`make end-state EVENTS=<events>` on the fake) writing the Lua state and capture under `tmp/`; the skills cite it only as an alternative to the direct `tmp/fake_love` command.
- **H-1-4**: `make e2e`'s per-replay outputs (capture, hash, error, and state for `make end-state`) land under a `tmp/` directory per target, like `tmp/love-e2e/tests/e2e/<name>.json.bin` today; the skills read the capture there only as a shortcut and otherwise rerun the fake themselves, so a different path changes one line.
- **H-1-5**: `make e2e` no longer prints the hint `make e2e-ref`; the C `e2e-ref` target may still exist after step 1 but is not used by the fix loop.
- **H-1-6**: the fake still takes `<game dir> <events> <capture> [<state>]`, and the capture layout (9600 bytes per frame, low nibble first) is unchanged by step 1.
- **H-1-7**: `make test` after step 1 still has the shape of HEAD (parallel prerequisites, `FAKE_LOVE_CHECKS`), so the new targets join it the same way, and design.md's timing protocol still applies.

If a hypothesis is false when this PRD is implemented, the skill wording follows what step 1 actually did; the decisions above do not change.
