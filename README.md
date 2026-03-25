# Neo1-x

Neo1-x is a modern 65C02 system running on the Olimex Neo6502 platform: It can be configured as a Neo1-50 to emulate an Apple-1 50 years after it was first demoed at the Homebrew Computer Club in April 1976 or a Neo1-23 to emulate a Replica 1 23 years after its initial release in 2003.

## What boots and where

On reset, the machine boots into Woz Monitor.

From WozMon:
- `E000R` → Integer BASIC
- `F000R` → Krusader assembler/editor
- `0400R` → Neo1-23 Filer (simple MSC/USB file loader)
- `1810R` → VCFFA1 (full-featured MSC/USB file utility)

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
- The Neo1-x Filer at `0400R` is an intentionally primitive MSC byte-loader proof of concept.
  - Today it can enumerate files and load by index.
  - It should be thought of as an Apple-1 cassette-style peripheral, not a ProDOS-aware disk interface.
  - Long-term this path may be renamed toward `VACI` (Virtual Apple Cassette Interface) or `NMI` (Neo1 MSC Interface).
- CFFA1 compatibility layer exposes signature bytes at `$AFDC`/`$AFDD` and a read-only ProDOS block interface at `$AFF0`–`$AFFF`.
  - Auto-mounts first `CFFA1.PO`, `CFFA1.HDV`, or `*.po`/`*.hdv`/`*.2mg` image found in root.
  - Supports `PRODOS_STATUS` (`$00`) and `PRODOS_READ` (`$01`) commands via the `$AFFF` command register.
  - 512-byte block data streams out of `$AFF8` one byte per read.
  - Real-image block reads are now hardware-validated against `cp2 rb CFFA1.PO 2`.
- Existing bring-up loader and newer Phase 2 loader images are kept under `src/ram/`.

## Repository layout (high-level)

- `systems/neo1-x/` — machine build target and RP2040-side platform code
- `src/systems/` — core Neo1 runtime/memory model
- `src/roms/` — ROM images/assets
- `src/ram/` — RAM-loaded helper programs (filer/loader images)
- `lib/` — Pico SDK, TinyUSB, PicoDVI, FatFs
