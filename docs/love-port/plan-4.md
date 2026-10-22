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
