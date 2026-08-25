# Neo1 Current State

Last updated: 2026-08-24

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
- The SDL-23 target also builds locally, but that build does not establish
  equivalent storage or hardware behavior.
- CPU adapter selector 2 has been retired. Configure now accepts only the
  physical W65C02 adapter (1) or software 65C02 adapter (3), and each runner
  rejects the other target's adapter explicitly.
- The SDL host configuration now includes `neo1_msc_register_contract` and
  `neo1_vaci_payload_contract`. Both passed locally on 2026-08-24. The first
  compiles the production Pico MSC backend against an in-memory FatFs fake; the
  second executes BASIC and ordinary transfer paths in the generated VACI image
  on the software 65C02.
- SDK 2.3.0 has not been configured, built, or hardware-tested.

## Last Neo6502 hardware validation

User-supplied results from 2026-08-22 through 2026-08-24 used the Neo1-23
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
| VACI BASIC save/load | Verified | The 2,230-byte sentinel test restored all checked values across `$004A-$00FF` and `$0800-$0FFF` |
| VACI ordinary-transfer hardening | Verified | The 2026-08-24 smoke test confirmed the profile marker, valid 16-byte write/read, 64 KB write rejection, Neo1-23 ROM-destination rejection, close behavior, unchanged ROM data, and error messages beginning on new lines |
| VCFFA1 | Verified at workflow level | User reported VCFFA1 working and successfully ran a loaded `.po` image |

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
reliability work in defects 1 and 9-12 is deferred until after the next
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
   the software 65C02. There are still no focused tests for shared memory
   decoding, PIA behavior, VACI delete, VCFFA1, or broad CPU compatibility.
6. **MSC register decoding is not independently selectable.** The shared
   machine always routes the supported `$D014-$D01C` accesses to an MSC
   implementation. `NEO1_ENABLE_VACI` controls installation of the 6502-side
   VACI payload, not ownership of those addresses.
7. **Pico terminal publication is not synchronized across cores.** Core 0
   mutates the caller-owned terminal while core 1 copies it at a frame boundary.
   The dirty flag and buffer indices are volatile but not lock-protected, so the
   source copy is not guaranteed to be atomic.
8. **SDL execution is not wall-clock or cycle paced.** `neo1_exec(2000)` runs
    about 2,043 soft ticks per UI iteration, but each tick is a complete
    instruction and the runner does not govern the batch using elapsed time.
9. **The VCFFA1 utility's create/delete updates are not transactional.** New
    file creation commits allocation bits before its directory and sapling
    index writes, without rollback. Delete may free an index block after an
    index-read error, ignores a bitmap-write error, and can then remove the
    directory entry. Failures can leak blocks or leave ProDOS metadata
    inconsistent; use a disposable image for write/delete testing.
10. **VCFFA1 existing-file writes do not update catalog metadata.** The utility
    writes the requested bytes into an existing seedling or sapling but leaves
    its EOF, blocks-used, auxtype, and other directory fields unchanged when
    source or length differs from the entry.
11. **The VCFFA1 utility has hard-coded filesystem limits.** Catalog, lookup,
    create, and delete inspect only root directory block 2. Allocation/freeing
    uses only the first bitmap block and assumes at most 4096 volume blocks;
    load/create/write support only seedling and two-data-block sapling files
    through 1024 bytes. Load destinations are not range checked.
12. **The VCFFA1 block driver can wait forever for DRQ.** Read/write checks the
    error register immediately after command issue, then polls DRQ without a
    timeout or further busy/error checks. A device or backend that never raises
    DRQ stalls the 6502 utility indefinitely.

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
