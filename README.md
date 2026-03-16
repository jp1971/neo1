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

## Current storage/filer status

- USB MSC (mass storage) is integrated through TinyUSB + FatFs.
- The Neo1-23 Filer at `0400R` can enumerate files and load by index.
- Existing bring-up loader and newer Phase 2 loader images are kept under `src/ram/`.

## Repository layout (high-level)

- `systems/neo1-23/` — machine build target and RP2040-side platform code
- `src/systems/` — core Neo1 runtime/memory model
- `src/roms/` — ROM images/assets
- `src/ram/` — RAM-loaded helper programs (filer/loader images)
- `lib/` — Pico SDK, TinyUSB, PicoDVI, FatFs

## Next planned work

- Phase 3 (optional): directory navigation in filer
- Phase 4: save/write flow from 6502-side tools
- Longer-term: richer RAM-loaded tools (e.g. TaliForth 2, Applesoft Lite workflows)
