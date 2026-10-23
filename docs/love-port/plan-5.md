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

### Decisions of phase 0

- The Lua no longer mirrors the C: files, functions, order and names may
  diverge freely. Comparing with the C side by side is given up; the hashes
  and end states are the only net.
- Memory zones and allocators go entirely, with no Lua stand-in, and their
  `SIZEOF` entries and the `cap` lines of the state dump with them (the
  `.end-state` files filter `cap` out). A capacity limit stays only where it
  is a game rule (the save format's 255 nodes and slipways).
- Entity ids become plain numbers, never reused, resolved through one
  `id → entity` table that drops an entity at its death. The
  `{ index, generation }` tables, `entity_id_none()` and `entity_id_equal`
  go.
- `entity_list`, `pointer_list`, `entity_id_list` and `value_list` become
  plain 1-based Lua arrays. A traversal that tolerates changes says so (a
  copy, or a removal deferred to the exit queue) instead of the `last_count`
  iterator. `collect` becomes local loops; the exit queue stays, as a plain
  array.
- Every array is 1-based, data tables included.
- Predicates return booleans; `x ~= 0` tests and `and 1 or 0` on truth
  values go.
- An entity's optional parts are fields that are `nil` when absent; the `has`
  bits go.
- Exported names drop the repeated module prefix (`planet.init_instance`,
  `p8_` inside `p8/` too); `CLAUDE.md`'s naming rule changes with them.
- `lua-test-modules` keeps only "every module loads": the comparison with
  `tests/lua/table_reference.txt` and the file go. `lua-guard` stays. Each
  `tests/lua/game/` test changes with the module it tests.
- The `-- Copy of src/...` headers go. `cint.div` and `cint.mod` stay (LuaJIT
  has no integer division), as do `goto continue` and the call-time
  `require` of include cycles.
- `design.md` § Conventions inherited from the C is replaced by the
  conventions the code follows.

### Phase 1 — The engine foundation

1. `lua-test-modules` checks only that every module loads. Seen when
   `tests/lua/table_reference.txt` is gone and `make test` is green.
2. Zones and allocators removed. Seen when `game/engine/zone.lua`,
   `allocator.lua` and the other zone modules are gone and
   `grep -r SIZEOF game/` finds nothing.
3. Entity ids as plain numbers. Seen when
   `grep -rE 'generation|entity_id_equal|entity_id_none' game/` finds nothing.
4. The four list kinds as plain arrays. Seen when `entity_list.lua`,
   `pointer_list.lua`, `entity_id_list.lua`, `value_list.lua` and
   `collect.lua` are gone and `grep -r last_count game/` finds nothing.

### Phase 2 — The entity model

1. Optional parts as `nil`-able fields. Seen when
   `grep -rE 'ENTITY_HAS_|entity_has\(' game/` finds nothing.

### Phase 3 — The surface

One kind per step, split by directory (`p8/`, `engine/`, `game/` with
`devtools/`) when it is too large to fail fast.

1. Booleans. Seen when `grep -rE 'and 1 or 0|~= 0 then' game/` finds only
   tests on numbers that are not truth values, each listed in the step note.
2. 1-based arrays. Seen when `grep -rE '\[0\] *=' game/` finds nothing.
3. The C headers. Seen when `grep -r -- '-- Copy of' game/` finds nothing.
4. Names without the module prefix, with `CLAUDE.md`'s naming rule. Seen
   when no exported function of a module starts with the module's name.
5. `design.md` § Conventions inherited from the C replaced. Seen when the
   section is gone and no convention left in `design.md` names a C construct.

## 4. Done when

- Every C-ism listed in phase 0 is handled as decided: every step after
  phase 0 has its own check holding.
- `make test-all` green and `make e2e end-state` passing, every hash and end
  state unchanged.
- `docs/love-port/design.md` and `CLAUDE.md` describe the code as it stands.
- `make e2e` under 9 s.

## 5. Open questions

- None.

## 6. Step notes

### Step 0.1

Grilling held with the user on 2026-10-22/23. The user wants the code clean
for its own sake (no feature or port depends on it) and agreed to every
decision under "Decisions of phase 0" and to the order of phases 1–3.
Checked during the grilling: the save format depends on no memory layout;
`end_state.sh` filters the `cap` lines, so removing the zones changes no
`.end-state`.
