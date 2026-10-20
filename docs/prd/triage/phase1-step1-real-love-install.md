---
title: "Real LÖVE 11.5 in the container and CI: install script, first capture, CI on the working branch"
description: "Nothing in the container or CI can run real LÖVE 11.5 or build against LuaJIT yet, so no capture program can produce the reference PNGs the fake LÖVE must match."
status: needs-triage
---

## Problem Statement

Plan 1 builds a fake LÖVE whose every drawing convention must come from PNGs
that real LÖVE 11.5 produced. Today neither the cloud container nor CI has
LÖVE, Mesa software rendering wired for it, or the LuaJIT development package;
there is no `make love-capture`; `CLAUDE.md` forbids any virtual screen without
exception; and CI only runs on `master` and pull requests, so a commit pushed to
the working branch gets no CI verdict. An agent starting the next steps would
have no way to take a real-LÖVE capture, no way to build a C program against
LuaJIT, and no proof that the container and CI see the same versions.

## Solution

One committed install script brings LÖVE 11.5, Xvfb, Mesa and the LuaJIT
development package to an Ubuntu 24.04 machine. Every `ci.yml` job that runs
`make` calls it, and a SessionStart hook calls it in cloud sessions, so the
container and CI install the same packages from the same archive.

`make love-capture` runs every capture program of `tools/love_capture/` in real
LÖVE 11.5, under Xvfb with Mesa software rendering, and rewrites each program's
PNG. Step 1 ships exactly one program: it clears the 160×120 canvas to colour 1
(`1D2B53`). `make love-check` proves the toolchain: it builds a C program
against the system LuaJIT that prints its version, and checks that the
committed PNG of the clear program is 160×120 pixels of opaque `1D2B53`. A new
CI job runs install, `make love-capture` and `make love-check`; CI now also runs
on pushes to the working branch. `CLAUDE.md` carries the Xvfb exception.

## User Stories

1. As an agent in a cloud session, I want LÖVE 11.5 installed when the session starts, so that I can run `make love-capture` without installing anything by hand.
2. As an agent, I want the install to be skipped quickly when every package is already present, so that a resumed session does not pay for a reinstall.
3. As an agent, I want the install script to fail loudly when the LÖVE it would install is not 11.5, so that I never capture PNGs with the wrong LÖVE.
4. As an agent, I want the install script to print the installed versions of LÖVE, LuaJIT, Mesa and Xvfb at its end, so that the container log and the CI log can be compared line by line.
5. As a maintainer, I want one install script used by CI and by the session hook, so that the two environments cannot drift apart through two copies of a package list.
6. As a maintainer, I want every CI job that runs `make` to call the install script, so that a later step adding a LuaJIT-linked binary to `make test` already finds its library in every job.
7. As a maintainer, I want CI to run on pushes to the agent's working branch, so that each pushed commit gets a green or red verdict before any pull request exists.
8. As a maintainer, I want the working-branch trigger to match every `claude/` branch, so that the next session's branch name needs no `ci.yml` edit.
9. As an agent, I want `make love-capture` to run each capture program in real LÖVE and rewrite its PNG, so that reference images come from LÖVE, never from guesses.
10. As an agent, I want `make love-capture` to read the canvas, never the window, so that window size, scaling and the window system cannot leak into a reference PNG.
11. As an agent, I want `make love-capture` to fail with a non-zero status and the Lua error text when a capture program errors, so that LÖVE's interactive error screen never hangs the run.
12. As an agent, I want every capture run bounded by a timeout, so that a stuck LÖVE process cannot block CI or a session forever.
13. As an agent, I want `make love-capture` to fail when the GL renderer is not Mesa's software renderer, so that the container and CI draw through the same rasterizer.
14. As an agent, I want `make love-capture` to fail when the running LÖVE is not 11.5, so that a stray system LÖVE is caught at capture time too.
15. As an agent, I want `make love-capture` to fail with a message naming the install script when `love` or `xvfb-run` is missing, so that I know the one command that fixes it.
16. As an agent, I want the capture program's drawing part to be a separate file from the LÖVE harness that creates the canvas and writes the PNG, so that the fake LÖVE of later steps can run the very same drawing file.
17. As an agent, I want the first capture program to clear the canvas to colour 1, so that the whole pipeline (canvas, clear, read-back, PNG encode) is proven on the simplest image.
18. As an agent, I want `make love-check` to confirm that the clear program's PNG holds 160×120 pixels, all `1D2B53` and opaque, so that the pipeline's output is checked by something other than LÖVE itself.
19. As an agent, I want `make love-check` to build a C program against the system LuaJIT through `pkg-config` and print its version, so that the next steps know embedding LuaJIT works on this machine.
20. As an agent, I want that C program to fail when the header's LuaJIT version and the running library's `jit.version` disagree or are not 2.1, so that a mismatched dev package is caught before the fake embeds it.
21. As a maintainer, I want a dedicated CI job that installs, runs `make love-capture` and then `make love-check`, so that the CI log shows the same LuaJIT version line and the same PNG check as the container.
22. As a maintainer, I want that job to be the one step 2 extends with its `git diff --exit-code`, so that step 2 adds one CI step instead of a new job.
23. As a maintainer, I want `make test` and `make e2e` untouched, so that the fast loop stays exactly as fast as before.
24. As a maintainer, I want `CLAUDE.md` to state that Xvfb with Mesa software rendering is allowed only inside `make love-capture`, so that no agent opens a virtual screen anywhere else and no agent refuses to run the capture.
25. As a maintainer, I want `make help` to list `love-capture` and `love-check` with a one-line description of at most 20 words, so that the targets are discoverable like every other.
26. As an agent, I want the SessionStart hook to do nothing outside cloud sessions, so that a developer's own machine is never touched by `apt-get`.
27. As a maintainer, I want the hook merged into the existing `.claude/settings.json` without losing its deny list, so that merging a pull request stays forbidden.
28. As a maintainer, I want the capture harness to disable LÖVE's audio modules, so that a container without a sound device cannot fail a capture (audio is out of scope anyway).
29. As an agent, I want the committed clear PNG to be byte-stable across runs, so that step 2's `git diff --exit-code` can rely on it.

## Implementation Decisions

- **Install script.** One POSIX shell script under `tools/`, the only place the package list lives. It installs, with `apt-get install -y --no-install-recommends`: `love`, `xvfb`, `xauth`, `libgl1-mesa-dri` (Mesa, llvmpipe), `luajit`, `libluajit-5.1-dev`, `pkg-config`. Ubuntu 24.04 only: on any other system it exits non-zero naming what it supports. It uses `sudo` when not root (the CI runner) and runs `apt-get` directly as root (the container). Idempotent: when `dpkg-query` reports every package installed, it skips `apt-get update` and the install. After installing (or skipping), it checks that the installed `love` version starts with `11.5`, fails otherwise, and prints one line per package with its installed version.
- **Versions.** No exact Debian revision is pinned: the container and CI are both Ubuntu 24.04 on the same archive, which today serves `love` 11.5-1build1 and LuaJIT 2.1.0+git20231223. The guards are the `11.5` check in the script and in the capture harness, and the LuaJIT 2.1 check of the version program. Recorded so a later archive update is caught by those checks rather than by a pin that breaks on every security rebuild.
- **SessionStart hook.** A script under `.claude/hooks/`, registered under `hooks.SessionStart` in `.claude/settings.json`, merged with the existing `permissions.deny` list (kept as is). It exits 0 at once unless `CLAUDE_CODE_REMOTE` is `true`, then calls the install script. Synchronous, not async: an agent's first `make love-capture` must not race the install; the cost is paid once since the container state is cached after the hook.
- **CI trigger.** `on.push.branches` becomes `[master, 'claude/**']`; `pull_request` stays. A pattern, not today's branch name, so each new working branch is covered.
- **CI jobs.** The existing `test` job gains an install-script step before its first `make`. A new job (`love-capture`, `ubuntu-24.04`) runs: checkout, the install script, `make love-capture`, `make love-check`. Step 2 will append its `git diff --exit-code tools/love_capture/` step to this job.
- **Capture program format.** A capture program is a Lua file `tools/love_capture/<name>.lua` that returns one function; called with the 160×120 canvas already current, it draws using only functions of the design list. Its PNG is `tools/love_capture/<name>.png`, committed. The first program, `clear`, calls `love.graphics.clear` with colour 1's bytes divided by 255 (`0x1D, 0x2B, 0x53` from `src/p8/palette.c`, alpha 1).
- **Capture harness.** A LÖVE game directory under `tools/love_capture/` (its own `conf.lua` and `main.lua`) that takes the program path and the PNG path in `arg`. `conf.lua`: window 160×120, vsync off, `audio` and `sound` modules disabled, `gammacorrect` off. `main.lua`: on the first `love.draw`, checks `love.getVersion()` is 11.5 and that `love.graphics.getRendererInfo()` names `llvmpipe` (failing otherwise), prints both, creates `newCanvas(160, 120)`, sets it, calls the program's function, unsets it, reads back with `Canvas:newImageData():encode("png")`, writes the bytes to the PNG path with Lua's `io.open` (LÖVE's filesystem cannot write outside its save directory), then `love.event.quit(0)`. `love.errorhandler` is overridden to print the error and traceback to stderr and `os.exit(1)`, so a failure never shows LÖVE's error screen. The canvas read-back and the PNG write live only in this harness, never in a program file, so the fake can run program files unchanged.
- **`make love-capture`** (`## runs the tools/love_capture/ programs in real LÖVE 11.5 under Xvfb, rewrites their PNGs`). Fails first with a message naming the install script when `love` or `xvfb-run` is missing. Then one `xvfb-run -a` (screen 640×480×24) wraps a sequential loop over every `tools/love_capture/*.lua` program, each run as `LIBGL_ALWAYS_SOFTWARE=1 timeout 60 love <harness dir> <program> <png>`; any failure fails the target, naming the program. Not under the `flock` of `test`/`e2e`/`test-all`, not a prerequisite of either. It is the only target allowed to start Xvfb.
- **`make love-check`** (`## builds a LuaJIT version probe and checks the clear capture is 160×120 of 1D2B53`). Builds two C programs into `tmp/` and runs them; not in `make test`, `make e2e` or `make test-all`:
  - *LuaJIT version probe*: compiled with `pkg-config --cflags --libs luajit`, opens a `lua_State`, prints the header's `LUAJIT_VERSION` and the runtime `jit.version`, exits non-zero unless both are equal and start with `LuaJIT 2.1`. It is the seed of the fake's startup version check (step 3).
  - *PNG check*: decodes the clear program's PNG with `stb_image.h` and exits non-zero, naming the first offending pixel or the wrong size, unless it is 160×120 with every pixel `1D2B53` at alpha 255. The existing `tests/png_reader` cannot decode LÖVE's compressed PNGs, hence `stb_image.h`.
- **Vendoring `stb_image.h`.** Done now, as the plan's rules already decide it, in `tools/fake_love/` where the fake will include it (not `third_party/`, which `.gitignore` excludes). One file, unmodified, its upstream version and licence line kept at its top.
- **Where the C programs live.** The probe and the PNG check are tool programs, not game binaries: they go next to the capture harness in `tools/love_capture/`, compiled with the repo's `CFLAGS` except that the PNG check's translation unit defines `STB_IMAGE_IMPLEMENTATION` and may need the stb-specific warnings silenced for that unit only. They are not unit tests and do not go in `tests/`.
- **`CLAUDE.md`.** The "Never open a window or a virtual screen" bullet gains its single exception: Xvfb with Mesa software rendering is allowed only inside `make love-capture`, which runs the capture programs in real LÖVE 11.5, rewrites their PNGs and stops; agents and CI may run it; it is never part of `make test` or `make e2e`. The Build section gains one line: the install script brings LÖVE, Xvfb, Mesa and LuaJIT, run by the SessionStart hook and CI.

## Implementation Chunks

1. **Install script, hook, CI wiring.** The install script; the SessionStart hook and its merge into `.claude/settings.json`; the install step in the `test` job; the `claude/**` push trigger. Verified by running the script twice in the container (second run skips the install) and printing the versions.
2. **LuaJIT probe and `make love-check`'s first half.** The version probe and the target building and running it. Verified by the target printing `LuaJIT 2.1…` in the container.
3. **Capture harness, clear program, `make love-capture`, PNG check.** Vendor `stb_image.h`, the PNG check, the harness, the clear program, `make love-capture`; `make love-check` gains the PNG check. Red first: `make love-check` fails because the PNG is missing; green after `make love-capture` writes it; committed PNG. Then rerun `make love-capture` and confirm the PNG is byte-identical.
4. **CI job and `CLAUDE.md`.** The `love-capture` job; the `CLAUDE.md` exception and Build line. Verified once pushed by a green CI run on that commit, every job included, with the probe's version line in the log.

## Testing Decisions

- These are environment and tooling changes; their tests are the commands themselves, run red then green: `make love-check` must fail before the PNG exists and when the PNG is altered (one pixel changed by hand, then the file restored with `make love-capture`), and pass on the real capture.
- The probe's failure path is checked once by hand (e.g. comparing against a wrong expected prefix in a throwaway edit, reverted); not kept as a test.
- No unit test in `tests/`: one would put `stb_image.h` and LuaJIT into `make test`, which `CLAUDE.md` forbids slowing. Step 6 owns the in-`make test` PNG comparison harness.
- `make test` and `make e2e` are not touched; their wall time is unchanged and every `*.sha256` of `tests/e2e/` stays as is.
- Container and CI must both show: the install script's version lines, the probe's `LuaJIT 2.1…` line, `make love-check` passing on the clear PNG. The CI side is verified by a green run of every job on the pushed commit (pushing is done by the caller's flow, not by this PRD's implementer).
- The hook is validated by running it directly with `CLAUDE_CODE_REMOTE=true` (installs or skips, exits 0) and without it (exits 0, does nothing).
- Prior art: the Makefile's existing tool targets (`asset-export`, `float-link`) for how a `tmp/` tool binary is built and run; `tools/*.sh` for script style.

## Out of Scope

- Step 2: the CI `git diff --exit-code tools/love_capture/` check and its deliberate red run.
- Any fake LÖVE code (`tmp/fake_love`, loop, pad, disk): steps 3–5. Only `stb_image.h` is vendored into its future directory.
- Any capture program other than `clear`, and any pixel convention in `docs/love-port/design.md`: phase 3.
- macOS installation (the design's `brew install luajit` stays documentation; the script supports Ubuntu 24.04 only).
- Audio, keyboard play, iOS.

## Further Notes

- `1D2B53` is colour 1 of `P8_PALETTE_RGB`; the clear goes through byte/255, the same rule the Lua game will use for `setColor`.
- If `love.graphics.clear` on an rgba8 canvas does not return exactly `1D2B53` under llvmpipe, that is a finding to record in the step note, not to fudge: the PNG check stays strict.
- The working branch at writing time is `claude/friendly-bell-yrer76`, covered by the `claude/**` pattern.
