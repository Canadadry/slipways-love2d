# Plan 4 — Delete the C game

## 1. Goal

Once the 20 replays match in the fake LÖVE, remove the C game and everything
that only served it, so the repository holds the Lua game, the fake LÖVE and
the tests that do not need the C. Read `docs/love-port/design.md` first.

## 2. Rules

- Starts only when plan 3 is done: `make love-e2e` and `make love-e2e-state`
  green on the 20 replays.
- Before any deletion, the fixed-point tests stop depending on the C: their
  reference results are saved to a file in `tests/lua/`.
- After this plan, `make e2e` **is** the Lua e2e (the fake LÖVE on the 20
  replays) and `make test` holds the Lua tests and the fake's captures.
- The e2e speed rule becomes "`make e2e` under N seconds", N measured here,
  aiming for about 5 seconds; it replaces "never make `make e2e` slower" in
  `CLAUDE.md`.
- `make e2e-ref` goes with the C: from now on a divergence is read from the
  Lua state dumps and the capture alone.

## 3. Phases and steps

### Phase 1 — Cut the tests loose from the C

Before step 1, the step notes list every target of today's `make help` as
kept, ported to the fake LÖVE, or deleted with a reason. Steps 3 and 4
check against that list; a target changing category is recorded there as
an amendment, never decided silently inside a step.

1. Switch `make e2e` and `make end-state` to the fake LÖVE on the 20
   replays (absorbing `love-e2e` and `love-e2e-state`), measure the new
   `make e2e` (6 cold runs, median) and write the N-seconds rule into
   `CLAUDE.md` in the same change, replacing "never make `make e2e`
   slower". Verified by `make e2e` and `make end-state` green with the C
   still present, and the rule in `CLAUDE.md`.
2. Make the fix loop independent of the C: a `make test` home for Lua
   regression tests of `game/` modules, and `check-e2e`, `fix-bug`, `tdd`
   and `help-me-debug` pointed at that home and at the fake-LÖVE run and
   its state dump instead of the C unit tests, `tmp/replay` and the C as
   reference. Verified by `check-e2e` run once, in a throwaway worktree, on
   a replay broken by a one-line Lua change: it reaches `fix-bug` with a
   fake-LÖVE reproduction and a regression test that `make test` runs
   without `src/`.
3. Save a few thousand fixed-point and random-generator results (inputs and
   outputs, edge values included) to `tests/lua/fixed_reference.txt`; the Lua
   tests read that file instead of the C binary. Do the same for every other
   surviving check or generator that consumes something built from `src/`
   (table reference, generated Lua data, constants check): freeze its output
   or drop it. Verified by a dry run with `src/` moved aside and `tmp/`
   emptied, then restored: every target that survives the plan, and every
   CI command that survives it, is green, and every target listed as kept
   or ported is among them.

### Phase 2 — Delete

4. Delete `src/`, the C unit tests, the C-only tools and guards
   (`asset_export`, coverage, float/pointer/alloc guards, …) and their
   Makefile targets, and the CI jobs that only ran them. The fake LÖVE's own
   tests stay. Verified by no `src/` left, every target listed as kept or ported
   still present and green, `make e2e` under N, and CI green on the pushed
   branch.

### Phase 3 — Rules and docs

5. Update `CLAUDE.md`, `README`, `docs/love-port/design.md` and the
   `.claude/skills/`, and every other tracked file outside `game/` (other
   docs, e2e notes, tools), to describe a Lua repository. Verified by a
   search of all tracked files outside `game/` finding no current mention of
   `src/`, `tmp/replay` or the deleted targets, each remaining hit recorded
   in the step notes as a deliberate leftover for plan 5, and by step 2's
   `check-e2e` check run again.

## 4. Done when

- No C game code left; `make test-all` green; `make e2e` checks the 20
  replays with the fake LÖVE.
- `CLAUDE.md` holds the N-seconds rule, and `make e2e` runs under N.

## 5. Open questions

None.

## 6. Step notes

### Target inventory

Every target of `make help` at the start of the run. Kept: unchanged, needs
no C. Ported: rewired to the fake LÖVE or to frozen data. Deleted: only
serves the C.

- Kept: `help`, `test`, `test-all`, `unit` (its fake LÖVE binaries),
  `fake-love`, `fake-love-guard`, `love`, `love-capture`, `love-check`,
  `lua-guard`, `lua-test-runtime`.
- Ported: `e2e`, `end-state` (absorbing `love-e2e` and `love-e2e-state`,
  which go once absorbed), `e2e-baseline`, `end-state-baseline` (set the
  references from the fake LÖVE), `state`, `replay` (the fake LÖVE's state
  dump and capture of one replay), `lua-test-fixed`, `lua-test-modules`
  (frozen reference files).
- Deleted, C-only: `alloc-guard`, `button-guard`, `data-guard`, `p8-guard`,
  `pointer-guard`, `random-guard`, `float-guard`, `float-grep`,
  `float-link`, `constants-check` (they scan `src/`); `asset-export`,
  `sin-table`, `bindings-lua`, `demo-lua` (generators built from `src/`:
  the Lua files they wrote become the source); `coverage`, `e2e-order`,
  `e2e-order-check` (C line coverage); `e2e-ref` (plan rule); `checklist`,
  `lua-sweep-fixed` (compare the Lua with the C itself); `sanitize-net`,
  `full-game` (replay the C).

### Step 1.1

`make e2e` and `make end-state` run `tmp/fake_love` with the Lua game: one
shared run per replay under `tmp/love-e2e/` writes the capture, its hash
and the end state, longest replay first; `make e2e end-state` runs each
replay once. `make end-state EVENTS=<f> [REFERENCE=<r>]` runs one replay,
`worst-case/` included. `e2e-baseline` and `end-state-baseline` were ported
here, since they write the references these targets read; `state` and
`replay` stay on the C until step 1.2. `love-e2e` and `love-e2e-state` are
gone; `e2e-ref` stays until step 2.4, but a divergence only names
`make e2e-baseline`. N = 9 s: six cold runs on 4 cores, median 7.107 s,
rounded up plus 1 s. Both targets: 20 match.

### Step 1.2

Lua regression home: `tests/lua/game/test_<module>.lua`, found by
wildcard, one parallel `make test` target each (group `lua-test-game`),
plain LuaJIT with `game/` on the path, helper `check.lua`, seeded with
`economy_formulas`. `tools/capture_read.lua` prints a capture frame as
text and finds the first differing frame (`lua-test-capture-read`).
`check-e2e`, `fix-bug`, `help-me-debug` and `tdd` reproduce with
`tmp/fake_love`, debug with `capture_read` and the state dump, judge
against the `.sha256`/`.end-state` and a known-good `game/` from git; a C
test in `UNIT_TESTS` only for a bug in the fake. `state` and `replay`
were left to step 1.3. Proof: in a throwaway worktree, a one-line break of
`economy_formulas_cost_scale` made 5 replays diverge with `make test`
green; the loop reached a Lua regression test red on the break, green on
the revert, 8 ms, green with `src/` and `tmp/` moved aside. Deviation: the
skills' reference fallback (the commit that set the `.sha256`) has no
`game/`; `guideline` decided to walk the last 20 commits touching `game/`
for one whose capture matches. Timing (design.md rule 4, 22 rounds, 4
cores): plain medians 5.853 s current, 5.775 s before (778c235), 5.578 s
reference (8dff812); paired +0.88 % vs before (pass), +4.38 % vs reference
(fail); spreads 6.6 / 5.9 / 6.2 %; slowest new check 98 ms
(`capture_read`'s test). `guideline` decided to keep both checks: the
reference fail is inherited, 778c235 alone is +3.42 % over 8dff812.

### Step 1.3

`lua-test-fixed` reads `tests/lua/fixed_reference.txt` (5291 records:
every section, edge values in each input position, the random streams in
draw order, both double-product trap records); the generator writes text,
`test_lua_fixed` covers the reader. `lua-test-modules` reads
`tests/lua/table_reference.txt` (876 blocks); the four generated-data
modules are only loaded, their Lua files being the source.
`test_love_runtime` compares with frozen frames and meter counts per
fixture, written from the C, and is a fake-side binary. `make state` and
`make replay` run the fake (`EVENTS=` required; `SAVE_HEX`, `DIGEST`,
`ATTRACT_STATE`, `REPLAY_STARTS` and the no-`EVENTS` peaks mode dropped,
not a category change). `UNIT_TESTS` holds the 33 surviving tests; the
other 134 and the `src/` guards sit in one `ifneq ($(wildcard src),)`
group that step 2.4 deletes. Dry run (worktree, `src/` removed, `tmp/`
empty): every kept and ported target, step 1.2's targets and every
surviving CI command green, the baselines leave `tests/e2e/` untouched.
`lua-sweep-fixed` now takes ~12 s (text parsing), still outside every
gate until step 2.4 deletes it. Timing (rule 4, 22 rounds, 4 cores): plain
medians 5.239 s current, 5.347 s before (518369e), 5.049 s reference;
paired −0.66 % vs before, +3.01 % vs reference, kept on step 1.2's
`guideline` decision (inherited: before is +4.89 % over the reference);
spreads 13.3 / 10.7 / 11.5 %; slowest new check `test_lua_fixed`, 71 ms.
Left for step 3.5: `lua-test-modules` still prints "equal to C"; e2e
notes and README cite `SAVE_HEX` and bare `make state`.

### Step 2.4

Deleted `src/` (250 files), the 135 C tests, their helpers and 19
fixtures, `tests/e2e/order.txt`, the C-only tools (guards, checklist,
coverage, e2e order, `frames_ref`, state peaks, `fixed_reference`,
`table_reference`, `gen_quarter_wave`) and every Makefile rule and target
that served them, step 1.3's `src/` switch and the orphaned state-peaks
runs included: the Makefile went from 1551 to 493 lines, `make help`
lists the 21 kept and ported targets, no inventory amendment. CI's `test`
job runs `make test-all` only (`sanitize-net`, `full-game` gone); its run
on the pushed branch is green. `bindings.default.cfg` and `demo/` kept
for step 3.5 to decide. Every target green on an empty `tmp/` and on a
`tmp/` left from before. `make e2e`: six cold runs, median 5.864 s (< 9 s).
`make test` (rule 4, 11 rounds, 4 cores): plain medians 0.670 s current,
5.493 s before (60e62c1), 5.381 s reference; paired −87.41 % / −87.53 %;
spreads 19.0 / 13.9 / 13.6 %; no new check. No `.sha256` or `.end-state`
changed. Left for step 3.5: "equal to C", comments naming the deleted
generators, the "~3 s" `lua-sweep-fixed` line of `CLAUDE.md`.

### Step 3.5

`CLAUDE.md`, the README, `.gitignore`, `design.md`, `save-format.md`,
eleven skill files, the 20 e2e notes and `bindings.default.cfg` describe
the Lua repository; `lua-test-modules` prints "equal to the reference";
`demo/` deleted (its data is `game/game/demo_data.lua`). `warn-review`'s
pointer rules became reference rules on `game/` (rule 11 dropped);
`fix-bug` lost its sanitizer step. Re-running the e2e notes' readings on
the fake found `pad_planet_projects`' burst ending at f187 instead of the
C's f186 and three memory peaks that differ from the C (not covered by
any `.end-state`); the notes state both. Search of tracked files outside
`game/`: no current `src/`, `tmp/replay` or deleted target. Deliberate
leftovers for plan 5: `fake-love-guard` and its test; two negative
`e2e-ref` asserts; one `git show 60e62c1:src/...` history pointer each in
`CLAUDE.md` and `design.md`; the `game/` headers naming the C or the
deleted generators; nothing checks `bindings.default.cfg` against
`bindings_default.lua`; no target writes a save to a file; rule 4's
reference `8dff812` still has the C. `check-e2e` re-run in a throwaway
worktree (break in `economy_score_set_difficulty`, `pad_probe`): reference
found by the 20-commit walk, regression test red on the break, green on
the revert, 13 ms; no skill instruction failed.
