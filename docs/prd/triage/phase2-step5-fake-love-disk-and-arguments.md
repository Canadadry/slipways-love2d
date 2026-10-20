---
title: "fake_love gives the game its disk and its arguments"
description: "The fake LÖVE ignores a replay's initial_save, bindings and attract_after and offers no love.filesystem, so no game that loads a save, reads its bindings or honours --attract-after can be checked headless."
status: needs-triage
---

## Problem Statement

After plan steps 3 and 4, `tmp/fake_love` loads a LÖVE game, plays the
replay's pad and calls `love.update`/`love.draw` once per frame. Three
top-level members of the replays still mean nothing to it:

- `initial_save` (5 of the 20 e2e replays): the hex bytes of the save the C
  replay seeds before the title screen. The Lua game will read its save
  through `love.filesystem.read("save.bin")`, which does not exist in the
  fake.
- `bindings` (2 replays): the name of a bindings fixture next to the
  replay. The Lua game will read it as `bindings.cfg` from its save
  directory, again through `love.filesystem`.
- `attract_after` (1 replay, `attract.json`): the idle threshold of the
  attract demo. The C replay passes it to the game; the Lua game will get
  it as `--attract-after N` in the `arg` of `love.load`. Today the fake
  always passes an empty table.

The game's own writes (the save after a turn, for instance) need somewhere
to go that is not the real disk: a replay must never change a file of the
repository, and two replays run in parallel must never see each other's
saves.

Nothing yet settles what the fake's files look like to the game: which
names exist, what `read` returns for a missing file, what `getInfo`
returns, whether `love.filesystem` exists in `conf.lua`, whether the game's
own files (`main.lua`, `sprites.png`) are visible, what a bad path does. If
nobody fixes them once, plans 2–3 find them through e2e mismatches.

Finally, the step 3 reader accepts any value for these three members. A
replay with `"attract_after": 0`, an odd-length `initial_save` or
`"bindings": "personal"` would run with the wrong start state. The C
replay refuses all of them.

## Solution

The fake builds a fake disk per run, before `love.load`:

- `save.bin` holds the bytes of `initial_save` when the replay has one;
  otherwise it does not exist.
- `bindings.cfg` holds the bytes of the file named by `bindings`, resolved
  in the replay's directory; otherwise it does not exist.
- Below these in-memory files, the game directory is visible read-only,
  as LÖVE shows the game's source under its save directory.
- `love.filesystem.write` writes into memory only; later reads see it,
  for the rest of the run. Nothing the game does writes to the real disk.

`love.filesystem` holds exactly `read`, `write` and `getInfo`, with LÖVE
11.5's return conventions for the forms the fake supports. Any other form
raises a Lua error naming the function.

`love.load(arg)` receives `{"--attract-after", "N"}` when the replay has
`"attract_after": N`, otherwise `{}`.

The reader validates the three members as strictly as the C replay does,
and refuses `start` (a C-only member the fake does not play). Every refusal
happens before the capture file is created, exits with status 1 and names
the file, the member and the problem.

A test game, `disk_echo`, reads both files back, echoes `arg`, exercises
writes, and returns everything in its `debug_state()`. The CLI test runs it
on a fixture replay with all three members, on a replay with none, and on
two e2e replays, and compares the state file with the exact expected text.
The settled conventions go into `docs/love-port/design.md`.

## User Stories

1. As a plan-2 developer, I want `love.filesystem.read("save.bin")` to return exactly the bytes of the replay's `initial_save`, so that the Lua game resumes the same save the C replay seeds.
2. As a plan-2 developer, I want `save.bin` to not exist when the replay has no `initial_save`, so that the game starts as on a fresh install, as the C replay erases its save.
3. As a plan-2 developer, I want `love.filesystem.read("bindings.cfg")` to return exactly the bytes of the fixture named by `bindings`, so that `pad_swap_aim_menu` and `pad_unbound_enter_aim` play with their bindings.
4. As a plan-2 developer, I want `bindings.cfg` to not exist when the replay has no `bindings`, so that the game falls back to its default bindings, as the C replay does.
5. As a plan-2 developer, I want the `bindings` name resolved in the replay's directory, whatever the current directory, so that `tests/e2e/swap_aim_menu.cfg` is found from any caller.
6. As a plan-2 developer, I want `read(name)` to return the content and its size in bytes, as LÖVE 11.5 does, so that the same code runs in real LÖVE.
7. As a plan-2 developer, I want `read` of a missing file to return `nil` and LÖVE's message (`Could not open file <name>. Does not exist.`), so that my "no save yet" branch behaves as in LÖVE.
8. As a plan-2 developer, I want `read` to return binary content intact (zero bytes, bytes above 127), so that a binary save survives the round trip.
9. As a plan-2 developer, I want `write(name, data)` to return `true` and store `data` in memory, so that my save code works as in LÖVE.
10. As a plan-2 developer, I want a write to replace the whole previous content of that name, so that a shorter save leaves no tail of the older one.
11. As a plan-2 developer, I want `write(name, "")` to create an empty file, so that `getInfo` then reports size 0, as in LÖVE.
12. As a plan-2 developer, I want a file written in one frame readable in every later callback and in `debug_state()`, so that the save outlives the frame it was written in.
13. As a plan-2 developer, I want overwriting `save.bin` or `bindings.cfg` to work like any write, so that the game can update the seeded files.
14. As a plan-2 developer, I want `getInfo(name)` to return `{type = "file", size = N}` for an existing file and `nil` for a missing one, so that I can test for a save before reading it.
15. As a plan-2 developer, I want `getInfo(name, "file")` to filter like LÖVE (nil when the type differs), so that the filtered form works too.
16. As a plan-2 developer, I want the files of the game directory (`main.lua`, `sprites.png`, a sub-directory) visible to `read` and `getInfo`, read-only, as LÖVE mounts the game's source, so that the game can read its own data files.
17. As a plan-2 developer, I want a file written under the name of a game file to shadow it for later reads, as LÖVE's save directory comes first, so that the precedence is LÖVE's.
18. As a plan-2 developer, I want `getInfo` on a game sub-directory to return `{type = "directory"}`, so that directory checks behave as in LÖVE.
19. As a plan-2 developer, I want a write to a name inside a directory (`saves/a.bin`) to return `false` and a message, as LÖVE does without `createDirectory`, so that the game cannot rely on directories the fake does not offer.
20. As a plan-2 developer, I want a malformed path (empty, leading `/`, a `.` or `..` element, an empty element, a backslash, a colon) to raise an error naming the function and the path, so that a path that could escape the disk is reported at once.
21. As a plan-2 developer, I want a wrong argument (missing name, non-string name, non-string data, unknown filter type, extra arguments of an unsupported form) to raise an error naming the function, so that a slip is reported and not answered with nil.
22. As a plan-2 developer, I want `love.filesystem` to hold `read`, `write` and `getInfo` and nothing else, so that the fake never offers a function the design does not list.
23. As a plan-2 developer, I want `love.filesystem` absent while `conf.lua` runs and present from `main.lua` on, so that the fake never accepts a `conf.lua` that reads the save directory before LÖVE has mounted it.
24. As a plan-2 developer, I want `love.load(arg)` to receive `{"--attract-after", "3"}` for `attract.json`, so that the Lua game reads the threshold the way it would from a command line.
25. As a plan-2 developer, I want `love.load(arg)` to receive an empty table when the replay has no `attract_after`, so that the game uses its default threshold.
26. As a plan-2 developer, I want the global `arg` left unset, so that the game reads its arguments only from `love.load`'s parameter, the one place the fake and LÖVE agree on.
27. As a plan-2 developer, I want `N` written in plain decimal, without sign or leading zero, so that a simple `tonumber` reads it.
28. As a plan-1 developer, I want an `initial_save` that is not a string, is empty, has an odd number of digits or a non-hex character refused, as the C replay refuses it, so that a broken save never reaches the game.
29. As a plan-1 developer, I want `initial_save` hex digits accepted in upper and lower case, as the C replay accepts them, so that every replay the C plays is valid for the fake.
30. As a plan-1 developer, I want `"bindings": "personal"` refused with the C replay's reason, so that a recording made with a player's own bindings never runs with others.
31. As a plan-1 developer, I want a `bindings` value that is not a plain `.cfg` file name next to the replay (a `/`, a leading `.`, `bindings.cfg` itself, another extension, over 64 characters, a character outside `A–Z a–z 0–9 _ . -`) refused, as the C replay refuses it, so that a replay cannot read outside its directory.
32. As a plan-1 developer, I want a missing or unreadable bindings fixture refused with its resolved path, so that a typo in a replay is reported, not played with default bindings.
33. As a plan-1 developer, I want a bindings fixture over 65 536 bytes refused, as the C replay refuses it, so that the two readers accept the same files.
34. As a plan-1 developer, I want `attract_after` refused unless it is an integer from 1 to 2 147 483 647, so that the game never gets a threshold the C game would assert on.
35. As a plan-1 developer, I want each of `initial_save`, `bindings`, `attract_after` given twice refused, so that the replay has one meaning.
36. As a plan-1 developer, I want a replay with `start` refused, naming the member, so that a C-only resume recording is not played from the title screen in silence.
37. As a plan-1 developer, I want every refusal to name the file, the member and the problem, exit with status 1 before `love.load`, and leave no capture or state file, so that a broken replay never runs half a game.
38. As a plan-1 developer, I want the reader to keep accepting all 20 replays of `tests/e2e/`, and to read back `attract.json`'s threshold 3, its 3 921 save bytes and the bindings names of the two bindings replays, so that the new checks are proven on the real files.
39. As a plan-1 developer, I want the fake disk to be a module of its own, pure C with no Lua, so that its layering, replacement and path rules are tested without a game.
40. As a plan-1 developer, I want a test game that reads both files back and echoes `arg`, run through the real binary, so that the whole path from replay to Lua is checked.
41. As the repository owner, I want no run of the fake to create, change or delete a file in the game directory or the current directory, so that replays are side-effect free and can run in parallel.
42. As a plans-2–3 reader, I want the fake disk and argument conventions written in `docs/love-port/design.md`, so that I do not need plan 1, which will be deleted.
43. As the repository owner, I want `make test` measured before and after this step with the design's protocol and kept within its gate, so that the fast loop stays fast.
44. As the repository owner, I want `make e2e`, the C game and every `*.sha256` untouched, so that the reference is unaffected.

## Implementation Decisions

### Modules

- **Fake disk** (new deep module, pure C, no Lua, no include from `src/`,
  in the fake's sources). It owns two layers: in-memory files (name →
  bytes) on top, and an optional game directory below, read-only. Its
  interface:
  - create, with the game directory path or none; free;
  - check a name: returns no reason when the name is valid, else a static
    string naming the problem. Valid: non-empty, `/`-separated elements,
    no leading `/`, no empty, `.` or `..` element, no `\` and no `:`;
  - put (name, bytes, length): copies the bytes, replacing any in-memory
    file of that name. It refuses a name holding `/` (the fake disk has no
    directories) with a reason the binding turns into `write`'s message;
  - read (name): looks in memory, then in the game directory; returns a
    newly allocated buffer and its length (the caller frees it), or a
    "missing" result. A game-directory directory is "missing" for read;
  - info (name): missing, file with its size, or directory (game
    directory only).

  Names are byte-compared, case-sensitive. The in-memory set is a small
  array searched linearly: replays hold a handful of files. Reading a game
  file reads it from the real disk each time; nothing is cached, nothing
  is ever written there.
- **Replay reader** (step 3's module, extended by step 4, H-4-1). At read
  time it checks the three members and keeps them; it adds three
  accessors:
  - the decoded `initial_save` bytes and their count, owned by the replay
    (valid until it is freed); none when absent;
  - the `bindings` file name, owned by the replay; none when absent;
  - `attract_after`, 0 when absent.

  Checks, mirroring the C replay's header parser:
  - `initial_save`: a string of hex digits (either case), non-empty, even
    length. No upper bound beyond memory: the C cap (8 192 bytes) is the
    C game's save format, which the fake does not know;
  - `bindings`: a string; `"personal"` is refused with its own reason;
    otherwise it must pass the C replay's fixture-name rule (5–64
    characters, ends in `.cfg`, no leading `.`, not `bindings.cfg`, only
    `A–Z a–z 0–9 _ . -`);
  - `attract_after`: an integer, 1 ≤ N ≤ 2 147 483 647;
  - each of the three appears at most once (the reader's tree keeps
    duplicate keys; the check walks the root's members);
  - `start` present → refused: the fake plays from the title screen only.

  Messages name the file, the member and the problem
  (`"attract_after" is not 1-2147483647`, `"initial_save" has an odd
  number of digits`, `"bindings": "personal" ...`, `"start" is not
  supported`). Exact wording is the implementer's; tests match the member
  name and the quoted bad value only. The reader does not open the
  bindings file: it only knows the replay's name, not where `replay_parse`
  texts live.
- **Filesystem binding** (new module, the only one that knows both Lua and
  the disk, laid out like step 4's joystick binding, H-4-4). It installs
  `love.filesystem` holding only `read`, `write` and `getInfo`, each a C
  closure over the disk (a light userdata upvalue). Supported forms and
  answers:
  - `read(name)` → `content, size`; missing → `nil, "Could not open file
    <name>. Does not exist."`. Any second argument raises an error naming
    `love.filesystem.read` (the container and size forms are not
    supported);
  - `write(name, data)` with `data` a string → `true`; a name with `/` →
    `false, "Could not open file <name>. Does not exist."` (no directory
    exists). A third argument, or non-string data, raises an error naming
    `love.filesystem.write`;
  - `getInfo(name)` or `getInfo(name, filtertype)` → a new table
    `{type = "file", size = N}` or `{type = "directory"}`, or `nil`.
    `filtertype` must be one of LÖVE's `"file"`, `"directory"`,
    `"symlink"`, `"other"`; a mismatch returns `nil`. The info-table forms
    raise an error naming `love.filesystem.getInfo`. No `modtime`: the
    fake has no clock, and LÖVE documents the field as possibly absent;
  - an invalid name (fake disk's check) raises an error naming the
    function, the name and the reason.
- **LÖVE host** (extended):
  - opening takes the disk as well as the game directory (H-4-7). The host
    does not own the disk; the disk must outlive the host. After
    `love.conf(t)` and before `main.lua`, next to step 4's joystick
    install, it installs the filesystem binding. `love.filesystem` is
    therefore nil in `conf.lua`;
  - `love.load` gets a list of argument strings from the caller, pushed as
    a new sequence table. The global `arg` stays nil (the embedded LuaJIT
    never sets it, and the host does not).
- **Command**: after reading the replay and before creating the capture
  file, it builds the disk:
  - creates it over the game directory;
  - puts `save.bin` from the reader's decoded bytes;
  - resolves `bindings` against the replay path's directory (the part up
    to the last `/`, or the current directory), reads it whole, refuses it
    when it cannot be read or holds more than 65 536 bytes, and puts it as
    `bindings.cfg`;
  - builds the argument list: `--attract-after` and the decimal `N` when
    `attract_after` is set, else nothing.

  A refusal prints `fake_love: <message>`, exits 1 and creates no output
  (the capture is created only after the disk is built). The disk is freed
  after the host is closed.

### Contracts settled here

- The fake disk at `love.load`: `save.bin` iff `initial_save`,
  `bindings.cfg` iff `bindings`, over the read-only game directory. No
  other file exists in memory.
- In-memory files shadow game files of the same name; writes never reach
  the real disk; the disk lives for one run.
- `love.filesystem` exists from `main.lua` on, holds three functions, and
  answers with LÖVE 11.5's conventions for the forms above. Unsupported
  forms are errors, not nil.
- `love.load` receives `{}` or `{"--attract-after", "N"}`; global `arg`
  is nil.
- The replay members the fake reads: `input`, `frames`, `initial_save`,
  `bindings`, `attract_after`. `start` is refused. Other unknown members
  stay accepted and ignored (step 4's rule).
- These go into `docs/love-port/design.md`: item 2 and 3 of the loop gain
  the details, and a short "Fake disk and arguments" paragraph follows the
  pad paragraph (H-4-8).

### Decisions taken without the plan

- **Game directory visible read-only.** The plan names only the two seeded
  files. LÖVE's `read` sees the game's source too, and plans 2–3 read game
  files (step 12 reads `sprites.png` through the same names). Showing it
  now fixes the precedence once; the disk module keeps it to one lookup.
- **`love.filesystem` installed after `love.conf`.** Real LÖVE exposes it
  in `conf.lua`, but the save directory is only mounted once the identity
  is known. Hiding it keeps the fake stricter than LÖVE: what runs in the
  fake runs in LÖVE.
- **`start` refused.** No e2e replay holds it, and the C replay's resume
  start has no LÖVE equivalent in the design.
- **No `modtime`, no info-table form, no size/container forms.** None is
  needed by the port, each would widen the contract.
- **Strict paths raise.** LÖVE strips a leading `/`; the fake refuses it.
  The game has no reason to write one.

## Implementation Chunks

1. **Replay members.** The reader's three checks, `start`'s refusal, the
   duplicate checks, the three accessors, and the reader tests (e2e
   replays, in-memory refusals). Stories 28–31, 34–36, 38.
2. **Fake disk.** The pure C module and its unit test, with a small
   game-directory fixture. Stories 10, 11, 16–20 (disk side), 39, 41.
3. **`love.filesystem` and the disk in the run.** The binding, the host
   install after `love.conf`, the command building the disk (bindings
   resolution, size cap, refusals), the `disk_echo` game and its fixture
   replays, the CLI cases on files. Stories 1–23, 32, 33, 37, 40, 41.
4. **Arguments, e2e cases, docs and timing.** `love.load(arg)`, the
   CLI cases on `arg` and on the two e2e replays, the design.md paragraph,
   the "after" timing and the step note. Stories 24–27, 42–44.

## Testing Decisions

- A good test drives a public interface and checks external behaviour:
  the disk's answers, the reader's accessors and messages, the binary's
  exit status, stderr and state file, and the files on the real disk
  before and after. No test reaches into the Lua state or the reader's
  tree.
- **Fake disk, unit test** (new `test_fake_love_disk`, pure C, linked
  with the disk module only; it joins the unit list as
  `test_fake_love_replay` did). Cases:
  - empty disk: read and info of `save.bin` → missing;
  - put then read gives the same bytes, including `\0` and `\xff`; put
    again shorter replaces the whole; put empty gives size 0;
  - put of `a/b` refused with a reason;
  - name check: valid `save.bin`, `dir/x.png`; invalid `""`, `/x`, `./x`,
    `a/../b`, `a//b`, `a\b`, `c:x`, `..`;
  - game-directory layer over a fixture directory (a file, a
    sub-directory with a file): file read and size, sub-directory info is
    directory and read is missing, in-memory put over a game file name
    shadows it, and the fixture file's bytes are unchanged afterwards.
- **Reader, unit test** (`test_fake_love_replay`, extended):
  - every `tests/e2e/*.json` is still accepted;
  - `attract.json`: `attract_after` 3, 3 921 save bytes starting
    `53 4c 57 59` (`SLWY`), no bindings; `pad_swap_aim_menu.json`:
    bindings `swap_aim_menu.cfg`, no save, `attract_after` 0;
    `pad_unbound_enter_aim.json`: bindings `unbound_enter_aim.cfg` and
    4 186 save bytes; `pad_title.json`: none of the three;
  - in-memory texts: `"initial_save": "aBcD"` gives `ab cd`; refusals,
    each matched on the member and the quoted value: `initial_save` as a
    number, `""`, `"abc"`, `"zz"`, twice; `bindings` `"personal"`,
    `"../x.cfg"`, `"bindings.cfg"`, `".x.cfg"`, `"x.txt"`, a 65-character
    name, a number, twice; `attract_after` `0`, `-1`, `2147483648`,
    `"3"`, twice; `"start": "resume"`;
  - texts in memory, no reuse of the C game's `tests/fixtures/*.json`.
- **The binary, CLI test** (new `test_fake_love_files`, its own target in
  `FAKE_LOVE_CHECKS`, H-4-6; not added to `test_fake_love` so each check
  stays under 100 ms).
  - **The `disk_echo` game** (a directory under the fake's fixtures). Its
    `conf.lua` logs whether `love.filesystem` is nil. `love.load(arg)` logs
    `#arg` and each element quoted; whether the global `arg` is nil; the
    names in `love.filesystem` sorted; for `save.bin` and `bindings.cfg`,
    `getInfo` (type and size, or nil) and the `read` result as size plus
    the bytes in hex; `read("missing.bin")` as `nil` and its message;
    `getInfo("main.lua").type`, `getInfo("data", "file")` (nil) and
    `getInfo("data").type` for a `data/` sub-directory; `write("note.txt",
    "hello")` and a re-read; `write("saves/a.bin", "x")` as `false` and
    its message. Frame 0's `update` overwrites `save.bin` with `"\0\255"`;
    frame 1's `draw` reads it back; `debug_state()` adds the final
    `save.bin` and `note.txt` and returns the log.
  - **Fixture replays**: `disk_full.json` (`input` `pad`, two empty-pad
    frames, `initial_save` `"00ff10A5"`, `bindings` `"disk_bindings.cfg"`,
    a 3-line fixture next to it, `attract_after` 7) and `disk_bare.json`
    (two frames, none of the three). The expected logs are written out in
    full in the test and the state file must equal them byte for byte.
  - **e2e replays**: `pad_unbound_enter_aim.json` and `attract.json` with a
    variant of the game that logs sizes only (no hex dump); the expected
    sizes are the fixture file's size on disk and the reader's save count,
    and `attract.json` gives `--attract-after` and `3`.
  - **Side effects**: after each run, `note.txt` exists neither in the game
    directory nor in the current directory, and the fixture `.cfg` and the
    game files are unchanged.
  - **Errors**: `read("../x")`, `read()`, `write("a", 1)`,
    `getInfo("a", "bogus")`, `read("a", 10)` each fatal, naming the
    function (and the path for the first); a replay whose bindings fixture
    is missing exits 1, names the resolved path and leaves no capture or
    state file; a fixture over 65 536 bytes (generated by the test into
    `tmp/`, with its replay) is refused the same way.
  - If this test goes over the 100 ms budget, the error cases move to a
    `test_fake_love_files_failures` target, as step 3 split its failures.
- **Prior art:** step 3's `test_fake_love`/`test_fake_love_failures` and
  their shared CLI helper for running the binary; `test_fake_love_replay`
  for in-memory reader texts; `tests/test_replay_source.c` for the refusal list this step mirrors.
- **Timing:** `make test` measured before and after with the design's
  protocol (11 rotated paired rounds against the step's "before" and
  against `2e7215c`, both ≤ 3 %). The step note records the plain and
  paired medians, the spreads, the rounds, the core count and the slowest
  new check's own time.
- The C game's unit tests, `make e2e` and every `*.sha256` stay
  unchanged.

## Out of Scope

- Any other `love.filesystem` function (`createDirectory`, `remove`,
  `getDirectoryItems`, `lines`, `newFile`, `setIdentity`, …): absent, the
  plain Lua error until step 17's named fatal error.
- `love.filesystem.read`'s container and size forms, `write`'s size form,
  `Data` objects, `getInfo`'s info-table forms, `modtime`.
- Writing the final fake disk out anywhere, or comparing it with the C
  game's save: no replay output covers it.
- The C replay's `start: "resume"` (refused here).
- Parsing or validating the bindings file's content: the Lua game's job
  (plans 2–3); the fake passes bytes.
- Image loading from the disk (`newImageData(file)`, step 12), which will
  reuse the disk's read.
- Every drawing function and any capture byte (phases 3–4).
- Audio, always.

## Hypotheses

Plan step 4 is being implemented while this PRD is written. These
assumptions come from its PRD (`phase2-step4-fake-love-pad`), not from its
code. Where one turns out false, the implementer adapts the named decision
and keeps the behaviour this PRD settles.

- **H-4-1**: step 4 left the replay reader with the same read/parse/free
  interface plus one per-frame accessor (the held 12-bit mask of frame
  `i`), and stricter frame validation: `"input": "pad"` required, every
  frame exactly `{"pad": [...]}` with distinct integers 1–12. Other
  top-level members (`initial_save`, `bindings`, `attract_after`, unknown
  ones) are still accepted and ignored. This step's checks go after step
  4's, in the same read pass. If step 4 already validates any of the three
  members, this step keeps that check and adds the rest.
- **H-4-2**: step 4's refusal messages name the file and the frame as
  `f<N>`; header-member refusals here name the member instead of a frame,
  in the same `<file>: <problem>` shape.
- **H-4-3**: step 4 changed the host's per-frame call to take the frame
  index and its held mask, and calls the pad callbacks before `update`.
  This step does not touch the per-frame call.
- **H-4-4**: step 4 put the joystick binding in its own module that
  installs into the host's Lua state, called by the host between
  `love.conf` and `main.lua`. The filesystem binding follows the same
  layout and is installed at the same point. If step 4 kept the joystick
  code inside the host, the filesystem binding is still its own module.
- **H-4-5**: step 4 updated step 3's fixture replays to `"input": "pad"`
  and `{"pad": []}` frames. This step's fixture replays use that format.
- **H-4-6**: step 4 added a pure-C pad unit test to the unit list and its
  CLI cases either to `test_fake_love` or to a target of their own in
  `FAKE_LOVE_CHECKS`. The new `test_fake_love_disk` and
  `test_fake_love_files` join the same way; the Makefile's source lists
  for the fake gain the disk (plain C) and the filesystem binding (with
  LuaJIT flags) as step 4 added its modules.
- **H-4-7**: step 4 left the host's open operation taking only the game
  directory and its load operation taking nothing. This step adds the disk
  to open and the argument list to load. If step 4 already changed either
  signature, this step extends it rather than replacing it.
- **H-4-8**: step 4 wrote a "Pad semantics" paragraph under "Pad buttons"
  in `docs/love-port/design.md`; this step's paragraph follows it.
- **H-4-9**: step 4 kept step 3's failure path: an invalid replay is
  refused before the capture file is created, with status 1, a
  `fake_love: ` prefix and no output left behind. This step's refusals,
  including the bindings fixture read, use that path.
- **H-4-10**: step 4's note in the plan records the "before" commit and
  timings; this step's "before" is the last commit of step 4.

## Further Notes

- The step 5 note in the plan must hold the before and after `make test`
  measurements (both paired medians, the plain medians, spreads, rounds,
  core count) and the slowest new check's time.
- The C replay seeds the save into its storage before the title screen
  and erases it when there is no `initial_save`; the fake's "file absent"
  is the same start state for a game that reads `save.bin` at load.
- `bindings.cfg` is a name of the Lua game's save directory: a player's
  own file in real LÖVE, a replay fixture in the fake. The game's default
  bindings, when the file is absent, are the game's concern (plans 2–3).
- No capture program covers the disk: the plan's capture rule is for
  drawing functions, and the file conventions above come from LÖVE 11.5's
  documentation and PhysFS's path rules, made stricter where they differ.
