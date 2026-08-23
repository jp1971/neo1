# ROM Assets (`src/roms`)

This directory holds checked-in ROM/image headers embedded directly into
firmware. CMake does not regenerate them, and the corresponding source binaries
are not tracked here.

## Files

- `neo1_system_rom_image.h`: embedded Neo1 system ROM bytes.
- `neo1_apple1_rom_image.h`: embedded Apple-1 WozMon ROM bytes.
- `neo1_apple1_video_rom_image.h`: Apple-1 video character ROM bytes (font data).
- `neo1_roms.h`: aggregate include for runtime ROM image selection.
- `wozmon.asm`: source reference material for monitor ROM lineage.

## Notes

- Treat large byte-array headers as imported/generated build inputs rather than
  hand-edited source.
- `wozmon.asm` is lineage/reference material; it is not wired to an automated
  header build.
- Preserve provenance notes when a verified source image is imported.

## Regeneration / Import Example

When a verified system-ROM binary is available, the documented import command is:

```sh
xxd -i neo1_system_rom.bin > src/roms/neo1_system_rom_image.h
```

This example does not establish provenance or reproducibility for the Apple-1
ROM and video-font headers; record their verified source before replacing them.
