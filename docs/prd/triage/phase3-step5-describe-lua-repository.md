---
title: "Docs, rules and skills describe a Lua repository"
description: "After step 4 deletes the C game, CLAUDE.md, the README, design.md, the skills, the e2e notes and the kept tools and tests still describe src/, deleted targets and C-only recipes, so every reader and every agent is told to use things that no longer exist."
status: needs-triage
---

## Problem Statement

After step 4, the repository holds the Lua game (`game/`), the fake LÖVE (`tools/fake_love/`, C), its tests and the Lua tests. Every text a human or an agent reads first still describes the C repository:

- `CLAUDE.md` opens with "One C game, the reference … until plan 4 deletes it", describes `src/`'s six directories, `-Isrc` includes, "flat C structs", "one `test_<module>.c` per module", a `lua-sweep-fixed` bullet and a lock list naming it, and a comment rule about "a C contract".
- `README.md` says the game is "ported to C, kept here as the reference", lists `make coverage`, `e2e-order`, `e2e-order-check`, `asset-export`, `checklist` and a bare `make state`, and says "replays and tests compile" `bindings.default.cfg`, which nothing compiles any more.
- `docs/love-port/design.md` says the C is the reference "until plan 4", puts `float-guard` first in `make test`, says `make e2e` reads nothing "from `src/` nor `tmp/frames/`", says `make asset-export` writes the PNGs and `make test` fails when they are stale, describes `lua-sweep-fixed`, `checklist`, `order.txt`, `e2e-order-check`, `TABLE_REFERENCE_SOURCES` and the generators that write the generated Lua data, maps every Lua file to a live `src/` file, and cites `docs/psx-game.md`, which is not in the repository.
- `docs/save-format.md` documents `src/game/save_state.c`, `src/game/save_state.h` and the C `save_check_*` devtools, and tells a reader to re-cut a fixture with commands that no longer exist.
- The skills: `warn-review` checks its pointer rules "in `src/`" and cites `tools/pointer_guard_exceptions.txt`; `rework-comments` measures `cloc src/`, compiles `make src/obj/module.o` and gates on C headers only; `tdd`'s `mocking.md` backs a C allocator with the deleted `tests/heap_allocator.h`, and its examples include `game/trade.c`; `fix-bug` says "`src/` is not a reference" and runs a sanitizer build that is gone; `check-e2e` says "no `src/` needed"; `git-commit`'s example lists `src/game/planet.c`.
- The e2e notes in `tests/e2e/`: four give `make replay … SAVE_HEX=` recipes, one `make state / make replay … SAVE_HEX=`; dozens of readings are bare `make state at fN`, which now fails without `EVENTS=`; two cite `make coverage` and a `src/game/*.c` file; `attract.txt` re-baselines through `tmp/frames/`; others cite targets and tools gone before plan 4 (`make resume-check`, `make demo-check`, `make pad-metrics`, `make pad-metrics-check`, `make pad-author-proof`, `./tmp/pad_author`), the deleted `test_cart`, and "Cost:" timings of the C e2e (`tmp/frames removed`).
- Kept tools and tests: `lua-test-modules` prints "lines equal to C" and "names differ from C" and labels the reference side `C`. The header of `tests/lua/fixed_reference.txt` and the format comments of `tests/lua/test_fixed.lua`, `tests/lua/test_modules.lua` and `tests/test_lua_modules.c` name `tools/fixed_reference.c` and `tools/table_reference.c`. `tests/lua/test_runtime.lua` cites `src/p8/draw_cost.c`. `tests/test_fake_love_png_file.c` asks "did make asset-export change the format?".
- `demo/` (`demo.json`, `demo_record.hex`) was only the input of the deleted `demo-lua` generator and of deleted C tests. `game/game/demo_data.lua` holds the same data and is now the source.

An agent following `CLAUDE.md` or a skill runs a command that fails, or looks for a directory that is gone. A human reading the README sees a C project.

## Solution

Rewrite every tracked file outside `game/` that describes the C, so it describes what is there: a Lua game for LÖVE 11.5, checked headless by a C fake LÖVE on the 20 replays whose hashes the C once set. The rules that keep:

- **Present tense for what exists.** Every current command, path and target cited exists after step 4.
- **Past tense, no command, for what the C proved.** A verification an e2e note records keeps its result, stated as done with the C (or with a tool since removed). The command is dropped, and git keeps it.
- **History pointers to the C are allowed only where plan 5 needs them** (reading a Lua function against its C original). They name the last commit that holds `src/`, and each one is recorded as a leftover for plan 5.
- **`game/` is not touched.** Its comments naming `src/` or deleted targets are plan 5's.
- **`docs/love-port/plan-5.md` is not touched.** It mentions the C only as the origin of the C-isms plan 5 removes, which stays true. It names no `src/`, `tmp/replay` or deleted target.

Then the step proves it in two ways. A search of all tracked files outside `game/` finds no current mention of `src/`, `tmp/replay`, `tmp/frames` or any deleted target, and every hit left is recorded. Step 1.2's `check-e2e` dry run is run again: a one-line Lua break in a throwaway worktree reaches `fix-bug` with a fake-LÖVE reproduction and a regression test that `make test` runs.

## User Stories

1. As an agent starting a session, I want `CLAUDE.md`'s first section to say the game is Lua in `game/`, played headless by the fake LÖVE and judged by the 20 replays, so that I never look for a C reference.
2. As an agent, I want `CLAUDE.md` to say where the deleted C is in git history (commit `60e62c1`, the last one holding it), so that I can read a Lua function's C original when plan 5 needs it without thinking it is live.
3. As an agent, I want `CLAUDE.md`'s layout section to describe `game/`'s directories (`p8/`, `engine/`, `game/`, `devtools/`, `love/`, `main.lua`, `conf.lua`) with what each holds, so that I place new code correctly.
4. As an agent, I want the module rule to read "required by its path under `game/`" instead of "includes relative to `src/`, `-Isrc` only", so that I follow the rule the code uses.
5. As an agent, I want `CLAUDE.md`'s `tests/` description to name the C tests of the fake and tools (`tests/test_<name>.c`, `UNIT_TESTS`), the Lua library tests and frozen references (`tests/lua/`), the game regression tests (`tests/lua/game/test_<module>.lua`), the fixtures and the replays, so that I put a test where `make test` finds it.
6. As an agent, I want the game-object sentence rewritten to describe the Lua tables the game actually uses, checked against `game/engine/`, so that I don't write C structs.
7. As an agent, I want the `lua-sweep-fixed` bullet gone and the `flock` lock list to name only existing targets, so that `CLAUDE.md` matches `make help`.
8. As an agent, I want the comment rule to say "a contract the name doesn't state" with a Lua-valid example, so that the rule applies to the code I edit.
9. As a reader of the README, I want its first paragraph to say what the repository is now, a Lua port of Slipways for LÖVE 11.5 checked headless, so that I know what I am looking at.
10. As a reader of the README, I want its command list to show only existing targets with their required arguments (`make replay EVENTS=…`, `make state EVENTS=…`, `make end-state`), so that every line I copy works.
11. As a player, I want the README's Controls section to say that the game's default bindings are `game/game/bindings_default.lua`, that a personal `bindings.cfg` overrides them, and that `bindings.default.cfg` documents that format with every mode and action, so that I can rebind the pad.
12. As a maintainer, I want `demo/` deleted once a search shows nothing reads it, and `attract.txt` to cite `game/game/demo_data.lua` instead, so that the repository holds no dead generator input.
13. As a maintainer, I want each `.gitignore` entry that only the C build wrote removed, so that the ignore list describes the tools that exist.
14. As a reader of `design.md`, I want its introduction and goal to state the 20 hashes as the reference (set by the C game, now deleted), so that "until plan 4 deletes it" no longer misleads.
15. As a reader of `design.md`, I want rule 1 of "`make test` and the fake's checks" rid of the claim that `test` lists its longest prerequisites first (`float-guard`, `float-grep`), since those are gone and step 4 did not re-measure the order, so that the rule states only what holds.
16. As a reader of `design.md`, I want the `make e2e` section rid of "reads nothing from `src/` nor `tmp/frames/`", so that it describes only the fake's run.
17. As a reader of `design.md`, I want "The asset PNGs" to say the PNGs are committed sources, edited directly and checked by the capture fixtures and the replays, so that no one looks for `make asset-export`.
18. As a reader of `design.md`, I want "Numbers and randomness" to describe `game/p8/fixed.lua` and its frozen reference, with no `lua-sweep-fixed`, so that the section matches `make help`.
19. As a reader of `design.md`, I want "Layout of the fork" to list the directories that exist (`game/`, `tools/fake_love/`, `tools/love_capture/`, `tools/` scripts, `tests/` C tests, `tests/lua/`, `tests/lua/game/`, `tests/e2e/`, `worst-case/`), so that the map is right.
20. As a plan-5 agent, I want "Reading the Lua against the C" rewritten as the conventions the Lua follows today, inherited from the C copy, with one pointer to the C in history, so that I can still put a function next to its original while knowing the C is gone.
21. As a reader of `design.md`, I want the data-table paragraph to say the generated-data modules are now the source, edited by hand, and that `lua-test-modules` compares tables with a reference frozen from the C, with no `checklist`, generators or `TABLE_REFERENCE_SOURCES`, so that it describes the check that runs.
22. As a reader of `design.md`, I want the dangling reference to `docs/psx-game.md` removed and the pad table kept, so that no cited file is missing.
23. As a reader of `docs/save-format.md`, I want it to document `game/game/save_state.lua` (its module, entry points and constants as the Lua names them), so that the page describes code that exists.
24. As a reader of `docs/save-format.md`, I want the visual-random paragraph to describe how the Lua game keeps the flicker identical across a resume, or to drop the C devtools it cites, so that it names nothing deleted.
25. As a maintainer, I want `docs/save-format.md` to say that no target writes a save to a file today, and that re-cutting `pad_resume_at_start`'s fixture needs one (a leftover for plan 5), so that nobody runs a dead `SAVE_HEX` recipe.
26. As `warn-review`, I want my pointer rules to apply to `game/`, stated in their Lua form (an entity link held as an id, not as the entity's table; presence as a bit in `has`, not a nil field; no dead id left in a list; a by-value table never mutated by its receiver; no reference kept beyond the call; no game concept in `game/engine/`), so that I check the code that exists.
27. As `warn-review`, I want the rules that have no Lua meaning (`const` pointer fields) dropped and the deleted `pointer_guard_exceptions.txt` gone from the last rule, so that no rule cites a missing file.
28. As `rework-comments`, I want my scope, measure and gates to cover Lua (`cloc` over `game/`, `tools/` and `tests/`; a stripped-bytecode comparison through `luajit -b` as the Lua byte gate; `make lua-guard` as the Lua compile check) and keep the C gates for the fake's C, so that the skill runs on this repository.
29. As `rework-comments`, I want my anecdotes kept in the past tense without a live path (`src/`), and my agent brief to have a Lua variant (one `.lua` file, `assert(cond, "msg")`), so that the method still teaches without pointing at a deleted tree.
30. As `tdd`, I want my examples for game code in Lua against real `game/` functions, and C examples only from `tools/fake_love/`, so that I copy patterns that compile in this repository.
31. As `tdd`, I want `mocking.md` to name the Lua boundaries the game has (the allocator table a module receives, the scripted events, the graphics backend read back as a capture), with no `tests/heap_allocator.h`, so that a double I write has something to replace.
32. As `fix-bug`, I want the "`src/` is not a reference" sentence and the sanitizer check removed, and "sanitizer" removed from what can surface a bug, so that I never ask for a build that is gone.
33. As `check-e2e`, I want "no `src/` needed" removed from the reproduction, so that the wording matches a repository without it.
34. As `git-commit`, I want the example's file list to name a Lua module and its Lua test, so that the example matches real commits.
35. As `help-me-debug`, I want the sentence on the e2e notes' state readings to match how the notes now write them, so that I can read a note's reading against a state file.
36. As a reader of an e2e note, I want every state reading to come from `make state EVENTS=<this file cut after fN>` (stated once, with the cut command `help-me-debug` gives), the inline readings saying "the state at fN", so that every command I see works.
37. As a reader of an e2e note, I want each `SAVE_HEX` recipe rewritten as a past-tense record ("cut from <commit>'s replay with the C game's save dump, since removed") keeping its printed results, so that I know how the fixture was made without being handed a dead command.
38. As a reader of an e2e note, I want the `make coverage` claims kept as facts measured with the C, naming the Lua module (`message_box.lua`, `menu.lua`) instead of the C file, so that the reason the replay exists survives.
39. As a reader of an e2e note, I want the verifications made with tools removed earlier (`resume-check`, `demo-check`, `pad-metrics`, `pad-metrics-check`, `pad-author-proof`, `tmp/pad_author`, `test_cart`) kept as past-tense results without their commands, so that the note is true and complete.
40. As a reader of an e2e note, I want C source names (`slipway_display.c`, `probe.c`) replaced by the Lua module of the same name, so that I can open the file.
41. As a reader of an e2e note, I want the "Cost:" paragraphs that time the C e2e deleted, so that no note gives a measurement of a run that no longer exists; the N-seconds rule in `CLAUDE.md` is the current cost.
42. As a maintainer, I want `attract.txt`'s re-baseline block through `tmp/frames/` deleted, so that the note keeps the "never re-baseline" rule of the fix loop.
43. As a developer, I want `lua-test-modules` to print "lines equal to the reference", "names differ from the reference", and to label the frozen side `reference` instead of `C` in each difference, so that its output describes what it compares.
44. As a developer, I want `test_lua_modules` to check the new wording, so that the output change is pinned by a test.
45. As a developer, I want the header of `tests/lua/fixed_reference.txt` and the format comments of `tests/lua/test_fixed.lua`, `tests/lua/test_modules.lua` and `tests/test_lua_modules.c` to say the file was frozen from the C (deleted in plan 4) without naming a deleted path, so that no comment sends me to a missing generator.
46. As a developer, I want `tests/lua/test_runtime.lua`'s draw-cost comment to cite the Lua module, and `test_fake_love_png_file`'s refusal message not to name `make asset-export`, so that kept tests name only what exists.
47. As the plan pilot, I want a search of every tracked file outside `game/` for `src/`, `tmp/replay` (other than `tmp/replay_dump.txt`, which `make replay` writes), `tmp/frames`, `SAVE_HEX`, "equal to C" and each deleted target name, with every remaining hit listed and justified, so that the step's check is mechanical.
48. As the plan pilot, I want every `make <target>` cited outside `game/` to be a target of `make help` or one of its pattern targets, every cited repository path to exist, and every cited `test_<name>` to be an existing test, so that the targets and tools removed before plan 4 are caught too.
49. As the plan pilot, I want step 2's `check-e2e` dry run repeated in a throwaway worktree after the skills are rewritten, so that the rewritten skills are shown to drive the fix loop.
50. As a maintainer, I want every `*.sha256` and `.end-state` unchanged and `make test-all` green after the step, so that the docs pass touched nothing the replays judge.
51. As the plan pilot, I want the evidence (the search output, the leftovers with reasons, the dry run's record) handed to the "Step 3.5" note, so that plan 5 starts from a written list.

## Implementation Decisions

### Search terms and what counts as a hit

- **Scope**: `git ls-files`, minus `game/`, minus the run records that end with plan 4 (`docs/love-port/plan-4.md` and `progress.md`, which plan-pilot removes once the plan is done, and `docs/prd/`, whose files prd-autopilot removes).
- **Terms**: `src/`; `tmp/replay` not followed by `_dump.txt`; `tmp/frames`; `SAVE_HEX`; `equal to C`; each deleted target name of the inventory (`alloc-guard`, `button-guard`, `data-guard`, `p8-guard`, `pointer-guard`, `random-guard`, `float-guard`, `float-grep`, `float-link`, `constants-check`, `asset-export`, `sin-table`, `bindings-lua`, `demo-lua`, `e2e-order`, `e2e-order-check`, `e2e-ref`, `lua-sweep-fixed`, `sanitize-net`, `full-game`) plus `love-e2e` and `love-e2e-state`. The generic words `coverage` and `checklist` are searched only as `make coverage`, `make checklist` or in backticks, since warn-review uses both words in their ordinary meaning.
- **Target and path audit**, wider than the plan's terms, because the e2e notes cite targets removed before plan 4. Every `make <word>` cited is in `make help`, or is a pattern target (`lua-run-test_<module>`, `unit-run-test_<name>`, `love-capture-check-<name>`). Every cited path under `docs/`, `tools/`, `tests/`, `game/`, `worst-case/`, `.claude/` or `.github/` exists. Placeholders with `<…>` and paths given with a git revision are skipped. Every cited `test_<name>` names an existing `tests/test_<name>.c`, `tests/lua/test_<name>.lua` or `tests/lua/game/test_<name>.lua`. These audits are throwaway shell or Lua run from `tmp/`, not committed. Adding a guard to `make test` is not needed for this step (guideline rule "question why the change is asked").
- **Hits allowed to remain**, each recorded in the step note as a deliberate leftover for plan 5:
  - `tools/fake_love_guard.sh`, `tests/test_fake_love_guard.c` and `fake-love-guard`'s help line: the guard exists to fail if `src/` comes back. The inventory keeps it; plan 5 decides whether it still earns its place.
  - The negative asserts `make e2e-ref` must not appear in the logs of `test_frames_check` and `test_love_e2e`. Removing an assertion weakens a test (guideline: no test weakened), and they still guard the reports against naming a target that is gone.
  - The history pointers to the C (story 2, story 20): at most one in `CLAUDE.md` and one in `design.md`, as `git show 60e62c1:src/<path>`, `60e62c1` being the last commit holding `src/` (H-4-11). Plan 5 uses them to read a Lua function against its C original and removes them when its conventions no longer follow the C.
  - Any hit inside a file listed as a plan-5 leftover by step 4's PRD that this step could not reword without touching `game/`.

### Files and their new content

- **`CLAUDE.md`**:
  - "The port" becomes "The game": the Lua game in `game/`, played by LÖVE 11.5 and headless by the fake LÖVE. The replays and their hashes, set by the C game before plan 4 deleted it, are the reference. The C is reachable at the commit named once (history pointer). The `slipways-psx` line stays. The game-object sentence describes the Lua tables of `game/engine/` as the code has them. Audio and source-of-truth bullets stay.
  - "Layout and naming" describes `game/`, `tests/` and `tools/` as in stories 3–5. The names rule stays, applied to Lua (`M.module_verb_object`, `p8_` in `game/p8/` only). The 40-line rule stays.
  - "Build": the `lua-sweep-fixed` bullet goes, and the lock list drops it. `make test`'s approximate time becomes "~1 s", as its help line already says (0.670 s median in step 2.4, H-4-8). The N-seconds rule is unchanged.
  - "Comments": "a C contract" becomes "a contract", with the example "what the function returns when the field is missing" kept and "who frees the memory" dropped (Lua has no free). The rework-comments line stays.
  - "What we don't do", "Language" and "Docs" are unchanged.
- **`README.md`**: a new first paragraph (story 9), and the screenshots kept. "Checking the reference" becomes "Checking the game", with `test`, `e2e`, `test-all`, `end-state`, `replay EVENTS=`, `state EVENTS=`, `love-capture` and `help`, each with its output path as the Makefile writes it (H-4-9). "Playing" is unchanged. "Controls" is rewritten as in story 11. "To do" is unchanged.
- **`bindings.default.cfg`** stays. It is the documented example of the `bindings.cfg` format the game parses (`game/main.lua` reads a personal `bindings.cfg`), and the two `.cfg` e2e fixtures are written as edits of it. Nothing checks that it matches `game/game/bindings_default.lua` any more: this is recorded as a leftover for plan 5, not fixed here, since adding a check is not this step's work.
- **`demo/`** is deleted after a search of all tracked files shows nothing outside `attract.txt` names it (H-4-5). `game/game/demo_data.lua` holds the frames, the captions and the record (`demo_data_record`, 3852 bytes) that `demo/` held. Decided under the guideline rule "make the plan work": a dead generator input contradicts "describe a Lua repository", and keeping it would need a sentence explaining a file nothing reads. The header of `demo_data.lua` (`Generated by make demo-lua … from demo/demo.json`) is in `game/` and stays a plan-5 leftover.
- **`.gitignore`**: each entry is checked against the surviving Makefile, tools and fake (what writes it). An entry only the C build or the C PC binary wrote goes. `tmp/` and `.claude/worktrees/` stay.
- **`docs/love-port/design.md`**: rewritten section by section as stories 14–22 say. The fake-LÖVE contract sections (pad semantics, capture, canvas, points, rectangles, colour, images, transforms, filter, function list, captures) only change where they name the C as live: "as strict as the C replay" becomes a statement of the replay format itself, and "the capture format is the C replay's" becomes "the format the `.json.sha256` were set with". Rule 4's protocol and its reference commit `8dff812` are not changed: changing a gate is a rule change, not a description, and this step only describes.
- **`docs/save-format.md`**: the module and entry points are those of `game/game/save_state.lua`, read from the code. The constants keep their names (the Lua exports them as `M.SAVE_STATE_*`). `save_state_t` becomes the table `save_state_decode` fills. The visual-random paragraph describes what `game/` does on resume (read from `game/game/visual_random.lua` and its callers), or drops the sentence if the Lua keeps nothing beside the record. The re-cut paragraph follows story 25. If the Lua names differ from the page anywhere, the page follows the Lua: the page describes the code, as its own rule says.
- **Skills**, as in stories 26–35. `warn-review`'s rule numbers are kept where a rule survives, so that warnings stay comparable across runs, and the dropped numbers are skipped. `rework-comments/worked-example.md` stays unchanged: it is an explicit worked example of the C port, told in the past. `plan-pilot`'s example PR title ("match the C") stays: it is a quoted past title. `rework-comments/header_residue.sh` stays for C headers (the fake's).
- **E2e notes** (`tests/e2e/*.txt`), as in stories 36–42. Replay files, hashes, end states and the `.cfg` fixtures are not touched.
- **`lua-test-modules` wording**: `tests/lua/test_modules.lua`'s summary and difference lines say `reference` where they say `C`. Values printed from the reference file (`&SOFTWARE`) are unchanged. `tests/test_lua_modules.c` checks the new lines.
- **Comments in kept tests and data**: `tests/lua/fixed_reference.txt`'s `#` header (skipped by the reader, so the 5291 records are unchanged), `tests/lua/test_fixed.lua`, `tests/lua/test_modules.lua`, `tests/test_lua_modules.c` and `tests/lua/test_runtime.lua`, as in stories 45–46. `test_fixed.lua`'s standard-input mode, which only `lua-sweep-fixed` used, stays: removing code is not this step's (H-4-13). Its comments no longer name `lua-sweep-fixed`.
- **Makefile and CI**: no change expected (H-4-3). If the search finds a hit there, the hit is fixed in this step and recorded as a step-4 leftover.

### The dry run (step 2's check, again)

- Same protocol as step 1.2. In a throwaway `git worktree` under `tmp/` on this step's last commit, a one-line change to a `game/` module that makes at least one replay diverge, with `make test` still green before the fix loop. Then `check-e2e` runs as written in the rewritten skills.
- The break is a different line from step 1.2's (`economy_formulas_cost_scale`), chosen so that a different module's test home is exercised. If no other module gives a divergence that a sub-50 ms test can pin, the step-1.2 line is reused and the reason recorded.
- Pass: `check-e2e` reaches `fix-bug`. The bug report carries a `tmp/fake_love` reproduction and a reference found by the skill's walk. The commit gate accepts a `tests/lua/game/test_<module>.lua` that is red on the break, green on the fix, under 50 ms, and run by `make test`. `make e2e` is green again. The worktree and its branch are removed afterwards. Nothing reaches the real branch.
- Recorded for the note: the line broken, the count of diverging replays, the test file, its red/green results and time, and any skill instruction that failed or misled (fixed in this step, then the dry run repeated once).

### If step 4 left something behind

If a hypothesis below is false (a deleted target still listed, a tool still present, a doc already changed), this step describes the tree as it is. A leftover of step 4 inside the Makefile or CI is fixed here and recorded. A category change in the inventory is not made here: it is reported to plan-pilot as an amendment.

## Implementation Chunks

1. **`lua-test-modules` wording and kept test comments.** First `test_lua_modules.c`'s expected lines change to the `reference` wording (red), then `test_modules.lua` (green). Then the comment-only edits in `fixed_reference.txt`'s header, `test_fixed.lua`, `test_modules.lua`, `test_lua_modules.c` and `test_runtime.lua`, and `test_fake_love_png_file`'s message. Green: `make test`.
2. **`CLAUDE.md`, `README.md`, `.gitignore`, `demo/`.** Delete `demo/` after its search. Green: `make test-all`.
3. **`design.md` and `save-format.md`.**
4. **Skills**: `warn-review`, `rework-comments` (with `agent-brief.md` and `report-template.md`), `tdd` (`SKILL.md`, `tests.md`, `mocking.md`, `interfaces-design.md`), `fix-bug`, `check-e2e`, `help-me-debug`, `git-commit`.
5. **E2e notes**, every `tests/e2e/*.txt`, under the rules of stories 36–42.
6. **Prove and record.** Run the search and the audits on the committed tree, fix any hit that is not an allowed leftover, then run the `check-e2e` dry run. Confirm `git status --porcelain tests/e2e/ worst-case/` shows only `.txt` changes, and that `make test-all` is green. Hand plan-pilot the search output, the leftover list with reasons, and the dry-run record for the "Step 3.5" note.

## Testing Decisions

- A good test here checks what a user of the tool sees (its printed lines and exit status), not how the script builds them. `test_lua_modules` already drives `lua-test-modules` on fixture modules and checks its output lines; it is the prior art and the only test this step changes, for the only behavior it changes.
- No other module gets a test. The rest of the step is text, verified by the search and the audits (throwaway, not committed) and by the dry run.
- `make test` and `make e2e` must stay green after each chunk. No `.sha256` or `.end-state` changes.
- Timing: this step adds no check and changes no check's cost (two output strings), so design.md rule 4's protocol is not run. `make e2e` stays under 9 s without any change to it.
- Prior art: step 1.2's dry run (protocol and record), step 1.3's and step 2.4's searches.

## Out of Scope

- Any change under `game/`, including the "Copy of src/…" and "Generated by make …" headers and the comments naming the C (plan 5).
- `docs/love-port/plan-5.md`.
- The Makefile and CI, except a step-4 leftover the search finds.
- New checks or guards (a docs grep in `make test`, a check that `bindings.default.cfg` matches `bindings_default.lua`, a save dump in the fake).
- Changing design.md's rules or gates (rule 4's reference commit, the N-seconds rule).
- Re-baselining anything, or changing a replay or its fixture.
- Removing `test_fixed.lua`'s standard-input mode or `fake-love-guard`.

## Further Notes

- Leftovers for plan 5 already known, to land in the "Step 3.5" note with the search output: the `game/` headers and comments (listed by step 4's PRD); the history pointers to the C; `fake-love-guard`; the two negative `e2e-ref` asserts; `bindings.default.cfg` unchecked against `bindings_default.lua`; no way to write a save to a file (needed to re-cut `pad_resume_at_start`'s fixture); design.md rule 4's reference commit, which still has the C and now always passes.
- `fix-bug` loses its sanitizer check because no sanitizer build survives (H-4-7). A memory bug in the fake is then caught only by its C tests' asserts. This is recorded as an observation, not a decision of this step.

### Hypotheses on step 4

Step 4 is done: commits `039404a` to `8b5946f` and the plan's "Step 2.4" note. Each hypothesis has been checked against the committed tree.

- **H-4-1** (confirmed, one number corrected): `src/` is gone (250 files). The 135 C tests outside `UNIT_TESTS` (not 134), their helpers (`heap_allocator`, `png_reader`, `save_storage_contract`) and the 19 root fixtures of `tests/fixtures/` are deleted. `UNIT_TESTS` keeps its name and its 33 tests of the fake and the tools; `tests/` holds exactly those 33 `test_*.c`.
- **H-4-2** (confirmed): `make help` lists exactly the 21 targets `help`, `test`, `test-all`, `unit`, `fake-love`, `fake-love-guard`, `love`, `love-capture`, `love-check`, `lua-guard`, `lua-test-runtime`, `e2e`, `end-state`, `e2e-baseline`, `end-state-baseline`, `state`, `replay`, `lua-test-fixed`, `lua-test-modules`, `lua-test-game`, `lua-test-capture-read`. The pattern targets `lua-run-test_%`, `unit-run-test_%` and `love-capture-check-%` remain. No inventory amendment was made.
- **H-4-3** (confirmed): the Makefile (493 lines) names `src/` only in `fake-love-guard`'s help line, and no deleted target. CI's `test` job is the install plus `make test-all`; its run on the pushed branch was green.
- **H-4-4** (confirmed): the C-only tools, guards, exception lists and generators are deleted, `tests/e2e/order.txt` too. `tools/` holds exactly `capture_read.lua`, `e2e_limits.sh`, `e2e_long_exceptions.txt`, `end_state.sh`, `fake_love/`, `fake_love_guard.sh`, `frames_check.sh`, `love_capture/`, `love_e2e_report.sh`, `love_e2e_run.sh`, `love_e2e_state_report.sh`, `love_install.sh`, `lua_guard.lua` and `progress_check.sh`.
- **H-4-5** (confirmed): `bindings.default.cfg` and `demo/` are kept "for step 3.5 to decide". Outside `game/`, only `tests/e2e/attract.txt` and the plan file name `demo/`. `game/` is untouched.
- **H-4-6** (confirmed): step 4 changed only the Makefile, CI, the plan's "Step 2.4" note and `progress.md` outside the deletions. `CLAUDE.md`, `README.md`, `design.md`, the skills, `tests/lua/` and the e2e notes are unchanged, so every hit in the Problem Statement is still there.
- **H-4-7** (confirmed): no sanitizer build survives; neither the Makefile nor `tools/` mention one.
- **H-4-8** (corrected): step 4 re-measured `make test` (0.670 s median, 11 rounds, 4 cores; `~1 s` in its help line), but did not re-order `test`'s prerequisites. The `test` line is step 1.3's minus the C-only groups: `unit`, `$(FAKE_LOVE_CHECKS)`, `lua-test-fixed`, `lua-test-runtime`, `lua-guard`, `lua-test-modules`, the Lua game test runs, `lua-test-capture-read`, `fake-love-guard`. So design.md rule 1 drops its claim of a longest-first order (`float-guard`, `float-grep`, the slowest unit tests) rather than naming a new one (story 15). `CLAUDE.md` says `~1 s`. Rule 1's reason for listing long checks early still holds as a principle and stays, worded without the deleted names. Decided under the guideline rule "between two options that both work, take the smaller change": measuring and re-ordering is not this step's work.
- **H-4-9** (confirmed): `make replay EVENTS=` writes `tmp/replay_dump.txt` and its capture under `tmp/love-replay/`, and `make state EVENTS=` writes `tmp/state.txt`. Both still require `EVENTS=`.
- **H-4-10** (corrected): the "Step 2.4" note leaves to step 3.5 only "equal to C", the comments naming the deleted generators, and `CLAUDE.md`'s "~3 s" `lua-sweep-fixed` line. The `SAVE_HEX` and bare `make state` leftovers come from the "Step 1.3" note, and the `make coverage` lines of two e2e notes come from step 4's PRD. The `game/` headers are plan 5's per step 4's PRD; neither note mentions them. Nothing else changes: this PRD already covers all of these.
- **H-4-11** (confirmed, with a hash): `src/` was deleted in `039404a`, whose parent `60e62c1` is the last commit holding the C. The history pointers name `60e62c1` (for example `git show 60e62c1:src/game/planet.c`).
- **H-4-12** (confirmed): `test_frames_check`, `test_love_e2e` and `test_fake_love_png_file` are unchanged, with their `make e2e-ref` asserts and their `make asset-export` message.
- **H-4-13** (confirmed): `tests/lua/test_fixed.lua` keeps its standard-input mode (`path == "-"`). `lua-test-fixed` reads `tests/lua/fixed_reference.txt` and skips `#` lines.
