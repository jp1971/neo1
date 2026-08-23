# Neo1 Current State

Last updated: 2026-08-23

This document records evidence-backed capabilities and known defects. It is a
snapshot, not the architecture contract or a roadmap.

## Build baseline

- The supported Neo1 Pico workflow uses the Raspberry Pi Pico VS Code extension
  with CMake Tools and the named CMake configure/build presets.
- Raspberry Pi Pico SDK 2.1.0 and Arm GNU Toolchain 13.3.Rel1 are the verified
  hardware-build baseline.
- Builds passed on 2026-08-23 for the normal and diagnostic Neo1-23 and
  Neo1-50 Pico presets. ELF inspection confirmed that normal builds omit the
  verbose trace strings and diagnostic builds retain them. The working
  `build/` directory was restored to the normal Neo1-23 profile afterward.
- Normal and diagnostic Pico presets set `NEO1_DIAGNOSTICS` explicitly so
  switching back to a normal profile restores concise serial output.
- The SDL-23 target also builds locally, but that build does not establish
  equivalent storage or hardware behavior.
- SDK 2.3.0 has not been configured, built, or hardware-tested.

## Last Neo6502 hardware validation

User-supplied results from 2026-08-22 through 2026-08-23 used the Neo1-23
profile with VACI and VCFFA1 enabled.

| Capability | Result | Evidence |
| --- | --- | --- |
| Reset and WozMon | Verified | Reset reached WozMon and monitor commands executed |
| Neo1-23 ROM entries | Verified | `E000R` entered Integer BASIC and `F000R` entered Krusader |
| DVI video | Verified | User reported working display output |
| Serial console | Verified | Normal and diagnostic profiles produced their intended transcripts while preserving monitor output |
| USB HID keyboard | Verified | User explicitly verified keyboard input |
| USB MSC/FatFs | Verified | Media mounted and directory/file workflows operated |
| VACI read/load | Verified | `.BIN` files loaded and ran |
| VACI write | Verified | A write larger than 512 bytes produced a host-reported 2 KB file; rewriting the same name produced an exact 16-byte file, confirming multi-sector operation and truncation |
| VCFFA1 | Verified at workflow level | User reported VCFFA1 working and successfully ran a loaded `.po` image |

The first VACI write was displayed by the host as rounded “2 KB,” so that test
does not independently establish an exact 2,369-byte length. It does establish
that the old 512-byte cap is gone. The exact 16-byte overwrite establishes
short final-write length and truncation behavior.

## Implemented storage commands

The Pico VACI image is reproducibly generated from `neo1_vaci_v1.s`, is 2,369
bytes, and occupies `$C100-$CA40`. Its visible commands are `R`, `W`, `L`, `S`,
and `Q`; destructive delete `D` is hidden.

The Pico VCFFA1 backend implements status, 512-byte block read, and 512-byte
block write. Writable operation requires a preferred writable image such as
`CFFA1RW.PO` or `CFFA1RW.HDV`; fallback images are opened read-only.

## Known defects and unverified behavior

1. **VACI BASIC load is not a correct inverse of save.** `S` stores 330 bytes
   from `$0800-$0949` in sector 0, but `L` currently discards those bytes and
   resumes copying at `$094A`. Do not rely on `L`/`S` to preserve a BASIC
   workspace until this is fixed and hardware-tested.
2. **Generic VCFFA1 `.po` discovery is faulty.** The extension matcher handles
   `.hdv` and `.2mg`, but its `.po` comparison uses the wrong character
   positions. Preferred names such as `CFFA1RW.PO` and `CFFA1.PO` still work.
3. **SDL does not install VACI or the VCFFA1 RAM utility.** Its MSC file and
   directory commands are accepted as no-ops and its storage path maps raw
   sectors to one host image. SDL preset labels therefore do not imply Pico
   VACI file behavior.
4. **Neo1-50 hardware behavior is build-verified only in this pass.** The dated
   physical smoke result above is for Neo1-23.
5. **SDK 2.3.0 is unverified.** Upgrade validation requires both Pico profile
   builds followed by the reset, DVI, keyboard, MSC, VACI, and VCFFA1 hardware
   smoke tests.
6. **Automated coverage is absent.** Current CMake builds define no focused
   host tests for memory decoding, PIA behavior, storage protocols, or the VACI
   payload.
7. **CPU backend value 2 is not usable.** `neo1_cpu_backend.h` declares a
   `MOS6502` backend and includes `mos6502cpu.h` when it is selected, but that
   header is not present in the repository. Current presets use the physical
   W65C02 backend (1) or the soft-65C02 adapter (3); value 2 is not a supported
   configuration.
8. **MSC register decoding is not independently selectable.** The shared
   machine always routes the supported `$D014-$D01C` accesses to an MSC
   implementation. `NEO1_ENABLE_VACI` controls installation of the 6502-side
   VACI payload, not ownership of those addresses.

## Storage-test expectations still outstanding

Future write-path validation should use disposable media or images and cover
missing media, read-only media, invalid commands/ranges, out-of-range blocks,
and short I/O in addition to the successful write/truncate path already
verified.
