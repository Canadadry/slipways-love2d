---
name: check-e2e
description: Internal — called by prd-autopilot after each PRD, and by plan-pilot after a "skill:" step. Runs make e2e and drives every red replay to green through fix-bug, one failure at a time.
---

# check-e2e

`make test` is the dev loop: unit tests, Lua tests and static checks, seconds. `make e2e` is the net: every e2e of `tests/e2e/`, played from the title screen by the Lua game in `tmp/fake_love`, captured and compared to its `.json.sha256` reference, about 7 seconds. It runs once, at the end — here.

A red e2e is always a bug. An intended rendering change is out of scope for this skill: never run `make e2e-baseline` or `make end-state-baseline`, never edit a `.sha256` or `.end-state` reference to get green.

Every e2e bug leaves behind a test that catches it in `make test`. That is the point: the next time, the dev loop catches it in seconds instead of the net catching it later.

## Rule #1 — never wait on the human

No one answers during this loop. A `QUESTION:` from any sub-agent goes to a fresh `guideline` agent, never to the user. The only way back to the human is the stop after 3 failed attempts (step 5).

## Rule #2 — you are the human at fix-bug's commit gate

`fix-bug` stops once, at its `git-commit` proposal, for a human go-ahead. Here, you give or refuse it, after running the checks of step 4 yourself. Never accept on the strength of an agent's report.

## Loop

### 1. Run the net
Run `make e2e`. Green → report `E2E-GREEN:` with the list of `fix:` commits made during this run, and stop.

### 2. Split the red into failures
One failure = one replay, reported either way:

- `frames: DIVERGENT <events>` and its `frames:` lines below (expected, actual, and a `make e2e-baseline` hint never to follow): the capture differs from the reference.
- `e2e: FAILED <events>` and its indented `e2e:` lines below: the fake stopped before the last frame (a Lua error, a crash). Never skipped: it is a failure like a divergence.

Keep the full output of each.

Take the failure whose replay has the fewest frames: it is the cheapest to diagnose. The count is the "<N> frames." of the first paragraph of the replay's `.txt` (`tests/e2e/<name>.json` → `tests/e2e/<name>.txt`):

```
awk 'NF == 0 { exit } { printf "%s ", $0 }' tests/e2e/<name>.txt | grep -o '[0-9]* frames\.'
```

Handle it alone; the others wait for the next `make e2e`, since a fix often repairs more than one.

### 3. Invoke fix-bug on it
Invoke the `fix-bug` skill in place. Its argument is a bug report you write, containing:

- **Symptom**, verbatim: the `make e2e` output for this failure, nothing paraphrased.
- **Reproduction**, on the Lua game:
  - `make fake-love`, then `./tmp/fake_love game <events> tmp/debug/<name>.bin tmp/debug/<name>.state` (`mkdir -p tmp/debug` first): the capture and the Lua state after the last frame. A failed replay exits non-zero with the error on stderr; a divergent one exits 0, and the capture's `sha256sum` (macOS: `shasum -a 256`) differs from `<events>.sha256`. `make e2e` already left its capture at `tmp/love-e2e/<events>.bin`.
  - Alternative: `make end-state EVENTS=<events>` writes the capture and state to `tmp/end-state/<name>.json.bin` and `.state` and checks the end state against `<events>.end-state`.
  - The reference commit, a known-good `game/` for this replay: written into the report as a hash: `git rev-parse --verify -q '@{upstream}'`; with no upstream, the first of `git log -20 --format=%H HEAD -- game/` whose `game/`, run by the fake on this replay, gives a capture whose sha256 equals `<events>.sha256` (extract and run each as `help-me-debug`'s "The reference game" says); none matches → no good reference, say so in the report. Every `.sha256` predates the Lua game, so the commit that last set it is never a reference.
  - The `.txt` next to each e2e says what it does and at which frames.
- **Constraints on the fix**, to be carried into every stage:
  - The RED test is a **Lua test run by `make test`**: `tests/lua/game/test_<module>.lua`, named after the `game/` module it drives, found by wildcard (no Makefile registration). Only when the proven cause is in the fake's C code (`tools/fake_love/`): a C test in the Makefile's `UNIT_TESTS`. Never a new e2e in `tests/e2e/`.
  - Its cost is **negligible**: it builds the minimal state that reaches the cause, it does not replay a recorded session nor run the fake on a `tests/e2e/` replay, and it runs well under 50 ms. Needing a long event sequence to go red is the sign the real cause has not been found — go back to diagnosis, don't ship it.
  - The fix turns **both** the new test and `make e2e` green.
  - A divergence is a bug. Never re-baseline, never edit a `.sha256` or `.end-state`.
- **Already eliminated**, on a retry (step 5): every hypothesis the previous attempts disproved, and why their commit was refused.

### 4. The commit gate — check, then accept or refuse
When `fix-bug` proposes its commit, check each point yourself, with commands you run:

1. The diff adds (or extends) a `tests/lua/game/test_<module>.lua`. For a cause proven in the fake: a `tests/test_<name>.c` whose binary is in `UNIT_TESTS`.
2. That test does not replay any file of `tests/e2e/` nor run `tmp/fake_love`, and runs under 50 ms: `time (LUA_PATH='game/?.lua;tests/lua/game/?.lua' LUA_CPATH= luajit tests/lua/game/runner.lua tests/lua/game/test_<module>.lua)`, or `time ./tmp/<test>` for a fake test.
3. It **fails on the pre-fix code**. Extract `HEAD` into a throwaway copy and lay the working tree's `tests/` over it — `rm -rf tmp/gate && mkdir -p tmp/gate && git archive HEAD | tar -x -C tmp/gate && cp -R tests tmp/gate/` (for a fake test, also `cp Makefile tmp/gate/`) — then `make -C tmp/gate lua-test-game LUA_GAME_TESTS=tests/lua/game/test_<module>.lua` (fake test: `make -C tmp/gate unit-run-test_<name>`) must fail. Then `make lua-test-game LUA_GAME_TESTS=tests/lua/game/test_<module>.lua` (or `make unit-run-test_<name>`) passes on the working tree.
4. `make test` and `make e2e` are green, and the failure you handed over no longer appears.

All four hold → accept: `fix-bug` commits (folded into `HEAD` when `progress.md` exists, see `git-commit`). Remove `tmp/gate`.
Any one fails → refuse, with the reason. Nothing is committed; restore the working tree to `HEAD` before the next attempt.

### 5. Count the attempt
An attempt fails when you refuse the commit, or when `fix-bug` stops on its own (cause unproven after two debug agents, verification failing twice).

- Fewer than 3 failed attempts on this failure → back to step 3 with the same failure, the bug report extended with what was eliminated and why the last attempt failed.
- 3 failed attempts → stop. Report `E2E-FAILED:` to the user: the failure, the three attempts, what each eliminated, and which observation is missing. This is the only moment this skill hands back to the human.

### 6. Loop
After an accepted commit, back to step 1: `make e2e` from the top.

## Output

- `E2E-GREEN:` + the `fix:` commits made, or folded into `HEAD` (possibly none).
- `E2E-FAILED:` + the report of step 5. The working tree is back at `HEAD`, nothing half-fixed is left behind.

Never push: the caller decides.
