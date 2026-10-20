# Project notes

## The port

- One C game, the reference for the LÖVE port described in `docs/love-port/design.md`, until plan 4 deletes it. The C code is the reference: the game is covered by the e2e replays, and the cart's decompiled source is in the history of `slipways-psx` only: `git show 536ddd4:pico8/lua.lua` there. Game objects are flat C structs: a shared core (`entity_t`), one `static const` archetype per kind, and components held by pointer.
- Audio (`sfx`, `music`) is **out of scope**, not postponed.
- **The e2e replays are the source of truth, not the unit tests.** A change that improves the port and keeps every `*.sha256` in `tests/e2e/` unchanged may break a unit test that froze the old behavior: update that test.

## Layout and naming

- `src/` has six directories: `p8/` is the emulated PICO-8 runtime (symbols prefixed `p8_`, file names unprefixed); `engine/` is the object and memory layer, with no planets or slipways (`entity_t` and the classes in `entity_class.h`, registries, lists, zones, output queue); `game/` is the game; `platform/` is the PC fatal-error handler; `devtools/` is headless verification (dump, replay, capture, state); `cmd/` holds one file per binary. Includes are written relative to `src/` (`#include "game/planet.h"`, `-Isrc` only).
- `tests/` is flat, one `test_<module>.c` per module, with the replays in `tests/e2e/`; `tools/` holds the check scripts and guards.
- Names are full English words: no prefix for the game, `p8_` for the runtime only. An exported function is named `module_verb[_object]`. One- or two-letter names are only for a local or parameter whose scope fits in 40 lines.

## Language

Everything written in the repository is in English: code, comments, docs, e2e notes, skills, commit messages, PR titles and descriptions, PRDs. Older commits and PRs stay as they are.

## What we don't do

- **Never merge a pull request**, in any way (GitHub tool, `gh`, auto-merge). Merging is the user's job, always; denied in `.claude/settings.json`.
- **Never make `make test` or `make e2e` slower**, under any circumstances: the loop must stay fast. A slow check goes in a separate target, outside `make test-all`.
- **Never open a window or a virtual screen.** No window, no X server: an agent has neither eyes nor a mouse and would see nothing of what it opens. Verification is headless — N frames driven by a scripted `events.json`, then a text dump of the framebuffer (`make replay`).

## Build

- `make test` is the fast loop (unit tests and static checks, ~8 s). `make e2e` replays the e2e files of `tests/e2e/` from the title screen (~1 s). `make test-all` runs both. `make help` lists the targets: each public target carries a one-line `## ` description of at most 20 words.
- `test`, `e2e` and `test-all` hold `tmp/make.lock` through `flock` (macOS: `brew install flock`): a second one fails at once.

## Docs

`docs/` only describes what is true or still to do. Anything finished — a plan, a study, an implemented PRD — is deleted: git is the history.

## Comments

A comment stays only if it carries one of two things: a trap someone would reintroduce (an `assert` with a message, otherwise 40 words at most), or a C contract the name doesn't state (who frees the memory, what the function returns when the field is missing). `TODO` is allowed. In a `Makefile`, only help for reading a command stays. Everything else goes in the commit message.

To rework a file written before these rules: skill `rework-comments`.
