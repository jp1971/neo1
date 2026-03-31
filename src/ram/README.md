# RAM Artifacts (`src/ram`)

This directory contains 6502 RAM-resident payload sources and generated artifacts used by Neo1 at runtime.

## Source of Truth

- `neo1_vaci_v1.s`: VACI utility code image source.
- `neo1_cffa1_m2_blockdrv.s`: VCFFA1 M2 block driver source.
- `*.cfg`: linker/config files for corresponding assembly payloads.
- `gen_vaci_header.py`: generates `neo1_vaci_v1.h` from `neo1_vaci_v1.bin`.
- `gen_cffa1_m2_header.py`: generates `neo1_cffa1_m2_blockdrv.h` from labels + binary.

## Generated / Build Outputs

These are generated and may be overwritten by build steps:

- `*.bin`, `*.o`, `*.map`, `*.lst`, `*.labels`
- `neo1_vaci_v1.h`
- `neo1_cffa1_m2_blockdrv.h`

Edit the `.s`/`.cfg`/generator scripts instead of editing generated headers directly.

## Regeneration Examples

Generate VACI image header from a built binary:

```sh
python3 src/ram/gen_vaci_header.py \
	--bin src/ram/neo1_vaci_v1.bin \
	--out src/ram/neo1_vaci_v1.h
```

Generate VCFFA1 M2 block driver header from labels + binary:

```sh
python3 src/ram/gen_cffa1_m2_header.py \
	--labels src/ram/neo1_cffa1_m2_blockdrv.labels \
	--bin src/ram/neo1_cffa1_m2_blockdrv.bin \
	--out src/ram/neo1_cffa1_m2_blockdrv.h
```