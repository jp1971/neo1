# ROM Assets (`src/roms`)

This directory holds ROM/image assets embedded into firmware.

## Files

- `neo1_system_rom_image.h`: embedded Neo1 system ROM bytes (generated image header).
- `neo1_apple1_rom_image.h`: embedded Apple-1 WozMon ROM bytes.
- `neo1_apple1_video_rom_image.h`: Apple-1 video character ROM bytes (font data).
- `neo1_roms.h`: aggregate include for runtime ROM image selection.
- `wozmon.asm`: source reference material for monitor ROM lineage.

## Notes

- Large byte-array headers are generated artifacts and should usually not be edited by hand.
- Keep generation/import process notes in `neo1_roms.h` synchronized with actual workflow.

## Regeneration / Import Example

For system ROM binaries, the current workflow is `xxd -i`-based (as documented in `neo1_roms.h`):

```sh
xxd -i neo1_system_rom.bin > src/roms/neo1_system_rom_image.h
```

Apply the same pattern for other ROM/image headers when replacing source binaries.