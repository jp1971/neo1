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
- The SDL host configuration now includes `neo1_msc_register_contract` and
  `neo1_vaci_basic_round_trip`. Both passed locally on 2026-08-23. The first
  compiles the production Pico MSC backend against an in-memory FatFs fake; the
  second executes the generated VACI image on the software 65C02.
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
did not independently establish the then-current 2,369-byte payload length. It
does establish that the old 512-byte cap is gone. The exact 16-byte overwrite
establishes short final-write length and truncation behavior.

## Implemented storage commands

The Pico VACI image is reproducibly generated from `neo1_vaci_v1.s`, is 2,415
bytes, and occupies `$C100-$CA6E`. Its visible commands are `R`, `W`, `L`, `S`,
and `Q`; destructive delete `D` is hidden.

The Pico VCFFA1 backend implements status, 512-byte block read, and 512-byte
block write. Writable operation requires a preferred writable image such as
`CFFA1RW.PO` or `CFFA1RW.HDV`; fallback images are opened read-only.

VCFFA1 is retained as an optional Replica 1 compatibility feature, but the
reliability work in defects 5 and 14-17 is deferred until after the next
portable-core checkpoint. VACI remains the preferred Apple-1 storage path.
Until that work resumes, use VCFFA1 `W` and `D` only with disposable images;
the verified catalog/load workflow may continue to be used within the stated
directory, bitmap, file-size, and destination limitations.

## Known defects and unverified behavior

1. **The corrected VACI BASIC memory restore awaits physical confirmation.**
   `L` now restores sector 0's `$0800-$0949` bytes before continuing at `$094A`.
   The generated payload passes a software-65C02 test that round-trips all 2,230
   bytes, but the required Neo6502 smoke test below is still outstanding.
2. **The corrected VACI scratch preservation awaits physical confirmation.**
   VACI now captures `$F0-$FC` at entry in `$0210-$021C`, writes those saved
   bytes into the zero-page snapshot, and defers restoring them during load
   until all MSC operations finish. Existing files retain the same layout, but
   scratch bytes corrupted by the old saver cannot be reconstructed.
3. **VACI transfer bounds and lifecycle are incomplete.** Read destinations and
   write sources are not protected from address wrap or overlap with VACI,
   page-2 filename state, I/O, stack, or ROM. A `$0000-$FFFF` write wraps its
   16-bit length to zero, and a successful ordinary read leaves its indexed
   file open. The VACI linker ceiling also permits growth beyond safe payload
   RAM even though the current image remains at `$C100-$CA6E`.
4. **VACI status polling is permissive and unbounded.** `WaitReady` has no
   timeout and accepts every `$01-$7F` value as success, although the MSC
   contract assigns `$01` to ready and leaves the other positive values
   reserved.
5. **Generic VCFFA1 `.po` discovery is faulty.** The extension matcher handles
   `.hdv` and `.2mg`, but its `.po` comparison uses the wrong character
   positions. Preferred names such as `CFFA1RW.PO` and `CFFA1.PO` still work.
6. **SDL does not install VACI or the VCFFA1 RAM utility.** Its MSC file and
   directory commands are accepted as no-ops and its storage path maps raw
   sectors to one host image. SDL preset labels therefore do not imply Pico
   VACI file behavior.
7. **Neo1-50 hardware behavior is build-verified only in this pass.** The dated
   physical smoke result above is for Neo1-23.
8. **SDK 2.3.0 is unverified.** Upgrade validation requires both Pico profile
   builds followed by the reset, DVI, keyboard, MSC, VACI, and VCFFA1 hardware
   smoke tests.
9. **Automated coverage remains limited.** Focused host tests cover the Pico MSC
   register protocol and execute the VACI BASIC save/load round trip on the
   software 65C02. There are still no focused tests for shared memory decoding,
   PIA behavior, other VACI commands, VCFFA1, or broad CPU compatibility.
10. **CPU backend value 2 is not usable.** `neo1_cpu_backend.h` declares a
   `MOS6502` backend and includes `mos6502cpu.h` when it is selected, but that
   header is not present in the repository. Current presets use the physical
   W65C02 backend (1) or the soft-65C02 adapter (3); value 2 is not a supported
   configuration.
11. **MSC register decoding is not independently selectable.** The shared
   machine always routes the supported `$D014-$D01C` accesses to an MSC
   implementation. `NEO1_ENABLE_VACI` controls installation of the 6502-side
   VACI payload, not ownership of those addresses.
12. **Pico terminal publication is not synchronized across cores.** Core 0
   mutates the caller-owned terminal while core 1 copies it at a frame boundary.
   The dirty flag and buffer indices are volatile but not lock-protected, so the
   source copy is not guaranteed to be atomic.
13. **SDL execution is not wall-clock or cycle paced.** `neo1_exec(2000)` runs
    about 2,043 soft ticks per UI iteration, but each tick is a complete
    instruction and the runner does not govern the batch using elapsed time.
14. **The VCFFA1 utility's create/delete updates are not transactional.** New
    file creation commits allocation bits before its directory and sapling
    index writes, without rollback. Delete may free an index block after an
    index-read error, ignores a bitmap-write error, and can then remove the
    directory entry. Failures can leak blocks or leave ProDOS metadata
    inconsistent; use a disposable image for write/delete testing.
15. **VCFFA1 existing-file writes do not update catalog metadata.** The utility
    writes the requested bytes into an existing seedling or sapling but leaves
    its EOF, blocks-used, auxtype, and other directory fields unchanged when
    source or length differs from the entry.
16. **The VCFFA1 utility has hard-coded filesystem limits.** Catalog, lookup,
    create, and delete inspect only root directory block 2. Allocation/freeing
    uses only the first bitmap block and assumes at most 4096 volume blocks;
    load/create/write support only seedling and two-data-block sapling files
    through 1024 bytes. Load destinations are not range checked.
17. **The VCFFA1 block driver can wait forever for DRQ.** Read/write checks the
    error register immediately after command issue, then polls DRQ without a
    timeout or further busy/error checks. A device or backend that never raises
    DRQ stalls the 6502 utility indefinitely.

## Storage-test expectations still outstanding

Future write-path validation should use disposable media or images and cover
missing media, read-only media, invalid commands/ranges, out-of-range blocks,
and short I/O in addition to the successful write/truncate path already
verified.

## Required Neo6502 smoke test for the VACI BASIC fix

Use a disposable USB volume and the normal Neo1-23 profile. From WozMon, set
sentinels across both packed regions:

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

Enter `C100R`, choose `S`, and save to a new filename. Confirm on the host that
the file is exactly 2,230 bytes. Back in WozMon, replace every sentinel above
with `00`. Enter `C100R`, choose `L`, and select the saved file. After VACI
returns to WozMon, inspect `004A`, `00EF-00FF`, `0800`, `0949-094A`, and `0FFF`;
all original sentinel values must be restored. This test affects `$004A-$00FF`,
`$0200-$021C`, `$0800-$0FFF`, and MSC registers `$D014-$D01C`.
