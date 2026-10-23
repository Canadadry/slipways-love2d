---
name: warn-review
description: Internal — called by plan-pilot at each phase end. Flags design smells and pointer-rule violations as bare warnings, never fixes.
---

# Warn Review

Flags design smells as warnings only. Never suggests fixes, causes, or direction - just states what's wrong and where. The caller (plan-pilot, through plan-review) decides what to do about them.

## Scope

- Given paths (plan-pilot passes the code a phase touched): review only those.
- Given no argument: recursively scan the current working directory. Respect `.gitignore`. Skip build output, vendor/third-party code, binary assets, and `.git`. Restrict to the project's main detected language's source files.

## What to flag

Design smells only - never correctness bugs, never simplification/efficiency nitpicks. This is a smell, not a science: judge holistically, don't apply numeric thresholds or ratios.

Checklist (each is an outside-in hint that something is off):

1. No lib/utils/shared package despite repeated logic across files - whoever built this assumed nothing here would ever be reused.
2. A comment block longer than the code it explains, on a path that isn't inherently complex - if it needs a paragraph to explain, is the code itself too hard to follow?
3. A generic name (History, Manager, Handler, Utils, Data...) whose actual behavior doesn't match that concept - read the file and judge the mismatch yourself, don't just pattern-match the name.
4. A file dramatically larger than everything else, especially an entry point - what is this hiding that should've been split out?
5. A module required/imported by nearly everything else - a hidden hub nobody can avoid depending on.
6. Config values or magic numbers scattered inline with no central place - nobody expected this to need tuning or reuse.
7. Dependency direction should form a tree: main depends on specific modules, specific modules depend on generic ones from lib - never the reverse. Tangled or flat imports with no such layering is a smell.
8. The internal design should be readable from folder/file names alone. If you can't infer the architecture just by browsing the tree, that's the smell.
9. The largest or most complex file has little to no direct test coverage, relying only on broader end-to-end checks - the code most likely to hide bugs is the code least protected against them.
10. A systematic audit/sweep's completeness claim (e.g. "N sites, all handled") with no mechanism (test, CI check, or process step) to catch new code later recreating the exact pattern it swept for - the confidence is frozen at audit time while the codebase keeps moving.

## Project ground rules: references

Check these too, on every run, in `game/`. Unlike the checklist above they are rules, not smells: flag every violation, still as a bare warning.

Why they exist, and the only two goals they serve:

- **Safety**: a link to something that died must fail loudly (`nil`, an `assert`), never reach a stale table silently, and must not depend on someone remembering to clear it.
- **Readability**: one name per piece of data, and a field whose meaning the code says, not a paragraph of comment about what `nil` means or who writes through it.

The rules are the means, not the goal. If code breaks one in a case nobody anticipated and the result is clearly safer or clearer than following the rule, don't flag it as a violation: report it as `file[:line]: breaks rule N on purpose`, so the user can check the reasoning. Following a rule in a way that defeats safety or readability is the violation.

Allowed table references: `self` and other tables as parameters (borrowed for the call only), an entity's own components (tables no one else holds), static data never changed after load (archetypes, data tables, strings), functions, runtime handles (the allocator table a module receives, the graphics backend, the vram), and the engine registry's internal lists. Anything else is a violation. Rule 11 (`const` pointer fields) has no Lua meaning and is dropped; the numbers of the others are kept so that warnings stay comparable across runs:

12. A field holding another field of the same table (`self.x = self.x_own`): the component is a value.
13. Presence or absence encoded by a bit or a `has_<x>` 0/1 number instead of a field that is `nil` when absent.
14. A link to another entity held as the entity's table, alone or in a list, instead of its id.
15. A dead entity's id left in a list instead of removed at its death, or a list walk that tolerates a dead id instead of asserting.
16. A by-value table (a `vec`, a `box`) changed by a function that receives it.
17. A function keeping a table it received as a parameter beyond the call.
18. A game concept (`targeting`, `emitter`, or any new one) named in `game/engine/`.
19. A regenerated e2e `*.sha256` or `*.end-state`, or a relaxed `assert`, in a change that removes references.
20. A truth value held as a `0`/`1` number, or a predicate tested against `0` or `1`.
21. An array indexed from 0, or a `[0]` key that is a position rather than a value (a colour, a pixel coordinate, an address, a byte, a disk code or rank, a layer, a bit position, an image slot handle).

## Output format

Plain markdown list, one line per finding: `file[:line]: should not <do Y>`.

- Include a line number only when a specific line makes sense (e.g. items 2, 3, 6); omit it for whole-file/structural smells (e.g. items 1, 4, 5, 7, 8).
- State the bare rule only. No "why", no failure scenario, no suggested fix, no fix direction.
- No severity ranking - flat list, in the order found.
- If nothing is found, say so plainly.

## When a finding doesn't fit the checklist

Report it the same way, prefixed `[off-checklist]`, so the reader can tell it apart.
