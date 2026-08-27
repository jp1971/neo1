# Neo1 Fruit Jam Execution Plan

Date: 2026-08-26

Status: proposed; no Fruit Jam implementation has started

Baseline: `neo1-portable-core-complete-2026-08-26`

## Decision

Adafruit Fruit Jam is the next logical platform target, provided it is built as
a second consumer of the software-CPU path established by SDL. It must not be a
port of the Neo6502 physical-bus runner and must not turn SDL's mixed local
interface into a speculative universal HAL.

The implementation should add `systems/neo1-fruitjam/` as a thin RP2350B
runner around the existing shared machine and `neo1_soft_runner`. Fruit Jam
will own HSTX video, GPIO/PIO USB-host transport, microSD transport, timing,
diagnostics, and lifecycle. The shared machine will continue to own all
6502-visible memory and device behavior.

Work should begin with Pico SDK 2.3.0 compatibility and software-CPU
qualification. Board code should not begin by copying the Neo6502 RP2040
runner: Fruit Jam has no physical W65C02 bus, uses HSTX rather than PicoDVI,
uses GPIO-based USB host rather than the RP2040 native-host arrangement, and
uses onboard microSD rather than USB mass storage for its preferred media.

## Hardware and toolchain evidence

The plan is based on the following verified upstream facts:

- Fruit Jam uses an RP2350B with 520 KB internal SRAM, 16 MB flash, and 8 MB
  PSRAM. Initial Neo1 work does not require PSRAM.
- DVI is wired to the RP2350 HSTX-capable GPIO range. Raspberry Pi provides an
  official HSTX DVI encoder example for RP2350.
- The two USB-A connectors sit behind a hub connected to GPIO-based USB-host
  data pins; host power has a separate enable pin. This needs a Fruit
  Jam-specific host transport and dependency decision.
- The onboard microSD slot supports SPI and SDIO. Start with SPI; SDIO is an
  optimization, not a bring-up prerequisite.
- Pico SDK 2.1.0, Neo1's prior verified baseline, does not include the
  `adafruit_fruit_jam` board. Official board support was added in SDK 2.2.0;
  Neo1's current SDK 2.3.0 baseline also defines board PSRAM constants.
- Published Fruit Jam boards using A2 RP2350B silicon are affected by the E9
  GPIO erratum. Bring-up must record the tested board revision and avoid
  assumptions about weak pulls or high-impedance inputs.

Pin constants must come from the selected SDK board definition and be checked
against Adafruit's schematic/pinout before hardware code is committed. Do not
copy pin numbers from an example without that comparison.

## Current Neo1 boundary inventory

| Component | Current owner | Fruit Jam use |
| --- | --- | --- |
| 64 KB address space, RAM/ROM policy, decode | `neo1_machine` | Reuse unchanged |
| Neo1-23/Neo1-50 ROM layouts | `neo1_profile` | Reuse unchanged |
| Apple-1 `$D010-$D013` behavior | `neo1_apple1_pia` | Reuse unchanged |
| Software 65C02 execution and timing budget | `neo1_soft_runner` | Reuse after qualification |
| 40x24 character cells and scrolling | `neo1_terminal` | Reuse unchanged |
| `$D014-$D01C` MSC protocol | `neo1_msc` | Reuse unchanged |
| Pixel rendering and output-byte policy | Pico/SDL target code | Add Fruit Jam HSTX implementation |
| Keyboard transport | Pico TinyUSB or SDL events | Add Fruit Jam GPIO/PIO USB host |
| Filesystem transport | Pico USB MSC or SDL raw image | Add Fruit Jam microSD/FatFs transport |
| VACI RAM payload installation | Pico runner | Share only when Pico and Fruit Jam consume it |
| VCFFA1 | Separate Pico and SDL compatibility code | Disabled initially |

## Gaps to resolve before target completion

1. The build recognizes only `pico` and `sdl`; its on-device branch also
   hard-codes RP2040 TinyUSB and PicoDVI assumptions.
2. The managed SDK baseline is now 2.3.0, but the official Fruit Jam board
   selection and minimal diagnostic probe have not been exercised.
3. `neo1_soft_runner` still installs an SDL BRK-recovery jump into
   `$0000-$0002`. A CPU runner must not silently alter shared machine memory.
4. The checked-in `fake65c02` dependency is process-global, has unresolved
   provenance, and lacks a broad, focused W65C02 compatibility suite.
5. Pico's HID report decoding is embedded in RP2040 USB lifecycle code even
   though Fruit Jam needs the same key-to-Apple-1 translation over a different
   host transport.
6. Pico's FatFs file backend is reusable in concept but process-global, while
   the checked-in `diskio.c` is specifically a USB-MSC transport.
7. No selected, pinned, licensed GPIO/PIO USB-host dependency exists in this
   repository.

These are checkpoint boundaries, not reasons to create one broad platform API.

## Initial scope

The first useful Fruit Jam release will provide:

- Neo1-23 and Neo1-50 through the shared software runner;
- WozMon reset and execution;
- 40x24 DVI text output through HSTX;
- USB keyboard input through the onboard USB-A hub;
- VACI file service on a disposable FAT-formatted microSD card;
- concise diagnostics through a selected USB-C or UART transport;
- the same shared Apple-1 and MSC register contracts as existing targets.

The following are explicitly deferred:

- VCFFA1;
- audio, cassette waveform emulation, Wi-Fi, mouse, gamepad, and networking;
- SDIO and PSRAM optimization;
- snapshots and multiple concurrent software CPUs;
- a shared SDL/Fruit Jam event-loop or universal platform HAL;
- changes to the Neo6502 physical W65C02 runner.

Audio should not be added merely because the board contains a codec. It needs a
defined 6502-visible behavior or application requirement first.

## Cross-check required at every checkpoint

Each completed checkpoint must:

1. preserve all focused host tests;
2. build SDL-23 and SDL-50 and reach WozMon headlessly;
3. build Pico-23 and Pico-50;
4. identify any affected 6502-visible addresses;
5. keep completed work in separately reviewable commits;
6. restore the normal Neo1-23 Pico and SDL build directories;
7. state the exact Fruit Jam and Neo6502 physical tests still required.

An SDK, shared machine, shared device, input-decoder, FatFs adapter, or
generated-payload change also requires the relevant Neo6502 smoke test. A
Fruit Jam-only renderer or transport change does not require Neo6502 hardware
testing when existing builds and host contracts remain unchanged.

## Checkpoint 0: SDK 2.3.0 and board-support gate

Status: existing-target SDK gate complete on 2026-08-27; Fruit Jam probe
outstanding

### Boundary

Upgrade the supported Pico SDK baseline from 2.1.0 to 2.3.0 as an independent
change. Do not add the Neo1 Fruit Jam runner in the same commit.

Verify:

- the VS Code Pico extension installs/selects SDK 2.3.0 consistently;
- `PICO_BOARD=olimex_neo6502` still finds Neo1's project-carried board header;
- `PICO_BOARD=adafruit_fruit_jam` selects an RP2350 ARM platform from the
  official SDK definition;
- the toolchain and picotool versions required by the extension are recorded;
- the checked-in SDK fallback policy is explicit rather than accidentally
  using a different SDK than VS Code.

### Acceptance gate

- All host tests pass.
- SDL-23 and SDL-50 remain unchanged.
- Normal and diagnostic Pico-23 and Pico-50 build with SDK 2.3.0.
- A normal Neo1-23 hardware gate confirms reset, DVI, serial, keyboard, VACI,
  and the verified VCFFA1 read workflow.
- A minimal upstream-style Fruit Jam probe builds, flashes, identifies the
  board over diagnostics, and does not yet contain Neo1 machine code.

### Evidence to date

- The Pico extension selected SDK 2.3.0 and picotool 2.3.0 while retaining Arm
  GNU Toolchain 13.3.Rel1.
- Normal and diagnostic Neo1-23 and Neo1-50 configured and built against SDK
  2.3.0 using Neo1's project-carried `olimex_neo6502.h`.
- Both SDL profiles built, all twelve host tests passed, Neo1-50 reached WozMon
  headlessly, and the generated VACI payload check passed.
- The user supplied a passing normal Neo1-23 hardware result on 2026-08-27.
- The minimal Fruit Jam board probe remains the only open checkpoint-0 gate.

### Rollback

Do not retain the SDK upgrade if the existing Neo6502 baseline regresses or the
extension and command-line workflows select different SDKs.

## Checkpoint 1: software-runner readiness

### Boundary

Qualify the CPU path before making it a second hardware target:

- move the `$0000-$0002` BRK-recovery patch out of `neo1_soft_runner`;
- preserve it explicitly in SDL only if current SDL behavior still requires it;
- start Fruit Jam without a hidden memory patch;
- add focused tests for reset/vector fetch, IRQ masking and RTI, NMI, BRK,
  decimal ADC/SBC flags, and the W65C02 instructions actually used by supported
  ROMs and utilities;
- audit instruction and cycle-count coverage against the selected core;
- make an evidence-backed retain-or-replace decision for `fake65c02` and record
  its license/provenance before a distributable Fruit Jam release.

Replacing the CPU core, if required, is its own checkpoint. It must not be
combined with RP2350 bring-up.

### Acceptance gate

- The software runner performs no machine-memory initialization.
- WozMon, keyboard/display, decimal mode, interrupt, and selected W65C02 tests
  pass on the host.
- Both SDL profiles still reach WozMon without target-specific PIA or memory
  exceptions.
- Both Pico profiles remain buildable and behaviorally untouched.

### Rollback

Revert if SDL needs a new shared-machine exception, ROM behavior changes, or
the CPU dependency decision cannot be supported by tests and provenance.

## Checkpoint 2: Fruit Jam serial skeleton

### Boundary

Add `systems/neo1-fruitjam/` and explicit `fruitjam` CMake presets with a
separate `build-fruitjam/` directory. The target owns one `neo1_machine_t`, one
`neo1_soft_runner_t`, elapsed-time scheduling, reset/lifecycle, and a concise
diagnostic transport.

Start with video, keyboard, MSC, VACI, and VCFFA1 disabled. Do not copy Pico's
physical W65C02 runner, GPIO latch code, PicoDVI code, USB-MSC disk I/O, or
Neo1-50 entry-stub policy.

### Acceptance gate

- Fruit Jam Neo1-23 and Neo1-50 configurations compile for RP2350 ARM.
- On hardware, reset fetches `$FFFC/$FFFD`, execution reaches `$FF00`, and the
  WozMon prompt is observed through the selected diagnostic transport.
- Ctrl-R or a documented board reset action reliably repeats the reset path.
- The runner neither patches RAM nor changes shared machine semantics.
- Existing SDL and Pico gates remain green.

### Rollback

Revert if target setup leaks RP2350 conditionals into the shared machine or if
the target cannot demonstrate the reset-vector-to-WozMon path independently of
video and storage.

## Checkpoint 3: HSTX DVI text output

### Boundary

Add a Fruit Jam-only HSTX renderer that consumes snapshots of the shared 40x24
terminal grid. Begin from the official Raspberry Pi HSTX DVI example and
preserve its license notice. Use bounded scanline or text buffers in internal
SRAM; do not introduce PSRAM solely for a full-color framebuffer.

Choose and document Fruit Jam's output-byte policy. The recommended starting
policy is the verified physical Neo6502 behavior: CR newline, form-feed clear,
printable glyph output, and no SDL-only backspace behavior. Share that policy
with Pico only if the extraction has two real consumers and preserves the Pico
terminal tests byte-for-byte.

### Acceptance gate

- WozMon appears on DVI with a stable 40x24 layout.
- Clear, wrap, scroll, cursor, and sustained-output tests pass visually.
- Video DMA/interrupt work does not starve software-CPU execution or corrupt
  the terminal snapshot.
- Diagnostic output and repeated reset remain operational.
- No PicoDVI source or configuration is used by Fruit Jam.

### Rollback

Revert if video requires shared-machine timing changes, an unbounded
framebuffer, or target-specific state inside `neo1_terminal`.

## Checkpoint 4: USB keyboard through the onboard hub

### Boundary

Select and pin the GPIO/PIO USB-host dependency and document its license,
TinyUSB compatibility, required clocking, and RP2350 support. Keep transport
initialization Fruit Jam-owned.

Extract Pico's HID keyboard-report-to-byte logic into a small pure C decoder
only when both Pico and Fruit Jam consume it. The decoder may own key rollover,
Shift, Ctrl-letter, Return, Backspace, Tab, and Space translation; it must not
own TinyUSB tasks, mount callbacks, GPIO, or machine state. Both targets feed
decoded bytes to `neo1_machine_key_down()`.

### Acceptance gate

- A keyboard connected through either USB-A hub port reaches WozMon.
- Uppercase, shifted punctuation, Return, Backspace, and Ctrl-R behave as
  documented.
- Disconnect/reconnect does not wedge the CPU or video loop; any unsupported
  hot-plug behavior is recorded explicitly.
- Native USB diagnostics and GPIO/PIO host operation coexist without callback
  or TinyUSB configuration collisions.
- Pico keyboard behavior and its hardware smoke remain unchanged after any
  decoder extraction.

### Rollback

Revert if Fruit Jam host support requires target conditionals in the decoder,
breaks diagnostic USB, or changes the verified Neo6502 key translation.

## Checkpoint 5: microSD FatFs and VACI

### Boundary

Use the onboard microSD card as Fruit Jam's VACI media. Start with SPI and a
target-owned FatFs `diskio` transport. Do not adapt Pico's USB-MSC `diskio.c`
with board conditionals.

Once Fruit Jam is the second FatFs consumer:

- make the FatFs named-file MSC backend reusable and instance-owned;
- keep Pico USB-MSC and Fruit Jam SD-SPI sector transports separate;
- move the exact VACI payload installation/profile-boundary patch into a shared
  RAM-tool helper only if Pico and Fruit Jam both consume it;
- preserve the shared `$D014-$D01C` protocol without target conditionals;
- keep VCFFA1 disabled on Fruit Jam.

### Acceptance gate

Use a disposable FAT-formatted microSD card and verify:

1. directory listing and cancel/return;
2. load and run of a known `.BIN`;
3. exact 16-byte write, host size check, reload, and byte comparison;
4. multi-sector write and truncating overwrite;
5. BASIC `S`/`L` snapshot round trip;
6. missing, read-only, invalid-range, short-I/O, and removal paths without a
   CPU hang;
7. recovery behavior after reinsertion or reset, documented exactly;
8. simultaneous DVI and USB keyboard operation during file activity.

The host MSC fixtures must continue to exercise the same register-level error
paths. Any shared FatFs adapter extraction also requires the existing
Neo6502 disposable-media smoke.

### Rollback

Revert if the SD transport changes VACI's 6502-visible status/error contract,
duplicates the MSC register state machine, or regresses Pico USB storage.

## Checkpoint 6: profile parity and target hardening

### Boundary

Complete both profiles without inventing Fruit Jam-only machine semantics.
Neo1-23 must expose its protected `$E000-$FFFF` ROM. Neo1-50 must retain
writable `$E000-$FEFF`; any convenience entry stubs are an explicit target
policy and must not enter the shared profile or machine.

Measure CPU scheduling, DVI servicing, USB polling, and SD latency under load.
Optimize only demonstrated bottlenecks. PSRAM, SDIO, the second core, and higher
clock rates remain optional responses to evidence, each in a separate change.

### Acceptance gate

- Both Fruit Jam profiles build and pass reset, WozMon, memory
  deposit/examine, DVI, keyboard, scrolling, and VACI gates.
- Neo1-23 enters Integer BASIC at `$E000` and Krusader at `$F000`.
- Neo1-50 preserves its writable load ranges and ROM protection.
- A sustained combined CPU/video/input/storage test shows no corruption or
  unbounded scheduling backlog.
- Existing target and host regression matrices remain green.

### Rollback

Revert an optimization if it changes 6502-visible timing assumptions, profile
memory policy, or makes correctness depend on overclocking.

## Checkpoint 7: documentation and release baseline

### Boundary

After physical gates pass:

- update `README.md` with Fruit Jam SDK 2.3.0 preset/build/flash instructions;
- add the target and verified ownership boundaries to `docs/architecture.md`;
- record exact hardware, board revision, media, known defects, and validation
  dates in `docs/current-state.md`;
- keep this document as the dated execution evidence;
- create a completion tag only from a clean, tested worktree.

### Acceptance gate

A new contributor can trace reset, one memory read/write, keyboard input,
display output, ROM protection, and a VACI command without following SDL or
Neo6502 framework internals. Documentation must not claim VCFFA1, audio,
hot-plug, PSRAM, SDIO, or Wi-Fi behavior that was not tested.

## Commit boundaries

Keep at least these concerns separate:

1. SDK/toolchain upgrade;
2. software-CPU tests or core replacement;
3. removal of the runner-owned RAM patch;
4. Fruit Jam target skeleton;
5. HSTX renderer;
6. HID decoder extraction;
7. GPIO/PIO USB-host transport;
8. FatFs adapter extraction;
9. SD-SPI transport;
10. VACI installation and storage behavior;
11. performance changes;
12. documentation and milestone evidence.

Do not combine generated 6502 payload changes, CPU-core replacement, SDK
upgrade, target bring-up, and physical I/O work in one commit.

## Recommended starting point

Begin with checkpoint 0 only: upgrade and validate SDK 2.3.0 across the current
targets, then prove that the official Fruit Jam board definition can build and
run a minimal diagnostic probe. Checkpoint 1 follows before Neo1 Fruit Jam
target code because the current software runner's hidden `$0000-$0002` patch
and unqualified CPU dependency should not become a second hardware target's
accidental contract.

## Upstream references

- [Adafruit Fruit Jam pinout and hardware guide](https://learn.adafruit.com/adafruit-fruit-jam/pinout)
- [Adafruit Fruit Jam PCB and schematic repository](https://github.com/adafruit/Adafruit-Fruit-Jam-PCB)
- [Raspberry Pi Pico SDK releases](https://github.com/raspberrypi/pico-sdk/releases)
- [Raspberry Pi RP2350 HSTX DVI example](https://github.com/raspberrypi/pico-examples/tree/master/hstx/dvi_out_hstx_encoder)
- [Adafruit Fruit Jam SDIO guide](https://learn.adafruit.com/adafruit-fruit-jam/sdio-usage)
