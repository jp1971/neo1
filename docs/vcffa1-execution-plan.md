# Neo1 VCFFA1 Compatibility Execution Plan

Date: 2026-09-27

Status: planned and deferred until the Fruit Jam execution plan is complete;
CFFA1 binary-use permission recorded on 2026-09-27

Starting baseline: `9899eef` (`feat(pico): add runtime video aspect correction`)

## Decision

VCFFA1 will become a user-visible emulation of the original CFFA1 v1.1 card,
not merely a Neo1 block device with CFFA1-like addresses. A user who already
knows CFFA1 should be able to use its firmware menu, documented entry points,
API, disk images, and error behavior without learning a Neo1-specific storage
workflow.

The implementation underneath will remain native to Neo1:

- one shared, instance-owned CompactFlash/ATA controller will own the
  6502-visible register protocol;
- the shared machine will own the optional ROM and register address decode;
- Pico and SDL will supply storage backends without redefining the controller;
- the physical W65C02 and software-CPU runners will continue to use the same
  ordinary machine read/write interface; and
- VACI will remain Neo1's preferred Apple-1-oriented storage interface.

This work resumes only after Fruit Jam checkpoint 7 establishes its release
baseline. VCFFA1 remains disabled on Fruit Jam during the current Fruit Jam
plan. Adding it there later requires a separate decision and a concrete second
media-backend use case.

## Firmware and reference-material policy

On 2026-09-27, Rich Dreher gave Jameson explicit written permission to use the
CFFA1 binary in Neo1: “you have my permission to use the CFFA1 binary to add to
your project.” This resolves whether the unmodified firmware binary may be
added to Neo1. It does not explicitly grant permission to modify or relicense
the ROM, or to redistribute the source archive, manual, or supplied disk
images independently.

Until the deferred VCFFA1 workstream deliberately adds the authorized artifact
with its provenance and attribution:

- the original ROM, source archive, manual, disk images, and extracted files
  remain under ignored local `ref/` paths;
- no ROM bytes, ROM-derived C array/header, archive, manual, or supplied disk
  image is staged as part of the current Fruit Jam work;
- no test may silently download the firmware;
- an optional local ROM test must report `SKIP` with a useful path hint when
  the fixture is absent;
- build-generated embedding objects must live only in the selected build
  directory; and
- any writable test must operate on a disposable copy of a disk image.

The local v1.1 ROM fixture is 8,160 bytes and has SHA-256
`ada5c4c1a918b924a708eb1ac4ac726d3a712f395ff177a6945e1585b7b2dae8`.
The eventual local-fixture option should validate both properties before using
the file. An absolute path may be supplied at configure or test time, but it
must not be written into tracked presets, caches, generated source, or logs
intended for publication.

POM1 is a useful, GPL-3.0-licensed comparison implementation. It is not a code
source for Neo1 and is not the behavioral authority. In particular, the
reviewed implementation models the mirrored ATA window and sector data phases
but does not implement the ATA IDENTIFY command used by the v1.1 firmware.
Neo1 will use clean, independently written C based on observable behavior and
the original card documentation/firmware contract.

## Compatibility target

The target is specifically the Apple-1 CFFA1 v1.1 experience.

### Firmware surface

| Address | Required behavior |
| --- | --- |
| `$9000` | Enter the menu and exit to WozMon on Quit |
| `$9003` | Enter the menu and exit to BASIC on Quit |
| `$9006` | Enter the menu as a subroutine and return on Quit |
| `$9009` | ProDOS-style `CFBlockDriver` API |
| `$900C` | High-level CFFA1 API dispatcher |
| `$9012` | Menu using the firmware's serial-I/O path |
| `$9000-$AFDF` | Read-only 8,160-byte v1.1 firmware window |
| `$AFDC-$AFDD` | CFFA1 signature bytes `$CF,$FA` within that ROM window |

The same firmware bytes should produce the original prompts and commands.
Neo1 should not reproduce those screens independently in platform code.

The `$900C` API compatibility gate covers firmware version, menu, error
display, directory open/read/find, file read/write, BASIC save/load, rename,
delete, new-directory, and format selectors. Destructive functions must be
tested only on disposable images.

### Controller surface

The controller decodes `$AFE0-$AFFF`; A4 is not decoded, so `$AFEx` and
`$AFFx` access the same registers. The primary `$AFF0` view is:

| Address | Read | Write |
| --- | --- | --- |
| `$AFF1` | Unused | Set-CS-mask strobe |
| `$AFF2` | Unused | Clear-CS-mask strobe |
| `$AFF6` | Alternate status | Device control / software reset |
| `$AFF8` | 8-bit streaming data | 8-bit streaming data |
| `$AFF9` | ATA error | ATA feature |
| `$AFFA` | Sector count | Sector count |
| `$AFFB-$AFFE` | LBA task-file bytes | LBA task-file bytes |
| `$AFFF` | ATA status | ATA command |

At minimum, the v1.1 firmware requires READ SECTORS `$20`, WRITE SECTORS
`$30`, IDENTIFY DEVICE `$EC`, and SET FEATURES `$EF`. It polls BSY, DRQ, and
ERR; VCFFA1 does not assert IRQ or NMI. The controller must retain the upper
device/mode bits of `$AFFE` while using only its low nibble as LBA bits 24-27.

IDENTIFY data must report media capacity in the words consumed by the original
firmware. Exact sector-count-zero behavior, reset signatures, error-register
values, and status transitions will be fixed by the checkpoint-1 evidence
rather than copied unquestioningly from POM1.

### Media surface

The first compatibility target is a raw ProDOS-order `.po` image with 512-byte
logical sectors. Existing `.hdv` support may remain when it has the same raw
sector semantics. A `.2mg` filename must not be accepted as a raw image unless
Neo1 explicitly parses and validates its container header.

Media selection is platform policy, not an ATA-controller behavior. The shared
controller receives only an instance-owned backend capable of reporting:

- media presence and change/reset state;
- read-only state;
- logical sector count;
- one-sector read;
- one-sector write; and
- synchronization/flush success.

Pico may discover an image through FatFs on USB mass storage. SDL may open an
explicit host path. Neither backend receives CFFA1 register addresses, ATA
commands, machine memory, or firmware API calls.

## Current gap

| Concern | Current Neo1 behavior | Required end state |
| --- | --- | --- |
| User entry | Neo1 utility at `$1810` | CFFA1 v1.1 menu at `$9000` and documented alternate entries |
| Firmware | No CFFA1 ROM is mapped | Optional, protected `$9000-$AFDF` firmware image |
| Register decode | `$AFF0-$AFFF` only | Mirrored `$AFE0-$AFFF` window |
| Commands | Neo1-specific `$00/$01/$02` at `$AFFF` | ATA `$20/$30/$EC/$EF`; ProDOS `$00/$01/$02` only through `$9009` parameters |
| Controller state | Separate process-global Pico and SDL implementations | One caller-owned shared controller per machine |
| Media access | Protocol and FatFs/raw-image operations are intertwined | Target backends expose sectors and media properties only |
| Error model | ProDOS errors placed directly in ATA error | ATA status/error at the device; firmware translates to ProDOS/API errors |
| Utility | Reimplements a limited subset of ProDOS catalog/file behavior | Original firmware owns the CFFA1 UI and filesystem behavior |
| Fruit Jam | Disabled | Remains disabled through the current Fruit Jam plan |

The current `$1810` utility is migration scaffolding. Its known transactional,
metadata, filesystem-range, destination-range, and unbounded-DRQ defects should
not be expanded into a second CFFA1 firmware implementation.

## Checkpoint 0: permission record and reproducible local fixture

Status: binary-use permission recorded on 2026-09-27; fixture plumbing deferred

### Boundary

Preserve Rich Dreher's permission record and its exact scope: the CFFA1 binary
may be used and added to Neo1, while modification, relicensing, and unrelated
reference artifacts were not addressed. Separately add an optional local-
fixture configuration before deciding how the authorized binary enters normal
builds.

This checkpoint does not alter memory decode or runtime behavior.

### Acceptance gate

- A missing fixture produces a clear skip or a configure-time message, not a
  network request or an unexplained failure.
- An incorrect length or SHA-256 is rejected.
- Generated embedding output stays inside ignored build directories.
- `git status --ignored` confirms the supplied ROM and derived build artifact
  are not candidates for a commit.
- Normal builds without the local option remain byte-for-byte independent of
  the firmware fixture.

### Rollback

Remove the fixture plumbing if it can leak an absolute path or ROM bytes into
tracked files, build metadata intended for publication, or diagnostics.

## Checkpoint 1: black-box compatibility specification

### Boundary

Before changing the controller, turn the original manual, API equates,
firmware behavior, and safe observations into a concise test contract. Use the
ROM only as a local black-box fixture; do not translate its implementation into
Neo1 code.

Capture:

1. power-on and software-reset register values;
2. `$AFE0/$AFF0` mirroring;
3. command/status/error transitions for each required ATA command;
4. byte ordering and termination of 512-byte data phases;
5. IDENTIFY words used for capacity;
6. sector-count and LBA progression behavior;
7. no-media, read-only, invalid-LBA, short-I/O, and unknown-command results;
8. the menu entry/exit behavior at all documented entry points; and
9. the API selectors and ProDOS error results required for a representative
   menu and programmatic workflow.

Where the manual, firmware, POM1, and current Neo1 behavior differ, record the
difference and choose the original v1.1 user-visible result. Do not treat a
POM1 behavior as proof by itself.

### Acceptance gate

- The contract names every owned address and every read/write side effect.
- Timing expectations distinguish required polling transitions from host wall
  time; no arbitrary emulator delay becomes part of the machine contract.
- Tests can use an in-memory disposable sector backend.
- Firmware tests are optional/local; controller tests require no copyrighted
  fixture.

### Rollback

Do not implement a disputed behavior until the evidence and chosen result are
recorded.

## Checkpoint 2: share the existing legacy controller without behavior change

### Boundary

First remove the Pico/SDL duplication while preserving the current
`$00/$01/$02` shim exactly:

- move register and transfer state into an instance-owned module under
  `src/devices/`;
- define the sector-media backend described above;
- attach one instance from Pico and one from SDL;
- preserve the current `$AFDC-$AFDD` and `$AFF0-$AFFF` decode temporarily;
- preserve the `$1810` utility byte-for-byte; and
- leave ROM decode absent.

This is an architectural extraction, not the CFFA1 behavior conversion.

### Acceptance gate

- Focused tests cover two independent controller instances.
- Existing enabled/disabled decode and RAM-fallthrough tests pass.
- SDL's raw image and Pico's FatFs image retain their documented current
  behavior, including read-only policy.
- The generated `$1810` payload is byte-identical.
- Both Pico profiles and both SDL profiles build.
- The existing Neo1-23 VCFFA1 signature, status, catalog, and load smoke passes
  on physical hardware; no write is required.

### Rollback

Revert if the shared module learns FatFs, SDL file APIs, machine memory, or
target lifecycle, or if either target retains a second register state machine.

## Checkpoint 3: add the authentic ATA controller contract

### Boundary

Extend the shared controller with the mirrored register window, software reset,
ATA status/error state, 8-bit data phases, LBA masking/progression, required
commands, and IDENTIFY data. Retain the legacy `$00/$01/$02` commands only as a
clearly marked transition path for the unchanged `$1810` utility.

Do not add ROM mapping in this checkpoint.

### Acceptance gate

Controller tests cover:

- exact `$AFEx/$AFFx` mirroring;
- signature behavior during the transition;
- READ, WRITE, IDENTIFY, SET FEATURES, and unsupported commands;
- alternate-status reads without unintended side effects;
- software reset and abandoned partial transfers;
- one- and multi-sector transfers and defined count-zero behavior;
- LBA high-nibble masking and LBA advancement;
- no media, media change, read-only, invalid LBA, short read/write, and sync
  failure;
- DRQ/ERR/BSY transitions and completion; and
- no IRQ or NMI assertion.

Both target backends and all pre-existing `$1810` behavior remain green.

### Rollback

Revert if an ATA rule is implemented in a backend, if synchronous host I/O is
made visible as an accidental permanent BSY timing guarantee, or if legacy
behavior changes before its caller is migrated.

## Checkpoint 4: map the optional ROM and run it on SDL

### Boundary

Add optional CFFA1 memory ownership to the shared machine:

- `$9000-$AFDF` reads come from the supplied, read-only firmware image;
- `$AFE0-$AFFF` routes to the shared controller, with A4 mirroring;
- writes to the ROM window are ignored while VCFFA1 ROM is present;
- with VCFFA1 disabled, the full region remains ordinary backing RAM; and
- an enabled build with no firmware fixture retains the documented transitional
  `$1810` path rather than pretending full CFFA1 compatibility.

Begin with SDL because the software runner gives deterministic instruction and
memory tests. ROM bytes remain external and local.

### Acceptance gate

- Address tests prove ROM contents, write protection, register precedence at
  `$AFE0-$AFFF`, and disabled-device RAM fallthrough.
- WozMon `9000R`, `9003R`, and `9006R` follow their documented exit paths.
- `$9009` status/read/write calls transfer the requested ProDOS block.
- `$900C` version and representative directory/read/write functions return
  documented carry/A results.
- Menu catalog, load, save/overwrite, rename, delete, directory creation, BASIC
  save/load, and format are exercised on disposable image copies.
- Reset, WozMon, Integer BASIC, Krusader, PIA, terminal, and VACI tests remain
  unchanged.

### Rollback

Revert if ROM ownership leaks into the software runner, if SDL supplies
firmware semantics, or if the machine copies/patches ROM bytes to accommodate
the CPU core.

## Checkpoint 5: migrate and retire the Neo1 command shim

### Boundary

Rewrite only the low-level block-transfer portion of the `$1810` utility to use
the real ATA task-file protocol. Preserve its visible behavior long enough to
verify the migration, then remove support for commands `$00/$01/$02` at
`$AFFF` in a separate commit.

Once the local ROM workflow is proven, decide whether the utility still has a
supported fallback role. Do not repair or enlarge its independent ProDOS file
implementation merely to compete with the original firmware.

### Acceptance gate

- The assembly source clearly distinguishes ProDOS commands passed at `$9009`
  from ATA commands written to `$AFFF`.
- Generated-payload review isolates expected low-level driver changes.
- The utility's catalog/load smoke works through ATA `$20`.
- A disposable 512-byte write/readback works through ATA `$30`.
- No code or test writes `$00`, `$01`, or `$02` to `$AFFF` after removal.
- Legacy command constants and duplicated ProDOS-style ATA error definitions
  are gone from the shared device header.

### Rollback

Keep the transition aliases for one more checkpoint if a tracked, supported
caller still uses them. Do not silently retain them as permanent undocumented
behavior.

## Checkpoint 6: Pico integration and physical compatibility gate

### Boundary

Allow a local Pico build to embed the verified external ROM through a
build-directory-only artifact. Use the same shared controller as SDL and retain
Pico's FatFs image-discovery/backend ownership.

Correct image selection and media-state defects as separate commits. Do not
combine controller behavior, filename-policy fixes, hot-plug recovery, and
firmware UI changes in one commit.

### Acceptance gate

Using disposable USB media and a local ROM fixture on Neo1-23:

1. reset reaches WozMon with DVI, serial, and USB keyboard working;
2. `9000R` shows the CFFA1 menu and Quit returns to WozMon;
3. `9003R` returns to BASIC and `9006R` returns to its caller;
4. catalog and load/run match the SDL fixture results;
5. save, overwrite, rename, delete, directory creation, BASIC save/load, and
   format survive remount or power cycle;
6. no-media, read-only, invalid-block, and removal paths return without a CPU
   hang;
7. the verified VACI read/write/BASIC workflows remain intact; and
8. VCFFA1-disabled builds retain RAM at `$9000-$AFFF` except for other
   explicitly owned profile regions.

Repeat the non-destructive reset/menu/catalog gate on Neo1-50. Compare a short
diagnostic bus trace only if shared machine decode or physical bus ordering is
changed; the expected reset vector itself should remain unchanged.

### Rollback

Revert if a normal build accidentally embeds the local ROM, if storage polling
starves the physical bus/video/USB services, or if a media error can leave the
W65C02 stuck indefinitely.

## Checkpoint 7: distribution decision and release baseline

### Boundary

Apply the recorded permission without changing controller semantics:

- preserve the permission notice and Rich Dreher/R&D Automation attribution;
- commit only the specifically authorized, unmodified CFFA1 binary artifact;
- do not describe the binary as relicensed or modification-authorized;
- make the checked-in ROM reproducible and hash-verified; and
- enable the CFFA1 user experience only in profiles whose memory map and media
  backend have passed the physical gate.

### Acceptance gate

- `README.md`, `docs/architecture.md`, and `docs/current-state.md` distinguish
  controller compatibility, firmware availability, and verified targets.
- A contributor can trace `$9000R` through ROM fetches, ATA register accesses,
  a backend sector read, and display output without entering platform
  framework internals.
- Pico and SDL builds, host tests, local-ROM tests, and required physical gates
  are recorded from a clean worktree.
- A milestone tag is created only after the chosen distribution policy is
  reflected in the repository contents.

## Commit boundaries

Keep at least these concerns separate:

1. local-fixture plumbing and permission record;
2. evidence-backed compatibility tests;
3. legacy controller extraction;
4. backend conversion for each target;
5. authentic ATA command/state implementation;
6. shared ROM/address decode;
7. SDL firmware integration;
8. `$1810` low-level driver migration and generated payload;
9. legacy-command removal;
10. Pico local-ROM embedding support;
11. image discovery/media recovery fixes;
12. documentation and distribution decision.

Do not combine firmware redistribution, broad repository cleanup, Fruit Jam
bring-up, a CPU-core change, or unrelated VACI behavior with these commits.

## Resume condition

Resume at checkpoint 0 only after Fruit Jam checkpoint 7 is complete and its
baseline is tagged. The first implementation turn should re-read the permission
response, verify the local fixture hash, rerun all host tests and both target
build matrices, and confirm that the current VCFFA1 behavior still matches this
plan's starting inventory.

Until then, the existing `$1810` utility remains available under the limitations
in `docs/current-state.md`; writes and deletes require disposable images.

## Research sources

- Local, ignored `ref/cffa1/CFFA1_cdromv1.1.zip`: original v1.1 manual,
  firmware binary, API equates, firmware source, CPLD artifacts, and sample
  images used to establish the planning contract. Permission covers adding the
  binary to Neo1; the other archive contents remain local evidence unless their
  inclusion is separately authorized.
- [POM1 repository](https://github.com/habib256/POM1): secondary behavioral
  comparison and integration example.
- [POM1 CFFA1 controller](https://github.com/habib256/POM1/blob/main/src/CFFA1.cpp)
  and [header](https://github.com/habib256/POM1/blob/main/src/CFFA1.h): reviewed
  locally at upstream commit `a6ff221b406a156ebc48d0256871de1532344327`.
