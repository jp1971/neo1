# Neo1 Portable-Core Execution Plan

Started: 2026-08-24

Baseline tag: `neo1-portable-core-baseline-2026-08-24`

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

Status: host/build gates passed 2026-08-25; awaiting physical validation.

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
  physical gate remains required.

### Physical gate

Using the normal Neo1-23 image:

1. confirm reset reaches WozMon on DVI and serial;
2. confirm `E000R` enters Integer BASIC and `F000R` enters Krusader, returning
   to WozMon with reset after each;
3. deposit and examine a byte at `$0300`;
4. enter `C100R`, list the VACI directory, cancel with Return, and use `Q` to
   return to WozMon;
5. confirm USB keyboard, serial input, DVI output, and scrolling remain stable.

No storage write is required. The checkpoint remains incomplete until the user
supplies this physical result.

### Rollback condition

Revert this checkpoint if SDL-visible reset, instruction scheduling, memory or
device behavior changes; if SDL still requires the physical wrapper/backend
selector; if either target fails its gates; or if the extraction requires CPU
semantics inside `neo1_machine_t`.
