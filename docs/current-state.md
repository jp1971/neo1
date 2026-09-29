# Neo1 Current State

Last updated: 2026-09-28

This document records evidence-backed capabilities and known defects. It is a
snapshot, not the architecture contract or a roadmap.

## Build baseline

- The supported Neo1 Pico workflow uses the Raspberry Pi Pico VS Code extension
  with CMake Tools and the named CMake configure/build presets.
- Raspberry Pi Pico SDK 2.3.0, Arm GNU Toolchain 13.3.Rel1, and picotool 2.3.0
  are the verified hardware-build baseline.
- The SDK 2.3.0 upgrade gate passed on 2026-08-27. Normal and diagnostic
  Neo1-23 and Neo1-50 Pico presets configured and built against the
  extension-managed SDK, while retaining Neo1's checked-in
  `olimex_neo6502.h` board definition. Both SDL profiles built, all twelve host
  tests passed, Neo1-50 reached WozMon headlessly, and the generated VACI image
  matched its checked-in header. Both build directories were restored to the
  normal Neo1-23 profile. The user then supplied a passing normal Neo1-23
  hardware result.
- Fruit Jam checkpoint 0 passed on 2026-09-01 without adding a Neo1 target or
  changing existing-target code. The standalone probe selected the official
  `adafruit_fruit_jam` board, built as an RP2350 ARM Secure image with SDK
  2.3.0 and Arm GNU Toolchain 13.3.Rel1, flashed through picotool 2.3.0, and
  enumerated as USB CDC. Its transcript confirmed the board definition,
  RP2350B platform, 16 MB flash, 8 MB PSRAM, and the expected HSTX,
  GPIO/PIO-USB-host, and SD pins. Picotool identified the tested silicon as
  RP2350 revision A4 in the QFN80 package; the probe then produced a sustained
  heartbeat.
- Fruit Jam checkpoint 1 qualified the current software-CPU boundary on
  2026-09-01. The reusable runner no longer patches `$0000-$0002`; SDL owns
  that compatibility jump explicitly. IRQ masking, IRQ/NMI entry and RTI, BRK,
  interrupt-entry cycles, decimal ADC/SBC flags, reset-vector execution, and
  the `STZ`, `PHX/PHY`, and `PLX/PLY` forms used by Neo1 RAM utilities now have
  focused host coverage. The audit found and corrected fake65c02's decimal
  `99 + 01` overflow flag. All thirteen host tests passed, both SDL profiles
  reached WozMon headlessly, both Pico profiles built with SDK 2.3.0, and both
  build directories were restored to normal Neo1-23.
- Fruit Jam checkpoint 1A integrated qe6502 1.0.0, pinned at commit
  `8ae9074203e0a6c46ae687e19194c6a3f4bc1d07`, as fake65c02's replacement on
  2026-09-01. The MIT-licensed C11 core stores each WDC65C02 in a 16-byte
  caller-owned value and exposes reset, IRQ, NMI, and every memory transaction
  as explicit bus cycles without callbacks, allocation, or mutable global CPU
  state. Its WDC Klaus standard and extended tests passed; all 2,540,000
  available WDC65C02 SingleStepTests cases passed with final state and bus-cycle
  comparison; its functional, save/load, netlist, and interrupt-lockstep subset
  passed; and the unmodified static core compiled for the Fruit Jam RP2350 ARM
  target with SDK 2.3.0. Each software runner now owns its CPU and explicit bus
  request; two-runner coverage passes and the global callback/state restriction
  and old core are gone. The integration also corrected the contract test to
  reflect W65C02 IRQ/NMI recognition after the opcode already on the bus. All
  thirteen host tests pass, both SDL profiles reach WozMon headlessly, and both
  Pico profiles build with SDK 2.3.0. Both working build directories were
  restored to normal Neo1-23.
- Fruit Jam checkpoint 2 passed on 2026-09-01. Named Neo1-23 and Neo1-50 serial
  presets build in `build-fruitjam/` for the official `adafruit_fruit_jam`
  `rp2350-arm-s` target with SDK 2.3.0. The target owns one shared machine, one
  qe6502 runner, elapsed-time scheduling, USB-CDC output, and serial Ctrl-R
  lifecycle reset; video, Apple-1 input, storage, VACI, VCFFA1, audio, PicoDVI,
  physical-W65C02 code, and Neo1-50 entry stubs remain absent. Both profiles
  were flashed to Fruit Jam serial `21A41F42391B99FA`; each reported
  `reset=$FF00 entry=$FF00`, printed the WozMon `\` prompt, and repeated the
  sequence after Ctrl-R. All thirteen host tests passed, both SDL profiles
  reached WozMon headlessly, and both Pico profiles built. Moving the Neo6502
  TinyUSB host configuration into its target directory preserved both Pico
  builds while preventing it from contaminating Fruit Jam USB-device CDC. The
  Pico, SDL, and Fruit Jam build directories were restored to Neo1-23.
- Fruit Jam checkpoint 2A passed on 2026-09-01. The target-local USB-CDC console
  adapter uppercases input, collapses CRLF, applies one-byte backpressure at the
  shared Apple-1 PIA latch, and reserves Ctrl-R for lifecycle reset. On Fruit
  Jam `21A41F42391B99FA`, lowercase console bursts deposited and examined `AA`,
  `BB`, and `CC` at `$0300-$0302` using CR, LF, and CRLF respectively; each
  line ending submitted one command. Ctrl-R then returned to WozMon with
  `reset=$FF00 entry=$FF00`. Both Fruit Jam and Pico profiles built with SDK
  2.3.0, all thirteen host tests passed, and all build directories were
  restored to Neo1-23. A 2026-09-02 follow-up expanded outgoing Apple-1 CR to
  CRLF in the Fruit Jam console transport; physical testing confirmed proper
  line advancement and successful interactive use of WozMon, Integer BASIC,
  and Krusader. The Fruit Jam remains flashed with Neo1-23.
- Fruit Jam checkpoint 3 passed on 2026-09-28. Both personalities compile with
  the renamed `neo1-fruitjam-23-dvi` and `neo1-fruitjam-50-dvi` presets. The
  Fruit Jam-only HSTX renderer produces 640x480 timing from compact internal-
  SRAM terminal rasters and scanline buffers, without PicoDVI, PSRAM, or a full
  framebuffer. Core 0 publishes coherent snapshots of the shared 40x24
  terminal and core 1 owns rasterization and video DMA. Physical Neo6502 and
  Fruit Jam now share the verified CR, form-feed, printable-glyph, and ignored-
  control-byte output policy. Serial console input and output remain enabled;
  Ctrl-L clears only the DVI terminal and Ctrl-R resets the terminal, PIA, and
  CPU without delivering either control to the Apple-1 keyboard latch. All
  thirteen host tests passed; SDL-23 and SDL-50 reached WozMon headlessly; and
  both Pico profiles built with SDK 2.3.0.
- The 2026-09-28 Fruit Jam checkpoint-3 physical pass established a stable
  640x480 DVI signal, correct WozMon prompt/cursor rendering, interactive
  monitor commands, and a sustained `E000.EFFF` dump with scrolling. Early
  prototypes exposed a stray prompt-adjacent raster mark and HSTX starvation
  under output; direct DMA from pre-expanded text rasters removed the mark and
  signal resets, while bounded 30 Hz snapshot publication removed sustained-
  output starvation. Physical testing also confirmed that Ctrl-L clears the
  DVI terminal and repeated Ctrl-R resets return reliably to WozMon without
  destabilizing video or serial.
- A headless Neo6502 regression passed on 2026-09-20 using the normal Neo1-23
  profile built with SDK 2.3.0 and flashed immediately before testing. Through
  the serial console, Ctrl-R reached WozMon, `$0300` accepted and returned an
  `A5` deposit, `E000R` entered Integer BASIC, and `F000R` entered Krusader.
  With USB storage present from boot, VACI `R` listed the root directory and
  cancellation plus `Q` returned cleanly to WozMon. The test was read-only;
  video, USB keyboard, storage writes, and live media reinsertion were not
  exercised.
- A 2026-09-22 follow-up confirmed DVI video and USB keyboard operation on the
  Neo6502. The Pico renderer now retains full-width 640×480 output by default
  and offers an F12-controlled centered 480-pixel compensation view for 16:9
  displays that stretch 4:3 input. On 2026-09-27, the user confirmed that F12
  switches the physical display successfully. The 1024×768 4:3 Beetronics has
  not yet received the separate full-width comparison.
- Normal Neo1-23 and Neo1-50 Pico builds passed on 2026-08-24 with the VACI
  error-line follow-up. Normal and diagnostic builds for both profiles passed
  on 2026-08-23; ELF inspection confirmed normal builds omit verbose trace
  strings and diagnostic builds retain them. The working `build/` directory
  was restored to the normal Neo1-23 profile afterward.
- Normal and diagnostic Pico presets set `NEO1_DIAGNOSTICS` explicitly so
  switching back to a normal profile restores concise serial output.
- The SDL-23 target builds locally and now schedules its qe6502 software CPU
  from a monotonic elapsed-time budget using explicit represented bus cycles. A
  headless WozMon startup smoke and the focused cycle-budget test passed on
  2026-08-24; this does not establish equivalent Pico storage or hardware
  behavior.
- The build-wide CPU-backend selector and all active Reload/CHIPS execution
  surfaces have been removed. SDL links the explicit `neo1_soft_runner`; Pico
  owns an explicit `neo1_wdc_runner` beside the shared machine.
- `NEO1_ENABLE_MSC` now controls `$D014-$D01C` ownership explicitly. Focused
  host tests prove enabled accesses reach the device and disabled accesses use
  backing RAM; VACI-without-MSC configurations are rejected.
- The SDL host configuration now includes thirteen focused tests. All passed
  locally through 2026-09-01: the production Pico MSC contract against an
  in-memory FatFs fake, the SDL raw MSC backend and separate VCFFA1 state, the
  generated VACI BASIC/ordinary transfer paths, enabled and disabled MSC and
  VCFFA1 address decode, the shared Apple-1 PIA contract, the CPU-neutral
  RAM/ROM/address-space contract, software-CPU cycle budgeting and CPU contract,
  the real Neo1-23/Neo1-50 profile layouts, and the shared terminal grid plus
  preserved Pico/SDL control-byte policies.
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
- Portable-core checkpoint 9 now gives both runners one instance-owned MSC
  register protocol for `$D014-$D01C`. Pico supplies only FatFs operations;
  SDL supplies only its raw-image sector operations, with an empty directory
  and no raw-image truncation. The SDL duplicate command/register state machine
  and its false-success/post-command-WRITE accommodations are gone; VCFFA1 is
  unchanged. All twelve host tests pass, both SDL profiles reach WozMon
  headlessly, and Pico-23, Pico-50, and diagnostic Pico-23 build with SDK
  2.1.0. Both build trees are restored to normal Neo1-23. The normal Neo1-23
  disposable-media gate passed on 2026-08-26 with the cold-boot USB-storage
  limitations recorded below.
- The post-checkpoint cleanup on 2026-08-26 tracked the historical baseline and
  engineering guide, closed superseded planning state, and simplified SDL's
  presentation call by removing ignored framebuffer arguments and dead
  bookkeeping. All twelve host tests pass, both SDL profiles build and reach
  WozMon headlessly, and both Pico profiles build with SDK 2.1.0. Both working
  build directories are restored to normal Neo1-23. No shared-machine, Pico,
  or 6502-visible behavior changed, so no additional physical gate is required.
## Last Neo6502 hardware validation

User-supplied results from 2026-08-22 through 2026-09-27 used the Neo1-23
profile with VACI and VCFFA1 enabled. The latest follow-up confirms DVI video
and USB keyboard after the passing headless, read-only normal Neo1-23 gate, and
the F12 widescreen-stretch correction now has a passing physical result.
Checkpoint 9 remains the latest detailed disposable-media result: ordinary
storage operation passed; live USB reinsertion was not established, and
recovery was verified by power cycling with the medium inserted. The Beetronics
comparison remains outstanding.

| Capability | Result | Evidence |
| --- | --- | --- |
| Reset and WozMon | Verified | Reset reached WozMon and monitor commands executed |
| Neo1-23 ROM entries | Verified | `E000R` entered Integer BASIC and `F000R` entered Krusader |
| DVI video | Verified; Beetronics comparison pending | The shared-grid checkpoint passed sustained WozMon output/scrolling, form-feed clear, cursor, keyboard, and VACI-return checks on Neo1-23; DVI operation was reconfirmed on 2026-09-22 and the F12 aspect toggle passed physically on 2026-09-27 |
| Serial console | Verified | Normal and diagnostic profiles produced their intended transcripts while preserving monitor output |
| USB HID keyboard | Verified | User explicitly reconfirmed keyboard input on 2026-09-22 |
| Apple-1 PIA-like interface | Verified | Checkpoint 3 preserved WozMon memory examine/deposit, DVI/serial display output, USB/serial input, VACI directory/return, and stable scrolling |
| CPU-neutral machine boundary | Verified | Checkpoint 4 preserved WozMon reset, both Neo1-23 ROM entries, `$0300` memory deposit/examine, VACI directory/cancel/return, USB and serial input, DVI output, and stable scrolling |
| Explicit software-runner boundary | Verified | Checkpoint 5 preserved the same Neo1-23 WozMon, ROM-entry, memory, VACI, input, DVI, and scrolling gate after removing the build-wide CPU selector |
| Explicit physical-runner boundary | Verified | Checkpoint 6 preserved the defined reset-vector, WozMon opcode/device, and relative stack-access trace plus the complete normal Neo1-23 functional gate |
| Clock-qualified physical reset | Verified | Checkpoint 7 preserved the defined diagnostic trace, repeated Ctrl-R reliably returned to WozMon, and the complete normal Neo1-23 functional gate passed |
| Shared machine profiles | Verified | Checkpoint 8 preserved WozMon reset, both Neo1-23 ROM entries, `$0300` memory deposit/examine, VACI directory/cancel/return, USB and serial input, DVI output, and stable scrolling |
| Shared MSC register protocol | Verified with limitations | Checkpoint 9 preserved the normal Neo1-23 gate, ordinary VACI list/load/run, an exact 16-byte disposable write and readback, and VCFFA1 signature/status/read/catalog behavior. With media unavailable, BASIC `L` returned silently to the VACI menu; live reinsertion was not established, while a power cycle with the medium inserted restored storage |
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
controller and firmware compatibility work is deferred until after Fruit Jam
checkpoint 7. Rich Dreher granted written permission on 2026-09-27 to use the
CFFA1 binary in Neo1; the exact scope, attribution requirement, and staged
integration policy are recorded in `docs/vcffa1-execution-plan.md`. The ROM
remains in ignored local references until that workstream resumes. VACI
remains the preferred Apple-1 storage path.
Until that work resumes, use VCFFA1 `W` and `D` only with disposable images;
the verified catalog/load workflow may continue to be used within the stated
directory, bitmap, file-size, and destination limitations.

## Known defects and unverified behavior

1. **Generic VCFFA1 `.po` discovery is faulty.** The extension matcher handles
   `.hdv` and `.2mg`, but its `.po` comparison uses the wrong character
   positions. Preferred names such as `CFFA1RW.PO` and `CFFA1.PO` still work.
2. **SDL does not install VACI or the VCFFA1 RAM utility.** It uses the shared
   MSC command/register protocol, but its backend maps sector reads/writes to
   one raw host image, reports an empty directory, and cannot truncate that
   image after a short logical write. SDL preset labels therefore do not imply
   Pico VACI file behavior.
3. **Neo1-50 hardware behavior is build-verified only in this pass.** The dated
   physical smoke result above is for Neo1-23.
4. **Fruit Jam physical I/O remains incomplete.** Checkpoint 3 HSTX video is
   physically verified. USB-host keyboard input, microSD storage, VACI,
   VCFFA1, and audio are not implemented; USB-CDC remains the temporary
   Apple-1 input transport.
5. **The shared terminal is a modernized presentation model.** It provides a
   conventional 40x24 grid with immediate row advancement and scrolling. A
   2026-09-28 physical Replica 1 observation found that its Apple-1-style video
   progression renders left-to-right rather than as discrete line updates.
   Neo1 has not yet traced that behavior against the original Apple-1 circuit,
   defined an exact compatibility contract, or implemented selectable modern
   and period-correct modes.
6. **Video color is currently target-defined and inconsistent.** Fruit Jam
   explicitly renders RGB332 `$1C` green on black. Neo6502 uses PicoDVI's 1-bpp
   encoder and is believed to appear white on black, but the two targets have
   not received a controlled side-by-side comparison. A future presentation
   feature should provide selectable white, green, and period-style amber/brown
   profiles without changing 6502-visible terminal behavior.
7. **Automated coverage remains limited.** Focused host tests cover the shared
   MSC protocol with the Pico FatFs backend, the SDL raw MSC backend and basic
   VCFFA1 separation, and execute VACI BASIC plus ordinary read/write paths on
   the software 65C02; enabled/disabled MSC and VCFFA1 address decode, the
   Apple-1 PIA-like register/latch contract, both profile RAM/ROM layouts and
   vectors, reset preservation, and soft-instruction cycle budgeting are also
   covered. There are still no focused tests for VACI delete, the complete
   VCFFA1 protocol/error behavior, snapshots, or broad CPU compatibility.
8. **The VCFFA1 utility's create/delete updates are not transactional.** New
    file creation commits allocation bits before its directory and sapling
    index writes, without rollback. Delete may free an index block after an
    index-read error, ignores a bitmap-write error, and can then remove the
    directory entry. Failures can leak blocks or leave ProDOS metadata
    inconsistent; use a disposable image for write/delete testing.
9. **VCFFA1 existing-file writes do not update catalog metadata.** The utility
    writes the requested bytes into an existing seedling or sapling but leaves
    its EOF, blocks-used, auxtype, and other directory fields unchanged when
    source or length differs from the entry.
10. **The VCFFA1 utility has hard-coded filesystem limits.** Catalog, lookup,
    create, and delete inspect only root directory block 2. Allocation/freeing
    uses only the first bitmap block and assumes at most 4096 volume blocks;
    load/create/write support only seedling and two-data-block sapling files
    through 1024 bytes. Load destinations are not range checked.
11. **The VCFFA1 block driver can wait forever for DRQ.** Read/write checks the
    error register immediately after command issue, then polls DRQ without a
    timeout or further busy/error checks. A device or backend that never raises
    DRQ stalls the 6502 utility indefinitely.
12. **USB-storage recovery is cold-boot verified only.** During checkpoint 9,
    the specialized VACI BASIC `L` command returned silently to its menu when
    storage was unavailable instead of printing an error. Live reinsertion did
    not establish recovery; power cycling with the USB medium already inserted
    restored normal listing, loading, and writing. The shared register-level
    missing-media error paths remain covered by host tests.
13. **The Pico aspect control has not been compared on the Beetronics.** F12
    successfully switches to the centered 480-pixel widescreen-stretch
    correction on tested hardware. The 1024×768 4:3 Beetronics still needs a
    check that restart-default native 640×480 uses its full display area and
    that toggling twice returns cleanly to that view.

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
