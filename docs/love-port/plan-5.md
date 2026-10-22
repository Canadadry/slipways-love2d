# Plan 5 — Clean the Lua into natural Lua

## 1. Goal

Turn the faithful copy of the C (plan 2) into Lua that reads as Lua, with the
20 e2e as the safety net: every step keeps every capture sha256 and every end
state unchanged. Read `docs/love-port/design.md` first.

## 2. Rules

- Every step keeps `make test-all` green and passes `make e2e end-state`,
  with every `.json.sha256` and `.end-state` unchanged. End states are outside
  `test-all` and check-e2e: `plan-pilot` runs `make e2e end-state` after
  check-e2e is green and before the step note. A red result goes through
  `fix-bug` as check-e2e does; the step is reverted only when that path
  stops. Never re-baselined.
- Each step note records how long `make e2e` took. Over 9 s is red, like a
  changed hash: fixed in the step, reverted only when that fails.
- A step that finds a phase-0 decision cannot be carried out as written stops
  `plan-pilot`, which reports instead of asking `guideline`. So do a reverted
  step (the report says why) and a phase-end `warn-review` warning that bears
  on a decision for a later step; other warnings go to `plan-review`. The
  decision is reopened with the user, the plan goes through `/plan-converge`
  again, and the run restarts at that step.
- Each step updates or deletes, in the same step, the parts of
  `docs/love-port/design.md` and of `CLAUDE.md` § The game that it makes
  false: later steps are planned from them.
- One module (or one kind of C-ism) per step, small enough to fail fast.
- The fixed-point format and the random generator stay: they are what makes
  the hashes match, not a C-ism.
- Not this plan: new features, iOS.

## 3. Phases and steps

### Phase 0 — Decide what "natural" means

Attended: run in its own session with the user, before `plan-pilot`. Once
phase 1 is written, rerun `/plan-converge` on this plan, then start
`/plan-pilot` at phase 1. If phase 1 still reads "(written by step 1)",
`plan-pilot` stops and reports instead of running phase 0.

1. A grilling (`/grill-me`) with the user over a list of the C-isms found in
   `game/` — for example manual memory zones and allocators, registries and
   id lists, the output queue, components held by pointer, `_t` names and
   module prefixes, fixed-size arrays — and the checks that freeze them
   (`lua-test-modules` and its reference, which can no longer be
   regenerated, `lua-guard`, the `tests/lua/game/` tests), deciding for
   each: keep, replace with what, in which order. The proposed order follows
   dependencies, then puts first the steps whose decision may not keep the
   hashes; a check that freezes a structure is dealt with no later than the
   step that breaks it. Each later step states how its result shows in the
   repository, false before the step and true after; its note records that
   check. The steps are grouped into as many phases as useful, each ending
   where a `warn-review` of the code touched so far could still change the
   steps after it. The decisions are written
   into this plan's phases below before any code change, and the step ends,
   in the same attended session, by writing its `### Step 0.1` note: the
   decisions and the user's agreement. Verified by that note.

### Phases 1..N — The steps decided in phase 0

(written by step 1)

## 4. Done when

- Every C-ism listed in phase 0 is handled as decided: every step after
  phase 0 has its own check holding.
- `make test-all` green and `make e2e end-state` passing, every hash and end
  state unchanged.
- `docs/love-port/design.md` and `CLAUDE.md` describe the code as it stands.
- `make e2e` under 9 s.

## 5. Open questions

- The whole of phase 0.

## 6. Step notes
