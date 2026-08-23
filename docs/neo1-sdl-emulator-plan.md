# Neo1 Host Emulator Plan

> Historical experiment plan: implementation has diverged from parts of this
> document. Treat acceptance statements as planned behavior unless confirmed in
> `docs/current-state.md` or current code.

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

## 5) Host Storage Model

The SDL target should replace the Pico-only TinyUSB/FatFs service for VACI,
while keeping the 6502-visible MSC register protocol unchanged. VCFFA1 is
intentionally out of the SDL scope: its emulation will continue on the
Neo6502, where the hardware behavior can be validated against the real device.

The current SDL M1 bridge only exposes raw sector reads/writes and accepts VACI
directory commands as no-ops; that is enough for a smoke window but cannot make
VACI enumerate or open files. Replace it with a VACI-only host directory
backend.

Use one explicit host storage surface:

### 5.1 VACI file store

VACI is a file-oriented protocol. Its SDL backend should use a host directory,
not a FAT volume embedded inside a disk image:

- Default directory: `neo1_sdl_files/` relative to the working directory.
- Override with `NEO1_SDL_FILES`, accepting an absolute or relative path.
- Enumerate regular files only, skip dot files and directories, and return a
  stable sorted order so `R`/`D` indexes do not change between runs.
- Implement open/create, close, 512-byte reads, 512-byte writes, directory
  enumeration, indexed open, and indexed delete.
- Keep the existing filename and size limits defined by `neo1_msc.h`.
- Treat a missing directory as an empty drive; create it during initialization
  when possible and report a protocol error when it is not writable.

### 5.2 Removable-drive policy

Do not make SDL enumerate USB or SD devices. On macOS, an attached thumb or SD
card is already exposed as a mounted directory, and VACI only needs file
operations. `NEO1_SDL_FILES` should accept any user-selected directory,
including a path such as `/Volumes/NEO1`, while keeping a project-local default
for development and automated tests.

This gives us three useful modes without adding device-specific code:

- Project-local fixture directory for repeatable tests.
- A directory on a mounted thumb/SD drive for testing removable media.
- A read-only mounted directory for validating VACI error handling.

The backend should log the canonical path at startup, reject paths that are not
directories, and surface permission or read-only failures through the existing
MSC status register. It should not recursively search, format, partition, or
mount media. Those are hardware or operating-system responsibilities.

### 5.3 Storage backend boundary

Move the storage operations currently embedded in the Pico `neo1_msc.c` and
`neo1_cffa1.c` implementations behind a small backend selected by the platform:

```c
typedef struct {
    bool (*init)(void);
    void (*shutdown)(void);
    bool (*file_dir_open)(void);
    bool (*file_dir_next)(char* name, size_t name_size, uint32_t* size);
    bool (*file_open)(const char* name, bool create);
    bool (*file_close)(void);
    bool (*file_delete)(const char* name);
    bool (*file_read_sector)(uint32_t sector, uint8_t* buf);
    bool (*file_write_sector)(uint32_t sector, const uint8_t* buf, uint16_t size);
} neo1_storage_backend_t;
```

The exact names can follow the existing style, but the ownership rule matters:
the register bridges own protocol state and status codes; the selected backend
owns paths, handles, enumeration, and host I/O. The Pico backend remains backed
by FatFs/TinyUSB, while SDL supplies the directory implementation. The SDL
target should stop compiling `neo1_storage_stub.c` once this boundary is in
place and should compile with `NEO1_ENABLE_VCFFA1=0`.

---

## 6) Build Integration

### 6.1 CMake host preset

Add to `CMakePresets.json`:
```json
{
  "name": "neo1-sdl-50-full",
  "displayName": "Neo1 SDL 50 (VACI only)",
  "generator": "Ninja",
  "binaryDir": "${sourceDir}/build-sdl",
  "cacheVariables": {
    "NEO1_PLATFORM": "sdl",
    "NEO1_PERSONALITY": "50",
    "NEO1_ENABLE_VACI": "1",
    "NEO1_ENABLE_VCFFA1": "0"
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
./build-sdl/systems/neo1-sdl/neo1
```

Storage configuration for the current SDL implementation is environment based:

```bash
NEO1_SDL_FILES=neo1_sdl_files \
./build-sdl/systems/neo1-sdl/neo1
```

The SDL VACI backend reads `NEO1_SDL_FILES`; `main.c` does not parse command-
line storage arguments.

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

### M5 — VACI backend boundary (1 session)
Objective: make the VACI protocol platform-neutral without changing its
6502-visible register map, while explicitly excluding VCFFA1 from SDL.

Deliverables:
- Extract the common MSC protocol state machine from the Pico-only FatFs
  implementation.
- Define the backend operations needed by VACI files and directories.
- Keep the existing Pico backend behavior unchanged.
- Replace the SDL M1 storage stub with an SDL VACI backend registration.
- Set `NEO1_ENABLE_VCFFA1=0` for SDL builds and remove SDL CFFA1 plumbing.

Exit criteria: SDL builds without linking Pico FatFs/TinyUSB storage code, and
the SDL binary has no VCFFA1 behavior enabled.

### M6 — SDL VACI host directory (1-2 sessions)
Objective: make VACI useful against ordinary host files.

Deliverables:
- Implement sorted directory enumeration and indexed file selection.
- Implement VACI open/read/write/close/delete against `NEO1_SDL_FILES`, whether
  it points to a project directory or a mounted removable drive.
- Add clear startup logging for the selected directory and per-command errors.
- Add a small host-side backend test using temporary files, including an empty
  directory, a file larger than 512 bytes, delete, and a failed write.

Exit criteria: from WozMon, `C100R` lists files, `R` loads a known binary, `W`
saves a memory range, and `D` deletes the selected file.

### M7 — SDL VACI regression (1 session)
Objective: protect both platform backends while storage code is refactored.

Deliverables:
- Add scripted SDL smoke coverage for VACI list/load/save/delete using a
  temporary project-local directory.
- Repeat the smoke test with `NEO1_SDL_FILES` pointed at a mounted removable
  volume when one is available.
- Run the same VACI protocol-level checks against the Pico backend where
  practical.
- Document the directory environment variable and working-directory behavior
  in `README.md`.

Exit criteria: a clean SDL build followed by the VACI smoke script passes
without manual file copying or TinyUSB hardware.

---

## 8) Dependencies and Risk

| Item | Risk | Mitigation |
|---|---|---|
| SDL2 font rasterization performance | Low — 560×192 is tiny | Simple pixelblit, no scaling library needed |
| Shared MSC/CFFA1 code is coupled to FatFs | High — SDL cannot reuse the Pico implementation directly | Separate protocol bridges from a platform-selected storage backend |
| VACI indexes change between runs | Medium — host directory order is not stable | Filter files and sort names before enumeration |
| Removable volume permissions/ejection | Medium — host paths can become unavailable | Validate the directory at startup and return protocol errors for failed operations |
| Neo1-pico regression during backend extraction | Medium | Keep the Pico backend and run the existing hardware build after each storage milestone |
| SDL2 availability on a new machine | Low | Install with `brew install sdl2` before building |

---

## 9) Reference Material

- [CHIPS project](https://github.com/floooh/chips) — Andre Weissflog's portable 6502/Z80 cores with sokol_app platform layer; direct ancestor of `wdc65C02cpu.h` style.
- [CHIPS Apple 1 demo](https://github.com/floooh/chips-test) — reference for Apple 1 PIA + Wozmon + BASIC on a CHIPS+sokol host target.
- SDL2 docs: `SDL_CreateTexture`, `SDL_UpdateTexture`, `SDL_RenderCopy`.
- `src/roms/neo1_apple1_video_rom_image.h` — character ROM already in-tree.
