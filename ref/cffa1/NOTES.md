# CFFA1 Port Notes (Neo1-23)

Purpose: capture explicit scope decisions for the Neo1-23 CFFA1 compatibility layer before and during implementation.

## 1) Phase Scope

### Phase 0 (first implementation target)
- [ ] Read-only block path end-to-end for one mounted image/file
- [ ] Minimal register/command subset required by chosen 6502-side test flow
- [ ] Deterministic status/error behavior documented

### Out of scope (for now)
- [ ] Multi-device support
- [ ] Full write/format flows
- [ ] Complete timing fidelity

## 2) Source Baseline Used

Primary references:
- `ref/cffa1/CFFA1_Manual_1.1.pdf`
- `ref/cffa1/CFFA1_API.s`
- `ref/cffa1/CFFA1.s`
- `ref/reload/prodos_hdd_msc.h`
- `ref/reload/prodos_hdd.h`

Notes:
- Document any intentional deviations from CFFA1 behavior and why.

## 3) Register/Command Mapping Decisions

### CFFA1-facing (6502-visible)
- Address range: `$AFF0-$AFFF` (ATA-like register window) plus ID bytes at `$AFDC/$AFDD`
- Registers implemented:
	- M0 stub register file for `$AFF0-$AFFF` with deterministic read/write behavior
	- Signature bytes: `$AFDC=$CF`, `$AFDD=$FA`
	- Status/alt-status mirror at offsets `$0F/$06`
	- DATA register `$AFF8` streams 512-byte block payload after read command
- Commands implemented:
	- M1 bridge commands (written to `$AFFF`):
		- `$00` = `PRODOS_STATUS` (bridge status query)
		- `$01` = `PRODOS_READ` (read one 512-byte block into DATA stream)

### Neo1 backend mapping
- Maps to existing MSC registers (`$D014`-`$D01A`)?
- New backend API required?
- Buffer ownership and size rules:

Current M0 choice:
- CFFA1 front-end is wired in bus decode, but no command translation to MSC yet.
- Existing MSC path remains authoritative for filer/loader flow.
- Next milestone (M1) will map CFFA1 read command semantics to current FatFs/MSC backend.

Current M1 choice:
- Bridge backend opens one disk image from mounted USB FatFs volume (`CFFA1.PO`/`CFFA1.HDV` preferred, then first `*.po|*.hdv|*.2mg` in root).
- Block number source is LBA registers `$AFFB-$AFFE` (little-endian 32-bit).
- Error codes returned via `$AFF9` (`$00`, `$01`, `$27`, `$28`, `$2D`).

## 4) Device/Image Model

- Image type(s): `.PO` preferred, `.HDV` / `.2MG` fallback by extension
- Block size: 512 bytes
- Mount policy (single image vs selectable): single auto-selected root image for M1
- End-of-media behavior: out-of-range block returns `$2D` (bad block)

## 5) Status/Error Model

- Busy/ready semantics:
- Error bit conventions:
- Error code mapping table location:

## 6) Milestones

### M0: Interface skeleton
- [x] Register stubs visible to 6502
- [x] Known status responses

### M1: Read sector path
- [x] Command accepted
- [x] Block returned correctly
- [x] Smoke test from monitor/tooling

### M2: Boot/useful software path
- [x] Test with selected CFFA1-side workflow
- [x] Confirm expected control flow

### M3: Write path and verify
- [x] WRITE command to block 1 (test-only RAM buffer)
- [x] End-to-end write-verify test (fill $AA, write, read, compare)
- [x] Runtime integration (installed at $1800 like M2)
- [x] Negative path: WRITE to block 2 returns BADBLOCK ($2D)

### M4: Expand deterministic write/read window
- [x] WRITE accepted for block 1 and block 2 (RAM-backed deterministic buffers)
- [x] End-to-end write/read for block 1 with $AA pattern
- [x] End-to-end write/read for block 2 with $55 pattern
- [x] Negative path moved to block 3 returns BADBLOCK ($2D)

### M5: Real image block-read verification
- [x] Removed deterministic RAM block override path from backend read command
- [x] Read block 2 directly from mounted `CFFA1.PO` image
- [x] Verified first 64 bytes match CiderPress II `rb CFFA1.PO 2` output

### M6: Interactive arbitrary-block inspector
- [x] Added `HHLL` block prompt loop at `1810R` (`CR` exits)
- [x] Reads requested block and dumps first `00-7F` bytes
- [x] Verified block `0000` boot bytes and block `0002` directory bytes on hardware

### M7: Minimal ProDOS-aware catalog parse
- [x] Reads catalog block `0002` at startup
- [x] Parses active entries and prints filename, key block, and EOF fields
- [x] Verified `HELLORLD.BIN` entry on hardware: `KEY=0007 EOF=00001D`

## 7) Test Vectors

### Golden inputs
- Image/file:
- Expected blocks:

### 6502 test programs
- Program: `src/ram/neo1_cffa1_m2_blockdrv.s` (`1810R` entry, now M7 catalog+inspect)
- Command sequence: `1810R` from WozMon


- M7 test flow:
	1. Check signature ($CF/$FA)
	2. STATUS command
	3. READ and parse catalog block `0002`
	4. Print active entries as `index: NAME KEY=hhhh EOF=hhhhhh`
	5. Enter `BLK HHLL` interactive loop (`CR` exits)
	6. READ requested block and print first `00-7F` bytes
- Expected output:
	- `NEO1 CFFA1 M7 CATALOG+INSPECT`
	- `SIG OK`
	- `STATUS OK`
	- `CATALOG BLK 0002`
	- `00: HELLORLD.BIN KEY=0007 EOF=00001D`
	- `BLK HHLL (CR=EXIT)?`
	- `READ BLK 0002 OK HEX[00-7F]:`
	- `00 00 03 00 F7 4E 45 57 44 49 53 4B 00 00 00 00`
	- `00 00 00 00 00 00 70 34 1C 14 00 00 70 34 1C 14`
	- `05 00 C3 27 0D 01 00 06 00 40 06 1C 48 45 4C 4C`
	- `4F 52 4C 44 2E 42 49 4E 00 00 00 00 07 00 01 00`

### Regression checks
- [x] Existing `0400R` filer still works
- [ ] Existing raw BIN load path still works

## 8) Open Questions

- How closely should the eventual Neo1-side menu follow original CFFA1 firmware behavior versus using a Neo1-specific UI shell?
- Should early CFFA1 save/write support be limited to pre-existing file overwrite flows before attempting full file create/delete semantics?
- When we return to the separate MSC byte-loader path, do we want classic ACI-like address syntax, menu prompts, or both?

## 9) Near-Term Roadmap

The storage work is now intentionally split into two peripherals:

### Track A: CFFA1 (active focus)
- `M6`: arbitrary block inspector (complete)
	- monitor-driven tool reads any requested ProDOS block and dumps bytes
- `M7`: minimal ProDOS-aware catalog parse (complete)
	- parses directory entries in block `0002`
	- prints filename + key block + EOF
- `M7.1`: load one selected file by key block (complete)
	- seedling load of first catalog entry to `$0300`; hardware-confirmed `HELLORLD!` at `300R`
- `M7.2`: CFFA1-style mini menu with C/L/B/Q (complete)
	- `C` re-catalogs, `L` prompts `LOAD INDEX (00-0C)` + `ADDR ($xxxx)` with auxtype default
	- `B` enters interactive block inspector, `Q` returns to WozMon
	- `00 SUCCESS` printed on successful load; hardware-confirmed `2eca8ef`

- `M7.3`: filename-based `LOAD FILE:` prompt (complete)
	- replace index selection with name-match search (closer to original CFFA1 UX)
	- requires input buffer and string compare against catalog entries
	- hardware-validated in-session (`LOAD FILE:` flow works as expected)

- `M8`: evaluate save/write scope (in progress)
	- initial policy chosen: explicit read-only / write-protect behavior
	- backend `PRODOS_WRITE` currently returns `$2B` (`WRITE PROTECT`) rather than generic bad-command
	- mini-menu `W` now issues a real `PRODOS_WRITE` probe against invalid block `$FFFF`, which is non-destructive and confirms low-level policy on hardware
	- first mutating step chosen: opt-in raw block writes only when the mounted image is explicitly named `CFFA1RW.PO` or `CFFA1RW.HDV`
	- default auto-mounted images (`CFFA1.PO`, `CFFA1.HDV`, discovered `*.po|*.hdv|*.2mg`) remain read-only
	- existing-file overwrite remains a later step after raw block write behavior is validated on a sacrificial writable image

- `M8.1`: BA1 compatibility branch (deferred)
	- gated on capture of known-good CFFA1-generated BA1 artifact (see Section 11)

Recommended next step:
- Hardware-validate `W` on the normal read-only image (`WRITE:2B` expected), then test real block writes using an explicit writable image name on sacrificial media.

### Track B: Neo1 filer / `VACI` / `NMI` (deferred)
- Treat current `0400R` loader as a proof of concept.
- Future `M1`:
	- choose file `00-99`
	- enter explicit 16-bit load address
	- load bytes to RAM
	- return to monitor without auto-run
- Future `M2`:
	- save a chosen memory range back to MSC storage
	- keep semantics primitive and ACI-like rather than ProDOS-aware

## 10) Decision Log

- 2026-03-16: Implemented M0 CFFA1 shim in Neo1 runtime. Added ID signature bytes at `$AFDC/$AFDD` and safe stub I/O window at `$AFF0-$AFFF` with mirrored status/alt-status.
- 2026-03-16: Implemented M1 bridge subset in shim: command `$00` status + `$01` read, 512-byte DATA stream via `$AFF8`, single-image auto-open on mounted USB volume.
- 2026-03-16: Verified M1 smoke test in monitor (`AFFF:00` status, `AFFF:01` read) and confirmed block data stream from `CFFA1.PO`.
- 2026-03-16: Verified M2 6502-side `CFBlockDriver` harness from `1810R` with `SIG OK`, `STATUS OK`, and correct block-0 bytes.
- 2026-03-16: Implemented M4 deterministic extension: RAM-backed write/read for blocks 1 and 2, negative write moved to block 3 (`$2D`), harness output updated to `NEO1 CFFA1 M4 TEST`.
- 2026-03-17: Verified M4 on hardware from WozMon via `1810R`: `WRITE BLK1 OK`, `READ BLK1 OK` with matching `AA` lines, `WRITE BLK2 OK`, `READ BLK2 OK` with matching `55` lines, and `NEG WRITE OK BADBLOCK:2D`, followed by expected `BRK` return.
- 2026-03-17: Implemented M5 real-image read path by removing deterministic RAM block read/write test overrides and validating `1810R` block-2 output against CiderPress II `rb CFFA1.PO 2` (including `NEWDISK` and `HELLORLD.BIN` directory bytes).
- 2026-03-21: Implemented and validated M6 interactive block inspector (`BLK HHLL`) with hardware reads of `0000` (boot code) and `0002` (directory block).
- 2026-03-21: Implemented and validated M7 catalog parse from block `0002`; hardware output shows `00: HELLORLD.BIN KEY=0007 EOF=00001D`.
- 2026-03-21: Implemented and validated M7.1 first-entry seedling load to `$0300`; hardware confirms `HELLORLD!` from `300R`.
- 2026-03-21: Fixed CR exit: replaced `BRK` with `JMP WOZMON_ENTRY` (`$FF00`); confirmed returns cleanly to WozMon.
- 2026-03-21: Implemented and validated M7.2 mini menu (`2eca8ef`); `C` re-catalogs, `L 00 0300` loads HELLORLD.BIN with address override and prints `00 SUCCESS`, `Q` exits to WozMon.
- 2026-03-21: Started M8 with explicit read-only policy. Neo1 CFFA1 backend now returns ProDOS `WRITE PROTECT` (`$2B`) for `PRODOS_WRITE`, and the mini-menu advertises `W` as a visible write/save status stub.
- 2026-03-21: Advanced M8 to a real driver-level write probe. `W` now executes `PRODOS_WRITE` against invalid block `$FFFF` so the normal image still proves `$2B` non-destructively, while explicitly named writable images (`CFFA1RW.PO` / `CFFA1RW.HDV`) unlock the first raw block-write path.

## 11) BA1 Format Notes (Paused)

Status:
- BA1 load-path implementation is intentionally paused until we have a known-good BA1 generated by actual CFFA1 `SAVE (WOZ BASIC)` flow.

Verified from source (`ref/cffa1/CFFA1.s`):
- `SAVE (WOZ BASIC)` menu path calls `SaveBASICFile` (API `$24`).
- CFFA1 BASIC files are ProDOS filetype `$F1` (`BA1`).
- First block format begins with `A1` + version byte (`'A'`, `'1'`, `$00` currently).
- Save captures zero-page `$4A..$FF` and BASIC program memory range derived from `LOMEM/HIMEM`.
- Load validates both filetype `$F1` and header `A1,version=0`; otherwise returns `eUnknownBASICFormat` (`$FE`).

Capture checklist for future BA1 work:
1. Generate BA1 on real CFFA1/Replica 1 (or VCFFA1 once save path exists):
	- Enter Woz BASIC (`E000R`).
	- Create a tiny test program (for deterministic bytes).
	- Use CFFA1 menu `S` (`SAVE (WOZ BASIC)`) and save as a short filename.
2. Preserve both artifacts:
	- Raw file copied from media (`.BA1` or ProDOS filename entry).
	- Directory metadata dump (filetype, auxtype, key block, EOF, storage type).
3. Verify expected signatures before implementing loader logic:
	- Filetype `$F1` in catalog entry.
	- First block bytes `[41 31 00]` at offset `0`.
	- Bytes `$4A..$FF` of first block contain saved BASIC zero-page snapshot.
4. Only after those checks, implement Neo1 BA1 path to mirror CFFA1 behavior:
	- Read first block to staging buffer.
	- Validate signature/version.
	- Restore `$4A..$FF`.
	- Load remaining file blocks at Auxtype/LOMEM destination.

Note on current sample:
- `ref/BASIC.BA1` in this repo does not currently match the expected CFFA1 first-block header at offset `0` (`41 31 00`), so it is not being used as canonical BA1 evidence.
