# 3D Pinball - Space Cadet (Nintendo 64 / libdragon)

N64-focused port of the Space Cadet pinball decompilation, using libdragon runtime and asset pipeline.

## Current scope (v1)
- Space Cadet table only (`PINBALL.DAT`)
- Software renderer blitted to `640x480x16` framebuffer
- Fixed `60 Hz` update loop
- Music via `xm64player` (`PINBALL.xm64`)
- SFX via `wav64` + mixer channels
- EEPROMFS persistence for options + highscores

## Build
This project expects libdragon and uses `n64.mk`.

1. Put assets in `assets/`.
2. Build:

```sh
libdragon make
```

Output ROM:
- `3dpinball-n64.z64`

## Asset pipeline
- `.xm/.XM` -> `.xm64` via `audioconv64`
- `.wav/.WAV` -> `.wav64` via `audioconv64 --wav-compress 1`
- other files from `assets/` are copied into `filesystem/`
- DFS is packaged into ROM (`build/3dpinball-n64.dfs`) and read at runtime via `rom:/`

Required assets checked at build time:
- `assets/PINBALL.DAT`
- `assets/PINBALL.xm` (or `PINBALL.XM`)

## Controls (N64)
- Left flipper: `D-Left` or `L`
- Right flipper: `D-Right` or `R`
- Launch ball: `A` or `D-Down`
- Nudge mode: hold `B` + D-pad direction
- Pause: `Start`
- New game: `Start + A`
- Exit: hold `Start + L + R`

## Save data
- EEPROM save type default: `eeprom4k` (set in Makefile)
- EEPROMFS files:
  - `/pinball/options.bin`
  - `/pinball/highscores.bin`

If EEPROM is unavailable, the game runs without persistence.
