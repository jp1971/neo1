# Neo1 Portable-Core Execution Plan

Started: 2026-08-24

Baseline tag: `neo1-portable-core-baseline-2026-08-24`

Status: completed 2026-08-26 through checkpoint 9.

This plan records reversible extraction checkpoints and their evidence. Stable
6502-visible contracts remain in `docs/architecture.md`; verified runtime state
and defects remain in `docs/current-state.md`.

## Checkpoint 1: shared terminal grid

Status: completed 2026-08-24.

### Boundary

Extract the duplicated 40×24 character-grid state and its primitive mutations
into ordinary shared C code under `src/terminal/`:

- clear all cells and reset the cursor;
- advance to the next row, scrolling when required;
- place one caller-approved glyph, wrapping at column 40;
- erase one glyph to the left without crossing a row boundary.

This grid represents host-side display state, not the Apple-1 PIA and not a
pixel/display transport. It has two concrete consumers: the Neo6502 runner's
PicoDVI path and the SDL runner's renderer.

### Preserved target ownership

- Pico retains high-bit stripping, CR as newline, form feed as clear, ignored
  LF/backspace, UART echo, DVI publication, rasterization, and cursor blink.
- SDL retains high-bit stripping, CR as newline, ignored LF/form feed,
  backspace erase, direct SDL rendering, geometry, and cursor blink.
- Neither target gains a new control-byte conditional in the shared machine.
- No 6502-visible address, PIA behavior, ROM/RAM policy, CPU integration,
  storage protocol, or physical bus timing changes in this checkpoint.

### Acceptance criteria recorded before implementation

Focused host tests must prove:

1. clear produces 40×24 spaces and cursor `(0,0)`;
2. glyph placement advances the cursor and column 40 wraps exactly once;
3. newline resets column to zero, and newline on row 23 scrolls exactly once;
4. scrolling preserves rows 1–23 in rows 0–22 and blanks the final row;
5. backspace erases only when the cursor is right of column zero;
6. the shared glyph primitive preserves all eight bits, leaving target callbacks
   responsible for high-bit stripping;
7. Pico policy preserves CR/form-feed behavior and ignores LF/backspace;
8. SDL policy preserves CR/backspace behavior and ignores LF/form feed.

Build and runtime gates:

- all focused host tests pass without initializing SDL;
- Neo1 SDL personalities 23 and 50 build;
- a headless SDL smoke reaches WozMon for both personalities;
- Neo1 Pico personalities 23 and 50 build using SDK 2.1.0;
- the working Pico and SDL build directories are restored to personality 23;
- diff review shows no changes to physical W65C02 bus, PicoDVI scanout,
  keyboard injection, storage, memory decoding, or PIA code.

### Physical gate

After the host/build gates pass, flash the normal Neo1-23 image and confirm:

1. reset reaches WozMon on DVI and serial;
2. `0000.0FFF` wraps and scrolls without corrupt rows or DVI instability;
3. enter `D012:0C` and confirm form feed clears the DVI terminal, then confirm
   the cursor continues blinking;
4. UART and USB keyboard input still work;
5. `C100R`, followed by `Q`, returns cleanly to WozMon.

The user confirmed that this complete physical gate passed on 2026-08-24.

### Evidence to date

- `neo1_terminal_grid_contract` passes with clear, wrap, scroll, backspace,
  eight-bit glyph, Pico-policy, and SDL-policy fixtures.
- All six host tests pass under the SDL-23 configuration.
- SDL personalities 23 and 50 build and reach the WozMon `\` prompt with a
  disposable raw-image startup probe.
- Pico personalities 23 and 50 build with SDK 2.1.0; `build/` is restored to
  Neo1-23 and `build-sdl/` is restored to SDL-23.
- Diff review found no change to memory/PIA decoding, CPU integration, physical
  bus timing, storage, PicoDVI scanout, or terminal snapshot publication.
- The normal Neo1-23 image passed the physical gate: WozMon appeared on DVI and
  serial, the range dump wrapped and scrolled stably, `D012:0C` cleared the
  display with the cursor still active, UART and USB input worked, and VACI
  returned cleanly to WozMon.

### Rollback condition

Revert this checkpoint if any golden terminal state changes, either target
fails to build, SDL WozMon output changes, the Neo6502 physical smoke regresses,
or preserving behavior requires target conditionals in the shared grid.

## Checkpoint 2: explicit optional-device ports

Status: completed 2026-08-24.

### Boundary

Remove the shared machine header's direct dependency on Pico-owned MSC and
VCFFA1 backend headers and target-global I/O symbols:

- move the stable register/address contracts to shared headers under
  `src/devices/`;
- keep MSC ownership at `$D014-$D01C` conditional on `NEO1_ENABLE_MSC`;
- keep VCFFA1 ownership at `$AFDC-$AFDD` and `$AFF0-$AFFF` conditional on
  `NEO1_ENABLE_VCFFA1`;
- add explicit optional-device read/write ports to `neo1_desc_t` and retain
  those ports in the machine instance;
- have the Pico and SDL runners attach their existing implementations.

The shared machine continues to own address decode and RAM fallthrough. The
device implementations continue to own their existing register/protocol state
and backend I/O.

### Preserved behavior and non-goals

- Pico MSC remains the canonical FatFs file service used by VACI.
- SDL MSC remains its documented divergent raw-image/no-op accommodation.
- Pico and SDL VCFFA1 implementations remain separate and behaviorally
  divergent; deferred VCFFA1 reliability work is not part of this checkpoint.
- Device initialization order, synchronous command completion, status/error
  meanings, data phases, payload installation, and storage writes are unchanged.
- Disabled device addresses continue to use ordinary backing RAM.
- No CPU adapter, PIA, ROM/RAM policy, terminal, DVI, USB, FatFs, SDL renderer,
  or physical bus timing code changes.

### Acceptance criteria recorded before implementation

Focused host tests must prove:

1. enabled MSC readable/writable addresses invoke the attached MSC port once;
2. disabled MSC addresses read and write backing RAM without port calls;
3. enabled VCFFA1 signature and register addresses invoke the attached VCFFA1
   port once;
4. disabled VCFFA1 signature/register addresses use backing RAM;
5. addresses outside each device's documented ownership never reach its port;
6. builds with an enabled device require complete read and write callbacks;
7. existing MSC protocol, VACI payload, cycle-budget, and terminal tests pass.

Build and runtime gates:

- all host tests pass under SDL-23;
- SDL personalities 23 and 50 build and reach WozMon headlessly;
- Pico personalities 23 and 50 build with SDK 2.1.0;
- both working build directories are restored to personality 23;
- diff review finds no protocol-state or backend-I/O behavior changes.

### Physical gate

Using the normal Neo1-23 image and read-only workflows:

1. confirm reset reaches WozMon on DVI and serial;
2. enter `AFDC.AFDD` and confirm `$CF,$FA`;
3. enter `AFFF` and confirm VCFFA1 status remains readable;
4. enter `C100R`, list the VACI directory, then use `Q` to return to WozMon;
5. confirm USB keyboard and serial input remain responsive.

No storage write is required. The user confirmed that this complete physical
gate passed on 2026-08-24.

### Evidence to date

- Shared MSC and VCFFA1 contract headers now hold the stable addresses,
  register meanings, status/error values, and interrupt behavior.
- The shared machine owns conditional decode and calls explicitly attached
  read/write ports; neither it nor the SDL runner includes a Pico backend
  header. Snapshot loading preserves the currently attached ports.
- Enabled/disabled MSC and VCFFA1 decode tests prove exact routing and ordinary
  RAM fallthrough, including representative addresses immediately outside each
  device window. The existing MSC protocol, generated VACI payload,
  software-cycle, and terminal-grid tests also pass: eight tests total.
- SDL personalities 23 and 50 build and reach WozMon headlessly with disposable
  raw images. Pico personalities 23 and 50 build with SDK 2.1.0. The working
  Pico and SDL build directories are restored to personality 23.
- Diff review found no changes to MSC or VCFFA1 protocol state, backend I/O,
  payload installation, CPU/PIA behavior, ROM/RAM policy, DVI, USB, or physical
  bus timing. The duplicated target-side VCFFA1 address filters were removed;
  decode now has the one shared-machine owner described above.
- The normal Neo1-23 image passed the physical gate: WozMon appeared on DVI and
  serial, `AFDC.AFDD` returned the VCFFA1 signature, `AFFF` remained readable,
  VACI listed the USB directory and returned to WozMon, and USB keyboard plus
  serial input remained responsive.

### Rollback condition

Revert this checkpoint if callback routing changes any enabled/disabled address
result, either target's existing storage behavior changes, a build/smoke gate
fails, or the physical read-only storage smoke regresses.

## Checkpoint 3: shared Apple-1 keyboard/display device

Status: completed 2026-08-25.

### Boundary

Extract the minimal Apple-1 PIA-like keyboard/display state from the
transitional machine header into ordinary shared C under `src/devices/`:

- own `$D010-$D013` and the Replica 1 display mirrors `$D0F2-$D0F3` in one
  device module;
- own keyboard/display control, data-direction, and data registers plus the
  pending keyboard latch;
- return an explicit display-byte event to the machine, which retains the
  runner-provided character callback;
- have `neo1_key_down()` inject input through that device;
- give Pico and SDL the same register and latch semantics.

The physical behavior is the authority: KBD accesses select DDR or data using
KBDCR bit 2, and the first pending key remains latched until the 6502 consumes
it. This removes the SDL-only unconditional KBD data read and pending-key
replacement accommodations. It corrects the software runner instead of
changing the Apple-1-visible device for emulator convenience.

### Preserved behavior and non-goals

- KBDCR bit 7 continues to indicate a pending key; reading KBD data consumes it.
- DSP accesses continue to select DDR or data using DSPCR bit 2; data writes
  emit exactly one unmodified byte and display reads remain non-blocking.
- DSPCR reads continue to report ready in bit 7.
- LF input continues to normalize to CR and input bytes retain bit 7.
- Reset clears all PIA-like registers and pending input.
- No CPU adapter API, software-core globals, `$0000-$0002` recovery patch,
  reset signaling, ROM/RAM policy, storage, terminal policy, platform event
  loop, DVI, USB, or physical bus timing changes.

### Acceptance criteria recorded before implementation

Focused host tests must prove:

1. reset clears both data, DDR, and control registers and reports no key;
2. KBDCR bit 2 selects keyboard DDR versus latched input;
3. input normalizes LF to CR, sets bit 7, and retains the first pending byte;
4. reading KBD data consumes the latch and clears KBDCR ready;
5. DSPCR bit 2 selects display DDR versus data, with one callback event only
   for a data-register write;
6. DSP and DSPCR mirrors behave exactly like `$D012-$D013`;
7. addresses outside the six documented PIA addresses remain machine RAM;
8. existing MSC/VCFFA1 decode, protocol, VACI, cycle-budget, and terminal tests
   continue to pass.

Build and runtime gates:

- all host tests pass under SDL-23;
- SDL personalities 23 and 50 build and reach WozMon headlessly;
- Pico personalities 23 and 50 build with SDK 2.1.0;
- both working build directories are restored to personality 23;
- diff review finds no CPU, memory-policy, storage-protocol, terminal-policy,
  platform-I/O, or physical bus-timing changes.

### Physical gate

Using the normal Neo1-23 image:

1. confirm reset reaches WozMon on DVI and serial;
2. enter and execute ordinary WozMon memory examine/deposit commands, proving
   keyboard input and display output remain responsive;
3. confirm USB keyboard and serial input both work;
4. enter `C100R`, list the VACI directory, cancel the selection with Return,
   then use `Q` to return to WozMon;
5. confirm the display remains stable while the directory scrolls.

No storage write is required. The user confirmed that this complete physical
gate passed on 2026-08-25.

### Evidence to date

- `neo1_apple1_pia` is an ordinary shared C module linked by both runners. It
  owns the four Apple-1 registers, two Replica 1 display mirrors, DDR/control
  selection, keyboard latch, and display-byte event.
- The shared machine now contains that device state and has no CPU-backend
  conditional in keyboard/display decode or input injection. SDL therefore
  uses the physical first-pending-byte and KBDCR-bit-2 semantics.
- `neo1_apple1_pia_contract` proves reset state, DDR/data selection, LF
  normalization, high-bit input, first-byte retention, consume-on-read, status,
  display events, mirrors, machine routing, and RAM immediately outside the
  device addresses. All nine host tests pass.
- SDL personalities 23 and 50 build and reach WozMon headlessly with disposable
  images. Pico personalities 23 and 50 build with SDK 2.1.0. The working Pico
  and SDL build directories are restored to personality 23.
- The state-layout extraction advances the internal snapshot version from 1 to
  2 so an older layout is rejected rather than misread.
- Diff review found no change to the CPU adapter implementation, software-core
  globals, `$0000-$0002` accommodation, reset signal, RAM/ROM policy, storage,
  terminal byte policy, platform I/O, DVI, USB, or physical bus timing.
- The normal Neo1-23 image passed the physical gate: WozMon appeared on DVI and
  serial, memory examine/deposit worked, USB keyboard and serial input remained
  responsive, VACI listed the directory and returned to WozMon, and scrolling
  remained stable.

### Rollback condition

Revert this checkpoint if any PIA register result differs from the documented
physical semantics, WozMon keyboard/display behavior regresses on either
target, a build/smoke gate fails, or target-specific PIA behavior is required
inside the shared device.

## Checkpoint 4: CPU-neutral machine address space

Status: complete 2026-08-25.

### Boundary

Extract the 6502-visible address space from the transitional CPU-bearing
`neo1_t` wrapper into ordinary shared C under `src/systems/`:

- own the 64 KB backing array, deterministic power-on pattern, ROM placement,
  and ROM write-protection boundary in `neo1_machine_t`;
- own the shared Apple-1 PIA-like device and optional MSC/VCFFA1 decode there;
- expose explicit `neo1_machine_read()` and `neo1_machine_write()` bus
  operations plus keyboard injection and reset-state operations;
- select optional-device ownership from attached ports rather than compiling
  CPU-dependent address-space variants;
- have the existing physical/software CPU wrapper service both adapters through
  that explicit machine surface.

This establishes the portable machine boundary without moving either CPU in
the same checkpoint. The transitional wrapper may still own CPU selection,
execution budgeting, reset signaling, startup tracing, and snapshots.

### Preserved behavior and non-goals

- Both profiles retain the even `$00`/odd `$FF` initial RAM pattern, ROM bytes,
  reset vectors, and current write-protection boundaries.
- PIA, MSC, and VCFFA1 address ownership and disabled-device RAM fallthrough
  remain exactly as documented.
- The SDL-only `$0000-$0002` BRK-recovery patch remains in the transitional CPU
  wrapper for a later behavior decision; it is not portable machine policy.
- The software CPU remains process-global and embedded through the existing
  macro adapter; the physical bus adapter remains embedded with its GPIO/latch
  operation order unchanged. The completed physical gate confirms the resulting
  service timing remains viable.
- No payload installation, terminal byte policy, storage protocol/backend,
  platform event loop, DVI, USB, or physical GPIO/latch change.

### Acceptance criteria recorded before implementation

Focused CPU-free host tests must prove:

1. initialization fills ordinary memory with the even/odd pattern before ROM;
2. Neo1-23-style ROM placement/protection preserves `$E000-$FFFF`;
3. Neo1-50-style ROM placement/protection preserves `$FF00-$FFFF` while
   `$E000` and `$F000` remain writable RAM;
4. ordinary RAM reads and writes succeed immediately below protection;
5. PIA addresses route to the shared PIA and adjacent addresses remain RAM;
6. attached MSC and VCFFA1 ports receive only their documented addresses;
7. unattached optional-device addresses remain RAM;
8. machine reset clears PIA state without changing RAM or ROM;
9. existing PIA, MSC/VCFFA1 decode, protocol, VACI, cycle-budget, and terminal
   tests continue to pass through the transitional wrapper.

Build and runtime gates:

- all host tests pass under SDL-23;
- SDL personalities 23 and 50 build and reach WozMon headlessly;
- Pico personalities 23 and 50 build with SDK 2.1.0;
- both working build directories are restored to personality 23;
- diff review finds the CPU adapters, reset sequence, storage backends,
  platform I/O, and physical GPIO/latch operation order unchanged; the physical
  gate confirms the resulting service timing.

### Evidence to date

- `neo1_machine` is ordinary shared C with no CPU, Pico, SDL, or Chips-memory
  dependency. It owns the deterministic 64 KB backing store, ROM placement and
  protection, shared PIA state, optional MSC/VCFFA1 decode, keyboard injection,
  and explicit read/write bus operations.
- The transitional `neo1_t` wrapper now forwards both CPU adapters through that
  bus surface. It still owns CPU selection and execution, reset signaling,
  startup tracing, snapshots, and the SDL-only `$0000-$0002` accommodation.
  Snapshot layout version 3 rejects older state rather than misreading it.
- `neo1_machine_address_space` is CPU-free and proves both profile layouts and
  reset-vector bytes, ROM protection, RAM access, PIA routing/reset, exact
  attached-device routing, and unattached-device RAM fallthrough. It and the
  nine existing focused tests pass under SDL-23.
- SDL personalities 23 and 50 build and reach WozMon headlessly with disposable
  images. Pico personalities 23 and 50 build with SDK 2.1.0. The working Pico
  and SDL build directories are restored to personality 23.
- Source review found no changes to the physical CPU adapter, GPIO/latch
  sequence, storage protocol/backends, terminal byte policy, DVI, USB, or
  platform event loops. Because calls now cross the extracted C-module
  boundary, external-bus service timing required the physical confirmation
  recorded below.
- The user confirmed the complete normal Neo1-23 physical gate on 2026-08-25:
  WozMon reset on DVI and serial, `E000R` Integer BASIC, `F000R` Krusader,
  `$0300` deposit/examine, VACI directory/cancel/return, USB and serial input,
  DVI output, and stable scrolling all passed.

### Physical gate

Using the normal Neo1-23 image:

1. confirm reset reaches WozMon on DVI and serial;
2. confirm `E000R` enters Integer BASIC and return to WozMon with reset;
3. confirm `F000R` enters Krusader and return to WozMon with reset;
4. use WozMon to deposit and examine a byte at `$0300`;
5. enter `C100R`, list the VACI directory, cancel with Return, and use `Q` to
   return to WozMon;
6. confirm USB keyboard, serial input, DVI output, and scrolling remain stable.

No storage write was required. The user confirmed this complete gate passed on
2026-08-25.

### Rollback condition

Revert this checkpoint if any RAM, ROM, vector, PIA, or optional-device address
result changes, either target fails its build/runtime gate, or extracting the
address space requires CPU- or target-specific semantics in `neo1_machine_t`.

## Checkpoint 5: explicit software-CPU runner

Status: complete 2026-08-25.

### Boundary

Move SDL software-CPU execution out of the transitional CPU-selected `neo1_t`
wrapper and into an ordinary shared runner under `src/runners/`:

- keep `neo1_machine_t` as separately owned CPU-neutral state and give the
  software runner an explicit pointer to its read/write surface;
- move fake65c02 callback bridging, reset, IRQ/NMI presentation, instruction
  stepping, and represented-cycle budgeting into `neo1_soft_runner`;
- keep the SDL-only `$0000-$0002` BRK-recovery patch in that software runner,
  outside `neo1_machine` policy;
- have SDL assemble and reset the machine and software runner explicitly;
- remove the Reload-style software adapter and the build-wide CPU-backend
  selector; have the still-transitional Pico wrapper include its physical WDC
  adapter directly.

This is one runner extraction, not the final physical-runner cleanup. Pico may
continue to use `src/systems/neo1.h` and its `MOS6502CPU_*` surface until a
separate checkpoint can preserve and test physical reset, trace, and bus-cycle
ownership on its own.

### Preserved behavior and non-goals

- SDL retains both ROM profiles, the current `$0000-$0002` patch, instruction
  cycle accounting, catch-up limit, input normalization, display output, and
  raw-image storage stub behavior.
- The checked-in fake65c02 source is not modified. Its CPU architectural state
  and required callback bridge remain process-global, so the new runner must
  explicitly support only one active instance. Provenance and long-term CPU
  suitability remain unresolved dependency decisions.
- No decimal-mode, IRQ/NMI, WAI/STP, opcode, or cycle-accuracy claim is added;
  those require a later software-CPU compatibility checkpoint.
- The unused/incomplete SDL snapshot surface is not recreated in the new
  runner. The Pico transitional wrapper retains its existing snapshot helpers.
- No physical GPIO/latch implementation, machine address, profile ROM, PIA,
  terminal policy, storage protocol/backend, DVI, USB, or platform event-loop
  behavior changes.

### Acceptance criteria recorded before implementation

Focused host tests must prove:

1. the software runner reads the reset vector through an attached
   `neo1_machine_t` and a two-cycle NOP step remains two represented cycles;
2. 100 and 101 microsecond budgets retain the current 102- and 104-cycle
   results at 1.0218 MHz;
3. runner reset clears its represented-cycle total and restarts from the reset
   vector without reinitializing machine RAM;
4. the `$0000-$0002` BRK-recovery jump is installed by the runner and targets
   the selected reset vector, while the CPU-free machine test retains the
   ordinary even/odd bytes there;
5. a second simultaneous software-runner instance is rejected explicitly;
6. machine PIA and optional-device tests compile without a CPU backend or
   `CHIPS_IMPL` implementation header;
7. the other MSC, VACI, machine, terminal, and protocol tests remain green.

Build and runtime gates:

- all host tests pass under SDL-23;
- SDL personalities 23 and 50 build and reach WozMon headlessly;
- no SDL source or host test includes `neo1.h`, `neo1_cpu_backend.h`, or
  `soft65C02cpu.h`;
- Pico personalities 23 and 50 build with SDK 2.1.0 after selecting the WDC
  adapter directly;
- both working build directories are restored to personality 23;
- source review finds no change to the WDC adapter implementation or Pico
  GPIO/latch/reset/bus-service ordering.

### Evidence to date

- `neo1_soft_runner` is an ordinary C module linked by SDL and host tests. It
  points explicitly to a separately owned `neo1_machine_t` and owns the existing
  fake65c02 callbacks, reset/interrupt presentation, instruction stepping,
  represented-cycle accounting, and microsecond budgeting.
- SDL now assembles and resets its machine and CPU runner separately. Its
  `$0000-$0002` BRK-recovery jump moved with software execution and remains
  absent from the CPU-neutral machine initialization test.
- The obsolete `neo1_cpu_backend.h` and `soft65C02cpu.h` macro adapter were
  removed, as was the `NEO1_CPU_BACKEND` CMake/preset setting. SDL and all host
  tests compile without `neo1.h` or a `CHIPS_IMPL` CPU wrapper. Pico selects
  `wdc65C02cpu.h` directly.
- `neo1_soft_cycle_budget` proves reset-vector execution, the two-cycle NOP,
  preserved 102/104-cycle time budgets, cycle-total reset, RAM preservation,
  exact BRK-recovery bytes, and explicit rejection of a second active runner.
  All ten focused tests pass under SDL-23.
- SDL personalities 23 and 50 build and reach WozMon headlessly with a
  disposable image. Pico personalities 23 and 50 build with SDK 2.1.0. The
  working Pico and SDL build directories are restored to personality 23.
- Source review found no change to `wdc65C02cpu.h` or its GPIO/latch sequence.
  The only Pico source selection change is replacing the removed backend
  selector include with a direct include of that same physical adapter; the
  physical gate remained required.
- The user confirmed the complete normal Neo1-23 physical gate on 2026-08-25:
  WozMon reset on DVI and serial, `E000R` Integer BASIC, `F000R` Krusader,
  `$0300` deposit/examine, VACI directory/cancel/return, USB and serial input,
  DVI output, and stable scrolling all passed.

### Physical gate

Using the normal Neo1-23 image:

1. confirm reset reaches WozMon on DVI and serial;
2. confirm `E000R` enters Integer BASIC and `F000R` enters Krusader, returning
   to WozMon with reset after each;
3. deposit and examine a byte at `$0300`;
4. enter `C100R`, list the VACI directory, cancel with Return, and use `Q` to
   return to WozMon;
5. confirm USB keyboard, serial input, DVI output, and scrolling remain stable.

No storage write was required. The user confirmed this complete gate passed on
2026-08-25.

### Rollback condition

Revert this checkpoint if SDL-visible reset, instruction scheduling, memory or
device behavior changes; if SDL still requires the physical wrapper/backend
selector; if either target fails its gates; or if the extraction requires CPU
semantics inside `neo1_machine_t`.

## Checkpoint 6: explicit physical W65C02 runner

Status: complete 2026-08-25.

### Boundary

Replace the remaining transitional Pico-only `neo1_t`/`MOS6502CPU_*` wrapper
with an ordinary physical runner under `systems/neo1-pico/src/`:

- let the Pico application own a `neo1_machine_t` and a separate
  `neo1_wdc_runner_t` explicitly;
- move physical W65C02 GPIO setup, reset and interrupt signalling, address/data
  latch access, PHI2 stepping, machine bus service, and startup tracing into
  that runner;
- have every captured physical bus cycle call the attached machine's explicit
  read or write interface;
- remove the now-unused Reload-style physical adapter, common support header,
  clock helper, and transitional system wrapper.

This checkpoint removes the final active Reload/CHIPS execution surface. It
does not replace or evaluate the separate fake65c02 dependency used by the SDL
software runner.

### Preserved behavior and non-goals

- Preserve the exact Pico pin assignments and directions, active-low
  RESET/IRQ/NMI polarity, initialization levels, one-millisecond reset and NMI
  pulses, latch enable ordering, and existing two- and six-`nop` settling
  delays.
- Preserve the cycle sequence: drive PHI2 low, capture address and R/W, drive
  PHI2 high, then service exactly one machine read or write. Preserve the
  first-64-access startup trace at that same service point.
- Preserve startup ordering, including the initial reset pulse during runner
  initialization and the later explicit machine/CPU reset before execution.
- Preserve both machine profiles, ROM/RAM policy and payloads, PIA behavior,
  terminal behavior, storage protocols/backends, DVI, USB, and the Pico event
  loop.
- Do not change the SDL software runner, fake65c02, any 6502-visible address,
  or optional-device policy. Do not begin profile, storage, or platform-HAL
  work here.
- Do not recreate unused transitional execution, snapshot, or generic debug
  callback surfaces that have no callers. Keep the hardware startup trace.
- Preserve the existing WDC adapter's attribution and license notice in the
  extracted runner.

### Acceptance criteria recorded before implementation

1. Pico owns the machine and physical runner as distinct state, and the runner
   holds an explicit pointer to the machine it services.
2. Each physical tick retains the exact PHI2/latch/data ordering and services
   exactly one `neo1_machine_read` or `neo1_machine_write` after bus capture.
3. GPIO initialization, interrupt polarity, and both startup reset pulses
   retain their current ordering and duration.
4. Startup diagnostics retain the first 64 serviced read/write events.
5. Active code no longer uses `CHIPS_IMPL`, `MOS6502CPU_*`, `chips_common.h`,
   `clk.h`, `wdc65C02cpu.h`, or the transitional `src/systems/neo1.h`.
6. All focused host tests and both SDL profiles remain unaffected.

Build and review gates:

- all focused host tests pass under SDL-23;
- SDL personalities 23 and 50 build and reach WozMon headlessly;
- Pico personalities 23 and 50 build with SDK 2.1.0;
- a diagnostic Pico-23 image builds with startup tracing enabled;
- both working build directories are restored to the normal personality 23;
- a source diff confirms that GPIO pins, latch ordering, settling delays,
  signal polarity, and PHI2/bus-service order did not change.

### Physical gate

First flash the diagnostic Neo1-23 image and compare its reset vector and first
64 bus events with the pre-extraction trace. Compare the defined sequence from
the `$FFFC/$FFFD` vector fetch onward, including opcode/device accesses and the
relative stack push/pop pattern. Do not require identical pre-vector dummy
addresses or absolute `$01xx` stack offsets: the W65C02S does not initialize
the PC or stack pointer to a specified value during hardware reset. A departure
in the defined sequence or relative stack behavior blocks the checkpoint. Then
flash the normal Neo1-23 image and:

1. confirm reset reaches WozMon on DVI and serial;
2. confirm `E000R` enters Integer BASIC and `F000R` enters Krusader, returning
   to WozMon with reset after each;
3. deposit and examine a byte at `$0300`;
4. enter `C100R`, list the VACI directory, cancel with Return, and use `Q` to
   return to WozMon;
5. confirm USB keyboard, serial input, DVI output, and scrolling remain stable.

No storage write is required.

### Evidence to date

- Pico now owns a separate `neo1_machine_t` and `neo1_wdc_runner_t`. The runner
  points explicitly to its machine and owns GPIO/latch timing, physical reset
  and interrupt pins, one-cycle bus service, cycle counting, and the buffered
  first-64-access startup trace.
- The physical implementation is ordinary `.c`/`.h` code under the Pico target.
  The transitional `neo1_t`, `MOS6502CPU_*`, `CHIPS_IMPL`, common/snapshot
  support, and unused clock helper were removed from active code. The separate
  SDL fake65c02 dependency is unchanged.
- Source comparison found the same GPIO pin map and initialization order, the
  same active-low RESET/IRQ/NMI behavior and one-millisecond pulses, the same
  OE1/OE2/OE3 latch order, and exactly the same 20 inline settling `nop`s.
  PHI2 remains low for address/R/W capture and returns high before data service
  and trace capture.
- All ten focused host tests pass under SDL-23. SDL personalities 23 and 50
  build and reach the WozMon prompt headlessly. Pico personalities 23 and 50,
  plus the diagnostic Pico-23 profile, build with SDK 2.1.0. Both working build
  directories are restored to normal personality 23.
- The user supplied the diagnostic Neo1-23 trace on 2026-08-25. It fetched
  `RESET=$FF00` from `$FFFC/$FFFD`, then matched the pre-extraction WozMon
  instruction, PIA-write, display-poll, and relative stack-access sequence.
  Pre-vector addresses and the absolute stack offset differed, as permitted for
  registers not initialized by hardware reset. The diagnostic trace gate
  therefore passed.
- The user confirmed the complete normal Neo1-23 physical gate on 2026-08-25:
  WozMon reset on DVI and serial, `E000R` Integer BASIC, `F000R` Krusader,
  `$0300` deposit/examine, VACI directory/cancel/return, USB and serial input,
  DVI output, and stable scrolling all passed.

### Rollback condition

Revert this checkpoint if the diagnostic trace changes, physical reset or bus
timing regresses, either target fails its build/runtime gates, or the runner
requires Pico-specific CPU semantics to leak into `neo1_machine_t`.

## Checkpoint 7: clock-qualified physical reset

Status: complete 2026-08-26.

### Boundary

Correct the Pico physical runner's RESET pulse to satisfy the W65C02S clocked
reset contract:

- retain the existing active-low RESET polarity and one-millisecond assertion;
- while RESET remains asserted, drive two explicit complete PHI2 cycles before
  releasing it;
- use the same qualified pulse during physical-runner initialization and every
  later runner reset, including Ctrl-R;
- keep reset-qualification cycles outside machine bus service, startup tracing,
  and represented-cycle accounting.

The W65C02S datasheet requires RESET low for at least two clock cycles. Elapsed
time with PHI2 stationary does not establish that requirement. This checkpoint
corrects only that physical signal sequence.

### Preserved behavior and non-goals

- Preserve all GPIO assignments and directions, latch enable ordering, inline
  settling delays, IRQ/NMI behavior, and normal PHI2 bus-service ordering.
- Preserve machine reset ordering: the caller resets 6502-visible device state
  before resetting the physical CPU.
- Preserve both startup reset pulses, ROM/RAM contents and protection, machine
  profiles, PIA behavior, terminal behavior, storage, DVI, USB, and the Pico
  event loop.
- Do not service or trace bus values observed while RESET is asserted; they are
  reset qualification rather than program-visible accesses.
- Do not initialize or normalize the W65C02 stack pointer or other registers in
  software. Their pre-vector values remain outside the trace comparison.
- Do not alter the SDL software runner or shared machine.

### Acceptance criteria recorded before implementation

1. One reset helper owns both initialization and later physical reset pulses.
2. RESET is driven low, held for the existing one millisecond, remains low for
   two complete low/high PHI2 cycles with explicit half-cycle settling time,
   and is released only after the second cycle.
3. The two qualification cycles do not read address/data latches, access
   `neo1_machine`, enter the startup trace, or increment `system_cycles`.
4. Normal `neo1_wdc_runner_tick()` ordering and its timing-sensitive latch
   delays remain unchanged.
5. All focused host tests pass, both SDL profiles reach WozMon headlessly, and
   Pico-23, Pico-50, and diagnostic Pico-23 build with SDK 2.1.0.

### Physical gate

Using diagnostic Neo1-23:

1. confirm the reset vector remains `$FF00` and the defined trace from
   `$FFFC/$FFFD` through WozMon/PIA startup matches checkpoint 6;
2. press Ctrl-R repeatedly and confirm every reset returns cleanly to WozMon
   without display corruption or a stalled bus.

Then use normal Neo1-23 for the checkpoint-6 functional smoke: WozMon on DVI
and serial, `E000R`, `F000R`, `$0300` deposit/examine, VACI
directory/cancel/return, USB and serial input, DVI output, and scrolling. No
storage write is required.

### Evidence to date

- One `neo1_wdc_pulse_reset()` helper now owns the initialization and runtime
  reset sequences. It retains the one-millisecond active-low assertion, drives
  two complete PHI2 cycles with one-microsecond half-cycle delays while RESET
  remains low, and releases RESET only after the second high phase.
- The qualification loop calls only the RESET, PHI2, and delay primitives. It
  does not sample address/data latches, service `neo1_machine`, capture trace
  events, or increment `system_cycles`.
- Source review confirms that normal bus-cycle/latch code, pin assignments,
  inline settling delays, IRQ/NMI behavior, and caller-owned machine-reset
  ordering are unchanged.
- All ten focused host tests pass under SDL-23. SDL personalities 23 and 50
  build and reach WozMon headlessly. Pico personalities 23 and 50 plus the
  diagnostic Pico-23 profile build with SDK 2.1.0.
- The user supplied the diagnostic Neo1-23 trace on 2026-08-26. It retained
  `RESET=$FF00` and matched checkpoint 6's defined vector, WozMon/PIA, display
  polling, and relative stack-access sequence.
- The user confirmed repeated Ctrl-R and the complete normal Neo1-23 physical
  gate passed on 2026-08-26: every reset returned cleanly to WozMon, and ROM
  entry, memory deposit/examine, VACI, input, DVI, serial, and scrolling
  behavior remained correct.

### Rollback condition

Revert this checkpoint if reset no longer reaches `$FF00`, repeated Ctrl-R is
unreliable, the defined startup trace or normal bus timing changes, qualification
cycles leak into machine-visible service/trace/accounting, or either target
fails its build/runtime gates.

## Checkpoint 8: shared machine profiles

Status: complete 2026-08-26.

### Boundary

Move the Neo1-23 and Neo1-50 ROM-layout policy into one ordinary shared machine
profile module:

- each profile identifies its personality, ROM image, ROM placement, and ROM
  write-protection boundary;
- both the Pico and SDL runners select one of those shared profiles and pass it
  to `neo1_machine_init()`;
- the shared machine uses the selected profile when initializing memory and
  retains its identity for runner diagnostics and profile-specific startup
  decisions.

This checkpoint removes duplicated profile semantics. It does not move
runner-installed software into the shared machine.

### Preserved behavior and non-goals

- Preserve the exact Neo1-23 8 KB system ROM at `$E000-$FFFF`, protection from
  `$E000`, and reset vector `$FF00`.
- Preserve the exact Neo1-50 256-byte WozMon ROM at `$FF00-$FFFF`, protection
  from `$FF00`, and reset vector `$FF00`.
- Keep the Neo1-50 `$E000/$F000` safety stubs and the VACI/VCFFA1 RAM payloads
  Pico-runner policy. SDL must not begin installing those payloads here.
- Preserve all device enablement, address decode, PIA behavior, terminal
  behavior, storage behavior, CPU-runner behavior, physical bus timing, DVI,
  USB, and event loops.
- Do not begin the shared MSC or VCFFA1 protocol extraction, introduce a
  platform HAL, or change the build-time profile choices.

### Acceptance criteria recorded before implementation

1. One shared profile lookup owns both profiles' ROM pointer, ROM size, ROM
   base, and protection base; neither runner defines those values separately.
2. `neo1_machine_init()` accepts a selected profile and rejects missing or
   invalid profile data before modifying machine state.
3. The machine retains the selected profile, and Pico diagnostics and
   profile-specific startup use that identity instead of duplicated layout
   constants.
4. A focused host test verifies both real ROM images, sizes, placements,
   protection boundaries, vectors, lookup failure, and machine write policy.
5. Existing focused tests pass, both SDL profiles reach WozMon headlessly, and
   Pico-23, Pico-50, and diagnostic Pico-23 build with SDK 2.1.0.
6. Both working build directories are restored to normal personality 23.

### Physical gate

Flash normal Neo1-23 and:

1. confirm reset reaches WozMon on DVI and serial;
2. confirm `E000R` enters Integer BASIC and `F000R` enters Krusader, returning
   to WozMon with reset after each;
3. deposit and examine a byte at `$0300`;
4. enter `C100R`, list the VACI directory, cancel with Return, and use `Q` to
   return to WozMon;
5. confirm USB keyboard, serial input, DVI output, and scrolling remain stable.

No storage write or diagnostic bus trace is required because this checkpoint
does not alter physical bus or reset sequencing.

### Evidence to date

- Acceptance criteria recorded before implementation.
- The ordinary shared `neo1_profile` module now owns the immutable Neo1-23 and
  Neo1-50 identities, ROM images, image sizes, placements, and protection
  boundaries. Both runners select through `neo1_profile_find()` and pass the
  result to `neo1_machine_init()`; the machine retains that profile.
- Pico profile diagnostics, VACI's ROM-boundary patch, RAM-payload bounds
  checks, and the Neo1-50 entry-stub decision now consume the selected machine
  profile. The stubs and both RAM payloads remain Pico-runner policy; SDL does
  not install them.
- The focused profile test covers both real ROM images, exact sizes and bases,
  reset vectors, protection behavior, unsupported lookup values, and invalid
  machine descriptions. All eleven focused host tests pass.
- SDL personalities 23 and 50 build and reach the WozMon `\` prompt with
  headless stdout enabled. The offscreen SDL video driver still reports its
  pre-existing OpenGL window warning, but execution and terminal output
  continue.
- Pico personalities 23 and 50 plus diagnostic Pico-23 build with SDK 2.1.0.
  Source review confirms no changes to storage protocols/backends, PIA and
  terminal behavior, CPU execution, physical GPIO/bus/reset timing, DVI, USB,
  or event loops. Both working build directories are restored to normal
  personality 23.
- The user confirmed the complete normal Neo1-23 physical gate passed on
  2026-08-26: WozMon reset on DVI and serial, both Neo1-23 ROM entries, `$0300`
  deposit/examine, VACI directory/cancel/return, USB and serial input, DVI
  output, and stable scrolling all remained correct.

### Rollback condition

Revert this checkpoint if either profile's ROM contents, placement, vectors, or
write protection changes; Pico-only startup policy leaks into the shared
machine; either target fails its build/runtime gates; or the normal Neo1-23
physical smoke regresses.

## Checkpoint 9: shared MSC register protocol

Status: complete 2026-08-26.

### Boundary

Extract the Neo1 MSC register state machine at `$D014-$D01C` into one ordinary,
instance-owned shared C module:

- the shared protocol owns command dispatch, BUSY/READY/error status, filename
  collection, sector/index/size registers, the 512-byte DATA buffer, directory
  filtering/index sequencing, short-read padding, and write-length rules;
- a backend contract owns filesystem or block I/O and returns explicit error
  codes without observing 6502 register accesses;
- Pico supplies the existing FatFs file backend;
- SDL supplies its existing raw-image sector backend and no file directory.

This checkpoint shares the 6502-visible protocol. It does not make SDL's raw
image into a host-directory file service.

### Preserved behavior and explicit convergence

- Preserve Pico's verified OPEN, CLOSE, READ, WRITE, DIR_OPEN, DIR_NEXT,
  OPEN_INDEX, and DELETE_INDEX behavior, including FatFs low-seven-bit errors,
  512-byte sectors, short-read zero padding, multi-sector writes, and final
  truncation.
- Preserve the Pico storage diagnostics setting and synchronous bus-service
  execution model.
- Preserve SDL raw-sector READ/WRITE through `NEO1_SDL_DISK`, including access
  without a preceding OPEN. SDL directory enumeration remains empty and its
  raw backend cannot truncate a host image on a short logical write.
- Intentionally remove SDL's divergent post-WRITE-command data-arming mode and
  its success responses for unknown or nonexistent indexed-file commands.
  DATA-before-WRITE and standard command/error sequencing now follow the
  shared contract.
- Keep VCFFA1 unchanged and SDL-local. Do not install VACI or the VCFFA1 RAM
  utility on SDL, add a host-directory backend, change storage addresses, add
  IRQ/NMI behavior, or introduce a general platform HAL.
- Preserve profiles, ROM/RAM policy, PIA and terminal behavior, CPU runners,
  physical bus/reset timing, DVI, USB lifecycle, and event loops.

The protocol extraction and SDL convergence must remain separately reviewable
commits so the small SDL behavior correction is not hidden inside the move.

### Acceptance criteria recorded before implementation

1. `neo1_msc_t` contains all `$D014-$D01C` register/buffer/transaction state;
   no target implements a second register state machine or uses protocol
   process globals.
2. A backend interface represents named-file open/close, sector read/write,
   root-directory iteration, and delete operations with opaque backend state.
   It has no access to machine memory or register addresses.
3. Shared code owns loadable-entry filtering and indexed-file resolution.
4. Pico's FatFs adapter contains filesystem calls and handles only; it does not
   dispatch commands or stream register bytes.
5. SDL's adapter contains only raw-image operations and explicit capability
   limits; VCFFA1 remains byte-for-byte behaviorally separate.
6. Focused host tests execute the production shared protocol with the Pico
   FatFs adapter and cover reset, invalid command, missing media, read-only
   open, directory filtering/end, indexed open/delete, short reads, multi-sector
   write, truncating overwrite, short write, seek/read errors, DATA bounds, and
   two independent protocol instances.
7. All host tests pass, both SDL profiles reach WozMon headlessly, and Pico-23,
   Pico-50, and diagnostic Pico-23 build with SDK 2.1.0.
8. Both working build directories are restored to normal personality 23.

### Physical gate

Use normal Neo1-23 and disposable USB media/files:

1. confirm reset, DVI/serial WozMon, USB keyboard, serial input, and scrolling;
2. enter `C100R`, list the VACI directory, load and run a known `.BIN`, then
   return to WozMon;
3. use VACI to write a unique 16-byte disposable file, confirm the host reports
   exactly 16 bytes, reopen/read it, and verify the bytes;
4. remove or unmount the USB medium, confirm a VACI directory/read operation
   reports an error without hanging, then remount and confirm listing recovers;
5. confirm VCFFA1 signature/status and the previously verified read/catalog
   workflow still operate. Do not perform a VCFFA1 write.

### Evidence to date

- Acceptance criteria recorded before implementation.
- Inventory confirmed Pico owns the complete verified FatFs/register behavior,
  while SDL duplicates the registers with raw-sector READ/WRITE, no-op file
  commands, an extra post-command write mode, and false success for unknown
  commands. The shared machine already supplies the required explicit device
  port, so no machine-address or CPU-runner change is needed.
- The instance-owned shared `neo1_msc` module now owns all command, register,
  filename, directory-index, status, and 512-byte DATA state. Both runners
  attach it through their existing machine device port; two initialized
  protocol objects retain independent register/transaction state.
- The Pico module now contains only FatFs handles and named-file, sector,
  directory, and delete callbacks. The production FatFs fixture still passes
  missing-media, read-only, filtering/end, indexed open/delete, short-read,
  multi-sector/truncating write, short-I/O, seek/read-error, and DATA-bound
  cases while preserving FatFs error values.
- SDL now supplies only raw-image callbacks. A focused fixture proves openless
  raw sector READ/WRITE, media errors, empty directory behavior, invalid and
  nonexistent-index errors, removal of post-command WRITE arming, and unchanged
  separate VCFFA1 signature/status behavior.
- All twelve focused host tests pass. SDL personalities 23 and 50 build and
  reach the WozMon `\` prompt headlessly. Pico personalities 23 and 50 plus
  diagnostic Pico-23 build with SDK 2.1.0. Both working build directories are
  restored to normal personality 23.
- Source review confirms VCFFA1 command/register code is unchanged and that
  target MSC files contain no second register array, DATA offset, status state,
  or command dispatcher. Profile, PIA, terminal, CPU-runner, physical bus/reset,
  DVI, USB-lifecycle, and event-loop code are unchanged.
- The user confirmed the normal Neo1-23 physical gate passed: reset and the
  display/input paths remained correct, ordinary VACI directory/load/run and an
  exact 16-byte disposable write/readback worked, and VCFFA1
  signature/status/read/catalog behavior remained correct.
- With USB storage unavailable, the specialized VACI BASIC `L` command returned
  silently to the menu without hanging rather than printing an error. Live
  reinsertion did not establish recovery during this gate; power cycling with
  the medium inserted restored normal storage operation. These are accepted,
  documented current limitations rather than evidence of hot-plug recovery.

### Rollback condition

Revert this checkpoint if Pico MSC/VACI behavior changes, SDL can no longer use
its raw disk for sector access, VCFFA1 behavior changes, protocol state becomes
process-global, filesystem details leak into the shared module, either target
fails its gates, or the disposable-media physical smoke regresses.

## Phase closeout

Checkpoints 1-9 establish the shared machine and remove the active Reload/CHIPS
execution architecture. Pico and SDL now use explicit physical and software CPU
runners around one CPU-neutral address space, shared profiles, Apple-1 PIA
state, terminal grid, and MSC register protocol. The phase closes without
claiming that the SDL raw-image backend equals Pico's file service.

Remaining work is follow-on dependency and compatibility work rather than a
prerequisite for closing this phase: qualify or replace the process-global
`fake65c02` dependency, resolve its provenance, decide how to share or retire
the duplicate SDL VCFFA1 implementation, and introduce additional software
platform seams only when a second consumer requires them. Current limitations
and hardware evidence remain in `docs/current-state.md`.
