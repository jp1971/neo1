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
- `M7.1`: load one selected file by key block (next)
	- target single-seedling style flow for tiny binaries like `HELLORLD.BIN`
	- load file payload to a fixed RAM address and report entry/load pointers
- `M8`: evaluate save/write scope
	- decide whether to support raw block writes only, existing-file overwrite, or fuller ProDOS mutation

Recommended next step:
- Continue on the CFFA1 track while current context is fresh.
- The next best milestone is `M7.1`: file payload load using parsed catalog metadata.

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
