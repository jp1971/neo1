# Neo1-x

Neo1-x is a modern 65C02 system running on the Olimex Neo6502 platform: It can be configured as a Neo1-50 to emulate an Apple-1 50 years after it was first demoed at the Homebrew Computer Club in April 1976 or a Neo1-23 to emulate a Replica 1 23 years after its initial release in 2003.

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

Defaults are set in `systems/neo1-x/CMakeLists.txt`.

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

- `neo1-23-full`
- `neo1-50-full`
- `neo1-50-vaci-only`
- `neo1-50-vcffa1-only`

In VS Code:

1. Run **CMake: Select Configure Preset** and choose one of the profiles above.
2. Run **CMake: Configure** once.
3. Continue using **Compile Project** (Raspberry Pi Pico extension task) as normal.

CLI equivalent example:

```sh
cmake --preset neo1-50-vaci-only
```

## Neo1-x memory map (current)

```
+-----------------------+
| FFFF  IRQ vector      |
| FFFE                  |
| FFFD  RESET vector    |
| FFFC                  |
| FFFB  NMI vector      |
| FFFA                  |
|                       |
| FF00  Woz Monitor     |
|                       |
| F000  Krusader        |
|                       |
| E000  Integer BASIC   |
+-----------------------+
| DFFF                  |
| D000  I/O space       |
|       keyboard        |
|       display         |
|       MSC registers   |
+-----------------------+
| CFFF                  |
|                       |
| C100  VACI (RAM)      |
|                       |
| 0200  program RAM     |
|                       |
+-----------------------+
| 01FF  stack           |
| 0100                  |
+-----------------------+
| 00FF  zero page       |
| 0000                  |
+-----------------------+
```

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

Neo1-x uses FatFs over TinyUSB MSC. FatFs requires an **MBR-partitioned FAT32** volume.

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

- `systems/neo1-x/` — machine build target and RP2040-side platform code
- `src/systems/` — core Neo1 runtime/memory model
- `src/roms/` — ROM images/assets
- `src/ram/` — RAM-loaded utility payloads (VACI and VCFFA1 support)
- `lib/` — Pico SDK, TinyUSB, PicoDVI, FatFs

## Planning docs

- `docs/neo1-milestone-plan.md` — overall Neo1 milestone plan (VACI track)
- `docs/vcffa1-v0-baseline.md` — VCFFA1 smoke baseline and naming notes
