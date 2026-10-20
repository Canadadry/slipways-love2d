---
title: "The fake LÖVE writes the C replay's capture: canvas 0 as packed palette numbers after every frame"
description: "The fake LÖVE runs a game on a replay but its capture file stays empty, so no sha256 of the Lua game can ever be compared with the C game's .json.sha256."
status: needs-triage
---

## Problem Statement

Since step 3 (2.3), `tmp/fake_love <game dir> <events.json> <capture.bin>
[<state.txt>]` runs a game frame by frame and writes `debug_state()` after
the last frame, but its capture file is created empty and never written.
Every drawing function now lands its pixels on the fake's canvases (steps 6
to 15), yet nothing turns them into the bytes the e2e check hashes. Without
the capture, step 18 (`make love-e2e`) has nothing to compare with the 20
`.json.sha256` files, and plans 2–3 cannot check the Lua game at all.

design.md also describes the capture wrongly: its loop item 5 says the fake
"appends the 19 200 bytes" per frame and that this is "the C replay's"
format. The C replay's capture (the `screencast` writer of the devtools)
packs **two pixels per byte**: 9 600 bytes per frame (measured:
`tmp/frames/tests/e2e/pad_title.json.bin` is 307 200 bytes for 32 frames).
A fake that followed design.md word for word would never match one
`.sha256`. design.md does not say which canvas is read when the game has
several, nor what a pixel outside the 16 colours becomes before step 17
makes it fatal.

## Solution

After each frame's draw step (and after the existing "canvas still
selected" check), the fake reads **canvas 0**, the first canvas the game
created, turns each opaque pixel whose RGB is one of the 16 palette colours
into its number 0–15, packs the 19 200 numbers into 9 600 bytes exactly as
the C replay does, and appends them to the capture. After the last frame it
closes the capture, then calls `debug_state()` and writes the state file,
as today. A failed run still removes both files.

Until step 17 makes it fatal, a pixel that is not an opaque palette colour
(another RGB, alpha `00`, or every pixel of a frame drawn while no canvas
exists) is written as colour 0, and the run, which still succeeds, ends
with one warning line on stderr naming how many such pixels it wrote and
the first one (frame, pixel, RGBA, or "no canvas").

design.md is corrected (9 600 packed bytes, canvas 0, the palette's 16 RGB
triples written out, since `src/p8/palette.c` goes with plan 4), and a test
game drawing known colours proves the capture's bytes.

## User Stories

1. As a plan-2 developer, I want the fake to write one capture frame after every frame's draw step, so that the capture of a replay holds exactly its frame count of frames.
2. As a plan-2 developer, I want each frame to be 9 600 bytes in the C replay's packing, so that the sha256 of my game's capture can equal the `.json.sha256` the C game wrote.
3. As a plan-2 developer, I want the even pixel of each pair in the low nibble and the odd one in the high nibble, rows top to bottom, columns left to right, so that the byte order is the C replay's to the bit.
4. As a plan-2 developer, I want the capture to read canvas 0, the first canvas my game creates, so that scratch canvases (recoloured sprite builds, step 15's second canvas) never reach the capture.
5. As a plan-2 developer, I want the capture to read canvas 0 whatever canvas was selected last, so that switching to the screen for the window upscale, as LÖVE requires before present, changes nothing.
6. As a plan-2 developer, I want design.md to tell me to create the game canvas first, before any other canvas, so that my game's canvas is the one captured.
7. As a plan-2 developer, I want a frame captured even when the game has no `love.draw`, so that a frame always yields a frame, as the C replay does.
8. As a plan-2 developer, I want canvas 0's pixels captured as they stand after the draw step, including pixels kept from earlier frames, so that a game that draws only what changed is captured as the player sees it.
9. As a plan-2 developer, I want each of the 16 palette colours, drawn opaque with `setColor(byte / 255)`, `clear(byte / 255)` or an image texel, to become its number 0–15, so that every drawing path I use converts.
10. As a plan-2 developer, I want the palette's 16 RGB triples written in design.md, so that I build my colour table from the same bytes the fake converts with, after `src/` is deleted.
11. As a plan-2 developer, I want a pixel outside the palette written as colour 0 with a warning naming it, until step 17, so that a mistake is visible on stderr without the conversion being undefined.
12. As a plan-2 developer, I want a pixel with alpha `00` treated as outside the palette, even when its RGB is a palette colour, so that a canvas I forgot to clear opaque is reported, not silently read as black.
13. As a plan-2 developer, I want a frame drawn while no canvas exists treated as 19 200 pixels outside the palette, so that a game that forgot `newCanvas` gets a full-size frame and a warning, not a short capture.
14. As a plan-2 developer, I want the warning to name the first offending frame, pixel and RGBA bytes, and the total count, so that I find the draw call without a debugger.
15. As a plan-2 developer, I want `debug_state()` called after the capture's last frame is written and closed, so that a `debug_state` that draws cannot change the capture.
16. As a plan-2 developer, I want `debug_state()` and the state file to behave as since step 2.3, so that my `.end-state` comparison keeps working.
17. As a plan-2 developer, I want a failed run (Lua error, fatal misuse, canvas left selected) to leave neither capture nor state file, so that a partial capture can never be hashed as a regression.
18. As a plan-2 developer, I want a capture of zero frames to be an empty file, so that a replay without frames still produces its output.
19. As a plan-1 developer, I want the palette conversion and packing in their own module with a small interface, so that step 17 turns "outside the palette" into a fatal error in one place.
20. As a plan-1 developer, I want the fake's palette table to be its own, never included from `src/`, so that `fake-love-guard` stays green and plan 4 can delete `src/`.
21. As a plan-1 developer, I want a unit test over the conversion of all 16 colours, near misses and alpha `00`, so that a typo in the table fails a fast test before any e2e.
22. As a plan-1 developer, I want a test game drawing known colours on known pixels over several frames, and a test comparing the capture with bytes built in C, so that the step's verification is the plan's: "a test game drawing known colours and the capture's bytes".
23. As a plan-1 developer, I want that test game to draw on a second canvas and on the screen too, so that the choice of canvas 0 is proven by the bytes.
24. As a plan-1 developer, I want the existing tests that asserted an empty capture updated to the new truth, so that they keep checking something real.
25. As a plan-1 developer, I want an unwritable capture path still refused before `love.load`, naming the path, so that the existing failure test keeps holding.
26. As a plan-1 developer, I want a write or close error on the capture to fail the run naming the path, so that a full disk never yields a truncated capture with exit 0.
27. As a plan-1 developer, I want design.md's loop item 5 to state 9 600 packed bytes and canvas 0, so that step 18 and plans 2–3 read a true contract.
28. As a step-17 implementer, I want the warning path documented as temporary in the step note, so that I know which behaviour I replace and which tests change with it.
29. As a step-18 implementer, I want the capture's size always to be frames × 9 600, so that "each capture has the replay's frame count of frames" holds by construction.
30. As the repository owner, I want the new checks to cost a few milliseconds each, run in parallel inside `make test`, and `make test` measured with design.md's protocol, so that the fast loop stays fast.
31. As the repository owner, I want `make e2e`, the C game and every `*.sha256` untouched, so that the reference is unaffected.
32. As the repository owner, I want `love_capture_check` and every capture program unchanged, so that the real-LÖVE comparisons keep reading all four channels of canvas 0 as before.

## Implementation Decisions

### What the C replay writes (read-only reference, explored for this PRD)

- The C replay writes one frame after each `cart_draw()`, `frame_count`
  frames per replay, no header, no index, no count; the file is opened
  `"wb"` and closed before the text dump.
- A frame is 9 600 bytes: for each row y from 0 to 119, for each even x
  from 0 to 158, one byte `screen[y][x] | screen[y][x + 1] << 4`. Each
  screen value is a palette number 0–15 (asserted); the C game has no
  display palette.
- The 16 RGB triples (C palette, copied into the fake by value):

  | n | RGB | n | RGB | n | RGB | n | RGB |
  |---|---|---|---|---|---|---|---|
  | 0 | `000000` | 4 | `AB5236` | 8 | `FF004D` | 12 | `29ADFF` |
  | 1 | `1D2B53` | 5 | `5F574F` | 9 | `FFA300` | 13 | `83769C` |
  | 2 | `7E2553` | 6 | `C2C3C7` | 10 | `FFEC27` | 14 | `FF77A8` |
  | 3 | `008751` | 7 | `FFF1E8` | 11 | `00E436` | 15 | `FFCCAA` |

  They equal the `PALETTE` of step 15's draft `game_frame.lua` (H-15-5).

### Decisions

- **Format: the C replay's, not design.md's "19 200 bytes".** 9 600 bytes
  per frame, packed as above. design.md's item 5 is a mistake (the number
  of pixels written as the number of bytes); the `.sha256` files are the
  contract and were computed over the packed form. design.md is corrected
  in this step.
- **Canvas 0 is captured.** The first canvas the game creates, the same
  one the capture harnesses call "the fake's canvas 0". Rejected: the
  canvas selected last (after the draw step the screen must be selected,
  so "last" would mean "last before the screen", which a scratch-canvas
  build in the middle of a frame makes fragile); the screen (the fake does
  not model it). design.md gains the rule for the game: create the game
  canvas first, in `love.load`, before any other canvas.
- **When.** At the end of each frame's draw step, after the "present
  cannot be called while a Canvas is active" check, with or without
  `love.draw`. The capture reads the canvas through the host's existing
  read-back of a canvas by creation index; `host_frame` itself is
  unchanged (H-15-1, H-15-6).
- **Conversion, defined now.** A pixel converts to n when its alpha is
  `FF` and its RGB equals triple n byte for byte. Anything else is
  *foreign*: alpha `00` (also with a palette RGB, and a fresh canvas's
  `00000000`), any RGB outside the table. Alpha other than `00`/`FF`
  cannot reach a canvas (setColor and newImage refuse it), and if it
  ever did it is foreign too. A frame while no canvas exists is 19 200
  foreign pixels.
- **A foreign pixel, until step 17: written as 0, counted, warned.** The
  run continues and exits 0; at the end of a successful run, if any were
  written, stderr gets one line, then the state file is written as usual:
  `fake_love: warning: <count> pixels outside the palette written as
  colour 0, first f<N> pixel (<x>, <y>) <RRGGBBAA>` or `…, first f<N> no
  canvas`. Why not fatal now: step 17 owns that error and its message,
  and many existing fixture games run frames without a canvas or draw
  non-palette bytes (`graphics` clears to `FF004C`); making it fatal here
  would rewrite those fixtures twice. Why not silent: a wrong colour
  would then only show as a sha256 mismatch in step 18. design.md keeps
  its target sentence ("a pixel of another colour on the canvas" is
  fatal), which step 17 makes true; the temporary warning is recorded in
  the step note only.
- **Module `frame_capture`** (new, in the fake's directory; not
  `capture`, as `test_fake_love_capture` already names the
  `love_capture_check` test). Interface, deep and pure:
  - the frame size constant (9 600) and the palette table (16 RGB
    triples, the fake's own);
  - `frame_capture_palette_number(rgba)` → 0–15, or a "foreign" value;
  - `frame_capture_pack(pixels or NULL, out[9600], foreign_record)` →
    the count of foreign pixels in the frame, filling the record of the
    first one (pixel and RGBA; NULL pixels: "no canvas") when the record
    is still empty.
  - The writer (open, append a frame, close) stays in the command: it
    replaces today's `create_empty_file`, opens the capture before the
    game is opened (an unwritable path still fails before `love.load`,
    naming it), appends after each `host_frame`, and is closed before
    `debug_state()`; any write or close error fails the run naming the
    path, and the existing cleanup removes both files.
  - Speed: a linear search of 16 with a cache of the last converted RGBA
    (neighbouring pixels repeat); no other optimisation. Only step 18's
    600-frame replays care, outside `make test`.
- **Nothing from `src/`.** The table is written in the fake's module;
  `fake-love-guard` covers the new files by directory, unchanged.
- **design.md edits.**
  - Loop item 5: "reads canvas 0 (the first created), turns each opaque
    pixel's RGB into its palette number 0–15, packs two pixels per byte
    (even x in the low nibble), row by row, and appends the 9 600 bytes";
    item 6 unchanged in substance, plus "after the capture is closed".
  - A new paragraph **"Capture"** after "Pad semantics"/"Fake disk and
    arguments" and before "Canvas and clear" (or, if step 15 reordered
    the section, beside the loop description, H-15-2): the packing, the
    16-triple table above, canvas 0, a frame with no `love.draw`, alpha
    `00` and other RGB not palette colours. **The rule for the game:
    create the game canvas first; end every frame with every pixel of it
    opaque and one of the 16 triples, drawn as `byte / 255`.**
  - "How the Lua game draws → Colours" points to this table instead of
    `src/p8/palette.c`.
- **No change** to `love_capture_check`, the harnesses, any capture
  program or PNG, the CI jobs, `make e2e`, the C game.

## Implementation Chunks

1. **`frame_capture` module and its unit test.**
   - Palette table, `frame_capture_palette_number`, `frame_capture_pack`,
     first-foreign record.
   - Unit test `test_fake_love_frame_capture` (one `FAKE_LOVE_CHECKS`
     target): each of the 16 triples → its number; each with one byte off
     by one (`FF004C`, `000001`, …) → foreign; alpha `00` with a palette
     RGB → foreign; a packed frame of known pixels: byte 0 holds pixel 0
     low and pixel 1 high, byte 79 ends row 0, byte 80 starts row 1, byte
     9 599 holds (158, 119) low and (159, 119) high; NULL pixels → 19 200
     foreign and 9 600 zero bytes; the first-foreign record keeps the
     first, not the last.
   - Stories 2, 3, 9, 10 (table), 12, 19–21.
2. **The command writes the capture; the test game; the docs.**
   - `fake_love.c`: writer opened in place of the empty file, a frame
     after each `host_frame` from canvas 0, closed before `debug_state`,
     the warning line, errors naming the path.
   - Fixture game `capture_colours` (with a replay of 4 frames, no pad):
     `love.load` creates canvas 0 then a second canvas; frame 0 clears
     canvas 0 opaque to colour 0, then fills 16 bands of 10 columns, colour
     c in columns 10c … 10c + 9, with `rectangle` and `byte / 255`; frame
     1 clears to colour 0 and lights (0, 0) colour 1, (1, 0) colour 2 with
     `points`, fills (159, 119) colour 15 with a 1×1 rectangle, draws a
     2×1 image of colours 9 and 10 (`setPixel`, `newImage`, `draw` under
     white) at (2, 1); frame 2 draws nothing on canvas 0 but clears the
     second canvas to colour 8 and draws on the screen, and has no
     drawing on canvas 0 (frame 2 must equal frame 1); frame 3 lights one
     pixel with a non-palette colour (`setColor(0.3, 0, 0)`, i.e. `4C0000`)
     at (0, 0), over frame 1's colour 1. `debug_state` returns the frames drawn.
   - CLI test `test_fake_love_frames` (one `FAKE_LOVE_CHECKS` target,
     under 100 ms): the capture is exactly 4 × 9 600 bytes and equals a
     buffer built in C from the description above (bands frame: each row
     is five bytes of `0x00`, five of `0x11`, …, five of `0xFF`; frame 1
     byte 0 is `0x21`, byte 81 (row 1, x 2 and 3) is `0xA9`, byte 9 599
     is `0xF0`; frame 3 is frame 1 with byte 0 `0x20`); exit 0, the state file written; stderr holds the warning with
     `1 pixels`, `f3`, `(0, 0)` and `4C0000FF`; a run of the `counting`
     game (no canvas) gives 3 × 9 600 zero bytes and the warning with
     `57600 pixels` and `f0 no canvas`; `zero_frames.json` gives an empty
     capture and no warning.
   - Update `test_fake_love`: its "capture stays empty" assertions become
     "frames × 9 600 bytes" (counting: 3 frames of zeros; zero frames:
     empty).
   - design.md as above.
   - Stories 1, 4–8, 11, 13–18, 22–27, 29.
3. **Timing and the step note.**
   - `make test` before and after with design.md's protocol against the
     step's "before" (step 15's last commit, H-15-3) and `2e7215c`;
     record medians, paired medians, spreads, rounds, cores and each new
     check's own time.
   - Step note "Step 4.16" in plan 1: format correction, canvas 0, the
     temporary warning step 17 replaces, the tests that will change with
     it.
   - Stories 28, 30–32.

## Testing Decisions

- A good test drives a public interface and checks what comes out: the
  module's return values and bytes, the binary's exit status, capture
  bytes, state file and stderr. No test reads the fake's Lua state or
  internal structures.
- **`frame_capture` gets a unit test** (pure C, no process, ~1 ms): the
  table, the conversion, the packing order, the foreign record.
- **The command gets a CLI test** over the `capture_colours` fixture and
  the existing `counting` game, comparing every byte of the capture with
  a buffer the test builds in C; this is the plan's verification. Its
  expected bytes are written from the description of the drawing, not
  from a run of the fake.
- **Prior art**: `test_fake_love` (counting game, state text, capture
  file), `test_fake_love_failures` (failed runs leave no file, unwritable
  capture), `test_fake_love_joystick` and `test_fake_love_files`
  (fixture game plus replay plus exact output), `tests/fake_love_cli.h`
  helpers; for the packing, the C side's own screencast writer (read
  only, never linked).
- **Budget**: each new check is its own `FAKE_LOVE_CHECKS` target, one
  process, under 100 ms (expected: a few ms each; the CLI test runs the
  fake twice or three times with ≤ 4 frames). `test_fake_love` grows by
  writing 3 frames of 9 600 bytes, negligible.
- **Not tested here**: the fatal form of a foreign pixel (step 17), a
  capture's sha256 against an e2e replay (step 18), write errors on a
  full disk (no cheap way to provoke; the code path shares the existing
  cleanup).
- The C game's unit tests, `make e2e` and every `*.sha256` stay
  unchanged.

## Out of Scope

- Fatal errors for a foreign pixel, a non-palette `setColor`, an unlisted
  function: step 17. This step's warning is the stand-in it replaces.
- `make love-e2e`, sha256 checks against `tests/e2e/`: step 18.
- Any change to drawing rules, the canvas store, `love_capture_check`, the
  capture programs or their PNGs.
- The window upscale and the screen's contents: not modelled.
- A display palette (PICO-8 `pal(…, 1)`): the C game has none.
- Speed work on the conversion beyond the last-pixel cache.
- The Lua game itself: plans 2–3. Audio, always.

## Hypotheses

Plan step 15 (the combined capture `game_frame`) is being implemented
while this PRD is written. Its PRD was read; none of its work was. Before
starting, the implementer checks each item against the committed step 15.
When one differs, adapt names, placement and dependent lines, not the
decisions.

- **H-15-1**: step 15 changes no C code of the fake (its chunk 3, a fake
  fix, was not needed), or, if it was, it changed a drawing rule only:
  `host_frame`, `host_read_canvas` and canvas creation order are as
  described here.
- **H-15-2**: step 15 edits design.md only in "How the Lua game draws"
  ("In combination", the "Text" bullet) and "Real-LÖVE captures"; loop
  items 5 and 6 still say "19 200 bytes" and "the 160×120 canvas", and
  the section order of "The fake LÖVE" is unchanged, so the new "Capture"
  paragraph's placement above holds.
- **H-15-3**: step 15's last commit is this step's "before" timing
  baseline, and its step note (Step 3.15) records its timings.
- **H-15-4**: step 15 adds one capture check
  (`love-capture-check-game_frame`) through `LOVE_CAPTURE_CHECKS`, and no
  other change to `FAKE_LOVE_CHECKS` or to the `test` prerequisites'
  order.
- **H-15-5**: the committed `game_frame.lua` holds the same 16 RGB
  triples as the table above. If it differs, `src/p8/palette.c` (read
  only, at this step's commit) decides, and the difference is reported in
  the step note.
- **H-15-6**: step 15 keeps the end-of-frame order: `love.update`,
  transform reset, `love.draw`, "canvas still selected" check; the
  capture goes right after that check.
- **H-15-7**: step 15 adds no fixture game under
  `tests/fixtures/fake_love/` named `capture_colours`, and no test named
  `test_fake_love_frames` or `test_fake_love_frame_capture`.

## Further Notes

- design.md's "Anything outside the contract … is a fatal error that
  names … the frame and pixel … a pixel of another colour on the canvas"
  stays: it is the target step 17 reaches. For one step the code writes
  such a pixel as 0 with a warning; the step note says so.
- Alpha `00` counts as foreign although real LÖVE would show it over the
  window's black clear as black (colour 0): the game is meant to own every
  pixel of its canvas, and a transparent pixel means it forgot to.
  Step 17 may revisit this when it writes the fatal message.
- No measurement of real LÖVE was needed: the capture reads the fake's own
  canvas bytes, already proven equal to real LÖVE's by steps 6–15.
- The new module and tests include nothing from `src/`; the C packing and
  the palette bytes were read from `src/devtools/screencast.c` and
  `src/p8/palette.c` for this PRD only.
