# Plan 3 — The 20 e2e, one by one

## 1. Goal

Make the Lua game of `game/` produce, in the fake LÖVE, the same capture
sha256 as the C game on each of the 20 replays of `tests/e2e/`, one replay at
a time, in the order of `tests/e2e/order.txt` (fewest C lines executed first).
Read `docs/love-port/design.md` first.

## 2. Rules

- Order: `tests/e2e/order.txt`, top to bottom. A replay is worked on only when
  every replay above it matches; a step never leaves an earlier replay
  diverging.
- The `.json.sha256` files are never changed: the C game is the reference.
  A divergence is a bug in the Lua game (or the fake LÖVE), fixed there.
- A fix keeps the copy faithful: the Lua function is corrected toward its C
  original, not redesigned.
- A missing LÖVE function is not added to the fake without its capture
  program (plan 1's rule) and a line in `docs/love-port/design.md`.
- `make e2e` (C) stays green throughout; `make love-e2e` shows progress.

## 3. Phases and steps

### Phase 1 — The state check

1. `make love-e2e-state`: the runner passes a state path to the fake LÖVE and
   checks each Lua end state against its `.end-state`. Verified both ways:
   `make love-e2e-state` and `make love-e2e` both report 20/20 on the
   current tree, and a deliberately wrong reference is reported as
   differing. If both report 20/20, step 1's note commit also gives steps
   2, 3 and 4 each a note ("not needed: step 1 found 20/20 on both checks")
   and ticks their `progress.md` items, and deletes the step-2 PRD written
   ahead in `docs/prd/next/`, so the pilot goes straight to "Done when"; a replay failing either check sends the plan to Phase 2.

### Phase 2 — The diff tool (only if a replay diverges)

2. C side: the replay can write the state dump after **every** frame
   (`STATE_EACH_FRAME=<file>`), in the format of `debug_state()`. Lua side:
   the fake LÖVE can call `debug_state()` after every frame the same way.
   Verified by both dumps of `pad_title.json`'s first frame agreeing on the
   lines both sides already produce.
3. `make e2e-diff E=<replay>`: runs the C replay and the fake LÖVE on the
   same file and prints the first frame whose pixels differ and how many
   pixels differ, writes `tmp/e2e-diff/<replay>.png` (C, Lua, differing
   pixels highlighted, side by side) and the lines of the two state dumps that
   differ at that frame. Verified on a deliberately broken Lua colour.

### Phase 3 — The diverging replays

4. One step per diverging replay, in the order of `tests/e2e/order.txt`: run
   `make e2e-diff`, fix the Lua game until its sha256 matches, check every
   replay above still matches, and its `.end-state` matches through
   `make love-e2e-state`. Verified by `make love-e2e` showing that replay and
   every earlier one matching.

## 4. Done when

- `make love-e2e`: the 20 replays match their `.json.sha256`.
- `make love-e2e-state`: the 20 end states match their `.end-state`.
- `make e2e` (C) still green.

## 5. Open questions

None.

## 6. Step notes

### From plan 1

- `make love-e2e-state` does not exist yet, and step 4, "Done when" and
  plan 4's start condition and step 2 depend on it. What exists:
  `tmp/fake_love <game> <replay> <capture> [<state>]` writes `debug_state()`
  to the optional fourth argument; `make love-e2e` passes no state path, and
  its runner (`tools/love_e2e_run.sh`) can be extended to. Step 1 builds it.
- `make love-e2e` is proven both ways: a game that fails to load gives 20
  FAILED, a fixed stub 20 DIVERGENT, and a stub redrawing the C captures
  matches all 20 hashes. A divergence is therefore the Lua game's, or a
  fake LÖVE rule the captures did not settle.

### From plan 2

- `make love-e2e` matches all 20 replays on the closing commit of plan 2:
  hence step 1's confirmation before any diff tool or per-replay step.
- The rules for reading a Lua function against its C original (names,
  0-indexed arrays, `cint`, out-parameters as extra returns, `has_`, module
  loading) are in design.md § Reading the Lua against the C.
- Left open, with no check in the tree: the four planet fields Lua leaves
  nil (`sprite`, `palette_block`, `acceleration_factor`, `speed`) are read
  nowhere, by audit only; no code compares the score panel's text
  addresses, by audit only; `game.pad_nav` and `game.save_state` have no
  direct test (their C-vs-Lua checks were throwaway programs).
