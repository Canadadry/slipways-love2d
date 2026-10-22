---
title: "make e2e and make end-state run the Lua game in the fake LÖVE"
description: "The e2e net and the end-state check still replay the C game, which plan 4 deletes, while the Lua game already matches all 20 replays in the fake LÖVE."
status: needs-triage
---

## Problem Statement

`make e2e` replays the 20 files of `tests/e2e/` through the C game (`tmp/replay`) and compares each capture's sha256 with its committed `.json.sha256`. `make end-state` does the same with the C game's last-frame state against each `.end-state`, over the 20 replays and the 5 `worst-case/` games. The Lua game, the one that survives plan 4, is checked only by two side targets, `make love-e2e` and `make love-e2e-state`. They sit outside `make test-all` and CI, and each runs its own 20 fake-LÖVE runs. So the net that `check-e2e`, `prd-autopilot` and CI rely on still guards the C game, and nothing in CI guards the Lua game. The C can't be deleted while the e2e net depends on it.

`CLAUDE.md` also says "never make `make e2e` slower". The C e2e takes about 1 s. The fake LÖVE on the same 20 replays takes about 10 s on 4 cores. The fake LÖVE e2e can't become `make e2e` until that rule is replaced by the measured "under N seconds" rule that plan 4 calls for.

## Solution

`make e2e` becomes the fake-LÖVE e2e. It runs `tmp/fake_love` with the Lua game on each of the 20 replays, one make job per replay, and starts the longest replays first. It keeps today's replay-limits check (`tools/e2e_limits.sh`) and compares each capture's sha256 with the committed `.json.sha256`. Every replay is reported as matches, DIVERGENT or FAILED, followed by one count line. `make end-state` checks the same runs' Lua end states against the committed `.end-state` files. Its single-replay form (`EVENTS=`, `REFERENCE=`) runs the fake too. `make e2e-baseline` and `make end-state-baseline` write their references from those Lua outputs. `make love-e2e` and `make love-e2e-state` are absorbed and disappear.

Before the change is committed, the new `make e2e` is timed over 6 cold runs. The median sets N, and `CLAUDE.md` swaps "never make `make e2e` slower" for "`make e2e` runs under N seconds" in the same change. The C stays in the tree, untouched, and still serves `e2e-ref`, `state`, `replay`, `full-game`, `sanitize-net` and the C unit tests until later steps.

## User Stories

1. As a developer, I want `make e2e` to replay the Lua game in the fake LÖVE, so that the e2e net guards the game that survives plan 4.
2. As a developer, I want `make e2e` to keep comparing each capture with the committed `.json.sha256`, so that the source of truth is unchanged.
3. As a developer, I want every `*.sha256` and `*.end-state` in `tests/e2e/` to stay byte-identical in this change, so that switching the producer can't hide a regression.
4. As a developer, I want `make e2e` to keep the replay-limits check (frame limits, companion `.txt` and `.json.sha256`, pad-only input, the "N frames." note), so that a malformed replay still fails the net.
5. As a developer, I want the limits check and the capture comparison to both run before `make e2e` concludes, so that one red run shows every problem.
6. As a developer, I want each replay reported on one line (matches, DIVERGENT with the expected and actual hash, FAILED with the fake's message indented below), so that I can read a red run without opening files.
7. As a developer, I want a final count line `e2e: <m> match, <d> diverge, <f> failed, of <n> replays in tests/e2e`, so that I see the overall result at a glance.
8. As a developer, I want `make e2e` to exit 0 only when every replay matches and the limits check passes, so that green means green.
9. As a developer, I want a DIVERGENT replay to tell me that `make e2e-baseline` resets the reference when the change is intended, without pointing at the C-only `make e2e-ref`, so that the hint matches the game being checked.
10. As a developer, I want every replay rerun on every `make e2e`, so that a Lua edit, which make can't see, is never hidden behind a stale capture.
11. As a developer, I want the longest replays started first, so that the parallel run ends as early as the cores allow.
12. As a developer, I want `make e2e` to build `tmp/fake_love` itself when it is missing, so that it works on a fresh checkout.
13. As a developer, I want `LOVE_E2E_GAME` (default `game`, `%` replaced by the replay's file name) to keep working on `make e2e`, so that a verification can still point the net at a stub game.
14. As a developer, I want `make end-state` to compare each Lua end state with its committed `.end-state`, with today's normalization, so that the state check also follows the surviving game.
15. As a developer, I want `make end-state` to report each replay as matching, differing (with the `-` reference and `+` Lua lines) or FAILED, followed by a count line `end-state: <m> match, <d> differ, <f> failed, of <n> replays in tests/e2e`, so that its output reads like `make e2e`'s.
16. As a developer, I want `make e2e end-state` in one invocation to run each replay only once, so that checking both costs no more than one fake run per replay.
17. As a developer, I want `make end-state EVENTS=<events> [REFERENCE=<file>]` to run that one file in the fake LÖVE and check its state, so that I can check a single replay or a `worst-case/` game against its reference.
18. As a developer, I want `make e2e-baseline` to set each `.json.sha256` from the Lua captures, so that an intended rendering change is recorded from the game that `make e2e` checks.
19. As a developer, I want `make e2e-baseline` to refuse to write a reference for a replay the fake failed on, so that a crash never becomes a reference.
20. As a developer, I want `make end-state-baseline` to write each missing `.end-state` from the Lua state and never overwrite one, so that it behaves as it does today with the new producer.
21. As a developer, I want `make love-e2e` and `make love-e2e-state` to be gone, so that `make help` lists one e2e and one end-state check, not two of each.
22. As a developer, I want `make test-all` to keep meaning `make test` plus `make e2e`, so that the full suite now includes the Lua e2e.
23. As a CI maintainer, I want CI's `make test-all` to run the fake-LÖVE e2e, so that every push guards the Lua game against the 20 references.
24. As a developer, I want `make e2e`, `make end-state` and both baselines to take `tmp/make.lock`, so that two of them never write the shared fake outputs at once.
25. As a developer, I want `CLAUDE.md` to state the rule "`make e2e` runs under N seconds" with the measured N, in place of "never make `make e2e` slower", so that the speed rule covers the e2e that actually runs.
26. As a developer, I want N measured from 6 cold runs of the new `make e2e` (median), with the measurement recorded, so that the rule rests on a number someone can reproduce.
27. As a developer, I want the `## ` help line of `make e2e` and the time in `CLAUDE.md`'s Build section to state the new duration, so that the docs don't promise "~1 s".
28. As a developer, I want `CLAUDE.md` and `docs/love-port/design.md` to describe `make e2e` and `make end-state` as the fake-LÖVE checks and to drop `love-e2e` and `love-e2e-state`, so that the docs describe only what is true.
29. As a maintainer of the C until plan 4 deletes it, I want `make e2e-ref`, `make state`, `make replay`, `make full-game` and `make sanitize-net` to keep working on the C unchanged, so that later steps can port or delete them on their own.
30. As a reviewer, I want `make e2e` and `make end-state` green with `src/` still present, so that this step proves the switch without depending on the deletion.

## Implementation Decisions

- **One fake run per replay feeds both checks.** Today's two run rules (`tmp/love-e2e/` capture-only and `tmp/love-e2e-state/` with state) merge into one per-replay rule under a single output directory under `tmp/`. That rule runs `tools/love_e2e_run.sh` with the state path, so each run leaves the capture, its hash and the state, or an error file. `e2e`, `end-state`, `e2e-baseline` and `end-state-baseline` all depend on these per-replay targets. The rule stays forced (FORCE): make can't see a Lua edit. Measured cost: writing the state doesn't change the run time (both side targets took about 10 s).
- **The run script loses its 7-argument form.** No caller passes no state path any more, so `tools/love_e2e_run.sh` always takes the state argument. Its removal-before-run and error-file contracts stay.
- **`make e2e`** depends on the per-replay hashes, on `tools/e2e_limits.sh` with `tools/e2e_long_exceptions.txt`, and on the report script. It runs the limits check and the report, both always, and fails if either fails. It no longer depends on `tmp/replay` or the C capture rule.
- **Replays start longest first.** Today's alphabetical order starts the longest replays last (`pad_tech_costs`, ~4 s alone, ~558 frames), so the run takes about 10 s on 4 cores for ~23 s of total work. The per-replay prerequisites of `e2e` are listed largest replay file first, which follows the frame count. The order comes from the files, so a new replay needs no list edit, and computing it costs at most one cheap process when the Makefile is read.
- **Report scripts keep their file names; their output takes the target names.** `tools/love_e2e_report.sh` prints with the prefix `e2e:` and `tools/love_e2e_state_report.sh` with `end-state:`. The count lines become `e2e: <m> match, <d> diverge, <f> failed, of <n> replays in tests/e2e` and `end-state: <m> match, <d> differ, <f> failed, of <n> replays in tests/e2e`. Renaming the files is left to the docs pass of step 5.
- **`tools/frames_check.sh` has one hint behavior.** On a DIVERGENT replay it prints the expected and actual hash and "if the divergence is intended: make e2e-baseline". It no longer prints `make e2e-ref` (that replays the C, which isn't the game checked) or "the .json.sha256 is the C game's reference". On a missing reference it keeps the `make e2e-baseline` hint. With no caller left, `--no-baseline-hints` goes.
- **`make e2e-ref` is left as it is.** It still builds its own C captures under `tmp/frames/` and stays usable until step 4 deletes it (plan rule). Nothing in the new `make e2e` points to it.
- **`make end-state` with no `EVENTS`** checks the 20 replays of `tests/e2e/`, not the `worst-case/` games it covered with the C. The step states "on the 20 replays". The 5 full games take about 6,000 frames each, roughly 45 s each in the fake, and plan 3 never checked their Lua end states. Their committed `.end-state` files stay untouched. They can still be checked one at a time through the single-replay form, and their fate belongs to steps 3–4 (the `full-game`/`state` inventory entries).
- **`make end-state EVENTS=<events> [REFERENCE=<file>]`** runs the fake once on that file into a scratch directory under `tmp/` and checks its state with `tools/end_state.sh check` against `REFERENCE` or `<events>.end-state`. A fake failure is reported as FAILED with its message, the same way as in the default form.
- **`make e2e-baseline`** copies each replay's current Lua hash to its `.json.sha256`. A replay with an error file gets no reference, and the target fails naming it.
- **`make end-state-baseline`** runs `tools/end_state.sh baseline` over the 20 replays' Lua states. It writes only missing references, as today. A replay the fake failed on gets nothing and fails the target.
- **Ported with this step:** `e2e-baseline` and `end-state-baseline`. They write the references `e2e` and `end-state` read, and leaving them on the C would make an intended change recorded from a different producer than the one checked.
- **Not ported in this step:** `state` and `replay`. They don't read the e2e outputs, and switching `e2e` doesn't make them inconsistent. They are the fix loop's tools, which step 2 rewires. `end-state` no longer uses the C state runs, which stay only for `make state`'s peaks mode.
- **`love-e2e` and `love-e2e-state` are removed** from the Makefile, `.PHONY` and `make help`.
- **Lock.** `LOCKED_GOALS` loses `love-e2e` and `love-e2e-state` and gains `end-state`, `e2e-baseline` and `end-state-baseline`, all of which write the shared fake outputs. The `CLAUDE.md` lock line lists the new set.
- **Cold-run measurement.** Measure after the switch is otherwise complete. `tmp/fake_love` is already built, since `make test` builds it and the measurement times the e2e, not the compile. Each of the 6 runs first removes the e2e output directory, then times one `make e2e` alone (wall clock, nothing else running). The median of the 6 is the measurement.
- **N** is the median rounded up to the next whole second, plus 1 s of headroom. The machine's core count, the 6 times and the median go into the commit message and the plan's step notes. The `CLAUDE.md` "What we don't do" bullet becomes: never let `make e2e` run longer than N seconds, and a slow check goes in a separate target outside `make test-all`. The "never make `make test` slower" half of the bullet stays as it is.
- **Docs updated in the same change**, limited to what this step makes false. In `CLAUDE.md`: the speed rule, the Build section's `make e2e` duration, the two `love-e2e` bullets (dropped), and the lock line. In `docs/love-port/design.md`: the "Out of `make e2e`" principle, the `make love-e2e` and `make love-e2e-state` sections (now describing `make e2e` and `make end-state`), and the "Rules for the fork" line on N. Every other C mention in docs and skills is step 5's work.
- **CI** needs no edit. `make test-all` already runs after `tools/love_install.sh`, which installs LuaJIT for the fake.

## Implementation Chunks

1. **Shared fake runs and `make e2e`.** Merge the run rules into one per-replay rule with state. Make the run script take the state path always, rewire `e2e` (limits check plus report, longest replay first), switch the report prefix to `e2e:`, give `frames_check.sh` its single hint behavior, port `e2e-baseline`, and remove `love-e2e`. Update `test_love_e2e` and `test_frames_check`. Done when `make test` and `make e2e` are green.
2. **`make end-state` on the shared runs.** Rewire the default and single-replay forms, port `end-state-baseline`, switch the state report prefix to `end-state:`, remove `love-e2e-state`, and update `LOCKED_GOALS`. Update `test_love_e2e`. Done when `make test`, `make e2e` and `make end-state` are green.
3. **Measure and write the rule.** Run the 6 cold runs and set N. Update the `e2e` help line, the `CLAUDE.md` speed rule, Build section and lock line, and `docs/love-port/design.md`. Done when `make test-all` and `make end-state` are green and `CLAUDE.md` holds the N-seconds rule.

## Testing Decisions

- A good test drives a script through its arguments on real or hand-written files and checks its exit status and printed lines. It never checks how the Makefile is laid out.
- `tests/test_love_e2e.c` stays the test of the run and report scripts. It is updated for the run script's single (8-argument) form, which drops the "seven arguments write no state" case, and for the `e2e:`/`end-state:` prefixes and count lines. It keeps covering stale outputs removed, FAILED with the fake's message, matches/diverges/differs counting, and usage errors.
- `tests/test_frames_check.c` is updated for the single hint behavior. A divergence names `make e2e-baseline` and not `make e2e-ref`, a missing reference is reported and not compared, and a missing current hash fails. The `--no-baseline-hints` cases go with the option.
- `tests/test_e2e_limits.c` and `tests/test_end_state.c` don't change: `tools/e2e_limits.sh` and `tools/end_state.sh` keep their contracts.
- The Makefile wiring is proven end to end, as the step states: `make e2e` and `make end-state` green on the 20 replays with `src/` present, `make test-all` green, and every `*.sha256`/`*.end-state` in `tests/e2e/` unchanged (`git diff` empty there). The red path is checked once by hand and not committed. A throwaway game copy with a one-line drawing change, passed through `LOVE_E2E_GAME`, must make `make e2e` print DIVERGENT and the count line and exit non-zero.
- No new test binary. Every changed test stays in `make test` and keeps its fake-side registration, so `make test` gets no slower.
- Prior art: `tests/test_love_e2e.c` (scripts on the real `tmp/fake_love` and hand-written fixtures), `tests/test_frames_check.c` and `tests/test_end_state.c` (shell scripts driven through `system()` with a log file).

## Out of Scope

- Porting `state` and `replay` to the fake LÖVE, and pointing `check-e2e`, `fix-bug`, `tdd` and `help-me-debug` at the fake (step 2). Until then `check-e2e` still names `tmp/frames/` and `make e2e-ref` in its diagnosis instructions; it's only reached on a red `make e2e`.
- Freezing the fixed-point, table and generator references, and the `src/`-free dry run (step 3).
- Deleting `e2e-ref`, `full-game`, `sanitize-net`, the C capture rules, `tmp/replay` or anything under `src/` (step 4).
- Checking the `worst-case/` end states in the default `make end-state`, and deciding the fate of their `.end-state` files (steps 3–4).
- Renaming `tools/love_e2e_*.sh`, `tests/test_love_e2e.c` or `LOVE_E2E_GAME`, and rewriting docs and skills beyond what this step makes false (step 5).
- Speeding the fake or the Lua game beyond scheduling replays longest first.
- Audio, as everywhere in the port.

## Further Notes

- Timings measured while writing this PRD (4 cores, not cold): `make love-e2e` 9.9 s, `make love-e2e-state` 10.0 s. Single replays run serially take 0.18 s (`pad_title`) to 4.0 s (`pad_tech_costs`), about 23 s in total. Longest first should bring `make e2e` near the plan's 5 s goal on 4 cores. The cold measurement, not these numbers, sets N.
- The plan's target inventory lists `e2e-baseline` and `end-state-baseline` as ported and `state` and `replay` as ported later. This PRD follows that inventory and changes no category.
- `progress.md`'s step 1.1 box is ticked by the run, not by this PRD.
