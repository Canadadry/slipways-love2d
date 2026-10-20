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
- `make e2e-diff` goes with the C: from now on a divergence is read from the
  Lua state dumps and the capture alone.

## 3. Phases and steps

### Phase 1 — Cut the tests loose from the C

1. Save a few thousand fixed-point and random-generator results (inputs and
   outputs, edge values included) to `tests/lua/fixed_reference.txt`; the Lua
   tests read that file instead of the C binary. Verified by the tests green
   with the C binary removed from their target.

### Phase 2 — Delete

2. Delete `src/`, the C unit tests, the C-only tools and guards
   (`asset_export`, coverage, float/pointer/alloc guards, …) and their
   Makefile targets. `make e2e` and `make e2e-state` now run the fake LÖVE.
   Verified by `make test-all` green and no `src/` left.
3. `make love` zips `game/` into `tmp/slipways.love`. Verified by the archive
   listing `main.lua`, `conf.lua` and the PNGs.

### Phase 3 — Rules and docs

4. Measure `make e2e` (6 cold runs, median); write the N-seconds rule into
   `CLAUDE.md`. Update `CLAUDE.md`, `README` and `docs/love-port/design.md`
   to describe a Lua repository. Verified by reading: no mention of the
   deleted C left as current.

## 4. Done when

- No C game code left; `make test-all` green; `make e2e` checks the 20
  replays with the fake LÖVE.
- `CLAUDE.md` holds the N-seconds rule.

## 5. Open questions

None.

## 6. Step notes
