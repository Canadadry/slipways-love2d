# slipways-love

*Slipways* (PICO-8 edition) ported to C, kept here as the reference for a port to LÖVE. The C game builds no playable binary: it runs headless, driven by recorded replays, and its output is what the LÖVE port must match. The plans are in `docs/love-port/`; start with `docs/love-port/design.md`.

![The title screen with a game to resume](docs/img/title-resume.png) ![A planet's build menu](docs/img/build-menu.png) ![A slipway network mid-game](docs/img/network.png)

## Checking the reference

Everything runs headless, from the repository root:

```sh
make test                          # unit tests and static guards
make e2e                           # replays tests/e2e/, checks each capture hash
make test-all                      # both
make replay EVENTS=<events.json>   # replays one file, dump in tmp/replay_dump.txt
make state                         # last-frame state in tmp/state.txt
make coverage                      # line coverage in tmp/coverage/lines.txt
make e2e-order                     # ranks the replays by C lines executed in tests/e2e/order.txt
make e2e-order-check               # re-measures that order, fails when it differs
make asset-export                  # writes game/sprites.png and game/font.png from the C data
make checklist                     # each C function and table with its Lua copy in tmp/checklist.txt
make help                          # every target
```

## Playing the Lua game

`make love` writes `tmp/slipways.love`; run it with LÖVE 11.5 (`love tmp/slipways.love`). It needs a gamepad: the game reads no keyboard.

## Controls

The game reads 12 numbered buttons. `bindings.default.cfg` says what each button does in each mode, and explains every mode and action; replays and tests compile it. The pad table of `docs/love-port/design.md` maps the numbers to LÖVE gamepad buttons.

## To do

Rule for this list: each item is one clear sentence of at most 30 words, readable without any outside reference.

- [ ] Add a cheat that grants bonus score points, so a short e2e can show the score panel's lit stars.
