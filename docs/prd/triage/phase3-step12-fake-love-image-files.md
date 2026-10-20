---
title: "fake_love loads newImageData(name) from a PNG exactly as real LÖVE 11.5 decodes it"
description: "The Lua game must load sprites.png and font.png from its game directory, but the fake LÖVE refuses the file form of newImageData, and nobody has measured whether real LÖVE's PNG decoder gives the same bytes as the fake's stb_image."
status: needs-triage
---

## Problem Statement

After plan steps 6 to 10, and step 11 under way, `tmp/fake_love` can build
an ImageData pixel by pixel. It cannot load one from a file:
`love.image.newImageData("sprites.png")` fails with "the file form is not
supported". The Lua game of plans 2–3 starts by loading its two assets,
`sprites.png` and `font.png`, from its game directory (design.md, "The
asset PNGs"). Every sprite, map tile and letter comes from them.

Real LÖVE 11.5 and the fake decode PNGs with different code. LÖVE uses
lodepng. The fake has the vendored `stb_image.h`, so far used only to read
the capture PNGs. If the two decoders disagree on any byte, the fake draws
a game that real LÖVE does not. Nobody would notice: the e2e replays
compare the fake with the C game, never with real LÖVE. Four things could
differ unnoticed:

- **Gamma.** A PNG can carry `gAMA`, `sRGB`, `cHRM` or `iCCP` chunks. A
  decoder that applies them changes every byte.
- **Formats.** 16-bit, greyscale, palette, colour-key transparency and
  interlaced PNGs each go through their own conversion code.
- **Broken files.** A decoder that skips a check accepts a file the other
  refuses, or decodes garbage.
- **Lookup.** Which file a name reaches: the game directory, a file the
  game wrote, a subdirectory.

## Solution

`love.image.newImageData(name)` joins the fake. It takes one string, a path
on the fake disk. It reads the file the way `love.filesystem.read` does: a
file the game wrote first, then the game directory. It decodes the file as
a PNG and returns a new ImageData of the PNG's size, holding its RGBA8
bytes. This ImageData is the same kind of object as one from
`newImageData(w, h)`: `setPixel`, `getPixel` and step 11's `newImage` all
work on it (H-11-1).

The fake accepts only PNGs that real LÖVE 11.5 was measured to decode to
the same bytes as stb_image, and refuses the rest with a named reason. It
accepts:

- bit depth 8;
- RGB or RGBA;
- interlaced or not;
- with or without a `tRNS` colour key on RGB;
- ancillary chunks, which are ignored (gamma included, as LÖVE does).

It refuses:

- every other bit depth and colour type: 16-bit becomes an `rgba16`
  ImageData in LÖVE, and palette indices out of range differ;
- every file LÖVE refuses but stb_image would decode: a bad chunk CRC, a
  bad zlib Adler-32, image data of the wrong length;
- sides above 1024, the ImageData store's limit.

The game's own assets are 8-bit RGB (design.md), so they load. Both were
decoded in real LÖVE and in stb_image while this PRD was written: the
bytes are identical.

One capture program pins the accepted kinds in real LÖVE on every CI push.
It reads five fixture PNGs and shows each decoded pixel through `getPixel`,
`setColor` and 1×1 rectangles: RGB in one block, alpha as grey in a second.
Its PNG is compared with the fake in `make test`. It uses no step 11
function, so it stands whatever step 11's outcome. Capture programs find
fixture files through a `files` directory that both harnesses can see. The
conventions go into `docs/love-port/design.md` as an "Image files"
paragraph, with the rule the game must follow.

## User Stories

1. As a plan-2 developer, I want `love.image.newImageData("sprites.png")` in the fake, so that the game loads its sprite sheet as it will in LÖVE.
2. As a plan-2 developer, I want `love.image.newImageData("font.png")` in the fake, so that the game builds its letter images from the font.
3. As a plan-2 developer, I want the decoded bytes to be exactly the bytes real LÖVE 11.5 decodes, so that every sprite pixel the fake draws is the pixel LÖVE draws.
4. As a plan-2 developer, I want an 8-bit RGB PNG to decode with alpha `FF` everywhere, so that the opaque assets stay opaque until the game makes its see-through copies.
5. As a plan-2 developer, I want an 8-bit RGBA PNG to keep its four bytes, alpha 0 and translucent alphas included, so that a PNG written by LÖVE's own `encode` reads back exactly.
6. As a plan-2 developer, I want gamma, chromaticity, sRGB and text chunks ignored, as LÖVE ignores them, so that no colour shifts between the fake and LÖVE.
7. As a plan-2 developer, I want interlaced PNGs and every row filter decoded exactly, so that an asset written by another tool still loads the same.
8. As a plan-2 developer, I want a `tRNS` colour key on an RGB PNG to give alpha 0 with the RGB kept, as LÖVE does, so that a keyed image behaves the same in both.
9. As a plan-2 developer, I want the loaded ImageData to support `getPixel` and `setPixel` like one made with `newImageData(w, h)`, so that I can build recoloured copies from it.
10. As a plan-2 developer, I want each call to return a new, independent ImageData, so that editing one copy of the sheet never changes a later load, as in LÖVE.
11. As a plan-2 developer, I want `love.graphics.newImage` to accept a loaded ImageData, so that `newImage(newImageData("sprites.png"))` works as in LÖVE (H-11-1).
12. As a plan-2 developer, I want a file the game wrote with `love.filesystem.write` to shadow the game directory's file of the same name, as LÖVE's save directory does, so that the lookup matches `love.filesystem.read`.
13. As a plan-2 developer, I want a path into a subdirectory of the game directory (`dir/name.png`) to load, so that assets may be grouped later.
14. As a plan-2 developer, I want a missing file or a directory refused with an error naming the path, so that a typo in an asset name fails at once.
15. As a plan-2 developer, I want a name the fake disk refuses (empty, leading `/`, `.` or `..`, `\`, `:`) refused here too, with the same reason as `love.filesystem.read`, so that one rule covers every file name.
16. As a plan-2 developer, I want a file that is not a PNG refused naming the path and the reason, so that a wrong file is reported where it is loaded.
17. As a plan-2 developer, I want 16-bit PNGs refused, with a message saying that LÖVE would make an `rgba16` ImageData, so that I re-export the asset in 8 bits.
18. As a plan-2 developer, I want greyscale, grey-alpha and palette PNGs refused, naming the colour type, so that only the measured kinds reach the game.
19. As a plan-2 developer, I want a PNG with a bad chunk CRC, a bad Adler-32, image data of the wrong length, an unknown critical chunk or no `IEND` refused, as LÖVE refuses it, so that a corrupted asset never decodes in the fake when LÖVE would fail.
20. As a plan-2 developer, I want a PNG wider or taller than 1024 refused naming its size, so that the ImageData limit of step 10 holds for files too.
21. As a plan-2 developer, I want the File and FileData forms, extra arguments (a format string, a trailing nil) and non-string arguments refused, so that the fake implements exactly the form the game uses.
22. As a plan-2 developer, I want a refused call to change nothing, so that an error caught by `pcall` leaves the state as it was.
23. As a plan-2 developer, I want `newImageData(w, h)` to keep every behaviour and message of step 10, so that nothing built on it moves.
24. As a plan-2 developer, I want `love.image` to stay nil in `conf.lua` and to hold exactly `newImageData` from `main.lua` on, so that no function exists without its capture.
25. As a plan-2 developer reading design.md, I want an "Image files" paragraph with the form, the lookup, the accepted and refused PNG kinds, the measured reasons and the rule for the game, so that I do not need plan 1, which will be deleted.
26. As a plan-2 developer, I want the real `game/sprites.png` and `game/font.png` decoded by the fake's PNG module in `make test`, so that a change to `make asset-export` that breaks the format is caught before plan 2 needs the files.
27. As a plan-1 developer, I want capture programs to read fixture files by a relative name that works in both harnesses, so that this and later steps can capture file input without copying the harness.
28. As a plan-1 developer, I want a capture program decoding five fixture PNGs (the asset format; RGBA with every filter and alpha; interlaced; ancillary chunks and split image data; a colour key) and showing every byte, so that real LÖVE pins the decoding on every CI push.
29. As a plan-1 developer, I want the capture to load the same file twice after editing the first copy, so that real LÖVE pins the independence of each load.
30. As a plan-1 developer, I want the PNG acceptance rules tested directly in a pure C module, with hand-built PNG bytes, so that a missing CRC, Adler or length check is caught with a precise message.
31. As a plan-1 developer, I want each misuse checked through the fake's command line with its message, so that the error contract is pinned, including the refusals real LÖVE does not make.
32. As the repository owner, I want `make test` measured before and after this step with design.md's protocol and kept within its gate, so that the fast loop stays fast.
33. As the repository owner, I want the CI love-capture job green with the new program and its fixture files, so that the PNG is proven to be what real LÖVE draws.
34. As the repository owner, I want `make e2e`, the C game and every `*.sha256` untouched, so that the reference is unaffected.
35. As the repository owner, I want nothing of this step to include or link anything from `src/`, so that plan 4 can delete `src/`.

## Implementation Decisions

### What real LÖVE 11.5 does (explored while writing this PRD)

How it was measured: `make love-capture LOVE_CAPTURE_DIR=<scratch>`, with a
copy of the real harness and probe programs that wrote their results to a
scratch file. The fake's own `stb_image.h` was built in a scratch program
with the fake's defines (`STBI_ONLY_PNG`, `STBI_NO_LINEAR`, `STBI_NO_HDR`,
4 channels requested). Run on LÖVE 11.5.0 with Mesa 25.2.8 llvmpipe. These
results are the expected answers of this step's tests. The committed PNG
remains the proof.

- **Lookup.**
  - The real harness's source is `tools/love_capture/harness`. Its
    identity is `harness`, and its save directory is
    `~/.local/share/love/harness`.
  - Symlinks are enabled. A directory symlink inside the harness is
    listed as `symlink` by `getInfo`, and files read through it load.
    `getDirectoryItems` on it returns nothing, which this step does not
    use.
  - A missing name fails with `Could not open file missing.png. Does not
    exist.` and a directory with `Could not read from file.`
  - A non-PNG file fails with `Could not decode file '<name>' to
    ImageData: unsupported file format`.
  - A file written with `love.filesystem.write` to the save directory
    shadows the source file of the same name:
    `getRealDirectory` names the save directory, and the bytes are the
    written file's.
- **Forms.**
  - A path string, a `FileData` and a `File` are all accepted. So are
    `newImageData(name, "rgba8")`, `newImageData(name, nil)` and a
    leading `/`.
  - `inputs/../inputs/x.png` fails with "Does not exist".
  - `newImageData(5)` fails with `bad argument #2 … (number expected, got
    no value)`, and a table with `value expected, got table`.
- **Independence.** Two loads of one file are independent: `setPixel` on
  the first leaves the second as decoded. `love.graphics.newImage`
  accepts a loaded `rgba8` ImageData.
- **Decoding, 8-bit and below: identical to stb_image.** 52 files were
  decoded in both, 13×7 unless stated, all with random content and
  rotating row filters. For every file whose bit depth is 8 or less,
  `getFormat()` is `rgba8`, every `getPixel` value is exactly
  `byte / 255`, and **the RGBA bytes equal stb_image's byte for byte**:
  - RGB 8 with stored and with zlib-compressed deflate;
  - RGBA 8 in both;
  - grey 1, 2, 4 and 8;
  - grey-alpha 8;
  - palette 1, 2, 4 and 8 with a partial `tRNS`, and palette 8 without
    one;
  - RGB 8 with a `tRNS` colour key, and grey 8 with one;
  - Adam7-interlaced RGB 8 and RGBA 8;
  - `gAMA` 0.45455, `gAMA` 1.0, `sRGB`, and `cHRM` with `gAMA` 0.2: **no
    gamma is applied**;
  - an unknown ancillary chunk;
  - image data split over two `IDAT` chunks;
  - garbage after `IEND`;
  - 1025×1 and 2048×2 images: LÖVE has no 1024 limit;
  - the committed `game/sprites.png` (128×128) and `game/font.png`
    (48×30).
- **Decoding, 16-bit: different.** RGB 16, RGBA 16, grey 16 and grey-alpha
  16 give an **`rgba16`** ImageData in LÖVE, whose `getPixel` returns
  `v / 65535`. stb_image gives 8 bits (`v >> 8`). About 363 of 364 bytes
  differ once compared as bytes.
- **Broken files: LÖVE refuses where stb_image decodes.**
  - A bad CRC on `IDAT`, on an ancillary `tEXt` or on `IEND`: LÖVE
    refuses with `invalid CRC encountered`. stb_image decodes.
  - A bad zlib Adler-32: LÖVE refuses (`custom zlib or inflate
    decompression failed`). stb_image decodes.
  - Image data longer than the image: LÖVE refuses (`invalid decompressed
    idat size`). stb_image decodes.
  - A palette index beyond the palette: LÖVE gives opaque black.
    stb_image gives other bytes.
- **Broken files: both refuse.** Shorter image data, a filter byte of 7,
  an unknown critical chunk, no `IEND`, a palette PNG without `PLTE`,
  `tRNS` on RGBA, bit depth 4 with RGB, width 0, a bad signature, an
  empty file, a truncated file.
- **A prototype gate** of about 60 lines of C, then stb_image from memory,
  separated all 57 files (the 52 probes and the 5 capture fixtures) exactly as the decisions below require:
  - it accepted every file that is 8-bit RGB or RGBA, all measured
    identical to LÖVE;
  - it refused every file LÖVE refuses, and every 16-bit, grey and
    palette file.

  The gate was:
  - the signature;
  - every chunk's length inside the file and its CRC valid;
  - `IHDR` first, bit depth 8, colour type 2 or 6, sides 1 to 1024;
  - an unknown critical chunk other than `PLTE` refused;
  - the walk stopping at `IEND`;
  - the concatenated `IDAT` inflated by stb_image's own zlib, its
    Adler-32 checked, and its length equal to the filtered size (per
    Adam7 pass when interlaced).
- **The capture draft below**, captured in real LÖVE. Compared with a
  model that decodes the five fixtures with stb_image and paints RGB and
  alpha as the program does: **0 differing pixels**, 3 812 opaque pixels.

### Decisions

- **Form: `newImageData(name)` only.** Exactly one argument, a Lua
  string, a valid fake-disk name. FileData and File are not on the
  design's list, and neither is the format argument. A table, a number
  alone, a boolean, two arguments with a string first, and a trailing nil
  are refused. Stricter than LÖVE, like every binding of the fake.
- **Lookup: the fake disk, unchanged.** The bytes come from the disk's
  read, as `love.filesystem.read` gets them: a written file first, then
  the game directory, subdirectories included (read only). Names are
  checked by the disk's name rule, with the reason the filesystem binding
  gives. A missing file and a directory are refused alike (the disk
  tells them apart for neither). A leading `/` is refused, though LÖVE
  accepts it, since `love.filesystem` already refuses it.
- **Accepted PNGs: exactly the measured identical kinds the game can
  use.**
  - Bit depth 8, colour type RGB (2) or RGBA (6).
  - Interlaced or not, any row filters.
  - `tRNS` colour key allowed on RGB. stb_image refuses it on RGBA, as
    LÖVE does.
  - Ancillary chunks and a suggested `PLTE` ignored, image data in any
    number of `IDAT` chunks, bytes after `IEND` ignored.
  - Grey, grey-alpha and palette are refused though they measured
    identical. The game's assets are RGB, LÖVE's own `encode` writes
    RGBA, and accepting palettes would need an index check besides. A
    later step that needs them measures them again and adds them with a
    capture.
- **Integrity checked before stb_image, as LÖVE checks it.** The gate's
  checks are the prototype's: signature, chunk bounds, every CRC, `IHDR`
  first, `IEND` reached, no unknown critical chunk, Adler-32, exact
  filtered length. stb_image then decodes from memory with 4 channels.
  Its own refusals (bad filter, `tRNS` on RGBA) pass through as reasons.
  The inflate is done twice, once by the gate and once by stb_image: the
  assets total about 64 KiB of pixels, so it costs nothing measurable,
  and stb_image stays unmodified.
- **Sides: 1 to 1024**, the ImageData store's limit (H-10-2's
  `IMAGE_DATA_MAX_SIDE`), checked from `IHDR` before inflating. LÖVE has
  no such limit (2048×2 measured), so this is a fake refusal, like step
  10's.
- **The result** is an ordinary ImageData built by the image data store
  (same metatable, same size rule, garbage collected), its bytes copied
  from the decoded pixels, which are then freed. Translucent alphas are
  kept: `getPixel` reads them, and step 11's `newImage` refuses them,
  naming the pixel (H-11-2).
- **Each call decodes anew.** No cache: LÖVE returns independent
  ImageData (measured), and the game loads each asset once.
- **Capture fixtures: a `files` directory beside the capture programs,
  reached through a `files` symlink in each harness.**
  - In real LÖVE, a capture program can only read files under its
    harness, the LÖVE source. A symlink there pointing at the capture
    directory's `files` directory works (measured).
  - The fake's capture check builds its disk over the fake harness, so
    the fake harness gets the same symlink.
  - Both harnesses then resolve `files/<name>.png` to the same bytes.
    Neither harness's Lua changes, nor the capture check.
  - The capture registration only globs `*.lua` and `*.png` at the
    capture directory's top level, and `make love-capture` only runs
    those. A fixture PNG in `files` is therefore neither a program nor a
    check.
  - A scratch `LOVE_CAPTURE_DIR` copy must copy `files` too. The
    symlink is relative.
- **Fixture files are committed bytes, not generated by the build.** They
  are written once with any throwaway tool (Python's `zlib` was used for
  the measurement). The table under "Capture program" specifies them, and
  nothing in `make` regenerates them. A changed fixture shows in the
  capture's PNG, which `make love-capture` rewrites and CI checks.
- **The real assets get no capture of their own.** They are 8-bit RGB
  with stored deflate. The first fixture is that exact format, and both
  assets were measured identical. A unit test decodes them through the
  fake's module (story 26). Plan 2's game will draw them in every replay.
- **Messages**, all prefixed `love.image.newImageData: `:
  - the count or form (`takes 2 numbers, width and height, or 1 file
    name, not N arguments`, `argument 1 is a <type>, not a file name`);
  - `invalid path "<name>": <reason>`;
  - `Could not open file <name>. Does not exist.` (LÖVE's wording);
  - `"<name>" is not a PNG the fake decodes as LÖVE 11.5 does: <reason>`.

  Reasons name the problem: `not a PNG signature`, `bad CRC in chunk
  <type>`, `bit depth 16 (LÖVE makes an rgba16 ImageData)`, `colour type
  3 (palette)`, `size 2048x2 is above 1024`, `zlib Adler-32 mismatch`,
  `image data is N bytes, M expected`, `unknown critical chunk <type>`,
  `no IEND`, or stb_image's reason. The exact wording is the
  implementer's. Tests match the prefix, the path and the key word.

### Modules

- **PNG file** (existing module) gains a pure in-memory decoder that takes
  a byte buffer, its length and a maximum side. It returns NULL with
  row-major RGBA8 pixels the caller frees, or a static reason. It holds:
  - the gate, with a CRC-32 and an Adler-32 of its own (bit-wise or
    table, the implementer's choice);
  - stb_image's zlib to inflate;
  - stb_image to decode.

  The existing file loader, used by the capture check to read the
  reference PNGs, is unchanged. The module's header states the accepted
  kinds and that 16-bit, grey and palette are refused on purpose.
- **Image binding** (existing) takes the disk, as the filesystem binding
  does. `newImageData` dispatches on its arguments: two numbers keep step
  10's path and messages, and one string is the file form. Its other
  refusal messages are unchanged, except "the file form is not
  supported", which goes. The ImageData accessor is unchanged.
- **Host** passes the disk to the image binding. Install order is
  unchanged.
- **Makefile.**
  - The PNG file module joins the fake's host sources: `tmp/fake_love`
    links it, and the capture check keeps linking it once.
  - Its new unit test joins the unit list and `FAKE_SIDE_BINARIES`.
  - The fixture wildcard of the fake's tests gains `.png` files.
  - The capture check needs no new rule (H-11-4).
- **Harnesses**: one relative `files` symlink each, nothing else.

### Capture program of this step

`imagedata_files` uses only `newImageData`, `getPixel`, `setPixel`,
`setColor` and `rectangle`, all from steps 8 to 10. Its fixture files, all
8-bit with a valid CRC on every chunk:

| file | size | type | content | chunks and encoding |
|---|---|---|---|---|
| `rgb_stored` | 24×16 | RGB | `(11x+7y, x²+3y, 200−xy) mod 256` | stored deflate, filter 0 on every row: the asset format |
| `rgba_filters` | 21×13 | RGBA | `(37x+y, 19y+3x, 5xy) mod 256`, alpha `[0, 255, (13x+7y) mod 256][(x+2y) mod 3]` | zlib level 9, row y uses filter y mod 5 |
| `rgb_interlaced` | 23×15 | RGB | `(9x, 17y, xy+40) mod 256` | Adam7, zlib, filter = row-in-pass mod 5 |
| `rgb_ancillary` | 17×11 | RGB | `(255−9x, 21y, 6(x+y))` | `gAMA` 1.0, `cHRM`, `sRGB`, `tEXt`, private ancillary `prVt`, a suggested 4-entry `PLTE`, `IDAT` split in two, `tEXt` after `IDAT`; zlib, filters mod 5 |
| `rgb_key` | 19×9 | RGB | `(10, 200, 30)` where `(xy+x) mod 3 = 0`, else `(10, 200, 31+x)` | `tRNS` key `(10, 200, 30)`; zlib, filters mod 5 |

The draft, captured in real LÖVE, 0 differing pixels from the stb_image
model:

```lua
local names = {"rgb_stored", "rgba_filters", "rgb_interlaced", "rgb_ancillary", "rgb_key"}
local loaded = {}
for i, name in ipairs(names) do
    loaded[i] = love.image.newImageData("files/" .. name .. ".png")
end
local edited = love.image.newImageData("files/rgba_filters.png")
for x = 0, 20 do
    edited:setPixel(x, 0, 1, 0, 0, 1)
end
local reloaded = love.image.newImageData("files/rgba_filters.png")

local function show(data, width, height, left, top)
    local graphics = love.graphics
    for y = 0, height - 1 do
        for x = 0, width - 1 do
            local r, g, b, a = data:getPixel(x, y)
            graphics.setColor(r, g, b)
            graphics.rectangle("fill", left + x, top + y, 1, 1)
            graphics.setColor(a, a, a)
            graphics.rectangle("fill", left + x, top + height + 2 + y, 1, 1)
        end
    end
end

local sizes = {{24, 16}, {21, 13}, {23, 15}, {17, 11}, {19, 9}}

return function()
    for i, data in ipairs(loaded) do
        show(data, sizes[i][1], sizes[i][2], 2 + (i - 1) * 32, 2)
    end
    show(edited, 21, 13, 2, 40)
    show(reloaded, 21, 13, 34, 40)
end
```

What it pins:

- every decoded byte of the five kinds, alpha shown as grey because
  `setColor` refuses alpha (design.md, "Colour");
- a `getPixel` value passed to `setColor` draws its byte (step 10's
  rule);
- no gamma;
- the colour key gives alpha 0 with the RGB kept;
- interlacing and every filter;
- the reload at (34, 40) equals the original while the edited copy at
  (2, 40) has a red first row.

The plan's edge cases (off-canvas, negative coordinates, nested translate)
concern drawing, which this step does not add. Transparent pixels are
here: alpha 0 and translucent alphas in `rgba_filters` and `rgb_key`.
If the PNG disagrees with this section, the PNG wins and design.md follows
it.

### Conventions written into `docs/love-port/design.md`

- An **"Image files"** paragraph after "Image data" (and before step 11's
  "Images" paragraph, H-11-6). It holds:
  - the form;
  - the lookup (fake disk, written files first, as LÖVE's save
    directory);
  - each call independent;
  - the accepted kinds;
  - no gamma (measured);
  - the refusals with their measured reason (16-bit gives `rgba16`;
    integrity checks LÖVE makes and stb_image does not; palette indices;
    grey and palette refused though identical);
  - the 1024 limit;
  - the error prefix;
  - **the rule for the game**:
    - load assets by their bare name from the game directory, once, in
      `love.load`;
    - keep them 8-bit RGB or RGBA (`make asset-export` writes RGB);
    - a translucent alpha loads but cannot become an Image (H-11-2);
  - the capture `imagedata_files`.
- "Image data" loses "the file form `newImageData(file)` is not
  implemented yet".
- The function list's "Images" line becomes `newImageData(name)`.
- "Real-LÖVE captures" gains one sentence: a capture program reads
  fixture files as `files/<name>`, from the capture directory's `files`
  directory, through a symlink in each harness. Fixtures are committed
  bytes.

## Implementation Chunks

1. **The PNG decoder.**
   - The in-memory decoder with its gate in the PNG file module.
   - Its unit test, including the two real assets.
   - Stories 3–8, 17–20, 26, 30.
2. **`newImageData(name)` in the fake.**
   - The image binding's file form and the disk passed by the host.
   - The fake linking the PNG module.
   - The CLI test's new fixture games, and step 10's log updated.
   - Stories 1, 2, 9–16, 21–24, 31.
3. **Capture, docs, proof.**
   - The `files` fixtures and both symlinks.
   - The program and its PNG from `make love-capture` (git diff shows only
     the new files).
   - The comparison passing in `make test`.
   - design.md, the timing, the CI run, the step note.
   - Stories 25, 27–29, 32–35.

## Testing Decisions

- A good test drives a public interface and checks external behaviour:
  the decoder's pixels and reason, the binary's exit status, stderr and
  state file, the capture comparison's verdict. No test reaches into the
  Lua state or the decoder's internals.
- **PNG decoder, unit test** (new, in the unit list). It works on PNG
  bytes built in memory by a small test-side builder, which may extend
  the tests' existing PNG writer. The builder picks colour type, bit
  depth, interlace flag, extra chunks and stored or real deflate, and
  can corrupt a CRC, the Adler-32 or the data length on demand.
  - Accepted:
    - a 3×2 RGBA gives exactly its bytes, alpha 0 and 128 included;
    - a 3×2 RGB gives its bytes with alpha `FF`;
    - an RGB with a `tRNS` key gives alpha 0 on matching pixels with
      their RGB kept;
    - a `gAMA` 1.0 chunk, a private ancillary chunk, a suggested `PLTE`,
      split `IDAT` and bytes after `IEND` change nothing;
    - 1024×1 is accepted.
  - Refused, each reason checked by key word:
    - an empty buffer, a bad signature, a truncated chunk;
    - a bad CRC on `IHDR`, on an ancillary chunk and on `IEND` (a
      failing assert says the gate probably skips CRCs, which stb_image
      never checks);
    - a bad Adler-32 (the assert says stb_image does not check it);
    - image data one row longer and one byte shorter;
    - bit depth 16 on RGB and RGBA;
    - colour types 0, 3 and 4;
    - an unknown critical chunk;
    - no `IEND`;
    - widths 0 and 1025;
    - filter byte 7.
  - The real assets: `game/sprites.png` and `game/font.png` are
    accepted, 128×128 and 48×30, with every alpha `FF`. Reading `game/`
    is not reading `src/`.
- **Image binding, CLI test**: step 10's test target, extended, not a new
  target, to keep `make test` within its gate.
  - Step 10's log keeps every line except `new file`, now an open
    failure naming `sprites.png`. `new string` (`"4", 3`) stays refused.
  - The accessor test passes a disk to the binding.
  - A new fixture game logs, through `pcall` and `debug_state()`:
    - the decoded values of a committed 3×2 RGBA PNG with alphas 0, 128
      and 255, and of a committed 2×2 RGB PNG;
    - a load from a subdirectory of the fixture game;
    - `setPixel` on a loaded ImageData and a second load unchanged;
    - `newImage` of the RGB load accepted, and of the RGBA load refused
      naming the translucent pixel (H-11-1, H-11-2);
    - a PNG read with `love.filesystem.read` and written under the other
      PNG's name, then loaded, giving the written file's bytes (shadow);
    - misuses, each with its message: no argument, a missing file, the
      subdirectory itself, `conf.lua` (not a PNG), a non-PNG string
      written to memory, `""`, `"/rgb.png"`, `"../rgb.png"`, `"a\\b"`,
      a table, `true`, `5` alone, `("rgb.png", "rgba8")`,
      `("rgb.png", nil)`;
    - `love.image` keys still exactly `newImageData`.
  - An uncaught `newImageData("missing.png")` in `love.load` exits 1
    naming `love.load`, `love.image.newImageData` and the path.
- **The capture comparison** is the test of the decoding conventions:
  `imagedata_files` passes in `make test`, and every earlier capture still
  passes.
- **Prior art:**
  - `test_fake_love_image` (step 10: log and uncaught fixtures, the
    accessor test);
  - `test_fake_love_files` (disk names and shadowing);
  - the tests' PNG writer;
  - `test_fake_love_capture`.
- **Proofs recorded in the step note** (Step 3.12 in plan 1):
  - `make love-capture` writes the new PNG and leaves every other file of
    `tools/love_capture/` byte-identical;
  - the CI run on the pushed commit with the love-capture job green (run
    id);
  - `make test` before and after, with design.md's protocol: 11 rotated
    paired rounds against the step's "before" (step 11's last commit,
    H-11-5) and against `2e7215c`. Both paired medians must be ≤ 3 %
    (22 pooled rounds if either exceeds it). Record the plain and paired
    medians, spreads, rounds, cores, and the slowest new check's own
    time.
  - The new work is one unit binary (milliseconds), one capture check
    (about 4 800 `getPixel` calls and 9 600 `setColor`/`rectangle` calls,
    a few milliseconds), and cases added to an existing CLI target. No
    new CLI target. If the gate still fails, design.md rule 5 applies.
- The C game's unit tests, `make e2e` and every `*.sha256` stay
  unchanged.

## Out of Scope

- File and FileData forms, the format argument, `ImageData:encode`,
  `getFormat`, `getDimensions` and every other ImageData method.
- Grey, grey-alpha, palette and 16-bit PNGs (measured, refused), and
  every image format other than PNG (LÖVE also decodes JPEG, TGA, BMP,
  …).
- `newImage(name)` straight from a file: `newImage` takes an ImageData
  only (step 11's decision, H-11-3).
- Drawing a loaded image: steps 11 and 15. `push`/`pop`/`translate`:
  step 13. `setDefaultFilter`: step 14.
- Caching decoded files, a larger ImageData limit.
- Changing `make asset-export` or the asset format.
- Audio, always.

## Hypotheses

Plan step 11 (`newImage`, `newQuad`, `draw`) is being implemented while
this PRD is written. Its PRD was read, and nothing of its code is. Before
starting, the implementer checks each item below against the committed
step 11. When one differs, adapt the names and the one dependent test
line, not the decisions. This step's capture uses no step 11 function, so
only the CLI test's `newImage` lines and design.md's placement depend on
step 11.

- **H-11-1**: `love.graphics.newImage(data)` accepts any value step 10's
  accessor finds as an ImageData, and copies its bytes. An ImageData
  this step builds from a file has the same metatable, so it is accepted
  with no change to step 11's code.
- **H-11-2**: `newImage` refuses an ImageData holding an alpha other than
  `00` and `FF`, naming the first such pixel. This step therefore keeps
  translucent alphas from a PNG in the ImageData, and leaves the refusal
  to `newImage`.
- **H-11-3**: `newImage` refuses a file name, as step 11's PRD decides
  (`newImage("sprites.png")` in its misuse list). This step keeps that:
  the game goes through `newImageData(name)`.
- **H-11-4**: step 11 leaves unchanged:
  - the image binding's install signature and the accessor;
  - the host's install order;
  - the fake's host sources, without the PNG module;
  - both harnesses' Lua;
  - the capture check;
  - the Makefile's capture registration, so a `.lua` and `.png` pair in
    `tools/love_capture/` becomes a check by itself.

  It creates no `files` directory or symlink.
- **H-11-5**: step 11's last commit is this step's "before" timing
  baseline, and its note (Step 3.11) records its timings. Its paired
  ratio against `2e7215c` is not known here. Step 10 ended at +2.79 %,
  so this step adds no new CLI target.
- **H-11-6**: step 11 adds an "Images" paragraph after "Image data" in
  design.md, and changes the function list's "Drawing" line only. This
  step's "Image files" paragraph goes between the two.
- **H-11-7**: step 11 changes neither the image data store nor the disk,
  and `love.image` still holds exactly `newImageData`.

## Further Notes

- The exploration files lived in a scratch directory passed to `make
  love-capture`, with a copy of the harness and a symlink to the fixture
  directory. Nothing of them is committed. Bytes were compared with
  Python. Running the probes created an empty `~/.local/share/love/harness`
  save directory, which was removed. A stale file there would shadow the
  source in real LÖVE, so capture programs never write files.
- The decoding table above is the reason for each refusal, should a later
  plan need more kinds:
  - grey and palette are one capture and one palette-index check away;
  - 16-bit needs an `rgba16` ImageData the fake does not have.
- Real LÖVE decodes PNGs with lodepng (its error strings show it).
  stb_image matches it on every well-formed 8-bit file measured. The two
  part ways only where lodepng checks integrity and stb_image does not,
  which is what the gate restores.
