# Neo1 Engineering Guidance

## Purpose

Neo1 is a focused, understandable implementation of an Apple-1-class computer
for the Apple-1's fiftieth anniversary. It may run on modern hardware and in a
host emulator, but the 6502-visible machine must remain explainable in terms of
addresses, bus cycles, ROM, RAM, and a small number of devices.

The guiding principle is:

> Modern outside, 1976 inside.

Do not require someone to understand a general-purpose emulator framework before
they can understand how Neo1 resets, runs WozMon, reads a key, writes a character,
or accesses storage.

## Targets and long-term shape

Neo1 has two current execution targets:

- `systems/neo1-pico/`: Olimex Neo6502, where an RP2040 services the bus of a
  physical W65C02 and provides DVI, USB keyboard, and USB storage.
- `systems/neo1-sdl/`: a desktop SDL target using a software 65C02.

An Adafruit Fruit Jam target is a future goal. It should reuse the software-CPU
and shared-machine path established for SDL while supplying Fruit Jam-specific
display, input, timing, audio, and storage services. Do not create Fruit Jam
code or a speculative universal HAL until the existing two targets establish
the required boundaries.

The intended architecture is one shared Apple-1 machine model with thin runners:

- the shared machine owns the 64 KB memory space, ROM/RAM policy, Apple-1
  PIA-like behavior, memory-mapped device contracts, and machine profiles;
- the Neo6502 runner owns physical W65C02 bus timing and GPIO/latch behavior;
- a software-CPU runner owns emulated CPU execution and is shared by SDL and,
  eventually, Fruit Jam;
- each platform owns its event loop, display transport, input transport,
  storage backend, timing, and lifecycle.

The physical Neo6502 must not be modeled as an awkward instance of an emulator
CPU. The SDL target is a behavioral test instrument, not the definition of the
machine.

## Current portability baseline

The active Reload/CHIPS execution surface has been removed. Both runners now
use the ordinary shared `neo1_machine` read/write interface, shared machine
profiles and PIA state, a shared terminal grid, and a shared MSC register
protocol. Treat the following as remaining areas to inspect rather than
patterns to preserve:

- `src/chips/fake65c02.h` is the SDL runner's direct software-CPU dependency.
  Its state is process-global, its provenance remains unresolved, and broad
  W65C02 compatibility has not been established.
- `systems/neo1-sdl/src/neo1_platform.h` mixes display, input, timing,
  lifecycle, and block storage in an SDL-local interface. It is not
  automatically the future shared platform API.
- `systems/neo1-sdl/src/neo1_storage_stub.c` supplies a raw-image MSC backend
  and a separate VCFFA1 protocol/backend. SDL does not install the Pico VACI or
  VCFFA1 RAM utilities and is not a file-service equivalent of Pico.
- Pico and SDL still own separate VCFFA1 register state machines. VCFFA1 is an
  optional compatibility layer, but a future change must share that protocol
  or remove an unnecessary target implementation rather than preserve both.
- `docs/neo1-sdl-emulator-plan.md` is a superseded historical plan. Verify
  current behavior in code, `docs/architecture.md`, and
  `docs/current-state.md`.

Do not paper over these differences with additional preprocessor conditionals.
First identify which behavior is authentic Apple-1 behavior, which is a useful
platform seam, and which is a temporary SDL accommodation.

## Architectural rules

- Preserve verified behavior before restructuring it.
- Keep both Neo6502 and SDL buildable at every completed milestone.
- Maintain one shared implementation of each 6502-visible device protocol.
- Do not duplicate the Apple-1 keyboard/display model, terminal state machine,
  MSC register state machine, or VCFFA1 register state machine by platform.
- A platform backend may perform host or hardware I/O, but it must not redefine
  the 6502-visible protocol.
- Keep the software CPU outside the shared machine state. It consumes the
  machine's read/write interface; it does not determine memory semantics.
- Keep the physical W65C02 bus outside the shared machine state. It supplies
  observed bus cycles to the same read/write interface.
- Prefer ordinary `.c` and `.h` modules over `CHIPS_IMPL` implementation headers.
- Prefer explicit bus cycles, addresses, and reads/writes over generic pin-mask
  or emulator lifecycle abstractions.
- Do not retain Reload/Chips abstractions merely because both current targets
  happen to use them.
- Do not change Apple-1-visible behavior solely to make an emulator backend
  convenient. Correct the runner or software CPU integration instead.
- New abstractions require at least two concrete consumers and must state which
  physical component or observable behavior they represent.
- Keep 6502-side programs and generated images separate from platform code.
- Preserve attribution and license notices when extracting or rewriting derived
  code. Treat the provenance and suitability of `fake65c02.h` as an explicit
  dependency decision, not an accidental permanent choice.
- Do not combine behavior changes, architectural extraction, broad renaming, and
  new features in one commit.

## Apple-1 legibility requirements

A contributor should be able to trace these paths without following platform
framework internals:

1. reset signal to reset-vector fetch and WozMon entry;
2. one 6502 memory read and one memory write;
3. keyboard input to `$D010/$D011`;
4. display output through `$D012/$D013`;
5. ordinary RAM access and ROM write protection;
6. optional storage commands from 6502-visible registers to a backend operation.

Every memory-mapped device must document:

- its address range;
- read and write behavior;
- status and error meanings;
- interrupt behavior, if any;
- whether it is part of the Apple-1 core, a Neo1 extension, or a Replica 1
  compatibility feature.

VACI is the preferred Apple-1-oriented storage interface. VCFFA1 is an optional
Replica 1 compatibility layer and must not determine the core architecture.

Do not assume every device will remain virtual. Keep address ownership, device
enablement, IRQ/NMI behavior, and bus direction explicit so a future physical
expansion interface can replace an internal device without changing the shared
machine model.

## Working method

For architectural work:

1. Inventory the existing behavior and identify its owner.
2. Define the smallest boundary that can be tested on SDL and preserved on
   Neo6502.
3. Record acceptance criteria before editing.
4. Make one reversible extraction.
5. Build and test SDL.
6. Build Neo6502 and state the physical-hardware test still required.
7. Review the diff for new duplication or target-specific semantics in shared
   code.
8. Update evidence-backed documentation.

Do not claim physical-hardware validation unless the user supplies the result.
If local hardware is unavailable, provide an exact smoke-test procedure and
leave the milestone awaiting hardware confirmation.

## Validation expectations

Changes to shared machine behavior require:

- focused host tests where practical;
- an SDL build and behavioral smoke test;
- a Neo6502 build;
- identification of the affected 6502-visible addresses;
- a physical Neo6502 smoke test supplied or performed by the user.

Changes to the physical bus layer additionally require reset-vector and early
bus-trace comparison. Preserve proven GPIO/latch ordering and timing until
hardware evidence supports a change.

Changes to the software CPU runner require WozMon reset, keyboard, display,
decimal-mode, interrupt, and 65C02-instruction compatibility checks appropriate
to the selected CPU core. Do not add memory patches or PIA exceptions merely to
make a failing CPU integration appear functional.

Storage write tests must use disposable media or images. Exercise success,
missing-media, read-only, invalid-command, out-of-range, and short-I/O paths.

## Documentation

- Stable design belongs in `docs/architecture.md`.
- Verified capabilities, known defects, and last hardware-test dates belong in
  `docs/current-state.md`.
- Migration checkpoints and their evidence belong in a dedicated execution plan
  or dated test log.
- Do not put changing milestone status in this file.
- Treat comments, plans, and agent files as claims to verify against code and
  test evidence, not as proof of current behavior.
