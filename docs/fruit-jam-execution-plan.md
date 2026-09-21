# Neo1 Fruit Jam Execution Plan

Date: 2026-08-26

Last updated: 2026-09-20

Status: checkpoints 0 through 2A complete; checkpoint 3 is next

Baseline: `neo1-portable-core-complete-2026-08-26`

## Decision

Adafruit Fruit Jam is the next logical platform target, provided it is built as
a second consumer of the software-CPU path established by SDL. It must not be a
port of the Neo6502 physical-bus runner and must not turn SDL's mixed local
interface into a speculative universal HAL.

`systems/neo1-fruitjam/` is now a thin RP2350B runner around the shared machine
and `neo1_soft_runner`. It owns timing, USB-CDC diagnostics and console input,
and lifecycle. Later checkpoints add Fruit Jam-owned HSTX video, GPIO/PIO
USB-host transport, and microSD transport. The shared machine continues to own
all 6502-visible memory and device behavior.

The completed foundation established Pico SDK 2.3.0 compatibility and qualified
the software CPU without copying the Neo6502 RP2040 runner. Fruit Jam has no
physical W65C02 bus, uses HSTX rather than PicoDVI, uses GPIO-based USB host
rather than the RP2040 native-host arrangement, and uses onboard microSD rather
than USB mass storage for its preferred media.

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
| Software 65C02 execution and timing budget | `neo1_soft_runner` | Reused by the current serial target |
| 40x24 character cells and scrolling | `neo1_terminal` | Attach in checkpoint 3 |
| `$D014-$D01C` MSC protocol | `neo1_msc` | Attach in checkpoint 5 |
| Pixel rendering and output-byte policy | Pico/SDL target code | Add Fruit Jam HSTX implementation |
| Keyboard transport | Fruit Jam USB-CDC console, Pico TinyUSB, or SDL events | Add Fruit Jam GPIO/PIO USB host in checkpoint 4 |
| Filesystem transport | Pico USB MSC or SDL raw image | Add Fruit Jam microSD/FatFs transport |
| VACI RAM payload installation | Pico runner | Share only when Pico and Fruit Jam consume it |
| VCFFA1 | Separate Pico and SDL compatibility code | Disabled initially |

## Gap disposition

| Original gap | Status | Current disposition |
| --- | --- | --- |
| Build and target selection | Resolved | `fruitjam` is a first-class platform with isolated presets and TinyUSB configuration. |
| Fruit Jam board and Neo1 runner | Resolved | Both personalities build and run on the official SDK board definition. |
| Process-global `fake65c02` dependency | Resolved | The instance-owned, MIT-licensed qe6502 runner passed the recorded compatibility gates. |
| Reusable HID report decoding | Open | Address in checkpoint 4 only when Pico and Fruit Jam are concrete consumers. |
| Reusable FatFs named-file backend | Open | Address in checkpoint 5 while keeping target transports separate. |
| GPIO/PIO USB-host dependency | Open | Select, pin, license, and qualify it in checkpoint 4. |

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
6. restore the normal Neo1-23 Pico, SDL, and Fruit Jam build directories;
7. state the exact Fruit Jam and Neo6502 physical tests still required.

An SDK, shared machine, shared device, input-decoder, FatFs adapter, or
generated-payload change also requires the relevant Neo6502 smoke test. A
Fruit Jam-only renderer or transport change does not require Neo6502 hardware
testing when existing builds and host contracts remain unchanged.

## Checkpoint 0: SDK 2.3.0 and board-support gate

Status: complete on 2026-09-01

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
- The standalone probe configured the official `adafruit_fruit_jam` board as
  `rp2350-arm-s`, built with SDK 2.3.0 and Arm GNU Toolchain 13.3.Rel1, and
  produced an RP2350 ARM Secure UF2 identified by picotool 2.3.0.
- The UF2 flashed successfully to physical Fruit Jam hardware and rebooted
  into USB CDC diagnostics. The transcript confirmed RP2350B, 16 MB flash,
  8 MB PSRAM, the official HSTX/USB-host/SD pin constants, and a sustained
  heartbeat. Picotool identified the tested chip as RP2350 revision A4 in the
  QFN80 package.
- The probe lives under `tools/fruit-jam-probe/` and contains no Neo1 machine
  code. All checkpoint-0 gates are complete.

### Rollback

Do not retain the SDK upgrade if the existing Neo6502 baseline regresses or the
extension and command-line workflows select different SDKs.

## Checkpoint 1: software-runner readiness

Status: qualification complete on 2026-09-01; checkpoint 1A replacement is
required before checkpoint 2.

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

### Evidence and decision

- `neo1_soft_runner` performs no machine-memory initialization. SDL now owns
  its historical Neo1-50 `$0000-$0002` BRK-to-RESET jump explicitly.
- IRQ masking, seven-cycle IRQ/NMI entry, RTI, BRK stack/return behavior,
  reset-vector execution, decimal ADC/SBC flags, and the `STZ`, `PHX/PHY`, and
  `PLX/PLY` forms used by Neo1's RAM utilities have focused black-box tests.
- The decimal `99 + 01` case exposed an incorrect overflow flag in fake65c02;
  a focused local correction now passes the W65C02 expectation.
- All thirteen host tests pass, both SDL profiles reach WozMon headlessly, and
  both Pico profiles build with SDK 2.3.0. Both build directories are restored
  to normal Neo1-23. Because Pico does not compile the software runner or
  fake65c02, this checkpoint does not require a new Neo6502 physical gate.
- Neo1's imported header was traced to archived
  [MyLittle6502 commit `2684fba`](https://github.com/C-Chads/MyLittle6502/tree/2684fbaab72162110aa959787053f72304689547).
  Before the decimal correction, it matched the upstream file after CRLF
  normalization except for Neo1's missing-semicolon fix. The upstream notice
  claims public-domain/CC0 status but also identifies incorporated
  non-public-domain Commander X16 work and leaves that license question open.
- Decision: do not make fake65c02 a Fruit Jam dependency. Keep it temporarily
  for SDL, then replace it in checkpoint 1A with an instance-owned core having
  an explicit license, pinned source revision, and broad W65C02 test evidence.

### Rollback

Revert if SDL needs a new shared-machine exception, ROM behavior changes, or
the CPU dependency decision cannot be supported by tests and provenance.

## Checkpoint 1A: software CPU replacement

Status: complete on 2026-09-01.

### Boundary

Select and pin a clearly licensed, instance-owned W65C02 core before adding the
Fruit Jam runner. Adapt `neo1_soft_runner` without changing the shared machine
or SDL platform interface, then remove fake65c02 and its global callback/state
constraint in a separate commit.

Candidate evaluation must cover:

- ordinary C/C++ suitability for macOS/Linux and RP2350 ARM builds;
- explicit per-instance registers, IRQ/NMI inputs, and memory or bus access;
- W65C02 rather than NMOS-6502 opcode and decimal semantics;
- instruction-cycle reporting suitable for Neo1's elapsed-time scheduler;
- an unambiguous license and a pinned upstream revision;
- published or reproducible Klaus Dormann 6502, decimal, interrupt, and 65C02
  extended-opcode results.

### Candidate decision

| Candidate | Evidence-backed fit | Decision |
| --- | --- | --- |
| [qe6502](https://github.com/nnqe/qe6502) | MIT, C11, explicit WDC model, 16-byte caller-owned state, no allocation or mutable global CPU state, and one visible bus request per tick | Select version 1.0.0 at commit `8ae9074203e0a6c46ae687e19194c6a3f4bc1d07` |
| [vrEmu6502](https://github.com/visrealm/vrEmu6502) | MIT, C99, WDC model, multiple CPU objects, and strong bundled Klaus tests | Do not select: memory callbacks have no caller context, CPU construction allocates opaque state, and its aggregate interrupt timing needs adapter correction |
| [C99-6502](https://github.com/MercuriusDream/C99-6502) | C99 and CMOS instruction support | Do not select: AGPL-3.0 and a larger emulator-owned bus/memory framework are a poorer dependency boundary |
| [redcode/6502](https://github.com/redcode/6502) | Mature ANSI C, caller context, explicit callbacks, and per-instance state | Do not select: it implements the NMOS 6502 rather than the required W65C02 |

qe6502 represents the physical component directly: the runner owns a CPU value
and services explicit address, data, read/write, opcode-fetch, reset, IRQ, and
NMI cycles through `neo1_machine_read()` and `neo1_machine_write()`. This is a
better fit than an instruction callback API and does not introduce a universal
emulator framework into the shared machine.

The pin is the `v1.0.0` release commit. The evaluated upstream `main` revision
was `7af43cec306de72459e0b71d6587066876afaa3a`; only README changes separate it
from the selected release, so the tested CPU sources are byte-identical to the
pin.

### Selection evidence

- The complete selected source is MIT-licensed, has 373 commits beginning in
  2025, and identifies one copyright holder/contributor identity.
- The upstream native build completed with AppleClang 17. The WDC Klaus
  functional and extended-opcode tests passed at more than 300 emulated MHz on
  the development host.
- Twelve selected upstream functional, save/load, netlist, and interrupt
  lockstep tests passed. The interrupt lockstep suite exercises NMOS timing;
  Neo1's unchanged WDC-focused IRQ/NMI tests remain the integration gate.
- The upstream SingleStep harness passed all 2,540,000 available cases from
  [SingleStepTests/65x02 commit `2f6980a`](https://github.com/SingleStepTests/65x02/tree/2f6980a2d95757486c7bee24355c360e40e2a224/wdc65c02/v1).
  The comparison covered final registers, memory, and every expected bus cycle
  for all 256 opcode files; the WAI (`$CB`) and STP (`$DB`) files contain no
  vectors, while the other 254 files contain 10,000 cases each. The Klaus
  extended suite separately reaches the WDC extended instruction surface.
- The unmodified qe6502 static C core configured and linked for
  `adafruit_fruit_jam` as `rp2350-arm-s` with Pico SDK 2.3.0 and Arm GNU
  Toolchain 13.3.Rel1. The minimal compile-check image used 105,740 bytes of
  text; its 66,320-byte BSS included the harness's 64 KB test memory rather
  than CPU-owned memory.
- vrEmu6502 revision `aae98cb14386d832cb7357c99626520b6590bc24` also
  compiled for RP2350 and passed all eleven bundled tests, but its callback and
  interrupt-adapter costs make it the runner-up rather than the selection.

### Integration sequence

1. Vendor qe6502's license, native C headers, source, and control-store files at
   the pinned release without formatting or semantic edits. Record the upstream
   URL, tag, commit, and local verification commands beside the dependency.
2. Add a static CMake target consumed only by software runners. Do not expose
   qe6502 types through `neo1_machine` or platform interfaces.
3. Make `neo1_soft_runner_t` own `qe6502_t` and its pending bus request. Service
   each read/write tick through the ordinary machine interface and remove the
   active-runner callback bridge and single-instance restriction.
4. Preserve the public runner contract: reset completes through the real reset
   sequence without charging later instruction budgets, NMI remains one latched
   edge, IRQ remains a level, and `neo1_soft_runner_step()` continues to return
   the represented cycles expected by the checkpoint-1 tests.
5. Run the unchanged Neo1 CPU contract, all host tests, both SDL WozMon smokes,
   both Pico builds, the pinned qe6502 Klaus suites, and the pinned WDC
   SingleStepTests harness before deleting fake65c02.
6. Keep dependency import, runner adaptation, fake65c02 removal, and milestone
   documentation in reviewable commits. No Fruit Jam platform code belongs in
   this checkpoint.

### Acceptance gate

- The checkpoint-1 CPU contract passes against the replacement, with IRQ/NMI
  expectations corrected for W65C02 recognition after the opcode already on
  the bus rather than fake65c02's immediate-entry behavior.
- The upstream broad functional, decimal, interrupt, and W65C02 suites pass in
  a reproducible Neo1-owned test harness or documented upstream harness.
- Both SDL profiles reach WozMon and retain keyboard/display behavior.
- Both Pico profiles build unchanged.
- fake65c02, its global callbacks, and the single-active-runner restriction are
  removed with provenance and license records preserved in history.

### Completion evidence

- qe6502 is pinned as a Git submodule at the selected release, with its MIT
  license and upstream/test record retained beside the dependency.
- `neo1_soft_runner_t` owns the 16-byte CPU state and pending bus request.
  Focused coverage executes two independent runners and proves reset does not
  patch either machine's RAM.
- The VACI payload fixture also runs on qe6502; no active source or test includes
  fake65c02, and the legacy header has been removed.
- All thirteen Neo1 host tests passed for both SDL configurations. Neo1-23 and
  Neo1-50 each emitted the WozMon `\` prompt through the headless stdout path;
  the existing offscreen OpenGL warning and storage self-test failure remained
  unchanged.
- Normal Neo1-23 and Neo1-50 Pico images built with SDK 2.3.0 and Arm GNU
  Toolchain 13.3.Rel1. This software-only checkpoint does not require a new
  Neo6502 physical test. Both working build directories were restored to their
  normal Neo1-23 presets.

### Rollback

Do not adopt a core with unclear licensing, global architectural state,
missing W65C02 operations, or timing semantics that require machine-memory or
PIA exceptions.

## Checkpoint 2: Fruit Jam serial skeleton

Status: complete on 2026-09-01.

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

### Completion evidence

- The `neo1-fruitjam-23-serial` and `neo1-fruitjam-50-serial` presets configure
  the official `adafruit_fruit_jam` board as `rp2350-arm-s` in the separate
  `build-fruitjam/` tree. Both compile with SDK 2.3.0 and Arm GNU Toolchain
  13.3.Rel1.
- The target owns one static 64 KB machine, one qe6502 runner, monotonic-time
  scheduling, USB-CDC character output, and serial Ctrl-R lifecycle reset. It
  attaches no storage device and installs no target-specific RAM contents.
- Both profiles were flashed on Fruit Jam `21A41F42391B99FA`. Neo1-23 and
  Neo1-50 each reported `reset=$FF00 entry=$FF00`, emitted the WozMon `\`
  prompt, and repeated the same sequence after Ctrl-R.
- Link/source inspection confirms video, Apple-1 input, MSC, VACI, VCFFA1,
  audio, PicoDVI, the physical W65C02 runner, and Neo1-50 entry stubs are absent.
- All thirteen host tests pass, both SDL profiles reach WozMon headlessly, and
  both Pico profiles build with SDK 2.3.0. The Neo6502 TinyUSB host
  configuration now resides in its owning target directory so it cannot
  override Fruit Jam's USB-device CDC configuration. All three working build
  directories were restored to Neo1-23.

### Rollback

Revert if target setup leaks RP2350 conditionals into the shared machine or if
the target cannot demonstrate the reset-vector-to-WozMon path independently of
video and storage.

## Checkpoint 2A: serial WozMon interaction

Status: complete on 2026-09-01.

### Boundary

Use USB-CDC as a temporary target-local console input transport before HSTX
video and USB-host input are added. Convert lowercase ASCII to uppercase,
accept CR, LF, and CRLF as one Return, retain input while the one-byte PIA latch
is occupied, and keep Ctrl-R reserved for target lifecycle reset. Do not change
the shared PIA or introduce a shared input abstraction for this single consumer.

### Acceptance gate

- Both Fruit Jam profiles compile without changing the shared machine or CPU.
- Lowercase serial commands can deposit and examine memory in WozMon.
- CR, LF, and CRLF terminal modes each submit exactly one command.
- Ctrl-R still resets the shared PIA and software CPU.
- Existing host, SDL, and Pico regression gates remain green.

### Completion evidence

- Both Fruit Jam profiles compiled with SDK 2.3.0 and Arm GNU Toolchain
  13.3.Rel1. All thirteen host tests passed, and both Pico profiles built.
- Neo1-23 was flashed to Fruit Jam `21A41F42391B99FA`. A single lowercase
  console burst deposited `AA` at `$0300` and examined it successfully, proving
  ASCII normalization, PIA delivery, WozMon execution, and input backpressure.
- Additional bursts deposited and examined `BB` at `$0301` using bare LF and
  `CC` at `$0302` using CRLF. Each line ending submitted one command.
- Ctrl-R after the interaction again reported `reset=$FF00 entry=$FF00` and
  returned to the WozMon prompt.
- A 2026-09-02 physical follow-up found that WozMon's CR-only output returned
  the cursor to column zero without advancing the USB-CDC terminal. Expanding
  outgoing CR to CRLF in the target callback corrected line presentation;
  WozMon, Integer BASIC, and Krusader then worked interactively on Neo1-23.
- The Pico, SDL, and Fruit Jam working build directories were restored to their
  Neo1-23 presets. The Fruit Jam remains flashed with Neo1-23.

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

## Checkpoint 5A: repository consolidation and documentation

### Boundary

After HSTX, USB-host keyboard, and microSD/VACI establish the target's actual
ownership boundaries, perform a dedicated repository-structure and
documentation pass before profile hardening. This checkpoint may move, rename,
archive, or remove files only when their owner and replacement are proven by
the three working targets. It must not introduce new machine behavior, a
speculative platform HAL, or performance changes.

The pass should:

1. inventory active source, generated artifacts, hardware probes, tests, plans,
   and historical evidence with their current owners;
2. separate current documentation from superseded plans and dated test logs;
3. decide whether bring-up tools such as `tools/fruit-jam-probe/` remain useful,
   should be archived as evidence, or can be removed;
4. consolidate 6502-side payload sources, generators, and checked-in outputs
   without mixing them into platform code;
5. normalize file and directory names where the established architecture makes
   ownership unambiguous;
6. remove dead compatibility code and stale inline claims only after confirming
   that no build, test, or documented workflow consumes them; and
7. add or refresh a concise documentation map for new contributors.

Keep file moves, documentation changes, dead-code removal, and any necessary
build-path adjustments in reviewable commits. Preserve attribution, licenses,
and historical evidence rather than rewriting old records as current claims.

### Acceptance gate

- Pico, SDL, and Fruit Jam both-personality builds remain green.
- All focused host tests pass from the consolidated layout.
- No 6502-visible address, protocol, ROM/RAM policy, or target behavior changes.
- Current documents agree on supported targets, completed checkpoints, known
  defects, and the next implementation step.
- Historical documents are clearly labeled, and active instructions do not
  depend on superseded plans.
- The worktree contains no unexplained duplicate implementation or orphaned
  generated artifact discovered by the inventory.

### Rollback

Revert a move or deletion if ownership is still ambiguous, a historical or
license record would be lost, a target needs compatibility forwarding solely
because of the reorganization, or behavioral fixes become entangled with the
cleanup.

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
11. repository consolidation and documentation structure;
12. performance changes;
13. documentation and milestone evidence.

Do not combine generated 6502 payload changes, CPU-core replacement, SDK
upgrade, target bring-up, and physical I/O work in one commit.

## Recommended next step

Begin checkpoint 3 with a Fruit Jam-only HSTX DVI text renderer fed by the
shared terminal grid. Preserve serial diagnostics and reset while keeping
USB-host keyboard input, storage, VACI, VCFFA1, and audio disabled.

## Upstream references

- [Adafruit Fruit Jam pinout and hardware guide](https://learn.adafruit.com/adafruit-fruit-jam/pinout)
- [Adafruit Fruit Jam PCB and schematic repository](https://github.com/adafruit/Adafruit-Fruit-Jam-PCB)
- [Raspberry Pi Pico SDK releases](https://github.com/raspberrypi/pico-sdk/releases)
- [Raspberry Pi RP2350 HSTX DVI example](https://github.com/raspberrypi/pico-examples/tree/master/hstx/dvi_out_hstx_encoder)
- [Adafruit Fruit Jam SDIO guide](https://learn.adafruit.com/adafruit-fruit-jam/sdio-usage)
- [qe6502 1.0.0 source pin](https://github.com/nnqe/qe6502/tree/8ae9074203e0a6c46ae687e19194c6a3f4bc1d07)
- [SingleStepTests WDC65C02 vectors](https://github.com/SingleStepTests/65x02/tree/2f6980a2d95757486c7bee24355c360e40e2a224/wdc65c02/v1)
