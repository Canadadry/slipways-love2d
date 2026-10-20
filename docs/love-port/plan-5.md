# Plan 5 — Clean the Lua into natural Lua

## 1. Goal

Turn the faithful copy of the C (plan 2) into Lua that reads as Lua, with the
20 e2e as the safety net: every step keeps every capture sha256 and every end
state unchanged. Read `docs/love-port/design.md` first.

## 2. Rules

- Every step keeps `make test-all` green with every `.json.sha256` and
  `.end-state` unchanged. A step that changes one is reverted, not re-baselined.
- One module (or one kind of C-ism) per step, small enough to fail fast.
- The fixed-point format and the random generator stay: they are what makes
  the hashes match, not a C-ism.
- Not this plan: new features, iOS.

## 3. Phases and steps

### Phase 0 — Decide what "natural" means

1. A grilling (`/grill-me`) with the user over a list of the C-isms found in
   `game/` — for example manual memory zones and allocators, registries and
   id lists, the output queue, components held by pointer, `_t` names and
   module prefixes, fixed-size arrays — deciding for each: keep, replace with
   what, in which order. The decisions are written into this plan's phases
   below before any code change. Verified by the user's consent.

### Phase 1 — The steps decided in phase 0

(written by step 1)

## 4. Done when

- Every C-ism listed in phase 0 is handled as decided.
- `make test-all` green, every hash and end state unchanged.

## 5. Open questions

- The whole of phase 0.

## 6. Step notes
