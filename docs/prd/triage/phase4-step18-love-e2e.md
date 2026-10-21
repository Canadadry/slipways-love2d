---
title: "make love-e2e: the fake LÖVE on the 20 replays, each reported as matching, divergent or failed"
description: "Nothing runs the fake LÖVE on the e2e replays, so the Lua game of plans 2–3 has no way to learn which replays it reproduces and why the others do not."
status: needs-triage
---

## Problem Statement

`tmp/fake_love` plays a LÖVE game on one replay and writes the C replay's
capture format (step 16), and step 17 makes it stop with a named error when
the game breaks the contract. But nothing runs it on `tests/e2e/`: plan 3
works replay by replay on "`make love-e2e` shows progress", plan 3's and
plan 4's "done when" are "`make love-e2e`: the 20 replays match their
`.json.sha256`", and plan 1 itself is done only when `make love-e2e` runs
the fake on the 20 replays. Today:

- there is no target; running the fake by hand on 20 replays, hashing 20
  captures and comparing 20 hashes is a shell session nobody repeats the
  same way twice;
- a game that stops on a fatal error (an unlisted function, a colour
  outside the 16, a foreign pixel, a Lua error, a missing `main.lua`)
  leaves no capture, and `tools/frames_check.sh` would only say "current
  hash missing", hiding the cause the fake printed;
- `tools/frames_check.sh`'s advice on a divergence (`make e2e-ref`,
  `make e2e-baseline`) is the C game's: for the Lua game a divergence is a
  bug in the Lua game or the fake, and the `.json.sha256` are never
  changed (plan 3's rule);
- `game/` holds only `sprites.png` and `font.png`, no `main.lua`: the
  target must be provable now, with stub games, and never assume the real
  game exists;
- nothing has yet shown that the fake's capture path, end to end on the
  real replays (their frame counts, `initial_save`, `bindings` fixtures,
  `attract_after`), can produce the reference hashes at all.

## Solution

`make love-e2e` runs `tmp/fake_love <game dir>` on every replay of
`tests/e2e/`, in parallel (one make job per replay), hashes each capture,
and reports every replay on one line: **matches**, **DIVERGENT** (through
`tools/frames_check.sh`) or **FAILED** (the fake exited non-zero; the line
carries the fake's own message). It examines all 20 before concluding,
prints a count line, and exits non-zero unless all 20 match.

The game directory is `LOVE_E2E_GAME`, default `game`. A `%` in it is
replaced by the replay's file name (`pad_title.json`), so a verification
can give each replay its own stub directory; the default has no `%`, so
every replay runs the same `game/`.

The target is outside `make test`, `make e2e`, `make test-all` and CI. It
takes `tmp/make.lock` like `test`, `e2e` and `test-all`, since it builds
`tmp/fake_love` in the same `tmp/`.

Proven three ways, once, by the implementer:

1. **Load failure.** `make love-e2e` on today's `game/` (no `main.lua`)
   reports 20 FAILED lines, each naming `main.lua`, distinct from a
   divergence, and exits non-zero.
2. **Negative stub.** A stub game drawing one fixed frame runs all 20
   replays to completion; each capture is the replay's frame count × 9 600
   bytes (equal to the size of the C capture in `tmp/frames/`); each is
   reported DIVERGENT, none FAILED.
3. **Positive control.** A stub game that reads the C replay's own capture
   from the fake disk and redraws each frame with `setColor` and
   `rectangle` runs makes all 20 replays report matches, exit 0.

## User Stories

1. As a plan-3 developer, I want one command that runs the Lua game on every replay of `tests/e2e/`, so that I see at once which replays it reproduces.
2. As a plan-3 developer, I want each replay reported on its own line as matches, DIVERGENT or FAILED, so that I can read progress replay by replay in `order.txt`'s terms.
3. As a plan-3 developer, I want a final count line (match, diverge, failed, out of how many), so that progress is one number to compare between runs.
4. As a plan-3 developer, I want a replay where the fake stopped on a fatal error to show the fake's own message (function, frame, pixel, Lua `file:line:`), so that I fix the cause, not chase a missing hash.
5. As a plan-3 developer, I want a failure to be reported as FAILED, never as DIVERGENT or "hash missing", so that "the game crashed" and "the game drew something else" never read alike.
6. As a plan-3 developer, I want every replay run and reported even when some fail, so that one crashing replay does not hide the state of the 19 others.
7. As a plan-3 developer, I want the target to exit non-zero unless every replay matches, so that "done when the 20 match" is a plain exit status.
8. As a plan-3 developer, I want the 20 replays run in parallel, so that the whole check takes seconds, not a minute.
9. As a plan-3 developer, I want every replay rerun on every invocation, so that a change to a Lua file (which make does not track) is never hidden by a stale capture.
10. As a plan-3 developer, I want the capture of each successful run kept on disk next to its hash, under its own directory of `tmp/`, so that `make e2e-diff` and my own tools can compare it with the C capture.
11. As a plan-3 developer, I want outputs of the previous run removed before each replay runs, so that a failed run never leaves an older capture or hash looking current.
12. As a plan-3 developer, I want the divergence advice not to point at `make e2e-ref` or `make e2e-baseline`, so that I am never told to rewrite a C reference for a Lua bug.
13. As a plan-3 developer, I want the replay's `initial_save`, `bindings` fixture and `attract_after` to reach the game exactly as `tmp/fake_love` already provides them, so that `love-e2e` adds no second reading of the replay.
14. As a plan-3 developer, I want the game directory to be a make variable defaulting to `game`, so that I can run the e2e on a copy or a branch of the game.
15. As a verifier, I want a `%` in the game directory replaced by the replay's file name, so that a stub can be given one directory per replay.
16. As a verifier, I want `make love-e2e` on today's `game/` (no `main.lua`) to report 20 named load failures, so that the load-failure path is proven on the real default.
17. As a verifier, I want a committed stub game drawing one fixed frame, so that the 20 runs, their frame counts and the DIVERGENT path are proven without the real game.
18. As a verifier, I want a committed stub game that redraws the C capture, so that the fake's capture path is proven able to reproduce all 20 reference hashes before plan 2 starts.
19. As a verifier, I want the positive control to read the C capture through the fake disk, as the real game reads its files, so that it uses no back door the real game lacks.
20. As a verifier, I want both stubs to follow design.md's drawing rules (canvas 0 created in `love.load`, palette colours as `byte / 255`, every pixel of canvas 0 opaque at each frame's end, no canvas left selected), so that they pass step 17's checks.
21. As a plan-1 developer, I want the runner and the report as two small scripts with a test, so that their behaviour (failure capture, stale cleanup, the three outcomes, the count, the exit status) is checked without running 20 replays.
22. As a plan-1 developer, I want that test to drive the real `tmp/fake_love` on a 3-frame fixture replay, so that the runner is proven against the binary it wraps.
23. As a plan-1 developer, I want `tools/frames_check.sh` reused for the hash comparison, with an option that drops its C-only advice, so that `make e2e` and `make love-e2e` compare hashes the same way.
24. As a plan-1 developer, I want `make e2e`'s output unchanged by that option, so that the C loop reads exactly as before.
25. As a plan-4 developer, I want `make love-e2e` to rely on nothing from `src/` or from the C capture, so that it survives the deletion of the C game and becomes `make e2e`.
26. As a plan-4 developer, I want only the positive control to read `tmp/frames/`, and only in its one-off verification, so that the target itself does not need the C replay.
27. As the repository owner, I want `make love-e2e` outside `make test`, `make e2e`, `make test-all` and CI, so that the fast loops and CI stay as they are while the Lua game cannot pass yet.
28. As the repository owner, I want `make love-e2e` serialised with `test`, `e2e` and `test-all` through `tmp/make.lock`, so that two makes never build `tmp/fake_love` and its objects at once.
29. As the repository owner, I want the new test under 100 ms, in `FAKE_LOVE_CHECKS` and `FAKE_SIDE_BINARIES`, and `make test` measured with design.md's protocol, so that the fast loop stays fast.
30. As the repository owner, I want `make e2e`, the C game and every `*.sha256` and `.end-state` untouched, so that the reference is unaffected.
31. As the repository owner, I want `make help` to list `love-e2e` with a one-line description of at most 20 words, so that it is found like every target.
32. As a plan-2/3 developer, I want design.md to describe `make love-e2e` (what it runs, the variable, the `%`, the outputs, the three outcomes, the exit status, what it does not do), so that I read the contract and not the Makefile.

## Implementation Decisions

### Scope: `love-e2e` only, no `love-e2e-state`

- Plan 1's step 18 and its "done when" name `make love-e2e` and the
  sha256 only. `make love-e2e-state` (end states against `.end-state`) is
  named only by plans 3 and 4; it is **not** added here. `make love-e2e`
  passes **no** state path to the fake, so a game without `debug_state`
  (the stubs, an early plan-2 game) is not failed for it.
- `love-e2e` does not check the C side (`make e2e`), does not build
  `tmp/frames/`, and does not compare frame counts at run time: the fake
  writes frames × 9 600 by construction (step 16), checked once in the
  verification.

### The runner (new shell script in `tools/`, one replay)

- Arguments: the hash command, the fake binary, the game directory (with
  `%` already replaced), the replay, the capture path, the hash path, the
  error path.
- Removes the three outputs first, then runs the fake as
  `<fake> <game dir> <replay> <capture>` with its stderr kept aside.
- Fake exit 0: writes the capture's sha256 (first field of the hash
  command, newline-terminated, as `make e2e`'s hash files) to the hash
  path; passes the fake's stderr through (empty under step 17, see
  H-17-5).
- Fake exit non-zero: writes the error file: one line
  `fake_love exited <status>` (or `fake_love killed by signal <n>` above
  128), then the fake's stderr verbatim; removes any capture left behind
  (none under H-17-4, but the runner does not rely on it); writes no
  hash.
- The runner itself exits 0 in both cases, so that `make -j` keeps
  starting the other replays and the report sees all 20. It exits
  non-zero only on its own problems (wrong argument count, hash command
  failing), which then stop make as a broken tool should.
- Hash command: the Makefile's `SHA256_CMD` (`sha256sum`, `shasum -a 256`
  on macOS), passed in, not re-detected.

### The report (new shell script in `tools/`, all replays)

- Arguments: one quadruple per replay: replay, reference `.json.sha256`,
  current hash, error file. The output prefix is fixed, `love-e2e:`.
- For a replay with an error file: prints
  `love-e2e: FAILED <replay> -- the fake stopped before the last frame.`
  then each line of the error file indented, on stderr. Other replays go,
  as triples, to `tools/frames_check.sh --no-baseline-hints` in one call.
- Ends with
  `love-e2e: <m> match, <d> diverge, <f> failed, of <n> replays in tests/e2e`
  where m counts frames_check's `matches` lines, f the error files, d the
  rest. Exit 0 only when f = 0 and frames_check passed and m = n.
- Order of lines: replays in the order make lists them (sorted file
  names, as `E2E_EVENTS`); FAILED lines first, then frames_check's
  output, then the count. Rejected: interleaving in one order, because it
  would mean re-implementing frames_check's comparison in the report.

### `tools/frames_check.sh --no-baseline-hints`

- New optional first argument. With it, the advice lines naming
  `make e2e-ref` and `make e2e-baseline` (on a divergence, and on a
  missing reference) are not printed; every other line, the comparison and
  the exit status are unchanged. Without it, the output is byte for byte
  today's, so `make e2e` reads as before.
- With it, a divergence instead prints one line:
  `frames:   the .json.sha256 is the C game's reference: fix the Lua game or the fake, never the reference`.

### The Makefile target

- `LOVE_E2E_GAME ?= game`. `LOVE_E2E_DIR := $(TMP_DIR)/love-e2e`; outputs
  per replay mirror `tmp/frames/`: `tmp/love-e2e/tests/e2e/<name>.json.bin`,
  `.sha256`, `.error`.
- One target per replay (the hash file), always rerun (forced, as a Lua
  edit is invisible to make), depending on `tmp/fake_love`, the runner and
  the replay, its recipe the runner with the game directory
  `$(subst %,<replay file name>,$(LOVE_E2E_GAME))`.
- `love-e2e` depends on the 20 per-replay targets and the two scripts and
  `tools/frames_check.sh`; its recipe is the report. Parallelism comes
  from the Makefile's existing `-j$(NPROC)`; `--output-sync=target` keeps
  each runner's stderr whole.
- `love-e2e` joins `LOCKED_GOALS`, so `make love-e2e` while `make test`
  runs fails at once with the existing message; `CLAUDE.md`'s Build line
  naming the locked targets gains `love-e2e`.
- Help line (≤ 20 words): `runs tmp/fake_love on every tests/e2e/ replay in
  parallel and checks each capture's sha256 (LOVE_E2E_GAME=game)`.
- Not a prerequisite of `test`, `e2e`, `test-all`; no CI job (it fails on
  `game/` until plan 3 ends).

### The stub games (committed fixtures in `tests/fixtures/fake_love/`)

All stubs use only `love.load`, `love.draw`, `love.filesystem.read`,
`love.graphics.newCanvas`, `setCanvas`, `clear`, `setColor` and
`rectangle("fill", …)`, plus plain Lua (`string.byte`, arithmetic). No
`conf.lua`, no `debug_state`.

- **`e2e_fixed`** (negative stub): `love.load` creates canvas 0. Each
  `love.draw`: select canvas 0, `clear` to colour 1 (`1D2B53`) as
  `byte / 255`, opaque; `setColor` colour 8 (`FF004D`) as `byte / 255`;
  `rectangle("fill", 0, 0, 2, 1)`; select the screen. Every frame is
  therefore byte 0 = `0x88`, bytes 1–9 599 = `0x11`. Also used by the
  runner's test.
- **`e2e_redraw`** (positive control): `love.load` creates canvas 0 and
  reads the whole of `capture.bin` with `love.filesystem.read`. Each
  `love.draw`, frame k (counted by the stub from 0): select canvas 0; for
  each row, walk the 160 nibbles of frame k (low nibble = even x), and
  for each run of equal colour call `setColor` of that palette colour as
  `byte / 255` (16-entry table of design.md's Capture table) then
  `rectangle("fill", x0, y, length, 1)`; select the screen. A frame
  beyond the data raises (a too-short capture is a FAILED, never a silent
  match). No `clear` needed: the runs cover all 19 200 pixels.
  Prototype result (pre-step-17 fake, this PRD's exploration):
  `pad_tech_costs.json` (558 frames, 1.58 M runs) redrawn in 0.82 s and
  its hash equal to `pad_tech_costs.json.sha256`.
- **How the positive control reaches the C capture** (one-off, in the
  verification, nothing committed under `tmp/`): `make e2e` first (writes
  `tmp/frames/tests/e2e/<name>.json.bin`), then for each replay a
  directory `tmp/love-e2e-redraw/<name>.json/` holding a symlink
  `main.lua` to the committed `e2e_redraw/main.lua` and a symlink
  `capture.bin` to that replay's C capture; then
  `make love-e2e LOVE_E2E_GAME=tmp/love-e2e-redraw/%`. The fake disk's
  read-only view stats and opens through symlinks (current `disk.c`), so
  the stub reads the file by its plain name, as the real game reads
  `sprites.png`. Rejected: one directory with the 20 captures (the stub
  cannot know which replay it is playing: `love.load` gets only
  `--attract-after`), and copying the 40 MB of captures.
- **Load failure**: no new stub. `game/` (no `main.lua`) is the real
  case: the fake prints `fake_love: main.lua: cannot open game/main.lua:
  No such file or directory` and exits 1. The runner's test uses the
  existing `no_main` fixture.

### Speed estimate (decided, not gated)

- The C captures hold 4 072 frames, 11.1 M colour runs (2 727 per frame
  on average, 4 508 at most). The prototype did 1.58 M runs (setColor +
  rectangle each) in 0.82 s, so the whole positive control is about 6 s
  of CPU, about 2 s of wall time on 4 cores, bounded by
  `pad_planet_projects`/`pad_tech_costs` (< 1 s each). The fixed stub
  and a load failure take milliseconds per replay. No optimisation of
  the stub or the fake is needed; the per-replay wall time of each proof
  is recorded in the commit message.

### Hypotheses on step 17 (being implemented; its PRD is the source)

- **H-17-1** The function-list guard raises on any read of an unlisted
  field of `love` or its modules, and allows the game to define
  `love.load` and `love.draw`; `love.filesystem.read`,
  `love.graphics.newCanvas`, `setCanvas`, `clear`, `setColor` and
  `rectangle` stay listed with today's signatures and returns
  (`read` returns the content and its size).
- **H-17-2** In `tmp/fake_love`, `setColor` accepts exactly the bytes of
  the 16 palette triples and white `FFFFFF`, judged after LÖVE's
  truncating conversion, so `byte / 255` of a palette byte is accepted;
  the stubs pass only palette colours that way.
- **H-17-3** At the end of each frame, canvas 0 (the first created) must
  exist and every one of its pixels be an opaque palette colour,
  otherwise the run stops; the stubs create canvas 0 in `love.load`
  (H-17-3 is why canvas 0 must exist from `love.load`, not from the first
  `love.draw`, though the check itself is at frame end) and repaint every
  pixel opaque in every frame.
- **H-17-4** Every fatal error, step 17's three included, prints
  `fake_love: <message>` on stderr, exits 1, and leaves no capture and no
  state file.
- **H-17-5** On a successful run the fake prints nothing on stderr (step
  16's end-of-run warning is gone).
- **H-17-6** `clear` with a palette colour on canvas 0 is accepted and
  `clear` is not subject to the palette rule (the fixed stub uses a
  palette colour anyway, so it holds either way).
- **H-17-7** Step 17 keeps `three_frames.json` and `no_main` as they are
  (`no_main` fails before any frame, so step 17 leaves it untouched).
- If a hypothesis is false when implementing: the stubs are written to
  pass with or without step 17 (palette colours only, canvas 0 from
  `love.load`, every pixel opaque), so only H-17-4/H-17-5 can change
  behaviour; the runner relies on the exit status and keeps any stderr,
  so it stays correct, and the implementer records the actual behaviour.
  Verification runs after step 17 is committed.

## Implementation Chunks

1. **`frames_check.sh --no-baseline-hints`.** The option and its one
   replacement line; cases in `test_frames_check` (with the option: no
   `make e2e-ref`/`make e2e-baseline` on a divergence nor on a missing
   reference, the replacement line present, same exit statuses; without
   it: today's expectations unchanged). Stories 12, 23, 24.
2. **Runner, report, fixed stub, test.** The two scripts; the `e2e_fixed`
   fixture; `test_love_e2e` (new, in `FAKE_LOVE_CHECKS` and
   `FAKE_SIDE_BINARIES`, linking nothing from `src/`). Stories 2–7, 10, 11,
   17, 21, 22.
3. **Makefile target and docs.** `LOVE_E2E_GAME` with `%`, per-replay
   forced targets, `love-e2e`, `LOCKED_GOALS`, help line; `CLAUDE.md`
   Build line; design.md paragraph "`make love-e2e`" under the fake's
   section (and its rule 3 sentence pointing to it). The `e2e_redraw`
   fixture. Stories 1, 8, 9, 13–15, 25–28, 30–32.
4. **Verification and timing.** The three proofs of the Solution, with
   outputs summarised in the commit message (20 FAILED naming `main.lua`;
   20 DIVERGENT with each capture size equal to the C capture's; 20
   matches, exit 0; wall time of each). `make test` measured with
   design.md's protocol against the step's "before" and `2e7215c`, the
   slowest new check's own time. Stories 16, 18–20, 29.

## Testing Decisions

- A good test drives a script or binary through its arguments and checks
  what comes out: files written or removed, stdout/stderr lines, exit
  status. No test reads a script's internals.
- **`test_frames_check`** (existing) gains the option's cases; prior art
  is the file itself (work directory under `tmp/`, hash files, log).
- **`test_love_e2e`** (new C test, own work directory under `tmp/`):
  - runner + real `tmp/fake_love` + `e2e_fixed` on
    `tests/fixtures/fake_love/three_frames.json`: exit 0, capture of
    3 × 9 600 bytes, hash file equal to the sha256 of the expected bytes
    computed by the test from the stub's drawing (`0x88` then 9 599 ×
    `0x11`, three times), no error file;
  - runner + `no_main`: exit 0, error file whose first line is
    `fake_love exited 1` and which holds `main.lua`, no capture, no hash;
  - stale outputs: stale capture, hash and error files present before a
    run are gone or replaced after it, in both cases;
  - report over prepared files (one match, one divergence, one error
    file): FAILED line with the indented error, `matches` and
    `DIVERGENT` lines from frames_check, no `make e2e-baseline` /
    `make e2e-ref`, the count line `1 match, 1 diverge, 1 failed, of 3`,
    exit non-zero; all matching: exit 0.
  - Prior art: `test_frames_check` (scripts on files),
    `tests/fake_love_cli.c` helpers and `test_fake_love_frames` (driving
    `tmp/fake_love`, capture bytes).
  - Budget: under 100 ms (two short fake runs and a few shell processes);
    split into two targets if over.
- **Not tested in `make test`:** the 20-replay runs (one-off verification
  only, by the CLAUDE.md speed rule) and the `e2e_redraw` stub (its proof
  is the 20 matching hashes).
- `make e2e`, the C unit tests, every `*.sha256` and `.end-state`, every
  capture program and PNG stay unchanged.

## Out of Scope

- `make love-e2e-state` and any `.end-state` comparison (plans 3–4).
- `make e2e-diff`, per-frame state dumps (plan 3, phase 1).
- Running `love-e2e` in CI, in `make test`, `make e2e` or `make test-all`.
- A timeout on a hanging game.
- Any change to `tmp/fake_love`, its contract, the fake disk or the replay
  reader; any new LÖVE function.
- The Lua game (`game/main.lua`, plans 2–3); audio, always.
- Choosing a subset of replays (`E=`): plan 3's `e2e-diff` serves one
  replay.

## Further Notes

- Plan 3 step 3 and plan 4 rely on `make love-e2e-state`, which no step of
  plan 1 builds; plan 3 must add it (the fake already writes the state
  file when given a fourth argument, and the runner can take an optional
  state path then). Recorded here so plan 3's review sees the gap.
- The `%` substitution is the only test-oriented knob in the target; with
  the default `game` it is inert.
- After plan 4 deletes the C game, `make e2e` becomes this target's
  behaviour; nothing here reads `src/` or `tmp/frames/` except the
  one-off positive control.
