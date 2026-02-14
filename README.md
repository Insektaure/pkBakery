# pkBakery - Donut Editor

A Nintendo Switch homebrew application for editing donuts in Pokemon Legends: Z-A save files.

## Features

### Save File Handling
- Reads and writes Pokemon Legends: Z-A save files using SCBlock encryption (SwishCrypto)
- Account-based profile selector — pick which Switch user's save to edit
- Automatic save backup before any modification (`backups/<profile>/<timestamp>/`)
- In-place file writing (`r+b`) to avoid Switch filesystem journal corruption
- Round-trip verification after load (encrypt(decrypt(file)) == file)
- Donut data accessed via SCBlock key `0xBE007476`

### Donut Editing
Each donut (72 bytes, up to 999 per save) has the following editable fields:

| Field | Range | Description |
|-------|-------|-------------|
| Stars | 0-5 | Star rating |
| Level Boost | 0-255 | Level boost value |
| Calories | 0-9999 | Calorie content |
| Sprite ID | 0-202 | Donut appearance |
| Berry Name | item ID | Primary berry used |
| Berry 1-8 | item ID | 8 berry ingredients (66 valid berries) |
| Flavor 1-3 | FNV-1a hash | Flavor effects (284 valid flavors) |

Berry and flavor fields cycle through all valid values with L/R, or jump by 10 with L1/R1.

Empty slots auto-fill with a shiny template when entering edit mode.

### Batch Operations
- **Fill All: Shiny Power** — Fill all 999 slots with 5-star shiny donuts with randomized Sparkling Power, size effects, and Catch Power flavors
- **Fill All: Random Lv3** — Fill all 999 slots with 3 distinct random level-3 flavors each
- **Clone Selected to All** — Copy the current donut to all 999 slots with unique timestamps
- **Delete Selected Donut** — Clear the current slot
- **Delete ALL Donuts** — Wipe all 999 slots
- **Compress** — Remove gaps by packing non-empty donuts to the front

## Controls

### Profile Selector

| Button | Action |
|--------|--------|
| D-Pad L/R | Navigate profiles |
| A | Select profile |
| - | About |
| + | Quit |

### Donut List

| Button | Action |
|--------|--------|
| D-Pad U/D | Move cursor |
| L1 / R1 | Page up / down |
| A | Edit selected donut |
| X | Delete selected donut |
| Y | Batch operations menu |
| + | Save & exit |
| - | About |
| B | Back to profile selector |

### Edit Mode

| Button | Action |
|--------|--------|
| D-Pad U/D | Select field |
| D-Pad L/R | Adjust value +/-1 |
| L1 / R1 | Adjust value +/-10 |
| A or B | Return to list |

### Batch Menu

| Button | Action |
|--------|--------|
| D-Pad U/D | Select operation |
| A | Execute |
| B | Cancel |

## Donut9a Memory Layout

```
Offset  Size  Field
0x00    8     Timestamp (ms since 1970)
0x08    1     Stars
0x09    1     Level Boost
0x0A    2     Donut Sprite
0x0C    2     Calories
0x0E    2     Berry Name
0x10    16    Berry[8] (8 x uint16)
0x20    8     (reserved)
0x28    8     Flavor 1 (FNV-1a hash)
0x30    8     Flavor 2
0x38    8     Flavor 3
0x40    8     (reserved)
```

Total: 0x48 (72) bytes per donut, 999 max.

## Building

Requires [devkitPro](https://devkitpro.org/) with libnx, SDL2, SDL2_ttf, SDL2_image, and dmntcht.

```
export DEVKITPRO=/opt/devkitpro
make
```

Produces `pkBakery.nro`. Place in `sdmc:/switch/pkBakery/` on your SD card.

## Credits

- [PKHeX](https://github.com/kwsch/PKHeX) by kwsch — PokeCrypto research, SCBlock/SwishCrypto and donut structure reference
- [JKSV](https://github.com/J-D-K/JKSV) by J-D-K — Save backup and write logic reference
- Built with [libnx](https://github.com/switchbrew/libnx) and [SDL2](https://www.libsdl.org/)
