---
title: "make love-file and the archive completeness check"
description: "Nothing yet builds a .love file that is proven to run once unpacked, and nothing checks that every module the game requires and every PNG it loads is inside game/, so an archive for the phone could miss a file and nobody would know before trying it."
status: needs-triage
---

## Problem Statement

Plan 6 (`docs/plan-6.md`) ends with the game played on an iPhone in Love2D
Studio, which imports a `.love` file: a zip whose root is the game's root,
`main.lua` included. The repository already has a `make love` target that
zips `game/` into `tmp/slipways.love`, but:

- it has the wrong name for the plan (§2 "Packaging" and step 8 name it
  `make love-file`), and nothing proves the archive runs: nobody has played
  an unpacked `.love` and compared the result with a replay hash;
- it needs `zip`, which happens to be in the container but is installed by
  neither `tools/love_install.sh` (the SessionStart hook and CI) nor any
  other script, so a fresh machine can lack it with no clear message;
- nothing checks that the archive is *complete*. The game runs in the fake
  from `game/`, but a module required only inside a function body (the
  include cycle of `engine.entity_registry` / `engine.entity_geom`, for
  instance), or a PNG named in code that no replay reaches, could point at
  a file that does not exist under `game/`. The e2e replays would stay
  green, and the phone would be the first to fail. `lua-test-modules`
  requires every module at load, but a lazy `require` and a PNG name are
  invisible to it.

Step 8 can be done before the pad exists (steps 9–16): it does not depend
on the pad, and a build can be tried on the phone after step 7's present.

## Solution

1. **`make love-file`** replaces `make love`. It zips the content of
   `game/` into `tmp/slipways.love` with `main.lua` at the zip's root (as
   today's recipe does), then proves the archive:
   - it checks the archive lists `main.lua` at its root (the zip's own
     listing, not the source tree), failing with a message naming it;
   - it unpacks the archive into a fresh directory under `tmp/`, and runs
     the fake on that directory, through the same game-directory argument
     that `LOVE_E2E_GAME` feeds to `tools/love_e2e_run.sh`, for the replay
     `tests/e2e/pad_title.json` (32 frames, the smallest replay); the hash
     of the capture must equal the committed `pad_title.json.sha256`, or
     the target fails naming the replay and both hashes (or the fake's
     error);
   - it prints the archive path on success.
   It is outside `make test-all` and CI, with a `## ` help line of at most
   20 words. Whole run: ~0.3 s (zip ~90 ms, unzip ~15 ms, the replay
   ~155 ms, measured on a prototype of HEAD).
2. **The completeness check, inside `lua-guard`** (no new process, as plan
   §6 step 2.6's budget fixes): for every file the guard already reads,
   - each `require` of a module must name a literal string, and that
     module must resolve under the game root as the fake and LÖVE resolve
     it (`<root>/<a/b>.lua` or `<root>/<a/b>/init.lua`, dots turned into
     slashes), or be the one LuaJIT built-in the game uses, `bit`;
   - each string literal in code that names a PNG file must name a file
     that exists under the game root.
   A failure is one line per offence, `<file>:<line>: <reason>`, as the
   guard's other rules, and makes `make test` fail.
3. **`zip` and `unzip` are installed by `tools/love_install.sh`**, so the
   SessionStart hook and CI provide them; the target fails at once with a
   message naming `tools/love_install.sh` when either is missing (as
   `love-capture` does for `love` and `xvfb-run`).
4. Docs follow: `docs/love-port/design.md` (layout of the fork, the
   `lua-guard` paragraph of the code conventions) and `README.md` name
   `make love-file`, what it checks, and how the archive reaches the phone
   (import into Love2D Studio, by AirDrop or Files).

## User Stories

1. As a player with an iPhone, I want one command that writes `tmp/slipways.love`, so that I can AirDrop it into Love2D Studio and play.
2. As a player, I want `main.lua` at the archive's root, so that Love2D Studio recognises the file as a game.
3. As a player, I want the archive to contain the real `sprites.png` and `font.png` (and later `pad.png`), so that the game on the phone looks like the game on desktop.
4. As a player, I want the archive to carry step 7's `conf.lua` (resizable, high-DPI, 640×480), so that the phone build is the one the plan describes.
5. As a maintainer, I want `make love-file` to run the unpacked archive in the fake, so that "the archive runs" is proven, not assumed.
6. As a maintainer, I want that run to match a committed e2e hash exactly, so that the archive's game is byte-for-byte the tested game.
7. As a maintainer, I want the target to name the replay and both hashes when they differ, so that I see at once what diverged.
8. As a maintainer, I want the fake's own error shown when the unpacked game fails to boot, so that a missing file is named rather than hidden.
9. As a maintainer, I want the target to fail when `main.lua` is not at the archive's root, so that a recipe change that nests `game/` is caught.
10. As a maintainer, I want each run of the target to start from a fresh unpack directory, so that a file left by an earlier run cannot hide a missing one.
11. As a maintainer, I want the archive rebuilt from scratch each time it is out of date, so that a file deleted from `game/` does not survive in the zip.
12. As a maintainer, I want `make love-file` outside `make test` and `make test-all`, so that the fast loop does not pay for zipping.
13. As a maintainer, I want `make love-file` not to share `tmp/love-e2e/` with `make e2e`, so that running both at once corrupts neither.
14. As a maintainer, I want `make love-file` to take well under a second, so that I can rebuild a phone build at will.
15. As a maintainer, I want a one-line `## ` help for `love-file` in `make help`, so that the target is discoverable (CLAUDE.md).
16. As a maintainer, I want the old `make love` name gone rather than kept as a second name, so that the docs and the Makefile name one target.
17. As a maintainer, I want `zip` and `unzip` installed by `tools/love_install.sh`, so that the SessionStart hook and CI provide them on a fresh machine.
18. As a maintainer, I want a clear message when `zip` or `unzip` is missing, so that I know to run `tools/love_install.sh`.
19. As a maintainer, I want `tools/love_install.sh` to keep skipping apt when every package is already installed, so that a session start stays fast.
20. As a developer, I want `lua-guard` to fail when a module the game requires is not under `game/`, so that an incomplete archive is caught by `make test`.
21. As a developer, I want a `require` hidden inside a function body checked as well as one at the top of a file, so that the lazy requires of an include cycle are covered.
22. As a developer, I want a `require` with a non-literal argument refused, so that the check stays total and no module name escapes it.
23. As a developer, I want `require("bit")` accepted, so that LuaJIT's built-in bit library, which the game uses, is not a false offence.
24. As a developer, I want a module that resolves to `<dir>/init.lua` accepted, so that the check follows the same search path as LÖVE and the fake.
25. As a developer, I want `lua-guard` to fail when a PNG named in code is not under `game/`, so that a renamed or forgotten image is caught before the phone.
26. As a developer, I want a PNG name in a comment ignored, so that writing about `sprites.png` in a comment raises nothing.
27. As a developer, I want a message such as `"sprites.png: pixel (%d, %d) …"` not to count as a PNG file name, so that the check reads only literals that are exactly a file name.
28. As a developer, I want each completeness offence reported as `<file>:<line>: <reason>` among the guard's other offences, so that the output reads as one list.
29. As a developer, I want the completeness check to add only a millisecond or two to `lua-guard`, so that `make test` does not slow down.
30. As a developer, I want the guard's existing rules (compilation, globals, the LÖVE list) unchanged, so that this step adds a rule without moving others.
31. As a reviewer, I want the guard's new cases tested in the guard's existing C test with at most one new guard run, so that the test costs a few milliseconds at most.
32. As a reviewer, I want the existing guard test whose fixture requires a module it does not provide fixed by providing the module, so that the test keeps checking what it was written for.
33. As a reviewer, I want the 20 replays unchanged, their `*.sha256` and `*.end-state` byte for byte, so that this step is proven to change no game behaviour.
34. As a reviewer, I want the timing gate passed and its medians reported, so that the plan's budget record stays exact.
35. As a reviewer, I want design.md and the README to name `make love-file` and the completeness rule, so that docs describe only what is true.
36. As a phone tester, I want the README to say how to import the archive into Love2D Studio, so that I do not have to guess.
37. As a phone tester, I want the README to point at the known risk on high-DPI canvases (step 7's further notes), so that a blurry or gappy picture is diagnosed quickly.
38. As a later step's implementer (16), I want `make love-file` to keep passing once `pad.png` is in `game/`, so that "`make love-file` still passes" is a one-command check.

## Implementation Decisions

**Hypotheses on step 7 (not yet done; its PRD is
`docs/prd/triage/phase2-step7-present-in-game-rectangle.md`).** Every one
is checked first by the implementer; one that fails is reported, and the
decision that rests on it is revisited.

- **H-7-1**: step 7 is committed and `make test-all` is green on it; its
  last commit is this step's "before" commit for the timing gate.
- **H-7-2**: step 7 closed its gate with at least one point of margin
  against the plan's start commit 8c830c6 (its PRD aims at about −8 %, at
  worst +2 %). Step 8's cost (~1–2 ms in `lua-guard`, one guard run of
  ~4 ms in `test_lua_guard`, ≈ 0.2 point) fits without a reclaim. If the
  margin is under 0.5 point, the new guard cases are folded into an
  existing `run_guard` call instead of a new one (zero new process).
- **H-7-3**: step 7 left unchanged the interfaces this step uses:
  `luajit tools/lua_guard.lua <game dir> <file.lua>...`, the
  `tests/test_lua_guard.c` fixture helpers (`reset`, `write_file`,
  `run_guard`, `out_has`, `out_lines`), `tools/love_e2e_run.sh`'s eight
  arguments, `LOVE_E2E_GAME`, the `love` target's recipe, and
  `tools/love_install.sh`'s package list.
- **H-7-4**: after step 7, every `require` under `game/` takes a literal
  name that resolves under `game/` or is `bit`, and every PNG literal names
  `sprites.png` or `font.png`, both under `game/`: the new rule is green on
  step 7's tree with no game change. `game/` still holds `main.lua`,
  `conf.lua`, `sprites.png` and `font.png` at its root.
- **H-7-5**: step 7 kept `tests/e2e/pad_title.json` and its `.sha256`
  unchanged (the 20 replays keep their hashes) and the replay has no
  `window` member, so the unpacked game plays it at 640×480 with the same
  capture.
- **H-7-6**: the fixture replay of step 7's screen check and its expected
  frames live under `tests/`, not `game/`, so the archive does not ship
  them.
- **H-7-7**: step 7 did not add `love-file` or `zip`/`unzip` to any target
  or script, and design.md's "Layout of the fork" still names `make love`.

**The Makefile.**

- The `love` target and its phony declaration are renamed `love-file`; no
  `love` alias remains. Its help line (at most 20 words), for example:
  "zips game/ into tmp/slipways.love, then plays one e2e replay from the
  unpacked archive in tmp/fake_love".
- The archive rule keeps its shape: rebuilt when any file under `game/`
  changes, the old archive removed first, zipped from inside `game/` so
  the zip's root is the game root, `-X` (no extra attributes). Every file
  under `game/` ships; nothing is excluded (the devtools state dump is
  required by `main.lua`).
- `love-file` depends on the archive and on `tmp/fake_love`, and is
  always run (phony: the proof runs each time, ~0.3 s). Its recipe:
  1. both `zip` and `unzip` on the path, else
     `love-file: zip or unzip missing, run tools/love_install.sh`, exit 1
     (the `zip` check is in the archive rule too, since that rule may run
     alone);
  2. `main.lua` listed at the archive's root (`unzip -Z1 <archive>
     main.lua` succeeds), else `love-file: tmp/slipways.love has no
     main.lua at its root`;
  3. the unpack directory `tmp/love-file/game` removed and recreated, the
     archive unpacked into it;
  4. `tools/love_e2e_run.sh` with `$(SHA256_CMD)`, `./tmp/fake_love`, the
     unpack directory as the game directory (the same slot `LOVE_E2E_GAME`
     fills for `make e2e`), `tests/e2e/pad_title.json`, and its outputs
     under `tmp/love-file/` (never `tmp/love-e2e/`);
  5. on an `.error` file: `love-file: FAILED tests/e2e/pad_title.json`
     followed by the error's lines, exit 1; on a hash different from the
     committed `tests/e2e/pad_title.json.sha256`: `love-file: FAILED
     tests/e2e/pad_title.json, capture <hash> instead of <committed>`,
     exit 1; otherwise `love-file: tmp/slipways.love (pad_title.json
     matches its .sha256)`.
- The replay is fixed in the recipe, not a variable: one replay is what
  the plan asks, `pad_title` is the smallest (32 frames) and boots the
  game, reads `sprites.png` and `font.png` and presents every frame. Running
  all 20 replays (~7 s) was rejected: it would compete with `make e2e` for
  `tmp/love-e2e/` or duplicate its rules for no extra coverage of the
  archive (a missing file fails at boot or is caught by the completeness
  check).
- `love-file` is not added to `LOCKED_GOALS`: it writes only
  `tmp/slipways.love` and `tmp/love-file/`, and builds `tmp/fake_love`
  exactly as the unlocked `make replay` does today. CLAUDE.md's list of
  locked targets therefore stays true.

**`tools/love_install.sh`.** `zip` and `unzip` join the package list
(Ubuntu 24.04 packages of the same names). The skip-when-installed path is
unchanged, so a session where both are present (the current container,
GitHub's `ubuntu-24.04` runner) pays nothing; the header comment names them.

**The completeness rule in `tools/lua_guard.lua`.** One more pass over each
file the guard already reads and strips, no new process, no new argument:

- **Requires.** A `require` word in code (not in a comment or string, not a
  field such as `x.require`) must be followed, after optional spaces and an
  optional `(`, by a short string literal; otherwise
  `<file>:<line>: require takes a literal module name`. The module name,
  dots turned into slashes, must exist as `<root>/<name>.lua` or
  `<root>/<name>/init.lua` (the fake's and LÖVE's search path), or be in a
  built-in list holding `bit` only; otherwise
  `<file>:<line>: module <name> is not under <root>/`. Existence is tested
  once per module name (cached), by opening the file.
- **PNGs.** A short string literal in code whose whole content is a file
  name ending in `.png` (letters, digits, `_`, `-`, `.`, `/` only) must
  name an existing file `<root>/<name>`; otherwise
  `<file>:<line>: image <name> is not under <root>/`. A literal with other
  text around the name (a `string.format` message) is not a file name and
  is ignored; a name in a comment is ignored because comments are stripped.
- Positions come from the stripped text (comments and strings blanked,
  newlines kept, as the guard already does), so lines match; the literal's
  content is read from the original text at the same position.
- Offences join the guard's single list, printed and failing as today; the
  success line is unchanged.
- The rule covers every PNG the game *names*, which today means
  `love.image.newImageData("sprites.png")` and `("font.png")`, and after
  step 9 `pad.png`. A file the game reads from the save directory
  (`bindings.cfg`, the save file through `love.filesystem.read`) is not
  shipped and not checked.
- Why in `lua-guard` and not a new target: plan §6 step 2.6 places it there
  (~1–2 ms), and `lua-guard` already compiles and reads every file once.
  Why static and not by requiring: `lua-test-modules` loads every module but
  misses requires made inside functions and never sees PNG names.

**Docs.**

- `docs/love-port/design.md`: "Layout of the fork" says `make love-file`
  zips `game/` into `tmp/slipways.love` and plays one replay from the
  unpacked archive; the code-conventions paragraph on `lua-guard` adds that
  every `require` takes a literal name resolving under `game/` (or `bit`)
  and every PNG named in code is a file under `game/`. The "nothing in
  these plans targets iOS" line is left to the step that ships the pad
  (plan §4).
- `README.md`: `make love-file` replaces `make love`; the line says the
  archive runs in LÖVE 11.5 and is imported into Love2D Studio by AirDrop or
  Files, and that until the pad exists (steps 9–16) the game still needs a
  gamepad.

## Implementation Chunks

1. **The completeness rule.** In `tests/test_lua_guard.c`, one new test
   (one guard run) over a fixture root holding: a module required at top
   level and one required inside a function body, both present (no
   offence); `require('bit')` (no offence); a module resolving through
   `<dir>/init.lua` (no offence); a missing module; a `require(name)` with a
   variable; a present PNG literal and a missing one; a PNG name in a
   comment and inside a longer message (no offence). It asserts each
   expected line exactly and the line count. The existing
   `test_an_undeclared_global_read_fails_with_file_line_and_name` gets a
   `p8/fixed.lua` in its fixture so its line count stays two. Then the rule
   in `tools/lua_guard.lua`; `make lua-guard` green on `game/` (H-7-4).
2. **`make love-file` and the tools.** The renamed target and its proof,
   `zip`/`unzip` in `tools/love_install.sh`, design.md and README. Verified
   by hand: `make love-file` passes; `unzip -l tmp/slipways.love` lists
   `main.lua` with no directory prefix; a deliberately broken archive (for
   instance `main.lua` removed from a scratch copy) makes the recipe's
   checks fail with their messages (run in `tmp/`, not committed).
3. **Gate and report.** The timing gate (design.md "`make test` and the
   fake's checks", rule 4) against the "before" commit (H-7-1), 8dff812 and
   8c830c6; `make e2e` and `make end-state` 20/20; the report.

## Testing Decisions

- **A good test here** runs the real guard on real files and reads its
  output, as `tests/test_lua_guard.c` does: it never calls the guard's
  internal functions or stubs `io.open`. For the archive, the proof is the
  real fake on the real unpacked zip against a committed hash.
- **Tested in `make test`:** the completeness rule, by one new case in
  `test_lua_guard` (prior art: that file's
  `test_a_love_name_off_the_list_fails_but_not_in_strings_or_comments`, a
  fixture of several files, exact lines and a line count); and on the real
  tree by `lua-guard` itself, which must stay green.
- **Not in `make test`:** `make love-file` (it zips, which `make test`
  never does; plan §2 keeps it outside `make test-all`). It is its own
  test: the unpacked archive must reproduce `pad_title.json.sha256`.
- **Timing.** The step adds a check to `make test` (the guard rule and one
  guard run), so plan §2's timing gate applies: 11 rounds, paired medians
  against the "before" commit, 8dff812 and 8c830c6, each ≤ 3 %, and plan
  §2's ≤ 3 % against 8c830c6. Expected cost ≈ 0.2 point (H-7-2). The step
  measures and reports `lua-guard`'s wall time on `game/` before and after
  (today ~105 ms) and `test_lua_guard`'s own time before and after. The step
  does not change the fake's per-frame work or the game, so no `make e2e`
  timing is required; `make e2e` must stay 20/20.
- **Check of the step:** `make test-all` green; the new guard case passes;
  `make love-file` passes; `unzip -l tmp/slipways.love` shows `main.lua` at
  the root; the 20 replays unchanged (`make e2e`, `make end-state`);
  `make help` lists `love-file` with a help line of at most 20 words and no
  longer lists `love`; design.md and the README name `make love-file`; the
  gate passes.

## Out of Scope

- Writing plan §6 (the coordinator does, from the report).
- Any fake LÖVE change (frozen after phase 1).
- Any game change: the present, `conf.lua` (step 7), the pad, its sheet and
  `pad.png` (steps 9–16).
- Delivering the archive to the phone (the user's: AirDrop or Files);
  Love2D Studio's git sync (rejected by the plan).
- Running all 20 replays from the unpacked archive.
- Checking files the game reads from the save directory (`bindings.cfg`,
  the save file), and checking that `game/` holds no unused file.
- Reproducible (timestamp-free) archives.
- design.md's "nothing in these plans targets iOS" line (plan §4, the
  pad's steps).
- The high-DPI canvas risk itself (step 7's further notes): the phone trial
  may reveal it; a fix is its own step.

## Further Notes

- **Prototype** (scratch directory, not committed): HEAD's `game/` exported
  with `git archive`, zipped with today's recipe (`zip -q -9 -X -r`, ~90
  ms), unpacked (~15 ms); `unzip -Z1 <archive> main.lua` prints `main.lua`;
  `tools/love_e2e_run.sh` with the unpacked directory on
  `tests/e2e/pad_title.json` took ~155 ms and its hash equalled the
  committed `70cb3def…ffe7b`. The approach holds at HEAD; H-7-5 carries it
  over step 7.
- **Tools present.** In this container `zip` and `unzip` are installed
  (Debian packages `zip`, `unzip`), as is `python3`, but neither
  `tools/love_install.sh` nor CI installs any of them; GitHub's
  `ubuntu-24.04` image ships `zip` and `unzip`. Adding the two packages to
  the install script was preferred over a LuaJIT or Python zip writer: the
  Lua side has no deflate, Python is not guaranteed either, and the plan
  names `unzip -l` for verification.
- **Measured today:** `luajit tools/lua_guard.lua game <102 files>` ~105 ms
  wall; one guard run on a one-file fixture ~4 ms; `test_lua_guard` makes
  13 guard runs.
- The archive ships `game/devtools/state_format.lua`; `main.lua` requires it
  for `debug_state`, so leaving it out would break the boot.
