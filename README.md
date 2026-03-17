# Neo1-23

Neo1-23 is a modern 65C02 system running on the Olimex Neo6502 platform: a Replica 1 / Apple-1-inspired machine, rebuilt on RP2040-era hardware about 23 years later.

The design goal is simple and educational:
- keep the 6502 memory/bus model visible,
- keep software monitor-first,
- and evolve from Apple-1-compatible bring-up into a distinct Neo1 machine.

## What boots and where

On reset, the machine boots into Woz Monitor.

From WozMon:
- `E000R` → Integer BASIC
- `F000R` → Krusader assembler/editor
- `0400R` → Neo1-23 Filer (MSC/USB file loader)

## Neo1-23 memory map (current)

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
|                       |
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

Adapted from Reload:

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

Neo1-23 uses FatFs over TinyUSB MSC. FatFs requires an **MBR-partitioned FAT32** volume.

**macOS formatting (required procedure):**

```sh
# Find your USB drive identifier first:
diskutil list

# Format with MBR (replace diskN with your actual disk number):
diskutil eraseDisk FAT32 NEO1 MBR /dev/diskN
```

Do **not** use Disk Utility's GUI — it defaults to GUID Partition Map (GPT), which FatFs does not support and produces `FatFs mount failed: 13` (`FR_NO_FILESYSTEM`).

**Files to place in the root of the volume:**
- `HELLORLD.BIN` or any `.BIN` — loadable programs for the filer
- `CFFA1.PO` (optional) — ProDOS disk image for CFFA1 compatibility layer

## Current storage/filer status

- USB MSC (mass storage) is integrated through TinyUSB + FatFs.
- The Neo1-23 Filer at `0400R` can enumerate files and load by index.
- CFFA1 compatibility layer exposes signature bytes at `$AFDC`/`$AFDD` and a read-only ProDOS block interface at `$AFF0`–`$AFFF`.
  - Auto-mounts first `CFFA1.PO`, `CFFA1.HDV`, or `*.po`/`*.hdv`/`*.2mg` image found in root.
  - Supports `PRODOS_STATUS` (`$00`) and `PRODOS_READ` (`$01`) commands via the `$AFFF` command register.
  - 512-byte block data streams out of `$AFF8` one byte per read.
- Existing bring-up loader and newer Phase 2 loader images are kept under `src/ram/`.

## Repository layout (high-level)

- `systems/neo1-23/` — machine build target and RP2040-side platform code
- `src/systems/` — core Neo1 runtime/memory model
- `src/roms/` — ROM images/assets
- `src/ram/` — RAM-loaded helper programs (filer/loader images)
- `lib/` — Pico SDK, TinyUSB, PicoDVI, FatFs

## Next planned work

- M2: CFFA1 boot/software path (exercise a real CFFA1-side ProDOS workflow end-to-end)
- M3: optional CFFA1 write path
- Phase 3 (optional): directory navigation in filer
- Phase 4: save/write flow from 6502-side tools
- Longer-term: richer RAM-loaded tools (e.g. TaliForth 2, Applesoft Lite workflows)
