# Neo1 Architecture

This document describes the stable 6502-visible machine contracts. Platform
implementation status and known deviations belong in `docs/current-state.md`.

## Machine boundary

Neo1 presents one 64 KB address space to a 65C02. The machine model owns RAM,
ROM protection, Apple-1 keyboard/display behavior, and optional storage-device
address decoding. A runner supplies CPU bus cycles: Neo1 Pico observes a
physical W65C02, while SDL drives the same read/write surface from a software
CPU.

The ordinary shared `neo1_machine` C module implements that CPU-neutral state
and explicit bus surface. `neo1_soft_runner` attaches a software CPU to that
surface and owns instruction stepping, reset/interrupt presentation, and cycle
budgeting for SDL and future host-style targets. Pico owns a separate
`neo1_wdc_runner` that drives the physical W65C02 clock, reset and interrupt
pins, reads the Neo6502 address/data latches, and forwards each captured bus
access to `neo1_machine_read()` or `neo1_machine_write()`. Platform runners may
install RAM-resident tools directly in the machine's backing store; those tools
do not become machine-model policy.

The backing store is initialized with `$00` at even addresses and `$FF` at odd
addresses. A selected ROM image is then copied into top memory. Reads and
writes use the backing store unless one of the explicitly decoded devices below
owns the address. Writes at or above the profile's ROM-protection boundary are
ignored.

## Reset and top memory

The reset vector is read from `$FFFC-$FFFD` and resolves to WozMon at `$FF00` in
both personalities. The vectors occupy `$FFFA-$FFFF`.

| Personality | ROM contents | Write protection |
| --- | --- | --- |
| Neo1-23 | 8 KB system ROM at `$E000-$FFFF`, containing Integer BASIC, Krusader, WozMon, and vectors | `$E000-$FFFF` |
| Neo1-50 | 256-byte WozMon image at `$FF00-$FFFF`; Pico places temporary `JMP $FF00` stubs at `$E000` and `$F000` | `$FF00-$FFFF` |

Neo1 Pico asserts the external reset control through GPIO 26. The Neo6502 must
route that signal to the W65C02 RESET pin using DIP switch 3 or the documented
UEXT-to-bus connection. The software runner obtains the same reset entry from
the vector without modeling that physical wire.

## RAM and installed software

`$0000-$00FF` is zero page and `$0100-$01FF` is the hardware stack. Other
non-device, non-ROM addresses are ordinary writable RAM.

The following are 6502 programs copied into RAM by the Pico runner, not
memory-mapped devices:

| Range | Condition | Entry |
| --- | --- | --- |
| `$1800-$2C1E` | VCFFA1 enabled | M2 block driver at `$1800`; interactive utility at `$1810` |
| `$C100-$CB17` | VACI enabled | VACI at `$C100`; ordinary transfers reserve `$C100-$CFFF` for payload growth |

Because these ranges remain RAM, a 6502 program may overwrite them. The SDL
runner does not currently install either image.

## Apple-1 keyboard and display interface

The Apple-1 core interface occupies `$D010-$D013`. It models only the
control/data-direction behavior required by WozMon, not a complete 6820/6821.

| Address | Read | Write |
| --- | --- | --- |
| `$D010` KBD | If KBDCR bit 2 is clear, read keyboard DDR; otherwise read and consume the latched key | If KBDCR bit 2 is clear, write keyboard DDR; otherwise write the keyboard data register |
| `$D011` KBDCR | Stored control bits with bit 7 set while a key is pending | Store keyboard control byte |
| `$D012` DSP | If DSPCR bit 2 is clear, read display DDR; otherwise return ready value `$00` | If DSPCR bit 2 is clear, write display DDR; otherwise emit the character to the display callback |
| `$D013` DSPCR | Stored control bits with ready bit 7 set | Store display control byte |

`$D0F2-$D0F3` mirror `$D012-$D013` for Replica 1 compatibility. Input bytes
are latched with bit 7 set. The first pending key remains latched until the
6502 consumes it; later input is ignored while that byte is pending.

This interface does not assert IRQ or NMI. Keyboard readiness and display
readiness are polled.

Both runners use the ordinary shared `neo1_apple1_pia` C module for this
6502-visible state machine. Runners supply input bytes and consume emitted
display bytes, but do not redefine register or latch behavior.

## Shared terminal grid

Display callbacks on both runners feed one shared host-side 40×24 character
grid implementation. The grid owns cells, cursor position, clear, newline,
wrap, scroll, glyph placement, and backspace erase as explicit primitives. It
is presentation state outside the 6502 address space, not part of the Apple-1
PIA contract.

Each target retains its observable byte policy and rendering transport. Pico
uses CR for newline, form feed for clear, ignores LF/backspace, and publishes
snapshots to PicoDVI. SDL uses CR for newline, backspace for erase, ignores
LF/form feed, and renders the cells with SDL. Both target callbacks strip the
Apple-1 output high bit before applying those policies; the shared grid itself
preserves all eight bits supplied to its glyph primitive.

## Neo1 MSC file interface

The optional storage service at `$D014-$D01C` is a Neo1 extension used by VACI.
`NEO1_ENABLE_MSC` owns its decode independently from VACI payload installation;
supported configurations require MSC when VACI is enabled. When MSC is
disabled, these addresses fall through to backing RAM. The enabled device
exposes file/directory commands and a 512-byte sector buffer.

| Address | Access | Meaning |
| --- | --- | --- |
| `$D014` | Write | Command opcode |
| `$D015-$D016` | Write | 16-bit sector number, low byte first |
| `$D017` | Read/write | Filename, directory-entry name, or sector-data stream |
| `$D018` | Read | Command status |
| `$D019` | Read/write | Directory-entry index |
| `$D01A` | Read | Entry information; bit 0 means valid |
| `$D01B-$D01C` | Read/write | Saturating 16-bit file size or requested write length |

Command opcodes are:

| Opcode | Command | Behavior |
| --- | --- | --- |
| `$01` | OPEN | Receive a NUL-terminated filename through DATA and open/create it |
| `$02` | CLOSE | Close the active file |
| `$03` | READ | Read the selected 512-byte sector into DATA, zero-padding short reads |
| `$04` | WRITE | Write the requested byte count from DATA and truncate at the resulting file end |
| `$10` | DIR_OPEN | Open the root directory and reset enumeration |
| `$11` | DIR_NEXT | Select the next loadable file and expose its name and size |
| `$12` | OPEN_INDEX | Open the indexed loadable file |
| `$13` | DELETE_INDEX | Delete the indexed loadable file |

Status `$00` means busy, `$01` means ready/success, and values with bit 7 set
mean error. On Pico, the low seven bits carry the FatFs result where available;
internal generic failures use code 1. Commands complete synchronously in the
current backend, but 6502 software polls STATUS as part of the contract.

The MSC interface does not assert IRQ or NMI. It is a Neo1 extension rather
than part of the Apple-1 core.

## VCFFA1 compatibility interface

VCFFA1 is an optional Replica 1 compatibility device. It does not define the
Neo1 core architecture.

Reads from `$AFDC-$AFDD` return signature bytes `$CF,$FA`. The register window
is `$AFF0-$AFFF`:

| Address | Meaning |
| --- | --- |
| `$AFF6` | Alternate status/device-control mirror |
| `$AFF8` | Streaming 512-byte block data |
| `$AFF9` | Error/feature |
| `$AFFA` | Sector count pass-through |
| `$AFFB-$AFFE` | 32-bit logical block address, low byte first |
| `$AFFF` | Status on read, command on write |

Commands `$00`, `$01`, and `$02` perform ProDOS-style status, block read, and
block write. READ makes 512 bytes available from `$AFF8`. WRITE arms a data
phase and commits after 512 bytes have been written to `$AFF8`.

Status uses ATA-like bits: ERR bit 0, DRQ bit 3, DSC bit 4, DRDY bit 6, and BSY
bit 7. Error codes are `$01` bad command, `$27` I/O, `$28` no device, `$2B`
write protected, and `$2D` invalid block. A successful idle status has DRDY and
DSC set; an active data phase also has DRQ set.

VCFFA1 does not assert IRQ or NMI. Software polls status and DRQ.

## Address ownership and expansion

Device decoding is sparse. Addresses elsewhere in `$D000-$DFFF` remain RAM.
When MSC is disabled, `$D014-$D01C` remains RAM; when VCFFA1 is disabled,
`$AFDC-$AFDD` and `$AFF0-$AFFF` also remain RAM. This explicit enablement and
ownership allows a future physical expansion device to replace an internal
service without changing ordinary memory semantics.

The shared machine owns that conditional address decode and invokes optional
MSC and VCFFA1 read/write ports attached in its description. Pico and SDL attach
their target-owned protocol/backend implementations through those ports. A
port represents a decoded 6502 register access; it does not own memory policy,
CPU execution, physical bus timing, or platform lifecycle.
