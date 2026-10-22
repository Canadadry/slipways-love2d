---
title: "make love-e2e-state: check each Lua end state against its .end-state"
description: "Nothing compares the Lua game's end state in the fake LÖVE with the C reference states, so a port can match every capture yet hold a wrong game state."
status: needs-triage
---

## Problem Statement

Plan 3 needs two checks on the Lua game of `game/`: its captures against the
`.json.sha256` of the 20 replays of `tests/e2e/` (`make love-e2e`, which
exists and reports 20/20 since plan 2), and its end state against each
replay's `.end-state` (the C game's last-frame state, normalized by
`tools/end_state.sh`). The second check does not exist. `tmp/fake_love`
already writes `debug_state()` to an optional fourth argument, but the
runner `tools/love_e2e_run.sh` never passes one and no target compares the
result. Plan 3's "Done when", plan 4's start condition and plan 3's step 4
all name `make love-e2e-state`; until it exists, a Lua game whose pixels
agree but whose credits, techs or planets differ goes unnoticed.

## Solution

A new make target, `make love-e2e-state`, runs the fake LÖVE on every
replay of `tests/e2e/` with a state path, then checks each Lua end state
against the replay's committed `.end-state` through `tools/end_state.sh
check`. Each replay is reported as matching, differing (with the differing
lines from `end_state.sh`) or FAILED (the fake stopped, its message
indented below), then a count line. Exit 0 only when every replay matches.
The step is then run on the current tree: `make love-e2e-state` and
`make love-e2e` must both report 20/20, and a deliberately wrong reference
must be reported as differing.

## User Stories

1. As the plan pilot, I want `make love-e2e-state`, so that I can tell whether the Lua game ends every replay in the C game's state.
2. As the plan pilot, I want one count line `love-e2e-state: <m> match, <d> differ, <f> failed, of <n> replays in tests/e2e`, so that "20/20" is read from one line.
3. As the plan pilot, I want the exit status to be 0 only when every replay matches, so that a script can branch to Phase 2 on failure.
4. As a developer, I want every replay run and reported even when one fails, so that one stop does not hide the others.
5. As a developer, I want a differing replay to show the `-` reference and `+` Lua lines, so that I see which field diverged without rerunning.
6. As a developer, I want a replay where the fake stopped reported FAILED with the fake's message, so that a Lua error is not mistaken for a state difference.
7. As a developer, I want a game without `debug_state` reported FAILED with the fake's message naming `debug_state`, so that the cause is obvious.
8. As a developer, I want the state compared with the same normalization as the C `make end-state`, so that both sides are judged by one rule (camera, zones, screen positions dropped).
9. As a developer, I want `LOVE_E2E_GAME` honoured by the new target as by `make love-e2e`, including its `%` per-replay substitution, so that stubs can verify the check.
10. As a developer, I want every replay rerun on every invocation, so that a Lua edit make cannot see is always taken into account.
11. As a developer, I want stale states, captures, hashes and error files removed before each run, so that an older result never looks current.
12. As a developer, I want `make love-e2e` unchanged (no state path, no `debug_state` needed), so that the stubs plan 1 used to prove it still work.
13. As a developer, I want the runner to keep accepting its seven arguments, so that `make love-e2e` and its tests keep working without edits.
14. As a developer, I want the target to take `tmp/make.lock` through `flock`, so that two runs never race over `tmp/`.
15. As a developer, I want the target outside `make test`, `make e2e`, `make test-all` and CI, so that the fast loop stays fast.
16. As a developer, I want `make help` to list the target with a one-line `## ` description of at most 20 words.
17. As a developer, I want the `.end-state` files never written by this target, so that the C game stays the reference.
18. As a maintainer, I want the runner's and the report's behaviour covered by unit tests in `make test`, on fixture games, so that a regression is caught without running the real game.
19. As a maintainer, I want `docs/love-port/design.md` to describe `make love-e2e-state` beside `make love-e2e`, so that the docs say what is true.
20. As a maintainer, I want CLAUDE.md's Build section to list the new target among the flock-held ones and the ones outside CI, so that the rules stay accurate.
21. As the plan pilot, I want the step's verification recorded (both counts and the wrong-reference check), so that the step note can decide between "Done when" and Phase 2.

## Implementation Decisions

- **Runner (`tools/love_e2e_run.sh`)**: gains an optional eighth argument, `<state>`. With eight arguments it removes `<state>` along with the other outputs before the run and passes it to the fake as its fourth argument; on a failed run it leaves no state (the fake already removes it; the runner removes it too so the contract does not rest on the fake). Seven arguments behave exactly as today. Any other count stays a usage error (exit 2). The usage comment and message name the optional argument.
- **Separate runs, not shared with `make love-e2e`**: the new target runs the fake again per replay, into its own directory (`tmp/love-e2e-state/tests/e2e/`: `<name>.json.bin`, `.json.sha256`, `.json.error`, `.json.state`). Passing a state path in `make love-e2e` would make `debug_state` mandatory there and break its stub verifications; the double run costs seconds, outside every fast target.
- **Report (new `tools/love_e2e_state_report.sh`)**: arguments in quadruples `<replay> <state> <reference> <error file>`, the shape of `tools/love_e2e_report.sh`. A replay with an error file is FAILED, its lines on stderr prefixed `love-e2e-state: FAILED <replay> -- the fake stopped before the last frame.` then the error indented. The other triples go to one `sh tools/end_state.sh check` call, its output passed through. Differing replays are counted as the distinct `<replay>` names in its `end-state: <replay> <reason>` lines (replay paths hold no space); matches = replays − failed − differing. The count line is `love-e2e-state: <m> match, <d> differ, <f> failed, of <n> replays in tests/e2e`. Exit 0 only when every replay matches; exit 2 on an argument count not a multiple of four. `end_state.sh` is called through `sh` (it is not executable) and is not modified.
- **Makefile**: per-replay rules `$(LOVE_E2E_STATE_DIR)/%.state` (forced, like the love-e2e hashes) call the runner with eight arguments and the same `LOVE_E2E_GAME` substitution; `love-e2e-state` depends on them, on the report script and on `tools/end_state.sh` (not on `tools/frames_check.sh`, which it does not use); references are `<replay>.end-state`. The goal is added to `LOCKED_GOALS`, declared `.PHONY`, and carries a `## ` help line of at most 20 words, e.g. "runs tmp/fake_love on every tests/e2e/ replay and checks each Lua end state against its .end-state". It is not a prerequisite of `test`, `e2e`, `test-all`, and not added to CI.
- **No reference override**: the "deliberately wrong reference" verification is done by the unit test (a wrong reference file) and, on the real tree, by temporarily editing one line of a committed `.end-state`, running the target, seeing `differs` with the edited line, then `git checkout` of that file. No variable is added for it.
- **Docs**: `docs/love-port/design.md` gets a `make love-e2e-state` section next to `make love-e2e` (outputs, outcomes, count line, lock, outside test/e2e/test-all/CI); the `make love-e2e` section keeps saying it passes no state path. CLAUDE.md's Build section adds the target to the flock list and to the targets outside `make test`, `make e2e`, `make test-all` and CI.
- **Outcome on the tree**: the implementer runs `make love-e2e-state` and `make love-e2e` on the finished tree and states both count lines and the wrong-reference result in the commit message. A Lua state that differs is not fixed in this step: it is the evidence that sends the plan to Phase 2.

## Implementation Chunks

1. Runner: optional eighth `<state>` argument, with its unit tests.
2. Report script `tools/love_e2e_state_report.sh`, with its unit tests on hand-written state and reference files.
3. Makefile target, lock, help line, design.md and CLAUDE.md; then the run on the tree (both checks, plus the wrong-reference check) recorded in the commit message.

## Testing Decisions

- Tests drive the shell scripts through their arguments only, and assert on files left behind, the printed lines and the exit status — never on how the scripts work inside.
- Tested in `tests/test_love_e2e.c` (already in `make test` through `FAKE_LOVE_CHECKS`, already a prerequisite on the runner and report scripts; add the new report script and `tools/end_state.sh` to its prerequisites). Prior art: that file's runner tests (stale outputs, finished and stopped runs) and report tests (`prepare_report_files`, `assert_log_holds`), and `tests/test_end_state.c` for state/reference text.
- Runner tests: eight arguments on the `minimal` fixture (its `debug_state` returns `updates <n>`) leave the state with the expected text, the capture and the hash, no error, and a stale state is replaced; eight arguments on a fixture without `debug_state` (`e2e_fixed` or `no_debug_state`) leave only the error file, naming `debug_state`, and no state; seven arguments still leave no state file.
- Report tests on hand-written files (lines in `end_state.sh`'s kept format, e.g. `clock year 3401 month 1`, `difficulty 1`): a matching replay, a wrong reference reported `differs` with its `-`/`+` lines, a FAILED replay with its message, the count line for each mix, exit 0 only when all match, exit 2 on a bad argument count.
- Each test runs in milliseconds on fixtures: `make test` must not get slower in any noticeable way. The real 20-replay run is never part of `make test`.

## Out of Scope

- Fixing any Lua state or capture difference found (Phase 3).
- Per-frame state dumps and `make e2e-diff` (plan 3 steps 2 and 3).
- The step notes for steps 2–4, the `progress.md` ticks and the deletion of the step-2 PRD in `docs/prd/next/`: the pilot's bookkeeping after this step.
- Any change to `.end-state` or `.json.sha256` files, to `tools/end_state.sh`, or to the C `make end-state` / `make end-state-baseline` targets.
- Adding the target to CI or to `make test-all`.
- Audio.

## Further Notes

- The fake already removes the state on a failed run and fails when `debug_state` is missing or returns a non-string; this step relies on that and does not change the fake.
- `make love-e2e-state` and `make love-e2e` hold the same lock, so they run one after the other, never side by side.
- If both checks report 20/20, the pilot goes straight to plan 3's "Done when"; a replay failing either check sends the plan to Phase 2.
