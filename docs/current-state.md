# Neo1 Current State

Last updated: 2026-08-26

This document records evidence-backed capabilities and known defects. It is a
snapshot, not the architecture contract or a roadmap.

## Build baseline

- The supported Neo1 Pico workflow uses the Raspberry Pi Pico VS Code extension
  with CMake Tools and the named CMake configure/build presets.
- Raspberry Pi Pico SDK 2.1.0 and Arm GNU Toolchain 13.3.Rel1 are the verified
  hardware-build baseline.
- Normal Neo1-23 and Neo1-50 Pico builds passed on 2026-08-24 with the VACI
  error-line follow-up. Normal and diagnostic builds for both profiles passed
  on 2026-08-23; ELF inspection confirmed normal builds omit verbose trace
  strings and diagnostic builds retain them. The working `build/` directory
  was restored to the normal Neo1-23 profile afterward.
- Normal and diagnostic Pico presets set `NEO1_DIAGNOSTICS` explicitly so
  switching back to a normal profile restores concise serial output.
- The SDL-23 target builds locally and now schedules the software CPU from a
  monotonic elapsed-time budget using fake65c02's instruction cycle counts. A
  headless WozMon startup smoke and the focused cycle-budget test passed on
  2026-08-24; this does not establish equivalent Pico storage or hardware
  behavior.
- The build-wide CPU-backend selector and all active Reload/CHIPS execution
  surfaces have been removed. SDL links the explicit `neo1_soft_runner`; Pico
  owns an explicit `neo1_wdc_runner` beside the shared machine.
- `NEO1_ENABLE_MSC` now controls `$D014-$D01C` ownership explicitly. Focused
  host tests prove enabled accesses reach the device and disabled accesses use
  backing RAM; VACI-without-MSC configurations are rejected.
- The SDL host configuration now includes eleven focused tests. All passed
  locally through 2026-08-26: the production Pico MSC contract against an
  in-memory FatFs fake, the generated VACI BASIC/ordinary transfer paths,
  enabled and disabled MSC and VCFFA1 address decode, the shared Apple-1 PIA
  contract, the CPU-neutral RAM/ROM/address-space contract, software-CPU cycle
  budgeting, the real Neo1-23/Neo1-50 profile layouts, and the shared terminal
  grid plus preserved Pico/SDL control-byte policies.
- Portable-core checkpoint 1 now gives Pico and SDL one shared 40×24 terminal
  grid while leaving control-byte policy and rendering target-owned. Both SDL
  profiles reach WozMon headlessly, both Pico profiles build, and the eight host
  tests pass. The Neo1-23 DVI/serial, scrolling, form-feed, cursor, UART/USB
  input, and VACI-return smoke also passed on 2026-08-24.
- Portable-core checkpoint 2 removes Pico storage headers and target-global
  storage calls from the shared machine. The machine now owns MSC and VCFFA1
  address decode and invokes explicit runner-attached ports. Both SDL profiles
  reach WozMon headlessly, both Pico profiles build, and all eight host tests
  pass. The Neo1-23 DVI/serial reset, VCFFA1 signature/status, VACI directory,
  WozMon return, USB keyboard, and serial-input gate passed on 2026-08-24.
- Portable-core checkpoint 3 gives both runners one ordinary shared C model for
  `$D010-$D013` and `$D0F2-$D0F3`. The software runner now follows the physical
  DDR selection and first-pending-key rules. All nine host tests pass, both SDL
  profiles reach WozMon headlessly, and both Pico profiles build. The Neo1-23
  WozMon, memory examine/deposit, DVI/serial output, USB/serial input, VACI
  directory/return, and scrolling gate passed on 2026-08-25.
- Portable-core checkpoint 4 moves the 64 KB backing store, ROM placement and
  protection, PIA state, and optional-device decode into the CPU-neutral
  `neo1_machine` C module. Its focused test covers both profile layouts,
  vectors, RAM fallthrough, write protection, PIA reset, and attached/unattached
  device routing. All ten host tests pass, both SDL profiles reach WozMon
  headlessly, and both Pico profiles build with SDK 2.1.0. Both build trees are
  restored to Neo1-23. The normal Neo1-23 physical gate passed on 2026-08-25.
- Portable-core checkpoint 5 gives SDL an ordinary software-CPU runner attached
  to a separately owned `neo1_machine_t`. The runner owns fake65c02 callbacks,
  reset/interrupt presentation, instruction stepping, cycle budgeting, and the
  SDL-only `$0000-$0002` recovery patch. All ten host tests pass, both SDL
  profiles reach WozMon headlessly, both Pico profiles build with SDK 2.1.0,
  and both build trees are restored to Neo1-23. The normal Neo1-23 physical
  gate passed on 2026-08-25.
- Portable-core checkpoint 6 gives Pico an ordinary physical-W65C02 runner
  attached to a separately owned `neo1_machine_t`. The runner owns GPIO/latch
  timing, reset/interrupt pins, machine bus service, cycle counting, and the
  first-64-access startup trace. Source review preserved the pin map, signal
  polarity, latch order, all 20 settling `nop`s, and PHI2-before-service order.
  All ten host tests pass, both SDL profiles reach WozMon headlessly, both Pico
  profiles and the Pico-23 diagnostic profile build with SDK 2.1.0, and both
  build trees are restored to normal Neo1-23. The post-extraction diagnostic
  trace preserved the defined reset-vector, WozMon, PIA, and relative stack
  sequence, and the complete normal Neo1-23 physical gate passed on 2026-08-25.
- Portable-core checkpoint 7 clock-qualifies the physical RESET assertion with
  two complete PHI2 cycles while retaining the existing one-millisecond pulse.
  Qualification cycles bypass machine service, tracing, and cycle accounting.
  All ten host tests pass, both SDL profiles reach WozMon headlessly, and
  Pico-23, Pico-50, and diagnostic Pico-23 build with SDK 2.1.0. The diagnostic
  trace, repeated Ctrl-R, and complete normal Neo1-23 physical gate passed on
  2026-08-26.
- Portable-core checkpoint 8 now gives both runners one shared definition of
  the Neo1-23 and Neo1-50 ROM images, sizes, placements, and protection
  boundaries. The machine retains the selected profile; Pico-only RAM tools
  and Neo1-50 entry stubs remain runner policy. All eleven host tests pass,
  both SDL profiles reach WozMon headlessly, and Pico-23, Pico-50, and
  diagnostic Pico-23 build with SDK 2.1.0. Both build trees are restored to
  normal Neo1-23. The complete normal Neo1-23 physical gate passed on
  2026-08-26.
- SDK 2.3.0 has not been configured, built, or hardware-tested.

## Last Neo6502 hardware validation

User-supplied results from 2026-08-22 through 2026-08-26 used the Neo1-23
profile with VACI and VCFFA1 enabled. The latest result is checkpoint 8's
complete normal-profile functional gate after moving both ROM layouts into the
shared machine-profile module.

| Capability | Result | Evidence |
| --- | --- | --- |
| Reset and WozMon | Verified | Reset reached WozMon and monitor commands executed |
| Neo1-23 ROM entries | Verified | `E000R` entered Integer BASIC and `F000R` entered Krusader |
| DVI video | Verified | The shared-grid checkpoint passed sustained WozMon output/scrolling, form-feed clear, cursor, keyboard, and VACI-return checks on Neo1-23 |
| Serial console | Verified | Normal and diagnostic profiles produced their intended transcripts while preserving monitor output |
| USB HID keyboard | Verified | User explicitly verified keyboard input |
| Apple-1 PIA-like interface | Verified | Checkpoint 3 preserved WozMon memory examine/deposit, DVI/serial display output, USB/serial input, VACI directory/return, and stable scrolling |
| CPU-neutral machine boundary | Verified | Checkpoint 4 preserved WozMon reset, both Neo1-23 ROM entries, `$0300` memory deposit/examine, VACI directory/cancel/return, USB and serial input, DVI output, and stable scrolling |
| Explicit software-runner boundary | Verified | Checkpoint 5 preserved the same Neo1-23 WozMon, ROM-entry, memory, VACI, input, DVI, and scrolling gate after removing the build-wide CPU selector |
| Explicit physical-runner boundary | Verified | Checkpoint 6 preserved the defined reset-vector, WozMon opcode/device, and relative stack-access trace plus the complete normal Neo1-23 functional gate |
| Clock-qualified physical reset | Verified | Checkpoint 7 preserved the defined diagnostic trace, repeated Ctrl-R reliably returned to WozMon, and the complete normal Neo1-23 functional gate passed |
| Shared machine profiles | Verified | Checkpoint 8 preserved WozMon reset, both Neo1-23 ROM entries, `$0300` memory deposit/examine, VACI directory/cancel/return, USB and serial input, DVI output, and stable scrolling |
| USB MSC/FatFs | Verified | Media mounted and directory/file workflows operated |
| VACI read/load | Verified | `.BIN` files loaded and ran |
| VACI write | Verified | A write larger than 512 bytes produced a host-reported 2 KB file; rewriting the same name produced an exact 16-byte file, confirming multi-sector operation and truncation |
| VACI BASIC save/load | Verified | The 2,230-byte sentinel test restored all checked values across `$004A-$00FF` and `$0800-$0FFF` |
| VACI ordinary-transfer hardening | Verified | The 2026-08-24 smoke test confirmed the profile marker, valid 16-byte write/read, 64 KB write rejection, Neo1-23 ROM-destination rejection, close behavior, unchanged ROM data, and error messages beginning on new lines |
| VCFFA1 | Verified at workflow level | User reported VCFFA1 working and successfully ran a loaded `.po` image; checkpoint 2 also preserved `$AFDC-$AFDD` signature and `$AFFF` status reads |

The first VACI write was displayed by the host as rounded “2 KB,” so that test
did not independently establish the then-current 2,369-byte payload length. It
does establish that the old 512-byte cap is gone. The exact 16-byte overwrite
establishes short final-write length and truncation behavior.

## Implemented storage commands

The Pico VACI image is reproducibly generated from `neo1_vaci_v1.s`, is 2,584
bytes, and occupies `$C100-$CB17`. Its visible commands are `R`, `W`, `L`, `S`,
and `Q`; destructive delete `D` is hidden.

The Pico VCFFA1 backend implements status, 512-byte block read, and 512-byte
block write. Writable operation requires a preferred writable image such as
`CFFA1RW.PO` or `CFFA1RW.HDV`; fallback images are opened read-only.

VCFFA1 is retained as an optional Replica 1 compatibility feature, but the
reliability work in defects 1 and 6-9 is deferred until after the next
portable-core checkpoint. VACI remains the preferred Apple-1 storage path.
Until that work resumes, use VCFFA1 `W` and `D` only with disposable images;
the verified catalog/load workflow may continue to be used within the stated
directory, bitmap, file-size, and destination limitations.

## Known defects and unverified behavior

1. **Generic VCFFA1 `.po` discovery is faulty.** The extension matcher handles
   `.hdv` and `.2mg`, but its `.po` comparison uses the wrong character
   positions. Preferred names such as `CFFA1RW.PO` and `CFFA1.PO` still work.
2. **SDL does not install VACI or the VCFFA1 RAM utility.** Its MSC file and
   directory commands are accepted as no-ops and its storage path maps raw
   sectors to one host image. SDL preset labels therefore do not imply Pico
   VACI file behavior.
3. **Neo1-50 hardware behavior is build-verified only in this pass.** The dated
   physical smoke result above is for Neo1-23.
4. **SDK 2.3.0 is unverified.** Upgrade validation requires both Pico profile
   builds followed by the reset, DVI, keyboard, MSC, VACI, and VCFFA1 hardware
   smoke tests.
5. **Automated coverage remains limited.** Focused host tests cover the Pico MSC
   register protocol and execute VACI BASIC plus ordinary read/write paths on
   the software 65C02; enabled/disabled MSC and VCFFA1 address decode, the
   Apple-1 PIA-like register/latch contract, both profile RAM/ROM layouts and
   vectors, reset preservation, and soft-instruction cycle budgeting are also
   covered. There are still no focused tests for VACI delete, VCFFA1 protocol
   behavior, snapshots, or broad CPU compatibility.
6. **The VCFFA1 utility's create/delete updates are not transactional.** New
    file creation commits allocation bits before its directory and sapling
    index writes, without rollback. Delete may free an index block after an
    index-read error, ignores a bitmap-write error, and can then remove the
    directory entry. Failures can leak blocks or leave ProDOS metadata
    inconsistent; use a disposable image for write/delete testing.
7. **VCFFA1 existing-file writes do not update catalog metadata.** The utility
    writes the requested bytes into an existing seedling or sapling but leaves
    its EOF, blocks-used, auxtype, and other directory fields unchanged when
    source or length differs from the entry.
8. **The VCFFA1 utility has hard-coded filesystem limits.** Catalog, lookup,
    create, and delete inspect only root directory block 2. Allocation/freeing
    uses only the first bitmap block and assumes at most 4096 volume blocks;
    load/create/write support only seedling and two-data-block sapling files
    through 1024 bytes. Load destinations are not range checked.
9. **The VCFFA1 block driver can wait forever for DRQ.** Read/write checks the
    error register immediately after command issue, then polls DRQ without a
    timeout or further busy/error checks. A device or backend that never raises
    DRQ stalls the 6502 utility indefinitely.
10. **The software CPU dependency remains provisional.** The checked-in
    fake65c02 core and its callback API keep CPU state process-global, so
    `neo1_soft_runner` explicitly permits only one active instance. Its source
    provenance/license chain remains unresolved, it has no snapshot API, and
    broad W65C02 compatibility has not been established.

## Storage-test expectations still outstanding

Future write-path validation should use disposable media or images and cover
missing media, read-only media, invalid commands/ranges, out-of-range blocks,
and short I/O in addition to the successful write/truncate path already
verified.

## Neo6502 VACI BASIC fix validation

Result: passed on 2026-08-23 using a disposable USB volume and the normal
Neo1-23 profile. The test set sentinels across both packed regions:

```text
004A: 11
00EF: 22
00F0: 30 31 32 33 34 35 36 37 38 39 3A 3B 3C
00FD: 4D 4E 4F
0800: 55
0949: 66
094A: 77
0FFF: 88
```

The workspace was saved with `C100R` and `S`; the host reported an exact
2,230-byte file. After every sentinel was replaced with `00`, loading the file
with `C100R` and `L` restored all original values at `004A`, `00EF-00FF`,
`0800`, `0949-094A`, and `0FFF`. This test exercised `$004A-$00FF`,
`$0200-$021C`, `$0800-$0FFF`, and MSC registers `$D014-$D01C`.

## Neo6502 ordinary VACI hardening validation

Result: functional test passed on 2026-08-24 using a disposable USB volume and
the normal Neo1-23 profile. The test covered VACI RAM `$C100-$CB11`, its
installer-patched profile byte at `$C103`, ordinary transfer ranges, and MSC
registers `$D014-$D01C`. The only issue observed was that `WRITE ERR` and
`READ ERR` began on the address-prompt line; the follow-up image moves both to
a new line and occupies `$C100-$CB17`.

The passing procedure was:

1. In WozMon, inspect `C103`; it must contain `E0` for the Neo1-23 ROM boundary.
2. Enter `0300: 10 11 12 13 14 15 16 17 18 19 1A 1B 1C 1D 1E 1F`.
3. Enter `C100R`, choose `W`, name the file `VACI16.BIN`, and use start `0300`
   and end `030F`. Confirm on the host that the file is exactly 16 bytes.
4. Replace `$0300-$030F` with zeroes, enter VACI, choose `R`, select
   `VACI16.BIN`, and load it at `0300`. Confirm all 16 original values returned.
5. Enter VACI, choose `W`, name the file `REJECT.BIN`, and use start `0000` and
   end `FFFF`. VACI must print `WRITE ERR`, return to its menu, and create no
   file.
6. Enter VACI, choose `R`, select `VACI16.BIN`, and request destination `E000`.
   VACI must print `READ ERR`, return to its menu, leave `E000` unchanged, and
   remain able to perform the valid `0300` read again.

The follow-up image was then flashed and steps 5 and 6 were repeated. The user
confirmed that both error messages begin on a new line, closing the
formatting-only follow-up without another data-path retest.

## Neo6502 terminal-publication validation

Result: passed on 2026-08-24 using the normal Neo1-23 build. Sustained WozMon
output and scrolling remained stable without corrupted or partially mixed rows,
DVI dropouts, or hangs. Keyboard input continued working, and VACI returned to
WozMon normally.

The synchronized three-buffer publication path changes only how Pico core 0
hands completed terminal snapshots to the core-1 DVI renderer. It does not
change 6502 memory decoding; the relevant visible output path remains
`$D012/$D013`.

Using the normal Neo1-23 build:

1. Flash and confirm reset reaches a stable WozMon display.
2. Enter `0000.0FFF` several times to produce sustained output and scrolling.
3. While output is active and after it stops, confirm there are no corrupted or
   partially mixed rows, DVI dropouts, or hangs.
4. Confirm the keyboard still responds, enter `C100R`, then use `Q` to return to
   WozMon.
