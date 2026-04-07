# 3D Pinball - Space Cadet (Nintendo 64 / libdragon)

N64 port of the Space Cadet pinball decompilation, using libdragon runtime and asset pipeline.
Made from the Dreamcast version as a base. Plays Space Cadet only currently.

The Expansion Pak is not required.

<img width="632" height="504" alt="image" src="https://github.com/user-attachments/assets/1e32b537-c395-4db2-bc46-6b1b9037cccf" />


## Features
1. EEPROM support
2. Rumble Pak support

## Build
This project requires libdragon to be installed.

1. Put assets from your space-cadet game (PINBALL.DAT, PINBALL2.MID, .WAV files) in `assets/`.
2. Build:

```sh
libdragon make
```

## Controls
- Left flipper: `L`
- Right flipper: `R`
- Launch ball: `A`
- Nudge mode: hold D-pad direction
- Pause: `Start`
- New game: `B`
- Exit: hold `Start + B`

## Save data
- EEPROM save type default: `eeprom4k` (set in Makefile)

If EEPROM is unavailable, the game runs without persistence.
