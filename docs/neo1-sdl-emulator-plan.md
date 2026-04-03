# Neo1 Host Emulator Plan

Date: 2026-04-01
Status: Planning
Scope owner: Neo1 solo mainline

## 0) Motivation

The Neo1-pico target runs on RP2040 (Olimex Neo6502). All hardware-specific services
are currently tightly coupled to Pico SDK, PicoDVI, and TinyUSB. This plan
introduces an SDL host target (`neo1-sdl`) that runs Neo1 as a native macOS/Linux
desktop application, replacing RP2040 platform services with SDL2.

Goals:
- Run Neo1-50 (and Neo1-23) in a window on a laptop with no hardware required.
- Accelerate ROM development iteration by eliminating the flash/load cycle.
- Provide a portable reference implementation and learning vehicle.
- Establish a clean platform abstraction boundary in the codebase.

Non-goals (for this phase):
- Cycle-accurate timing.
- Save/restore state snapshots.
- Audio.
- Any change to Neo1-pico (hardware) behavior.

## 0.5) Naming and Build Matrix

Before the host target grows further, keep the naming axes explicit:

| Axis | Values | Meaning |
|---|---|---|
| Project | `Neo1` | The overall machine family |
| Personality | `23`, `50` | Replica 1 style vs. Apple-1 anniversary style runtime profile |
| Platform | `pico`, `sdl` | RP2040/Olimex hardware vs. desktop host implementation |

Recommended end-state naming:

- `systems/neo1-pico/` — RP2040 / Olimex Neo6502 implementation
- `systems/neo1-sdl/` — macOS/Linux SDL2 implementation

Reasoning:

- Personality is a compile-time profile and should remain in presets/cache variables, not directory names.

Recommended preset matrix:

- `neo1-pico-23-full`
- `neo1-pico-50-full`
- `neo1-pico-50-vaci-only`
- `neo1-sdl-23-full`
- `neo1-sdl-50-full`
- `neo1-sdl-50-vaci-only`

Recommended binary directories:

- `build/` for Pico tasks compatibility
- `build-sdl/` for host SDL builds

---

## 1) Architecture Overview

### 1.1 Shared portble core (unchanged)

These files have zero platform dependencies and are shared as-is between both targets:

| File | Role |
|---|---|
| `src/chips/wdc65C02cpu.h` | Header-only 65C02 core (cycles, bus callbacks) |
| `src/chips/mem.h`, `chips_common.h` | Memory helpers |
| `src/systems/neo1.h` / `neo1.c` | Bus dispatch, CPU tick, snapshot |
| `systems/neo1-pico/src/neo1_terminal.{h,c}` | Software terminal state machine |
| `src/ram/*.h` | ROM/RAM images (VACI, VCFFA1) |
| `src/roms/*.h` | ROM images (Integer Basic, Wozmon, Krusader) |

### 1.2 Platform services (to be abstracted)

These are the RP2040-specific responsibilities that need host counterparts:

| Service | Neo1-pico (RP2040) | neo1-sdl (SDL2) |
|---|---|---|
| Display output | PicoDVI, core1 render loop | SDL2 window + texture blit |
| Keyboard input | TinyUSB HID host callbacks | SDL2 keyboard events |
| Mass storage | TinyUSB MSC + FatFs | File-backed `.img` or host filesystem |
| Timing | `pico/stdlib.h` timers | `SDL_GetTicks64` / `clock_gettime` |
| Threading | `pico_multicore` | SDL render on main thread, tick loop inline |

### 1.3 Platform HAL

A thin header `systems/neo1-sdl/neo1_platform.h` will declare five functions:

```c
// Display
void     neo1_platform_init(int width, int height, const char* title);
void     neo1_platform_update_display(const uint32_t* pixels, int w, int h);

// Input
bool     neo1_platform_poll_key(uint8_t* out_apple1_keycode, bool* out_pressed);

// Time
uint64_t neo1_platform_time_us(void);

// Block storage (for VCFFA1 / VACI disk image)
bool     neo1_platform_disk_read(uint32_t lba, uint8_t* buf, uint32_t count);
bool     neo1_platform_disk_write(uint32_t lba, const uint8_t* buf, uint32_t count);
```

Two concrete implementations:
- `neo1_platform_pico.c` — thin wrappers over PicoDVI, TinyUSB, FatFs (current system, retroactively extracted)
- `neo1_platform_sdl.c` — SDL2 implementation for host target

---

## 2) Directory Layout

```
systems/
  neo1-pico/       ← existing hardware target
  neo1-sdl/        ← host target
    CMakeLists.txt
    src/
      main.c                  ← host entry point + SDL event loop
      neo1_platform.h         ← HAL interface (shared declaration)
      neo1_platform_sdl.c     ← SDL2 implementation
```

The `neo1-pico/` side can optionally gain `neo1_platform_pico.c` later to
formalize the abstraction, but that refactor is deferred — no changes to the
hardware target during this phase.

---

## 3) Display Model

Neo1's terminal state machine (`neo1_terminal`) produces a character grid.
`neo1_video` currently rasterizes that into a PicoDVI-compatible framebuffer.

For the host target:
- Rasterize the character grid into a `uint32_t` ARGB pixel buffer (same
  logical dimensions: 560×192 for 40-column, or 280×192 for 2x scale).
- Blit to an `SDL_Texture` each frame via `SDL_UpdateTexture` + `SDL_RenderCopy`.
- Font: embed the same Apple 1 character ROM bitmap already in
  `src/roms/neo1_apple1_video_rom_image.h`.

---

## 4) Input Model

SDL2 keyboard → Apple 1 ASCII translation:
- Printable ASCII: pass through directly with bit 7 set (Apple 1 keyboard convention).
- Return → `$8D`.
- Backspace/Delete → `$DF` (Apple 1 rub-out).
- Escape → `$9B`.
- No modifier handling required for v1.

---

## 5) Disk Image Model

VCFFA1/VACI storage on the host:
- Mount a flat `.img` file as a block device (512-byte sectors, same as the
  FatFs/USB MSC path).
- Pass path via CLI argument: `--disk neo1.img`.
- If absent, disable VCFFA1/VACI at runtime (equivalent to no USB drive).

---

## 6) Build Integration

### 6.1 CMake host preset

Add to `CMakePresets.json`:
```json
{
  "name": "neo1-sdl-50-full",
  "displayName": "Neo1 SDL 50 (VACI on, VCFFA1 on)",
  "generator": "Ninja",
  "binaryDir": "${sourceDir}/build-sdl",
  "cacheVariables": {
    "NEO1_PLATFORM": "sdl",
    "NEO1_PERSONALITY": "50",
    "NEO1_ENABLE_VACI": "1",
    "NEO1_ENABLE_VCFFA1": "1"
  }
}
```

### 6.2 SDL2 dependency

SDL2 is found via CMake's `find_package(SDL2 REQUIRED)`. On macOS:
```bash
brew install sdl2
```

### 6.3 Build and run
```bash
cmake --preset neo1-sdl-50-full
cmake --build build-sdl
./build-sdl/systems/neo1-sdl/neo1 --disk neo1.img
```

---

## 7) Milestones

### M0 — HAL boundary definition (1 session)
Objective: define `neo1_platform.h` and stub SDL implementation.

Deliverables:
- `systems/neo1-sdl/neo1_platform.h` with all six HAL signatures.
- `systems/neo1-sdl/src/neo1_platform_sdl.c` with stub bodies (no-ops).
- `systems/neo1-sdl/CMakeLists.txt` compiling against SDL2.
- Host binary links and launches (blank window, CPU not yet running).

Exit criteria: `cmake --preset neo1-sdl-50-full && cmake --build build-sdl` succeeds.

### M1 — CPU + terminal loop (1 session)
Objective: CPU running, terminal state updating, nothing displayed yet.

Deliverables:
- Host `main.c` event loop calling `neo1_tick()` at fixed rate.
- Terminal state machine advancing correctly (validate via debug print of char grid).

Exit criteria: Wozmon prompt appears in terminal debug output after reset.

### M2 — Display (1 session)
Objective: character grid rendering in SDL window.

Deliverables:
- `neo1_platform_update_display()` blitting Apple 1 font from character ROM.
- Correct 40-column layout, video inverse attribute working.

Exit criteria: Wozmon `\` prompt visible in SDL window.

### M3 — Keyboard (1 session)
Objective: typed characters reach the 6502.

Deliverables:
- SDL keyboard events translated to Apple 1 ASCII and routed through the
  existing `neo1_terminal` / `neo1_usb` keyboard injection path.

Exit criteria: can type and execute a Wozmon command (`$FF00.FF0FR`).

### M4 — Integer Basic + Krusader smoke test (1 session)
Objective: full Neo1-50 ROM stack interactive in the window.

Deliverables:
- `E000R` loads and runs Integer Basic.
- `F000R` loads and runs Krusader.

Exit criteria: `PRINT "HELLO"` executes and displays.

### M5 — Disk image (1 session, Neo1-23 only)
Objective: VCFFA1 and VACI functional with a `.img` file.

Deliverables:
- `--disk` CLI argument wired to `neo1_platform_disk_read/write`.
- FatFs or equivalent used to mount and traverse the image.

Exit criteria: `1810R` disk catalog lists files from the mounted image.

---

## 8) Dependencies and Risk

| Item | Risk | Mitigation |
|---|---|---|
| SDL2 font rasterization performance | Low — 560×192 is tiny | Simple pixelblit, no scaling library needed |
| FatFs on host without RP2040 HAL | Medium — FatFs expects low-level disk I/O functions | Provide `diskio.c` backed by `fread`/`fwrite` on the `.img` file |
| Neo1-pico regression during HAL extraction | Low for M0–M4 (no changes to neo1-pico) | HAL extraction to neo1-pico is deferred |
| SDL2 availability on trip machine | None | Pre-install via `brew install sdl2` before departure |

---

## 9) Reference Material

- [CHIPS project](https://github.com/floooh/chips) — Andre Weissflog's portable 6502/Z80 cores with sokol_app platform layer; direct ancestor of `wdc65C02cpu.h` style.
- [CHIPS Apple 1 demo](https://github.com/floooh/chips-test) — reference for Apple 1 PIA + Wozmon + BASIC on a CHIPS+sokol host target.
- SDL2 docs: `SDL_CreateTexture`, `SDL_UpdateTexture`, `SDL_RenderCopy`.
- `src/roms/neo1_apple1_video_rom_image.h` — character ROM already in-tree.
