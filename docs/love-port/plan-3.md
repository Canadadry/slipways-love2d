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

### Phase 1 — The diff tool

1. C side: the replay can write the state dump after **every** frame
   (`STATE_EACH_FRAME=<file>`), in the format of `debug_state()`. Lua side:
   the fake LÖVE can call `debug_state()` after every frame the same way.
   Verified by both dumps of `pad_title.json`'s first frame agreeing on the
   lines both sides already produce.
2. `make e2e-diff E=<replay>`: runs the C replay and the fake LÖVE on the
   same file and prints the first frame whose pixels differ and how many
   pixels differ, writes `tmp/e2e-diff/<replay>.png` (C, Lua, differing
   pixels highlighted, side by side) and the lines of the two state dumps that
   differ at that frame. Verified on a deliberately broken Lua colour.

### Phase 2 — The replays

3. One step per replay of `tests/e2e/order.txt`, in order: run
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
