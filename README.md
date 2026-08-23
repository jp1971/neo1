# Neo1

Neo1 is a modern 65C02 system project with multiple personalities and, now, multiple platform targets. The current hardware target runs on the Olimex Neo6502 platform and can be configured as a Neo1-50 to emulate an Apple-1 50 years after it was first demoed at the Homebrew Computer Club in April 1976 or a Neo1-23 to emulate a Replica 1 23 years after its initial release in 2003.

## Naming Axes

Neo1 now has three distinct naming axes that should stay separate:

- `Neo1` — the overall project and machine family.
- `23` / `50` — the machine personality selected at compile time.
- `pico` / `sdl` — the platform implementation target.

Working rule:

- Personality names should not appear in `systems/` directory names.
- `systems/` directory names should describe platform implementation, not the ROM personality.
- CMake presets should encode both axes explicitly.

Platform targets:

- `systems/neo1-pico/` — Olimex Neo6502 / RP2040 hardware target
- `systems/neo1-sdl/` — macOS/Linux host target using SDL2

## What boots and where

On reset, the machine boots into Woz Monitor.

From WozMon:
- `E000R` → Integer BASIC
- `F000R` → Krusader assembler/editor
- `C100R` → VACI (Virtual Apple-1 Cassette Interface)
- `1810R` → VCFFA1 (Virtual CFFA1)

## Personality selection (compile-time)

The build exposes CMake cache variables for runtime personality/features:

- `NEO1_PERSONALITY` = `23` or `50`
- `NEO1_ENABLE_VACI` = `1` or `0`
- `NEO1_ENABLE_VCFFA1` = `1` or `0`

Policy:

- `NEO1_PERSONALITY=23` requires both `NEO1_ENABLE_VACI=1` and `NEO1_ENABLE_VCFFA1=1`.
- Feature toggling is primarily intended for `NEO1_PERSONALITY=50` experiments.

Defaults are currently set in `systems/neo1-pico/CMakeLists.txt`.

Meaning:

- `NEO1_PERSONALITY=23` (default): top ROM region is `$E000-$FFFF`.
	- `E000R` and `F000R` are available from ROM.
- `NEO1_PERSONALITY=50`: WozMon ROM is placed at `$FF00-$FFFF` and only that page is write-protected.
	- `$E000-$EFFF` is writable so BASIC can be loaded by storage utility and run with `E000R`.

### Switching with CMake (recommended)

Configure once with desired values, then keep using **Compile Project** in VS Code:

```sh
cmake -S . -B build \
	-DNEO1_PERSONALITY=23 \
	-DNEO1_ENABLE_VACI=1 \
	-DNEO1_ENABLE_VCFFA1=1
```

Example for Neo1-50 with VACI on and VCFFA1 off:

```sh
cmake -S . -B build \
	-DNEO1_PERSONALITY=50 \
	-DNEO1_ENABLE_VACI=1 \
	-DNEO1_ENABLE_VCFFA1=0
```

After changing these values, run configure once, then use **Compile Project** as usual.

### Switching with CMake Profiles (preset workflow)

`CMakePresets.json` includes ready-to-use profiles:

- `neo1-pico-23-full`
- `neo1-pico-50-full`
- `neo1-pico-50-vaci-only`
- `neo1-sdl-23-full`
- `neo1-sdl-50-full`
- `neo1-sdl-50-vaci-only`

In VS Code:

1. Run **CMake: Select Configure Preset** and choose one of the profiles above.
2. Run **CMake: Configure** once.
3. Continue using **Compile Project** (Raspberry Pi Pico extension task) as normal.

CLI equivalent example:

```sh
cmake --preset neo1-pico-50-vaci-only
```

## 6502-visible memory map

This is the address space observed by the 65C02, not the RP2040's internal
memory map. Neo1 provides a flat 64 KB backing store. Addresses are ordinary
RAM unless a device intercepts them, a RAM-resident utility is installed at
startup, or the selected personality protects the top ROM region.

### Decoded device addresses

| Address | Owner | Purpose |
| --- | --- | --- |
| `$AFDC-$AFDD` | VCFFA1, when enabled | CFFA1 compatibility signature bytes |
| `$AFF0-$AFFF` | VCFFA1, when enabled | Replica 1-compatible block-device registers |
| `$D010-$D013` | Apple-1 PIA-like interface | Keyboard data/control and display data/control |
| `$D014-$D01C` | Neo1 MSC extension | File and sector command, data, status, index, information, and size registers |
| `$D0F2-$D0F3` | Display aliases | Mirrors of `$D012-$D013` |

These are sparse decoded addresses; the rest of `$D000-$DFFF` remains RAM.
When VCFFA1 is disabled, its signature and register addresses also remain RAM.

### RAM-resident utilities on Neo1 Pico

| Address range | Installed when | Contents and entry point |
| --- | --- | --- |
| `$1800-$2C1E` | VCFFA1 enabled | 5,151-byte M2 block driver; interactive entry at `$1810` |
| `$C100-$CA40` | VACI enabled | 2,369-byte VACI utility; enter from WozMon with `C100R` |

These utilities are copied into ordinary writable RAM during Pico startup;
their ranges are not ROM and are not separate memory-mapped devices. The SDL
target does not currently install either utility.

### Personality-specific top memory

| Personality/target | `$E000-$FEFF` | `$FF00-$FFFF` |
| --- | --- | --- |
| Neo1-23, Pico and SDL | Protected 8 KB system ROM beginning at `$E000`; contains Integer BASIC at `$E000` and Krusader at `$F000` | Final page of the same system ROM, containing WozMon and the hardware vectors |
| Neo1-50, Pico | Writable RAM; three-byte `JMP $FF00` startup stubs are placed at `$E000` and `$F000` until overwritten | Protected 256-byte WozMon ROM and hardware vectors |
| Neo1-50, SDL | Writable seeded RAM; no `$E000` or `$F000` startup stubs | Protected 256-byte WozMon ROM and hardware vectors |

In every profile, `$0000-$00FF` is zero-page RAM and `$0100-$01FF` is stack
RAM. The vectors occupy `$FFFA-$FFFF`; reset enters WozMon at `$FF00`.

## Quickstart

```sh
# Checkout pico-sdk & PicoDVI & tinyusb as git submodules
cd lib
git submodule update --init -- pico-sdk PicoDVI tinyusb

cd ..

# Build
mkdir -p build
cd build
cmake ..
cmake --build .

# Done
find . -type f -name "*.uf2" -ls
```

If your generator is Unix Makefiles, `make` works too after `cmake ..`.

## Hardware reset note (important)

For the 6502 to reset properly, either:
- connect pin 9 of the UEXT connector (GPIO 26) to pin 40 of the 6502 bus connector (RESET), **or**
- set DIP switch 3 to **ON**.

Without one of those, firmware reset control may not fully reset the external 65C02.

## USB storage requirements

The current RP2040 target uses FatFs over TinyUSB MSC. FatFs requires an **MBR-partitioned FAT32** volume.

**Files to place in the root of the volume:**
- Any `.BIN` — loadable programs for VACI
- `CFFA1.PO` (optional) — ProDOS disk image for VCFFA1

## Storage interfaces

### VACI — Virtual Apple-1 Cassette Interface (`C100R`)

Installed at `$C100`. Provides indexed file listing, load, and save modeled on the Apple-1 cassette interface.

- `R` — list files by index, prompt for selection and load address, load binary to RAM
- `W` — prompt for filename, start address, and end address, save memory range to file
- `D` — list files by index, delete selected file (hidden command)

### VCFFA1 — Virtual CFFA1 (`1810R`)

Exposes CFFA1 signature bytes at `$AFDC`/`$AFDD` and a ProDOS block interface at `$AFF0`–`$AFFF`.

- Auto-mounts first `CFFA1.PO`, `CFFA1.HDV`, or `*.po`/`*.hdv`/`*.2mg` image found in root.
- Supports `PRODOS_STATUS` (`$00`) and `PRODOS_READ` (`$01`) commands via the `$AFFF` command register.
- 512-byte block data streams out of `$AFF8` one byte per read.

## Repository layout (high-level)

- `systems/neo1-pico/` — RP2040-side platform target
- `systems/neo1-sdl/` — SDL2 host platform target
- `src/systems/` — core Neo1 runtime/memory model
- `src/roms/` — ROM images/assets
- `src/ram/` — RAM-loaded utility payloads (VACI and VCFFA1 support)
- `lib/` — Pico SDK, TinyUSB, PicoDVI, FatFs

## Planning docs

- `docs/neo1-milestone-plan.md` — overall Neo1 milestone plan (VACI track)
- `docs/vcffa1-v0-baseline.md` — VCFFA1 smoke baseline and naming notes
- `docs/neo1-sdl-emulator-plan.md` — host-target architecture, milestones, and naming plan
