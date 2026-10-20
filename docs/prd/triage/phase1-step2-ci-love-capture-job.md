---
title: "CI capture job fails when a regenerated real-LÖVE PNG differs from the commit"
description: "The love-capture CI job regenerates the PNGs but never compares them with the committed ones; it must fail when a capture program and its committed PNG have drifted apart."
status: needs-triage
---

## Problem Statement

The PNGs in `tools/love_capture/` are the ground truth for every pixel convention the fake LÖVE must reproduce (plan 1, Rules). Step 1 added a `love-capture` CI job that installs real LÖVE 11.5, runs `make love-capture` (which rewrites each program's PNG) and runs `make love-check` (LuaJIT probe, and `clear.png` holds 160×120 of `1D2B53`). But the job never compares the regenerated PNGs with the committed ones: a capture program can be edited, or a new one added, while its committed PNG still shows the old drawing — or no PNG is committed at all — and CI stays green. From then on `make test` would compare the fake against a picture real LÖVE never drew. The agents implementing phases 2–4 run unattended; discipline is not a safeguard.

## Solution

The existing `love-capture` CI job gains two checks, run right after `make love-capture` and before `make love-check`: `git diff --exit-code -- tools/love_capture/` (a committed file was changed by the regeneration) and a check that `tools/love_capture/` holds no file the commit does not (a PNG written for a program whose PNG was never committed). On failure each check names the offending files in the log. No second job is added.

The job is proven live: a commit deliberately changing `clear.lua`'s drawing without regenerating `clear.png` is pushed and its run is red on the diff step of `love-capture`; a commit reverting it is pushed and its run is green. Both run links go in the step note.

## User Stories

1. As a plan-1 implementer agent, I want CI to fail when I change a capture program without regenerating its PNG, so that a stale PNG can never become the reference for the fake.
2. As a plan-1 implementer agent, I want CI to fail when I add a capture program without committing its PNG, so that every program has the real-LÖVE picture the Rules require.
3. As a plan-1 implementer agent, I want CI to fail when someone hand-edits a PNG, so that only real LÖVE ever produces a reference picture.
4. As a plan-1 implementer agent, I want the failing step's log to list exactly which files under `tools/love_capture/` changed or appeared, so that I know which program to look at without downloading anything.
5. As a plan-1 implementer agent, I want the drift check to run before `make love-check`, so that a drift is reported as a drift, not as a confusing colour mismatch from `png_check`.
6. As the repository owner, I want this check to run on every push to master, every `claude/**` branch and every pull request, so that no path into the code skips it.
7. As the repository owner, I want the check inside the existing `love-capture` job, so that all real-LÖVE work lives in one job and the LÖVE install runs once per workflow run for it.
8. As the repository owner, I want CI to compare the regenerated PNGs byte for byte with the commit, so that container and CI provably produce the same files (the install script pins LÖVE 11.5 for both).
9. As the repository owner, I want `make test` and `make e2e` untouched by this step, so that the fast loop stays as fast as `CLAUDE.md` demands.
10. As the repository owner, I want the virtual screen to exist only inside `make love-capture`, so that the Xvfb exception in `CLAUDE.md` stays the only one.
11. As a reviewer, I want the step note to carry the link of one red run and one green run, so that I can see the job really catches a drift rather than passing vacuously.
12. As a reviewer, I want the deliberate change reverted in a separate commit, so that the tree after this step holds no trace of the experiment except in history.
13. As a future step-6-to-15 implementer, I want to regenerate PNGs locally with `make love-capture` and get the exact bytes CI produces, so that I commit PNGs once and CI agrees.
14. As the repository owner, I want the job bounded in time, so that a hung LÖVE or Xvfb cannot hold a runner for hours.
15. As a maintainer reading `ci.yml`, I want the new steps to be the plain git commands, so that the job reads as its purpose with no wrapper to open.
16. As an agent with no eyes, I want the check to be a plain exit status, so that I never need to look at a picture to know whether CI agrees.
17. As the plan-pilot, I want this step's acceptance to be pushed runs with known results, so that the phase-end rule ("every CI job green on the last pushed commit") is checkable.

## Implementation Decisions

- **Extend, don't add:** the existing `love-capture` job of the CI workflow is modified; no new job, no change to the `test` job.
- **Step order in the job:** checkout; `sh tools/love_install.sh`; `make love-capture`; **new** drift check on modified files; **new** check on files the commit does not hold; `make love-check`. Putting the checks before `love-check` makes a drift fail on the drift step, which is what the proof below must show.
- **Modified-file check:** `git diff --exit-code -- tools/love_capture/`, printing `git status --short -- tools/love_capture/` before exiting non-zero so the log names the files.
- **New-file check:** fails when `git status --porcelain --untracked-files=all -- tools/love_capture/` prints anything, printing that output. It goes beyond the plan's single `git diff`, which ignores files the commit does not hold: a new program without a committed PNG would otherwise pass. Recorded as a deliberate tightening, in line with design.md ("fails when a PNG changes"). It covers the whole directory, including `harness/` and the C sources of `love-check`: `make love-capture` writes nothing there and `make love-check` builds into `tmp/`, so they never show up — and the check runs before `love-check` anyway.
- **Time bound:** `timeout-minutes: 10` on the job (each LÖVE run is already capped at 60 s by `make love-capture`; the job-level bound also covers the install).
- **Byte-level comparison, not pixel-level:** the job compares files as git sees them. Two local runs of `make love-capture` were byte-identical (step 1); CI comparing against the committed `clear.png` is what checks container-versus-CI identity (H-1-6). If the first run of the extended job is red on an untouched tree, that is a container/CI encoding difference: this step finds its cause (differing package versions from `tools/love_install.sh`, a time-dependent PNG chunk) and makes the bytes identical; it does not introduce a pixel-level comparison.
- **No new make target**, nothing added to `make test`, `make e2e` or `make test-all`.
- **Deliberate-drift verification:** commit A changes `clear.lua` so it clears to colour 2 instead of 1, without regenerating `clear.png`; push; the `love-capture` job of that run is red on the modified-file step and its log names `tools/love_capture/clear.png`. Commit B reverts A (`git revert`); push; the run is green on every job. Commit messages say plainly that A is a deliberate CI check reverted by B. The run URLs of A (red) and B (green) and the job's wall time on B are recorded in the step note `### Step 1.2` of the plan file.
- **Precondition:** step 1's first CI run on its pushed commit must be green on both jobs before this step pushes anything; if it is red, that red goes through `fix-bug` first (plan Rules), since a red baseline would make commit A's red meaningless.

## Testing Decisions

- A good test here checks external behaviour only: the job's step results on a real push, not the YAML's shape.
- No unit test is written: the behaviour lives in CI configuration and git, which this repository's C test suite does not cover. `make test` and `make e2e` stay unchanged and green.
- Acceptance tests, in order:
  1. In the container: `make love-capture` twice in a row, `git status --porcelain --untracked-files=all -- tools/love_capture/` empty after each.
  2. The pushed commit extending the job: `love-capture` green, both new steps passing (container and CI bytes identical).
  3. Commit A pushed: `love-capture` red on the modified-file step, log naming `clear.png`.
  4. Commit B pushed: every job green.
- Prior art: the `love-capture` and `test` jobs of the CI workflow (checkout, install, `make` targets); `tools/frames_check.sh` for failing with the names of what differs.

## Out of Scope

- The install script, `make love-capture`, `make love-check`, the harness, the CI triggers, the `CLAUDE.md` Xvfb exception: step 1, done.
- Comparing the fake's output to the PNGs (step 6) and any capture program beyond `clear.lua`.
- Pixel-level diff tooling, PNG artifacts in CI, caching the LÖVE install between runs.
- Any change to `make test`, `make e2e`, `make test-all` or the `test` job.

## Hypotheses

Checked against step 1's committed state; what remains assumed is marked.

- **H-1-1** — Holds: `tools/love_install.sh` installs LÖVE 11.5, Xvfb, Mesa and the LuaJIT dev package, uses `sudo` when not root, supports Ubuntu 24.04 only, and verifies the LÖVE version. CI calls it as `sh tools/love_install.sh`.
- **H-1-2** — Revised: step 1 already added the `love-capture` job (checkout, install, `make love-capture`, `make love-check`). This step extends that job instead of adding one.
- **H-1-3** — Holds: `make love-capture` wraps one `xvfb-run` around a loop over `tools/love_capture/*.lua`, runs each through the real-LÖVE-only harness in `tools/love_capture/harness/`, writes `<name>.png` next to the program, and fails naming the program.
- **H-1-4** — Revised: `tools/love_capture/` holds `clear.lua`, its committed `clear.png` (97 bytes), `harness/` (`conf.lua`, `main.lua`) and the C sources `luajit_version.c` and `png_check.c` used by `make love-check`. Only `*.png` files are regenerated. Assumed: changing `clear.lua`'s clear colour changes `clear.png`'s bytes.
- **H-1-5** — Partly assumed: the trigger is `push: branches: [master, 'claude/**']` plus `pull_request`, so the working branch `claude/friendly-bell-yrer76` is covered. The first run on step 1's pushed commit is still in progress. Its being green is the precondition above.
- **H-1-6** — Assumed, verified by this step: PNG bytes from LÖVE's `encode("png")` depend only on the pixels and the LÖVE build, so the container and CI produce identical files. Two local runs being byte-identical supports it. The extended job on an untouched tree proves it.

## Further Notes

- The phase-end rule of plan 1 applies right after this step: every CI job must be green on the last pushed commit (commit B) before phase 2 starts.
- Record the job's wall time from run B in the step note so later steps can see it grow as capture programs are added. Caching the install is a later decision.
