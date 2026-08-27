# Neo1 portable-core baseline

> Historical evidence captured before portable-core extraction. For the
> resulting architecture and verified behavior, see `docs/architecture.md` and
> `docs/current-state.md`.

Date: 2026-08-22

Repository state: `ada2dbe526fcb2c043a581b35a3a4b3f397bd3fa` on
`wip/neo6502-sdl`

Purpose: evidence checkpoint before any portable-core extraction

## Evidence labels and scope

This document uses four labels deliberately:

- **Confirmed** means observed in the checked-out code, Git object database, a
  local clean configure/build, or the recorded local smoke output.
- **Inference** means a conclusion from confirmed code that has not been
  exercised on every relevant runtime or on physical hardware.
- **Documentation claim** means a statement in an existing document that has
  not been independently demonstrated here.
- **Hardware claim** means a result that requires a Neo6502 board. No new
  hardware result was supplied or produced for this checkpoint.

The only working-tree item present before this audit was the untracked
`AGENTS.md`. This audit adds only this document. It does not refactor code or
change 6502-visible behavior.

## Executive findings

1. **Confirmed:** the history at this checkout is linear, not a two-parent Git
   merge. `main` ends at `936c8d4`; four later commits add the software CPU and
   SDL runtime. Therefore Git contains no merge-resolution commit whose conflict
   choices can be audited.
2. **Confirmed:** `src/systems/neo1.h` is the only common memory/PIA dispatcher,
   but it is not yet a platform-neutral machine module. It selects CPU-dependent
   semantics and directly includes headers from `systems/neo1-pico/`.
3. **Confirmed:** all six visible configurations compile from isolated build
   directories with the checked-out dependencies. The three SDL builds used a
   Unix Makefiles override because Ninja was not on the audit shell's `PATH`.
   In a fresh standalone shell—not the user's working VS Code Pico extension
   workflow—Pico builds additionally required the Pico SDK ARM toolchain file
   to be passed explicitly; otherwise CMake selected AppleClang before Pico SDK
   setup and failed on RP2040 assembly.
4. **Confirmed:** a headless SDL smoke for personalities 23 and 50 printed the
   WozMon `\` prompt. The dummy video driver could not create the requested
   accelerated renderer, so this is CPU/ROM/output-callback evidence, not a GUI
   display test.
5. **Confirmed:** the SDL binary does not install the VACI payload at `$C100`,
   despite every SDL preset defining `NEO1_ENABLE_VACI=1`.
6. **Confirmed:** SDL and Pico expose the same MSC register addresses but do not
   implement the same protocol. Pico implements file/directory commands over
   FatFs; SDL accepts those commands as successful no-ops and interprets
   `READ`/`WRITE` against a raw disk image.
7. **Confirmed:** the soft-CPU path changes Apple-1-visible behavior at `$D010`,
   changes keyboard-latch overwrite policy, patches RAM at `$0000-$0002`, and
   uses instruction steps as if they were machine cycles.
8. **Recommendation:** the smallest first extraction is the 40x24 terminal
   character-grid state and its primitive clear/newline/printable/backspace
   operations. Both targets already consume that behavior, it avoids the
   timing-sensitive physical bus, and target wrappers can preserve the current
   Pico form-feed and SDL backspace differences exactly.

## Merge state and history

### Line attribution

| Commit/range | Confirmed contribution | Behavioral significance |
|---|---|---|
| `a139126` through `7a4d0cd` (`main` before rename) | Neo6502 firmware line: physical W65C02 glue, DVI, USB HID/MSC, FatFs, ROM profiles, VACI, and VCFFA1 | This is the hardware-derived baseline. Existing dated hardware notes predate the SDL integration. |
| `936c8d4` (`main`) | Renamed `neo1-x` to `neo1-pico`, aligned presets, added the initial SDL directory/stub, and added the SDL plan | This commit mixes a mechanical Pico rename with creation of the first SDL skeleton. Its only change to `src/systems/neo1.h` was include-path renaming. |
| `0848259` | Added `neo1_cpu_backend.h`, `soft65C02cpu.h`, and `fake65c02.h`; altered shared reset/init/tick/keyboard/PIA behavior | This is the main point where SDL requirements changed shared/Pico-adjacent code. |
| `5e57474` | Added SDL event loop, duplicate terminal, raw image backend, and duplicate MSC/VCFFA1 symbols | SDL-only source changes, but they define a second implementation of 6502-visible storage protocols. |
| `d660101` | Ignored SDL disk images | No runtime effect. |
| `ada2dbe` | Advanced SDL input/display behavior and plan text; changed Pico fallback personality from 23 to 50; disabled VCFFA1 in SDL presets | The Pico fallback changes only builds that omit `NEO1_PERSONALITY`; all visible Pico presets set it explicitly. |

**Confirmed:** `git log --graph --all` shows one parent per commit in this
range. There is no retained SDL branch tip or merge parent from which conflict
resolution can be reconstructed. If this branch was rebased or squash-merged,
the pre-linearization refs are required to distinguish author changes from
resolution changes.

### Shared-code changes that may affect Pico

`0848259` moved `MOS6502CPU_INIT` until after RAM pattern initialization and ROM
copy, added `dsp_ready` state (currently written but never consulted), wrapped
external bus service in `MOS6502CPU_NEEDS_EXTERNAL_BUS`, and replaced direct
`sleep_us` use with `NEO1_SLEEP_US`. The hardware backend still resolves the
external-bus flag to 1 and the sleep macro to Pico `sleep_us`, so the steady
physical bus loop is textually unchanged. The init/reset-line timing moved
relative to host-side memory initialization and should be included in the next
physical reset trace comparison.

## Confirmed current architecture

```text
Pico main / physical W65C02                       SDL main / fake65c02
        |                                                  |
wdc65C02cpu.h GPIO/latches                    soft65C02cpu.h global bridge
        |                                                  |
        +---------- src/systems/neo1.h --------------------+
                   RAM + ROM policy + PIA dispatch
                     |                    |
       Pico neo1_msc/neo1_cffa1       SDL storage stub
          FatFs file/image I/O          raw image I/O

Pico terminal -> PicoDVI                duplicate SDL terminal -> SDL renderer
TinyUSB/UART -> key latch                SDL events -> key latch
```

`src/systems/neo1.h` owns the 64 KiB backing array, ROM selection boundary,
manual ROM write protection, minimal Apple-1 PIA-like registers, storage address
dispatch, callback-based character output, startup trace, and CPU tick loop. It
is a `CHIPS_IMPL` header and includes Pico-owned MSC/CFFA1 declarations. The two
targets link different functions with the same `neo1_msc_*` and
`neo1_cffa1_*` names.

The CPU is embedded as the first field of `neo1_t`, but that means different
things by target. The hardware wrapper contains captured address/R/W state. The
software wrapper contains only its last access and IRQ flag; the actual PC,
registers, flags, wait state, instruction counter, and cycle counter are
process-global/static variables in `fake65c02.h`.

There is no shared software-CPU runner separate from the machine, no shared
storage protocol implementation, and no shared terminal consumer on SDL yet.

## Configure, build, and run matrix

### Preset definitions

| Preset | Platform/backend | Personality | VACI | VCFFA1 | Preset build directory | Audit result |
|---|---:|---:|---:|---:|---|---|
| `neo1-pico-23-full` | Pico / 1 | 23 | 1 | 1 | `build` | Configured and built; UF2 produced |
| `neo1-pico-50-full` | Pico / 1 | 50 | 1 | 1 | `build` | Configured and built; UF2 produced |
| `neo1-pico-50-vaci-only` | Pico / 1 | 50 | 1 | 0 | `build` | Configured and built; UF2 produced |
| `neo1-sdl-23-full` | SDL / 3 | 23 | 1 | 0 | `build-sdl` | Configured and built; WozMon prompt observed headlessly |
| `neo1-sdl-50-full` | SDL / 3 | 50 | 1 | 0 | `build-sdl` | Configured and built; WozMon prompt observed headlessly |
| `neo1-sdl-50-vaci-only` | SDL / 3 | 50 | 1 | 0 | `build-sdl` | Configured and built; configuration is identical to `neo1-sdl-50-full` |

Only one build preset exists: `cmake --build --preset build`, hard-wired to
`neo1-pico-50-full`. All other configurations use the configure preset followed
by `cmake --build build` or `cmake --build build-sdl`.

### Intended repository commands

For any visible preset `P`:

```sh
cmake --preset P --fresh
cmake --build build          # Pico presets
cmake --build build-sdl      # SDL presets
```

`--fresh` is important because all Pico presets share `build` and all SDL
presets share `build-sdl`. Without it, the directory is reconfigured in place
rather than proving a clean build.

SDL run command:

```sh
./build-sdl/systems/neo1-sdl/neo1
```

Current optional SDL environment variables are `NEO1_SDL_STDOUT=1` for
character echo and `NEO1_SDL_DISK=/path/to/image` for the raw image. There is
no command-line parser.

Pico load-and-run command, as encoded by `.vscode/tasks.json`:

```sh
$HOME/.pico-sdk/picotool/2.2.0-a4/picotool/picotool \
  load build/systems/neo1-pico/neo1.uf2 -fx
```

This is a hardware mutation and was not run during the audit.

### Commands actually used for isolated builds

Ninja 1.12.1 exists under the local Pico extension but was not on the audit
shell's `PATH`. SDL was therefore configured with the preset values and a clean
temporary directory using:

```sh
cmake --preset <sdl-preset> -G "Unix Makefiles" \
  -B /tmp/neo1-baseline-<sdl-preset>
cmake --build /tmp/neo1-baseline-<sdl-preset> --parallel 2
```

All three succeeded with AppleClang 17 and system SDL 2.32.10.

The same direct clean Pico configure initially selected AppleClang and failed
while assembling `boot2_generic_03h.S`. A successful clean CLI configuration
required:

```sh
cmake --preset <pico-preset> -G "Unix Makefiles" \
  -B /tmp/neo1-baseline-<pico-preset> \
  -DCMAKE_TOOLCHAIN_FILE="$PWD/lib/pico-sdk/cmake/preload/toolchains/pico_arm_gcc.cmake"
cmake --build /tmp/neo1-baseline-<pico-preset> --parallel 4
```

All three then built with the Pico extension's ARM GCC 13.3.1 and produced
`neo1`, `.bin`, `.dis`, `.elf.map`, `.hex`, and `.uf2` outputs. This is build
evidence only, not hardware validation.

This is a standalone clean-CLI integration gap, not a failure of the user's
current VS Code Pico SDK workflow. It follows from `project()` running before
`pico_sdk_import.cmake` in the top-level CMake file. The VS Code Pico extension
supplies Ninja and the ARM compiler/toolchain, and the existing IDE build cache
contains the correct ARM compilers. The source list also spells `utils.S` while
the tracked file is `utils.s`; this works on the audited case-insensitive macOS
filesystem but is a likely clean Linux failure.

### Smoke evidence

`ctest -N` reported zero tests for both SDL and Pico builds.

For SDL, the binary was run with a dummy SDL video driver, stdout echo, and a
disposable `/tmp` image path. Because `SDL_CreateRenderer(...,
SDL_RENDERER_ACCELERATED)` failed under the dummy driver, platform initialization
returned before opening the disk. The machine continued and produced:

| Personality | Observed ROM/vector bytes | Observed output |
|---|---|---|
| 50 | `FFFC=00 FFFD=FF`, hence RESET `$FF00`; IRQ vector `$0000` | WozMon `\` prompt |
| 23 | `FFFC=00 FFFD=FF`, hence RESET `$FF00`; IRQ vector `$FE03` | WozMon `\` prompt |

The disk self-test failed in this headless run because disk initialization came
after renderer creation. No SDL keyboard event, visible window, scrolling,
storage write, decimal-mode instruction, interrupt, or 65C02-specific
instruction was exercised.

## Dependency, revision, and provenance matrix

| Dependency | Source and pin | Version evidence | License/provenance status |
|---|---|---|---|
| Pico SDK | Git submodule `https://github.com/jp1971/pico-sdk.git` at `5b94e1b41086d810b906b5722079dad83cbaf088` | Vendored CMake reports 1.5.1; commit is an untagged fork commit dated 2026-03-17 | Pinned as a submodule, but upstream base/rebase relation is not recorded. Top-level extension metadata says 2.1.0 and `.vscode` says 2.2.0, neither of which matches the vendored SDK actually built. |
| PicoDVI | Git submodule `https://github.com/vsladkov/PicoDVI.git` at `7e15fb873ac17dc22e652c9026308601bafa8f12` | Untagged commit dated 2024-06-24 | Pinned; BSD-3-Clause file credits Luke Wren. The relationship between this fork and upstream PicoDVI beyond Git history is not documented in Neo1. |
| TinyUSB | Git submodule `https://github.com/vsladkov/tinyusb.git` at `56c6d2feab2e6e68a539534910900ae9f4e593d1` | Untagged commit dated 2023-09-19; no checked-out version manifest was found | Pinned; MIT license present. It is a fork URL even though the pinned commit is authored by upstream maintainer Ha Thach. |
| SDL2 | Host `find_package(SDL2 REQUIRED)`; local CMake package at `/opt/homebrew/lib/cmake/SDL2` | Local `sdl2-config` reports 2.32.10 | Unpinned system dependency. No minimum/maximum version or fetched source is recorded, and the repository does not carry its license. |
| FatFs | First-party-vendored tree under `lib/fatfs`, initially added by Neo1 commit `a12df8e` | `ff.c` says R0.15 with patch1; `ff.h` revision ID 80286, copyright 2022 | Not a submodule and no upstream URL, archive checksum, or import script is recorded. FatFs license text is present and describes a BSD-style/one-clause redistribution license. |
| Software 65C02 core | Single header `src/chips/fake65c02.h`, first appearing in `0848259`; no external pin | Header calls itself Fake65c02 v1.4 and points generally to Commander X16 | Provenance is insufficient. The header names Mike Chambers, Paul Robson, and David MHS Webster, claims public-domain/CC0 status, but also says it incorporates changes from a “non public domain” X16 emulator and asks maintainers to object if needed. No source URL, source commit, patch record, standalone license, or checksum is supplied. Treat licensing and suitability as unresolved. |

The Pico SDK's nested submodules are mostly uninitialized in this checkout. The
Neo1 build explicitly points Pico SDK at the separately pinned top-level
`lib/tinyusb`, and the audited targets did not require the other nested SDK
submodules.

## Target execution-flow traces

### Reset, reset-vector fetch, and WozMon

#### Neo6502 physical path

1. `systems/neo1-pico/src/neo1.c:app_init` clears the Pico terminal and calls
   `neo1_init` with either the 8 KiB Neo1-23 ROM or the 256-byte Apple-1 ROM.
2. `neo1_init` maps all 64 KiB to `neo1_t.ram`, fills even addresses with `$00`
   and odd addresses with `$FF`, copies the selected ROM, then calls the hardware
   CPU initializer.
3. `wdc65C02cpu_init` configures GPIO/latches/clock and pulses active-low RESET
   on GPIO 26 for 1 ms. For personality 50, Pico then writes temporary
   `JMP $FF00` stubs at `$E000` and `$F000`.
4. `neo1_reset` clears PIA/runtime state and pulses RESET a second time. Pico
   then copies VACI to `$C100` and, when enabled, the VCFFA1 test driver to
   `$1800` before any main-loop tick.
5. After DVI startup, `neo1_tick` drives clock low, samples the latched address
   and R/W, drives clock high, then services that observed bus cycle through
   `_neo1_mem_read` or `_neo1_mem_write`.
6. The physical W65C02's reset sequence therefore causes observed reads of
   `$FFFC/$FFFD`, both profiles resolve to `$FF00`, and instruction fetch enters
   WozMon. The first 64 serviced cycles are buffered and printed.

**Hardware status:** build-confirmed only at this checkpoint. The actual reset
pin connection, first 64 bus cycles, WozMon screen, DVI, UART, USB, and storage
were not re-tested after the SDL integration.

#### SDL software path

1. `systems/neo1-sdl/src/main.c` initializes the SDL platform and duplicate
   storage symbols, selects the same ROM images, then calls `neo1_init`.
2. After ROM copy, the soft-only branch writes `JMP <RESET-vector>` to
   `$0000-$0002`.
3. `soft65C02cpu_init` stores the machine pointer in a process-global bridge and
   calls `reset6502`, which directly invokes the machine read callback for
   `$FFFC/$FFFD` and sets the global PC to `$FF00`.
4. `neo1_reset` later clears PIA state and calls soft RESET assertion. Assertion
   immediately calls `reset6502` again; host sleep is a no-op and deassertion is
   a no-op.
5. Each `neo1_tick` calls `step6502`, which executes one entire instruction and
   performs internal semantic memory callbacks. External bus service is disabled.
   These callbacks do not populate the shared startup trace.
6. The headless smoke observed WozMon's `\` prompt for both ROM profiles.

### Normal RAM read and write

For both targets, an address not claimed by VCFFA1, PIA, or MSC falls through to
`mem_rd`. Writes fall through to `mem_wr` only when the address is below
`NEO1_ROM_PROTECT_BASE`. Pico reaches these functions once per captured physical
bus cycle. Fake65c02 reaches them directly through `read6502`/`write6502` during
an instruction. The backing byte array is common, but the bus timing and access
sequence are not.

### Keyboard `$D010/$D011`

Input transport differs intentionally: Pico polls UART and TinyUSB HID; SDL
polls SDL text/key events. Both uppercase ASCII letters in their main runners,
translate return to CR, then call `neo1_key_down`, which sets bit 7.

`$D011` reads return the saved control bits plus bit 7 when a key is latched.
On Pico, `$D010` obeys control bit 2: it reads the keyboard DDR when bit 2 is
clear, otherwise reads and consumes the key latch. On the soft backend,
`$D010` is forced to read and consume the key regardless of control bit 2.
Pico refuses a new character while the latch is nonzero; SDL overwrites a
pending character with the newest one. No PIA interrupt is generated.

### Display `$D012/$D013`

`$D012` obeys display control bit 2 on writes: clear selects the DDR; set saves
the data byte and invokes the character callback. Reads return DDR when bit 2
is clear and `$00` otherwise, so WozMon's busy test sees ready. `$D013` returns
saved control bits with bit 7 forced high. `$D0F2/$D0F3` normalize to
`$D012/$D013` for Replica 1 compatibility. No display interrupt is generated.

Pico strips bit 7, updates `neo1_terminal_t`, synchronizes it to the PicoDVI
renderer, and echoes CR as CR/LF on UART. SDL strips bit 7 in its platform
function, updates a separate static grid, and optionally emits the raw
character to stdout.

### ROM selection and write protection

| Profile | Payload | Copy/protect range | Additional target behavior |
|---|---|---|---|
| 23 | `neo1_system_rom_bin`, 8192 bytes | `$E000-$FFFF` | Pico and SDL select the same ROM. Pico also installs RAM tools after reset. |
| 50 | `neo1_apple1_rom_bin`, 256 bytes | `$FF00-$FFFF` | Pico installs temporary `$E000/$F000 -> $FF00` stubs. SDL does not, leaving the alternating RAM pattern there. SDL additionally patches `$0000`. |

Writes at or above the profile's protect base are silently ignored by the
shared dispatcher. Reads use the same backing array; there is no separate ROM
page. Snapshot loading can restore the full array, so ROM integrity after a
snapshot depends on snapshot provenance.

### MSC register access and VACI

The Neo1 extension occupies `$D014-$D01C`: command, 16-bit sector, data,
status, index, info, and 16-bit size. Operations are synchronous and there is
no IRQ.

Pico's `neo1_msc.c` implements `OPEN`, `CLOSE`, `READ`, `WRITE`, `DIR_OPEN`,
`DIR_NEXT`, `OPEN_INDEX`, and `DELETE_INDEX` against files on FatFs volume
`0:`. Filename bytes and sector payload share `$D017`; status is busy/ready or
`$80 | FatFs-error`.

Pico startup copies the checked-in 2302-byte VACI image to `$C100`. From WozMon,
`C100R` enters 6502-side VACI, which uses the MSC register protocol for file
listing, indexed load/delete, and named writes.

SDL never includes or copies `neo1_vaci_v1.h`; `$C100` remains the alternating
RAM pattern. Its stub treats `READ` as raw image LBA read and `WRITE` as a raw
image write/arming sequence. `OPEN`, `CLOSE`, and directory/index commands fall
through to success without doing anything. Thus the SDL preset's VACI flag is
only a compile definition and does not provide VACI startup or file semantics.

### VCFFA1 decode and commands

When `NEO1_ENABLE_VCFFA1=1`, the shared dispatcher routes signature reads at
`$AFDC/$AFDD` (`$CF/$FA`) and the register window `$AFF0-$AFFF` before normal
RAM. DATA is `$AFF8`; ERROR/FEATURE `$AFF9`; LBA bytes `$AFFB-$AFFE`; and
STATUS/COMMAND `$AFFF`. Commands `$00/$01/$02` mean status/read/write. There is
no IRQ/NMI behavior.

All visible Pico full presets enable this decode. Pico lazily mounts FatFs,
selects a `.PO`, `.HDV`, or `.2MG` image, checks block bounds, distinguishes
no-device/write-protect/bad-block/I/O errors, and commits a 512-byte write only
to a writable selected image. Pico also installs the `$1800` 6502-side driver.

All visible SDL presets disable decode at compile time. The SDL source still
defines duplicate VCFFA1 symbols unconditionally, but `main` does not initialize
them and the shared dispatcher does not call them. If manually enabled, SDL
would address the raw host image directly without Pico's image selection,
explicit block-count bound check, or read-only policy.

## Shared behavior, duplication, and intentional differences

| Area | Shared/identical today | Duplicate or divergent behavior | Intended platform difference |
|---|---|---|---|
| 64 KiB memory and ROM policy | One dispatcher/backing array in `neo1.h` | Soft-only `$0000` patch; Pico-only E/F stubs and RAM tools | Bus producer is physical on Pico and software on SDL |
| Apple-1 PIA | Addresses and most register logic are shared | Soft-only forced `$D010` data read and latch overwrite | Keyboard transport and display transport |
| CPU execution | Macro names create a common call surface | Macro surface hides instruction-step vs physical-cycle semantics | Physical W65C02 and software CPU must remain separate runners |
| Terminal grid | Same nominal 40x24 geometry/font asset | Two state machines; Pico supports form-feed clear and ignores BS, SDL erases on BS and does not clear on form feed; different glyph widths/cursor paths | PicoDVI scanout vs SDL rendering |
| MSC/VACI protocol | Same declarations/addresses/opcodes | Pico file protocol vs SDL raw blocks/no-op file commands; SDL has no VACI payload | FatFs/USB backend vs host-directory backend |
| VCFFA1 | Same declaration constants and approximate register shape | Two protocol state machines with different media, bounds, and write-protect behavior | Backend I/O only; 6502-visible protocol should eventually be one implementation |
| Reset | Both resolve RESET to `$FF00` | Direct vector assignment/two soft resets vs physical reset pulse and bus fetch; no SDL startup trace | Runner-specific reset mechanism |
| Timing | Shared `neo1_exec` API name | SDL counts one instruction as one 1.022 MHz tick and has no real-time governor | Pico GPIO timing and SDL scheduling |
| Input | Both inject bit-7-set ASCII | Event mappings, latch policy, control-register workaround | UART/TinyUSB vs SDL event source |
| Display | Same output callback point | Duplicate terminal control handling and stdout newline policy | DVI/core1 vs SDL main-thread renderer |

## Every current 6502-visible semantic difference

The following differences are confirmed in code:

1. Soft `$D010` reads always consume keyboard data; Pico can return the DDR.
2. Soft input replaces an unread key; Pico preserves the first unread key.
3. Soft init overwrites `$0000-$0002` with `JMP` to RESET; Pico retains the RAM
   seed until 6502 software writes it.
4. Pico 50 writes `JMP $FF00` to `$E000` and `$F000`; SDL 50 does not.
5. Pico installs VACI at `$C100` in every visible profile; SDL does not.
6. Pico installs the VCFFA1 `$1800` driver when enabled; SDL never does.
7. MSC file/directory commands execute on Pico and report real errors; SDL
   reports success without performing them.
8. MSC `READ`/`WRITE` address an open file sector on Pico but a raw image LBA on
   SDL. Pico respects the size register for short final writes; SDL writes a
   full raw sector.
9. VCFFA1 is decoded by Pico full presets and absent in all SDL presets. A
   manually enabled SDL implementation would differ in bounds, media discovery,
   write protection, and error meanings.
10. Display bytes feed terminal models with different control behavior:
    form-feed clears Pico but is ignored by SDL; backspace is ignored by Pico
    but erases one SDL cell.
11. Software reset has no bus cycles or 1 ms asserted interval and cannot
    populate the startup trace; Pico reset does.
12. One SDL “tick” executes an instruction and all its memory accesses. One
    Pico tick services one physical bus cycle. Consequently elapsed-tick and
    device-side-effect ordering are not equivalent.

No other backend conditional was found in the shared dispatcher at this
revision. Rendering pixels, host window focus, DVI timing, USB enumeration,
UART echo, and disk-file location are platform-visible but not directly
6502-visible unless their failures alter the device response described above.

## First-party source classification

Files are grouped only when every named file has the same primary
classification. “Ambiguous” calls out mixed ownership rather than endorsing it.

| File(s) | Primary classification | Evidence/qualification |
|---|---|---|
| `src/systems/neo1.h` | Obsolete, duplicated, or ambiguous | Mixed shared machine, CPU runner, snapshot code, backend conditionals, and direct Pico storage includes |
| `src/chips/neo1_cpu_backend.h` | Obsolete, duplicated, or ambiguous | Selector over incompatible execution models through one Reload-style macro surface |
| `src/chips/soft65C02cpu.h` | Software-CPU execution | Adapter and process-global machine bridge |
| `src/chips/fake65c02.h` | Software-CPU execution | Actual third-party software CPU and global architectural state; provenance remains ambiguous |
| `src/chips/wdc65C02cpu.h` | Physical W65C02 bus execution | Reload-derived GPIO, address/data latches, clock, RESET, IRQ, and NMI glue |
| `src/chips/chips_common.h`, `src/chips/clk.h`, `src/chips/mem.h` | Third-party/Reload-derived support | André Weissflog CHIPS-style types, clocks, and paged memory helper |
| `src/roms/neo1_roms.h`, `src/roms/neo1_apple1_rom_image.h`, `src/roms/neo1_system_rom_image.h`, `src/roms/neo1_apple1_video_rom_image.h`, `src/roms/wozmon.asm` | 6502-side software or generated payload | ROM/font payloads and monitor source reference |
| `src/ram/neo1_vaci_v1.s`, `src/ram/neo1_vaci_v1.cfg`, `src/ram/neo1_vaci_v1.h`, `src/ram/gen_vaci_header.py` | 6502-side software or generated payload | VACI source/link/generated header pipeline |
| `src/ram/neo1_cffa1_m2_blockdrv.s`, `src/ram/neo1_cffa1_m2.cfg`, `src/ram/neo1_cffa1_m2_blockdrv.h`, `src/ram/gen_cffa1_m2_header.py` | 6502-side software or generated payload | VCFFA1 source/link/generated header pipeline |
| `src/ram/gen_m2_header.py` | Obsolete, duplicated, or ambiguous | Older generic generator remains beside the named VCFFA1 generator and is not in the build |
| `systems/neo1-pico/src/neo1.c` | Physical W65C02 bus execution | Pico runner/orchestrator, currently mixed with profile payload installation and platform input/output wiring |
| `systems/neo1-pico/src/neo1_terminal.c`, `.h` | Shared Apple-1 machine behavior | Platform-independent character-grid state plus Pico-specific framebuffer conversion; located under Pico despite two-target need |
| `systems/neo1-pico/src/neo1_video.c`, `.h`, `utils.s` | Neo6502 platform service | PicoDVI, multicore scanout, and RP2040 assembly |
| `systems/neo1-pico/src/neo1_usb.c`, `.h` | Neo6502 platform service | TinyUSB keyboard/MSC host and FatFs disk-backend glue |
| `systems/neo1-pico/src/neo1_msc.c`, `.h` | Shared 6502-visible Neo1 extension | Canonical file-oriented MSC register state, currently mixed with direct FatFs backend calls |
| `systems/neo1-pico/src/neo1_cffa1.c`, `.h` | Shared 6502-visible Neo1 extension | Canonical optional Replica 1 compatibility protocol, currently mixed with direct FatFs image calls |
| `systems/neo1-sdl/src/main.c` | Software-CPU execution | Software runner, currently mixed with SDL lifecycle/event-loop orchestration |
| `systems/neo1-sdl/src/neo1_platform.h` | Obsolete, duplicated, or ambiguous | Bundles lifecycle, display, input, timing, and raw block storage; only one implementation/consumer experiment |
| `systems/neo1-sdl/src/neo1_platform_sdl.c` | SDL platform service | SDL lifecycle/render/input, currently mixed with a second terminal and raw-image backend I/O |
| `systems/neo1-sdl/src/neo1_storage_stub.c` | Obsolete, duplicated, or ambiguous | Duplicate MSC/VCFFA1 protocols coupled to the SDL raw image backend |
| `src/tusb_config.h` | Neo6502 platform service | TinyUSB compile-time configuration |
| `src/hid_app.c`, `src/msc_app.c` | Obsolete, duplicated, or ambiguous | Unreferenced Reload-derived support; older generic TinyUSB app callbacks overlap current Pico USB responsibilities |

`README.md`, `AGENTS.md`, `docs/*.md`, CMake files, `.vscode` files, and
`.gitignore` are first-party project/support files but are not runtime source,
so they are outside the requested behavioral source taxonomy.

## Reload/CHIPS dependency inventory

| Item | Current use | Attribution/license evidence | Portability consequence |
|---|---|---|---|
| `chips_common.h` | Debug/range/snapshot types | André Weissflog, zlib/libpng notice | Pulls broad emulator-framework types into the machine description |
| `mem.h` | Maps the single 64 KiB array, snapshot pointer fixups | André Weissflog, zlib/libpng notice | Far more general than the current flat address space; not itself target-specific |
| `clk.h` | Converts host microseconds to nominal ticks | André Weissflog, zlib/libpng notice | Helps conceal the software instruction/cycle mismatch |
| `wdc65C02cpu.h` | Physical bus GPIO/latch access behind `MOS6502CPU_*` | Veselin Sladkov, zlib/libpng notice | A physical component is presented through emulator-shaped macros |
| `src/systems/neo1.h` | `CHIPS_IMPL` machine/state/snapshot pattern | Veselin Sladkov 2023 plus 2026 modifications, zlib/libpng notice | Machine, runner, and device dispatch compile into one translation unit |
| `neo1_cpu_backend.h` | Selects WDC, missing `mos6502cpu.h`, or fake65c02 | First-party selector; no separate license header | Backend 2 names a header absent from this repository; only 1 and 3 are buildable |
| `utils.s` and legacy `src/*_app.c` | RP2040 scanline helper; currently unbuilt generic USB callbacks | No clear per-file upstream/license notice for the assembly/app files | Provenance should be resolved before extraction or reuse |

The code should not be removed merely for being derived. The first extraction
should avoid touching these files except for include/source-list adjustments
needed to consume the new terminal module.

## Contradictions between code and existing documentation

| Documentation claim | Confirmed code/build reality |
|---|---|
| SDL plan §1.1 calls `wdc65C02cpu.h` a shared header-only 65C02 core | It is GPIO/latch glue for a physical CPU and includes Pico GPIO calls under `CHIPS_IMPL`. SDL uses `fake65c02.h`. |
| SDL plan §1.1 says the Pico terminal is shared as-is | SDL implements a second static grid/state machine and does not compile the Pico terminal. |
| SDL plan §1.3 calls `neo1_platform.h` a five-function/two-implementation HAL | The header now declares eleven functions, exists only under SDL, and has no Pico implementation. |
| SDL plan display model says SDL uses an ARGB buffer and `SDL_Texture` blit | Current SDL draws glyph rectangles directly through `SDL_Renderer`; `update_display` ignores its pixel arguments. |
| SDL plan input says Return `$8D`, backspace `$DF`, escape `$9B` | The platform returns 7-bit CR and `$08`; `neo1_key_down` adds bit 7. Escape is not injected. |
| SDL plan/current text says `NEO1_SDL_FILES` selects a VACI host directory | Code recognizes only `NEO1_SDL_DISK` and creates/opens one raw image. |
| SDL plan says VCFFA1 is out of SDL and plumbing removed | Presets disable decode, but SDL still compiles the full duplicate VCFFA1 implementation and includes Pico's CFFA1 header. |
| Presets label SDL as “VACI only” | SDL does not install VACI at `$C100`; file-oriented commands are no-ops. |
| SDL plan M1 says `neo1_tick` runs at a fixed rate | The main loop calls `neo1_exec(2000)` without elapsed-time pacing; each “tick” is one full instruction. |
| SDL plan M2 says inverse attribute works | SDL masks bit 7 and has no inverse-attribute path. |
| SDL plan M3 says input uses the existing Pico terminal/USB injection path | SDL bypasses both modules and calls the shared latch directly. |
| SDL plan M4 and README imply `E000R`/`F000R` run BASIC/Krusader generally | Profile 23 contains them in the 8 KiB ROM. Profile 50 Pico installs return-to-WozMon stubs until storage loads programs; SDL 50 has neither payload nor stubs. |
| README says personality 23 is the default | Root/Pico cache default and presets now default to 50; only the prose and old source comment say 23. |
| README says personality 23 requires VACI and VCFFA1 | That policy check exists only in the Pico CMake file. The visible SDL-23 preset explicitly disables VCFFA1 and configures successfully. |
| README quickstart initializes submodules from inside `lib` using paths `pico-sdk PicoDVI tinyusb` | Those paths are rooted at the repository, so the shown command is not the reliable root-level submodule command. |
| Root Pico extension metadata says SDK/tool versions 2.1.0; `.vscode` says SDK/toolchain 2.2.0/14.2 | The repository forces vendored Pico SDK 1.5.1; successful audit build used extension toolchain 13.3.1. |
| Source lists `utils.S` | Git tracks `utils.s`; the audited macOS filesystem hides the case mismatch. |

The plan correctly describes the present raw-block/no-op VACI limitation in
§5, and its proposed storage ownership rule remains directionally sound. Its
milestone language is planning, not evidence that the milestones passed.

## Risks in the software CPU integration

1. All CPU architectural state is global/static, so only one machine instance
   can execute correctly in a process. `_soft65c02_user` and
   `_soft65c02_active` add another global bridge.
2. `neo1_save_snapshot` copies `soft65c02cpu_t` but not fake65c02's PC,
   registers, flags, wait state, counters, or callback state. Software snapshots
   cannot restore CPU execution.
3. A tick is an instruction, not a cycle. `neo1_exec(2000)` performs about 2043
   instructions, not 2043 cycles, and the SDL loop does not pace those slices to
   wall time.
4. Reset is a direct PC/vector assignment, with no reset bus sequence, dummy
   accesses, or startup trace. It is called during init and again on reset
   assertion.
5. Memory callbacks perform instruction-level semantic accesses. They cannot be
   assumed to match W65C02 bus cycles, read-modify-write phases, or timing of
   device side effects.
6. IRQ is checked before every instruction and forwarded to a global core;
   interrupt timing and WAI wake behavior have no focused tests.
7. The core claims CMOS additions and decimal fixes, but this repository has no
   decimal-mode, interrupt, WAI/STP, Rockwell bit-op, or general 65C02
   compatibility suite. Opcode `$DB` is implemented as a PC-decrementing wait;
   opcode/timing suitability for the physical W65C02 target is unverified.
8. The soft-only `$0000` JMP, forced keyboard data read, and overwrite latch can
   hide CPU/PIA integration defects and are not desired machine semantics.
9. The startup trace remains empty for SDL because direct soft callbacks bypass
   `_neo1_capture_trace`.
10. `neo1_cpu_backend.h` advertises backend 2 (`mos6502cpu.h`), but that file is
    absent. The selector is not a complete supported matrix.
11. The fake65c02 provenance/license chain is unresolved, so long-term adoption
    is a dependency decision even if functional tests eventually pass.

## Proposed end-state boundaries

These boundaries represent concrete physical components or observable
behavior; none requires a speculative universal HAL:

| Boundary | Owns | Concrete consumers/providers |
|---|---|---|
| Shared Apple-1 machine | 64 KiB address space, RAM seed/profile, ROM aperture/write protection, `$D010-$D013` PIA-like behavior, output/input latch state, explicit read/write API | Physical bus runner and software CPU runner |
| Physical W65C02 runner | RESET/IRQ/NMI pins, clock, address/data latch direction/order, one observed read/write cycle, startup bus trace | Neo6502 only |
| Software CPU runner | One instance-owned software CPU, reset/interrupt instruction execution, cycle accounting, calls to machine read/write | SDL and future Fruit Jam |
| Shared terminal grid | 40x24 cells, cursor, clear/newline/wrap/scroll and explicit editing primitives | Pico terminal adapter and SDL terminal adapter |
| Shared MSC protocol | `$D014-$D01C` register state/status/errors and VACI file-command sequencing | FatFs file backend and host-directory backend |
| Shared VCFFA1 compatibility protocol | `$AFDC/$AFDD`, `$AFF0-$AFFF`, ATA-like state/data phase/status/errors | Optional FatFs image backend and optional host image backend |
| Pico services | DVI scanout, UART, TinyUSB HID/MSC, FatFs disk I/O, timing/lifecycle | Pico runner |
| SDL services | SDL window/renderer/events/timing/lifecycle, host directory/image I/O | SDL runner |
| 6502-side payloads | WozMon/system ROMs, VACI, VCFFA1 driver and generated images | Installed by a shared profile/runner startup decision, separate from platform I/O |

Address ownership, enablement, bus direction, and interrupt behavior should be
explicit in the shared machine API. This does not require a generic expansion
framework: the first concrete shared consumers are the current two runners.

## Smallest recommended first extraction

Extract only the terminal character-grid state and primitive mutations from
`systems/neo1-pico/src/neo1_terminal.*` and the duplicate static SDL grid into
ordinary shared `.c/.h` files. Keep rendering, event translation, output
callbacks, and control-byte policy in their existing target modules.

The shared API should expose state plus explicit primitives such as clear,
carriage-return/newline, printable placement, and backspace-erase. A Pico
wrapper must preserve today's behavior: CR advances, form feed clears,
backspace is ignored. An SDL wrapper must preserve today's behavior: CR
advances, LF is ignored, backspace erases, and form feed is ignored. Pico's
framebuffer conversion can initially remain Pico-owned; SDL's rectangle drawing
can remain SDL-owned. This yields two real consumers without inventing a display
HAL or changing `$D012/$D013` semantics.

Why this seam is first:

- it removes a confirmed duplicate state machine;
- it is independent of fake65c02 selection and W65C02 GPIO timing;
- it can be exhaustively host-tested with small deterministic cell/cursor
  fixtures;
- rollback is one source-list/include reversal;
- it does not prejudge the larger file-oriented MSC backend boundary.

Do not start with `neo1_platform.h`: it bundles unrelated services and has only
one concrete implementation. Do not start with storage: the two targets do not
currently share observable semantics, so a behavior-preserving extraction needs
a protocol test fixture and explicit backend contract first.

## Validation and rollback gates for the first extraction

### Host tests

Add a host-only terminal test target that does not initialize SDL. Capture
pre-extraction golden state and assert:

1. clear produces 40x24 spaces and cursor `(0,0)`;
2. printable placement and 40-column wrap match both current implementations;
3. CR advances to column 0 and scrolls at row 23;
4. scrolling moves rows exactly once and clears the final row;
5. Pico policy: form feed clears and backspace changes nothing;
6. SDL policy: backspace moves left/erases, LF changes nothing, and form feed
   changes nothing;
7. high-bit stripping remains in the existing target callbacks, not silently
   moved into the shared grid;
8. no test writes storage or touches SDL/Pico hardware.

### SDL gates

For all three SDL presets:

- clean configure/build succeeds;
- headless stdout smoke still reaches the same WozMon `\` prompt for profiles
  23 and 50;
- interactive GUI test shows the same font geometry, cursor, wrapping,
  scrolling, CR, and backspace behavior;
- typing `FF00.FF0FR` produces the same monitor output;
- `NEO1_SDL_DISK` startup/self-test behavior is unchanged, even though storage
  is outside the extraction.

### Pico build gate

All three Pico presets must clean-build and produce UF2 files with no changes to
`wdc65C02cpu.h`, `neo1_video.*`, `neo1_usb.*`, `neo1_msc.*`, `neo1_cffa1.*`, or
`utils.s` other than the minimum terminal include/source-list adjustment.

### Physical Neo6502 gate (user confirmation required)

Use disposable storage/media and, for each affected profile needed by the
release matrix:

1. flash the pre-extraction and post-extraction UF2 built with the same toolchain;
2. confirm RESET wiring/DIP state and record the date/board/toolchain;
3. compare the printed 64-event startup trace byte-for-byte, including
   `$FFFC/$FFFD` fetch and early `$FF00` execution;
4. confirm WozMon `\` appears on DVI and UART;
5. type `FF00.FF0FR` over UART and USB keyboard and compare output;
6. emit enough characters to wrap and scroll, then confirm form-feed behavior
   and the blinking cursor are unchanged;
7. launch `C100R`; on a full profile also verify `$AFDC=$CF`, `$AFDD=$FA` and
   the existing VCFFA1 status smoke. Do not perform storage writes except on a
   disposable image/media.

This is a regression gate, not evidence produced by this audit.

### Rollback condition

Revert the extraction if any terminal golden state differs, any preset fails to
clean-build, SDL monitor/input/display output changes, the Pico startup trace
differs, DVI/USB stability regresses, or preserving behavior requires a new
target conditional in the shared machine. Investigate before attempting a
broader abstraction.

## Must remain untouched in the first extraction

- `src/chips/wdc65C02cpu.h`, especially clock low/sample/clock high order,
  latch OE order, GPIO direction changes, NOP delays, RESET/IRQ/NMI polarity,
  and data-drive timing.
- The order of `MOS6502CPU_TICK` followed by external bus service in
  `neo1_tick`.
- PicoDVI clock setup, core1 launch, scanline callback, TMDS buffer count, font
  RAM, and framebuffer handoff.
- TinyUSB host task cadence, HID report handling, MSC callbacks, FatFs disk
  glue, and UART polling.
- `$D010-$D013`, `$D0F2/$D0F3`, `$D014-$D01C`, `$AFDC/$AFDD`, and
  `$AFF0-$AFFF` decode or semantics.
- ROM base/protection policy, the soft `$0000` workaround, Pico E/F stubs,
  VACI/VCFFA1 payload installation, reset timing, snapshots, and CPU selection.
- SDL raw image behavior and all storage writes.

## Unresolved questions requiring user or hardware input

1. What are the original SDL branch tip and the actual merge/rebase commands?
   Without those refs, merge-resolution intent cannot be recovered from the
   linear history.
2. What is the last physical Neo6502 test performed on `ada2dbe` (date, board
   revision, RESET wiring/DIP state, profile, toolchain, DVI display, keyboard,
   and storage results)? Existing dated VCFFA1 notes predate this integrated
   baseline.
3. Is the Pico fallback personality intentionally 50, and should SDL-23 be
   allowed to violate the documented “23 requires VCFFA1” policy?
4. Should SDL-50 intentionally leave `$E000/$F000` as seeded RAM while Pico-50
   installs bounce stubs, or is that an unintentional visible difference?
5. Is VACI expected to work on SDL now, or are the preset labels knowingly
   ahead of implementation?
6. What exact upstream repository/commit and patch history produced
   `fake65c02.h`, and has its licensing been reviewed?
7. Which W65C02 compatibility tests, if any, have already been run against this
   fake65c02 revision (decimal mode, interrupts, WAI/STP, bit operations, and
   invalid opcodes)?
8. What upstream commits/tags correspond to the Pico SDK and TinyUSB fork pins,
   and which SDK/toolchain version is release-authoritative: vendored 1.5.1,
   top-level 2.1.0 metadata, or `.vscode` 2.2.0 metadata?
9. What is the import URL/archive/checksum for FatFs R0.15 patch1 and the ROM/
   character-ROM/generated payload assets?
10. Should the legacy unbuilt `src/hid_app.c`, `src/msc_app.c`, and generic
    `gen_m2_header.py` be retained for a known consumer, or classified for later
    removal after provenance review?

Until these are answered, the current checkout is a useful dual-target
experiment and buildable baseline, but not yet evidence of one behaviorally
identical shared Apple-1 machine on both targets.
