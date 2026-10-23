# Project notes

## The game

- A Lua game in `game/`, played by LÖVE 11.5 and, headless, by the fake LÖVE (`tools/fake_love/`, C). The 20 replays of `tests/e2e/` and their hashes, set by the C game before plan 4 deleted it, are the reference. The C is in history only: `git show 60e62c1:src/<path>`. The cart's decompiled source is in the history of `slipways-psx` only: `git show 536ddd4:pico8/lua.lua` there. Game objects are flat Lua tables: a shared header (the header fields listed at the top of `game/engine/entity_class.lua`), one class table per kind, optional parts as fields that are `nil` when absent, and links to other entities held as plain number ids (`nil` for none). The design is `docs/love-port/design.md`.
- Audio (`sfx`, `music`) is **out of scope**, not postponed.
- **The e2e replays are the source of truth, not the unit tests.** A change that improves the game and keeps every `*.sha256` in `tests/e2e/` unchanged may break a unit test that froze the old behavior: update that test.

## Layout and naming

- `game/` has `main.lua` and `conf.lua` (LÖVE's entry points) and five directories: `p8/` is the emulated PICO-8 runtime (symbols prefixed `p8_`, file names unprefixed); `engine/` is the object layer, with no planets or slipways (entities, ids, classes, the registry, geometry, the exit queue); `game/` is the game; `love/` draws through LÖVE (backend, screen, sprite sheet, text); `devtools/` is the state dump. A module is required by its path under `game/` (`require("game.planet")`).
- `tests/` holds the C tests of the fake and the tools (`tests/test_<name>.c`, listed in `UNIT_TESTS`), the Lua library tests and their frozen references (`tests/lua/`), the game regression tests (`tests/lua/game/test_<module>.lua`), the fixtures (`tests/fixtures/`) and the replays (`tests/e2e/`). `tools/` holds the fake LÖVE, the real-LÖVE capture programs (`tools/love_capture/`) and the check scripts.
- Names are full English words: no prefix for the game, `p8_` for the runtime only. An exported function is named `module_verb[_object]` (`M.planet_init_instance`). One- or two-letter names are only for a local or parameter whose scope fits in 40 lines.

## Language

Everything written in the repository is in English: code, comments, docs, e2e notes, skills, commit messages, PR titles and descriptions, PRDs. Older commits and PRs stay as they are.

## What we don't do

- **Never merge a pull request**, in any way (GitHub tool, `gh`, auto-merge). Merging is the user's job, always; denied in `.claude/settings.json`.
- **Never make `make test` slower**, under any circumstances, **and never let `make e2e` run longer than 9 seconds**: the loop must stay fast. A slow check goes in a separate target, outside `make test-all`.
- **Never open a window or a virtual screen.** No window, no X server: an agent has neither eyes nor a mouse and would see nothing of what it opens. Verification is headless — N frames driven by a scripted `events.json`, then a text dump of the framebuffer (`make replay`). Single exception: Xvfb with Mesa software rendering, only inside `make love-capture`, which runs the capture programs in real LÖVE 11.5, rewrites their PNGs and stops; agents and CI may run it; it is never part of `make test` or `make e2e`.

## Build

- `make test` is the fast loop (unit tests and static checks, ~1 s). `make e2e` replays the e2e files of `tests/e2e/` from the title screen with the Lua game in `tmp/fake_love` (`LOVE_E2E_GAME`, default `game`) and checks each capture against its `.json.sha256` (~7 s on 4 cores). `make test-all` runs both. `make help` lists the targets: each public target carries a one-line `## ` description of at most 20 words.
- `tools/love_install.sh` installs LÖVE 11.5, Xvfb, Mesa and LuaJIT; the SessionStart hook and CI run it.
- `make end-state` checks the Lua end states of the same runs against each `.end-state`; `make e2e end-state` runs each replay once. It is outside `make test-all` and CI.
- `test`, `e2e`, `test-all`, `end-state`, `e2e-baseline` and `end-state-baseline` hold `tmp/make.lock` through `flock` (macOS: `brew install flock`): a second one fails at once.

## Docs

`docs/` only describes what is true or still to do. Anything finished — a plan, a study, an implemented PRD — is deleted: git is the history.

## Comments

A comment stays only if it carries one of two things: a trap someone would reintroduce (an `assert` with a message, otherwise 40 words at most), or a contract the name doesn't state (what the function returns when the field is missing). `TODO` is allowed. In a `Makefile`, only help for reading a command stays. Everything else goes in the commit message.

To rework a file written before these rules: skill `rework-comments`.
