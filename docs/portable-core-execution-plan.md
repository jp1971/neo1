# Neo1 Portable-Core Execution Plan

Started: 2026-08-24

Baseline tag: `neo1-portable-core-baseline-2026-08-24`

This plan records reversible extraction checkpoints and their evidence. Stable
6502-visible contracts remain in `docs/architecture.md`; verified runtime state
and defects remain in `docs/current-state.md`.

## Checkpoint 1: shared terminal grid

Status: completed 2026-08-24.

### Boundary

Extract the duplicated 40×24 character-grid state and its primitive mutations
into ordinary shared C code under `src/terminal/`:

- clear all cells and reset the cursor;
- advance to the next row, scrolling when required;
- place one caller-approved glyph, wrapping at column 40;
- erase one glyph to the left without crossing a row boundary.

This grid represents host-side display state, not the Apple-1 PIA and not a
pixel/display transport. It has two concrete consumers: the Neo6502 runner's
PicoDVI path and the SDL runner's renderer.

### Preserved target ownership

- Pico retains high-bit stripping, CR as newline, form feed as clear, ignored
  LF/backspace, UART echo, DVI publication, rasterization, and cursor blink.
- SDL retains high-bit stripping, CR as newline, ignored LF/form feed,
  backspace erase, direct SDL rendering, geometry, and cursor blink.
- Neither target gains a new control-byte conditional in the shared machine.
- No 6502-visible address, PIA behavior, ROM/RAM policy, CPU integration,
  storage protocol, or physical bus timing changes in this checkpoint.

### Acceptance criteria recorded before implementation

Focused host tests must prove:

1. clear produces 40×24 spaces and cursor `(0,0)`;
2. glyph placement advances the cursor and column 40 wraps exactly once;
3. newline resets column to zero, and newline on row 23 scrolls exactly once;
4. scrolling preserves rows 1–23 in rows 0–22 and blanks the final row;
5. backspace erases only when the cursor is right of column zero;
6. the shared glyph primitive preserves all eight bits, leaving target callbacks
   responsible for high-bit stripping;
7. Pico policy preserves CR/form-feed behavior and ignores LF/backspace;
8. SDL policy preserves CR/backspace behavior and ignores LF/form feed.

Build and runtime gates:

- all focused host tests pass without initializing SDL;
- Neo1 SDL personalities 23 and 50 build;
- a headless SDL smoke reaches WozMon for both personalities;
- Neo1 Pico personalities 23 and 50 build using SDK 2.1.0;
- the working Pico and SDL build directories are restored to personality 23;
- diff review shows no changes to physical W65C02 bus, PicoDVI scanout,
  keyboard injection, storage, memory decoding, or PIA code.

### Physical gate

After the host/build gates pass, flash the normal Neo1-23 image and confirm:

1. reset reaches WozMon on DVI and serial;
2. `0000.0FFF` wraps and scrolls without corrupt rows or DVI instability;
3. enter `D012:0C` and confirm form feed clears the DVI terminal, then confirm
   the cursor continues blinking;
4. UART and USB keyboard input still work;
5. `C100R`, followed by `Q`, returns cleanly to WozMon.

The user confirmed that this complete physical gate passed on 2026-08-24.

### Evidence to date

- `neo1_terminal_grid_contract` passes with clear, wrap, scroll, backspace,
  eight-bit glyph, Pico-policy, and SDL-policy fixtures.
- All six host tests pass under the SDL-23 configuration.
- SDL personalities 23 and 50 build and reach the WozMon `\` prompt with a
  disposable raw-image startup probe.
- Pico personalities 23 and 50 build with SDK 2.1.0; `build/` is restored to
  Neo1-23 and `build-sdl/` is restored to SDL-23.
- Diff review found no change to memory/PIA decoding, CPU integration, physical
  bus timing, storage, PicoDVI scanout, or terminal snapshot publication.
- The normal Neo1-23 image passed the physical gate: WozMon appeared on DVI and
  serial, the range dump wrapped and scrolled stably, `D012:0C` cleared the
  display with the cursor still active, UART and USB input worked, and VACI
  returned cleanly to WozMon.

### Rollback condition

Revert this checkpoint if any golden terminal state changes, either target
fails to build, SDL WozMon output changes, the Neo6502 physical smoke regresses,
or preserving behavior requires target conditionals in the shared grid.
