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

## Monitor entry points

On reset, the machine boots into Woz Monitor.

From WozMon on Neo1 Pico:

| Command | Availability | Result |
| --- | --- | --- |
| `FF00R` | Every profile | Enter WozMon |
| `E000R` | Neo1-23, or after loading code on Neo1-50 | Enter Integer BASIC or the program loaded at `$E000` |
| `F000R` | Neo1-23, or after loading code on Neo1-50 | Enter Krusader or the program loaded at `$F000` |
| `C100R` | VACI enabled | Enter VACI |
| `1810R` | VCFFA1 enabled | Enter the VCFFA1 utility |

On Neo1-50 Pico, `$E000` and `$F000` initially contain return-to-WozMon stubs
until a storage utility overwrites them. SDL does not currently install the
VACI or VCFFA1 RAM utilities.

## Neo1 Pico build profiles

The Pico build exposes these CMake cache variables:

- `NEO1_PERSONALITY` = `23` or `50`
- `NEO1_ENABLE_VACI` = `1` or `0`
- `NEO1_ENABLE_MSC` = `1` or `0`
- `NEO1_ENABLE_VCFFA1` = `1` or `0`
- `NEO1_DIAGNOSTICS` = `1` for verbose host-side serial diagnostics, otherwise `0`

VACI requires the Neo1 MSC register device. Neo1-23 requires VACI, MSC, and
VCFFA1. The supplied Pico configure presets select the supported combinations:

- `neo1-pico-23-full` selects Neo1-23 with VACI and VCFFA1 enabled.
- `neo1-pico-50-full` selects Neo1-50 with VACI and VCFFA1 enabled.
- `neo1-pico-50-vaci-only` selects Neo1-50 with VACI enabled and VCFFA1 disabled.

For bring-up, `neo1-pico-23-diagnostics` and
`neo1-pico-50-diagnostics` select the corresponding full profile with
`NEO1_DIAGNOSTICS=1`.

Neo1-23 places the protected system ROM at `$E000-$FFFF`, so `E000R` and
`F000R` enter Integer BASIC and Krusader. Neo1-50 protects only WozMon at
`$FF00-$FFFF`; `$E000-$FEFF` remains writable for programs loaded from
storage.

## Building Neo1 Pico in VS Code

The supported hardware-build path uses the Raspberry Pi Pico VS Code extension
and the Microsoft CMake Tools extension. The currently verified toolchain is:

- Raspberry Pi Pico SDK 2.1.0, managed by the extension
- Arm GNU Toolchain 13.3.Rel1, managed by the extension
- Ninja and CMake supplied by the extension

SDK 2.3.0 has not yet been validated. Treat changing SDK versions as a separate
build change: update the extension-managed project SDK, perform a clean
configure and build of both Pico personalities, and repeat the Neo6502 smoke
test before making it the documented baseline.

### Prepare the checkout

Install the Raspberry Pi Pico extension, allow it to install the verified SDK
and toolchain, and initialize the three project submodules from the repository
root:

```sh
git submodule update --init -- lib/pico-sdk lib/PicoDVI lib/tinyusb
```

The build uses the extension-managed official SDK. The checked-in Pico SDK fork
is still required because it supplies Neo1's project-specific
`olimex_neo6502.h` board definition. PicoDVI and TinyUSB are linked from the
checked-in submodules.

### Understand the two CMake controls

The similarly named setting and commands belong to two different extensions:

- **Raspberry Pi Pico: Use CMake Tools** unchecked means the Raspberry Pi Pico
  extension performs configuration. Its **Clean CMake** and **Configure CMake**
  controls work, but they invoke plain `cmake -B build` and do not select a
  Neo1 CMake preset.
- **Raspberry Pi Pico: Use CMake Tools** checked means the separate Microsoft
  CMake Tools extension performs configuration. Neo1 presets work directly,
  but the Pico extension deliberately disables its own **Clean CMake** and
  **Configure CMake** controls.

Selecting a preset in CMake Tools does not make the Pico extension's configure
command preset-aware. A Pico **Clean CMake** also deletes `build/` and
immediately configures it with the CMake defaults: Neo1-50 with diagnostics
off.

### Keep the Pico Compile and Flash controls

To retain the Pico extension's controls while selecting a Neo1 profile, leave
**Raspberry Pi Pico: Use CMake Tools** unchecked and use this sequence:

1. Open the repository root in VS Code.
2. If a clean build is needed, use Pico **Clean CMake** first. Its temporary
   default Neo1-50 configuration will be replaced in the next steps.
3. Run **CMake: Select Configure Preset** from the Command Palette and choose
   the required normal or diagnostic `neo1-pico-*` preset.
4. Run **CMake: Configure** from the Command Palette. This is the Microsoft
   CMake Tools command, not Pico **Configure CMake**.
5. Confirm the configure output reports the intended values, for example:

   ```text
   Neo1 Pico profile: personality=23, vaci=1, msc=1, vcffa1=1, diagnostics=1
   ```

6. Use Pico **Compile Project**, then Pico **Flash**.

Both Pico personalities use the `build/` directory. Repeat the preset selection
and **CMake: Configure** after every Pico **Clean CMake** and whenever changing
profiles. Pico **Configure CMake** may reconfigure an existing cache, but it
cannot choose or recover a preset after the cache has been deleted.

Alternatively, check **Raspberry Pi Pico: Use CMake Tools** and use CMake Tools
for configure, build, and clean operations. The Pico extension may still be
used for flash, run, and debug, but its own Clean/Configure controls will be
unavailable in this mode.

The resulting hardware artifacts are written under
`build/systems/neo1-pico/`. The UF2 to flash is:

```text
build/systems/neo1-pico/neo1.uf2
```

Before flashing after a profile switch, check the `Neo1 Pico profile` configure
line for the expected personality, device flags, and diagnostics value.

### Serial diagnostics

Normal profiles set `NEO1_DIAGNOSTICS=0`. They retain 6502 terminal output on
UART and print only a concise Neo1 readiness line, keyboard/storage mount state,
and mount failures. They omit startup memory and vector dumps, the 64-event bus
trace, USB descriptors, root-directory listings, and per-operation storage
messages.

Select `neo1-pico-23-diagnostics` or `neo1-pico-50-diagnostics` when that
bring-up evidence is needed. Diagnostic builds also enable Ctrl-D terminal
buffer dumps on the UART input path. The flag changes host logging only; it
does not add `printf` calls to the live bus service path.

### Equivalent preset commands

These commands use the same presets as CMake Tools and are useful for checking
the selected workflow from the extension's configured terminal:

```sh
cmake --preset neo1-pico-23-full
cmake --build --preset build-neo1-pico-23-full
```

To clean that profile:

```sh
cmake --build --preset build-neo1-pico-23-full --target clean
```

The SDL target remains a development and behavioral-test target, but it is not
part of this hardware quickstart.

### Host storage tests

The SDL configure enables storage-focused CTest targets that run without real
media:

```sh
cmake --preset neo1-sdl-23-full
cmake --build --preset build-neo1-sdl-23-full
ctest --test-dir build-sdl --output-on-failure
```

`neo1_msc_register_contract` runs the shared MSC register protocol with the
production Pico FatFs backend against a test-only in-memory filesystem. It
covers directory filtering and indexed open, short-read padding, missing and
read-only media, invalid commands and seeks, short writes, delete, multi-sector
write, exact truncating overwrite, DATA bounds, and independent protocol
instances. `neo1_sdl_storage_backends` verifies SDL raw-sector MSC behavior,
shared command/error sequencing, and separation from its VCFFA1 compatibility
state. `neo1_vaci_payload_contract` executes the generated VACI payload on the
software 65C02. It verifies all 2,230 BASIC snapshot bytes plus ordinary
multi-sector read/write, close behavior, transfer-range rejection, profile ROM
boundaries, reserved status rejection, and bounded busy polling. These tests
do not establish SDL file-service equivalence or physical USB behavior.

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
| `$D014-$D01C` | Neo1 MSC extension, when enabled | File and sector command, data, status, index, information, and size registers |
| `$D0F2-$D0F3` | Display aliases | Mirrors of `$D012-$D013` |

These are sparse decoded addresses; the rest of `$D000-$DFFF` remains RAM.
When MSC or VCFFA1 is disabled, its decoded addresses remain ordinary RAM.

### RAM-resident utilities on Neo1 Pico

| Address range | Installed when | Contents and entry point |
| --- | --- | --- |
| `$1800-$2C1E` | VCFFA1 enabled | 5,151-byte M2 block driver; interactive entry at `$1810` |
| `$C100-$CB17` | VACI enabled | 2,584-byte VACI utility in a reserved `$C100-$CFFF` region; enter from WozMon with `C100R` |

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

## Hardware reset note (important)

For the 6502 to reset properly, either:
- connect pin 9 of the UEXT connector (GPIO 26) to pin 40 of the 6502 bus connector (RESET), **or**
- set DIP switch 3 to **ON**.

Without one of those, firmware reset control may not fully reset the external 65C02.

## USB storage

Neo1 Pico uses FatFs over TinyUSB host MSC. The verified media configuration is
an MBR-partitioned FAT32 volume; other layouts and filesystems have not yet been
included in the hardware smoke test.

Place VACI files and any VCFFA1 disk image in the volume root.

## Storage interfaces

### VACI — Virtual Apple-1 Cassette Interface (`C100R`)

Installed at `$C100-$CB17` on Neo1 Pico. The visible prompt is
`R/W/L/S/Q?:`.

| Command | Behavior |
| --- | --- |
| `R` | List files by index, select one, load it at a safe requested address, and close it |
| `W` | Save a safe inclusive RAM address range to a named file; multi-sector writes and truncation are hardware-verified |
| `L` | Load a packed Integer BASIC workspace by file index; the corrected 2,230-byte restore passes emulated and Neo6502 round-trip tests |
| `S` | Save the Integer BASIC zero-page and `$0800-$0FFF` workspace; `$F0-$FC` is preserved before VACI uses it as scratch |
| `Q` | Return to WozMon |

Ordinary `R` and `W` ranges must fit without 16-bit wrap and must not cross a
reserved range. Safe ranges are `$0300-$AEFF`, `$B000-$C0FF`, and
`$D100` through the byte below the selected profile's protected ROM. That last
limit is `$DFFF` on Neo1-23 and `$FEFF` on Neo1-50. This preserves Neo1-50
loads at `$E000` and `$F000` while rejecting them on Neo1-23. Invalid ordinary
ranges print `READ ERR` or `WRITE ERR` on a new line without transferring data.
The specialized `L` and `S` commands retain their documented BASIC snapshot
ranges.

If USB storage is unavailable, the current Pico VACI image may return silently
to the menu after the specialized BASIC `L` command instead of printing a load
error. Live USB-storage reinsertion has not been verified; power-cycle the
Neo6502 with the medium already inserted to restore the known-working storage
path.

`D` is an intentionally hidden destructive command that lists files and
deletes one by index. The corrected `L`/`S` format is compatible with existing
2,230-byte files, but bytes already corrupted by the older saver cannot be
recovered. See [Current state](docs/current-state.md) for the dated
hardware-validation evidence.

### VCFFA1 — Virtual CFFA1 (`1810R`)

Exposes CFFA1 signature bytes at `$AFDC`/`$AFDD` and a ProDOS block interface
at `$AFF0-$AFFF`. VCFFA1 is an optional Replica 1 compatibility feature; VACI
is the preferred Apple-1-oriented storage interface. The current VCFFA1
implementation is retained for compatibility while its reliability work is
deferred until after the next portable-core checkpoint.

- Prefers writable `CFFA1RW.PO` or `CFFA1RW.HDV` images.
- Falls back to read-only `CFFA1.PO` or `CFFA1.HDV`, then the first recognized
  read-only disk image.
- Supports `PRODOS_STATUS` (`$00`), `PRODOS_READ` (`$01`), and
  `PRODOS_WRITE` (`$02`) through `$AFFF`.
- Streams one 512-byte block through `$AFF8` for reads and writes.

The interactive utility supports `C` catalog, `L` load, `B` block inspect, `W`
write/create/overwrite, `D` delete, and `Q` quit. Its catalog and allocation
logic is deliberately limited to root directory block 2, the first bitmap
block, and seedling or two-block sapling files no larger than 1024 bytes.

The catalog/load/block-inspection workflow and loading a `.po` image have been
verified on Neo6502 hardware. The following limitations remain documented and
deferred:

- Generic lowercase or alternate `.po` fallback matching is faulty; use one of
  the preferred image names above.
- Create and delete are not transactional and can leave the ProDOS bitmap and
  directory inconsistent after an I/O failure.
- Overwriting an existing file does not update its EOF or related catalog
  metadata when the requested length changes.
- Block-driver DRQ waits have no timeout, and utility load destinations are not
  range checked.

Treat `W` and `D` as experimental and use them only with a disposable disk
image. Do not use the current utility to modify valuable media. See
[Current state](docs/current-state.md) for the complete evidence and defect
ledger.

## Repository layout (high-level)

- `systems/neo1-pico/` — RP2040-side platform target
- `systems/neo1-sdl/` — SDL2 host platform target
- `src/systems/` — CPU-neutral Neo1 machine and address space
- `src/runners/` — shared software-CPU execution runner for host-style targets
- `src/devices/` — shared 6502-visible device contracts and Apple-1 PIA model
- `src/terminal/` — shared host-side character-grid state
- `src/roms/` — ROM images/assets
- `src/ram/` — RAM-loaded utility payloads (VACI and VCFFA1 support)
- `lib/` — Pico SDK, TinyUSB, PicoDVI, FatFs

## Documentation

- `docs/architecture.md` — stable 6502-visible memory and device contracts
- `docs/current-state.md` — verified capabilities, known defects, and dated test evidence
- `docs/neo1-milestone-plan.md` — historical overall milestone plan (VACI track)
- `docs/vcffa1-v0-baseline.md` — dated VCFFA1 V0 smoke-test log
- `docs/neo1-sdl-emulator-plan.md` — SDL experiment plan; verify its claims against current code
