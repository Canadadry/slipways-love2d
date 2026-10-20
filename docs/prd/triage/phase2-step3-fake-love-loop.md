---
title: "fake_love runs the loop"
description: "There is no headless program that can load a LÖVE game and drive its update/draw loop from a replay, so nothing in plans 2–3 can be checked."
status: needs-triage
---

## Problem Statement

Plan 1 builds the fake LÖVE: a headless C program that plays a LÖVE game on
a replay and writes the same capture as the C replay. Today
`tools/fake_love/` holds only the vendored `stb_image.h`, and the only Lua
the repository can run is the `luajit_version` probe and real LÖVE under
Xvfb (`make love-capture`). The pad (step 4), the fake disk (step 5), every
drawing function (phase 3) and the capture (phase 4) all need a program that
embeds LuaJIT, loads a game directory as LÖVE does and calls its callbacks
once per replay frame. Without that skeleton none of them has anything to
plug into or test against.

The plan also says that `make test` will run every capture program in the
fake, and `CLAUDE.md` forbids making `make test` slower. Someone has to
decide once how the fake's checks stay out of `make test`'s wall time. Every
later step then follows that decision and records the time before and after.
This step makes that decision.

## Solution

A new binary, `tmp/fake_love`, built from C sources in `tools/fake_love/`:

```
tmp/fake_love <game dir> <events.json> <capture.bin> [<state.txt>]
```

It checks the LuaJIT version, reads the replay, loads `conf.lua` and then
`main.lua` from the game directory into one LuaJIT state, calls `love.load`,
and runs `love.update(1/60)` then `love.draw()` once per entry of the
replay's `frames`. It creates `capture.bin`, which stays empty because
nothing draws yet. When it is given `state.txt`, it writes there the text
returned by the game's global `debug_state()`. Any error stops it with a
message that names the file, the callback and the frame, plus a Lua
traceback. It then removes the outputs it created and exits with a non-zero
status.

Two C tests check it in `make test`: one for the replay reader, and one that
runs the binary on small test games. One of those games counts its `update`
calls on a 3-frame replay.

The `make test` time decision (see Implementation Decisions, "Keeping the
fake out of `make test`'s wall time"): each check of the fake is its own
make target, a direct prerequisite of `test`, so `-j` runs it in parallel
with the unit tests and off their critical path. Every step that adds such a
check measures `make test` with a fixed protocol before and after. Its
"after" median may not be more than 3 % above its own "before", nor more
than 3 % above the plan-wide reference: the commit this step starts from,
re-measured next to the current one so the two run on the same machine.
The step records the numbers in its step note.

## User Stories

1. As a plan-2 developer, I want `tmp/fake_love game/ replay.json out.bin` to run my Lua game headless, so that I can check it without a window.
2. As a plan-2 developer, I want `conf.lua` loaded before `main.lua`, as LÖVE does, so that a game that works in real LÖVE starts in the same order in the fake.
3. As a plan-2 developer, I want `love.conf(t)` called with a `t` that has `window`, `modules` and `audio` sub-tables, so that a normal `conf.lua` (setting `t.window.width`, `t.modules.audio = false`) runs without an error.
4. As a plan-2 developer, I want a missing `conf.lua` to be skipped, as LÖVE does, so that a game without one still runs.
5. As a plan-2 developer, I want a missing `main.lua` to be a named, fatal error, so that a wrong game directory is reported at once.
6. As a plan-2 developer, I want `require("module")` to resolve inside the game directory (`?.lua`, `?/init.lua`) and nowhere else, so that a multi-file game loads as it would from a `.love` and cannot pick up stray modules from the system.
7. As a plan-2 developer, I want `love.load` called once, before the first frame, with a table argument, so that game set-up runs as in LÖVE.
8. As a plan-2 developer, I want `love.update(dt)` then `love.draw()` called exactly once per entry of `frames`, in that order, so that one replay frame is one game frame.
9. As a plan-2 developer, I want `dt` to be exactly `1/60`, so that a game that reads it sees the frame rate the design fixes.
10. As a plan-2 developer, I want a callback the game does not define to be skipped, as LÖVE does, so that a minimal game only defines what it needs.
11. As a plan-2 developer, I want a replay with zero frames to call `love.load` and nothing else, so that the edge case has a defined behaviour.
12. As a plan-2 developer, I want `debug_state()` called after the last frame when I pass a state path, and its returned string written there byte for byte, so that I can compare the end state with the replay's `.end-state`.
13. As a plan-2 developer, I want `debug_state()` left uncalled when I pass no state path, so that a game under construction does not need one.
14. As a plan-2 developer, I want a missing `debug_state`, or one that returns something other than a string, to be a named fatal error when a state path is given, so that a broken state dump is never written silently.
15. As a plan-2 developer, I want a Lua error in `conf.lua`, `main.lua` or any callback reported on stderr with the callback's name, the 0-based frame index (the `f<N>` of the e2e notes) and a traceback, so that I can find the failing line at once.
16. As a make user, I want the fake to delete the capture and state files it created when it fails, so that make never treats a partial output as up to date.
17. As a make user, I want a wrong argument count, or an argument that starts with a dash, to print the usage and exit with status 2, so that a mistyped command never runs on a default.
18. As a make user, I want every other failure to exit with status 1, so that scripts can tell a usage error from a game error.
19. As a plan-1 developer, I want the fake to check at startup that the LuaJIT header and runtime versions match and start with `LuaJIT 2.1`, as the design requires, so that a wrong system LuaJIT is reported before any game code runs.
20. As a plan-1 developer, I want the replay reader to accept all 20 replays of `tests/e2e/`, with their exact frame counts, so that the reader is proven on the files it will actually read.
21. As a plan-1 developer, I want a malformed replay (not JSON, `frames` missing or not an array, a frame that is not an object, a non-integer number, trailing text) refused with a message naming the file and the problem, so that a broken fixture never runs a wrong number of frames.
22. As a plan-1 developer, I want the replay reader to keep the parsed document whole, so that steps 4 and 5 can read `pad`, `initial_save`, `bindings` and `attract_after` without writing a second parser.
23. As a plan-1 developer, I want the fake to depend on nothing from `src/`, so that it survives plan 4 deleting the C game.
24. As a plan-1 developer, I want the global `love` table to exist before `conf.lua` runs and to hold no functions yet, so that steps 4 to 17 each add their own functions to it.
25. As a step-4-to-18 implementer, I want one written rule for how a new fake check joins `make test` and how its cost is measured, so that I do not have to decide it again.
26. As the repository owner, I want `make test` no slower after this step than before it, measured and recorded, so that the fast loop stays fast.
27. As the repository owner, I want `make e2e` untouched by this step, so that the ~1 s replay check is unaffected.
28. As a CI maintainer, I want `make test` to need only the packages `tools/love_install.sh` already installs (LuaJIT dev, pkg-config), so that CI needs no change.
29. As a make user without LuaJIT installed, I want the build to fail with a message that names `tools/love_install.sh`, so that I know how to fix it.
30. As a make user, I want `make fake-love` to build the binary on its own and appear in `make help`, so that I can run it by hand.

## Implementation Decisions

### Modules

- **Replay reader** (deep module, in `tools/fake_love/`): it parses a replay
  file, or a text in memory, into a document and exposes what the loop
  needs. The public surface is small: read from a path, parse from a text
  and length, frame count, free, and an error message buffer that names the
  file and the problem with its byte offset. The JSON parser sits behind it,
  static. It accepts objects, arrays, strings (with the standard escapes),
  integers, `true`, `false` and `null`. It refuses fractions and exponents,
  because no replay holds one and the frame loop must never depend on a
  float. It keeps the whole parsed tree in one allocation that the reader
  owns and its free function releases, so steps 4 and 5 add accessors
  (`pad` lists, `initial_save`, `bindings`, `attract_after`) without
  re-parsing. Step 3 requires only `frames`, an array whose elements are all
  objects. Every other key is accepted and ignored until its step.
- **LÖVE host** (deep module): one LuaJIT state that runs one game. Its
  interface: open (create the state, open the standard libraries, set
  `package.path` to the game directory and `package.cpath` to empty, create
  the empty global `love` table, run `conf.lua` if it exists and call
  `love.conf(t)` if it is defined, run `main.lua`), call `load`, run one
  frame (`update(1/60)` then `draw()`), get the debug state, and close. Each
  call returns success or failure. A failure leaves a message in a buffer
  the host owns, valid until the next call. Every Lua call goes through
  `xpcall` with `debug.traceback` as the handler. The host knows nothing
  about replays or files other than the game's. The frame index is passed
  in so that it can appear in messages.
- **Command** (the binary's entry point): it parses the arguments, checks
  the LuaJIT version (header `LUAJIT_VERSION` equal to the runtime
  `jit.version`, prefix `LuaJIT 2.1`), reads the replay, creates the capture
  file (empty in this step), runs load and the frames, and writes the state
  file. On failure it removes the outputs it created. Every message goes to
  stderr, prefixed `fake_love: `.
- No file in `src/` is linked or included. `tools/love_capture/` is not
  touched: the probe there and `make love-check` stay as step 1 left them,
  even though the version check is written a second time. It is ten lines,
  and the two programs have different lifetimes.

### Contracts settled here

- Load order: `love` table, then `conf.lua` (optional), then `love.conf(t)`
  if defined, then `main.lua` (required), then `love.load(args)`. `t` has
  the sub-tables `window`, `modules` and `audio`, empty. The fake ignores
  every value the game writes into `t`.
- `love.load` receives an empty table in this step. Step 5 fills it
  (`--attract-after`).
- Per frame, in order: `love.update(1/60)`, then `love.draw()`. Step 4
  inserts the pad callbacks before `update` and step 16 inserts the canvas
  read after `draw`. An undefined `love.load`, `love.update` or `love.draw`
  is skipped. A defined one that is not a function is a fatal error.
- Frame numbers in messages are the 0-based index into `frames` (`f0` is
  the first, as in the e2e notes).
- `debug_state` is a global function, called with no arguments, only when a
  state path is given. It must return a string, and the file holds exactly
  those bytes. **Scope note:** design step 6 (the end state) was meant for
  step 16. It is done here because the state file is the only output this
  step has through which a test can see the game's counts. Step 16 keeps
  this behaviour and only adds the capture.
- The capture file is created (truncated) before `love.load`, and this step
  writes nothing into it. Step 16 appends 19 200 bytes per frame.
- Exit status: 0 on success, 1 on any failure of the game or its inputs, 2
  on a usage error. Usage: anything other than 3 or 4 arguments, or any
  argument starting with `-`.
- The global `love` table holds no functions in this step. A game that
  calls `love.graphics.…` gets the plain Lua error ("attempt to index a nil
  value"), reported as above. The named "unlisted function" error belongs
  to step 17.

### Build

- `tmp/fake_love` is built from the fake's sources into the normal object
  tree. The LuaJIT compile and link flags come from `pkg-config luajit`.
  They are added only to the fake's objects and link line, and are expanded
  lazily, so targets that do not build the fake (`make e2e`, `make help`,
  the C binaries) never call `pkg-config`. When `pkg-config luajit` fails,
  the build stops with a message that names `tools/love_install.sh`.
- A public target `fake-love` builds the binary. Its `## ` line has at most
  20 words.
- Compiled with the repository's `CFLAGS` (`-std=c99 -Wall -Wextra
  -Werror`). The static guards scan `src/` only, so they do not apply.

### Keeping the fake out of `make test`'s wall time (the decision for plan 1)

1. **Parallel, never serial.** Each check of the fake is its own make target
   and a direct prerequisite of `test`. That covers each C test that drives
   the fake in this step and, from step 6 on, each capture program
   comparison. No check loops over programs inside one recipe, and none
   orders itself after the unit tests. `make test` already runs with
   `-j$(NPROC)`, so the fake's checks fill the idle cores while the long
   unit tests run. Their cost is CPU time, not wall time.
2. **Cheap by construction.** One process per check, no interpreter other
   than the embedded LuaJIT, and no external tool (no Python, no
   ImageMagick, no shell pipeline over PNGs). Comparisons are done in C. A
   check's budget is 100 ms of wall time on the container. A check over that
   budget is split before its step closes.
3. **Out of `make e2e`.** No fake check is a prerequisite of `e2e`. Step 18
   adds `make love-e2e` as its own target, outside `make test` and outside
   `make e2e`.
4. **Measured, with one protocol, against two baselines.**
   - **The plan-wide reference** is the commit step 3 starts from (chunk
     0). The step 2.3 note records it as "the plan-wide `make test`
     reference": its hash, its median, the core count. Because
     containers differ between sessions, a recorded median is never
     compared across machines. Every later measurement re-runs the
     reference commit on the same machine, beside the current one.
   - **The measurement:** check out each commit under comparison (the
     step's "before" commit, the reference commit and the current commit)
     into its own git worktree under `tmp/`. Run `make test` once in each
     to warm the build. Then run 11 rounds; in each round, time one
     `make test` (wall time) in every worktree, back to back, rotating
     which goes first.
   - **The gate (paired ratios, decided by `guideline`):** for each round,
     r = current / baseline − 1, once against "before" and once against the
     reference; the statistic is the median of the 11 r values per
     baseline. Both ≤ 3 % → pass. Either > 3 % → 11 more rounds, pool the
     22, decide on the pooled medians: still > 3 % → fail, and the step
     reworks its checks under rules 1 and 2 (or rule 5). No spread check
     and no third batch: per-run spread is about 4 % in this container and
     does not shrink with more runs, while in-round pairs cancel drift.
   - **The record:** the step note gives the three plain medians, the two
     paired medians, each commit's spread (information only), the
     number of rounds, the core count, and the slowest new check's own
     time.
   - The plan is done only when the final median is within 3 % of the
     re-measured reference.
5. If rules 1 and 2 cannot hold a future check under the 3 % limit, that
   check goes into a separate target outside `make test-all`, as
   `CLAUDE.md` requires. Its step note says so, and `docs/love-port/design.md`
   is updated, because the design currently says `make test` runs every
   capture.

The rule for later steps (rules 1 to 5, including both comparisons, the
reference commit's hash and the noise check) goes into
`docs/love-port/design.md`, under the fake LÖVE section, as part of this
step. Plans 2–3 read that file, and plan
1 will be deleted.

### Test fixtures

- The test games and their replays live under the fixtures directory, in a
  `fake_love/` sub-directory: one directory per game (`conf.lua`,
  `main.lua`, optional modules), and the small replays next to them. The
  `tests/` layout stays flat for C files.

## Implementation Chunks

0. **Plan-wide reference** (no code, done by the caller): commit 2e7215c,
   4 cores, plain median 7.70 s over 15 runs (spread 4.3 %, information
   only). Later steps re-measure it beside their own commits.
1. **Replay reader.** It parses JSON into a kept document, gives the frame
   count, and gives errors with file and offset. Its unit test joins the
   unit tests. Covers stories 20–23.
2. **Binary, happy path.** The `fake-love` target and the LuaJIT flags; the
   version check; the host (state, `package.path`, `love` table, `conf.lua`
   then `main.lua`, `love.conf(t)`, `love.load`, `update(1/60)`/`draw` per
   frame, skipped callbacks); the empty capture file; `debug_state` into the
   state file. The counting test game on the 3-frame replay, and the
   CLI test joins `make test`. Covers stories 1–4, 6–13, 19, 24, 28, 30.
3. **Failures.** Usage (status 2); missing game directory, `main.lua`,
   replay or unwritable output (status 1); Lua errors in each phase with
   callback, frame index and traceback; a callback that is not a function;
   `debug_state` missing or not returning a string; outputs removed on
   failure; the `pkg-config` failure message. Then the rule in
   `docs/love-port/design.md`, the after-timing and the step note. Covers
   stories 5, 14–18, 25–27, 29.

## Testing Decisions

- A good test drives a public interface and checks external behaviour: the
  reader's results and error messages, and the binary's exit status, stderr
  text and output files. It never reaches into the Lua state, never checks
  the JSON tree's layout, and never matches a whole traceback (only the
  callback name, the frame index and the file:line).
- **Replay reader, unit test** (`test_fake_love_replay`, in the unit
  tests): it reads every `tests/e2e/*.json` and checks the frame count
  against a count taken independently (the replays' frame counts are fixed
  values: 32 for `pad_title.json`, 558 for `pad_tech_costs.json`, …; the
  test lists them all). It also parses texts in memory: empty `frames`;
  unknown keys ignored; each refusal (not JSON, missing `frames`, `frames`
  not an array, a frame not an object, `1.5`, `1e3`, an unterminated
  string, a bad escape, trailing text) with the problem named in the
  message.
- **The binary, CLI test** (`test_fake_love`, in the unit tests, built
  after `tmp/fake_love` like `test_replay_usage` is after `tmp/replay`): it
  runs `tmp/fake_love` with `system()` and reads the files it writes.
  - The counting game on the 3-frame replay: exit 0. The state file shows
    `conf` before `main` before `load`, `load` once with a table, exactly 3
    `update` and 3 `draw` alternating, and `dt == 1/60` every time. The
    capture file exists and is empty.
  - A 0-frame replay: `load` only.
  - A game without `conf.lua` and with no `love.draw`: it runs.
  - A game whose `main.lua` requires a module of its own directory: it runs.
    Requiring a module that exists only outside the game directory fails.
  - No state path: `debug_state` is not called (the game's `debug_state`
    raises an error, and the run still succeeds).
  - Failures: each case in chunk 3, with status 1 or 2 as specified, a
    stderr that names the cause (for a Lua error: the callback, `f<N>` and
    `main.lua:<line>`), and neither output file left behind.
  - The version check is not faked in the test: on the container it passes,
    and the run's success shows it ran. A mismatch cannot be produced
    without a second LuaJIT, so that branch is checked by reading the code
    only.
- Prior art: `tests/test_replay_usage.c` (it drives a built binary with
  `system()` and reads its stderr from a file under `tmp/`) and
  `tests/test_frames_check.c` (a tool tested from a C test). The fixtures
  follow `tests/fixtures/`.
- Unit tests of the C game are untouched. `make e2e` and every `*.sha256`
  stay unchanged.
- Each test binary writes only under `tmp/`, into paths of its own, so it
  runs safely in parallel with every other unit test.

## Out of Scope

- The pad: the button table, `gamepadpressed`/`gamepadreleased`,
  `isGamepadDown`, `getJoysticks`, and validating `pad` arrays (step 4).
- The fake disk, `initial_save`, `bindings`, `love.filesystem`, `arg` and
  `--attract-after` (step 5).
- Every `love.graphics` and `love.image` function, and any byte in the
  capture (phases 3 and 4).
- The named fatal errors for an unlisted function, a foreign colour or a
  foreign pixel (step 17).
- `make love-e2e` (step 18).
- Stopping the game from writing the real disk through Lua's `io`/`os`
  (not required by the contract in this step).
- Audio, always.

## Hypotheses

- **H-2-1** (confirmed by the step 1.2 note, commit 2e7215c): plan step 2
  changed only the CI `love-capture` job, and reverted the deliberate drift
  (a capture program) that turned it red. It changed neither `make test`,
  nor any Makefile target this step touches, nor `tools/fake_love/`.
  Nothing in this PRD depended on it: the plan-wide reference is measured
  at this step's own start (chunk 0).

## Further Notes

- The step 3 note in the plan must hold: the plan-wide reference, labelled
  as such (commit, median, spread, rounds, core count); step 3's "after"
  median, compared with it; the slowest fake check's time; and the scope
  note that `debug_state` and the state file landed here instead of in step
  16.
- macOS: `brew install luajit pkg-config` provides the same `pkg-config
  luajit`. Nothing macOS-specific is decided here.
- The JSON reader is written for this repository and nothing is vendored
  for it. The plan allows vendoring only `stb_image.h` without a recorded
  decision, and a strict integer-only reader for one known format is
  smaller than any library.
